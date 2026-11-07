# yirang-taskforge 소프트웨어 테스트 계획서 (STP)

| 항목 | 내용 |
|---|---|
| 문서 ID | YTF-STP-001 |
| 버전 | 2.1.0 |
| 기준 표준 | ISO/IEC/IEEE 29119-3:2021 |
| 작성일 | 2026-11-07 (현행화 판) |
| 최종 수정일 | 2026-11-07 |
| 상태 | Active — 사용자 승인 여부를 뜻하지 않음 |
| 대상 커밋 | `21888c7` |

## 1. 실행 게이트

실행 결과·환경·종료 코드는 [VERIFICATION](VERIFICATION.md)이 기준입니다. CI는 `.github/workflows/gates.yml`의 실제 명령을 따릅니다.

| 대상·디렉터리 | 필수 실행 | 자동화 |
|---|---|---|
| C++, 루트 | `./build.sh`; `clang-format-19 --dry-run --Werror Main/*.cpp Main/*.h Tests/*.cpp`; `ctest --test-dir build --output-on-failure --no-tests=error` | CTest5, CI 포맷·worker 이미지 빌드 안 CTest |
| Go, Interface/golang | `gofmt -l .`, `go build ./...`, `go vet ./...`, `go test ./... -race -count=1` | CI go |
| Python, Interface/python | `.venv/bin/ruff check . ../../Tests/worker_recovery_test.py`, `.venv/bin/ruff format --check . ../../Tests/worker_recovery_test.py`, `.venv/bin/pytest -q` | CI python은 설치한 동일 명령을 실행. 복구 스크립트에도 이 프로젝트 설정 적용 |
| JS, Interface/js | `npm run build`, `npm run lint`, `npm run format:check`, `npm test` | CI js |
| 통합, 루트 | Compose build/up, `bash docker/scripts/e2e_test.sh`, `python3 Tests/worker_recovery_test.py` | CI compose |

복구 스크립트는 기존 LocalStack과 worker 이미지가 필요합니다. UUID 이름의 별도 FIFO 큐·Redis·worker를 생성하고 자신이 만든 자원만 finally에서 정리합니다. 테스트 실패를 숨기지 않으며 이미 있는 스택을 초기화하지 않습니다. Python lock/mypy·Go 추가 린터·JS audit 미도입은 규약 미충족으로 구분합니다.

## 2. 테스트 케이스

기존 TC-IF-01–08/IT-01–30을 유지하고 C++ 구성요소 케이스 TC-WRK-01–03을 추가했습니다. 논리 케이스 41개와 실제 runner의 CTest5/pytest8/Jest9는 서로 다른 집계입니다. 수동 계획은 미실행 상태를 유지하며 부분 단언을 전체 통과로 확대하지 않습니다.

