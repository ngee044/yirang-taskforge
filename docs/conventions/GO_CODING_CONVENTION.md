# Go 코딩 규약 (REST API / Gin)

| 항목 | 내용 |
|---|---|
| 문서 ID | YTF-CONV-GO-001 |
| 버전 | 1.1.0 |
| 기준 표준 | 프로젝트 코드 규약; 체크아웃 설정·API 확인 |
| 작성일 | 2026-11-07 (현행화 판) |
| 최종 수정일 | 2026-11-07 |
| 상태 | Active — 사용자 승인 여부를 뜻하지 않음 |
| 대상 커밋 | `6f7fb67` |

## 현재 적용 상태

대상은 `Interface/golang`입니다. `go.mod:3` 최소 버전은 1.24입니다. 기준선 CI는 gofmt·build·vet·race test를 실행합니다. 아래 규범의 goimports·golangci-lint·staticcheck·govulncheck 및 모듈 tidy 검사는 CI 미도입이며 통과로 간주하지 않습니다. 일반 SQL/gRPC/Swaggo 예시는 사용 시 적용할 규약이고 현재 기능을 뜻하지 않습니다.

yirang-taskforge의 `Interface/golang`(Gin 기반 REST API, Go 1.24+)에 적용하는 코딩 규약입니다. (`basic-ai-agent-guideline/golang_guideline`에서 도입)

## 목적과 범위

- 목적: 팀 전체가 일관된 스타일과 아키텍처로 Go REST API를 작성하여 가독성, 유지보수성, 안정성을 확보합니다.
- 범위: Go 언어 관용구, 프로젝트 레이아웃, 인터페이스 설계, 에러 처리, 동시성, Gin 기반 HTTP 계층, 테스트, 보안까지 백엔드 개발 전반을 다룹니다.
- 기준: 본 문서는 `Effective Go`, `Go Code Review Comments`, `Uber Go Style Guide` 등 커뮤니티 표준을 근간으로 하며, 충돌 시 본 문서가 우선합니다.
- 프로젝트의 서비스 경계와 현재 구현은 상단 적용 상태 및 SAD를 따릅니다. 아래 예시는 스타일·설계 규칙을 설명하며 모든 예제 기능이 구현되어 있다는 뜻은 아닙니다.

## 언어·도구 버전과 필수 툴체인

- Go 버전: Go 1.24 이상을 사용합니다. `go.mod`의 `go` 지시어로 최소 버전을 고정합니다.
- 포매팅·정적 분석 도구는 로컬과 CI에서 동일하게 강제합니다. 통과하지 못한 코드는 병합하지 않습니다.
  - `gofmt` / `goimports`: 포매팅과 import 정리(CI에서 diff 검사).
  - `go vet`: 표준 정적 검사.
  - `golangci-lint`: 통합 린터(프로젝트 루트에 `.golangci.yml`로 규칙 고정).
  - `staticcheck`: 심화 정적 분석(golangci-lint에 포함 가능).
  - `go test -race ./...`: 데이터 레이스 탐지 포함 테스트.
  - `govulncheck ./...`: 알려진 취약점 점검.
- 로컬 검증(커밋/PR 전) 예시:

```bash
gofmt -l .                 # 출력이 있으면 포맷 위반
goimports -l .
go vet ./...
golangci-lint run
go test -race -cover ./...
govulncheck ./...
```

- CI 게이트 규범: 위 검사를 파이프라인에서 실행하여 실패 시 병합을 차단해야 합니다. 현행 CI의 구현 범위와 미도입 도구는 상단 표기를 따릅니다.

## 프로젝트 레이아웃

```text
Interface/golang/
├── cmd/api/main.go
├── internal/
│   ├── aws/                 # SDK 설정
│   ├── config/
│   ├── http/handlers/       # 라우터·핸들러
│   ├── repository/          # redisrepo/s3repo/sqsrepo
│   ├── response/            # 공통 envelope
│   └── service/             # 업무 로직·소비자 인터페이스
├── go.mod
└── go.sum
```

