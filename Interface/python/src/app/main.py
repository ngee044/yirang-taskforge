"""yirang-taskforge Python 인터페이스 (FastAPI) 부트스트랩."""

from __future__ import annotations

import logging
from contextlib import asynccontextmanager

from fastapi import FastAPI, Request
from fastapi.exceptions import RequestValidationError
from starlette.exceptions import HTTPException as StarletteHTTPException

from app.api.envelope import failure
from app.api.routes import router
from app.core.config import get_settings
from app.core.errors import AppError, CatalogNotFoundError, EnqueueError, JobNotFoundError
from app.repositories.redis_repo import RedisRepository
from app.repositories.s3_repo import S3Repository
from app.repositories.sqs_repo import SQSRepository
from app.services.job_service import JobService

logger = logging.getLogger("taskforge")


@asynccontextmanager
async def lifespan(app: FastAPI):
    settings = get_settings()
    settings.validate_required()

    logging.basicConfig(level=settings.log_level.upper())

    app.state.s3_repo = S3Repository(settings.aws_region, settings.aws_endpoint_url, settings.s3_bucket)
    app.state.sqs_repo = SQSRepository(settings.aws_region, settings.aws_endpoint_url, settings.sqs_request_queue_url)
    app.state.redis_repo = RedisRepository(
        settings.redis_host, settings.redis_port, settings.redis_db, settings.redis_password
    )
    app.state.job_service = JobService(
        s3=app.state.s3_repo,
        queue=app.state.sqs_repo,
        store=app.state.redis_repo,
        presign_ttl_sec=settings.s3_presign_ttl_sec,
        status_ttl_sec=settings.redis_ttl_sec,
        message_group_id=settings.sqs_message_group_id,
    )

    logger.info("taskforge python interface started")
    yield

    await app.state.redis_repo.close()


def create_app() -> FastAPI:
    app = FastAPI(title="yirang-taskforge interface (python)", lifespan=lifespan)
    app.include_router(router)

    # 요청 검증 실패는 FastAPI 기본 {"detail": [...]} 대신 계약 A 공통 envelope로 응답한다.
    # 상태코드 422는 계약 A-1이 Python 구현에 허용한 값이므로 유지하고, error.code만 정합화한다
    @app.exception_handler(RequestValidationError)
    async def validation_error(_: Request, exc: RequestValidationError):
        code = "invalid_request"
        message = "invalid request"
        for error in exc.errors():
            location = error.get("loc", ())
            if "filename" in location:
                code = "invalid_filename"
                message = "input_files[].filename is invalid"
                break

        return failure(code, message, status_code=422)

    # 라우팅·메서드 오류(404/405)도 envelope를 유지한다
    @app.exception_handler(StarletteHTTPException)
    async def http_error(_: Request, exc: StarletteHTTPException):
        code = "not_found" if exc.status_code == 404 else "http_error"

        return failure(code, str(exc.detail), status_code=exc.status_code)

    # 도메인 예외 → HTTP 변환은 이 경계에서만 수행한다
    @app.exception_handler(JobNotFoundError)
    async def job_not_found(_: Request, exc: JobNotFoundError):
        return failure(exc.code, exc.message, status_code=404)

    @app.exception_handler(CatalogNotFoundError)
    async def catalog_not_found(_: Request, exc: CatalogNotFoundError):
        return failure(exc.code, exc.message, status_code=404)

    @app.exception_handler(EnqueueError)
    async def enqueue_failed(_: Request, exc: EnqueueError):
        logger.error("enqueue failed: %s", exc.message)
        return failure(exc.code, "cannot enqueue job", status_code=500)

    @app.exception_handler(AppError)
    async def app_error(_: Request, exc: AppError):
        logger.error("unhandled app error: %s", exc.message)
        return failure(exc.code, "internal error", status_code=500)

    # 인프라 예외(Redis/boto3 등)도 스택을 노출하지 않고 envelope로 응답한다
    @app.exception_handler(Exception)
    async def unhandled_error(_: Request, exc: Exception):
        logger.exception("unhandled error: %s", exc)
        return failure("internal_error", "internal error", status_code=500)

    return app


app = create_app()


if __name__ == "__main__":
    import uvicorn

    settings = get_settings()
    uvicorn.run("app.main:app", host=settings.http_host, port=settings.http_port, log_level=settings.log_level)
