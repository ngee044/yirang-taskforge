# yirang-taskforge API 및 메시지 계약

| 항목 | 내용 |
|---|---|
| 문서 ID | YTF-API-001 |
| 버전 | 2.0.0 |
| 기준 표준 | ISO/IEC/IEEE 29148:2018 |
| 작성일 | 2026-11-07 (현행화 판) |
| 최종 수정일 | 2026-11-07 |
| 상태 | Active — 사용자 승인 여부를 뜻하지 않음 |
| 대상 커밋 | `6f7fb67` |

이 문서는 REST(계약 A), SQS(계약 B), Redis(계약 C)의 기준입니다. Go 1.24+·Python 3.11+·TypeScript의 구현 편차와 결함은 계약과 구분해 기록합니다. 구조는 [SAD](SAD.md), 테스트는 [STP](STP.md)를 참조합니다.

## 계약 A — HTTP API

### A-1. `POST /api/v1/jobs`

```json
{"task_name":"wordcount","arguments":[],"input_files":[{"filename":"hello.txt"}],"timeout_sec":30}
```

| 필드 | 계약·현재 동작 |
|---|---|
| `task_name` | 필수 비어 있지 않은 문자열, 최대 128. 실제 whitelist 검사는 워커가 수행 |
| `arguments` | 문자열 배열, 생략 가능 |
| `input_files` | filename 객체 배열, 생략/빈 배열 가능 |
| `filename` | 각 입력에서 필수, 비어 있지 않음. `/`, `\`, `..` 금지, 최대 255 |
| `timeout_sec` | 생략·null 허용, 명시 정수 1–3600. 생략/null이면 계약 B에 적재하지 않음 |

202 응답:

```json
{"success":true,"data":{"job_id":"07519548-6bcf-4a3e-9f2e-1c9c26b40f11","upload_urls":[{"filename":"hello.txt","upload_url":"https://example.invalid/presigned-put","method":"PUT"}]}}
```

API는 UUID v4를 발급하고 `jobs/{job_id}/inputs/{filename}`의 presigned PUT URL을 생성합니다. **Redis queued 선기록 후 SQS 발행**하며 실행 완료를 기다리지 않습니다. 업로드는 응답 뒤 클라이언트가 수행하므로 업로드 완료 전 워커가 메시지를 받을 수 있습니다. 접수 성공은 실행 성공을 보장하지 않습니다. TTL 기본값은 URL 600초, 상태 3600초입니다.

정상 오류 envelope는 `{success:false,error:{code,message}}`입니다. 큐 발행 실패는 500 `create_failed`입니다. presign/상태 저장 실패는 Go에서 `create_failed`, Python/JS에서 `internal_error`로 반환합니다. 현재 편차: 요청 검증 실패는 Go/JS 400, Python 422이며 Go filename 길이는 UTF-8 바이트, Python/JS는 문자열 길이입니다. JS만 arguments 64개·input_files 32개 상한이 있습니다. 전체 경계 입력의 동일 계약 충족으로 간주하지 않습니다.

근거: `Interface/golang/internal/service/job.go:114` `CreateJob`, `Interface/python/src/app/services/job_service.py:54` `create_job`, `Interface/js/src/jobs/jobs.service.ts:53` `createJob`.

### A-2. `GET /api/v1/jobs/{job_id}`

200의 `data`는 계약 C-1 문서입니다. 미존재는 404 `job_not_found`, 조회 실패는 500입니다(Go `get_failed`, Python/JS `internal_error`). 경로 ID는 UUID 형식으로 제한합니다. 형식 오류는 Go/JS 404, Python 422입니다. `task_catalog` 등 임의 Redis 키를 상태 조회에 사용할 수 없습니다. `done` 또는 `failed`까지 클라이언트가 폴링합니다.

### A-3. `GET /api/v1/tasks`

200의 `data`는 계약 C-2입니다. 카탈로그 부재는 404 `catalog_not_found`, 저장소 오류는 500입니다(Go `catalog_failed`, Python/JS `internal_error`).

### A-4. `GET /healthz`, `GET /readyz`

`/healthz`는 프로세스 생존과 uptime을 반환합니다. `/readyz`는 Redis/SQS/S3 전체 점검을 3초 이내에 끝내야 합니다(FR-OPS-01). 정상은 200 `status=ready`, 실패는 503 `status=not_ready`와 `checks`입니다. 내부 예외·엔드포인트·자격증명은 외부 응답에 포함하지 않아야 합니다(NFR-SEC-03).

**기준선 미충족:** Go는 공유 3초 context를 사용하지만 Python/JS는 각 점검에 3초를 적용해 3개 스톨 시 약 9초입니다. 세 구현은 raw 오류 문자열을 checks에 포함합니다. 수정과 회귀검증은 T-006/SPR-004에서 추적합니다.

## 계약 B — SQS 요청 메시지 (`JobRequested@1.0`)

기본 큐 `taskforge-jobs-request.fifo`, `MessageGroupId=request`, ContentBasedDeduplication 사용. 전달은 at-least-once입니다.

```json
{
  "job_id":"07519548-6bcf-4a3e-9f2e-1c9c26b40f11",
  "mode":"execute",
  "task":{"name":"wordcount","arguments":[],"timeout_sec":30},
  "download_s3":[{"bucket":"taskforge-jobs","key":"jobs/07519548-6bcf-4a3e-9f2e-1c9c26b40f11/inputs/hello.txt","method":"GET"}],
  "output_prefix":"jobs/07519548-6bcf-4a3e-9f2e-1c9c26b40f11/outputs/",
  "enqueued_at":"2026-11-07T00:00:00Z"
}
```

| 필드 | 워커 검증 |
|---|---|
| `job_id` | 필수 `[A-Za-z0-9_-]{1,64}` |
| `mode` | 필수 `execute` |
| `task.name` | 필수 비어 있지 않은 문자열 |
| `task.arguments` | 선택 문자열 배열 |
| `task.timeout_sec` | 선택 정수. REST의 1–3600 검증과 범위가 동일하지 않음 |
| `download_s3` | 필수 배열, 빈 배열 허용. bucket/key는 비어 있지 않은 문자열, method는 문자열 |
| `output_prefix` | 필수 비어 있지 않은 문자열 |
| `enqueued_at` | 생산자가 ISO-8601 시각 기록; 워커 스키마 검사에서 필수로 강제하지 않음 |
| `dlq_try_count` | 워커 재발행에서 부가하는 재시도 횟수 |

검증 구현: `Main/MessageValidation.cpp:24` `validate_request`. 직접 SQS 생산자의 bucket/prefix를 REST 생성 경로와 대조하지 않습니다. 메시지 생산자는 신뢰 경계 안에 있어야 합니다.

### 실패·재시도·ACK

| 경로 | 처리 |
|---|---|
| JSON/스키마 위반 | 재시도 없음. 형식 유효한 job_id가 있으면 failed 기록 시도 |
| whitelist 미등록 | failed 기록, DLQ 항목 제거, 재시도 없음 |
| 실행 오류·timeout·비영 exit·S3 실패 | 파일 try_count 기록, 백오프 후 요청 큐 재발행. 기본 3회 재시도 후 네 번째 실패에서 failed |
| 재발행 실패 / DLQ 기록 실패 | failed 기록 시도 |
| 상태 조회·저장 실패 | 기준선은 오류를 삼키고 ACK하는 결함 있음; T-005에서 ACK 보류 설계 예정 |

재시도 카운트는 `max(메시지 dlq_try_count, 로컬 try_count)+1`입니다. 재발행 dedup ID는 `{job_id}-retry-{count}`입니다. 지연은 capped exponential 값에 0–50% jitter를 더하므로 설정 cap이 실제 대기 최대값은 아닙니다. 기본 3회 재시도의 재발행 대기 범위는 1–1.5초, 2–3초, 4–6초입니다(`Main/TaskForgeWorker.cpp:34` `backoff_with_jitter`, `:707` 호출).

### 실행 규약

실행파일은 whitelist에서 선택하며 입력·출력 디렉터리를 첫 두 인자로 전달합니다. REST의 생략/null은 계약 B에서 timeout 필드를 생략하므로 워커 기본값을 사용합니다. 직접 SQS는 필드 부재 또는 정수 0/음수에 기본값을 적용하며, 명시 null은 스키마 위반으로 거부합니다. 양수 요청값은 워커 기본값으로 상한을 제한합니다. REST는 명시 0/음수를 거부합니다. 기준선 timeout은 직접 자식만 종료하며 자손 종료는 T-004 대상입니다. stdout_tail은 마지막 2048바이트이며 절단 경계의 UTF-8 continuation byte만 제거합니다. 임의 바이너리 출력의 UTF-8 정화는 제공하지 않습니다.

## 계약 C — Redis 문서

DB 기본값 0. API는 최초 queued 문서를 쓰고 워커가 이후 상태를 갱신합니다. Redis와 SQS 사이 트랜잭션은 없습니다.

### C-1. 작업 상태: `{job_id}`

| 필드 | 의미·존재 조건 |
|---|---|
| `job_id`, `status` | ID와 queued/running/done/failed |
| `task_name`, `enqueued_at` | 초기 요청에서 기록, 워커가 메시지에서 복원. 스키마 위반 메시지에 소스가 없으면 부재 가능 |
| `updated_at` | API의 최초 queued 기록 및 워커 상태 전이 시 기록되는 시각 |
| `exit_code`, `stdout_tail` | 실행 결과가 있는 경우 |
| `result_download_url` | `{filename,download_url,method:"GET"}` 배열 |
| `failure_reason` | 실패 원인. 기준선 running에는 이전 값이 남을 수 있으며 done에서 제거 |
| `retry_count` | 실패 기록 경로에서 추가. 성공 후에도 남을 수 있음 |

`retry_count` 부재만으로 결정적 실패를 판별하지 않습니다. DLQ 기록 실패에도 부재할 수 있고, 기존 문서 병합으로 이전 값이 남을 수도 있습니다. 상태는 기존 JSON을 병합하고 null 필드를 제거하며 TTL을 갱신합니다. API 3종은 조회 문서의 추가 필드를 그대로 전달합니다. 초기 queued 필드를 바꾸면 API 3종도 함께 수정해야 합니다.

근거: `Main/TaskForgeWorker.cpp:740` `set_job_status`, `:55` `contract_fields`.

### C-2. 카탈로그: `task_catalog`

워커 기동 시 `{tasks:[{name,description}],updated_at}`를 게시합니다. TTL을 지정하지 않아 자동 만료되지 않습니다. 실행 경로·default_timeout은 공개하지 않습니다(`Main/TaskForgeWorker.cpp:289` `upload_task_catalog`).

## 변경 이력

| 버전 | 일자 | 변경 |
|---|---|---|
| 1.0.0–1.4.1 | 이전 판 | 초기 작성 및 누적 검증 기록 |
| 2.0.0 | 2026-11-07 | 계약 A/B/C 현행화, 구현 편차·retry_count 해석·ACK 결함과 계획 구분 |