`internal` 경계로 외부 import를 제한합니다. 공개 사용처가 없는 `pkg`·독립 domain 계층·OpenAPI 생성물을 형식상 만들지 않습니다. 인터페이스는 소비자 측에서 최소 메서드로 정의하며 SDK와 구체 어댑터 오류가 서비스로 누출되지 않게 합니다.

## 네이밍 규칙

- `MixedCaps` / `mixedCaps`를 사용하고 `snake_case`나 `SCREAMING_CASE`는 쓰지 않습니다(상수도 예외 아님).
- 가시성은 첫 글자 대소문자로 결정합니다. 노출이 필요 없는 심볼은 소문자(unexported)로 시작합니다. 공개 API 표면을 최소화합니다.
- 약어(initialism)는 대소문자를 통일합니다: `ID`, `URL`, `HTTP`, `API`, `JSON`.

```go
// good
userID, apiURL, httpClient, ServeHTTP, parseJSON

// bad
userId, apiUrl, HttpClient, ServeHttp, parseJson
```

- 패키지명: 짧은 소문자 단어 하나, 언더스코어·복수형·`camelCase` 지양. 호출부에서 의미가 드러나도록 짓고 `stutter`(중복)를 피합니다.

```go
// bad: stutter  (호출부가 user.UserService 가 됨)
package user
type UserService struct{}

// good
package user
type Service struct{}   // 호출부: user.Service
```

- receiver 이름: 1~2글자의 짧은 이름을 타입 전체에서 일관되게 사용합니다(`self`, `this` 금지).

```go
func (s *Service) Create(...) {}   // good
func (this *Service) Create(...) {} // bad
```

- getter에 `Get` 접두어를 붙이지 않습니다. setter만 `Set`을 사용합니다.

```go
func (u *User) Name() string        // good
func (u *User) GetName() string     // bad
func (u *User) SetName(n string)    // good
```

## 포매팅과 import

- 포매팅은 `gofmt`가 유일한 기준입니다. 들여쓰기는 tab이며 수동 정렬하지 않습니다. 저장 시 자동 포맷(에디터 연동)을 권장합니다.
- import는 `goimports`로 정리하며 3그룹으로 나눕니다: 표준 라이브러리 / 서드파티 / 내부 패키지. 그룹 사이는 빈 줄로 구분합니다.

```go
import (
	"context"
	"fmt"
	"net/http"

	"github.com/gin-gonic/gin"
	"github.com/go-playground/validator/v10"

	"github.com/ngee044/yirang-taskforge/Interface/golang/internal/domain"
	"github.com/ngee044/yirang-taskforge/Interface/golang/internal/service"
)
```

- 파일 구성 순서(권장): 패키지 주석 → import → 상수 → 변수 → 타입 → 생성자(`New...`) → 메서드 → 헬퍼 함수. 관련 있는 것끼리 위에서 아래로 읽히게 배치합니다.
- dot import(`import . "..."`)는 금지합니다. blank import(`_`)는 side effect가 필요한 경우(드라이버 등록 등)에만 주석과 함께 사용합니다.

## 인터페이스 설계 [핵심]

- 인터페이스는 작게 유지합니다. 메서드 1~3개가 이상적이며, 큰 인터페이스는 역할별로 분리합니다.
- "accept interfaces, return concrete types": 함수는 인터페이스를 인자로 받고(유연성), 구체 타입을 반환합니다(호출자가 전체 기능 사용 가능).

```go
// good: 인자는 인터페이스, 반환은 구체 타입
func NewService(repo UserRepository) *Service { ... }

// bad: 인터페이스를 반환하면 호출자가 축소된 API만 받게 됨
func NewService(repo UserRepository) UserServicer { ... }
```

- 인터페이스는 구현체가 아니라 소비자(consumer) 패키지에 정의합니다. 리포지토리 인터페이스는 그것을 사용하는 `service` 패키지에 두고, 구현은 `repository` 패키지에 둡니다.

