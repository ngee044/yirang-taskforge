# JavaScript/TypeScript 코딩 규약 (REST API / NestJS)

| 항목 | 내용 |
|---|---|
| 문서 ID | YTF-CONV-JS-001 |
| 버전 | 1.2.0 |
| 기준 표준 | 프로젝트 코드 규약; 체크아웃 설정·API 확인 |
| 작성일 | 2026-11-07 (현행화 판) |
| 최종 수정일 | 2026-11-07 |
| 상태 | Active — 사용자 승인 여부를 뜻하지 않음 |
| 대상 커밋 | `21888c7` |

## 현재 적용 상태

대상은 `Interface/js`입니다. Docker/CI는 Node 22, package.json은 NestJS 11, TypeScript는 strict 설정입니다. build·lint·Prettier format:check·Jest 게이트가 있고 npm audit는 CI 미도입 규약입니다. JobsService는 기존 @Inject 토큰과 Pick으로 사용 메서드만 요구하며, 테스트의 어댑터 이중 단언을 제거했습니다. Redis는 HTTP drain 이후 onApplicationShutdown에서 닫습니다.

yirang-taskforge의 `Interface/js`(NestJS 기반 REST API, TypeScript strict)에 적용하는 코딩 규약입니다. 원본 가이드라인 저장소(`basic-ai-agent-guideline`)에 JS/TS 규약이 없어 Go/Python 규약 문서의 형식·원칙(계층 분리, 소비자 측 인터페이스, 경계에서만 HTTP 변환, 툴체인 게이트)을 준용하여 신규 작성했습니다.

## 목적과 범위

- Node.js + NestJS + TypeScript로 REST API 서비스를 개발할 때의 규약을 정의합니다.
- 심층 아키텍처 원칙(서비스 경계·통신·데이터 일관성·회복탄력성)은 [아키텍처 프로파일](../architecture/MSA_ARCHITECTURE.md)를 참조하며 본 문서는 언어·프레임워크 관용구만 담습니다.

## 언어·도구 버전과 필수 툴체인

- 이 프로젝트의 실행·CI 버전은 Node.js 22, TypeScript 5.x, NestJS 11.x입니다. 패키지 실제 버전은 package-lock.json을 확인합니다.
- 패키지 매니저: `npm` (lockfile `package-lock.json` 커밋 필수).
- 필수 게이트(전부 통과해야 완료):
  - `npm run build` — `tsc` 컴파일 에러 0
  - `npm run lint` — ESLint(typescript-eslint) 에러 0
  - `npm run format:check` — Prettier 검사
  - `npm test` — Jest 서비스·readiness·shutdown 회귀
- 포매터: Prettier (ESLint와 충돌 없는 구성, `eslint-config-prettier`).

## TypeScript 규칙 [핵심]

- `tsconfig.json`은 **strict 모드 필수**: `"strict": true`에 더해 `noImplicitOverride`, `noFallthroughCasesInSwitch`, `forceConsistentCasingInFileNames`를 켭니다.
- `any` 사용 금지. 외부 경계(JSON 파싱, AWS SDK 응답)에서는 `unknown`으로 받아 타입 가드/DTO 검증 후 사용합니다.
- `as` 단언은 최후 수단입니다. 컴파일러가 추론할 수 있도록 코드를 구조화하고, 불가피한 경우 이유를 주석으로 남깁니다.
- non-null 단언(`!`) 금지. 옵셔널 체이닝(`?.`)과 nullish 병합(`??`)으로 명시 처리합니다.
- `enum` 대신 union literal 타입(`type JobStatus = 'queued' | 'running' | 'done' | 'failed'`)을 우선합니다.
- 공개 함수/메서드는 반환 타입을 명시합니다(추론에 맡기지 않음).

## 프로젝트 레이아웃

```text
Interface/js/
├── src/
│   ├── main.ts              # 부트스트랩(얇게 유지: 파이프·종료 훅·리슨만)
│   ├── app.module.ts        # 루트 모듈 조립
│   ├── config/              # 환경변수 로딩·검증 (@nestjs/config + validate)
│   ├── jobs/                # 작업·task_catalog controller → service → infra
│   │   ├── jobs.controller.ts
│   │   ├── jobs.service.ts
│   │   └── dto/             # 요청/응답 DTO (class-validator)
│   ├── health/              # /healthz, /readyz
│   └── infra/               # 인프라 어댑터: s3.service.ts, sqs.service.ts, redis.service.ts
├── test/                    # Jest 서비스·health·HTTP shutdown 회귀
├── package.json / tsconfig.json / eslint.config.mjs / .prettierrc
└── Dockerfile
```

- 도메인 모듈(`jobs/`)은 인프라 구현이 아닌 **주입된 서비스 인터페이스**에 의존합니다.
- 인프라 어댑터(`infra/`)만 AWS SDK·ioredis를 직접 import합니다. 도메인 계층에서 SDK 타입이 누출되지 않게 합니다.

## 네이밍 규칙

| 대상 | 규칙 | 예 |
|---|---|---|
| 파일 | kebab-case + 역할 접미사 | `jobs.controller.ts`, `s3.service.ts` |
| 클래스 | PascalCase + 역할 접미사 | `JobsController`, `SqsService` |
| 인터페이스/타입 | PascalCase (I- 접두사 금지) | `JobStatusRecord` |
| 함수/메서드/변수 | camelCase | `createJob`, `presignPut` |
| 상수 | UPPER_SNAKE_CASE (모듈 수준 고정값만) | `DEFAULT_PRESIGN_TTL_SEC` |
| DTO | PascalCase + `Dto` 접미사 | `CreateJobDto` |

