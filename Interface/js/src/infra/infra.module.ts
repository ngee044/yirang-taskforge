import { Module } from '@nestjs/common';

import { RedisService } from './redis.service';
import { S3Service } from './s3.service';
import { SqsService } from './sqs.service';

@Module({
  providers: [S3Service, SqsService, RedisService],
  exports: [S3Service, SqsService, RedisService],
})
export class InfraModule {}
