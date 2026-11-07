# yirang-taskforge 요구사항 추적성 매트릭스 (RTM)

| 항목 | 내용 |
|---|---|
| 문서 ID | YTF-RTM-001 |
| 버전 | 2.0.0 |
| 기준 표준 | ISO/IEC/IEEE 29148:2018 (Traceability) |
| 작성일 | 2026-11-07 (현행화 판) |
| 최종 수정일 | 2026-11-07 |
| 상태 | Active — 사용자 승인 여부를 뜻하지 않음 |
| 대상 커밋 | `6f7fb67` |

## 1. 추적 범위

[SRS](SRS.md)의 FR 18개·NFR 9개·CON 4개, 총 31개를 보존합니다. `file:line`은 `6f7fb67` 기준 대표 구현이며 후속 변경 때 심볼 기준으로 갱신합니다. 설계 열은 [SAD](SAD.md)의 절입니다. 테스트 ID가 있다는 사실만으로 자동화·실행·PASS를 의미하지 않으며 정확한 강도는 [STP](STP.md), 결과는 [VERIFICATION](VERIFICATION.md)을 따릅니다.

## 2. 요구사항 → 설계 → 구현 → 검증

| 요구사항 | 요약 | SAD | 구현 근거 | STP 케이스 | 기준선 상태 |
|---|---|---|---|---|---|
| FR-JOB-01 | 작업 접수 | §3.1 | `Interface/golang/internal/service/job.go:114 CreateJob; Interface/python/src/app/services/job_service.py:54 create_job; Interface/js/src/jobs/jobs.service.ts:53 createJob` | TC-IF-01, IT-03 | 구현 |
| FR-JOB-02 | 업로드 URL | §3.1 | `Interface/golang/internal/service/job.go:123 CreateJob` | TC-IF-02, IT-03, IT-04 | 구현; 만료 경계 미검증 |
| FR-JOB-03 | 상태 조회 | §3.1 | `Interface/golang/internal/service/job.go:165 GetJob` | TC-IF-05, IT-05, IT-16 | 구현 |
| FR-JOB-04 | 카탈로그 조회 | §3.1 | `Interface/golang/internal/service/job.go:181 GetTaskCatalog` | IT-02 | 구현 |
| FR-QUE-01 | queued 선기록·발행 | §3.1, ADR-03 | `Interface/python/src/app/services/job_service.py:77 create_job` | TC-IF-01, TC-IF-03, TC-IF-08, IT-05 | 구현; 테스트 순서 단언 없음 |
| FR-WRK-01 | 소비·ACK | §3.2, ADR-04/05 | `Main/TaskForgeWorker.cpp:413 handle_message` | IT-05, IT-28 | 부분: T-005 ACK 예외 설계 예정 |
| FR-WRK-02 | 메시지 검증 | §3.2 | `Main/MessageValidation.cpp:24 validate_request` | IT-08, IT-09, IT-21 | 구현; 수동 경계 재검증 필요 |
| FR-WRK-03 | whitelist 실행 | §3.3, ADR-02/13 | `Main/TaskExecutor.cpp:27 find_task; Main/TaskForgeWorker.cpp:642 fail_without_retry` | IT-05, IT-07, IT-23, IT-24, IT-29 | 부분: T-007 숨김파일 |
| FR-WRK-04 | S3 입출력 | §3.3 | `Main/TaskForgeWorker.cpp:562 download_inputs, :598 upload_outputs` | IT-05, IT-06, IT-29 | 부분: T-007 숨김파일 |
| FR-WRK-05 | 실행 시간 제한 | §3.3 | `Main/TaskExecutor.cpp:40 execute` | IT-05, IT-27 | 미충족: T-004 자손 생존 |
| FR-WRK-06 | 병합·TTL | §3.2, ADR-11 | `Main/TaskForgeWorker.cpp:740 set_job_status` | IT-05, IT-15, IT-20, IT-21, IT-23, IT-28 | 부분: T-005 오류 전파 |
| FR-WRK-07 | 재시도 카운트·재발행 | §3.4, ADR-05/06/09/13 | `Main/TaskDLQ.cpp:24 record_failure; Main/TaskForgeWorker.cpp:654 handle_processing_failure` | IT-10, IT-19, IT-25 | 구현; 장애별 수동 회귀 필요 |
| FR-WRK-08 | 카탈로그 게시 | §3.2 | `Main/TaskForgeWorker.cpp:289 upload_task_catalog` | IT-02 | 구현 |
| FR-WRK-09 | DLQ 감시·정리 | §3.4/3.5, ADR-10 | `Main/TaskForgeWorker.cpp:391 start_dlq_backlog_monitor; Main/TaskDLQ.cpp:72 remove` | IT-15, IT-19, IT-23 | 구현; crash 고아 재조정 없음 |
| FR-WRK-10 | 터미널 가드 | §3.2, ADR-08 | `Main/TaskForgeWorker.cpp:785 is_terminal_status` | IT-14, IT-28 | 부분: T-005 조회 실패 처리 |
| FR-OPS-01 | health/readiness | §4 | `Interface/golang/internal/http/handlers/health.go:37; Interface/python/src/app/api/routes.py:62; Interface/js/src/health/health.controller.ts:30` | IT-01, IT-12 | 미충족: T-006 |
| FR-OPS-02 | 기동·종료 | §3.5/4 | `Main/main.cpp:96 signal_callback; Interface/js/src/infra/redis.service.ts:48` | IT-22, IT-26, IT-30 | 미충족: T-003/T-011 |
| FR-OPS-03 | 설정 fail-fast | ADR-12 | `Main/Configurations.cpp:122 validate_required` | IT-17 | 구현 |
| NFR-PORT-01 | 3종 계약 정합성 | §3.1 | `Interface/golang/internal/http/handlers/jobs.go:42; Interface/python/src/app/schemas/jobs.py:20; Interface/js/src/jobs/dto/create-job.dto.ts:25` | TC-IF-07, TC-IF-08 | 부분: API A-1 편차 |
| NFR-SEC-01 | 실행·경로 제한 | §3.2/3.3 | `Main/MessageValidation.cpp:11 is_valid_job_id; Main/TaskExecutor.cpp:27 find_task` | TC-IF-06, IT-09, IT-16 | 구현; 직접 생산자 신뢰 경계 제약 |
| NFR-SEC-02 | presign 만료 | §3.1/3.3 | `Main/Configurations.cpp:41 presign_ttl_sec_; Interface/golang/internal/service/job.go:123` | IT-04, IT-06 | 구현; 만료 시점 시험 없음 |
| NFR-SEC-03 | 내부 오류 비노출 | §4 | `Interface/python/src/app/main.py:100 unhandled_error; Interface/js/src/common/domain-exception.filter.ts:48` | IT-12 | 부분: readiness raw 오류 T-006 |
| NFR-REL-01 | at-least-once·백오프 | §3.4, ADR-09 | `Main/TaskForgeWorker.cpp:34 backoff_with_jitter, :654 handle_processing_failure` | TC-IF-04, IT-10, IT-13, IT-24, IT-25 | 구현; exactly-once 보장 없음 |
| NFR-REL-02 | 원격 timeout·연결 재시도 | §3.4/4 | `Main/TaskForgeWorker.cpp:251 connect_redis, :315 make_client_config; Interface/golang/internal/repository/redisrepo/redisrepo.go:32` | IT-11, IT-25 | 부분: 워커 Redis timeout 없음 |
| NFR-OBS-01 | 구조적 로깅 | §3.5/4 | `Main/main.cpp:37 Logger 시작; Interface/golang/cmd/api/main.go:37` | IT-18 | 부분: 전체 JSON 필드 규약 미충족 |
| NFR-DEP-01 | 독립 이미지·Compose | §4 | `docker/docker-compose.yml:1; docker/Dockerfile.worker:4` | 자동 케이스 없음; 정적/빌드 검증 | 정적 확인·Compose build/E2E 통과 |
| NFR-DEP-02 | 설정 외부화 | ADR-07/12 | `Main/Configurations.cpp:142 load; docker/docker-compose.yml:1` | IT-17 | 구현 |
| CON-01 | SQS 메시징 | §2 | `Main/TaskForgeWorker.cpp:331 connect_aws_service` | 자동 케이스 없음; 정적/빌드 검증 | 정적 확인 |
| CON-02 | CppToolkit 사용 | §3.5 | `Main/CMakeLists.txt:30; .gitmodules:1` | 자동 케이스 없음; 정적/빌드 검증 | 정적 확인·C++ 빌드 |
| CON-03 | Redis 상태 SSOT | §2, ADR-01 | `Main/TaskForgeWorker.cpp:740 set_job_status` | 자동 케이스 없음; 정적/빌드 검증 | 정적 확인·정상 E2E |
| CON-04 | Worker Ubuntu24.04 | §4 | `docker/Dockerfile.worker:4` | 자동 케이스 없음; 정적/빌드 검증 | 정적 확인·Docker build |

