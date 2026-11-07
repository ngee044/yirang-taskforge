# yirang-taskforge 소프트웨어 테스트 계획서 (STP)

| 항목 | 내용 |
|---|---|
| 문서 ID | YTF-STP-001 |
| 버전 | 2.0.0 |
| 기준 표준 | ISO/IEC/IEEE 29119-3:2021 |
| 작성일 | 2026-11-07 (현행화 판) |
| 최종 수정일 | 2026-11-07 |
| 상태 | Active — 사용자 승인 여부를 뜻하지 않음 |
| 대상 커밋 | `6f7fb67` |

## 1. 실행 게이트

명령은 저장소 루트 또는 표의 서비스 디렉터리에서 실행합니다. 도구가 없거나 검증이 실행되지 않았으면 PASS로 표시하지 않습니다. 실제 결과는 [VERIFICATION](VERIFICATION.md)에 기록하며 이 문서는 검증 범위와 판정 기준을 정의합니다.

| 대상 | 필수 실행 | 현재 자동화 |
|---|---|---|
| C++ | `./build.sh`, 변경 소스 clang-format dry-run, 관련 CTest | 기준선 CTest 0건, 포맷 설정 결함 T-002. C++ 컴파일은 Compose CI에도 포함 |
| Go | `gofmt -l .`, `go build ./...`, `go vet ./...`, `go test -race ./...` | `Interface/golang`, CI go 잡 |
| Python | `ruff check .`, `ruff format --check .`, `pytest -q` | `Interface/python`, CI는 ruff check·pytest. format/mypy 규약과 차이 있음 |
| JS | `npm run build`, `npm run lint`, `npm test` | `Interface/js`, CI js 잡 |
| 통합 | `docker compose -f docker/docker-compose.yml build`, `up -d`, `bash docker/scripts/e2e_test.sh` | CI compose 잡 |

컨벤션을 기준선의 미충족 상태에 맞춰 낮추지 않습니다. Go 추가 린터·Python mypy/lock·JS audit 등의 도입 상태는 각 [언어 규약](ISO_MSA_CONVENTION.md)과 task_list에 구분합니다.

## 2. 테스트 케이스

기존 TC-IF-01–08, IT-01–25를 유지합니다. 수동 계획은 재현 절차와 기대 결과이며 이번 커밋의 PASS를 뜻하지 않습니다. IT-26–30은 이번 리뷰에서 확인한 결함의 회귀 후보입니다.

