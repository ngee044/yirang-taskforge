# yirang-taskforge 검증 결과

| 항목 | 내용 |
|---|---|
| 문서 ID | YTF-VER-001 |
| 버전 | 2.1.0 |
| 기준 표준 | ISO/IEC/IEEE 29119-3:2021 |
| 작성일 | 2026-11-07 (현행화 판) |
| 최종 수정일 | 2026-11-07 |
| 상태 | Active — 사용자 승인 여부를 뜻하지 않음 |
| 대상 커밋 | `21888c7` |

## 1. 검증 대상과 환경

최종 코드 범위는 `6f7fb67..21888c7`입니다. 주요 실행·독립 검증은 `8003716`에서 수행했고, 이후 `21888c7`은 Tests의 Boost 탐색에 `CONFIG` 한 단어를 추가했습니다. 이 보완 뒤 macOS 빌드·CTest 등록과 Linux Docker builder 단계의 CTest 5개를 다시 확인했습니다. 문서 작성자는 상위 세션·독립 verifier의 원 명령 종료 코드와 로그를 전달받아 기록했습니다.

| 환경 | 확인 버전·범위 |
|---|---|
| macOS arm64 | Apple clang 21.0.0, CMake 4.3.4, Ninja 1.13.2; Go 1.26.4; Python 3.14.6, ruff 0.15.20 |
| Node 로컬 독립 검사 | Node 26.7.0/npm 11.19.0 |
| Node Linux 격리 검사 | Node 22.23.4/npm 10.9.9; Nest 11.1.28, TypeScript 5.9.3, lockfile 직접 의존성 26개 일치 |
| C++ Linux | Docker worker Ubuntu 24.04 Docker builder 단계의 빌드·CTest |
| 포맷 | 로컬 clang-format 19/22, Ubuntu clang-format 19.1.1 |

## 2. 최종 실행 결과

아래 원 명령은 모두 종료 코드 0입니다. 검사 도구가 정의되어 있다는 사실과 실제 실행·통과를 구분합니다.

| 대상·작업 디렉터리 | 실제 명령·범위 | 등록 / 실행 / PASS / FAIL / SKIP |
|---|---|---|
| C++, 루트 | `./build.sh` | 컴파일·링크 성공 |
| CTest, 루트 | `ctest --test-dir build -N`; `ctest --test-dir build --output-on-failure --no-tests=error` | 5 / 5 / 5 / 0 / 0 |
| C++ 포맷, 루트 | `clang-format-19 --dry-run --Werror Main/*.cpp Main/*.h Tests/*.cpp` 및 로컬 22 동일 범위 | 위반 0; C++23 컴파일 표준 유지 |
| Go, Interface/golang | `test -z "$(gofmt -l .)"`, `go build ./...`, `go vet ./...`, `go test -race ./...`; 최종 `go test -race -json ./...` | 최상위 10 / 10 / 10 / 0 / 0; 하위 테스트 7 / 7 / 7 / 0 / 0; 포맷 출력 0·빌드/vet 통과 |
| Python, Interface/python | `.venv/bin/ruff check . ../../Tests/worker_recovery_test.py`; `.venv/bin/ruff format --check . ../../Tests/worker_recovery_test.py`; `.venv/bin/pytest -q` | pytest 8 / 8 / 8 / 0 / 0 |
| JS, Interface/js | `npm run build`, `npm run lint`, `npm run format:check`, `npm test -- --runInBand` | Jest 3 suite, 9 / 9 / 9 / 0 / 0; macOS·Linux Node22 독립 확인 |
| Docker, 루트 | `docker compose -f docker/docker-compose.yml build`; Docker builder 단계에서 `ctest --test-dir build --output-on-failure` | 전체 이미지 빌드 성공; CTest 5 / 5 / 5 / 0 / 0 |
| E2E, 루트 | `bash docker/scripts/e2e_test.sh` | 19 / 19 / 19 / 0 / 0 |
| 장애 복구, 루트 | `python3 Tests/worker_recovery_test.py` | 4그룹 / 4그룹 / 4그룹 / 0 / 0 |

`Tests/worker_recovery_test.py`는 표준 라이브러리 스크립트이며 Ruff는 **Python 프로젝트 작업 디렉터리와 설정**으로 검사했습니다. 루트 Ruff 기본값으로 검사한 결과가 아닙니다.

## 3. 회귀가 확인한 동작

