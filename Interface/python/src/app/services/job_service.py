"""작업 등록/조회 비즈니스 로직. 인프라 어댑터는 Protocol로 좁게 정의해 주입받는다."""

from __future__ import annotations

import json
import uuid
from datetime import UTC, datetime
from typing import Protocol

import anyio.to_thread

from app.core.errors import CatalogNotFoundError, EnqueueError, JobNotFoundError
from app.schemas.jobs import CreateJobRequest, CreateJobResponse, UploadURL


class S3Presigner(Protocol):
    @property
    def bucket(self) -> str: ...

    def presign_put(self, key: str, ttl_sec: int) -> str: ...


class QueueProducer(Protocol):
    def enqueue(self, body: str, message_group_id: str) -> None: ...


class StatusStore(Protocol):
    async def get(self, key: str) -> str | None: ...

    async def set(self, key: str, value: str, ttl_sec: int) -> None: ...


def _utc_now() -> str:
    return datetime.now(UTC).strftime("%Y-%m-%dT%H:%M:%SZ")


class JobService:
    def __init__(
        self,
        s3: S3Presigner,
        queue: QueueProducer,
        store: StatusStore,
        presign_ttl_sec: int,
        status_ttl_sec: int,
        message_group_id: str,
    ) -> None:
        self._s3 = s3
        self._queue = queue
        self._store = store
        self._presign_ttl_sec = presign_ttl_sec
        self._status_ttl_sec = status_ttl_sec
        self._message_group_id = message_group_id

    async def create_job(self, request: CreateJobRequest) -> CreateJobResponse:
        job_id = str(uuid.uuid4())
        now = _utc_now()

        upload_urls: list[UploadURL] = []
        downloads: list[dict[str, str]] = []
        for input_file in request.input_files:
            key = f"jobs/{job_id}/inputs/{input_file.filename}"
            # boto3는 동기 SDK이므로 이벤트 루프를 막지 않도록 스레드로 위임한다
            url = await anyio.to_thread.run_sync(self._s3.presign_put, key, self._presign_ttl_sec)
            upload_urls.append(UploadURL(filename=input_file.filename, upload_url=url, method="PUT"))
            downloads.append({"bucket": self._s3.bucket, "key": key, "method": "GET"})

        # 계약 #2: queued 상태를 SQS 발행 이전에 기록해 워커의 running 갱신을 덮어쓰지 않게 한다
        status_document = json.dumps(
            {
                "job_id": job_id,
                "task_name": request.task_name,
                "status": "queued",
                "enqueued_at": now,
                "updated_at": now,
            }
        )
        await self._store.set(job_id, status_document, self._status_ttl_sec)

        # 계약 #1: SQS 요청 메시지 (docs/API.md)
        task: dict[str, object] = {"name": request.task_name, "arguments": request.arguments}
        if request.timeout_sec is not None:
            task["timeout_sec"] = request.timeout_sec

        message = json.dumps(
            {
                "job_id": job_id,
                "mode": "execute",
                "task": task,
                "download_s3": downloads,
                "output_prefix": f"jobs/{job_id}/outputs/",
                "enqueued_at": now,
            }
        )

        try:
            await anyio.to_thread.run_sync(self._queue.enqueue, message, self._message_group_id)
        except Exception as exc:
            raise EnqueueError(f"cannot enqueue job: {exc}") from exc

        return CreateJobResponse(job_id=job_id, upload_urls=upload_urls)

    async def get_job(self, job_id: str) -> dict[str, object]:
        value = await self._store.get(job_id)
        if value is None:
            raise JobNotFoundError(f"job not found: {job_id}")
        return json.loads(value)

    async def get_task_catalog(self) -> dict[str, object]:
        value = await self._store.get("task_catalog")
        if value is None:
            raise CatalogNotFoundError("task catalog is not published yet (worker not started?)")
        return json.loads(value)
