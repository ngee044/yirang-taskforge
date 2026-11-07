# Python 코딩 규약 (REST API / FastAPI)

| 항목 | 내용 |
|---|---|
| 문서 ID | YTF-CONV-PY-001 |
| 버전 | 1.2.0 |
| 기준 표준 | 프로젝트 코드 규약; 체크아웃 설정·API 확인 |
| 작성일 | 2026-11-07 (현행화 판) |
| 최종 수정일 | 2026-11-07 |
| 상태 | Active — 사용자 승인 여부를 뜻하지 않음 |
| 대상 커밋 | `21888c7` |

## 현재 적용 상태

대상은 `Interface/python/src/app`과 `tests`입니다. `pyproject.toml:5` 최소 Python 3.11, Docker/CI는 3.12입니다. 현재 도구는 pip 기반 설치·ruff·pytest이며 CI는 lint·format·pytest를 실행합니다. `Tests/worker_recovery_test.py`도 Interface/python 작업 디렉터리에서 같은 Ruff 설정으로 검사합니다. mypy strict·lockfile·uv/poetry·pre-commit·취약점 검사 규약은 미도입으로 남아 있으며 검사 통과로 간주하지 않습니다. ruff 실제 line-length는 120, select에는 ASYNC가 없습니다. 아래 도입 예시 설정과 실물을 구분합니다. SQLAlchemy/Alembic·gRPC·OpenTelemetry 예시는 해당 기술을 채택할 때만 적용합니다.

Python 3.11+ 기반 REST API 프로젝트의 코드 일관성·타입 안정성·유지보수성을 확보하기 위한 규약입니다. FastAPI + Pydantic v2를 기준으로 하며, 이 저장소의 Python API에 적용합니다.

## 목적과 범위

- 본 문서는 `yirang-taskforge`의 Python 백엔드(REST API) 코드에 적용됩니다.
- 대상: 애플리케이션 코드(`src/app/`), 테스트 코드(`tests/`), 배포/유틸 스크립트.
- 목표
  - 일관된 스타일과 강한 타입(`mypy --strict`)으로 결함을 조기에 발견합니다.
  - 계층형 아키텍처(router → service → repository)와 인터페이스 기반 의존성 주입으로 결합도를 낮춥니다.
  - CI에서 lint·format·type check·test를 강제하여 회귀를 방지합니다.
- 본 규약과 상충하는 프로젝트별 규칙이 있으면 프로젝트 루트의 규약이 우선합니다. 규약이 불명확하면 추측하지 말고 팀에 확인합니다.

## 언어·도구 버전과 필수 툴체인

- Python 최소 버전은 pyproject의 3.11+입니다. 런타임을 3.12로 선택하는 것과 최소 지원 버전을 올리는 것은 구분합니다.
- 필수 툴체인 (버전은 lock 파일로 고정)
  - `uv`: 패키지 관리·가상환경·실행 (대안 `poetry`).
  - `ruff`: 린트 + 포매터 (Black·isort·flake8을 대체).
  - `mypy --strict`: 정적 타입 검사.
  - `pytest`: 테스트 러너.
  - `pre-commit`: 커밋 시점 자동 검사.
- 로컬 실행 예시

```bash
uv sync                      # 의존성 설치 (uv.lock 기준)
uv run ruff format .         # 포맷
uv run ruff check --fix .    # 린트 + 자동 수정
uv run mypy .                # 도입 후 타입 검사 (--strict 설정 필요)
uv run pytest                # 테스트
```

- `pyproject.toml`에 도구 설정을 집중합니다(무엇을·왜: 설정 분산을 막고 CI/로컬 동작을 일치시키기 위함).

아래는 규약 도입 시 설정 예시이며 현행 pyproject.toml 내용의 복사본이 아닙니다.

```toml
[tool.ruff]
line-length = 100
target-version = "py311"

[tool.ruff.lint]
select = ["E", "F", "I", "UP", "B", "SIM", "ASYNC"]

[tool.mypy]
python_version = "3.11"
strict = true
warn_unused_ignores = true
disallow_untyped_defs = true
```

