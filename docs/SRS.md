# yirang-taskforge 소프트웨어 요구사항 명세서 (SRS)

| 항목 | 내용 |
|---|---|
| 문서 ID | YTF-SRS-001 |
| 버전 | 2.0.0 |
| 기준 표준 | ISO/IEC/IEEE 29148:2018 |
| 작성일 | 2026-11-07 (현행화 판) |
| 최종 수정일 | 2026-11-07 |
| 상태 | Active — 사용자 승인 여부를 뜻하지 않음 |
| 대상 커밋 | `21888c7` |

## 1. 서론

### 1.1 목적
본 문서는 요청 기반 작업 오케스트레이션 플랫폼 yirang-taskforge의 기능/비기능 요구사항을 정의한다. 설계(YTF-SAD-001), 테스트(YTF-STP-001), 추적성(YTF-RTM-001)의 기준이 된다.

### 1.2 범위
yirang-taskforge는 클라이언트의 작업 요청을 큐에 적재하고, 분산 워커에서 **화이트리스트에 등록된 실행파일**을 실행하며, 완료 상태와 결과 파일을 추적/제공하는 시스템이다. 포트폴리오 목적상 동일한 REST 계약을 3개 언어(Go/Python/TypeScript)로 구현한다.

### 1.3 이해관계자
| 이해관계자 | 관심사 |
|---|---|
| 클라이언트 개발자 | REST 계약의 명확성, 상태 폴링 규약 |
| 플랫폼 운영자 | 배포(Compose), 관측성, 실패 복구(DLQ) |
| 포트폴리오 평가자 | 다언어 동일 계약 구현, MSA 패턴 적용 수준 |

### 1.4 용어
| 용어 | 정의 |
|---|---|
| 작업(Job) | `job_id`로 식별되는 1회 실행 요청 |
| 태스크(Task) | 워커 화이트리스트에 등록된 실행파일 항목 |
| 터미널 상태 | `done` 또는 `failed` |
| DLQ | 파일 기반 실패 기록 (`{job_id}.json` + try_count). 재발행은 워커가 수행하며 별도 DLQ 소비자는 없음 |

## 2. 시스템 개요

시스템 구성과 데이터 흐름은 YTF-SAD-001 및 `docs/API.md`를 따른다.

- Interface (Go/Gin :8080, Python/FastAPI :8081, JS/NestJS :8082) — 작업 접수/조회
- Main/TaskForgeWorker (C++23, CppToolkit) — SQS 소비, 태스크 실행, 상태 갱신
- AWS SQS FIFO(메시징), S3(파일), Redis(상태) — 로컬 개발은 LocalStack

## 3. 기능 요구사항

### 3.1 작업 접수 (Interface)

| ID | 요구사항 | 우선순위 |
|---|---|---|
| FR-JOB-01 | 시스템은 `POST /api/v1/jobs` 요청에 대해 UUID `job_id`를 발급하고, 실행하지 않은 채 202로 즉시 응답해야 한다. | M |
| FR-JOB-02 | 시스템은 요청의 각 입력 파일에 대해 S3 키 `jobs/{job_id}/inputs/{filename}`의 presigned PUT URL을 발급해야 한다. | M |
| FR-JOB-03 | 시스템은 `GET /api/v1/jobs/{job_id}`로 상태 문서(`queued`→`running`→`done`/`failed`)를 제공해야 하며, 미존재 시 404를 반환해야 한다. | M |
| FR-JOB-04 | 시스템은 `GET /api/v1/tasks`로 워커가 게시한 태스크 카탈로그를 제공해야 한다. | M |
| FR-QUE-01 | 시스템은 작업 요청을 SQS FIFO 큐(`MessageGroupId=request`, ContentBasedDeduplication)에 계약 B 스키마로 발행해야 하며, 발행 전에 `queued` 상태를 Redis에 기록해야 한다. | M |

### 3.2 작업 실행 (Main 워커)

