# yirang-taskforge 문제 보고 및 시정 조치

| 항목 | 내용 |
|---|---|
| 문서 ID | YTF-SPR-001 |
| 버전 | 1.1.0 |
| 기준 표준 | ISO/IEC/IEEE 29119-3:2021, ISO/IEC/IEEE 12207:2017 §6.3.7 |
| 작성일 / 최종 수정일 | 2026-11-07 / 2026-11-07 |
| 상태 | Active |
| 대상 커밋 | `21888c7` |

## 1. 시정 결과

초기 재현은 `6f7fb67`, 시정 코드는 `21888c7`에 포함됩니다. 결함을 수정한 커밋과 회귀 테스트를 함께 연결하며 과거 실패를 현재 결함으로 표시하지 않습니다.

| ID | 요구사항·초기 문제 | 시정 근거 | 회귀·종결 |
|---|---|---|---|
| SPR-001 | FR-OPS-02: 기동 중 SIGTERM -11 | `Main/main.cpp:19` 신호 차단·sigwait, `f951f41` | IT-26 startup_shutdown·정상 종료, 종결 |
| SPR-002 | FR-WRK-05: timeout 뒤 자손 실행 | `Main/TaskExecutor.cpp:26`, `:125` 프로세스 그룹·마스크, `fb22b8d` | IT-27 executor, 종결 |
| SPR-003 | FR-WRK-01/06/10: 상태 오류 후 ACK | `Main/TaskForgeWorker.cpp:738` optional 조회, `:757` 저장 expected, `a676d7d` | IT-28 읽기/쓰기 실패 NACK·값 보존·복구 ACK, 종결 |
| SPR-004 | FR-OPS-01/NFR-SEC-03: readiness 약 9초·raw 오류 | Python/JS 병렬 3초 제한, 3종 unavailable, `3ca5174` | IT-12 언어별 회귀, 종결 |
| SPR-005 | FR-WRK-04: 숨김 입력 누락 | 두 태스크 dotglob, `f1d29a9` | IT-29 task_scripts, 종결 |
| SPR-006 | 컨벤션: 포매터 설정 파싱 실패 | `.clang-format:39` Latest, `b4f3a05` | clang-format 19/22 dry-run, 종결 |
| SPR-007 | FR-OPS-02: Redis를 HTTP drain 전에 해제 | `Interface/js/src/infra/redis.service.ts:49` onApplicationShutdown, `9516348` | IT-30 실제 Nest HTTP drain 테스트, 종결 |

## 2. 검증 강도와 잔여

각 시정은 독립 코드·구조·보안 리뷰와 관련 회귀를 통과했습니다. CTest는 등록 5/실행 5/PASS 5/FAIL 0/SKIP 0이며 언어별 검사는 [VERIFICATION](VERIFICATION.md)을 참조합니다. IT-12의 Go deadline/취소·Python 동시 시작/즉시 timeout·JS 가상시간 검증을 실제 원격 스톨 전체 실측과 혼동하지 않습니다.

워커 Redis I/O timeout 미지원, done 기록 실패 뒤 재실행 가능성, 장기 작업 종료 상한·Windows 미지원은 위 결함 종결로 해소되는 항목이 아닙니다. 후속 범위는 [task_list](task_list.md)에서 유지합니다.

## 변경 이력

| 버전 | 일자 | 변경 |
|---|---|---|
| 1.0.0 | 2026-11-07 | 기준선 결함 7건과 검증 조건 등록 |
| 1.1.0 | 2026-11-07 | 7개 문제의 수정 커밋·회귀·종결과 잔여 범위 기록 |
