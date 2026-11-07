# CppToolkit 사용 가이드

| 항목 | 내용 |
|---|---|
| 문서 ID | YTF-GUIDE-001 |
| 버전 | 1.0.0 |
| 기준 | 체크아웃된 CppToolkit 헤더·구현·Samples |
| 작성일 / 최종 수정일 | 2026-11-07 / 2026-11-07 |
| 상태 | Active |
| 대상 커밋 | 앱 `6f7fb67`, CppToolkit `a3392056bc9177b1fd5d94e57296f6b42718261c` |

외부의 최신 API를 현재 서브모듈에 있는 것으로 가정하지 않습니다. `.CppToolkit/docs/module_usage_guide.md`는 이 체크아웃에 없으므로 본 문서와 실제 헤더를 사용합니다.

## 1. 모듈과 빌드

| 폴더 | CMake 타깃 | 네임스페이스 | 사용 헤더 |
|---|---|---|---|
| `.CppToolkit/Utilities` | `Utilities` | `Utilities` | `Logger.h`, `ArgumentParser.h`, `File.h` |
| `.CppToolkit/ThreadPool` | `Thread` | `Thread` | `ThreadPool.h`, `ThreadWorker.h`, `Job.h`, `JobPriorities.h` |
| `.CppToolkit/Redis` | `Redis` | `Redis` | `RedisClient.h`, `TLSOptions.h` |
| `.CppToolkit/AWSService` | `AWSService` | `AWSService` | `AWSS3Client.h`, `AWSSQSConsumer.h`, `AWSSQSPublisher.h` |

프로젝트 include는 `#include "Logger.h"`처럼 flat 경로를 사용합니다. `Main/CMakeLists.txt:30`은 네 타깃과 필요한 Boost 타깃을 링크합니다. `Aws::InitAPI`/`Aws::ShutdownAPI`는 `Main/main.cpp:43`에서 앱이 소유합니다.

### 외부 의존성 매핑

| vcpkg 패키지 | find_package | 링크 대상·사용처 |
|---|---|---|
| boost-json/filesystem/system/process | Boost COMPONENTS json filesystem system process | `Boost::json`, `Boost::filesystem`, `Boost::system`, `Boost::process` — Main |
| redis-plus-plus | redis++ CONFIG | Windows `redis++::redis++`, 그 외 `redis++::redis++_static` — Redis |
| aws-sdk-cpp (s3/sqs features) | AWSSDK CONFIG COMPONENTS core s3 sqs | `${AWSSDK_LIBRARIES}` — AWSService |
| lz4 / efsw / cryptopp | 같은 이름 CONFIG | `lz4::lz4`, `efsw::efsw`, `cryptopp::cryptopp` — Utilities(cryptopp 조건부) |

근거는 `Main/CMakeLists.txt:28`과 `.CppToolkit/{Utilities,Redis,AWSService}/CMakeLists.txt`입니다. 새 소스는 해당 타깃의 명시 목록에 추가하고 신규 앱 디렉터리는 루트 `add_subdirectory`로 등록합니다. 타깃 명칭만 추정해 라이브러리로 링크하지 않습니다. `build.sh:7`은 `VCPKG_ROOT` → 형제 `../vcpkg` → `~/vcpkg` 순으로 툴체인을 찾습니다.

## 2. Logger — 정확한 호출 규약

근거: `.CppToolkit/Utilities/Logger.h:41`, `:49`, `:55`, `:59`, `:112`; `.CppToolkit/Utilities/LogTypes.h:7`.

| API | 실제 시그니처·의미 |
|---|---|
| `Logger::handle()` | `Logger&`; 포인터가 아니므로 `.`으로 호출 |
| `file_mode`, `console_mode` | `const LogTypes&`를 받는 setter와 인자 없는 getter |
| `write_interval(uint16_t)` | 밀리초 단위 setter; `set_write_interval`이라는 메서드는 없음 |
| `log_root(const std::string&)` | 로그 경로 설정 |
| `start(const std::string&)` | 설정 후 파일 베이스 이름을 전달해 시작 |
| `write(LogTypes, message, time)` | 완성된 문자열과 선택적 chrono 시작 시각; 가변 포맷 인자를 받지 않음 |
| `chrono_start()` | `high_resolution_clock::time_point`; `write`의 세 번째 인자로 사용 가능 |
| `stop()` / `destroy()` | 출력 스레드 종료 후 싱글턴 해제 |

