# yirang-taskforge 개선 작업 목록

| 항목 | 내용 |
|---|---|
| 문서 ID | YTF-TSK-001 |
| 버전 | 1.0.0 |
| 기준 표준 | ISO/IEC/IEEE 29148:2018, ISO/IEC/IEEE 12207:2017 |
| 작성일 / 최종 수정일 | 2026-11-07 / 2026-11-07 |
| 상태 | Active |
| 대상 커밋 | `6f7fb67` |

## 1. 범위

사용자가 승인한 현행 프로젝트 정비를 태스크별로 수행하고, 리뷰·테스트 후 개별 커밋합니다. 새 서비스·SaaS 기능·외부 저장소 변경은 이번 목록의 구현 범위에 넣지 않습니다. 근거와 컨셉은 [project_analysis](project_analysis.md), 요구사항 추적은 [RTM](RTM.md)을 따릅니다.

## 2. 실행 태스크

모든 작업은 기존 작업 오케스트레이션 컨셉에 부합합니다. 비용은 변경 규모의 상대 평가입니다.

| ID | 우선순위·분류 | 근거·조치 | 영향 범위·비용 | 선행 조건 | 검증 | 상태 |
|---|---|---|---|---|---|---|
| T-001 | P0 추적성·문서 | 구 문서의 과거/현재 혼재 제거, `docs/` 형상 관리 복원 | docs·README·`.gitignore`, 중 | 기준선 파악 | 독립 추적성 감사, 링크·ID 대조 | 진행 중 |
| T-002 | P1 컨벤션 | `.clang-format:39` 파싱 실패 복구, 변경 코드 포맷 검사 실효화 | 포매터 설정·C++ 포맷·CI, 소 | T-001 | clang-format 설정 파싱·dry-run, C++ 빌드 | 미착수 |
| T-003 | P1 결함 | `Main/main.cpp:96` 신호 처리 중 Logger·stop 호출 제거; 기동 중 종료 충돌 방지 | 워커 진입점·종료 회귀검증, 중 | T-002 | Redis 연결 실패 중 SIGTERM, 정상 종료·빌드 | 미착수 |
| T-004 | P1 결함 | `Main/TaskExecutor.cpp:97` 타임아웃 시 자손 프로세스까지 실행 제한 | TaskExecutor·회귀테스트, 중 | T-002 | 자손의 지연 파일 생성이 취소되는지, 정상 태스크 | 미착수 |
| T-005 | P1 결함 | `Main/TaskForgeWorker.cpp:740` Redis 조회·저장 오류 전파; 상태 미확정 메시지 ACK 보류 | 워커 상태·실패 분기·SRS/SAD/API, 중 | T-002 | Redis 장애 주입, 복구 후 재수신·상태 종결, E2E | 미착수 |
| T-006 | P1 계약 | Python `routes.py:62`·JS `health.controller.ts:30`의 전체 readiness 3초 제한; 3종 raw 오류 은닉 | API health 3종·테스트·API A-4, 중 | T-001 | 의존성 3개 동시 스톨, 503 envelope·민감정보 비노출 | 미착수 |
| T-007 | P1 결함 | `Main/tasks/wordcount.sh:10`·`checksum.sh:18` 숨김파일 누락 수정 | 태스크 2개·회귀검증, 소 | T-001 | 숨김파일 단독·혼합 입력, 빈 입력, 결과 내용 | 미착수 |
| T-008 | P2 구조 | Go `service/job.go:13` Redis sentinel 의존 분리; JS `jobs.service.spec.ts:27` fake 타입 검사 복구 | Go 서비스/어댑터·JS 서비스/test, 중 | T-001 | 조회 not-found/오류 회귀, Go race·TS strict | 미착수 |
| T-009 | P2 정리 | `Main/TaskForgeWorker.cpp:230` 유휴 우선순위 워커·설정과 `TaskForgeWorker.h:22` 미사용 상속 제거 | 워커·Configurations·JSON 2종, 중 | T-003/T-005 | 잡 제출 검색, 워커 기동·DLQ 모니터·빌드 | 미착수 |
| T-010 | P1 검증 | CMake에 C++ 자동 회귀 테스트가 없는 공백 보완 | 관련 테스트·CMake·STP/RTM, 중 | T-003~T-007 | CTest·언어별 게이트·Compose E2E, 독립 리뷰 | 미착수 |
| T-011 | P1 수명주기 | `Interface/js/src/infra/redis.service.ts:48` onModuleDestroy가 HTTP drain보다 먼저 종료되어 진행 중 요청 실패 | Redis 어댑터·Nest drain 회귀, 소 | T-001 | 실제 HTTP 처리 후 quit 순서·JS 전체 게이트 | 미착수 |

