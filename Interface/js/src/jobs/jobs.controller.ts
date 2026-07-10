import { Body, Controller, Get, HttpCode, Param, Post } from '@nestjs/common';

import { Envelope, successEnvelope } from '../common/envelope';
import { CreateJobDto } from './dto/create-job.dto';
import { CreateJobResult, JobsService } from './jobs.service';

// 컨트롤러는 얇게: DTO 검증 → 서비스 호출 → envelope 매핑만 수행한다.
@Controller('api/v1')
export class JobsController {
  constructor(private readonly jobs: JobsService) {}

  @Post('jobs')
  @HttpCode(202)
  async create(@Body() dto: CreateJobDto): Promise<Envelope<CreateJobResult>> {
    const result = await this.jobs.createJob(dto);
    return successEnvelope(result);
  }

  @Get('jobs/:job_id')
  async get(@Param('job_id') jobId: string): Promise<Envelope<unknown>> {
    const document = await this.jobs.getJob(jobId);
    return successEnvelope(document);
  }

  @Get('tasks')
  async tasks(): Promise<Envelope<unknown>> {
    const catalog = await this.jobs.getTaskCatalog();
    return successEnvelope(catalog);
  }
}
