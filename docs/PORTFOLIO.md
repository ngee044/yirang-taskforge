# yirang-taskforge 포트폴리오

| 항목 | 내용 |
|---|---|
| 문서 ID | YTF-PORT-001 |
| 버전 | 1.0.0 |
| 기준 표준 | ISO/IEC/IEEE 42010:2022 (구조 설명) |
| 작성일 | 2026-11-07 (현행화 판) |
| 최종 수정일 | 2026-11-07 |
| 상태 | Active — 사용자 승인 여부를 뜻하지 않음 |
| 대상 커밋 | `6f7fb67` |

## 프로젝트 컨셉

HTTP 요청을 SQS FIFO에 넣고 C++23 워커가 whitelist 실행파일을 실행한 뒤 Redis 상태와 S3 결과를 제공하는 작업 오케스트레이션 프로젝트입니다. 동일 REST 계약을 Go/Gin, Python/FastAPI, TypeScript/NestJS로 각각 구현하여 언어별 계층 구조와 오류·수명주기 처리를 비교합니다. 세 API의 중복 구현은 포트폴리오 목적입니다.

## 구현 범위

| 구성 | 구현 내용 | 근거 |
|---|---|---|
| Main | JSON 검증·whitelist 실행·입출력·timeout·DLQ 재발행 | `Main/TaskForgeWorker.cpp:413`, `Main/TaskExecutor.cpp:40` |
| API 3종 | 접수 202·presigned PUT·상태 조회·카탈로그·health/readiness | [API 계약](API.md) |
| 상태·파일 | queued/running/done/failed, Redis 병합·TTL, S3 presigned GET | [SAD §3](SAD.md) |
| 배포 | Docker Compose 6개 서비스, 애플리케이션 이미지 4개 | `docker/docker-compose.yml:1` |
| 자동 검사 | CI 4개 잡: Go, Python, JS, Compose | `.github/workflows/gates.yml:16` |

실행 방법은 [루트 README](../README.md), API별 명령은 [Interface README](../Interface/README.md)를 사용합니다. CppToolkit 정확한 API는 [모듈 가이드](module_usage_guide.md)에서 확인합니다.

## 검증 수준과 한계

2026-11-07 기준선의 C++·언어별 빌드 게이트와 로컬 Compose E2E 19개 검사는 통과했습니다. C++ 자동 테스트는 기준선 0건입니다. 정상 E2E 통과가 전체 신뢰성 충족을 뜻하지는 않습니다. 기동 중 종료 충돌, timeout 자손 실행, Redis 상태 오류 후 ACK, readiness 총시간, 숨김 입력 처리의 미해결 결함은 [SPR](SPR.md)에서 추적합니다.

API의 정상 계약은 공유하지만 검증 오류 코드·파일명 길이 단위·배열 상한의 차이는 남아 있습니다. Redis와 SQS 사이 원자적 트랜잭션, 업로드 완료 확인, exactly-once 실행, 워커 Redis timeout은 제공하지 않습니다. 인증·과금·웹 콘솔·분산 추적 백엔드는 미구현입니다. 세부 검사 강도와 실행 기록은 [VERIFICATION](VERIFICATION.md)이 기준입니다.

## 문서 탐색

| 질문 | 문서 |
|---|---|
| 무엇을 만족해야 하는가 | [SRS](SRS.md) |
| 어떻게 구성되는가 | [SAD](SAD.md), [API](API.md) |
| 코드와 시험을 어떻게 연결하는가 | [RTM](RTM.md), [STP](STP.md) |
| 어떤 개선이 필요한가 | [프로젝트 분석](project_analysis.md), [작업 목록](task_list.md) |
| 확장 아이디어는 무엇인가 | [범위 밖 SaaS 제안](NEXTJS_SAAS_TASK_LIST.md) |

## 변경 이력

| 버전 | 일자 | 변경 |
|---|---|---|
| 초기 소개판 | 이전 판 | 초기 작성 및 누적 검증 기록 |
| 1.0.0 | 2026-11-07 | 고정 LOC·과거 결함 수·미커밋 CI 서술 제거, 실재 구현과 검증 범위 정리 |