```go
// internal/service/user.go  (소비자가 필요한 만큼만 정의)
type UserRepository interface {
	FindByID(ctx context.Context, id string) (*domain.User, error)
	Save(ctx context.Context, u *domain.User) error
}
```

- 네이밍: 단일 메서드 인터페이스는 `-er` 접미어를 사용합니다(`Reader`, `Notifier`). 역할 기반 이름을 우선합니다.
- 과도한 추상화·선제적 인터페이스를 지양합니다. 구현이 하나뿐이고 테스트에서 목이 필요 없다면 인터페이스를 만들지 않습니다. 필요해질 때 추출합니다(추측 금지).
- 컴파일 타임에 구현을 검증합니다.

```go
// 컴파일 시점에 *PostgresUserRepo 가 UserRepository 를 만족하는지 보장
var _ UserRepository = (*PostgresUserRepo)(nil)
```

- 나쁜 예: 미래를 대비한 거대 인터페이스.

```go
// bad: 사용처가 하나뿐인데 모든 메서드를 미리 인터페이스로 노출
type UserServicer interface {
	Create(...); Update(...); Delete(...); List(...); Export(...); Import(...)
}
```

## 에러 처리

- `error`는 값입니다. 반환된 에러는 반드시 처리하며, 무시할 때는 `_ = f()`로 의도를 명시합니다.
- 컨텍스트를 붙여 래핑할 때 `%w`를 사용해 원인 체인을 보존합니다. 로그·비교를 위해 원인 에러를 잃지 않습니다.

```go
if err != nil {
	return fmt.Errorf("save user %s: %w", id, err)
}
```

- 에러 판별은 문자열 비교가 아니라 `errors.Is` / `errors.As`를 사용합니다.

```go
if errors.Is(err, domain.ErrNotFound) { ... }

var ve validator.ValidationErrors // ValidationErrors 는 []FieldError 슬라이스 타입이므로 포인터가 아닌 값으로 받는다
if errors.As(err, &ve) { ... }
```

- sentinel error는 `errors.New`로 패키지 레벨 변수로 정의합니다(변경 불가하도록 노출 최소화).

```go
var ErrNotFound = errors.New("not found")
```

- 추가 정보가 필요하면 커스텀 에러 타입을 정의하고 `Error()`와 필요 시 `Unwrap()`을 구현합니다.

```go
type ValidationError struct {
	Field string
	Msg   string
}

func (e *ValidationError) Error() string {
	return fmt.Sprintf("field %q: %s", e.Field, e.Msg)
}
```

- 요청 처리 경로에서 `panic`을 흐름 제어로 사용하지 않습니다. 복구 불가능한 초기화 실패에만 제한적으로 허용합니다. Gin의 `Recovery` 미들웨어는 예외로, 예기치 못한 panic으로 서버가 죽는 것을 막는 안전망입니다.
- 에러 로깅 위치 규칙: 에러는 최종적으로 처리되는 지점(주로 핸들러/최상위 경계)에서 한 번만 로깅합니다. 하위 계층은 로깅 대신 래핑해서 반환하여 중복 로그를 막습니다.

## context.Context

- `context.Context`는 항상 함수의 첫 번째 인자로 전달하며 이름은 `ctx`로 합니다.

```go
func (s *Service) GetUser(ctx context.Context, id string) (*domain.User, error)
```

- 취소·데드라인·타임아웃은 상위에서 하위로 전파합니다. HTTP 요청에서는 `c.Request.Context()`를 시작점으로 사용합니다.

```go
ctx, cancel := context.WithTimeout(ctx, 3*time.Second)
defer cancel()
```

- `context.Context`를 구조체 필드로 저장하지 않습니다. 호출마다 인자로 넘깁니다.
- `context.WithValue`는 요청 스코프의 부가 데이터(RequestID 등)에만 제한적으로 쓰고, 함수 파라미터 대체용으로 남용하지 않습니다. 키는 비노출 커스텀 타입을 사용해 충돌을 피합니다.

