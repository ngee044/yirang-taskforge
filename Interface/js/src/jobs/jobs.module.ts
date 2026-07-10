import { Module } from '@nestjs/common';

import { InfraModule } from '../infra/infra.module';
import { JobsController } from './jobs.controller';
import { JobsService } from './jobs.service';

@Module({
  imports: [InfraModule],
  controllers: [JobsController],
  providers: [JobsService],
})
export class JobsModule {}
