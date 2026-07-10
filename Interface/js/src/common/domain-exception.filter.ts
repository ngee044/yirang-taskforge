import { ArgumentsHost, Catch, ExceptionFilter, HttpException, Logger } from '@nestjs/common';
import { Response } from 'express';

import { errorEnvelope } from './envelope';
import { CatalogNotFoundError, DomainError, JobNotFoundError } from './domain-errors';

// 도메인/인프라 예외 → HTTP envelope 변환 경계.
// 스택 트레이스 등 내부 정보는 응답에 노출하지 않는다.
@Catch()
export class DomainExceptionFilter implements ExceptionFilter {
  private readonly logger = new Logger(DomainExceptionFilter.name);

  catch(exception: unknown, host: ArgumentsHost): void {
    const response = host.switchToHttp().getResponse<Response>();

    if (exception instanceof JobNotFoundError || exception instanceof CatalogNotFoundError) {
      response.status(404).json(errorEnvelope(exception.code, exception.message));
      return;
    }

    if (exception instanceof DomainError) {
      this.logger.error(`domain error: ${exception.code} ${exception.message}`);
      response.status(500).json(errorEnvelope(exception.code, 'internal error'));
      return;
    }

    if (exception instanceof HttpException) {
      const status = exception.getStatus();
      const body = exception.getResponse();
      const message =
        typeof body === 'object' && body !== null && 'message' in body
          ? String(body.message)
          : exception.message;
      const code = status === 400 ? 'invalid_request' : status === 404 ? 'not_found' : 'http_error';
      response.status(status).json(errorEnvelope(code, message));
      return;
    }

    this.logger.error(`unhandled error: ${String(exception)}`);
    response.status(500).json(errorEnvelope('internal_error', 'internal error'));
  }
}