- 목표 CI 게이트는 아래 4단계를 반드시 강제해야 합니다. 현행 CI는 format/lint/test를 실행하며 type check 미도입은 상단에 명시합니다.

```bash
uv run ruff format --check .
uv run ruff check .
uv run mypy .
uv run pytest --cov=app --cov-report=term-missing
```

- `pre-commit` 도입 시 커밋 전에 같은 검사를 수행하도록 `.pre-commit-config.yaml`에 ruff·mypy 훅을 등록합니다. 현재 파일과 훅은 미도입이며 게이트 요구가 충족된 것으로 간주하지 않습니다.

## 프로젝트 레이아웃

```text
Interface/python/
├── pyproject.toml
├── src/app/
│   ├── main.py
│   ├── api/                 # routes.py, envelope.py
│   ├── core/                # config.py, errors.py
│   ├── schemas/jobs.py
│   ├── services/job_service.py
│   └── repositories/        # Redis/S3/SQS
└── tests/                  # test_job_service.py, test_health.py
```

src 레이아웃을 유지하고 모듈 단일 책임·순환 import 금지·계층별 명시 타입 변환을 지킵니다. 현재 SQL ORM/db/migrations 계층은 없습니다. lockfile·pre-commit은 미도입 상태이며 존재하는 파일로 나열하지 않습니다.

## 네이밍·스타일

- PEP 8을 기본으로 하며 `ruff format`이 강제합니다.
- 명명 규칙
  - 함수·변수·모듈·패키지: `snake_case` (모듈/패키지는 소문자, 하이픈 금지).
  - 클래스·타입·Pydantic 모델·Enum: `PascalCase`.
  - 상수: `UPPER_SNAKE_CASE`.
  - 모듈 내부 전용(비공개): `_` 접두어(`_build_query`, `_CACHE`).
- 매직 넘버·매직 문자열 금지 → 이름 있는 상수나 Enum으로 표현합니다.

```python
# 나쁜 예
if user.status == 2:
    ...
if retries > 3:
    ...

# 좋은 예
class UserStatus(IntEnum):
    ACTIVE = 1
    SUSPENDED = 2

MAX_RETRIES = 3

if user.status is UserStatus.SUSPENDED:
    ...
if retries > MAX_RETRIES:
    ...
```

- import 순서(표준 라이브러리 → 서드파티 → 로컬)는 `ruff`(isort 규칙 `I`)가 정렬합니다.
- 한 줄 길이는 `pyproject.toml`의 `line-length`를 따릅니다(권장 100).

## 타입 힌트 [필수]

- 모든 공개 함수/메서드의 인자와 반환 타입을 명시합니다. `mypy --strict` 검사에서 누락을 실패로 처리해야 합니다. 현재 mypy 자동 게이트는 미도입이며 상단 미충족 목록을 따릅니다.
- 파일 상단에 `from __future__ import annotations`를 추가합니다(무엇을·왜: 어노테이션을 문자열로 지연 평가해 전방 참조와 성능을 개선).
- Optional은 `X | None` 스타일을 사용합니다(구식 `Optional[X]` 지양).
- `Any`는 원칙적으로 금지하며, 불가피하면 사유를 주석으로 남깁니다.
- 제네릭은 `TypeVar`/`Generic` 또는 PEP 695 문법(3.12+)을 사용합니다.

```python
from __future__ import annotations

from typing import TypeVar, Generic

T = TypeVar("T")

class Page(Generic[T]):
    items: list[T]
    total: int

    def __init__(self, items: list[T], total: int) -> None:
        self.items = items
        self.total = total

def find_user(user_id: int) -> User | None:
    ...

# 나쁜 예: 타입 누락, Optional 구문 혼용, Any 남용
def find_user(user_id):        # 인자/반환 타입 없음
    ...
def get(data: Any) -> Any: ...  # 타입 정보 상실
```

- 컬렉션은 내장 제네릭(`list[str]`, `dict[str, int]`)을 사용합니다(`typing.List` 등 지양).

## 인터페이스 설계 [핵심]

