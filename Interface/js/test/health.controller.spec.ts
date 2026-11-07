import { Logger } from '@nestjs/common';
import { Response } from 'express';

import { HealthController } from '../src/health/health.controller';

function makeResponse(): { response: Response; status: jest.Mock; json: jest.Mock } {
  const status = jest.fn().mockReturnThis();
  const json = jest.fn();
  // Express가 제공하는 응답에서 이 핸들러가 사용하는 두 메서드만 대체합니다.
  const response = { status, json } as unknown as Response;
  return { response, status, json };
}

describe('HealthController readiness', () => {
  beforeEach(() => {
    jest.useFakeTimers();
    jest.spyOn(Logger.prototype, 'warn').mockImplementation(() => undefined);
  });

  afterEach(() => {
    jest.restoreAllMocks();
    jest.useRealTimers();
  });

  it('reports ready when all dependencies respond', async () => {
    const probe = { ping: jest.fn(() => Promise.resolve()) };
    const controller = new HealthController(probe, probe, probe);
    const { response, status, json } = makeResponse();

    await controller.readiness(response);

    expect(probe.ping).toHaveBeenCalledTimes(3);
    expect(status).toHaveBeenCalledWith(200);
    expect(json).toHaveBeenCalledWith({
      status: 'ready',
      checks: { redis: 'ok', sqs: 'ok', s3: 'ok' },
    });
    expect(jest.getTimerCount()).toBe(0);
  });

  it('returns fixed check values while logging dependency failures', async () => {
    const probe = {
      ping: jest.fn(() => Promise.reject(new Error('redis.internal:6379'))),
    };
    const controller = new HealthController(probe, probe, probe);
    const { response, status, json } = makeResponse();

    await controller.readiness(response);

    expect(status).toHaveBeenCalledWith(503);
    expect(json).toHaveBeenCalledWith({
      status: 'not_ready',
      checks: { redis: 'unavailable', sqs: 'unavailable', s3: 'unavailable' },
    });
    expect(Logger.prototype.warn).toHaveBeenCalledTimes(3);
    expect(jest.getTimerCount()).toBe(0);
  });

  it('finishes all stalled probes within one three-second budget', async () => {
    const probe = { ping: jest.fn(() => new Promise<void>(() => undefined)) };
    const controller = new HealthController(probe, probe, probe);
    const { response, status, json } = makeResponse();

    const pending = controller.readiness(response);
    expect(probe.ping).toHaveBeenCalledTimes(3);
    await jest.advanceTimersByTimeAsync(2999);
    expect(status).not.toHaveBeenCalled();
    await jest.advanceTimersByTimeAsync(1);

    expect(status).toHaveBeenCalledWith(503);
    await pending;
    expect(json).toHaveBeenCalledWith({
      status: 'not_ready',
      checks: { redis: 'unavailable', sqs: 'unavailable', s3: 'unavailable' },
    });
    expect(jest.getTimerCount()).toBe(0);
  });
});
