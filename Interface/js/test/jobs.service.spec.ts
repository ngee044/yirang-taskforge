// JobsService 계약 검증: SQS 메시지/Redis 상태 문서가 docs/API.md와 일치해야 한다.
import { ConfigService } from '@nestjs/config';

import { EnqueueError, JobNotFoundError } from '../src/common/domain-errors';
import { AppConfig } from '../src/config/configuration';
import { RedisService } from '../src/infra/redis.service';
import { S3Service } from '../src/infra/s3.service';
import { SqsService } from '../src/infra/sqs.service';
import { JobsService } from '../src/jobs/jobs.service';

interface FakeInfra {
  s3: Pick<S3Service, 'bucket' | 'presignPut'>;
  sqs: Pick<SqsService, 'enqueue'>;
  redis: Pick<RedisService, 'get' | 'set'>;
  sentBodies: string[];
  sentGroups: string[];
  stored: Map<string, string>;
  calls: string[];
}

function makeService(options?: {
  enqueueFails?: boolean;
  storeFails?: boolean;
}): { service: JobsService } & FakeInfra {
  const sentBodies: string[] = [];
  const sentGroups: string[] = [];
  const stored = new Map<string, string>();
  const calls: string[] = [];

  const s3 = {
    bucket: 'test-bucket',
    presignPut: jest.fn((key: string) => Promise.resolve(`https://example.com/${key}`)),
  };

  const sqs = {
    enqueue: jest.fn((body: string, group: string) => {
      calls.push('enqueue');
      if (options?.enqueueFails) {
        return Promise.reject(new Error('sqs down'));
      }
      sentBodies.push(body);
      sentGroups.push(group);
      return Promise.resolve();
    }),
  };

  const redis = {
    get: jest.fn((key: string) => Promise.resolve(stored.get(key) ?? null)),
    set: jest.fn((key: string, value: string) => {
      calls.push('set');
      if (options?.storeFails) {
        return Promise.reject(new Error('store unavailable'));
      }
      stored.set(key, value);
      return Promise.resolve();
    }),
  };

  const config = new ConfigService<AppConfig, true>({
    redisTtlSec: 3600,
    sqsMessageGroupId: 'request',
  });

  const service = new JobsService(s3, sqs, redis, config);
  return { service, s3, sqs, redis, sentBodies, sentGroups, stored, calls };
}

describe('JobsService', () => {
  it('builds the contract queue message and queued status', async () => {
    const { service, s3, sentBodies, sentGroups, stored, calls } = makeService();

    const result = await service.createJob({
      task_name: 'wordcount',
      arguments: ['--verbose'],
      input_files: [{ filename: 'a.txt' }, { filename: 'b.txt' }],
      timeout_sec: 20,
    });

    expect(result.job_id).toBeTruthy();
    expect(result.upload_urls).toHaveLength(2);
    expect(result.upload_urls.every((upload) => upload.method === 'PUT')).toBe(true);
    expect(s3.presignPut).toHaveBeenNthCalledWith(1, `jobs/${result.job_id}/inputs/a.txt`);
    expect(s3.presignPut).toHaveBeenNthCalledWith(2, `jobs/${result.job_id}/inputs/b.txt`);
    expect(calls).toEqual(['set', 'enqueue']);

    expect(sentBodies).toHaveLength(1);
    expect(sentGroups).toEqual(['request']);

    const message = JSON.parse(sentBodies[0]) as Record<string, unknown>;
    expect(message.mode).toBe('execute');
    expect(message.job_id).toBe(result.job_id);
    expect(message.task).toEqual({ name: 'wordcount', arguments: ['--verbose'], timeout_sec: 20 });
    expect(message.output_prefix).toBe(`jobs/${result.job_id}/outputs/`);
    expect(message.download_s3).toEqual([
      { bucket: 'test-bucket', key: `jobs/${result.job_id}/inputs/a.txt`, method: 'GET' },
      { bucket: 'test-bucket', key: `jobs/${result.job_id}/inputs/b.txt`, method: 'GET' },
    ]);

    const status = JSON.parse(stored.get(result.job_id) ?? '{}') as Record<string, unknown>;
    expect(status.status).toBe('queued');
    expect(status.task_name).toBe('wordcount');
  });

  it('omits an explicit null timeout_sec from the queue message', async () => {
    const { service, sentBodies } = makeService();

    // ValidationPipe의 @IsOptional은 null을 통과시키므로 런타임에는 null이 도달한다
    await service.createJob({ task_name: 'wordcount', timeout_sec: null as unknown as number });

    const message = JSON.parse(sentBodies[0]) as { task: Record<string, unknown> };
    expect(message.task).toEqual({ name: 'wordcount', arguments: [] });
  });

  it('maps queue failure to EnqueueError', async () => {
    const { service } = makeService({ enqueueFails: true });

    await expect(service.createJob({ task_name: 'wordcount' })).rejects.toBeInstanceOf(
      EnqueueError,
    );
  });

  it('throws JobNotFoundError for missing jobs', async () => {
    const { service } = makeService();

    await expect(service.getJob('07519548-6bcf-4a3e-9f2e-1c9c26b40f11')).rejects.toBeInstanceOf(
      JobNotFoundError,
    );
  });

  it('does not enqueue when the initial status write fails', async () => {
    const { service, calls, sentBodies } = makeService({ storeFails: true });

    await expect(service.createJob({ task_name: 'wordcount' })).rejects.toThrow(
      'store unavailable',
    );
    expect(calls).toEqual(['set']);
    expect(sentBodies).toEqual([]);
  });
});