- 상위 계층이 하위 구현이 아닌 **추상(인터페이스)** 에 의존하도록 설계합니다(의존성 역전 원칙, DIP).
- 인터페이스는 **소비자 측**에서 필요한 만큼만 좁게 정의합니다(인터페이스 분리 원칙).

### `typing.Protocol` vs `abc.ABC` 선택 기준

| 구분 | `typing.Protocol` (구조적) | `abc.ABC` (명목적) |
|------|---------------------------|--------------------|
| 결합 | 구현체가 인터페이스를 몰라도 됨(덕 타이핑) | 구현체가 명시적으로 상속 |
| 용도 | 리포지토리·게이트웨이 등 경계 계약, 외부 타입 대체 | 공통 로직 상속·강제되는 계약 |
| 검사 | mypy가 정적으로 구조 일치 확인 | 상속 누락을 런타임에도 강제 |
| 권장 | **기본 선택**(소비자 측 좁은 인터페이스) | 공유 구현/템플릿 메서드가 필요할 때 |

- `@runtime_checkable`은 메서드 **존재 여부만** 검사하고 시그니처는 검사하지 않습니다. 런타임 `isinstance` 검사에만 제한적으로 사용하고, 정적 검증(mypy)을 우선합니다.

### 리포지토리 Protocol 정의 (소비자=service 측)

```python
from __future__ import annotations

from typing import Protocol

from app.models import User

class UserRepository(Protocol):
    async def get_by_id(self, user_id: int) -> User | None: ...
    async def add(self, user: User) -> User: ...
```

### 서비스에 인터페이스 주입

```python
from __future__ import annotations

from app.core.exceptions import UserNotFoundError

class UserService:
    def __init__(self, users: UserRepository) -> None:
        # 구현이 아닌 Protocol에 의존 → 테스트 시 fake로 교체 가능
        self._users = users

    async def get_profile(self, user_id: int) -> User:
        user = await self._users.get_by_id(user_id)
        if user is None:
            raise UserNotFoundError(user_id)
        return user
```

### FastAPI `Depends`와의 결합

```python
from fastapi import Depends

def get_user_service(
    users: UserRepository = Depends(get_user_repository),
) -> UserService:
    return UserService(users)
```

- 구현체(`SqlAlchemyUserRepository`)는 `repositories/`에 두고, 조립(와이어링)은 `api/deps.py`에서만 수행합니다.

## 예외 처리

- 도메인 예외 계층을 설계하고, 최상위 베이스 예외를 둡니다(무엇을·왜: 계층별 일괄 처리와 HTTP 매핑을 단순화).

```python
class AppError(Exception):
    """애플리케이션 도메인 예외의 베이스."""

class UserNotFoundError(AppError):
    def __init__(self, user_id: int) -> None:
        super().__init__(f"user not found: {user_id}")
        self.user_id = user_id
```

- `except:`(bare except)와 `except Exception: pass`(예외 삼키기) 금지. 반드시 구체 예외를 잡고, 처리하지 못하면 다시 raise 합니다.

```python
# 나쁜 예: 원인 유실 + 침묵
try:
    do_work()
except:            # bare except
    pass           # 예외 삼킴

# 좋은 예
try:
    do_work()
except TimeoutError as exc:
    logger.warning("work timed out", exc_info=exc)
    raise
```

- **도메인 예외 → HTTP 변환은 API 계층에서만** 수행합니다. service·repository는 `HTTPException`을 던지지 않습니다(계층 분리).
- FastAPI 전역 exception handler를 등록해 도메인 예외를 HTTP 응답으로 일괄 변환합니다.

```python
from fastapi import FastAPI, Request
from fastapi.responses import JSONResponse

def register_exception_handlers(app: FastAPI) -> None:
    @app.exception_handler(UserNotFoundError)
    async def _handle_not_found(request: Request, exc: UserNotFoundError) -> JSONResponse:
        return JSONResponse(status_code=404, content={"detail": str(exc)})
```

- 예외 메시지에 비밀번호·토큰 등 민감정보를 넣지 않습니다.

## 비동기