- 외부 JSON 계약(SQS 메시지·Redis 상태·REST 응답)은 시스템 계약([API 계약](../API.md))의 **snake_case 필드명**을 그대로 사용합니다. TS 내부 표현과 계약 직렬화를 분리하고, 경계에서 명시적으로 매핑합니다.

## NestJS 사용 규약

- 컨트롤러는 얇게: DTO 검증(`ValidationPipe`) → 서비스 호출 → 응답 매핑만 수행합니다. 비즈니스 로직 금지.
- 전역 `ValidationPipe`는 `whitelist: true`, `forbidNonWhitelisted: true`, `transform: true`로 설정합니다.
- DI는 생성자 주입만 사용합니다. `@Inject()` 토큰은 인프라 어댑터 교체 지점에만 사용합니다.
- 예외는 도메인 계층에서 도메인 에러로 던지고, HTTP 변환(`HttpException`)은 컨트롤러/예외 필터에서만 수행합니다(서비스 계층에서 `HttpException` 금지 — Python 규약의 "HTTP 변환은 API 계층에서만"과 동일 원칙).
- 응답은 시스템 공통 envelope `{success, data, error:{code,message}}`를 따릅니다.
- graceful shutdown: `app.enableShutdownHooks()`를 켜고, 인프라 연결은 HTTP 요청 drain 이후에 정리합니다. Nest의 `OnModuleDestroy`는 HTTP close 이전이므로 Redis 종료에는 `OnApplicationShutdown`을 사용합니다(T-011 완료).

## 비동기 규칙

- `async/await`만 사용합니다. `.then()` 체이닝·콜백 스타일 금지.
- Promise를 만들면 반드시 `await` 또는 반환합니다. floating promise 금지(ESLint `@typescript-eslint/no-floating-promises`로 강제).
- 이벤트 루프를 막는 동기 I/O(`fs.readFileSync` 등)는 부트스트랩 이외 경로에서 금지합니다.
- 외부 호출(AWS SDK, Redis)에는 타임아웃을 설정합니다.

## 에러 처리

- 도메인 에러 클래스를 정의하고(`JobNotFoundError` 등) `instanceof`로 판별합니다.
- 에러를 삼키지 않습니다. 로깅은 경계(예외 필터·부트스트랩)에서 1회 수행합니다.
- 외부 SDK 에러는 어댑터에서 도메인 에러로 변환해 상위로 전달합니다. SDK 예외 타입이 도메인에 누출되지 않게 합니다.

## 설정 관리

- 12-factor: 모든 설정은 환경변수로 주입합니다. 하드코딩 금지.
- `@nestjs/config` + 검증 함수로 부트 시 필수값(`SQS_REQUEST_QUEUE_URL`, `S3_BUCKET`, `REDIS_ADDR` 등)을 fail-fast 검사합니다.
- 시크릿은 저장소·이미지에 포함하지 않습니다.

## 로깅

- NestJS `Logger`(또는 pino)로 구조적 로깅을 합니다. `console.log` 금지.
- 요청 단위 컨텍스트(job_id 등)를 로그에 포함합니다.

## 테스트

- Jest 단위 테스트: 서비스 계층은 인프라 어댑터를 목으로 대체해 검증합니다.
- 커버리지 수치보다 경계·에러 경로(검증 실패, 인프라 실패, 404) 검증을 우선합니다.

## 보안 기본

- DTO 검증으로 모든 입력을 화이트리스트 방식으로 필터링합니다(`forbidNonWhitelisted`).
- 의존성 취약점 점검: `npm audit`를 CI에 포함합니다.
- 에러 응답에 내부 구현 상세(스택 트레이스 등)를 노출하지 않습니다.

## 지양·금지 사항

- `any`, non-null 단언(`!`), floating promise, `console.log`, 서비스 계층의 `HttpException`, 도메인 계층의 AWS SDK 직접 import, `require()`(ESM/TS import만).
- 선제적 추상화 금지: 어댑터 교체 필요가 실제로 생길 때 인터페이스를 도입합니다.

## 리뷰 체크리스트

- [ ] `build`/`lint`/`format:check`/`jest` 게이트 통과했는가?
- [ ] 계약 필드명(snake_case)이 [API 계약](../API.md)와 일치하는가?
- [ ] 컨트롤러가 얇고 비즈니스 로직이 서비스에 있는가?
- [ ] 외부 호출에 타임아웃이 있는가?
- [ ] 에러가 삼켜지지 않고 경계에서 1회 로깅되는가?
- [ ] DTO 검증이 모든 입력 경로에 적용되는가?

## 변경 이력

| 버전 | 일자 | 변경 |
|---|---|---|
| 초기 언어 가이드 적용판 | 이전 판 | 초기 작성 및 누적 검증 기록 |
| 1.1.0 | 2026-11-07 | 기존 언어 규칙 유지, 실제 구조·도구 상태·미도입 요구 구분 및 잘못된 경로/API 정정 |
| 1.2.0 | 2026-11-07 | Pick 주입·HTTP drain 이후 종료·Prettier 게이트와 테스트 범위 반영 |
