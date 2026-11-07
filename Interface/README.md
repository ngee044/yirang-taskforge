# Interface — 동일 REST 계약의 3개 언어 구현

yirang-taskforge의 작업 접수/조회 API입니다. 세 구현은 [공통 계약](../docs/API.md)을 바탕으로 같은 작업 흐름을 제공합니다. 검증 오류 상태 코드와 선택 필드 처리의 구현별 차이는 계약 문서에 명시합니다.

| 구현 | 스택 | 포트 | 검증 게이트 |
|---|---|---|---|
| [`golang/`](golang) | Go 1.24+ · Gin · aws-sdk-go-v2 · go-redis | 8080 | `gofmt` `go build` `go vet` `go test -race` |
| [`python/`](python) | Python 3.11+ · FastAPI · boto3 · redis-py | 8081 | `ruff check` `ruff format --check` `pytest` |
| [`js/`](js) | Node.js 20+ · NestJS · TypeScript strict · @aws-sdk v3 · ioredis | 8082 | `build` `lint` `format:check` `test` |

## 공통 엔드포인트

| 메서드/경로 | 동작 |
|---|---|
| `POST /api/v1/jobs` | 작업 등록 → 202 `{job_id, upload_urls[]}` (presigned PUT) |
| `GET /api/v1/jobs/{job_id}` | Redis 상태 문서 조회 (404 = 미존재) |
| `GET /api/v1/tasks` | 워커가 게시한 태스크 카탈로그 |
| `GET /healthz` | liveness |
| `GET /readyz` | Redis/SQS/S3 점검 (3초 타임아웃) |

성공 envelope: `{"success":true,"data":{...}}`. 실패 envelope: `{"success":false,"error":{"code":"...","message":"..."}}`.

## 공통 아키텍처 규칙

- 계층: handler/controller → service → repository/adapter (인프라 SDK는 어댑터에만)
- 서비스 계층은 소비자 측 인터페이스(Go interface / Python `Protocol` / NestJS DI)로 어댑터에 의존
- 도메인 오류 → HTTP 변환은 경계에서만 수행
- `queued` 상태를 Redis에 **먼저** 기록한 뒤 SQS 발행 (상태 역행 방지)
- 설정은 전부 환경변수 (12-factor)

## 공통 환경변수

| 변수 | 기본값 | 설명 |
|---|---|---|
| `AWS_REGION` | `us-east-1` | |
| `AWS_ENDPOINT_URL` | (없음) | LocalStack 등 커스텀 엔드포인트. 지정 시 S3 path-style |
| `S3_BUCKET` | `taskforge-jobs` | |
| `SQS_REQUEST_QUEUE_URL` | — | **필수** (없으면 부트 실패) |
| `SQS_MESSAGE_GROUP_ID` | `request` | FIFO 메시지 그룹 |
| `REDIS_HOST` / `REDIS_PORT` | `127.0.0.1` / `6379` | Go는 `REDIS_ADDR=host:port`도 지원 |
| 포트 | Go `HTTP_ADDR=:8080` · Python/JS `HTTP_PORT` | |
| presign TTL | Go `S3_PRESIGN_TTL=10m` · Python/JS `S3_PRESIGN_TTL_SEC=600` | |
| 상태 TTL | Go `REDIS_TTL=1h` · Python/JS `REDIS_TTL_SEC=3600` | |

언어별 코딩 규약은 [docs/conventions/](../docs/conventions/)를 따릅니다. 현재 검증 결과와 남은 차이는 [검증 보고서](../docs/VERIFICATION.md)를 참조하십시오.