- 엔드포인트는 원칙적으로 `async def`로 작성하고 `await`로 I/O를 수행합니다.
- **이벤트 루프 블로킹 금지**: `async` 경로에서 동기 블로킹 I/O(동기 DB 드라이버, `requests`, `time.sleep`, 무거운 CPU 연산)를 직접 호출하지 않습니다.
- 라이브러리는 async 지원을 사용합니다(예: `httpx.AsyncClient`, `asyncpg`/SQLAlchemy async, `redis.asyncio`).
- 불가피한 블로킹 작업은 스레드로 오프로딩합니다.

```python
import anyio

# 나쁜 예: async 함수 안에서 동기 블로킹 → 이벤트 루프 정지
async def bad_handler() -> bytes:
    import time
    time.sleep(2)              # 루프 블로킹
    return blocking_read()     # 동기 I/O

# 좋은 예: 블로킹 작업을 워커 스레드로
async def good_handler() -> bytes:
    return await anyio.to_thread.run_sync(blocking_read)
```

- 순수 CPU 바운드 작업이 무거우면 스레드 대신 프로세스풀/전용 워커를 고려합니다(GIL 영향).
- 동기 함수(`def`)와 코루틴을 혼용하지 않습니다. 동기 라우터를 어쩔 수 없이 쓰면 FastAPI가 스레드풀에서 실행함을 인지하고 일관성을 유지합니다.
- 코루틴 결과는 반드시 `await` 하거나 태스크로 관리합니다(await 누락 경고를 `ruff`의 `ASYNC` 규칙으로 잡습니다).

## Pydantic v2

- 요청/응답 스키마는 `schemas/`에 `BaseModel`로 정의합니다. 요청·응답·도메인(ORM) 모델을 분리합니다(무엇을·왜: 입력 검증·출력 노출·저장 구조는 서로 다른 관심사).

```python
from datetime import datetime

from pydantic import BaseModel, ConfigDict, EmailStr, Field, field_validator

class UserCreate(BaseModel):
    email: EmailStr
    password: str = Field(min_length=8, max_length=128)

    @field_validator("email")
    @classmethod
    def normalize_email(cls, v: str) -> str:
        return v.strip().lower()

class UserRead(BaseModel):
    # ORM 인스턴스 → 스키마 자동 매핑
    model_config = ConfigDict(from_attributes=True)

    id: int
    email: EmailStr
    created_at: datetime
```

- 필드 단위 검증은 `@field_validator`, 모델 전체(필드 간 관계) 검증은 `@model_validator`를 사용합니다.
- ORM 매핑은 `model_config = ConfigDict(from_attributes=True)`와 `UserRead.model_validate(orm_obj)`를 사용합니다.
- 비밀번호·토큰 등 민감 필드는 응답 스키마에 절대 포함하지 않습니다(별도 Read 스키마 사용).
- v1 문법(`class Config`, `@validator`, `.dict()`, `.parse_obj()`)은 사용하지 않습니다(v2: `model_config`, `@field_validator`, `.model_dump()`, `.model_validate()`).

## FastAPI 사용 규약

- 라우터는 `APIRouter`로 모듈화하고 `app.include_router`로 등록합니다.

```python
from fastapi import APIRouter, Depends, status

router = APIRouter(prefix="/users", tags=["users"])

@router.post("", response_model=UserRead, status_code=status.HTTP_201_CREATED)
async def create_user(
    payload: UserCreate,
    service: UserService = Depends(get_user_service),
) -> UserRead:
    user = await service.create(payload)
    return UserRead.model_validate(user)
```

- 응답 계약을 위해 `response_model`, `status_code`, `tags`를 명시합니다.
- 파라미터 구분: 경로는 함수 인자(`user_id: int`), 쿼리는 `Query(...)`, 바디는 Pydantic 모델로 받습니다.
- 의존성은 `Depends`로 주입하고, 조립은 `api/deps.py`에 모읍니다.
- 앱 초기화/정리는 `lifespan`으로 관리합니다(deprecated된 `@app.on_event` 금지).

```python
from contextlib import asynccontextmanager
from collections.abc import AsyncIterator

@asynccontextmanager
async def lifespan(app: FastAPI) -> AsyncIterator[None]:
    await init_engine()      # startup
    yield
    await dispose_engine()   # shutdown

app = FastAPI(lifespan=lifespan)
register_exception_handlers(app)
app.include_router(router, prefix="/api/v1")
```

