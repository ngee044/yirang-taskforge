# C++ 코딩 규약 (Coding Convention)

| 항목 | 내용 |
|---|---|
| 문서 ID | YTF-CONV-001 |
| 버전 | 1.2.0 |
| 기준 표준 | C++23, 프로젝트 코드 규약 |
| 작성일 | 2026-11-07 (현행화 판) |
| 최종 수정일 | 2026-11-07 |
| 상태 | Active — 사용자 승인 여부를 뜻하지 않음 |
| 대상 커밋 | `21888c7` |

> 본 문서는 **CppToolkit 및 그 소비 애플리케이션에 실제로 적용된 관례를 추출·일반화한 규약**으로, yirang-taskforge의 `Main`(TaskForgeWorker)에 적용합니다. (`basic-ai-agent-guideline/cpp_guideline`에서 도입) 규약 자체(C++23, `.clang-format` 설정, 후행 반환 타입, `std::expected` 오류 규약과 외부 API 반환형, `#pragma once`, 명명 규칙 등)는 보편 규칙이므로 그대로 유지합니다. 새 코드는 본 문서를 따르고, 기존 코드를 수정할 때는 주변 코드의 스타일에 맞춥니다. 신규 규칙 제안은 PR 논의를 통해 본 문서를 갱신한 뒤 적용합니다.

---

## 1. 적용 범위 (Scope)

- 본 컨벤션은 프로젝트의 모든 C++ 소스(`*.h`, `*.hpp`, `*.cpp`)와 `Samples/`(있는 경우) 예제에 적용됩니다.
- `vcpkg_installed/`, `build/`, 서브모듈(예: `.CppToolkit/`, `.cpp_tool_kit/`) 등 생성물·외부 의존성에는 적용하지 않습니다. 서브모듈은 해당 저장소의 규약을 따릅니다.
- 포맷팅(들여쓰기, 줄바꿈, 중괄호 위치)은 모두 `.clang-format`에 위임합니다. 본 문서는 포맷터가 강제할 수 없는 **명명·구조·관용구**를 규정합니다.
- 짝 문서: 모듈 사용법·CMake 작성·링크 규약(폴더명↔타겟명↔네임스페이스 매핑, vcpkg↔`find_package`↔링크 타겟 매핑, 모듈별 API 반환 타입 표)은 [모듈 사용 가이드](../module_usage_guide.md)(`YTF-GUIDE-001`)에 둡니다. 본 문서(스타일·관용구)와 그 문서(사용법·빌드)는 짝을 이룹니다.

---

## 2. 파일 및 디렉터리 구성 (File / Directory Layout)

- 모듈은 책임별 디렉터리와 CMake 타깃으로 분리합니다. 이 프로젝트의 `Main/`은 `TaskForgeWorker` 실행 타깃이며(`Main/CMakeLists.txt:26`), CppToolkit 재사용 모듈은 라이브러리 타깃입니다. 실행 프로그램을 형식상 라이브러리로 바꾸지 않습니다.
- **한 클래스 = 한 쌍의 파일** (`ClassName.h` + `ClassName.cpp`). 선언과 정의를 분리합니다.
- 파일명은 클래스명과 동일한 **PascalCase**로 합니다: `ThreadPool.h`, `NetworkSession.cpp`, `LogTypes.h`.
- 순수 enum/상수/타입 정의 전용 헤더는 접미사로 의도를 드러냅니다: `LogTypes.h`, `DataModes.h`, `NetworkConstexpr.h`, `Protocol.h`, `JobPriorities.h`.
- 템플릿 전용 정의(선언과 정의를 헤더에서 분리할 수 없는 경우)는 `.hpp` 확장자를 사용합니다(예: `DeliveryResult.hpp`, 애플리케이션의 `ModuleHeader.hpp`).
- 각 모듈 루트에 `CMakeLists.txt`를 두고, 모듈별 사용 설명이 필요하면 `README.md`를 둡니다.

---

## 3. 명명 규칙 (Naming)

| 대상 | 규칙 | 예시 |
|------|------|------|
| 클래스 / 구조체 | PascalCase | `ThreadPool`, `NetworkSession`, `RedisClient` |
| 파일 | PascalCase (클래스명 일치) | `JobPool.cpp` |
| 함수 / 메서드 | snake_case | `push`, `remove_workers`, `thread_title` |
| 지역 변수 / 매개변수 | snake_case | `backup_folder`, `stop_immediately` |
| 멤버 변수 | snake_case + **`_` 접미사** | `thread_title_`, `job_pool_`, `working_` |
| enum class | PascalCase 타입 + PascalCase 값 | `LogTypes::Warning`, `JobPriorities::Top` |
| 네임스페이스 | PascalCase | `TaskForge` (예: `Utilities`, `Thread`, `Network`) |
| 매크로 / 컴파일 플래그 | UPPER_SNAKE_CASE | `USE_ENCRYPT_MODULE`, `WSTRING_IS_U16STRING` |

- 멤버 변수의 `_` 접미사는 **필수**입니다. getter/setter 오버로드(§6)에서 함수명과 멤버명을 구분하는 근거가 됩니다.
- **모듈명(폴더명)과 네임스페이스명이 항상 일치하지는 않습니다.** 대표적으로 `ThreadPool/` 폴더의 네임스페이스는 **`Thread`** 입니다. 신규 파일 작성 시 디렉터리 이름이 아니라 **기존 동일 모듈 파일의 네임스페이스**를 따릅니다. 폴더명↔링크 타겟명↔네임스페이스 매핑 표는 [모듈 사용 가이드](../module_usage_guide.md)에 둡니다(§15).