```go
type ctxKey int
const requestIDKey ctxKey = iota
```

## 동시성

- goroutine의 수명을 명확히 관리합니다. 시작하는 쪽이 언제·어떻게 끝나는지(취소, 완료 채널) 책임집니다. 수명이 불명확한 goroutine은 만들지 않습니다(누수 방지).
- 통신은 채널로, 상태 보호는 `sync`로: 데이터 소유권 이전은 channel, 공유 상태 보호는 `sync.Mutex`가 적합합니다.

```go
select {
case <-ctx.Done():
	return ctx.Err()
case res := <-resultCh:
	return res, nil
}
```

- 여러 goroutine의 완료를 기다리고 첫 에러를 수집할 때 `golang.org/x/sync/errgroup`을 사용합니다. `ctx`와 연동하여 하나가 실패하면 나머지를 취소합니다.

```go
g, ctx := errgroup.WithContext(ctx)
for _, id := range ids {
	id := id // 1.22+에서는 루프 변수 캡처가 안전하나 명시적 캡처는 여전히 명확함
	g.Go(func() error { return process(ctx, id) })
}
if err := g.Wait(); err != nil {
	return err
}
```

- 공유 변수를 다루는 코드는 반드시 `go test -race`로 검증합니다. 레이스가 있으면 병합하지 않습니다.
- 무한정 늘어나는 goroutine을 만들지 않도록 요청당 fan-out 개수를 제한합니다(worker pool 또는 세마포어).

## 값 vs 포인터 리시버·구조체 설계

- 리시버 타입은 한 타입 안에서 일관되게 사용합니다. 값 리시버와 포인터 리시버를 섞지 않습니다.
- 다음 경우 포인터 리시버를 사용합니다: 상태를 변경해야 할 때, 구조체가 클 때, `sync.Mutex` 등 복사하면 안 되는 필드를 포함할 때. 기본적으로 서비스·리포지토리 타입은 포인터 리시버를 씁니다.
- zero value가 바로 유용하도록 설계합니다. 별도 초기화 없이 쓸 수 있으면 API가 단순해집니다(`sync.Mutex`, `bytes.Buffer`가 좋은 예).

```go
type Counter struct {
	mu sync.Mutex // zero value 로 바로 사용 가능
	n  int
}
```

- struct 임베딩은 "is-a"가 아니라 조합/확장에 사용합니다. 임베딩은 상속이 아니며, 필요한 메서드만 승격시키는 도구입니다. 무분별한 임베딩으로 API 표면이 커지지 않게 합니다.

## REST API 계층 구조

- 계층은 `handler → service → repository` 단방향으로 흐릅니다. 상위 계층이 하위 계층을 호출하며, 역방향 호출은 금지합니다.
  - handler: HTTP 관심사(바인딩, 검증, 상태코드, 응답 직렬화)만 담당. 비즈니스 로직 금지.
  - service: 비즈니스 로직·트랜잭션·도메인 규칙. HTTP·DB 세부사항을 모릅니다.
  - repository: 데이터 접근(DB, 캐시, 외부 API). 도메인 모델을 입출력합니다.
- 의존 방향은 항상 안쪽(도메인)으로 향합니다. `domain` 패키지는 다른 계층에 의존하지 않고, 상위 계층이 인터페이스를 통해 하위 구현에 의존합니다.
- 인터페이스 기반 의존성 주입: 구체 타입을 직접 참조하지 않고 인터페이스를 생성자로 주입합니다.

```go
// 수동 DI: main 에서 조립
repo := repository.NewPostgresUserRepo(db)
svc  := service.NewService(repo)          // service 가 정의한 UserRepository 를 만족
h    := handler.NewUserHandler(svc)
```

- 조립은 `cmd/.../main.go`에서 수행합니다. 규모가 커지면 `google/wire`로 컴파일 타임 DI를 도입할 수 있습니다(런타임 리플렉션 DI는 지양).

## Gin 사용 규약

