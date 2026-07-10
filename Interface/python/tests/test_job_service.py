"""JobService 계약 검증: SQS 메시지/Redis 상태 문서가 docs/API.md와 일치해야 한다."""

from __future__ import annotations

import asyncio
import json

import pytest

from app.core.errors import EnqueueError, JobNotFoundError
from app.schemas.jobs import CreateJobRequest, InputFile
from app.services.job_service import JobService


class FakeS3:
    def __init__(self) -> None:
        self.keys: list[str] = []

    @property
    def bucket(self) -> str:
        return "test-bucket"

    def presign_put(self, key: str, ttl_sec: int) -> str:
        self.keys.append(key)
        return f"https://example.com/{key}?ttl={ttl_sec}"


class FakeQueue:
    def __init__(self, fail: bool = False) -> None:
        self.bodies: list[str] = []
        self.groups: list[str] = []
        self._fail = fail

    def enqueue(self, body: str, message_group_id: str) -> None:
        if self._fail:
            raise RuntimeError("sqs down")
        self.bodies.append(body)
        self.groups.append(message_group_id)


class FakeStore:
    def __init__(self) -> None:
        self.values: dict[str, str] = {}

    async def get(self, key: str) -> str | None:
        return self.values.get(key)

    async def set(self, key: str, value: str, ttl_sec: int) -> None:
        self.values[key] = value


def make_service(
    queue: FakeQueue | None = None, store: FakeStore | None = None
) -> tuple[JobService, FakeQueue, FakeStore]:
    queue = queue or FakeQueue()
    store = store or FakeStore()
    service = JobService(
        s3=FakeS3(),
        queue=queue,
        store=store,
        presign_ttl_sec=600,
        status_ttl_sec=3600,
        message_group_id="request",
    )
    return service, queue, store


def test_create_job_builds_contract_message() -> None:
    service, queue, store = make_service()

    request = CreateJobRequest(
        task_name="wordcount",
        arguments=["--verbose"],
        input_files=[InputFile(filename="a.txt"), InputFile(filename="b.txt")],
        timeout_sec=20,
    )
    result = asyncio.run(service.create_job(request))

    assert result.job_id
    assert len(result.upload_urls) == 2
    assert all(upload.method == "PUT" for upload in result.upload_urls)

    assert len(queue.bodies) == 1
    assert queue.groups == ["request"]

    message = json.loads(queue.bodies[0])
    assert message["mode"] == "execute"
    assert message["job_id"] == result.job_id
    assert message["task"] == {"name": "wordcount", "arguments": ["--verbose"], "timeout_sec": 20}
    assert message["output_prefix"] == f"jobs/{result.job_id}/outputs/"
    assert message["download_s3"][0] == {
        "bucket": "test-bucket",
        "key": f"jobs/{result.job_id}/inputs/a.txt",
        "method": "GET",
    }

    status = json.loads(store.values[result.job_id])
    assert status["status"] == "queued"
    assert status["task_name"] == "wordcount"


def test_create_job_queue_failure_raises_domain_error() -> None:
    service, _, _ = make_service(queue=FakeQueue(fail=True))

    with pytest.raises(EnqueueError):
        asyncio.run(service.create_job(CreateJobRequest(task_name="wordcount")))


def test_get_job_not_found() -> None:
    service, _, _ = make_service()

    with pytest.raises(JobNotFoundError):
        asyncio.run(service.get_job("missing"))


def test_filename_path_traversal_rejected() -> None:
    with pytest.raises(ValueError):
        InputFile(filename="../etc/passwd")
    with pytest.raises(ValueError):
        InputFile(filename="a/b.txt")
