// 도메인 예외 계층. HTTP 변환은 예외 필터(경계)에서만 수행한다.
export class DomainError extends Error {
  constructor(
    readonly code: string,
    message: string,
  ) {
    super(message);
    this.name = new.target.name;
  }
}

export class JobNotFoundError extends DomainError {
  constructor(jobId: string) {
    super('job_not_found', `job not found: ${jobId}`);
  }
}

export class CatalogNotFoundError extends DomainError {
  constructor() {
    super('catalog_not_found', 'task catalog is not published yet (worker not started?)');
  }
}

export class EnqueueError extends DomainError {
  constructor(message: string) {
    super('create_failed', message);
  }
}