- 엔진과 라우팅은 `router` 패키지에서 조립합니다. `gin.Default()` 대신 `gin.New()`로 시작하고 필요한 미들웨어를 명시적으로 등록합니다.
- 미들웨어 순서(권장): `Recovery` → RequestID → 구조적 로깅 → CORS → 인증. 공통은 전역, 보호 리소스는 `RouterGroup`에 부착합니다.

```go
func New(h *handler.UserHandler) *gin.Engine {
	r := gin.New()
	r.Use(gin.Recovery(), middleware.RequestID(), middleware.Logger(), middleware.CORS())

	api := r.Group("/api/v1")
	{
		users := api.Group("/users")
		users.Use(middleware.Auth())
		users.POST("", h.Create)
		users.GET("/:id", h.GetByID)
	}
	return r
}
```

- 요청 바인딩은 `c.ShouldBindJSON`을 사용하고(자동 400을 내는 `MustBindWith` 계열 지양), validator 태그로 검증합니다. 바인딩 실패는 중앙 에러 응답으로 변환합니다.

```go
func (h *UserHandler) Create(c *gin.Context) {
	var req CreateUserRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		respondError(c, http.StatusBadRequest, "invalid request", err)
		return
	}
	u, err := h.svc.Create(c.Request.Context(), req.toInput())
	if err != nil {
		respondServiceError(c, err) // domain 에러 → 상태코드 매핑
		return
	}
	c.JSON(http.StatusCreated, toUserResponse(u))
}
```

- DTO와 도메인 모델을 분리합니다. 요청/응답 struct(`handler` 계층)와 도메인 모델(`domain` 계층)을 명시적 변환 함수로 오갑니다. 도메인 모델을 직접 JSON으로 노출하지 않습니다.
- 컨텍스트는 항상 `c.Request.Context()`를 하위 계층에 전달합니다. `*gin.Context`를 서비스·리포지토리로 넘기지 않습니다(HTTP 결합 방지).
- 중앙 에러 핸들링: 도메인 에러를 HTTP 상태코드·표준 에러 JSON으로 변환하는 헬퍼(또는 미들웨어)를 두어 핸들러 코드 중복을 없앱니다.

```go
func respondServiceError(c *gin.Context, err error) {
	switch {
	case errors.Is(err, domain.ErrNotFound):
		respondError(c, http.StatusNotFound, "not found", err)
	case errors.Is(err, domain.ErrConflict):
		respondError(c, http.StatusConflict, "conflict", err)
	default:
		respondError(c, http.StatusInternalServerError, "internal error", err)
	}
}
```

- graceful shutdown: `http.Server`를 직접 구성하고 시그널을 받아 진행 중 요청을 마친 뒤 종료합니다.

```go
srv := &http.Server{Addr: ":" + port, Handler: r}
go func() {
	if err := srv.ListenAndServe(); err != nil && !errors.Is(err, http.ErrServerClosed) {
		log.Fatalf("listen: %v", err)
	}
}()

quit := make(chan os.Signal, 1)
signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
<-quit

ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
defer cancel()
if err := srv.Shutdown(ctx); err != nil {
	log.Fatalf("shutdown: %v", err)
}
```

## 요청/응답 DTO와 검증

- 요청과 응답 struct를 분리하여 정의합니다. 요청 struct에만 validator 태그를 붙이고, 응답 struct에는 노출할 필드만 둡니다.

```go
type CreateUserRequest struct {
	Email string `json:"email" binding:"required,email"`
	Name  string `json:"name" binding:"required,min=1,max=100"`
	Age   int    `json:"age" binding:"gte=0,lte=150"`
}

type UserResponse struct {
	ID        string `json:"id"`
	Email     string `json:"email"`
	Name      string `json:"name"`
	CreatedAt string `json:"created_at"`
	// 비밀번호 해시 등 민감/내부 필드는 절대 포함하지 않음
}
```

