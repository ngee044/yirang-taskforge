import 'reflect-metadata';

import { Logger, ValidationPipe } from '@nestjs/common';
import { NestFactory } from '@nestjs/core';

import { AppModule } from './app.module';
import { DomainExceptionFilter } from './common/domain-exception.filter';
import { loadConfiguration, validateRequired } from './config/configuration';

async function bootstrap(): Promise<void> {
  const config = loadConfiguration();
  validateRequired(config);

  const app = await NestFactory.create(AppModule);

  app.useGlobalPipes(
    new ValidationPipe({
      whitelist: true,
      forbidNonWhitelisted: true,
      transform: true,
    }),
  );
  app.useGlobalFilters(new DomainExceptionFilter());
  app.enableShutdownHooks();

  await app.listen(config.httpPort);
  Logger.log(`taskforge js interface listening on :${config.httpPort}`, 'Bootstrap');
}

bootstrap().catch((error: unknown) => {
  Logger.error(`bootstrap failed: ${String(error)}`, undefined, 'Bootstrap');
  process.exit(1);
});