- 무거운 후처리는 `BackgroundTasks`(짧은 작업) 또는 별도 큐/워커(긴 작업)로 분리합니다.
- 미들웨어(요청 ID 부여, 처리시간 측정 등)는 `app.add_middleware`로 등록합니다.

## 계층 구조

- 의존 방향은 단방향입니다: `router(api) → service → repository`. 역방향 참조 금지.
- 각 계층의 책임
  - router: HTTP 관심사(파싱·검증·상태코드·직렬화), 의존성 조립.
  - service: 비즈니스 규칙·트랜잭션 경계·도메인 예외 발생.
  - repository: 데이터 접근만 담당(비즈니스 로직 없음).
- 계층 간 결합은 Protocol + `Depends`로 주입합니다. service는 구체 repository 구현을 import하지 않습니다.
- 도메인/ORM 모델과 API 스키마를 분리하고, 경계에서 명시적으로 변환합니다.

```text
UserRouter → UserService → UserRepository(Protocol)
                                 ↑ 구현: SqlAlchemyUserRepository
```

## 데이터 접근

- SQLAlchemy 2.0 async(`AsyncSession`, `async_sessionmaker`)를 사용합니다.
- 세션은 요청 스코프로 관리하고 `Depends`로 주입합니다.

```python
from collections.abc import AsyncIterator
from sqlalchemy.ext.asyncio import AsyncSession

async def get_session() -> AsyncIterator[AsyncSession]:
    async with async_session_factory() as session:
        yield session
```

- raw SQL은 지양하고 ORM/Core 식을 사용합니다. 불가피하면 **반드시 파라미터 바인딩**합니다(SQL 인젝션 방지).

```python
from sqlalchemy import text

# 나쁜 예: 문자열 포매팅 → SQL 인젝션
await session.execute(text(f"SELECT * FROM users WHERE email = '{email}'"))

# 좋은 예: 파라미터 바인딩
await session.execute(
    text("SELECT * FROM users WHERE email = :email"), {"email": email}
)
```

- 데이터 접근은 리포지토리 패턴으로 캡슐화하고, service는 세션이 아닌 리포지토리에 의존합니다.
- 스키마 변경은 `alembic` 마이그레이션으로 버전 관리합니다(수동 DDL 금지).

```bash
uv run alembic revision --autogenerate -m "add users table"
uv run alembic upgrade head
```

## 로깅

- 표준 `logging` 또는 `structlog`로 **구조적 로깅**(JSON 등)을 사용합니다. `print` 금지.
- 요청마다 Request ID를 발급해 로그에 포함하고, 응답 헤더로도 반환합니다(추적성).
- 로그 레벨을 목적에 맞게 사용합니다: `debug`/`info`/`warning`/`error`.
- 민감정보(비밀번호·토큰·주민번호·카드번호)를 로그에 남기지 않습니다.

```python
import logging

logger = logging.getLogger(__name__)

# 나쁜 예
print("user login", user.email, user.password)   # print + 민감정보

# 좋은 예: 구조적 필드, 민감정보 제외
logger.info("user login", extra={"user_id": user.id, "request_id": request_id})
```

## 설정 관리

- `pydantic-settings`의 `BaseSettings`로 설정을 타입 안전하게 로드합니다. 코드에 하드코딩 금지(12-factor).

```python
from pydantic import Field
from pydantic_settings import BaseSettings, SettingsConfigDict

class Settings(BaseSettings):
    model_config = SettingsConfigDict(env_file=".env", extra="ignore")

    database_url: str
    log_level: str = "INFO"
    jwt_secret: str = Field(min_length=32)

settings = Settings()   # 앱 시작 시 1회 로드
```

- `.env`는 로컬 개발용이며 **커밋 금지**(`.gitignore`에 추가). `.env.example`로 필요한 키만 문서화합니다.
- secrets는 환경변수/시크릿 매니저(예: AWS Secrets Manager)로 주입하고, 저장소·이미지에 포함하지 않습니다.

## 테스트

