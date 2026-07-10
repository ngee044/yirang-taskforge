"""도메인 예외 계층. HTTP 변환은 API 계층(main.py의 exception handler)에서만 수행한다."""

from __future__ import annotations


class AppError(Exception):
    """서비스 계층 공통 도메인 예외."""

    code = "app_error"

    def __init__(self, message: str) -> None:
        super().__init__(message)
        self.message = message


class JobNotFoundError(AppError):
    code = "job_not_found"


class CatalogNotFoundError(AppError):
    code = "catalog_not_found"


class EnqueueError(AppError):
    code = "create_failed"
