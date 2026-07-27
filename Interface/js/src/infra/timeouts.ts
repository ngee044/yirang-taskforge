// 원격 호출 데드라인. AWS SDK v3(@smithy/node-http-handler)와 ioredis는 기본값이 무제한이므로
// 명시하지 않으면 의존성 스톨 시 요청이 무기한 대기한다 (NFR-REL-02).
export const AWS_CONNECTION_TIMEOUT_MS = 2000;
export const AWS_REQUEST_TIMEOUT_MS = 3000;
export const REDIS_COMMAND_TIMEOUT_MS = 2000;

// 요청 단위 타임아웃은 SDK 재시도로 누적되므로(스톨 시 15초 초과 실측) 작업 전체에
// abortSignal로 상한을 건다. Go의 context.WithTimeout과 동일한 의미다.
export const AWS_OPERATION_TIMEOUT_MS = 5000;