- 도메인 유출 금지: 도메인 모델을 그대로 응답으로 내보내지 않습니다. 내부 필드(해시, 내부 상태, DB 전용 컬럼)가 API로 새어나가는 것을 막습니다.
- 표준 에러 응답 스키마를 하나로 통일합니다. 필드 검증 실패는 어떤 필드가 왜 실패했는지 알려줍니다.

```go
type ErrorResponse struct {
	Code    string            `json:"code"`              // 예: "VALIDATION_ERROR"
	Message string            `json:"message"`
	Details map[string]string `json:"details,omitempty"` // field -> reason
}
```

- 서버 내부 에러 메시지(스택, 원인 체인 문자열)를 클라이언트 응답에 노출하지 않습니다. 상세는 로그로만 남깁니다.

## 로깅

- 표준 라이브러리 `log/slog`로 구조적(JSON) 로깅을 합니다. 전역 logger 남용 대신 필요한 곳에 주입하거나 `slog.Default()`를 일관되게 사용합니다.
- 레벨을 구분합니다: `Debug`(개발 진단), `Info`(정상 이벤트), `Warn`(비정상이나 처리됨), `Error`(처리 실패). 프로덕션 기본 레벨은 `Info`.

```go
logger.Info("user created",
	slog.String("request_id", reqID),
	slog.String("user_id", u.ID),
)
```

- RequestID 상관관계: 미들웨어에서 요청마다 RequestID를 부여하고, 로그에 항상 포함시켜 요청 단위로 추적 가능하게 합니다.
- 민감정보(비밀번호, 토큰, 주민번호, 카드번호 등)는 로깅하지 않습니다. 필요 시 마스킹합니다. 요청 body 전체를 무분별하게 로깅하지 않습니다.

```go
// bad
logger.Info("login", slog.String("password", req.Password))
```

## 설정 관리

- 12-factor 원칙에 따라 설정은 환경변수에서 읽습니다. 코드에 하드코딩하지 않습니다.
- 기동 시 설정을 한 번 로드·검증하여 구조체로 만들고 하위 계층에 주입합니다. 필수 값 누락 시 기동을 중단(fail fast)합니다.

```go
type Config struct {
	Port        string
	DatabaseURL string
	LogLevel    string
}

func Load() (*Config, error) {
	cfg := &Config{
		Port:        getEnv("PORT", "8080"),
		DatabaseURL: os.Getenv("DATABASE_URL"),
		LogLevel:    getEnv("LOG_LEVEL", "info"),
	}
	if cfg.DatabaseURL == "" {
		return nil, errors.New("DATABASE_URL is required")
	}
	return cfg, nil
}
```

- 비밀정보(DB 비밀번호, API 키, 토큰)는 코드·저장소에 두지 않습니다. 시크릿 매니저나 배포 환경의 환경변수로 주입합니다. 로컬 `.env`는 `.gitignore`에 추가합니다.

## 테스트

- table-driven test를 기본 패턴으로 합니다. 케이스를 슬라이스로 정의하고 `t.Run`으로 서브테스트를 돌립니다.

```go
func TestValidateAge(t *testing.T) {
	tests := []struct {
		name    string
		age     int
		wantErr bool
	}{
		{"valid", 30, false},
		{"negative", -1, true},
		{"too large", 200, true},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			err := ValidateAge(tt.age)
			if (err != nil) != tt.wantErr {
				t.Fatalf("got err=%v, wantErr=%v", err, tt.wantErr)
			}
		})
	}
}
```

- HTTP 핸들러는 `net/http/httptest`로 실제 서버 없이 테스트합니다.

```go
w := httptest.NewRecorder()
req := httptest.NewRequest(http.MethodPost, "/api/v1/users", body)
router.ServeHTTP(w, req)
if w.Code != http.StatusCreated {
	t.Fatalf("status = %d", w.Code)
}
```