| ID | 요구사항 | 우선순위 |
|---|---|---|
| FR-WRK-01 | 워커는 SQS를 롱폴링으로 소비해야 한다. 실행/S3 처리 실패는 파일 DLQ 재발행(FR-WRK-07)으로 처리하고 원본을 삭제하되, 상태 조회·저장 실패는 오류를 전파해 삭제를 보류하고 SQS 가시성 시간 후 재수신해야 한다. 상태를 기록할 수 없는 잘못된 JSON/ID는 재시도 없이 폐기한다. | M |
| FR-WRK-02 | 워커는 수신 메시지를 계약 B 스키마로 검증하고, 위반 메시지는 재시도 없이 폐기하며 `job_id`가 형식 검증(`[A-Za-z0-9_-]{1,64}`)을 통과한 경우 `failed` 상태를 기록해야 한다. 단 상태 저장 오류는 FR-WRK-01에 따라 ACK를 보류한다. | M |
| FR-WRK-03 | 워커는 설정 화이트리스트(`task_whitelist`)에 등록된 실행파일만 `<executable> <inputs_dir> <outputs_dir> [args...]` 형태로 실행해야 한다. 미등록 태스크는 실패 처리한다. | M |
| FR-WRK-04 | 워커는 `download_s3[]`의 객체를 총 3회까지 시도하여 다운로드하고, 실행 후 `outputs_dir`의 모든 정규 파일을 `output_prefix` 하위로 업로드하며 각 파일의 presigned GET URL을 상태 문서에 기록해야 한다. | M |
| FR-WRK-05 | 워커는 양수 timeout을 `default_timeout_sec`로 상한 제한하고 필드 부재·정수 0/음수에는 기본값을 적용해야 한다. 제한 초과 시 실행 프로세스와 같은 프로세스 그룹의 자손을 강제 종료해야 한다. REST 생략/null은 메시지에서 필드를 생략하며 명시 0/음수는 거부한다. 직접 메시지의 null은 스키마 위반이다. | M |
| FR-WRK-06 | 워커는 상태 문서를 기존 필드 병합 방식으로 갱신하고 TTL(`redis_ttl_sec`)을 적용해야 한다. | M |
| FR-WRK-07 | 워커는 스키마 위반·화이트리스트 미등록을 제외한 실행/S3 처리 실패 시 파일 DLQ에 `try_count`를 기록하고, `dlq_max_retry_count`(기본 3) 이내면 원본 메시지에 `dlq_try_count`를 부가해 요청 큐로 재발행하며, 소진 시 `failed`로 확정해야 한다. 재시도 횟수의 SSOT는 메시지의 `dlq_try_count`이며, 워커는 재수신 시 이를 판독해 로컬 파일 소실(컨테이너 재생성·다중 인스턴스) 시에도 상한이 유지되어야 한다. | M |
| FR-WRK-08 | 워커는 기동 시 화이트리스트로부터 태스크 카탈로그를 생성해 Redis `task_catalog` 키에 게시해야 한다. | S |
| FR-WRK-09 | 워커는 DLQ 백로그가 임계치(`dlq_backlog_alert_count`) 이상이면 경고 로그를 남겨야 한다. 성공·소진으로 종결된 작업의 DLQ 항목은 제거되어 지표가 실제 재시도 대기분만 반영해야 한다. | S |
| FR-WRK-10 | 워커는 상태가 이미 터미널(`done`/`failed`)인 작업의 메시지를 재수신한 경우 재처리하지 않고 스킵해야 한다(멱등 소비). | M |

### 3.3 운영

| ID | 요구사항 | 우선순위 |
|---|---|---|
| FR-OPS-01 | 각 인터페이스는 `/healthz`(liveness)와 `/readyz`(Redis/SQS/S3 점검 전체 3초 타임아웃)를 노출해야 한다. | M |
| FR-OPS-02 | 모든 서비스는 SIGINT/SIGTERM 수신 시 graceful shutdown을 수행해야 한다. 기동 실패는 0이 아닌 종료 코드로 전파하고, 오케스트레이터의 restart 정책과 결합되어야 한다. | S |
| FR-OPS-03 | 워커는 필수 설정(`sqs_request_queue_url`, `s3_bucket`, `task_whitelist`) 누락 시 기동을 실패시켜야 한다(fail-fast). 기본값으로 조용히 기동해 큐를 소비하지 않는 상태가 healthy로 보고되어서는 안 된다. | M |

## 4. 비기능 요구사항