## 3. 검증 → 요구사항 역방향

아래 행은 §2와 같은 매핑을 사용합니다. 계약 C-1 stdout_tail의 절단 경계 정렬은 `Main/TaskExecutor.cpp:145` `read_stdout_tail`에서 확인되나 전용 자동 테스트는 없습니다. URL 사용 성공은 TTL 만료를 검증하지 않습니다.

| STP 케이스 | 요구사항 |
|---|---|
| TC-IF-01 | FR-JOB-01, FR-QUE-01 |
| TC-IF-02 | FR-JOB-02 |
| TC-IF-03 | FR-QUE-01 |
| TC-IF-04 | NFR-REL-01 |
| TC-IF-05 | FR-JOB-03 |
| TC-IF-06 | NFR-SEC-01 |
| TC-IF-07 | NFR-PORT-01 |
| TC-IF-08 | FR-QUE-01, NFR-PORT-01 |
| IT-01 | FR-OPS-01 |
| IT-02 | FR-JOB-04, FR-WRK-08 |
| IT-03 | FR-JOB-01, FR-JOB-02 |
| IT-04 | FR-JOB-02, NFR-SEC-02 |
| IT-05 | FR-JOB-03, FR-QUE-01, FR-WRK-01, FR-WRK-03, FR-WRK-04, FR-WRK-05, FR-WRK-06 |
| IT-06 | FR-WRK-04, NFR-SEC-02 |
| IT-07 | FR-WRK-03 |
| IT-08 | FR-WRK-02 |
| IT-09 | FR-WRK-02, NFR-SEC-01 |
| IT-10 | FR-WRK-07, NFR-REL-01 |
| IT-11 | NFR-REL-02 |
| IT-12 | FR-OPS-01, NFR-SEC-03 |
| IT-13 | NFR-REL-01 |
| IT-14 | FR-WRK-10 |
| IT-15 | FR-WRK-06, FR-WRK-09 |
| IT-16 | FR-JOB-03, NFR-SEC-01 |
| IT-17 | FR-OPS-03, NFR-DEP-02 |
| IT-18 | NFR-OBS-01 |
| IT-19 | FR-WRK-07, FR-WRK-09 |
| IT-20 | FR-WRK-06 |
| IT-21 | FR-WRK-02, FR-WRK-06 |
| IT-22 | FR-OPS-02 |
| IT-23 | FR-WRK-03, FR-WRK-06, FR-WRK-09 |
| IT-24 | FR-WRK-03, NFR-REL-01 |
| IT-25 | FR-WRK-07, NFR-REL-01, NFR-REL-02 |
| IT-26 | FR-OPS-02 |
| IT-27 | FR-WRK-05 |
| IT-28 | FR-WRK-01, FR-WRK-06, FR-WRK-10 |
| IT-29 | FR-WRK-03, FR-WRK-04 |
| IT-30 | FR-OPS-02 |

## 4. 추적성 검증

문서 ID·케이스 연결, 코드 경로/심볼, 자동 테스트의 실제 단언을 독립 감사합니다. 계획 상태는 완료로 승격하지 않습니다. CI 네 잡의 정의는 `.github/workflows/gates.yml:16`에서 확인하며 원격 실행 성공 여부는 해당 커밋의 실행 결과로 별도 판정합니다. 문서 형상 관리 복원은 T-001이며 초기 문서 커밋 전 상태와 이후 상태를 혼합하지 않습니다.

## 변경 이력

| 버전 | 일자 | 변경 |
|---|---|---|
| 1.0.0–1.7.0 | 이전 판 | 초기 작성 및 누적 검증 기록 |
| 2.0.0 | 2026-11-07 | 31개 요구사항 유지, 양방향 케이스 매핑 일치, 실측·정적·계획 분리 |