- 의존성은 인터페이스로 주입하고 테스트에서 목(mock)/스텁으로 대체합니다. 손으로 쓴 fake 또는 생성기(`mockgen`)를 사용합니다.
- 단언은 표준 라이브러리로 충분하며, 가독성을 위해 `testify`(`assert`/`require`)를 선택적으로 사용할 수 있습니다(프로젝트에서 하나로 통일).
- 커버리지 목표: 핵심 비즈니스 로직(service 계층) `70`% 이상. 커버리지 수치보다 경계·에러 경로 검증을 우선합니다.
- 동시성·공유 상태를 다루는 테스트는 `-race`로 실행합니다.

## 문서화

- exported 심볼(패키지, 타입, 함수, 상수)에는 godoc 주석을 답니다. 주석 문장은 심볼명으로 시작합니다.

```go
// UserService orchestrates user-related business logic.
type UserService struct { ... }

// Create validates the input and persists a new user.
func (s *UserService) Create(ctx context.Context, in CreateInput) (*domain.User, error)
```

- 패키지에는 `doc.go` 또는 대표 파일 상단에 패키지 주석(`// Package xxx ...`)을 둡니다.
- 주석은 WHY가 비자명할 때 답니다. 코드로 자명한 내용을 반복 설명하지 않습니다.
- REST API 스펙은 `swaggo/swag` 주석으로 핸들러에 기술하고 OpenAPI 문서를 생성합니다. 스펙은 `api/`에 두고 코드와 함께 관리합니다.

```go
// Create godoc
// @Summary  Create a user
// @Tags     users
// @Accept   json
// @Produce  json
// @Param    body  body  CreateUserRequest  true  "user payload"
// @Success  201   {object}  UserResponse
// @Failure  400   {object}  ErrorResponse
// @Router   /users [post]
func (h *UserHandler) Create(c *gin.Context) { ... }
```

## 의존성 관리

- Go modules를 사용합니다. `go.mod`, `go.sum`을 항상 커밋하고 정합성을 유지합니다.
- 의존성 추가는 최소화합니다. 표준 라이브러리로 해결 가능한 것은 서드파티를 도입하지 않습니다.
- 정리·검증 루틴을 CI에 포함합니다.

```bash
go mod tidy              # 불필요한 의존성 정리 (diff 발생 시 CI 실패로 처리)
go mod verify            # 체크섬 검증
govulncheck ./...        # 알려진 취약점 점검
```

- 버전 업그레이드는 changelog를 확인하고 테스트 통과를 전제로 진행합니다. 무분별한 `go get -u`를 지양합니다.

## 보안 기본

- 입력 검증: 모든 외부 입력을 validator 태그 등으로 검증하고, 신뢰하지 않습니다.
- SQL은 반드시 파라미터화(placeholder)합니다. 문자열 결합으로 쿼리를 만들지 않습니다(SQL injection 방지).

```go
// good
row := db.QueryRowContext(ctx, "SELECT id, email FROM users WHERE id = $1", id)

// bad
row := db.QueryRowContext(ctx, "SELECT id, email FROM users WHERE id = '"+id+"'")
```

- 인증/인가: 인증은 미들웨어에서, 인가(권한 확인)는 리소스 접근 지점에서 수행합니다. 인증과 인가를 혼동하지 않습니다.
- rate limiting: 공개 엔드포인트에 요청 제한을 적용해 남용을 막습니다(미들웨어 또는 게이트웨이).
- secrets 관리: 비밀정보는 환경변수/시크릿 매니저로 주입하고 로그·에러 응답·저장소에 남기지 않습니다.
- TLS: 외부 통신은 TLS를 사용합니다. 서버 자체 종단 또는 앞단 프록시/로드밸런서에서 TLS를 종단합니다.
- 응답 헤더·에러 메시지로 내부 구현(스택 트레이스, 버전, 경로)을 노출하지 않습니다.

## 서비스 경계와 확장 시 적용 규약

[아키텍처 적용 프로파일](../architecture/MSA_ARCHITECTURE.md)을 따릅니다. 새 서비스 분리는 독립 배포·소유권·부하·장애 격리 근거가 있을 때만 진행하며 파일 크기만으로 경계를 늘리지 않습니다.