| 테스트 | 실제 확인 |
|---|---|
| CTest validation/dlq/executor | 메시지 경계·카운트/파일 오류·정상/비영 exit·UTF-8 절단·timeout cap·자손 종료·자식 신호 마스크 |
| CTest task_scripts/startup_shutdown | 숨김/혼합/빈 입력; Redis 연결 실패 중 SIGTERM이 crash 대신 exit 1 정리로 종료 |
| 복구 그룹 1 | Redis 쓰기 실패 시 원본 유지; 복구 후 failed 저장·ACK |
| 복구 그룹 2 | Redis WRONGTYPE 읽기 실패 시 원본·기존 값 보존; nil로 복구 후 종결·ACK |
| 복구 그룹 3/4 | 초기 상태 없는 정상 done·터미널 재수신 문서 불변, 정상 SIGTERM exit 0 |
| API health | Go 공유 deadline/요청 취소; Python 동시 시작과 timeout=0 경계; JS 가상시간 3000ms; 오류 응답 비노출 |
| JS shutdown | 실제 Nest HTTP 요청의 마지막 Redis 쓰기가 끝난 뒤 quit |

Compose E2E는 정상 6개 검사×API 3종과 미등록 태스크 최종 failed 1개입니다. 오류 입력 전체·중간 running 상태·실 AWS 검증을 포함한다고 확대하지 않습니다. 세부 케이스는 [STP](STP.md), 요구사항 매핑은 [RTM](RTM.md)을 따릅니다.

## 4. 독립 검증과 로그

최종 문서 독립 감사는 문서 22종·README 2종을 대상으로 통과했습니다. 요구사항 31개, 테스트 케이스 41개, 정·역방향 추적 관계 72쌍이 일치하며, 명시 코드 참조 130개의 경로·행 범위와 문서 링크·JSON 예제에 오류가 없습니다.

코드·구조·보안 리뷰와 수정 뒤 더블체크를 통과했습니다. 별도 C++ verifier는 `8003716` 빌드·CTest 5개를 실행했고, `21888c7` CMake 보완을 재리뷰했습니다. TypeScript 전용 역할은 새 검증 세션에서 설치 지침을 명시적으로 적용하여 macOS와 Linux Node22로 수행했습니다. 이 문서는 로컬·컨테이너 검증 결과를 기록하며 원격 실행 이력은 [저장소 Actions](https://github.com/ngee044/yirang-taskforge/actions/workflows/gates.yml)에서 확인합니다. CI는 Go/Python/JS/Compose 4개 잡과 포맷·CTest·복구 게이트를 정의합니다.

| 로컬 임시 로그 | 내용 |
|---|---|
| `/tmp/ytf-final-ctest.log`, `/tmp/ytf-independent-ctest.log` | CTest 5개 PASS |
| `/tmp/ytf-independent-cpp-build.log` | 독립 C++ 빌드 |
| `/tmp/ytf-final-worker-build.log` | Linux 이미지 빌드·CTest |
| `/tmp/ytf-final-e2e.log` | E2E 19/0 |
| `/tmp/ytf-final-go-tests.json` | `21888c7` Go race 결과·최상위/하위 건수 |
| `/tmp/ytf-final-worker-recovery.log` | 장애 복구 4그룹 PASS |

임시 로그는 영구 저장소 산출물이 아닙니다. 재현 명령과 케이스 구현이 저장소의 재검증 근거입니다.

## 5. 잔여·미실행 범위

- 워커 Redis I/O timeout은 체크아웃 CppToolkit API에 없어 NFR-REL-02 미충족입니다. 기동/종료가 모든 원격 스톨에서 유계라는 보장은 없습니다.
- SQS/Redis/파일은 원자적 트랜잭션이 아니며 실행 후 done 저장 실패 시 재실행될 수 있습니다. exactly-once를 보장하지 않습니다.
- C++ 구현·시험은 POSIX를 사용합니다. macOS·Linux는 위 범위로 검증했고 Windows 빌드/동작은 지원하지 않습니다. 실 AWS IAM·대용량/부하·장기 실행 중 종료는 미검증입니다.
- Python lock/mypy, Go 추가 린터·취약점 검사, JS npm audit 등 미도입 규약은 PASS로 표기하지 않습니다. CI의 Go 1.24/Python 3.12 결과는 원격 Actions 실행 이력에서 별도로 확인합니다.
- 기존 CMake CMP0167 경고 3건(툴킷 2·Main 1)과 macOS 정적 라이브러리 중복 링크 경고는 남습니다. 새 Tests 경고는 CONFIG 보완으로 제거했으며 컴파일 에러는 없습니다.
- 수동 계획의 미실행 항목은 STP에서 유지합니다. 자동화된 부분 시험을 전체 장애 시나리오 PASS로 확대하지 않습니다.

## 변경 이력

| 버전 | 일자 | 변경 |
|---|---|---|
| 1.0.0–1.7.0 | 이전 판 | 초기 작성 및 누적 검증 기록 |
| 2.0.0 | 2026-11-07 | 누적 반박·미커밋 기록을 현재 기준선 결과로 대체; 과거 실측은 현재 PASS 근거에서 제외 |
| 2.1.0 | 2026-11-07 | 최종 로컬·독립 검증 명령/환경/등록·실행·PASS를 기록하고 미지원·미검증 분리 |