---

## 4. 포맷팅 (Formatting) — `.clang-format` 요약

포맷팅은 전적으로 `.clang-format`이 결정합니다. 커밋 전 `clang-format -i <file>`로 정렬합니다. 핵심 설정:

- `BasedOnStyle: GNU`, `IndentWidth: 4`, `TabWidth: 4`, **`UseTab: Always`** (들여쓰기는 탭).
- `ColumnLimit: 170`.
- **Allman 중괄호** (`BreakBeforeBraces: Custom`) — 함수·클래스·구조체·enum·제어문·네임스페이스·람다의 여는 중괄호는 다음 줄에 둡니다.
- `NamespaceIndentation: All` — 네임스페이스 내부 내용도 한 단계 들여씁니다.
- `PointerAlignment: Left` — `int* p` (포인터/참조 기호는 타입에 붙입니다).
- `SortIncludes: false` — include는 **자동 정렬하지 않습니다**(§8 참조, 수동 정렬).
- `BinPackParameters: false` — 인자가 길어지면 한 줄에 하나씩 나눕니다.
- `SpaceBeforeParens: ControlStatements` — `if (`, `for (`처럼 제어문 키워드 뒤에만 공백을 둡니다(함수 호출 `f(`에는 공백 없음).
- `AccessModifierOffset: -4` — `public:`/`private:`는 한 단계 내어씁니다.
- `AlignTrailingComments: Always` — 후행 주석을 정렬합니다.
- 포매터의 표준 옵션은 사용 버전이 지원하는 `Standard: Latest`를 사용합니다. C++ 컴파일 표준은 계속 C++23입니다. `Standard: Latest`는 clang-format 19/22에서 검사했으며 T-002 파싱 실패를 해소했습니다.

`.clang-format`은 프로젝트 루트(또는 서브모듈이 제공하는 것)를 그대로 사용합니다. 임의로 설정을 바꾸지 않습니다.

---

## 5. 함수 시그니처 (Function Signatures)

### 5.1 후행 반환 타입 (Trailing Return Type) — 필수

모든 함수/메서드는 **`auto name(params) -> ReturnType`** 형식을 사용합니다. 선행 반환 타입은 사용하지 않습니다.

```cpp
auto push(std::shared_ptr<Job> job) -> std::expected<void, std::string>;
auto thread_title(void) const -> const std::string;
static auto split(const std::string& source, const std::string& token) -> std::vector<std::string>;
```

### 5.2 빈 매개변수 목록은 `(void)`

매개변수가 없으면 `()`가 아니라 **`(void)`** 로 명시합니다. 소멸자도 동일합니다.

```cpp
TaskForgeWorker(const std::string& title = "TaskForgeWorker");
virtual ~TaskForgeWorker(void);
auto start(void) -> std::expected<void, std::string>;
```

### 5.3 매개변수 전달

- 읽기 전용 비-trivial 타입은 `const T&`로 받습니다: `const std::string&`, `const std::vector<uint8_t>&`.
- 소유권을 공유하는 `shared_ptr`는 값으로 받습니다: `std::shared_ptr<Job> job`.
- 기본 인자(default argument)는 **헤더 선언부에만** 둡니다(`.cpp` 정의부에는 다시 적지 않습니다).
- 콜백은 `const std::function<...>&`로 받아 멤버에 저장합니다.

---

## 6. getter / setter 오버로드 패턴

하나의 속성에 대해 **같은 이름의 함수 2개**를 오버로드합니다. 인자가 있으면 setter, `(void) const`이면 getter입니다.

```cpp
auto life_cycle(uint16_t cycle) -> void;        // setter
auto life_cycle(void) const -> uint16_t;        // getter

auto thread_title(const std::string& title) -> void;   // setter
auto thread_title(void) const -> const std::string;    // getter
```

- getter는 가능한 한 `const`로 선언합니다.
- 별도의 `get_`/`set_` 접두사는 쓰지 않습니다.

---

## 7. 오류 처리 (Error Handling)

> 대원칙: **예외를 던지지 않고, 반환값으로 실패를 말합니다.** 비동기·네트워크·백그라운드 스레드 경계에서 예외를 던지면 콜백 스택이 끊기고 리소스가 샙니다. 그래서 실패는 던지지 않고 반환합니다.

### 7.1 표준: `std::expected<void, std::string>`

부수효과만 있고 실패할 수 있는 API는 **`std::expected<void, std::string>`** 를 반환합니다(현행 표준, 라이브러리 계층 전반에서 사용). 성공 시 `{}`, 실패 시 `std::unexpected("사유")`.

```cpp
auto TaskForgeWorker::push(std::shared_ptr<Job> job) -> std::expected<void, std::string>
{
	if (job_pool_ == nullptr)
	{
		return std::unexpected("cannot push a job into null JobPool");
	}
	return job_pool_->push(job);
}

auto TaskForgeWorker::start(void) -> std::expected<void, std::string>
{
	// ...
	auto result = worker->start();
	if (!result)
	{
		return std::unexpected(result.error());   // 오류 전파
	}
	return {};   // 성공
}
```

호출 측 관용구:

```cpp
if (auto result = pool->start(); !result)
{
	Logger::handle().write(LogTypes::Error, result.error());
	return;
}
```

### 7.2 데이터 반환 + 오류: `std::expected<T, std::string>`

