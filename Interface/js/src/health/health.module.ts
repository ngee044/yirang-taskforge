import { Module } from '@nestjs/common';

import { InfraModule } from '../infra/infra.module';
import { HealthController } from './health.controller';

@Module({
  imports: [InfraModule],
  controllers: [HealthController],
})
export class HealthModule {}
