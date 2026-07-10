"""요청/응답 DTO (Pydantic v2). 계약 필드명은 docs/API.md의 snake_case를 그대로 따른다."""

from __future__ import annotations

from pydantic import BaseModel, Field, field_validator


class InputFile(BaseModel):
    filename: str = Field(min_length=1, max_length=255)

    @field_validator("filename")
    @classmethod
    def reject_path_traversal(cls, value: str) -> str:
        if "/" in value or "\\" in value or ".." in value:
            raise ValueError("filename must not contain path separators")
        return value


class CreateJobRequest(BaseModel):
    task_name: str = Field(min_length=1, max_length=128)
    arguments: list[str] = Field(default_factory=list)
    input_files: list[InputFile] = Field(default_factory=list)
    timeout_sec: int | None = Field(default=None, ge=1, le=3600)


class UploadURL(BaseModel):
    filename: str
    upload_url: str
    method: str = "PUT"


class CreateJobResponse(BaseModel):
    job_id: str
    upload_urls: list[UploadURL]
