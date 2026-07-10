import { Injectable, Logger } from '@nestjs/common';
import { ConfigService } from '@nestjs/config';
import { v4 as uuidv4 } from 'uuid';

import { CatalogNotFoundError, EnqueueError, JobNotFoundError } from '../common/domain-errors';
import { AppConfig } from '../config/configuration';
import { RedisService } from '../infra/redis.service';
import { S3Service } from '../infra/s3.service';
import { SqsService } from '../infra/sqs.service';
import { CreateJobDto } from './dto/create-job.dto';

export interface UploadUrl {
  filename: string;
  upload_url: string;
  method: 'PUT';
}

export interface CreateJobResult {
  job_id: string;
  upload_urls: UploadUrl[];
}

// 계약 #1: SQS 요청 메시지 (docs/API.md)
interface QueueMessage {
  job_id: string;
  mode: 'execute';
  task: {
    name: string;
    arguments: string[];
    timeout_sec?: number;
  };
  download_s3: Array<{ bucket: string; key: string; method: 'GET' }>;
  output_prefix: string;
  enqueued_at: string;
}

@Injectable()
export class JobsService {
  private readonly logger = new Logger(JobsService.name);
  private readonly statusTtlSec: number;
  private readonly messageGroupId: string;

  constructor(
    private readonly s3: S3Service,
    private readonly sqs: SqsService,
    private readonly redis: RedisService,
    config: ConfigService<AppConfig, true>,
  ) {
    this.statusTtlSec = config.get('redisTtlSec', { infer: true });
    this.messageGroupId = config.get('sqsMessageGroupId', { infer: true });
  }

  async createJob(dto: CreateJobDto): Promise<CreateJobResult> {
    const jobId = uuidv4();
    const now = utcNow();

    const inputFiles = dto.input_files ?? [];
    const uploadUrls: UploadUrl[] = [];
    const downloads: QueueMessage['download_s3'] = [];
    for (const inputFile of inputFiles) {
      const key = `jobs/${jobId}/inputs/${inputFile.filename}`;
      const url = await this.s3.presignPut(key);
      uploadUrls.push({ filename: inputFile.filename, upload_url: url, method: 'PUT' });
      downloads.push({ bucket: this.s3.bucket, key, method: 'GET' });
    }

    // 계약 #2: queued 상태를 SQS 발행 이전에 기록해 워커의 running 갱신을 덮어쓰지 않게 한다
    await this.redis.set(
      jobId,
      JSON.stringify({
        job_id: jobId,
        task_name: dto.task_name,
        status: 'queued',
        enqueued_at: now,
        updated_at: now,
      }),
      this.statusTtlSec,
    );

    const message: QueueMessage = {
      job_id: jobId,
      mode: 'execute',
      task: {
        name: dto.task_name,
        arguments: dto.arguments ?? [],
        ...(dto.timeout_sec !== undefined ? { timeout_sec: dto.timeout_sec } : {}),
      },
      download_s3: downloads,
      output_prefix: `jobs/${jobId}/outputs/`,
      enqueued_at: now,
    };

    try {
      await this.sqs.enqueue(JSON.stringify(message), this.messageGroupId);
    } catch (error) {
      throw new EnqueueError(`cannot enqueue job: ${String(error)}`);
    }

    this.logger.log(`job enqueued: ${jobId} task=${dto.task_name} inputs=${inputFiles.length}`);

    return { job_id: jobId, upload_urls: uploadUrls };
  }

  async getJob(jobId: string): Promise<unknown> {
    const value = await this.redis.get(jobId);
    if (value === null) {
      throw new JobNotFoundError(jobId);
    }
    return JSON.parse(value) as unknown;
  }

  async getTaskCatalog(): Promise<unknown> {
    const value = await this.redis.get('task_catalog');
    if (value === null) {
      throw new CatalogNotFoundError();
    }
    return JSON.parse(value) as unknown;
  }
}

function utcNow(): string {
  return new Date().toISOString().replace(/\.\d{3}Z$/, 'Z');
}