| ID | 절차·기대 결과 | 요구사항 | 구현·현재 상태 |
|---|---|---|---|
| TC-IF-01 | 계약 B 메시지의 주요 필드 생성 | FR-JOB-01, FR-QUE-01 | 자동: Go/Python/JS 서비스 테스트 |
| TC-IF-02 | 입력별 presigned PUT URL·S3 key | FR-JOB-02 | 자동 PASS: Go/JS는 presign 인자 key, Python은 URL 개수·PUT·첫 메시지 download key 확인(FakeS3.keys 미단언) |
| TC-IF-03 | SQS 발행 이전 queued 기록 | FR-QUE-01 | Go/JS 선기록 호출 순서·저장 실패 시 미발행 자동 PASS. Python은 종료 후 값만 확인 |
| TC-IF-04 | 큐 발행 실패가 도메인 오류로 전파 | NFR-REL-01 | 자동: Go/Python/JS |
| TC-IF-05 | 존재하지 않는 작업 조회 오류 | FR-JOB-03 | 자동: Go/Python/JS |
| TC-IF-06 | filename 경로 탈출 거부 | NFR-SEC-01 | 자동: Python만. Go/JS는 구현 확인 |
| TC-IF-07 | timeout 부재/null/30 수락, 0/3601 거부 | NFR-PORT-01 | 자동: Go HTTP 바인딩 테이블 5조건 |
| TC-IF-08 | null timeout을 계약 B에서 생략 | FR-QUE-01, NFR-PORT-01 | 자동: JS만 |
| IT-01 | API 3종 정상 readyz | FR-OPS-01 | 자동 E2E, 이번 PASS |
| IT-02 | API 3종 카탈로그 wordcount 노출 | FR-JOB-04, FR-WRK-08 | 자동 E2E, 이번 PASS |
| IT-03 | API 3종 job 접수 202·UUID·upload URL | FR-JOB-01, FR-JOB-02 | 자동 E2E, 이번 PASS |
| IT-04 | API 3종 presigned PUT 업로드 | FR-JOB-02, NFR-SEC-02 | 자동 E2E, 이번 PASS. 만료 경계는 미검증 |
| IT-05 | API 3종 작업이 done 도달 | FR-JOB-03, FR-QUE-01, FR-WRK-01, FR-WRK-03, FR-WRK-04, FR-WRK-05, FR-WRK-06 | 자동 E2E, 이번 PASS. 중간 running 전이·timeout 초과는 미단언 |
| IT-06 | API 3종 결과 URL 다운로드·내용 5 | FR-WRK-04, NFR-SEC-02 | 자동 E2E, 이번 PASS. URL 만료는 미검증 |
| IT-07 | 미등록 태스크 최종 failed | FR-WRK-03 | 자동 E2E, 이번 PASS. 즉시 종결·retry_count·DLQ 잔량은 미단언 |
| IT-08 | 스키마 위반 5종 직접 주입; 유효 job_id면 failed, 잘못된 메시지는 폐기 | FR-WRK-02 | 수동 직접 주입 미실행; TC-WRK-01이 validator 일부 경계를 자동 검증 |
| IT-09 | job_id 경로 탈출 입력; 외부 파일/디렉터리 보존 | FR-WRK-02, NFR-SEC-01 | 수동 파일 보존 시나리오 미실행; TC-WRK-01이 경로 ID 거부 자동 검증 |
| IT-10 | 재시도 중 DLQ 파일 제거; 메시지 카운트로 상한 유지 | FR-WRK-07, NFR-REL-01 | 전체 SQS 재시도 사슬 미실행; TC-WRK-02로 카운트 소스·손상 파일 처리 부분 검증 |
| IT-11 | Redis/AWS 스톨 중 API 생성·조회가 유계 5xx로 종료 | NFR-REL-02 | 수동 계획; 모든 3종 경로 재실측 필요 |
| IT-12 | Redis/SQS/S3 동시 스톨; 전체 3초 503·내부 오류 은닉 | FR-OPS-01, NFR-SEC-03 | 자동 PASS: Go deadline/cancel·은닉, Python 동시 시작/timeout=0 경계·은닉, JS 가상 3000ms·은닉 |
| IT-13 | 재시도 대상 실패 간격에 지수 백오프·jitter 적용 | NFR-REL-01 | 수동 계획; 현재 미실행 |
| IT-14 | done 메시지 재주입; 상태와 실행 횟수 불변 | FR-WRK-10 | 자동 복구 그룹3 PASS: 터미널 스킵 로그·상태 문서 불변 확인. 실제 실행 횟수 계측은 미실시 |
| IT-15 | 재시도 후 성공; DLQ 제거·failure_reason 삭제·계약 필드 보존 | FR-WRK-06, FR-WRK-09 | 수동 계획; 현재 미실행 |
| IT-16 | jobs/task_catalog 조회 거부; 비-job Redis 키 비노출 | FR-JOB-03, NFR-SEC-01 | 수동 계획; 현재 미실행 |
| IT-17 | 필수 키 3종·설정 파일 부재 각각 exit 1 | FR-OPS-03, NFR-DEP-02 | 수동 계획; 이번 전체 조건 미실행 |
| IT-18 | docker logs에서 워커 최신 로그 가시성 | NFR-OBS-01 | 복구 스크립트가 실패 경계 로그를 관측. 전체 JSON 구조/공통 필드는 미검증 |
| IT-19 | 재발행 실패; failed와 DLQ 파일 제거 | FR-WRK-07, FR-WRK-09 | 수동 계획; 현재 미실행 |
| IT-20 | 초기 Redis 문서 없이 직접 발행; task_name/enqueued_at 복원 | FR-WRK-06 | 직접 정상 작업의 task_name/enqueued_at 복원은 복구 그룹3 PASS; 미등록 failed 전 필드는 미검증 |
| IT-21 | task 없는 스키마 위반; enqueued_at만 복원 | FR-WRK-02, FR-WRK-06 | 수동 계획; 현재 미실행 |
| IT-22 | 실행·백오프 중 SIGTERM; 유예 내 종료와 작업 상태 | FR-OPS-02 | 복구 그룹4 정상 유휴 SIGTERM PASS. 장기 실행·백오프 중 종료는 미검증 |
| IT-23 | 미등록 태스크 failed 필드와 DLQ 잔량 | FR-WRK-03, FR-WRK-06, FR-WRK-09 | 수동 계획; retry_count 부재만으로 실패 유형 일반화 금지 |
| IT-24 | 미등록 태스크 선행 시 정상 작업 지연 | FR-WRK-03, NFR-REL-01 | 수동 계획; 현재 미실행 |
| IT-25 | S3 입력 미업로드; 다운로드 3회·재시도 소진 retry_count=4 | FR-WRK-07, NFR-REL-01, NFR-REL-02 | 수동 계획; 현재 미실행 |
| IT-26 | Redis 연결 대기 중 SIGTERM; crash 없이 종료 | FR-OPS-02 | 자동 CTest startup_shutdown PASS: 닫힌 Redis 연결 재시도 중 SIGTERM, exit 1 정리 |
| IT-27 | timeout 후 자손의 지연 파일 생성이 중단됨 | FR-WRK-05 | 자동 CTest executor PASS: 요청30초를 기본1초로 cap, 자손 지연 파일 미생성·신호 마스크 복원 |
| IT-28 | Redis 조회/쓰기 장애 시 ACK 보류, 복구 후 상태 종결 | FR-WRK-01, FR-WRK-06, FR-WRK-10 | 자동 복구 그룹1/2 PASS: Redis write 실패·WRONGTYPE read 오류·값 보존·nil 복구·ACK |
| IT-29 | 숨김파일 단독/혼합·빈 입력의 태스크 결과 | FR-WRK-03, FR-WRK-04 | 자동 CTest task_scripts PASS: 숨김 단독·혼합·빈 입력, 출력 내용 검증 |
| IT-30 | JS HTTP 처리 완료 후 Redis 연결 해제 | FR-OPS-02 | 자동 Jest redis.shutdown.spec.ts PASS: 실제 Nest HTTP drain 후 quit |
| TC-WRK-01 | 유효 메시지·job_id 경로/길이·인자 타입·SQS null timeout·bucket 부재 검증 | FR-WRK-02, NFR-SEC-01 | CTest validation PASS; Tests/CoreTests.cpp:50 |
| TC-WRK-02 | 초기/메시지/로컬 카운트·재시도 상한·종결 제거·손상/쓰기 불가 파일 | FR-WRK-07, FR-WRK-09 | CTest dlq PASS; Tests/CoreTests.cpp:74 |
| TC-WRK-03 | whitelist·정상/비영 exit·UTF-8 stdout 절단·timeout cap·자손·신호 마스크 | FR-WRK-03, FR-WRK-05 | CTest executor PASS; Tests/CoreTests.cpp:94; 계약 C-1 stdout 포함 |