- 계약은 [API](../API.md)가 기준입니다. SDK 모델과 타 서비스 내부 구현에 직접 의존하지 않습니다.
- 원격 호출에 deadline을 적용하고 취소를 전파합니다. 재시도는 지수 백오프·jitter·유계 예산을 적용합니다.
- at-least-once 메시지는 멱등성을 검토합니다. FIFO dedup이나 실패 기록만으로 exactly-once를 주장하지 않습니다.
- `/healthz`·`/readyz`와 graceful shutdown을 구현합니다. 종료 중 처리할 요청이 쓰는 의존성을 먼저 닫지 않습니다.
- 외부 응답에는 내부 예외를 노출하지 않고 경계에서 한 번 로깅합니다.
- outbox/saga·gRPC·OpenTelemetry·JWT/mTLS 등은 현재 미도입입니다. 해당 기술을 채택할 때 계약·실패 모델·소유권·추적 전파·보안 요구를 먼저 문서화하고 언어별 공식 API를 확인합니다.

## 지양·금지 사항

- panic을 요청 처리 흐름 제어에 사용하기(Recovery 미들웨어의 안전망 목적 제외).
- 에러 무시(`_`로 명시하지 않은 채 반환값 버리기), 에러 문자열 비교(`err.Error() == "..."`).
- `interface{}`/`any` 남용, 리플렉션 남용, 선제적 거대 인터페이스.
- 전역 가변 상태, 싱글턴 남용, `init()`에 부작용이 큰 로직.
- `context.Context`를 구조체 필드로 저장하거나 인자에서 생략하기.
- `*gin.Context`를 service·repository 계층으로 전파하기.
- 도메인 모델을 API 응답으로 직접 노출하기, DTO 없이 바로 직렬화하기.
- 비밀정보·민감정보 로깅, 문자열 결합 SQL, 하드코딩된 설정/시크릿.
- 수명이 불명확한 goroutine 생성, `-race` 미검증 동시성 코드.
- `gofmt`/린터를 우회한 채 병합, `go.mod`/`go.sum` 불일치 상태로 커밋.

## 리뷰 체크리스트

PR 생성·리뷰 전 아래 항목을 확인합니다.

- [ ] `gofmt`/`goimports` 적용됨(diff 없음), import 3그룹 정렬.
- [ ] `go vet`, `golangci-lint run`, `staticcheck` 통과.
- [ ] `go test -race -cover ./...` 통과, 신규 로직에 테스트 추가.
- [ ] `go mod tidy` 반영, `go.mod`/`go.sum` 정합, `govulncheck` 통과.
- [ ] 모든 에러 처리됨, 래핑에 `%w` 사용, `errors.Is/As`로 판별.
- [ ] 요청 경로에 panic 없음, 에러 로깅은 경계에서 1회.
- [ ] `context.Context`가 첫 인자로 전파됨, 구조체 저장 없음.
- [ ] 계층 방향 준수(handler→service→repository), 인터페이스 기반 DI.
- [ ] 인터페이스는 소비자에 정의, 작고 필요한 만큼만, `var _ Iface = (*T)(nil)` 확인.
- [ ] DTO와 도메인 모델 분리, 도메인/민감 필드 미노출.
- [ ] validator로 입력 검증, SQL 파라미터화, 인증/인가 적용.
- [ ] 구조적 로깅(RequestID 포함), 민감정보 미로깅.
- [ ] 설정은 환경변수, 시크릿 하드코딩 없음.
- [ ] exported 심볼 godoc 주석(심볼명으로 시작), 필요 시 OpenAPI 갱신.
- [ ] graceful shutdown 및 미들웨어 순서 확인(신규 서버/라우터 변경 시).

## 변경 이력

| 버전 | 일자 | 변경 |
|---|---|---|
| 초기 언어 가이드 적용판 | 이전 판 | 초기 작성 및 누적 검증 기록 |
| 1.1.0 | 2026-11-07 | 기존 언어 규칙 유지, 실제 구조·도구 상태·미도입 요구 구분 및 잘못된 경로/API 정정 |
