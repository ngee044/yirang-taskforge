import { Controller, Module, Post } from '@nestjs/common';
import { ConfigService } from '@nestjs/config';
import { NestFactory } from '@nestjs/core';
import Redis from 'ioredis';

import { AppConfig } from '../src/config/configuration';
import { RedisService } from '../src/infra/redis.service';

function deferred(): { promise: Promise<void>; resolve: () => void } {
  let resolve: () => void = () => undefined;
  const promise = new Promise<void>((fulfill) => {
    resolve = fulfill;
  });
  return { promise, resolve };
}

describe('Redis shutdown', () => {
  it('keeps Redis open until an in-flight HTTP request finishes its final write', async () => {
    const started = deferred();
    const release = deferred();
    const shutdownStarted = deferred();
    const quit = jest.spyOn(Redis.prototype, 'quit').mockResolvedValue('OK');
    const redis = new RedisService(
      new ConfigService<AppConfig, true>({
        redisHost: '127.0.0.1',
        redisPort: 6379,
        redisDb: 0,
        redisPassword: '',
      }),
    );
    jest.spyOn(redis, 'get').mockImplementation(async () => {
      started.resolve();
      await release.promise;
      return 'pending';
    });
    const write = jest.spyOn(redis, 'set').mockImplementation(() => {
      if (quit.mock.calls.length > 0) {
        return Promise.reject(new Error('Redis was closed before the request finished'));
      }
      return Promise.resolve();
    });

    @Controller()
    class PendingController {
      constructor(private readonly store: RedisService) {}

      @Post('pending')
      async save(): Promise<{ saved: boolean }> {
        await this.store.get('job');
        await this.store.set('job', 'done', 60);
        return { saved: true };
      }
    }

    @Module({
      controllers: [PendingController],
      providers: [
        { provide: RedisService, useValue: redis },
        {
          provide: 'ShutdownProbe',
          useValue: { beforeApplicationShutdown: () => shutdownStarted.resolve() },
        },
      ],
    })
    class TestModule {}

    const app = await NestFactory.create(TestModule, { logger: false });
    await app.listen(0, '127.0.0.1');
    const request = fetch(`${await app.getUrl()}/pending`, { method: 'POST' });
    let closing: Promise<void> | undefined;
    try {
      await started.promise;
      closing = app.close();
      await shutdownStarted.promise;
      expect(quit).not.toHaveBeenCalled();

      release.resolve();
      const response = await request;
      expect(response.status).toBe(201);
      expect(await response.json()).toEqual({ saved: true });
      await closing;
      expect(write).toHaveBeenCalledWith('job', 'done', 60);
      expect(quit).toHaveBeenCalledTimes(1);
    } finally {
      release.resolve();
      await (closing ?? app.close());
      await request;
      jest.restoreAllMocks();
    }
  });
});
