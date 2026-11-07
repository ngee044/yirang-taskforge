"""라우터: DTO 검증 → 서비스 호출 → envelope 매핑만 수행한다."""

from __future__ import annotations

import logging
import time
from collections.abc import Awaitable, Callable
from datetime import UTC, datetime
from typing import Annotated

import anyio
import anyio.to_thread
from fastapi import APIRouter, Path, Request
from fastapi.responses import JSONResponse

from app.api.envelope import success
from app.schemas.jobs import CreateJobRequest
from app.services.job_service import JobService

router = APIRouter()
logger = logging.getLogger("taskforge")

_started_at = time.monotonic()
_READINESS_TIMEOUT_SEC = 3.0

# 계약 A-1이 규정한 UUID 형식
_JOB_ID_PATTERN = r"^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$"


def _service(request: Request) -> JobService:
    return request.app.state.job_service


@router.post("/api/v1/jobs", status_code=202)
async def create_job(request: Request, body: CreateJobRequest) -> JSONResponse:
    result = await _service(request).create_job(body)
    return success(result.model_dump(), status_code=202)


@router.get("/api/v1/jobs/{job_id}")
async def get_job(
    request: Request,
    # 상태 문서의 Redis 키가 job_id이므로 형식을 검증하지 않으면 task_catalog 등
    # 임의 키를 job 문서로 조회할 수 있다. 문자 클래스 검증으로는 task_catalog가 통과하므로
    # UUID 형식을 요구한다 (계약 A-2)
    job_id: Annotated[str, Path(pattern=_JOB_ID_PATTERN)],
) -> JSONResponse:
    document = await _service(request).get_job(job_id)
    return success(document)


@router.get("/api/v1/tasks")
async def get_tasks(request: Request) -> JSONResponse:
    catalog = await _service(request).get_task_catalog()
    return success(catalog)


@router.get("/healthz")
async def healthz() -> dict[str, str | float]:
    return {
        "status": "ok",
        "uptime_sec": round(time.monotonic() - _started_at, 3),
        "time": datetime.now(UTC).strftime("%Y-%m-%dT%H:%M:%SZ"),
    }


@router.get("/readyz")
async def readyz(request: Request) -> JSONResponse:
    checks: dict[str, str] = {}

    async def check(name: str, probe: Callable[[], Awaitable[None]]) -> None:
        try:
            with anyio.fail_after(_READINESS_TIMEOUT_SEC):
                await probe()
            checks[name] = "ok"
        except Exception as exc:
            logger.warning("readiness check %s failed: %r", name, exc)
            checks[name] = "unavailable"

    state = request.app.state

    # abandon_on_cancel=False(기본값)면 anyio가 스레드 완료까지 취소를 차단하므로
    # 위 fail_after(3)가 boto3 호출이 스스로 끝난 뒤에야 적용된다 (계약 A-4의 3초 상한 무효화)
    async def sqs_ping() -> None:
        await anyio.to_thread.run_sync(state.sqs_repo.ping, abandon_on_cancel=True)

    async def s3_ping() -> None:
        await anyio.to_thread.run_sync(state.s3_repo.ping, abandon_on_cancel=True)

    async with anyio.create_task_group() as tasks:
        tasks.start_soon(check, "redis", state.redis_repo.ping)
        tasks.start_soon(check, "sqs", sqs_ping)
        tasks.start_soon(check, "s3", s3_ping)

    ready = all(value == "ok" for value in checks.values())
    return JSONResponse(
        status_code=200 if ready else 503,
        content={"status": "ready" if ready else "not_ready", "checks": checks},
    )
