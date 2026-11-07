# yirang-taskforge 검증 결과

| 항목 | 내용 |
|---|---|
| 문서 ID | YTF-VER-001 |
| 버전 | 2.0.0 |
| 기준 표준 | ISO/IEC/IEEE 29119-3:2021 |
| 작성일 | 2026-11-07 (현행화 판) |
| 최종 수정일 | 2026-11-07 |
| 상태 | Active — 사용자 승인 여부를 뜻하지 않음 |
| 대상 커밋 | `6f7fb67` |

## 1. 검증 기준

2026-11-07 기준선 `6f7fb67`의 로컬 검사입니다. 후속 태스크 변경에 대한 최종 검증 결과와 혼합하지 않습니다. 실행은 상위 작업 세션과 독립 검증 에이전트가 수행했으며 문서 작성자가 실행하지 않은 항목은 그 결과 전달을 근거로 기록합니다. 테스트 계획은 [STP](STP.md), 매핑은 [RTM](RTM.md), 열린 결함은 [SPR](SPR.md)입니다.

## 2. 이번 실행 결과

| 대상 | 명령·범위 | 결과·제한 |
|---|---|---|
| C++ | `./build.sh` | PASS. 기존 CMake 캐시의 lz4 탐색 실패는 `cmake --fresh` 후 재빌드로 복구. 소스 수정으로 해결한 결함이 아님 |
| CTest | `ctest --test-dir build -N` | 등록 테스트 0건. C++ 회귀 검증 공백(T-010) |
| C++ 포맷 | clang-format 22.1.8 설정 파싱 | FAIL: 기준선 `.clang-format`의 `Standard: c++23` 미지원(T-002) |
| Go | build, vet, test -race, gofmt | PASS |
| Python | ruff check, ruff format --check, pytest | PASS, 테스트 4건 |
| TypeScript | npm run build, npm run lint, Jest | PASS, 테스트 4건 |
| Docker | `docker compose build`, 6개 서비스 기동 | PASS, 6개 컨테이너 healthy |
| Compose E2E | `docker/scripts/e2e_test.sh` | PASS 19 / FAIL 0. 정상 흐름 6개 검사×API 3종 + 미등록 태스크 최종 failed 1개 |

E2E 로그는 실행 세션의 `/tmp/ytf-baseline-e2e.log`에 있습니다. 일시적인 로컬 로그 경로는 저장소의 영구 검증 산출물이 아닙니다. `.github/workflows/gates.yml`에는 Go/Python/JS/Compose 4개 잡이 정의되어 있으나, 이 문서는 `6f7fb67` 원격 Actions 성공을 별도로 확인했다고 주장하지 않습니다.

## 3. 결함 재현 결과

| 항목 | 실측·분석 | 추적 |
|---|---|---|
| 기동 중 SIGTERM | Redis 연결 대기 중 종료 신호로 exit -11 재현 | SPR-001 / T-003 |
| timeout 자손 생존 | 직접 프로세스가 약 1003ms에 종료된 뒤 자손이 약 3초 시점에 파일 생성 | SPR-002 / T-004 |
| 상태 오류 후 ACK | `set_job_status` 저장 오류가 반환되지 않고 handle_message가 성공 반환. 정적 확인, 장애 주입 후 회귀는 미실행 | SPR-003 / T-005 |
| readiness 누적 시간 | Redis/SQS/S3 모두 스톨할 때 Python 약 9.01초, JS 약 9.004초. 전체 3초 요구 위반 | SPR-004 / T-006 |
| JS 종료 순서 | Redis onModuleDestroy가 HTTP close 이전에 실행되는 수명주기 결함 확인. 실제 drain 회귀는 조치 후 실행 예정 | SPR-007 / T-011 |
| 숨김 입력 파일 | `.hidden.txt` 단독 입력에서 wordcount/checksum 스크립트 둘 다 exit 1 | SPR-005 / T-007 |

위 결함은 정상 E2E 19개 통과만으로 발견되거나 해소되지 않습니다.

## 4. 미실행·미충족 범위

- 기준선에는 C++ 자동 테스트가 없습니다. 과거 수동 IT-08–IT-25 기록을 현재 커밋의 회귀 PASS로 재사용하지 않습니다.
- 전체 작업 시간/파일 수 상한, 장기 실행 중 SIGTERM, 원격 서비스가 느린 경우의 종료 상한은 미검증입니다. Compose 60초 유예가 상한 보장은 아닙니다.
- 워커 Redis timeout은 현재 서브모듈 API에 없습니다. stdout 임의 바이너리 유효성, 부하/다중 워커 동시 배달, 실 AWS 배포도 별도 검증이 필요합니다.
- Python mypy·의존성 잠금, 모든 언어의 전체 구조적 로깅은 규약 미충족입니다. 검사 도구가 없거나 실행되지 않았으면 PASS로 표기하지 않습니다.
- 수정 이후 테스트·독립 코드 리뷰·추적성 재감사를 완료해야 최종 완료 판정할 수 있습니다.

## 변경 이력

| 버전 | 일자 | 변경 |
|---|---|---|
| 1.0.0–1.7.0 | 이전 판 | 초기 작성 및 누적 검증 기록 |
| 2.0.0 | 2026-11-07 | 누적 반박·미커밋 기록을 현재 기준선 결과로 대체; 과거 실측은 현재 PASS 근거에서 제외 |
