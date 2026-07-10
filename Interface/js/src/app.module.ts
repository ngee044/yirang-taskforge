import { Module } from '@nestjs/common';
import { ConfigModule } from '@nestjs/config';

import { loadConfiguration } from './config/configuration';
import { HealthModule } from './health/health.module';
import { InfraModule } from './infra/infra.module';
import { JobsModule } from './jobs/jobs.module';

@Module({
  imports: [
    ConfigModule.forRoot({
      isGlobal: true,
      load: [loadConfiguration],
    }),
    InfraModule,
    JobsModule,
    HealthModule,
  ],
})
export class AppModule {}