| ID | 분류(ISO 25010) | 요구사항 |
|---|---|---|
| NFR-PORT-01 | 이식성 | 세 인터페이스는 계약 A를 동일하게 구현해야 하며, E2E 테스트가 세 구현 모두에 대해 동일 시나리오로 통과해야 한다. |
| NFR-SEC-01 | 보안 | 워커는 화이트리스트 외 실행 경로를 제공하지 않아야 하며, 인터페이스는 `filename`의 경로 구분자/`..`를 거부해야 한다. |
| NFR-SEC-02 | 보안 | presigned URL은 TTL(기본 600초)로 만료되어야 한다. |
| NFR-SEC-03 | 보안 | 오류 응답에 스택 트레이스 등 내부 구현 정보를 노출하지 않아야 한다. |
| NFR-REL-01 | 신뢰성 | 메시지 소비는 at-least-once이며, 실패는 지수 백오프(+jitter)와 DLQ 재시도로 복구를 시도해야 한다. 재시도 간격은 관측 가능한 수준(초 단위)이어야 하며, 재시도 예산이 즉시 소진되어서는 안 된다. |
| NFR-REL-02 | 신뢰성 | 워커의 Redis/AWS 연결은 재시도(지수 백오프)로 수립하고, 원격 호출에는 타임아웃을 적용해야 한다. 인터페이스는 조회·생성 등 비즈니스 경로의 모든 원격 호출(Redis/SQS/S3)에 데드라인을 적용해, 의존성이 응답하지 않는 경우에도 요청이 유계 시간 내 오류로 종료되어야 한다. |
| NFR-OBS-01 | 관측성 | 서비스는 구조적 로깅(Go slog JSON, NestJS Logger, FastAPI logging, 워커 CppToolkit Logger)을 수행해야 한다. |
| NFR-DEP-01 | 배포 | 전체 스택은 Docker Compose로 빌드/기동되어야 하며, 서비스별 독립 이미지를 가져야 한다. C++ 워커 이미지는 Ubuntu 24.04 기반이고(CON-04), 인터페이스 3종은 언어별 공식 슬림/알파인 이미지를 사용한다. |
| NFR-DEP-02 | 배포 | 설정은 환경변수(인터페이스) 또는 마운트된 JSON(워커)으로 외부화하고 하드코딩하지 않아야 한다. |

## 5. 제약사항

| ID | 제약 |
|---|---|
| CON-01 | 메시징은 AWS SQS만 사용한다(RabbitMQ 미사용). 로컬 개발/테스트는 LocalStack. |
| CON-02 | 워커는 CppToolkit(git submodule, `std::expected` 오류 규약)을 사용한다. |
| CON-03 | 상태 저장소는 Redis 단일 소스이다(응답 큐 없음). |
| CON-04 | C++ 워커의 Docker 빌드·런타임 OS는 Ubuntu 24.04이다(CppToolkit 의존성 기준). 인터페이스 3종에는 적용되지 않는다. |

## 6. 검증 기준

각 요구사항의 검증 방법과 결과는 YTF-STP-001, 추적 매핑은 YTF-RTM-001을 따른다.

## 7. 현재 충족 상태와 잔여 범위

기능 수정과 검증 결과는 [SPR](SPR.md), [RTM](RTM.md), [VERIFICATION](VERIFICATION.md)을 따릅니다. `21888c7`에서 종료 신호·프로세스 그룹 timeout·상태 오류 ACK 보류·readiness·숨김파일·JS drain 순서 결함을 수정하고 해당 회귀를 통과했습니다.

| 요구사항 | 현재 상태 | 잔여 범위 |
|---|---|---|
| FR-WRK-01/06/10 | Redis 부재와 읽기 오류를 구분하고 조회·쓰기 실패를 expected로 전파. 실패 시 ACK 보류, 복구 후 종결·ACK 검증 | 실행 후 done 저장 실패 시 재수신으로 태스크가 재실행될 수 있음. exactly-once 아님 |
| FR-WRK-05 | timeout 시 프로세스 그룹 종료, 자식 종료 신호 마스크 복원 | POSIX 구현. 자손이 스스로 그룹을 벗어나는 경우의 격리 보장은 범위 밖 |
| FR-OPS-01 / NFR-SEC-03 | Go 공유 deadline, Python/JS 병렬 probe로 전체 3초 예산. 응답은 ok/unavailable | Python 동기 SDK 스레드의 실제 I/O는 취소되지 않을 수 있음 |
| FR-OPS-02 | sigwait 기반 직렬 종료와 HTTP drain 후 Redis 해제 | 미응답 Redis I/O·장기 작업의 최대 종료 시간은 미검증 |
| NFR-REL-02 | API timeout·워커 AWS 설정·연결 재시도 구현 | 워커 Redis I/O timeout은 현재 CppToolkit API에 없어 미충족 |
| NFR-OBS-01 | 각 언어 Logger와 실패 경계 로그 사용 | 전체 서비스의 JSON 구조·공통 필드 통일은 미충족 |
| NFR-PORT-01 | 정상 E2E 3종 통과 | 검증 오류 코드·filename 길이 단위·배열 상한 차이는 API A-1에 명시 |

기능 구현·자동 테스트 부재·플랫폼 미검증은 구분합니다. SaaS·콘솔·인증·과금은 현행 요구사항에 포함하지 않습니다.

## 변경 이력

| 버전 | 일자 | 변경 |
|---|---|---|
| 1.0.0–1.2.0 | 이전 판 | 초기 작성 및 누적 검증 기록 |
| 1.3.0 | 2026-11-07 | 요구사항 ID 유지, 다운로드 시도 수·실패 범위 명확화, 현재 미충족과 예정 변경 분리 |
| 2.0.0 | 2026-11-07 | ACK 보류와 프로세스 그룹 timeout 요구 반영; 수정 완료·잔여 NFR 구분 |