새 애플리케이션 API는 데이터와 실패를 `std::expected<T, std::string>`으로 표현합니다. 성공은 값, 실패는 `std::unexpected("사유")`를 반환합니다. `Main/TaskExecutor.h`의 `execute`와 프로젝트 루트 지침이 기준입니다. 결과 확인 없이 `value()`로 접근하지 않습니다.

```cpp
auto read_result(void) -> std::expected<std::string, std::string>;
```

CppToolkit 기존 API가 `tuple`을 반환하면 해당 헤더의 정확한 형태로 받습니다. 라이브러리의 레거시 반환형을 새 애플리케이션 코드의 데이터 오류 표준으로 확대하지 않습니다.

### 7.3 비동기 / 네트워크 경로에서 예외 금지

SQS 소비·Thread Job 등 비동기/콜백 경로에서는 예외를 던지지 않습니다. 외부 라이브러리 호출 등 예외가 발생할 수 있는 지점은 `try/catch`로 감싸고 오류를 `expected`/`tuple`로 변환해 반환합니다. 콜백 시그니처 자체가 `std::expected<void, std::string>`을 반환하도록 설계합니다(Job 콜백, 네트워크 콜백, 컨슈머 콜백 등).

### 7.4 방어적 null 검사

`shared_ptr` 멤버를 역참조하기 전에 항상 null을 확인합니다. 치명적이지 않으면 경고 로그를 남기고 안전한 기본값으로 빠져나갑니다.

```cpp
if (job_pool_ == nullptr)
{
	Logger::handle().write(LogTypes::Warning, "cannot get uncompleted jobs by null job_pool");
	return {};
}
```

> 기존 외부 API의 `tuple` 반환은 헤더대로 처리하되 신규 앱·Job/SQS 콜백은 `std::expected`를 사용합니다.

---

## 8. 헤더 및 include (Headers / Includes)

- 모든 헤더 최상단은 **`#pragma once`** 로 합니다(include guard 매크로 사용 안 함).
- include는 **수동 정렬**합니다(`SortIncludes: false`). 일반적 순서:
  1. 짝이 되는 헤더 (`.cpp`의 첫 줄은 같은 이름의 `.h`)
  2. 같은 프로젝트의 헤더 — **큰따옴표** `"JobPool.h"`
  3. 표준 라이브러리 — **꺾쇠** `<memory>`, `<expected>`
  4. 서드파티 (Boost 등) — 꺾쇠 `<boost/asio/steady_timer.hpp>`
- **flat include(평평한 포함).** 프로젝트 헤더는 경로 접두어 없이 파일명만 씁니다: `#include "Logger.h"` (O), `#include "Utilities/Logger.h"` (X, 컴파일 에러). 각 모듈이 `target_include_directories(... PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})`로 자기 소스 디렉터리를 공개하기 때문입니다. 이 규약은 CMake 작성 규약(§15)과 짝을 이룹니다.
- 헤더에서는 가능하면 **전방 선언(forward declaration)** 으로 의존성을 줄입니다.

```cpp
namespace TaskForge
{
	class Job;            // 전방 선언
	class JobPool;
	class ThreadWorker;
	class ThreadPool : public std::enable_shared_from_this<ThreadPool>
	{
		/* ... */
	};
} // namespace TaskForge
```

- 관련 멤버를 묶어 가독성을 높일 때 `#pragma region` / `#pragma endregion`을 사용합니다(예: `Logger`의 `#pragma region Handle`).

---

## 9. 네임스페이스 (Namespaces)

- 모든 라이브러리 코드는 모듈 네임스페이스로 감쌉니다: `TaskForge`.
- `NamespaceIndentation: All`에 따라 내부 내용을 한 단계 들여씁니다.
- 네임스페이스 닫는 중괄호 뒤에는 주석으로 이름을 표기합니다: `} // namespace TaskForge`.
- `.cpp`에서는 자주 쓰는 의존 네임스페이스를 파일 상단에서 `using`으로 끌어올 수 있습니다: `using namespace Utilities;`. **헤더에서는 `using namespace`를 금지**합니다.

---

## 10. 메모리 / 수명 관리 (Memory & Lifetime)

- 공유 소유권은 `std::shared_ptr`, 단독 소유권은 `std::unique_ptr`를 씁니다.
- 부모-자식 등 **순환 참조**는 `std::weak_ptr`로 끊습니다.
- 자기 자신을 콜백/하위 객체에 넘겨야 하는 클래스는 `std::enable_shared_from_this<T>`를 상속하고, **`get_ptr()`** 메서드로 `shared_from_this()`를 노출합니다.

```cpp
class TaskForgeWorker : public std::enable_shared_from_this<TaskForgeWorker>
{
public:
	auto get_ptr(void) -> std::shared_ptr<TaskForgeWorker>;   // return shared_from_this();
};
```

- **명시적 수명주기**: 리소스를 쥐는 객체는 **생성 → 설정 → `start()` → 사용 → `stop()` → `reset()`** 순서로 다룹니다. 생성자에서 스레드를 띄우지 않고, 설정을 먼저 다 한 뒤 `start()`로 명시적으로 켭니다. 종료도 명시적으로 `stop()`(필요 시 `wait_stop()`) 후 `reset()`합니다. 소멸자는 안전망일 뿐, 정상 경로에서 종료 순서를 소멸자에 맡기지 않습니다.
- 소멸자에서는 보유 리소스를 정리(`stop()` → `clear()` → `reset()`)하고 디버그 로그를 남깁니다.
- 싱글턴은 **`Logger`만** 허용합니다. `private` 생성자 + `static handle()` + `static destroy()` + `std::unique_ptr` + `std::once_flag` 패턴을 따릅니다. 그 외 컴포넌트는 싱글턴을 만들지 않습니다.

