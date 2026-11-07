# yirang-taskforge 문제 보고 및 시정 조치

| 항목 | 내용 |
|---|---|
| 문서 ID | YTF-SPR-001 |
| 버전 | 1.0.0 |
| 기준 표준 | ISO/IEC/IEEE 29119-3:2021, ISO/IEC/IEEE 12207:2017 §6.3.7 |
| 작성일 / 최종 수정일 | 2026-11-07 / 2026-11-07 |
| 상태 | Active |
| 대상 커밋 | `6f7fb67` |

## 1. 기준선 결함

| ID | 관련 요구사항 | 현상·원인 | 재현·근거 | 조치·상태 |
|---|---|---|---|---|
| SPR-001 | FR-OPS-02 | 시그널 핸들러가 기동 중 worker 자원을 해제 | Redis 닫힌 포트 연결 재시도 중 SIGTERM → 종료 코드 -11; `Main/main.cpp:96` | T-003 예정 |
| SPR-002 | FR-WRK-05 | 타임아웃이 부모 PID에만 적용되어 자손 실행 지속 | 1초 timeout 반환 후 자손이 3초 지연 출력 생성; `Main/TaskExecutor.cpp:97` | T-004 예정 |
| SPR-003 | FR-WRK-01/06/10, NFR-REL-01 | 상태 저장 실패를 로그만 남겨 소비 핸들러가 ACK | `Main/TaskForgeWorker.cpp:778` 실패 처리와 `:469` 성공 반환의 도달 경로 확인 | T-005 예정; 장애 주입 회귀 필요 |
| SPR-004 | FR-OPS-01, NFR-SEC-03 | readiness가 probe별 3초를 직렬 누적하며 원시 오류를 노출 | Python 9.01초·JS 9.004초(3개 스톨); `routes.py:62`, `health.controller.ts:30` | T-006 예정 |
| SPR-005 | FR-WRK-04 | 기본 태스크가 숨김 입력 파일을 누락 | `.hidden.txt` 단독 입력 시 두 스크립트 exit 1; `Main/tasks/wordcount.sh:10`, `Main/tasks/checksum.sh:18` | T-007 예정 |
| SPR-006 | 컨벤션·검증 | 포매터 설정 파싱 실패 | clang-format 22.1.8, `.clang-format:39` `unknown enumerated scalar` | T-002 예정 |
| SPR-007 | FR-OPS-02 | JS Redis 연결을 HTTP drain 이전에 닫아 진행 중 요청 실패 가능 | `Interface/js/src/infra/redis.service.ts:48` onModuleDestroy; 상위 세션의 Nest 수명주기 확인 | T-011 예정; 실제 drain 후 quit 회귀 계획 |

SPR-001/002/005는 리뷰 에이전트가 격리 프로브로, SPR-004/006은 주 세션이 실행으로 재현했습니다. SPR-003은 기준선 코드 경로로 확인했으며 동적 복구 시험을 조치 완료 조건으로 둡니다. 테스트 결과의 현재 기준은 [VERIFICATION](VERIFICATION.md)입니다.

## 2. 종료 기준

각 문제는 원인 수정, 재현 회귀 테스트 통과, 독립 코드 리뷰, 관련 RTM 갱신을 마친 뒤 종결합니다. 진행·커밋 기록은 [task_list](task_list.md)에서 관리합니다.

## 변경 이력

| 버전 | 일자 | 변경 |
|---|---|---|
| 1.0.0 | 2026-11-07 | 기준선 결함 7건과 검증 조건 등록 |
