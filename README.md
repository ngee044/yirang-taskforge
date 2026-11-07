# yirang-taskforge

Request-driven job orchestration platform — 요청을 큐에 넣고, 분산 워커에서 화이트리스트 실행파일을 실행하고, 완료를 추적합니다.

동일한 REST API 계약을 **3개 언어(Go/Python/TypeScript)** 로 구현한 포트폴리오 프로젝트입니다. 코어 워커는 **C++23 + [CppToolkit](https://github.com/ngee044/CppToolkit)** 으로 작성되었습니다.

## 아키텍처

```mermaid
flowchart LR
    Client([Client])

    Interface["Interface<br/>REST API"]

    subgraph AWS["AWS (LocalStack)"]
        SQS[["SQS FIFO<br/>taskforge-jobs-request.fifo"]]
        S3[("S3<br/>taskforge-jobs")]
    end

    REDIS[("Redis<br/>상태 · 태스크 카탈로그")]

    subgraph Main["Main"]
        WORKER["TaskForgeWorker<br/>C++23 + CppToolkit"]
        TASKS["화이트리스트 태스크<br/>wordcount · checksum"]
        DLQ["파일 DLQ<br/>(재시도 ≤ 3)"]
    end

    Client -->|"POST /api/v1/jobs → 202"| Interface
    Client -->|"PUT 입력파일 (presigned)"| S3
    Client -->|"GET /api/v1/jobs/{id} 폴링"| Interface
    Interface -->|"요청 발행"| SQS
    Interface -->|"queued 기록 · 조회"| REDIS
    SQS -->|롱폴링 소비| WORKER
    S3 <-->|"입력 다운로드 / 결과 업로드"| WORKER
    WORKER --> TASKS
    WORKER -->|"running → done/failed"| REDIS
    WORKER <--> DLQ
```

### 작업 처리 시퀀스

```mermaid
sequenceDiagram
    participant C as Client
    participant API as Interface (REST API)
    participant S3 as S3
    participant Q as SQS FIFO
    participant R as Redis
    participant W as TaskForgeWorker (C++)

    C->>API: POST /api/v1/jobs {task_name, input_files}
    API->>S3: presigned PUT URL 발급
    API->>R: status=queued 기록
    API->>Q: JobRequested 메시지 발행
    API-->>C: 202 {job_id, upload_urls}
    C->>S3: PUT 입력 파일 (presigned)

    Q->>W: 롱폴링 수신
    W->>W: 요청 스키마·화이트리스트 검증
    W->>R: status=running (병합)
    W->>S3: 입력 다운로드 (파일별 최대 3회 시도)
    W->>W: 태스크 실행 (타임아웃)
    W->>S3: 결과 업로드 + presigned GET
    W->>R: status=done {exit_code, result_download_url[]}
    Note over W,Q: 핸들러 성공 반환 시에만 메시지 삭제 (at-least-once)

    loop 터미널 상태까지
        C->>API: GET /api/v1/jobs/{job_id}
        API->>R: GET {job_id}
        API-->>C: {status, ...}
    end
    C->>S3: GET 결과 파일 (presigned)
```

실패 경로: 실행·파일 처리 실패 → 파일 DLQ에 try_count 기록 → 3회까지 요청 큐 재발행 → 소진 시 `failed` 확정. 스키마 위반과 화이트리스트 미등록 태스크는 재발행하지 않습니다. Redis 상태 조회·저장이 실패하면 ACK를 보류하여 SQS 재수신으로 복구합니다.

## 구성 요소

| 경로 | 스택 | 역할 |
|---|---|---|
| `Main/` | C++23, CppToolkit(서브모듈), aws-sdk-cpp, boost.process | SQS 소비 → 화이트리스트 태스크 실행 → S3/Redis 갱신 |
| `Interface/golang/` | Go 1.24+, Gin, aws-sdk-go-v2, go-redis | REST API (:8080) |
| `Interface/python/` | Python 3.11+, FastAPI, boto3, redis-py | REST API (:8081) |
| `Interface/js/` | Node.js 20+, NestJS, TS strict, @aws-sdk v3, ioredis | REST API (:8082) |
| `docker/` | 워커: Ubuntu 24.04, API: 언어별 공식 이미지, LocalStack, Redis 7 | 전체 스택 Compose + E2E |
| `docs/` | ISO 29148/42010/29119-3 | SRS·SAD·STP·RTM·API 계약·컨벤션 |

## 빠른 시작 (Docker)

전제: Docker Desktop (또는 docker + compose v2)

```bash
git clone --recursive https://github.com/ngee044/yirang-taskforge.git
cd yirang-taskforge/docker

# 전체 스택 빌드 + 기동 (워커는 컨테이너 안에서 vcpkg로 빌드 — 최초 수십 분 소요)
docker compose up -d --build

# E2E 검증 (3개 API 각각: 등록→업로드→폴링→결과 검증 + 실패 케이스)
./scripts/e2e_test.sh
```

## API 사용법

세 인터페이스는 같은 작업 접수·조회 흐름을 제공합니다 (`8080`=Go, `8081`=Python, `8082`=NestJS). 검증 오류의 HTTP 상태 등 구현별 차이를 포함한 전체 계약은 [`docs/API.md`](docs/API.md)를 참조하십시오.

```bash
# 1. 실행 가능한 태스크 목록
curl -s localhost:8080/api/v1/tasks | jq

# 2. 작업 등록 → job_id + 업로드 URL 수령
RESP=$(curl -s -X POST localhost:8080/api/v1/jobs \
  -H 'Content-Type: application/json' \
  -d '{"task_name":"wordcount","input_files":[{"filename":"hello.txt"}]}')
JOB_ID=$(echo "$RESP" | jq -r '.data.job_id')
UPLOAD_URL=$(echo "$RESP" | jq -r '.data.upload_urls[0].upload_url' | sed 's/localstack/localhost/')

# 3. 입력 파일 업로드 (API를 거치지 않고 S3로 직접)
echo "hello world from yirang taskforge" | curl -sf -X PUT --data-binary @- "$UPLOAD_URL"

# 4. 완료까지 폴링
curl -s "localhost:8080/api/v1/jobs/$JOB_ID" | jq '.data.status'

# 5. 결과 다운로드
curl -s "localhost:8080/api/v1/jobs/$JOB_ID" | jq -r '.data.result_download_url[0].download_url'
```

### 상태 전이

```
queued ──> running ──> done
   ▲          │
   └─ 재시도 ─┤
              └──> failed (소진 또는 복구 불가)
queued ──> failed (화이트리스트 미등록)
```

## 로컬 개발 (Docker 없이)

### C++ 워커

```bash
# vcpkg 탐색 순서: $VCPKG_ROOT > ../vcpkg > ~/vcpkg
./build.sh
./build/out/TaskForgeWorker   # 설정: build/out/task_forge_worker_configurations.json
```

### 인터페이스

```bash
# Go
cd Interface/golang
SQS_REQUEST_QUEUE_URL=http://localhost:4566/000000000000/taskforge-jobs-request.fifo \
AWS_ENDPOINT_URL=http://localhost:4566 go run ./cmd/api

# Python
cd Interface/python
python3 -m venv .venv && .venv/bin/pip install -e ".[dev]"
SQS_REQUEST_QUEUE_URL=... AWS_ENDPOINT_URL=http://localhost:4566 \
  .venv/bin/uvicorn app.main:app --app-dir src --port 8081

# NestJS
cd Interface/js
npm install && npm run build
SQS_REQUEST_QUEUE_URL=... AWS_ENDPOINT_URL=http://localhost:4566 npm start
```

### 검증 게이트

| 대상 | 명령 |
|---|---|
| C++ | `./build.sh && ctest --test-dir build --output-on-failure` |
| C++ 포맷 | `clang-format-19 --dry-run --Werror Main/*.cpp Main/*.h Tests/*.cpp` |
| Go | `test -z "$(gofmt -l .)" && go build ./... && go vet ./... && go test -race ./...` |
| Python | `.venv/bin/ruff check . ../../Tests/worker_recovery_test.py && .venv/bin/ruff format --check . ../../Tests/worker_recovery_test.py && .venv/bin/pytest` |
| JS | `npm run build && npm run lint && npm run format:check && npm test` |
| E2E | `docker/scripts/e2e_test.sh` |
| Redis 장애·SQS 복구 | Compose 기동 후 `python3 Tests/worker_recovery_test.py` |

Go/Python/JS 명령은 해당 `Interface/` 서비스 디렉터리에서, 나머지는 프로젝트 루트에서 실행합니다.

## 새 태스크 추가하기

1. 실행파일을 `Main/tasks/`에 추가 — 호출 규약: `<executable> <inputs_dir> <outputs_dir> [args...]`, 결과는 `outputs_dir`에 파일로 생성, 성공 시 exit 0
2. 워커 설정(`task_forge_worker_configurations.json`)의 `task_whitelist`에 등록
3. 워커 재기동 → `GET /api/v1/tasks` 카탈로그에 자동 노출

화이트리스트에 없는 태스크 요청은 재시도 없이 `failed`로 확정됩니다.

## 문서

| 문서 | 내용 |
|---|---|
| [`docs/API.md`](docs/API.md) | 서비스 간 계약 SSOT (REST / SQS 메시지 / Redis 문서) |
| [`docs/SRS.md`](docs/SRS.md) | 요구사항 명세 (ISO/IEC/IEEE 29148) |
| [`docs/SAD.md`](docs/SAD.md) | 아키텍처 기술서 + ADR (ISO/IEC/IEEE 42010) |
| [`docs/STP.md`](docs/STP.md) | 테스트 계획/결과 (ISO/IEC/IEEE 29119-3) |
| [`docs/RTM.md`](docs/RTM.md) | 요구사항 추적성 매트릭스 |
| [`docs/project_analysis.md`](docs/project_analysis.md) | 코드 기준 분석과 컨셉·제한사항 |
| [`docs/task_list.md`](docs/task_list.md) | 개선 태스크와 검증 상태 |
| [`docs/VERIFICATION.md`](docs/VERIFICATION.md) | 현재 검증 결과와 미검증 범위 |
| [`docs/module_usage_guide.md`](docs/module_usage_guide.md) | 체크아웃된 CppToolkit API와 Logger 사용법 |
| [`docs/conventions/`](docs/conventions) | 언어별 코딩 규약 (C++/Go/Python/JS) |
| [`docs/architecture/MSA_ARCHITECTURE.md`](docs/architecture/MSA_ARCHITECTURE.md) | MSA 원칙 SSOT |

## 라이선스

MIT — [LICENSE](LICENSE)