---

## 11. 동시성 (Concurrency)

- 플래그/카운터는 `std::atomic<bool>`, `std::atomic<size_t>` 등을 사용합니다(`working_`, `pause_`, `thread_stop_`).
- 임계 구역은 `std::scoped_lock<std::mutex> lock(mutex_);`로 보호합니다. 잠금 범위는 블록 `{ }`으로 최소화합니다.
- 대기/통지는 `std::condition_variable`을 사용합니다.
- ThreadPool은 우선순위(`Top`/`High`/`Normal`/`Low`/`LongTerm`) 기반 Job 스케줄링을 따릅니다. Job 콜백도 §7의 반환 규약(`std::expected<void, std::string>`)을 동일하게 적용합니다(`return {};` 성공 / `return std::unexpected("...")` 실패).
- 시작/정지는 명시적 `start()` / `stop()`으로 제어합니다. **주의: 종료 API는 모듈마다 다릅니다.** ThreadPool은 **`wait_stop()`가 없고 `stop()` 자체가 블로킹**(워커 조인까지 대기, `stop(true)`는 대기 잡을 버리지만 실행 중 콜백의 완료와 join은 기다림)입니다. 넘겨짚지 말고 [모듈 사용법](../module_usage_guide.md)에서 종료 API를 확인합니다.

---

### 11.1 상주 잡 (Resident Jobs) — 이름 있는 멤버 함수 + 위임 람다

스레드를 붙잡고 도는 **상주 잡**(주기 sweep, 소비 루프, watcher 등)은 다음 형태로 등록합니다.

- **잡 본문은 소유 클래스의 protected 멤버 함수**로 두고 snake_case 이름을 붙입니다. 본문을 익명 람다 안에 직접 쓰지 않습니다. 잡을 별도 Job 서브클래스로 빼면 매니저의 상태(중단 플래그·설정·핸들)를 외부에 열어야 하므로, 상태 소유자와 잡 본문을 같은 클래스에 둡니다.
- 잡 본문 함수의 **반환 타입은 Job 콜백 계약과 동일하게** 둡니다 — 이 규약에서는 §7.1의 `std::expected<void, std::string>`입니다(다른 체크아웃을 소비할 때는 실제 헤더의 반환 계약을 다시 확인합니다). 계약과 같으면 감싸는 람다가 `return 잡함수();` **한 줄 위임**이 됩니다.
- `start()`가 `[this]` 캡처 람다로 감싸 push합니다. `ThreadPool::push`는 콜러블을 직접 받지 않으므로 항상 `std::make_shared<Thread::Job>(priority, callback, title)`로 감쌉니다.
- **Job 제목 = 잡 함수명.** 로그의 `completed work <제목>`이 코드 지점을 1:1로 가리키게 합니다. 제목이 함수명과 다르면(예: 제목 `LeaseSweepWorker` vs 함수 `lease_sweep_job`) 로그→코드 추적이 끊어집니다.
- 상주 잡의 우선순위는 `LongTerm`으로 하고, **잡 수만큼 전용 `LongTerm` 워커**를 pool 시작 전에 push합니다. 상주 잡은 주기 대기 중에도 워커를 점유하므로, `Normal`을 쓰면 같은 pool의 다른 `Normal` 잡이 영구히 굶습니다.
- **워커 이름에 잡 함수명을 붙이지 않습니다.** JobPool은 우선순위로만 잡을 배분해 워커↔잡 짝이 고정되지 않습니다. 워커는 번호만 갖고(`sweep_worker_1` 등), 실행 주체 식별은 Job 제목이 담당합니다.
- **push 헬퍼**(`push_<잡>_job()`)를 두어 `start()`와 재등록 꼬리가 같은 함수를 쓰게 합니다 — Job 제목↔본문 대응이 코드에 한 곳만 존재합니다.
- 스레드를 점유하지 않는 일회성 통지 콜백(watcher의 `set_callback` 등)은 그대로 캡처 람다를 씁니다 — 이 규칙은 상주 잡에만 적용됩니다.

반복 방식은 두 가지이며 **둘 다 정상 패턴**입니다. 잡의 성격으로 고릅니다.

| 방식 | 대상 | 형태 |
|------|------|------|
| (A) `LongTerm` + `while` | consume·watcher 류 — 계속 대기·수신이 의도 | `while (running_)` 루프. 루프 안의 대기는 `sleep_for`가 아니라 `condition_variable::wait_for` + `running_` predicate — `stop()`이 즉시 깨울 수 있어야 합니다. **종료 순서는 반드시 `running_` 하강 → `notify_all` → `thread_pool_->stop()`** 입니다. `stop(true)`도 진행 중인 잡을 취소하지 못하므로(§11.2), 플래그를 먼저 내리지 않으면 `stop()`이 루프 종료를 기다리며 영구 블로킹합니다 |
| (B) 자기 재등록 | 주기 sweep 류 — "한 주기 수행 → 대기 → 반복" | 루프문 없이 한 주기를 수행하고 대기한 뒤 **자신을 다시 push** |

(B) 자기 재등록을 쓸 때의 규칙:

