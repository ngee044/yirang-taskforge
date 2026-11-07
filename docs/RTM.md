# yirang-taskforge 요구사항 추적성 매트릭스 (RTM)

| 항목 | 내용 |
|---|---|
| 문서 ID | YTF-RTM-001 |
| 버전 | 2.1.0 |
| 기준 표준 | ISO/IEC/IEEE 29148:2018 (Traceability) |
| 작성일 | 2026-11-07 (현행화 판) |
| 최종 수정일 | 2026-11-07 |
| 상태 | Active — 사용자 승인 여부를 뜻하지 않음 |
| 대상 커밋 | `21888c7` |

## 1. 추적 범위

[SRS](SRS.md)의 FR 18개·NFR 9개·CON 4개, 총 31개를 보존합니다. `file:line`은 최종 코드 기준 대표 구현입니다. 설계 열은 [SAD](SAD.md)의 절입니다. 테스트 ID가 있다는 사실만으로 자동화·실행·PASS를 의미하지 않으며 정확한 강도는 [STP](STP.md), 결과는 [VERIFICATION](VERIFICATION.md)을 따릅니다.

## 2. 요구사항 → 설계 → 구현 → 검증

| 요구사항 | 요약 | SAD | 구현 근거 | STP 케이스 | 현재 상태 |
|---|---|---|---|---|---|
| FR-JOB-01 | 작업 접수 | §3.1 | `Interface/golang/internal/service/job.go:112 CreateJob; Interface/python/src/app/services/job_service.py:54 create_job; Interface/js/src/jobs/jobs.service.ts:53 createJob` | TC-IF-01, IT-03 | 구현 |
| FR-JOB-02 | 업로드 URL | §3.1 | `Interface/golang/internal/service/job.go:121 CreateJob` | TC-IF-02, IT-03, IT-04 | 구현; 만료 경계 미검증 |
| FR-JOB-03 | 상태 조회 | §3.1 | `Interface/golang/internal/service/job.go:163 GetJob` | TC-IF-05, IT-05, IT-16 | 구현 |
| FR-JOB-04 | 카탈로그 조회 | §3.1 | `Interface/golang/internal/service/job.go:179 GetTaskCatalog` | IT-02 | 구현 |
| FR-QUE-01 | queued 선기록·발행 | §3.1, ADR-03 | `Interface/python/src/app/services/job_service.py:77 create_job` | TC-IF-01, TC-IF-03, TC-IF-08, IT-05 | Go/JS 순서 단언 PASS; Python 순서 단언 없음 |
| FR-WRK-01 | 소비·ACK | §3.2, ADR-04/05 | `Main/TaskForgeWorker.cpp:404 handle_message` | IT-05, IT-28 | 구현·읽기/쓰기 오류 복구 회귀 PASS |
| FR-WRK-02 | 메시지 검증 | §3.2 | `Main/MessageValidation.cpp:26 validate_request` | IT-08, IT-09, IT-21, TC-WRK-01 | 구현; 수동 경계 재검증 필요 |
| FR-WRK-03 | whitelist 실행 | §3.3, ADR-02/13 | `Main/TaskExecutor.cpp:56 find_task; Main/TaskForgeWorker.cpp:634 fail_without_retry` | IT-05, IT-07, IT-23, IT-24, IT-29, TC-WRK-03 | 구현·숨김/혼합/빈 입력 회귀 PASS |
| FR-WRK-04 | S3 입출력 | §3.3 | `Main/TaskForgeWorker.cpp:555 download_inputs, :590 upload_outputs` | IT-05, IT-06, IT-29 | 구현·숨김/혼합/빈 입력 회귀 PASS |
| FR-WRK-05 | 실행 시간 제한 | §3.3 | `Main/TaskExecutor.cpp:69 execute` | IT-05, IT-27, TC-WRK-03 | 구현·그룹 timeout/신호 회귀 PASS |
| FR-WRK-06 | 병합·TTL | §3.2, ADR-11 | `Main/TaskForgeWorker.cpp:757 set_job_status` | IT-05, IT-15, IT-20, IT-21, IT-23, IT-28 | 구현·부재/오류 구분·저장 오류 회귀 PASS |
| FR-WRK-07 | 재시도 카운트·재발행 | §3.4, ADR-05/06/09/13 | `Main/TaskDLQ.cpp:24 record_failure; Main/TaskForgeWorker.cpp:651 handle_processing_failure` | IT-10, IT-19, IT-25, TC-WRK-02 | 구현; 장애별 수동 회귀 필요 |
| FR-WRK-08 | 카탈로그 게시 | §3.2 | `Main/TaskForgeWorker.cpp:272 upload_task_catalog` | IT-02 | 구현 |
| FR-WRK-09 | DLQ 감시·정리 | §3.4/3.5, ADR-10 | `Main/TaskForgeWorker.cpp:381 start_dlq_backlog_monitor; Main/TaskDLQ.cpp:72 remove` | IT-15, IT-19, IT-23, TC-WRK-02 | 구현; crash 고아 재조정 없음 |
| FR-WRK-10 | 터미널 가드 | §3.2, ADR-08 | `Main/TaskForgeWorker.cpp:807 is_terminal_status` | IT-14, IT-28 | 구현·읽기 오류/터미널 재수신 회귀 PASS |
| FR-OPS-01 | health/readiness | §4 | `Interface/golang/internal/http/handlers/health.go:38; Interface/python/src/app/api/routes.py:68; Interface/js/src/health/health.controller.ts:31` | IT-01, IT-12 | 구현·단위 회귀 및 정상 E2E PASS |
| FR-OPS-02 | 기동·종료 | §3.5/4 | `Main/main.cpp:19 main/sigwait; Interface/js/src/infra/redis.service.ts:49` | IT-22, IT-26, IT-30 | 구현·기동 종료/HTTP drain 회귀 PASS |
| FR-OPS-03 | 설정 fail-fast | ADR-12 | `Main/Configurations.cpp:113 validate_required` | IT-17 | 구현 |
| NFR-PORT-01 | 3종 계약 정합성 | §3.1 | `Interface/golang/internal/http/handlers/jobs.go:42; Interface/python/src/app/schemas/jobs.py:20; Interface/js/src/jobs/dto/create-job.dto.ts:25` | TC-IF-07, TC-IF-08 | 부분: API A-1 편차 |
| NFR-SEC-01 | 실행·경로 제한 | §3.2/3.3 | `Main/MessageValidation.cpp:11 is_valid_job_id; Main/TaskExecutor.cpp:56 find_task` | TC-IF-06, IT-09, IT-16, TC-WRK-01 | 구현; 직접 생산자 신뢰 경계 제약 |
| NFR-SEC-02 | presign 만료 | §3.1/3.3 | `Main/Configurations.cpp:38 presign_ttl_sec_; Interface/golang/internal/service/job.go:121` | IT-04, IT-06 | 구현; 만료 시점 시험 없음 |
| NFR-SEC-03 | 내부 오류 비노출 | §4 | `Interface/python/src/app/main.py:100 unhandled_error; Interface/js/src/common/domain-exception.filter.ts:48` | IT-12 | 고정 오류 응답·단위 은닉 회귀 PASS |
| NFR-REL-01 | at-least-once·백오프 | §3.4, ADR-09 | `Main/TaskForgeWorker.cpp:34 backoff_with_jitter, :651 handle_processing_failure` | TC-IF-04, IT-10, IT-13, IT-24, IT-25 | 구현; exactly-once 보장 없음 |
| NFR-REL-02 | 원격 timeout·연결 재시도 | §3.4/4 | `Main/TaskForgeWorker.cpp:235 connect_redis, :298 make_client_config; Interface/golang/internal/repository/redisrepo/redisrepo.go:29` | IT-11, IT-25 | 부분: 워커 Redis timeout 없음 |
| NFR-OBS-01 | 구조적 로깅 | §3.5/4 | `Main/main.cpp:45 Logger 시작; Interface/golang/cmd/api/main.go:37` | IT-18 | 부분: 전체 JSON 필드 규약 미충족 |
| NFR-DEP-01 | 독립 이미지·Compose | §4 | `docker/docker-compose.yml:1; docker/Dockerfile.worker:4` | 전용 케이스 없음; 정적/빌드 검증 | 정적 확인·Compose build/E2E 통과 |
| NFR-DEP-02 | 설정 외부화 | ADR-07/12 | `Main/Configurations.cpp:133 load; docker/docker-compose.yml:1` | IT-17 | 구현 |
| CON-01 | SQS 메시징 | §2 | `Main/TaskForgeWorker.cpp:314 connect_aws_service` | 전용 케이스 없음; 정적/빌드 검증 | 정적 확인 |
| CON-02 | CppToolkit 사용 | §3.5 | `Main/CMakeLists.txt:30; .gitmodules:1` | 전용 케이스 없음; 정적/빌드 검증 | 정적 확인·C++ 빌드 |
| CON-03 | Redis 상태 SSOT | §2, ADR-01 | `Main/TaskForgeWorker.cpp:757 set_job_status` | 전용 케이스 없음; 정적/빌드 검증 | 정적 확인·정상 E2E |
| CON-04 | Worker Ubuntu24.04 | §4 | `docker/Dockerfile.worker:4` | 전용 케이스 없음; 정적/빌드 검증 | 정적 확인·Docker build |

