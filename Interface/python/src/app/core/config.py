"""환경변수 기반 설정 (12-factor). 필드명은 Go/JS 인터페이스와 동일한 env 키를 사용한다."""

from __future__ import annotations

from functools import lru_cache

from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    model_config = SettingsConfigDict(env_prefix="", case_sensitive=False)

    http_host: str = "0.0.0.0"
    http_port: int = 8081

    aws_region: str = "us-east-1"
    aws_endpoint_url: str = ""

    s3_bucket: str = "taskforge-jobs"
    s3_presign_ttl_sec: int = 600

    sqs_request_queue_url: str = ""
    sqs_message_group_id: str = "request"

    redis_host: str = "127.0.0.1"
    redis_port: int = 6379
    redis_db: int = 0
    redis_password: str = ""
    redis_ttl_sec: int = 3600

    log_level: str = "info"

    def validate_required(self) -> None:
        """필수값이 없으면 부트 시점에 즉시 실패한다."""
        if not self.sqs_request_queue_url:
            raise ValueError("SQS_REQUEST_QUEUE_URL is required")
        if not self.s3_bucket:
            raise ValueError("S3_BUCKET is required")


@lru_cache
def get_settings() -> Settings:
    return Settings()