- 함수 첫머리에서 `running_`을 검사해, 내려가 있으면 **재등록 없이 성공을 반환**합니다 — 잡이 자연히 소멸합니다.
- **재등록은 반드시 대기 후에** 합니다. 순서를 뒤집으면 유휴 워커가 방금 push된 잡을 곧바로 집어 간격 없이 도는 hot loop가 됩니다.
- 주기 대기는 `sleep_for`가 아니라 `condition_variable::wait_for` + `running_` predicate를 쓰는 전용 헬퍼(`wait_next_sweep` 등)로 묶습니다 — `stop()`의 `notify_all`이 즉시 깨웁니다.
- 재등록도 대기가 잡 안에서 일어나므로 **대기 중에 워커가 해방되지 않습니다** — "잡 수만큼 워커" 규칙은 (A)/(B) 모두에 적용됩니다.
- 재등록 push 실패는 `running_`으로 분기합니다. 종료 중(`stop()`이 JobPool을 잠근 뒤)의 실패는 정상이므로 조용히 넘기고, `running_`이 서 있는데 실패하면 그 잡이 영구 정지된 것이므로 반드시 `Error`로 남깁니다.
- **자기 재등록 push는 `stop()`과 교착하지 않습니다(확정).** 현행 CppToolkit `ThreadPool::stop()`은 워커 목록을 뮤텍스 아래에서 스냅샷한 뒤 **락을 놓고 join**하고, `push(Job)`은 어떤 풀 뮤텍스도 잡지 않은 채 `JobPool`에 위임하며 `JobPool::push`도 통지 콜백 호출 전에 자기 락을 풉니다. 종료 중 재등록은 교착 대신 "잠김" 실패로 즉시 반환되므로 재등록 루프가 자연 종료됩니다(위의 `running_` 분기 규칙이 이 실패를 처리합니다).
- **잡 본문에서 같은 풀의 `start()`/`stop()`/`remove_workers()` 호출을 금지합니다.** 다른 스레드의 `stop()`이 `lifecycle_mutex_`를 보유한 채 해당 잡을 join하면, 잡의 라이프사이클 호출이 뮤텍스를 기다려 교착할 수 있습니다. 자기 워커에 대한 stop/remove에는 self-join 위험도 있습니다. `start()`는 경합이 없으면 `already started`를 반환하며 join하지 않습니다. 호출 금지 규약과 실제 교착 조건을 구분합니다.
- 위 동작은 이 프로젝트가 고정한 CppToolkit 체크아웃 기준입니다. 다른 세대의 stop/push에도 같은 잠금 순서가 있다고 추정하지 말고 헤더·구현을 다시 확인합니다.

현재 앱은 (A) 패턴을 사용합니다. `Main/TaskForgeWorker.h:34`의 protected `dlq_backlog_monitor`, `Main/TaskForgeWorker.cpp:381`의 한 줄 위임과 동일 Job 제목, `:398`의 조건 변수 대기, `:188`의 플래그 하강·통지·종료 순서를 함께 확인합니다. 자기 재등록 패턴은 현재 앱에서 사용하지 않습니다.

### 11.2 ThreadPool API 확정 사실 (CppToolkit)

아래는 체크아웃 `a3392056bc9177b1fd5d94e57296f6b42718261c`의 CppToolkit `ThreadPool/` 헤더·구현으로 확인한 동작입니다. 근거는 `ThreadPool.cpp:51`, `:88`, `:265`, `JobPool.cpp:121`, `:180`, `Job.cpp:281`, `:286`, `ThreadWorker.cpp:211`입니다. 넘겨짚지 말고 이 표를 기준으로 사용하며, 프로젝트별 타겟명·반환 타입 매핑은 각 모듈의 `<모듈>/CMakeLists.txt`의 `set(LIBRARY_NAME …)`을 직접 확인합니다.

**종료 (`stop`)**

| 호출 | 대기 중인 잡 | 진행 중인 잡 | 반환 시점 |
|------|-------------|-------------|-----------|
| `stop(true)` | `clear()`로 폐기 | **취소되지 않음** | 모든 워커 `join()` 완료 후 |
| `stop(false)`(기본값) | 워커가 전부 소진(드레인) | **취소되지 않음** | 모든 워커 `join()` 완료 후 |

- 두 인자 모두 먼저 JobPool을 잠가 신규 push를 막습니다. 코드상 유일한 차이는 `clear()` 호출 여부입니다. `stop(false)`는 큐 잔량에 비례해 무한정 블로킹될 수 있습니다.
- `stop()` 완료 시 잠금이 해제되므로 **정지된 풀에도 `push`는 성공**합니다(잡은 실행되지 않고 쌓입니다). 종료 후 push를 막으려면 `lock(true)`를 명시적으로 호출합니다.
- `start()` 재호출은 `"already started"`, 미기동 상태의 `stop()`은 `"not started"`, 제거 대상 없는 `remove_workers()`는 실패로 반환됩니다. 소멸자가 `stop(true)`를 호출하므로, 명시적으로 `stop()`한 뒤 파괴하면 소멸자 쪽 실패는 조용히 무시됩니다. `stop()` 후 `start()` 재기동은 가능합니다.

**push / 콜백 / 예외**

