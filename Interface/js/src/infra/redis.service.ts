import { Injectable, OnModuleDestroy } from '@nestjs/common';
import { ConfigService } from '@nestjs/config';
import Redis from 'ioredis';

import { AppConfig } from '../config/configuration';
import { REDIS_COMMAND_TIMEOUT_MS } from './timeouts';

// Redis 상태 조회/기록 어댑터.
@Injectable()
export class RedisService implements OnModuleDestroy {
  private readonly client: Redis;

  constructor(config: ConfigService<AppConfig, true>) {
    const password = config.get('redisPassword', { infer: true });
    this.client = new Redis({
      host: config.get('redisHost', { infer: true }),
      port: config.get('redisPort', { infer: true }),
      db: config.get('redisDb', { infer: true }),
      ...(password ? { password } : {}),
      connectTimeout: 2000,
      // connectTimeout은 TCP 수립에만 적용된다. 연결은 되지만 응답하지 않는 서버에는
      // commandTimeout이 없으면 명령이 무기한 대기한다
      commandTimeout: REDIS_COMMAND_TIMEOUT_MS,
      maxRetriesPerRequest: 2,
      lazyConnect: true,
    });
  }

  async get(key: string): Promise<string | null> {
    return this.client.get(key);
  }

  async set(key: string, value: string, ttlSec: number): Promise<void> {
    if (ttlSec > 0) {
      await this.client.set(key, value, 'EX', ttlSec);
    } else {
      await this.client.set(key, value);
    }
  }

  async ping(): Promise<void> {
    await this.client.ping();
  }

  async onModuleDestroy(): Promise<void> {
    await this.client.quit();
  }
}