`process_job`의 `message_body` 인자는 기준선에서 미사용이지만 T-005 실패 처리 재배치에 필요할 수 있으므로 독립 삭제 태스크로 확정하지 않습니다.

## 3. 기존 백로그 통합

이전 ASSESSMENT/EVALUATION/REVIEW_HANDOFF에 반복되던 항목은 다음으로 정리합니다. 완료된 코드를 다시 미착수로 분류하지 않습니다.

| 이전 항목 | 현재 처리 |
|---|---|
| TSK-001·026 문서 제외/포맷 설정 | T-001·T-002 |
| TSK-006 시그널·TSK-044 자손 실행 | T-003·T-004 |
| TSK-016·038 C++ 테스트/상태 처리 | T-005·T-010 |
| TSK-030 readiness 오류 노출 | T-006 |
| TSK-039 Go 어댑터 의존·TSK-029 모니터 자원 | T-008·T-009 |
| 이미 반영된 job_id 검증·재시도 카운트 판독·기동 fail-fast·CI·timeout 0/null 정합화 | 기준선 구현에 포함; 재수정 작업으로 등록하지 않음 |

## 4. 후속 검토·사용자 논의가 필요한 범위

| 항목 | 근거·제약 | 후속 조건 |
|---|---|---|
| 워커 Redis I/O timeout | `.CppToolkit/Redis/RedisConnector.cpp:27`의 연결 옵션에 timeout이 없고 공개 생성자에 옵션 주입 경로가 없음 | CppToolkit 별도 저장소 API 변경과 릴리스 협의 |
| 업로드 완료 전 SQS 발행·직렬 백오프 | API가 업로드 URL과 함께 이미 발행한 job을 반환; `download_inputs`·재발행 대기가 같은 소비 루프 점유 | 업로드 완료 계약/허용 창·처리량 요구 확정. 다운로드 마지막 대기만 제거하면 기존 업로드 창이 짧아짐 |
| 입력 크기·배열 수·파일명 길이 단위 통합 | Go는 filename 바이트 길이, Python/JS는 문자 길이; JS만 배열 상한이 있음 | 계약 A 상한·호환 정책 확정 후 3종 동시 변경 |
| SQS 생산자 신뢰 경계 강화 | 직접 메시지의 bucket/prefix/job_id가 REST 발급 규칙과 일치하는지까지는 검사하지 않음 | 신뢰 경계·호환 정책을 먼저 문서화 |
| Python 잠금 파일·정적 타입 검사 | pyproject 의존성은 범위이며 lock/mypy 설정 없음 | 실행 환경·락 도구 결정, 규약 미충족을 완료로 표시하지 않음 |
| 로그 제어문자 | 외부 문자열 로그 경로에 별도 검토 필요 | 실제 재현과 로그 정책 확인 후 판단 |
| 장기 롱폴·대용량 S3·부하/복구 시험 | 현재 설정 범위를 넘는 시간·크기 상한은 검증되지 않음 | 부하 목표·리소스 예산 확정 |
| SaaS 콘솔·인증·과금·추적 백엔드 | 현행 실행 서비스의 컨셉을 확장함 | [NEXTJS_SAAS_TASK_LIST](NEXTJS_SAAS_TASK_LIST.md) 별도 검토 |

## 5. 완료 조건

관련 회귀 테스트와 언어별 빌드·포맷 게이트 통과, `$code-review` 독립 리뷰와 더블체크, SRS/SAD/STP/RTM 정합성 확인 후 커밋합니다. 최종 전체 테스트가 실패하면 수정·재검증하고 푸시합니다. 단순 코드 변경만으로 상태를 완료로 올리지 않습니다.

## 변경 이력

| 버전 | 일자 | 변경 |
|---|---|---|
| 1.0.0 | 2026-11-07 | 이전 중복 백로그 통합, 재현된 결함과 컨셉 밖 후속 검토 분리 |