- `push(std::shared_ptr<Job>)` → `std::expected<void, std::string>`. 실패 조건은 ① 풀의 JobPool이 null, ② `job`이 null, ③ JobPool이 잠김(=`stop()` 진행 중 또는 `lock(true)`), ④ 우선순위가 `Low`이고 `Low` 큐가 가득 찬 백프레셔(`Low`에만 적용)입니다. **기동 여부는 검사하지 않으므로** `start()` 이전 push도 유실 없이 처리됩니다.
- `push(std::shared_ptr<ThreadWorker>)` → `void`이므로 검사 대상이 아닙니다(§7의 "expected를 버리지 않는다"에 걸리지 않음). null 워커는 경고 로그 후 무시되고, 이미 기동된 풀이면 그 자리에서 워커가 시작됩니다.
- Job 콜백의 **인자 형태**는 4종(인자 없음 / `bool` / `const int&` / `const std::vector<uint8_t>&`)이며 반환형은 모두 `std::expected<void, std::string>`입니다. 생성자마다 하나만 설정됩니다. 넷 다 없으면 가상 `working()`이 불리는데 **기본 구현은 실패를 반환**하므로, 콜백 없이 만든 `Job`은 파생 클래스에서 `working()`을 재정의해야 합니다.
- 잡 본문의 예외는 `Job::callback_safe_caller`와 `ThreadWorker::do_run`이 이중으로 포착해 `std::unexpected`/로그로 변환합니다. **잡의 예외로 워커 스레드가 죽는 경로는 코드상 없습니다.** 이는 안전망일 뿐이며, 신규 코드는 §7.3대로 예외를 던지지 않고 반환값으로 표현합니다.
- 워커는 생성자에 넘긴 **우선순위 벡터의 나열 순서**대로 큐를 훑어 첫 비어있지 않은 큐에서 꺼냅니다(우선순위 간 = 벡터 순서, 우선순위 내 = FIFO). `LongTerm`은 스케줄러에서 특별 취급되지 않습니다 — §11.1의 "잡 수만큼 전용 `LongTerm` 워커" 규칙이 유일한 보호 장치입니다.

**놓치기 쉬운 API**

| API | 동작 | 주의 |
|-----|------|------|
| `pause(bool)` | 전 워커가 새 잡을 집지 않고 대기하며 잡은 큐에 쌓입니다 | 진행 중인 잡은 멈추지 않습니다 |
| `lock(bool)` / `lock()` | 뮤텍스가 아니라 **push 게이트 플래그**입니다. `true`면 이후 모든 push가 실패합니다 | 이미 큐에 있는 잡·진행 중인 잡에는 영향이 없습니다. `stop()`이 내부적으로 사용합니다 |
| `remove_workers(priority)` | 해당 우선순위 큐를 비우고, 각 워커에서 그 우선순위를 제거하며, 우선순위가 비게 된 워커만 join 후 제거하고 제거 수를 반환합니다 | 제거 대상이 없으면 실패로 반환되지만 **큐 clear와 우선순위 축소는 이미 수행된 뒤**입니다(부분 부작용) |
| `uncompleted_jobs(folder)` | 메모리 큐가 아니라 **백업 폴더**를 스캔해 잡 파일의 원시 바이트를 읽고 원본 파일을 삭제합니다 | `Job` 복원은 호출자 책임입니다. `LongTerm`은 확장자 매핑이 없어 백업 대상이 아닙니다 |

**워커를 만들었으면 그 우선순위로 push되는 잡이 있는지 확인합니다.** 우선순위별 워커 수를 설정으로 노출해 놓고 실제로는 한 우선순위에만 잡을 push하면, 나머지 워커는 영구 유휴 스레드가 됩니다(이 프로젝트의 과거 High/Normal/Low 유휴 워커는 T-009에서 제거했으며 현재 앱 풀은 DLQ 모니터용 LongTerm 1개만 생성).

---

## 12. 문자열 포맷 / 로깅 (Formatting & Logging)

- 문자열 조립은 **`std::format`** 을 사용합니다(`+` 연결, `sprintf` 금지).
- 로그는 프로젝트 공용 전역 싱글턴을 통해 남깁니다: `Logger::handle().write(LogTypes::<레벨>, std::format(...));`. `Logger::handle()`은 **참조**를 반환하며, 설정을 먼저 한 뒤 `start("<파일베이스명>")`을 호출해야 실제로 기록됩니다.
- `LogTypes` 단계(`LogTypes.h`, `enum class : uint8_t`): `None`, `Exception`, `Error`, `Warning`, `Information`, `Debug`, `Sequence`, `Parameter`, `Packet`. 필터는 **`메시지레벨 <= 타겟모드`** — 값이 클수록 더 많이 출력됩니다. 철자 주의: `Warning`/`Information`(WARN/INFO 아님), 최고 심각도는 `Exception`.

| 레벨 | 용도 |
|------|------|
| `Exception` / `Error` | 복구 불가 오류, 예외 |
| `Warning` | 비치명적 이상(예: null 의존성으로 인한 조기 반환) |
| `Information` | 주요 상태 전이 |
| `Debug` | 진단용(예: 객체 소멸) |
| `Sequence` | 호출 흐름 추적 |
| `Parameter` | 설정값/입력 파라미터 기록 |
| `Packet` | 네트워크 패킷 수준 상세 |

---

## 13. 조건부 컴파일 (Conditional Compilation)

- 기능 토글은 CMake 옵션과 매크로로 제어합니다: `USE_*`(예: `USE_ENCRYPT_LIBS`/`USE_ENCRYPT_MODULE`) 및 `BUILD_*`(예: `BUILD_NETWORK_LIB`, `BUILD_REDIS_LIB`, `BUILD_RABBIT_MQ_LIB`, `BUILD_KAFKA_LIB`, `BUILD_SAMPLES`).
- 매크로에 따라 시그니처가 달라지면 `#ifdef`/`#else`/`#endif`로 **양쪽 선언을 모두** 제공합니다(예: 암호화 모듈 사용 시 생성자의 `USE_ENCRYPT_MODULE` 분기).
- 모듈은 `CMakeLists.txt`의 `option()`으로 on/off 가능하게 유지합니다.