- `pytest`를 사용하고 `tests/`를 `unit`/`integration`으로 나눕니다.
- FastAPI는 `httpx.AsyncClient`(async) 또는 `TestClient`(sync)로 검증합니다.

```python
import pytest
from httpx import ASGITransport, AsyncClient

@pytest.mark.anyio
async def test_create_user() -> None:
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        resp = await client.post("/api/v1/users", json={"email": "a@b.com", "password": "pw12345678"})
    assert resp.status_code == 201
```

- 의존성은 Protocol 기반 fake/mock으로 교체하거나 `app.dependency_overrides`로 오버라이드합니다(외부 I/O 격리).

```python
class FakeUserRepository:
    def __init__(self) -> None:
        self._store: dict[int, User] = {}

    async def get_by_id(self, user_id: int) -> User | None:
        return self._store.get(user_id)

    async def add(self, user: User) -> User:
        self._store[user.id] = user
        return user
```

- 테스트 데이터는 fixture/factory로 생성합니다. 공용 픽스처는 `conftest.py`에 둡니다.
- 커버리지를 측정(`--cov=app`)하고 핵심 로직(service)의 커버리지 기준을 팀에서 정합니다.

## 문서화

- 공개 모듈·클래스·함수에는 Google 스타일 docstring을 작성합니다(자명한 경우 생략 가능).

```python
def paginate(items: list[T], page: int, size: int) -> Page[T]:
    """목록을 페이지 단위로 자릅니다.

    Args:
        items: 대상 목록.
        page: 1부터 시작하는 페이지 번호.
        size: 페이지당 항목 수.

    Returns:
        해당 페이지의 항목과 전체 개수를 담은 Page.

    Raises:
        ValueError: page 또는 size가 1 미만인 경우.
    """
```

- FastAPI의 자동 OpenAPI 문서(`/docs`, `/redoc`)를 활용합니다. `summary`/`description`/`response_model`로 스펙을 풍부하게 합니다.
- 타입 힌트 자체가 문서입니다. 정확한 타입을 유지해 OpenAPI 스키마 품질을 확보합니다.

## 의존성 관리

- 의존성은 `pyproject.toml`에 선언하고 lock 파일(`uv.lock` 또는 `poetry.lock`)을 커밋합니다(재현 가능한 빌드).
- 런타임/개발 의존성을 분리합니다(예: `[project.optional-dependencies]` 또는 dependency group의 `dev`).
- 버전은 상한을 두어 고정성을 확보합니다. 임의 업그레이드 금지, 업그레이드는 PR로 검토합니다.
- 취약점 점검을 CI에 포함합니다(예: `uv run pip-audit` 또는 pip-audit).

## 보안 기본

- 입력 검증은 Pydantic 스키마로 강제합니다(경계에서 신뢰할 수 없는 입력을 검증·정규화).
- 인증/인가는 FastAPI 의존성으로 구현합니다(OAuth2/JWT). 보호가 필요한 라우터에 인증 `Depends`를 부착합니다.

```python
from fastapi import Depends
from fastapi.security import OAuth2PasswordBearer

oauth2_scheme = OAuth2PasswordBearer(tokenUrl="/api/v1/auth/token")

async def get_current_user(token: str = Depends(oauth2_scheme)) -> User:
    return await verify_token(token)   # 실패 시 401 예외
```

- CORS는 `CORSMiddleware`로 **허용 오리진을 명시**합니다(`allow_origins=["*"]`와 credentials 동시 사용 금지).
- rate limiting을 도입합니다(게이트웨이 또는 애플리케이션 미들웨어 등).
- secrets는 환경변수/시크릿 매니저로 주입하고 로그·응답·에러 메시지에 노출하지 않습니다.
- DB 접근은 ORM/파라미터 바인딩으로 SQL 인젝션을 방지합니다(문자열 포매팅 금지).
- 비밀번호는 평문 저장 금지, 강한 해시(예: `bcrypt`/`argon2`)를 사용합니다.

## 서비스 경계와 확장 시 적용 규약

