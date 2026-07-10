// JobsService 계약 검증: SQS 메시지/Redis 상태 문서가 docs/API.md와 일치해야 한다.
import { ConfigService } from '@nestjs/config';

import { EnqueueError, JobNotFoundError } from '../src/common/domain-errors';
import { RedisService } from '../src/infra/redis.service';
import { S3Service } from '../src/infra/s3.service';
import { SqsService } from '../src/infra/sqs.service';
import { JobsService } from '../src/jobs/jobs.service';

interface FakeInfra {
  s3: S3Service;
  sqs: SqsService;
  redis: RedisService;
  sentBodies: string[];
  sentGroups: string[];
  stored: Map<string, string>;
}

function makeService(options?: { enqueueFails?: boolean }): { service: JobsService } & FakeInfra {
  const sentBodies: string[] = [];
  const sentGroups: string[] = [];
  const stored = new Map<string, string>();

  const s3 = {
    bucket: 'test-bucket',
    presignPut: jest.fn((key: string) => Promise.resolve(`https://example.com/${key}`)),
  } as unknown as S3Service;

  const sqs = {
    enqueue: jest.fn((body: string, group: string) => {
      if (options?.enqueueFails) {
        return Promise.reject(new Error('sqs down'));
      }
      sentBodies.push(body);
      sentGroups.push(group);
      return Promise.resolve();
    }),
  } as unknown as SqsService;

  const redis = {
    get: jest.fn((key: string) => Promise.resolve(stored.get(key) ?? null)),
    set: jest.fn((key: string, value: string) => {
      stored.set(key, value);
      return Promise.resolve();
    }),
  } as unknown as RedisService;

  const config = {
    get: jest.fn((key: string) => {
      const values: Record<string, unknown> = {
        redisTtlSec: 3600,
        sqsMessageGroupId: 'request',
      };
      return values[key];
    }),
  } as unknown as ConfigService<never, true>;

  const service = new JobsService(s3, sqs, redis, config);
  return { service, s3, sqs, redis, sentBodies, sentGroups, stored };
}

describe('JobsService', () => {
  it('builds the contract queue message and queued status', async () => {
    const { service, sentBodies, sentGroups, stored } = makeService();

    const result = await service.createJob({
      task_name: 'wordcount',
      arguments: ['--verbose'],
      input_files: [{ filename: 'a.txt' }, { filename: 'b.txt' }],
      timeout_sec: 20,
    });

    expect(result.job_id).toBeTruthy();
    expect(result.upload_urls).toHaveLength(2);
    expect(result.upload_urls.every((upload) => upload.method === 'PUT')).toBe(true);

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

  it('maps queue failure to EnqueueError', async () => {
    const { service } = makeService({ enqueueFails: true });

    await expect(service.createJob({ task_name: 'wordcount' })).rejects.toBeInstanceOf(
      EnqueueError,
    );
  });

  it('throws JobNotFoundError for missing jobs', async () => {
    const { service } = makeService();

    await expect(service.getJob('missing')).rejects.toBeInstanceOf(JobNotFoundError);
  });
});