---

## 14. 주석 (Comments)

- 주석은 **WHY가 자명하지 않을 때만** 작성합니다. 코드가 스스로 설명하는 내용을 중복 서술하지 않습니다.
- 미완성/보류 작업은 `// TODO:`로 표기하고 무엇이 남았는지 한 줄로 명시합니다.
- 주석 언어는 기존 파일의 관례를 따릅니다(라이브러리 계층은 영어를 기본으로, 프로젝트 문서·설명 주석은 프로젝트 관례).

---

## 15. 빌드 / 모듈 규약 (Build)

- 빌드 시스템은 **CMake + Ninja**, 패키지 관리는 **vcpkg**(`vcpkg.json` 매니페스트)입니다. 툴체인은 `build.sh`의 `VCPKG_ROOT` → `../vcpkg` → `~/vcpkg` 탐색 순서를 따릅니다.
- 새 의존성은 `vcpkg.json`에 추가하고, 새 모듈은 루트 `CMakeLists.txt`에 `option()` + `add_subdirectory()`로 등록합니다.
- 모듈 `CMakeLists.txt`는 **소스를 명시적으로 나열**합니다(`set(HEADER_FILES ...)` / `set(SOURCE_FILES ...)`; `file(GLOB ...)` 금지).
- `target_include_directories(<타겟> PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})`로 flat include(§8)를 성립시킵니다.
- **컴파일 가능한 변경은 반드시 `./build.sh`(또는 `cmake --build`)로 빌드가 에러 0으로 성공**해야 하며, 변경 모듈에 해당하는 `Samples/`를 실행해 검증합니다(`vcpkg.json` 변경 시에는 의존성 재설치 필요, 예: `FRESH_DEPS=1 ./build.sh`).

### 15.1 컴파일이 깨지는 3대 함정 (반드시 인지)

CppToolkit 기반 프로젝트에서 신규/AI 개발자가 거의 항상 처음 막히는 지점입니다. 상세와 매핑 표는 [모듈 사용 가이드](../module_usage_guide.md)에 둡니다.

1. **include는 평평하다(flat).** 프로젝트 헤더는 파일명만(`#include "Logger.h"`), 경로 접두어(`"Utilities/Logger.h"`)는 금지. 각 모듈이 소스 디렉터리를 `PUBLIC`으로 공개하기 때문입니다.
2. **폴더명 ≠ 링크 타겟명 ≠ 네임스페이스.** `target_link_libraries(...)`에는 폴더명이 아니라 **CMake 타겟명**을 씁니다(대표 함정: `ThreadPool/` 폴더 → 타겟 `Thread` → 네임스페이스 `Thread`). 이미 존재하는 모듈을 소비할 때는 상위 타겟만 링크하면 하위 의존이 `PUBLIC`으로 전파됩니다.
3. **vcpkg 패키지명 ≠ `find_package` 이름 ≠ 링크 타겟.** 새 외부 의존을 붙일 때, `vcpkg.json`에 적는 패키지명으로 `find_package()`를 부르면 실패합니다(예: `redis-plus-plus` → `find_package(redis++ CONFIG)` → 플랫폼별 `redis++::redis++` 또는 `redis++::redis++_static`). 세 이름의 불일치가 신규 CMake 작성 시 가장 흔한 실패 원인입니다.

> 이 세 매핑 표(폴더↔타겟↔네임스페이스, vcpkg↔`find_package`↔링크 타겟, 모듈별 API 반환 타입)는 프로젝트마다 다르므로 [모듈 사용 가이드](../module_usage_guide.md)(`YTF-GUIDE-001`)에 두고, 본 문서는 그 존재와 원칙만 규정합니다. 새 모듈 추가 시 그 문서의 매핑과 등록 절차를 확인합니다.

---

## 16. 서비스 경계에 적용하는 규약

[아키텍처 적용 프로파일](../architecture/MSA_ARCHITECTURE.md)과 [API](../API.md)를 따릅니다. 다른 서비스의 내부 헤더를 직접 링크하지 않고 계약으로 연결합니다. 현재 브로커는 AWS SQS FIFO이며 RabbitMQ/Kafka를 도입했다고 가정하지 않습니다.

- 메시지 중복 배달을 고려하고 idempotency와 재시도 상한을 검증합니다. FIFO dedup만으로 exactly-once를 주장하지 않습니다.
- 모든 원격 호출에 timeout을 두어야 합니다. 워커 Redis API에 timeout 설정이 없는 미충족은 [task_list](../task_list.md)에서 추적합니다.
- 진행 중 처리를 drain한 뒤 의존성·스레드·Logger를 종료합니다. 시그널 핸들러에서는 Logger·mutex·객체 해제를 호출하지 않습니다.
- 오류를 성공 반환으로 바꾸어 ACK하거나 상태 쓰기 실패를 숨기지 않습니다. expected의 성공/실패 의미를 소비자 API에서 확인합니다.
- 새 trace/correlation envelope, heartbeat, circuit breaker는 현재 계약에 없습니다. 필요하면 요구·계약을 먼저 확정하며 구현된 기능처럼 기재하지 않습니다.
- Logger 사용은 §12와 [module_usage_guide §2](../module_usage_guide.md)를 함께 확인합니다. `handle()`은 참조, `write()`의 두 번째 인자는 완성된 문자열, 세 번째 인자는 선택적 chrono 시각입니다.