[아키텍처 적용 프로파일](../architecture/MSA_ARCHITECTURE.md)을 따릅니다. 새 서비스 분리는 독립 배포·소유권·부하·장애 격리 근거가 있을 때만 진행하며 파일 크기만으로 경계를 늘리지 않습니다.

- 계약은 [API](../API.md)가 기준입니다. SDK 모델과 타 서비스 내부 구현에 직접 의존하지 않습니다.
- 원격 호출에 deadline을 적용하고 취소를 전파합니다. 재시도는 지수 백오프·jitter·유계 예산을 적용합니다.
- at-least-once 메시지는 멱등성을 검토합니다. FIFO dedup이나 실패 기록만으로 exactly-once를 주장하지 않습니다.
- `/healthz`·`/readyz`와 graceful shutdown을 구현합니다. 종료 중 처리할 요청이 쓰는 의존성을 먼저 닫지 않습니다.
- 외부 응답에는 내부 예외를 노출하지 않고 경계에서 한 번 로깅합니다.
- outbox/saga·gRPC·OpenTelemetry·JWT/mTLS 등은 현재 미도입입니다. 해당 기술을 채택할 때 계약·실패 모델·소유권·추적 전파·보안 요구를 먼저 문서화하고 언어별 공식 API를 확인합니다.

## 지양·금지 사항

- `print`로 로깅 금지 → 구조적 로거 사용.
- bare `except:` / `except Exception: pass`로 예외 삼키기 금지.
- `async` 경로에서 동기 블로킹 I/O 호출 금지.
- 타입 힌트 없는 공개 함수/메서드 금지, `Any` 남용 금지.
- ORM 모델을 API 응답으로 직접 노출 금지 → Read 스키마로 변환.
- service/repository에서 `HTTPException` 발생 금지(HTTP 변환은 API 계층).
- SQL 문자열 포매팅 금지(파라미터 바인딩 사용).
- 전역 가변 상태·싱글턴 남용 금지(의존성 주입 사용).
- secrets 하드코딩/커밋 금지, `.env` 커밋 금지.
- Pydantic v1 문법(`.dict()`, `@validator`, `class Config`) 사용 금지.
- `on_event` 대신 `lifespan` 사용.
- wildcard import(`from x import *`) 금지.

## 리뷰 체크리스트

PR 전 아래 항목을 확인합니다.

- [ ] `ruff format --check`, `ruff check`, `mypy .`, `pytest`가 모두 통과합니다.
- [ ] 모든 공개 함수/메서드에 타입 힌트가 있고 `Any` 남용이 없습니다.
- [ ] 계층 의존 방향(router → service → repository)이 지켜지고, service는 Protocol에 의존합니다.
- [ ] HTTP 변환(`HTTPException`)이 API 계층에만 존재합니다.
- [ ] `async` 경로에 동기 블로킹 I/O가 없습니다.
- [ ] 요청/응답/도메인 모델이 분리되었고, ORM 모델을 직접 노출하지 않습니다.
- [ ] Pydantic v2 문법을 사용하고 민감 필드가 응답에 없습니다.
- [ ] SQL은 파라미터 바인딩을 사용하고, 스키마 변경은 alembic 마이그레이션에 반영되었습니다.
- [ ] 로그에 민감정보가 없고 Request ID가 포함됩니다.
- [ ] secrets/`.env`가 커밋되지 않았습니다.
- [ ] 새 로직에 단위 테스트가 추가되었고 외부 I/O는 fake/override로 격리되었습니다.
- [ ] 신규/변경 엔드포인트가 `response_model`·`status_code`·`tags`를 갖고 OpenAPI에 올바르게 노출됩니다.
- [ ] 매직 넘버/문자열이 상수·Enum으로 치환되었습니다.

## 변경 이력

| 버전 | 일자 | 변경 |
|---|---|---|
| 초기 언어 가이드 적용판 | 이전 판 | 초기 작성 및 누적 검증 기록 |
| 1.1.0 | 2026-11-07 | 기존 언어 규칙 유지, 실제 구조·도구 상태·미도입 요구 구분 및 잘못된 경로/API 정정 |
| 1.2.0 | 2026-11-07 | CI format·health 회귀·공용 복구 스크립트의 Python 규약 검사 범위 반영 |
