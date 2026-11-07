# yirang-taskforge 개선 작업 목록

| 항목 | 내용 |
|---|---|
| 문서 ID | YTF-TSK-001 |
| 버전 | 1.1.0 |
| 기준 표준 | ISO/IEC/IEEE 29148:2018, ISO/IEC/IEEE 12207:2017 |
| 작성일 / 최종 수정일 | 2026-11-07 / 2026-11-07 |
| 상태 | Active |
| 대상 커밋 | `21888c7` |

## 1. 범위

사용자가 승인한 현행 프로젝트 정비를 태스크별로 수행하고, 리뷰·테스트 후 개별 커밋합니다. 새 서비스·SaaS 기능·외부 저장소 변경은 이번 목록의 구현 범위에 넣지 않습니다. 근거와 컨셉은 [project_analysis](project_analysis.md), 요구사항 추적은 [RTM](RTM.md)을 따릅니다.

## 2. 완료 태스크

수정 비용은 변경 규모의 상대 평가입니다. 모두 기존 작업 오케스트레이션 컨셉 안에서 수행했습니다. 코드·구조·보안 독립 리뷰의 반영과 더블체크를 마쳤으며 실제 명령·환경·검증 범위는 [VERIFICATION](VERIFICATION.md)에 기록합니다.

| ID | 우선순위·분류 | 완료 내용·근거 | 영향·비용 | 검증 | 커밋·상태 |
|---|---|---|---|---|---|
| T-001 | P0 추적성 | docs Git 추적, 중복 본문 통합·ISO 추적 복원 | docs·README·gitignore, 중 | 독립 문서 감사·ID/링크 대조 | `baecce4`, 완료 |
| T-002 | P1 컨벤션 | `.clang-format:39` Latest, 기존 C++ 포맷 정비 | 설정·Main, 소 | clang-format 19/22·빌드 | `b4f3a05`, 완료 |
| T-003 | P1 종료 | Logger 시작 전 신호 차단, main의 sigwait 후 종료 (`Main/main.cpp:19`) | 진입점, 중 | CTest startup_shutdown·격리 정상 SIGTERM | `f951f41`, 완료 |
| T-004 | P1 실행 제한 | 프로세스 그룹 SIGKILL·자식 신호 마스크 복원 (`Main/TaskExecutor.cpp:26`) | 실행기, 중 | CTest executor: timeout cap·자손·신호 | `fb22b8d`, 완료 |
| T-005 | P1 상태 일관성 | Redis nil/오류 구분·expected 전파·ACK 보류 (`Main/TaskForgeWorker.cpp:738`) | 워커 상태·실패 분기, 중 | 격리 읽기/쓰기 오류·복구·재수신 | `a676d7d`, 완료 |
| T-006 | P1 readiness | 전체 3초 예산·외부 unavailable·내부 경계 로그 | API 3종, 중 | Go deadline/cancel, Python 동시 시작/timeout, JS 가상 3초 | `3ca5174`, 완료 |
| T-007 | P1 입력 처리 | wordcount/checksum dotglob로 숨김 입력 처리 | 태스크 2개, 소 | CTest task_scripts: 숨김/혼합/빈 입력 | `f1d29a9`, 완료 |
| T-008 | P2 구조 | Go Get(value,found,error), JS Pick+기존 Inject 토큰; fake 이중 단언 제거 | Go/JS 서비스·테스트, 중 | 오류/부재·선기록 순서·key 검증·언어 게이트 | `5358496`, 완료 |
| T-009 | P2 정리 | 유휴 우선순위·미사용 상속/종료 future 제거, protected 모니터 위임·통지 대기 | 워커·설정 2종, 중 | 잡 제출/이름 대조·CTest·Docker 종료 | `1994857`, 완료 |
| T-010 | P1 검증 | CTest 5개·복구 4그룹·CI 포맷/복구 게이트 | Tests·CMake·CI, 중 | 로컬/이미지 CTest5·E2E19·복구4 | `479fc6e`, `8003716`, `21888c7`, 완료 |
| T-011 | P1 API 종료 | HTTP drain 뒤 onApplicationShutdown에서 Redis quit | JS 어댑터·회귀, 소 | 실제 Nest HTTP 마지막 쓰기 후 quit | `9516348`, 완료 |

`process_job`의 `message_body`는 실패 재발행 경로에서 사용하므로 보존했습니다. 제거한 priority count 설정은 실제 잡 제출처가 없던 앱 풀에만 해당하며 SQS SDK 내부 풀을 변경하지 않았습니다. 이전 TSK 백로그 중 이미 구현된 항목을 새 미착수 작업으로 되살리지 않습니다.

## 3. 후속 검토·사용자 논의가 필요한 범위

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

## 4. 완료 판정

태스크별 커밋과 최종 로컬 게이트·독립 코드 리뷰 및 문서 최종 감사를 완료했습니다. 원격 Actions 실행 결과는 [검증 보고서](VERIFICATION.md)의 실행 이력 링크에서 별도로 확인하며, 미실행한 검사를 통과로 취급하지 않습니다. 기능 완료는 §3 후속 범위 전체 구현을 의미하지 않습니다.

## 변경 이력

| 버전 | 일자 | 변경 |
|---|---|---|
| 1.0.0 | 2026-11-07 | 이전 중복 백로그 통합, 재현된 결함과 컨셉 밖 후속 검토 분리 |
| 1.1.0 | 2026-11-07 | T-001–011 구현·검증·커밋 완료와 범위 밖 후속 검토 분리 |