---

## 부록 A. 신규 클래스 골격 예시 (.h / .cpp 한 쌍)

프로젝트에 맞게 `TaskForge` / 클래스명(`Widget`)을 교체하여 사용합니다.

```cpp
// Widget.h
#pragma once

#include "Dependency.h"

#include <atomic>
#include <expected>
#include <memory>
#include <string>

namespace TaskForge
{
	class Widget : public std::enable_shared_from_this<Widget>
	{
	public:
		Widget(const std::string& name);
		virtual ~Widget(void);

		auto get_ptr(void) -> std::shared_ptr<Widget>;

		auto name(const std::string& value) -> void;   // setter
		auto name(void) const -> std::string;           // getter

		auto start(void) -> std::expected<void, std::string>;

	private:
		std::string name_;
		std::atomic_bool running_;
	};
} // namespace TaskForge
```

```cpp
// Widget.cpp
#include "Widget.h"

#include "Logger.h"

#include <format>

using namespace Utilities;

namespace TaskForge
{
	Widget::Widget(const std::string& name) : name_(name), running_(false) {}

	Widget::~Widget(void) { Logger::handle().write(LogTypes::Debug, std::format("destroyed {}", name_)); }

	auto Widget::get_ptr(void) -> std::shared_ptr<Widget> { return shared_from_this(); }

	auto Widget::name(const std::string& value) -> void { name_ = value; }
	auto Widget::name(void) const -> std::string { return name_; }

	auto Widget::start(void) -> std::expected<void, std::string>
	{
		if (running_.load())
		{
			return std::unexpected("already started");
		}
		running_.store(true);
		return {};
	}
} // namespace TaskForge
```

---

## 부록 B. 리뷰 체크리스트 (PR 전 확인)

커밋·PR 전에 아래 항목을 확인합니다.

- [ ] `./build.sh`(또는 `cmake --build`) 성공, 컴파일/링크 에러 0(`vcpkg.json` 변경 시 의존성 재설치).
- [ ] 변경 모듈의 `Samples/` 실행으로 동작 검증.
- [ ] `clang-format -i` 적용 후 `--dry-run --Werror` 통과(GNU/Tab/170, Allman 중괄호, Latest). Main과 Tests의 변경 C++를 함께 검사합니다.
- [ ] 명명 규칙 준수: 클래스/파일 PascalCase, 함수/변수 snake_case, 멤버 `_` 접미사.
- [ ] 모든 함수/메서드가 후행 반환 타입(`auto ... -> T`), 빈 매개변수는 `(void)`.
- [ ] getter/setter는 오버로드 패턴(`get_`/`set_` 접두사 없음), getter는 `const`.
- [ ] 오류 규약 준수: 성공/실패만은 `std::expected<void, std::string>`, 데이터+실패는 `std::expected<T, std::string>`(외부 레거시 API는 실제 헤더 준수). 콜백은 `return {};` / `std::unexpected(...)`. 반환된 expected를 검사 없이 버리지 않습니다.
- [ ] `shared_ptr` 역참조 전 null 검사, 순환 참조는 `weak_ptr`로 차단.
- [ ] 비동기/네트워크 경로에 예외 없음(예외 가능 지점은 `try/catch`로 감싸 반환값 변환).
- [ ] 헤더 `#pragma once`, flat include(경로 접두어 없음), include 수동 정렬, 헤더에서 `using namespace` 없음.
- [ ] 리소스 객체는 설정 → `start` → 사용 → `stop` → `reset` 순서(모듈별 종료 API 확인: ThreadPool은 `wait_stop` 없이 `stop()` 블로킹).
- [ ] 상주 잡은 protected 멤버 본문·한 줄 위임·함수명과 같은 Job 제목·잡 수만큼 전용 LongTerm 워커를 사용합니다. 워커 이름으로 잡과 고정 짝을 가정하지 않습니다(§11.1).
- [ ] while 패턴은 running 하강 → notify_all → pool.stop 순서이고 주기 대기는 condition_variable predicate를 사용합니다. 재등록 패턴은 대기 → push 순서이며 종료 중 실패와 실행 중 실패를 구분합니다.
- [ ] 잡 본문에서 같은 풀의 start/stop/remove_workers를 호출하지 않습니다. stop(true/false) 어느 쪽도 실행 중 콜백을 강제 취소하지 않는다는 사실을 확인합니다(§11.2).
- [ ] 실제 제출되는 잡이 없는 우선순위 워커·설정이 없는지 확인합니다.
- [ ] 로깅은 `Logger` 싱글턴 + `std::format`, 적절한 `LogTypes` 레벨 사용.
- [ ] CMake: 소스 명시 나열(GLOB 금지), 링크는 타겟명(§15.1), 새 외부 의존은 `find_package`/링크 타겟 매핑 일치.
- [ ] 관련 ISO 문서(요구사항·설계·추적성 등) 갱신 필요 시 함께 갱신.

## 변경 이력

| 버전 | 일자 | 변경 |
|---|---|---|
| 초기 CppToolkit 적용판 | 이전 판 | 초기 작성 및 누적 검증 기록 |
| 1.1.0 | 2026-11-07 | 기존 명명·포맷·수명주기 기준 유지; expected·Logger·ThreadPool 실제 API와 프로젝트 모듈 범위 정정 |
| 1.2.0 | 2026-11-07 | 공통 원본 §11.1/11.2·부록B를 체크아웃 API로 검증해 전파, 상주 잡 종료·리뷰 기준 강화 |
