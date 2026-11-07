import { Controller, Get, Inject, Logger, Res } from '@nestjs/common';
import { Response } from 'express';

import { RedisService } from '../infra/redis.service';
import { S3Service } from '../infra/s3.service';
import { SqsService } from '../infra/sqs.service';

const READINESS_TIMEOUT_MS = 3000;

@Controller()
export class HealthController {
  private readonly startedAt = Date.now();
  private readonly logger = new Logger(HealthController.name);

  constructor(
    @Inject(RedisService) private readonly redis: Pick<RedisService, 'ping'>,
    @Inject(SqsService) private readonly sqs: Pick<SqsService, 'ping'>,
    @Inject(S3Service) private readonly s3: Pick<S3Service, 'ping'>,
  ) {}

  @Get('healthz')
  liveness(): Record<string, unknown> {
    return {
      status: 'ok',
      uptime_sec: Math.round((Date.now() - this.startedAt) / 1000),
      time: new Date().toISOString(),
    };
  }

  @Get('readyz')
  async readiness(@Res() response: Response): Promise<void> {
    const probes: Array<[string, () => Promise<void>]> = [
      ['redis', () => this.redis.ping()],
      ['sqs', () => this.sqs.ping()],
      ['s3', () => this.s3.ping()],
    ];

    const checks: Record<string, string> = {};
    await Promise.all(
      probes.map(async ([name, probe]) => {
        try {
          await withTimeout(probe(), READINESS_TIMEOUT_MS);
          checks[name] = 'ok';
        } catch (error) {
          this.logger.warn(`readiness check ${name} failed: ${String(error)}`);
          checks[name] = 'unavailable';
        }
      }),
    );

    const ready = Object.values(checks).every((value) => value === 'ok');
    response.status(ready ? 200 : 503).json({ status: ready ? 'ready' : 'not_ready', checks });
  }
}

async function withTimeout(promise: Promise<void>, timeoutMs: number): Promise<void> {
  let timer: NodeJS.Timeout | undefined;
  try {
    await Promise.race([
      promise,
      new Promise<never>((_, reject) => {
        timer = setTimeout(() => reject(new Error('probe timeout')), timeoutMs);
      }),
    ]);
  } finally {
    clearTimeout(timer);
  }
}