## 3. 검증 → 요구사항 역방향

아래 행은 §2와 같은 매핑을 사용합니다. 계약 C-1 stdout_tail의 절단 경계는 `Main/TaskExecutor.cpp:179`와 TC-WRK-03(`Tests/CoreTests.cpp:94`)을 연결합니다. URL 사용 성공은 TTL 만료를 검증하지 않습니다.

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
| TC-WRK-01 | FR-WRK-02, NFR-SEC-01 |
| TC-WRK-02 | FR-WRK-07, FR-WRK-09 |
| TC-WRK-03 | FR-WRK-03, FR-WRK-05 |

## 4. 추적성 검증

31개 요구사항과 논리 시험 케이스 41개를 양방향으로 연결합니다. 각 시험의 자동/수동·전체/부분·미실행은 STP에 명시합니다. CI 네 잡은 `.github/workflows/gates.yml:14`에 정의되어 있고 원격 실행 이력은 저장소 Actions에서 별도 확인합니다. 문서는 `baecce4`부터 Git 추적에 포함되어 있습니다. 최종 코드와 문서 변경은 작성자와 독립 감사자가 분리하여 확인합니다.

## 변경 이력

| 버전 | 일자 | 변경 |
|---|---|---|
| 1.0.0–1.7.0 | 이전 판 | 초기 작성 및 누적 검증 기록 |
| 2.0.0 | 2026-11-07 | 31개 요구사항 유지, 양방향 케이스 매핑 일치, 실측·정적·계획 분리 |
| 2.1.0 | 2026-11-07 | 실재 회귀 테스트와 31개 요구사항의 정·역 매핑·현재 코드 근거 갱신 |
