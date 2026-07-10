import { Controller, Get, Res } from '@nestjs/common';
import { Response } from 'express';

import { RedisService } from '../infra/redis.service';
import { S3Service } from '../infra/s3.service';
import { SqsService } from '../infra/sqs.service';

const READINESS_TIMEOUT_MS = 3000;

@Controller()
export class HealthController {
  private readonly startedAt = Date.now();

  constructor(
    private readonly redis: RedisService,
    private readonly sqs: SqsService,
    private readonly s3: S3Service,
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
    let ready = true;
    for (const [name, probe] of probes) {
      try {
        await withTimeout(probe(), READINESS_TIMEOUT_MS);
        checks[name] = 'ok';
      } catch (error) {
        checks[name] = error instanceof Error ? error.message : String(error);
        ready = false;
      }
    }

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