| ID | 절차·기대 결과 | 요구사항 | 구현·상태 |
|---|---|---|---|
| TC-IF-01 | 계약 B 메시지의 주요 필드 생성 | FR-JOB-01, FR-QUE-01 | 자동: Go/Python/JS 서비스 테스트 |
| TC-IF-02 | 입력별 presigned PUT URL·S3 key | FR-JOB-02 | 자동 부분: 3종. Go는 bucket/method를 확인하나 key 값 단언은 없음 |
| TC-IF-03 | SQS 발행 이전 queued 기록 | FR-QUE-01 | 부분: 3종은 호출 완료 후 저장값만 확인. 호출 순서 단언 추가 필요 |
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
| IT-08 | 스키마 위반 5종 직접 주입; 유효 job_id면 failed, 잘못된 메시지는 폐기 | FR-WRK-02 | 수동 계획; 이번 기준선 미실행 |
| IT-09 | job_id 경로 탈출 입력; 외부 파일/디렉터리 보존 | FR-WRK-02, NFR-SEC-01 | 수동 계획; 이번 미실행 |
| IT-10 | 재시도 중 DLQ 파일 제거; 메시지 카운트로 상한 유지 | FR-WRK-07, NFR-REL-01 | 수동 계획; 이번 미실행 |
| IT-11 | Redis/AWS 스톨 중 API 생성·조회가 유계 5xx로 종료 | NFR-REL-02 | 수동 계획; 모든 3종 경로 재실측 필요 |
| IT-12 | Redis/SQS/S3 동시 스톨; 전체 3초 503·내부 오류 은닉 | FR-OPS-01, NFR-SEC-03 | 실측 FAIL: Python/JS 약 9초. 회귀 자동화 예정 T-006 |
| IT-13 | 재시도 대상 실패 간격에 지수 백오프·jitter 적용 | NFR-REL-01 | 수동 계획; 이번 미실행 |
| IT-14 | done 메시지 재주입; 상태와 실행 횟수 불변 | FR-WRK-10 | 수동 계획; 이번 미실행 |
| IT-15 | 재시도 후 성공; DLQ 제거·failure_reason 삭제·계약 필드 보존 | FR-WRK-06, FR-WRK-09 | 수동 계획; 이번 미실행 |
| IT-16 | jobs/task_catalog 조회 거부; 비-job Redis 키 비노출 | FR-JOB-03, NFR-SEC-01 | 수동 계획; 이번 미실행 |
| IT-17 | 필수 키 3종·설정 파일 부재 각각 exit 1 | FR-OPS-03, NFR-DEP-02 | 수동 계획; 이번 전체 조건 미실행 |
| IT-18 | docker logs에서 워커 최신 로그 가시성 | NFR-OBS-01 | 수동 계획; 구조적 필드/JSON 규약 충족까지 검증하지 않음 |
| IT-19 | 재발행 실패; failed와 DLQ 파일 제거 | FR-WRK-07, FR-WRK-09 | 수동 계획; 이번 미실행 |
| IT-20 | 초기 Redis 문서 없이 직접 발행; task_name/enqueued_at 복원 | FR-WRK-06 | 수동 계획; 이번 미실행 |
| IT-21 | task 없는 스키마 위반; enqueued_at만 복원 | FR-WRK-02, FR-WRK-06 | 수동 계획; 이번 미실행 |
| IT-22 | 실행·백오프 중 SIGTERM; 유예 내 종료와 작업 상태 | FR-OPS-02 | 수동 계획; 모든 실행 단계 미검증 |
| IT-23 | 미등록 태스크 failed 필드와 DLQ 잔량 | FR-WRK-03, FR-WRK-06, FR-WRK-09 | 수동 계획; retry_count 부재만으로 실패 유형 일반화 금지 |
| IT-24 | 미등록 태스크 선행 시 정상 작업 지연 | FR-WRK-03, NFR-REL-01 | 수동 계획; 이번 미실행 |
| IT-25 | S3 입력 미업로드; 다운로드 3회·재시도 소진 retry_count=4 | FR-WRK-07, NFR-REL-01, NFR-REL-02 | 수동 계획; 이번 미실행 |
| IT-26 | Redis 연결 대기 중 SIGTERM; crash 없이 종료 | FR-OPS-02 | 실측 FAIL -11. 자동 회귀 예정 T-003 |
| IT-27 | timeout 후 자손의 지연 파일 생성이 중단됨 | FR-WRK-05 | 실측 FAIL. 자동 회귀 예정 T-004 |
| IT-28 | Redis 조회/쓰기 장애 시 ACK 보류, 복구 후 상태 종결 | FR-WRK-01, FR-WRK-06, FR-WRK-10 | 계획 T-005. 기준선은 정적 결함 확인 |
| IT-29 | 숨김파일 단독/혼합·빈 입력의 태스크 결과 | FR-WRK-03, FR-WRK-04 | 실측 FAIL. 자동 회귀 예정 T-007 |
| IT-30 | JS HTTP 처리 완료 후 Redis 연결 해제 | FR-OPS-02 | 계획 T-011. 프레임워크 수명주기 순서로 결함 확인 |

## 3. 자동 테스트 근거와 한계

| 구현 | 기준선 근거 | 직접 검증하는 범위 |
|---|---|---|
| Go service | `Interface/golang/internal/service/job_test.go:67` `TestCreateJobBuildsContractMessage` | 생성 계약·큐 실패·작업 부재·카탈로그 정상 조회 |
| Go binding | `Interface/golang/internal/http/handlers/jobs_test.go:9` | timeout 다섯 경계 조건 |
| Python service | `Interface/python/tests/test_job_service.py:68` | 생성 계약·큐 실패·작업 부재·filename 거부, 4건 |
| JS service | `Interface/js/test/jobs.service.spec.ts:63` | 생성 계약·null timeout 생략·큐 실패·작업 부재, 4건 |
| Compose E2E | `docker/scripts/e2e_test.sh:63` `test_api`, `:142` 미등록 태스크 | API별 정상 6개 검사×3 + 최종 failed 1개 = 19 |

TC-IF-03은 세 테스트 모두 서비스 호출이 끝난 뒤 queued 값을 읽으므로 선기록 순서 자체는 단언하지 않습니다. 코드의 호출 순서 확인과 테스트 증거를 혼동하지 않습니다. C++ stdout 절단 경계, 여러 실패 분기, 중간 상태 전이는 정상 E2E만으로 검증되지 않습니다.

## 4. 검증 완료 기준

관련 회귀 테스트와 언어 게이트 통과, 컨벤션 독립 리뷰·작성자 조치·더블체크, 문서 추적성 재감사 후 완료로 판정합니다. SQS/Redis/REST 계약 변경은 세 API와 워커의 정합성을 함께 확인합니다. 새 테스트가 추가되면 위 표의 계획을 실제 경로·케이스명·실행 결과로 대체합니다. 최종 Compose E2E를 실패한 상태에서 푸시하지 않습니다.

## 변경 이력

| 버전 | 일자 | 변경 |
|---|---|---|
| 1.0.0–1.7.0 | 이전 판 | 초기 작성 및 누적 검증 기록 |
| 2.0.0 | 2026-11-07 | 케이스 ID 보존, 자동·부분·수동 계획 분리, IT-26–30 회귀 후보 추가 |
