// 환경변수 기반 설정 (12-factor). env 키는 Go/Python 인터페이스와 동일하다.
export interface AppConfig {
  httpPort: number;
  awsRegion: string;
  awsEndpointUrl: string;
  s3Bucket: string;
  s3PresignTtlSec: number;
  sqsRequestQueueUrl: string;
  sqsMessageGroupId: string;
  redisHost: string;
  redisPort: number;
  redisDb: number;
  redisPassword: string;
  redisTtlSec: number;
}

export function loadConfiguration(): AppConfig {
  return {
    httpPort: parseIntEnv('HTTP_PORT', 8082),
    awsRegion: process.env.AWS_REGION ?? 'us-east-1',
    awsEndpointUrl: process.env.AWS_ENDPOINT_URL ?? '',
    s3Bucket: process.env.S3_BUCKET ?? 'taskforge-jobs',
    s3PresignTtlSec: parseIntEnv('S3_PRESIGN_TTL_SEC', 600),
    sqsRequestQueueUrl: process.env.SQS_REQUEST_QUEUE_URL ?? '',
    sqsMessageGroupId: process.env.SQS_MESSAGE_GROUP_ID ?? 'request',
    redisHost: process.env.REDIS_HOST ?? '127.0.0.1',
    redisPort: parseIntEnv('REDIS_PORT', 6379),
    redisDb: parseIntEnv('REDIS_DB', 0),
    redisPassword: process.env.REDIS_PASSWORD ?? '',
    redisTtlSec: parseIntEnv('REDIS_TTL_SEC', 3600),
  };
}

// 필수값이 없으면 부트 시점에 즉시 실패한다 (fail-fast)
export function validateRequired(config: AppConfig): void {
  if (!config.sqsRequestQueueUrl) {
    throw new Error('SQS_REQUEST_QUEUE_URL is required');
  }
  if (!config.s3Bucket) {
    throw new Error('S3_BUCKET is required');
  }
}

function parseIntEnv(key: string, fallback: number): number {
  const raw = process.env[key];
  if (raw === undefined || raw === '') {
    return fallback;
  }
  const parsed = Number.parseInt(raw, 10);
  if (Number.isNaN(parsed)) {
    throw new Error(`invalid integer for ${key}: ${raw}`);
  }
  return parsed;
}