## 3. 자동 테스트의 실제 범위

| 구현 | 근거 | 확인 범위 |
|---|---|---|
| Go 서비스 | `Interface/golang/internal/service/job_test.go:77` | 메시지·실제 presign key·set→enqueue 순서·저장 실패 미발행·조회 오류/부재·카탈로그 정상/부재 |
| Go HTTP | `Interface/golang/internal/http/handlers/jobs_test.go:9`, `health_test.go:20` | timeout 다섯 조건, readiness 공유 deadline·요청 취소·고정 오류 |
| Python | `Interface/python/tests/test_job_service.py:68`, `test_health.py:30` | 기존 서비스4·health4. 동시 시작과 timeout=0 경계이며 원격 3초 스톨 전체 재실측은 아님 |
| JS | `Interface/js/test/jobs.service.spec.ts:69`, `health.controller.spec.ts:25`, `redis.shutdown.spec.ts:18` | 서비스5·health3·실제 HTTP drain1. readiness는 가상시계 3000ms |
| C++/셸 | `Tests/CMakeLists.txt:12`, `Tests/CoreTests.cpp:50`, `Tests/startup_shutdown_test.sh:20`, `Tests/task_scripts_test.sh:9` | CTest validation/dlq/executor/task_scripts/startup_shutdown 5개 |
| Compose | `docker/scripts/e2e_test.sh:63` | 정상6×API3 + 미등록 최종 failed =19. 중간 running·미등록 즉시성 미단언 |
| 격리 장애 | `Tests/worker_recovery_test.py:143` | 쓰기 실패·읽기 오류·정상/터미널 재수신·정상 종료 4그룹 |

TC-IF-03의 Python 테스트는 서비스가 끝난 뒤 저장된 값을 확인하므로 호출 순서 자체의 자동 검증은 남아 있습니다. stdout_tail 절단 경계는 TC-WRK-03으로 자동 검증하지만 임의 바이너리 정화는 범위 밖입니다.

## 4. 검증 완료 기준

관련 회귀 테스트와 언어 게이트 통과, 컨벤션 독립 리뷰·작성자 조치·더블체크, 문서 추적성 재감사 후 완료로 판정합니다. SQS/Redis/REST 계약 변경은 세 API와 워커의 정합성을 함께 확인합니다. 새 테스트가 추가되면 위 표의 계획을 실제 경로·케이스명·실행 결과로 대체합니다. 최종 Compose E2E를 실패한 상태에서 푸시하지 않습니다.

## 변경 이력

| 버전 | 일자 | 변경 |
|---|---|---|
| 1.0.0–1.7.0 | 이전 판 | 초기 작성 및 누적 검증 기록 |
| 2.0.0 | 2026-11-07 | 케이스 ID 보존, 자동·부분·수동 계획 분리, IT-26–30 회귀 후보 추가 |
| 2.1.0 | 2026-11-07 | 최종 runner·41개 논리 케이스·실제 단언과 미실행 수동 시험 동기화 |