```cpp
#include "Logger.h"
#include "LogTypes.h"

#include <format>
#include <string>

using namespace Utilities;

Logger::handle().console_mode(LogTypes::Information);
Logger::handle().write_interval(1000);
Logger::handle().start("TaskForgeWorker");

auto job_id = std::string("example-job");
auto started = Logger::handle().chrono_start();
Logger::handle().write(LogTypes::Information, std::format("job [{}] done", job_id), started);

Logger::handle().stop();
Logger::destroy();
```

`write(LogTypes::Information, "job {}", job_id)`는 잘못된 호출입니다. 두 번째 인자는 `std::format`으로 먼저 완성합니다. 레벨 철자는 `Warning`·`Information`이며 `Warn`·`Info`가 아닙니다. 단계는 `None, Exception, Error, Warning, Information, Debug, Sequence, Parameter, Packet` 순서입니다.

참고 실행 예제: `.CppToolkit/Samples/ThreadSample/ThreadSample.cpp:41`, `.CppToolkit/Samples/AWSServiceSample/AWSServiceSample.cpp:78`. 시그널 핸들러에서 Logger를 호출하지 않습니다. Logger 내부에는 mutex와 큐가 있습니다(`Logger.h:85`).

## 3. ThreadPool과 상주 작업

- `ThreadPool::start()`·`stop(bool)`는 `std::expected<void, std::string>`을 반환합니다(`ThreadPool.h:36`). 결과를 확인합니다.
- `stop(true)`는 대기 중 잡을 버린 뒤 워커 종료를 기다립니다. 실행 중 콜백을 강제 취소하지 않습니다(`ThreadPool.cpp:265`). `ThreadPool::wait_stop()`은 없습니다.
- `Job` 콜백 성공은 `return {};`, 실패는 `return std::unexpected("...");`입니다(`Job.h:26`). bool 성공값으로 바꾸지 않습니다.
- 상주 루프에는 전용 `LongTerm` 워커와 종료 조건이 필요합니다. 먼저 종료 조건을 바꾼 뒤 `stop()`을 호출해야 join이 끝납니다. 앱은 DLQ 모니터 루프를 이 방식으로 사용합니다(`Main/TaskForgeWorker.cpp:391`).

## 4. Redis와 AWS

| 관심사 | 현재 API·제약 |
|---|---|
| Redis 연결 | `RedisClient(address, port, TLSOptions, db_index)`; 생성자에 timeout 옵션은 없음(`RedisClient.h:18`) |
| Redis GET | `expected<string,string>`이며 키 부재도 오류로 반환; nil과 장애를 결과 타입만으로 구별할 수 없음(`RedisClient.cpp:100`) |
| Redis transaction | `transaction_functions(callback)`가 `expected<QueuedReplies,string>` 반환(`RedisClient.h:91`); 실제 연결·응답 타입은 헤더 확인 |
| S3 전송 | `upload_file`·`download_file`은 bucket/object/local filename 순서(`AWSS3Client.h:25`) |
| S3 presign | `generate_presigned_url(bucket, object, expiration_sec, HttpMethod)`; 결과는 `expected<string,string>`(`AWSS3Client.h:37`) |
| SQS 소비 | 소비 핸들러의 expected 성공 여부가 메시지 삭제를 제어(`AWSSQSBase.cpp:109` `receive_process_once`); 앱에서 오류를 삼키지 않도록 확인 |

SDK 시간 제한·호출 예외 동작은 설치된 버전의 헤더와 공식 문서로 확인하며 새 API가 있다고 추정하지 않습니다. 별도 저장소인 CppToolkit의 변경은 앱 수정과 구분합니다.

## 변경 이력

| 버전 | 일자 | 변경 |
|---|---|---|
| 1.0.0 | 2026-11-07 | 체크아웃 실측 기반 모듈·Logger·ThreadPool·Redis/AWS 사용 규약 정리 |
