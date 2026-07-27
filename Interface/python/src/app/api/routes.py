"""라우터: DTO 검증 → 서비스 호출 → envelope 매핑만 수행한다."""

from __future__ import annotations

import time
from datetime import UTC, datetime
from typing import Annotated

import anyio
import anyio.to_thread
from fastapi import APIRouter, Path, Request

from app.api.envelope import success
from app.schemas.jobs import CreateJobRequest

router = APIRouter()

_started_at = time.monotonic()


def _service(request: Request):
    return request.app.state.job_service


@router.post("/api/v1/jobs", status_code=202)
async def create_job(request: Request, body: CreateJobRequest):
    result = await _service(request).create_job(body)
    return success(result.model_dump(), status_code=202)


@router.get("/api/v1/jobs/{job_id}")
async def get_job(request: Request, job_id: Annotated[str, Path(min_length=1, max_length=64)]):
    document = await _service(request).get_job(job_id)
    return success(document)


@router.get("/api/v1/tasks")
async def get_tasks(request: Request):
    catalog = await _service(request).get_task_catalog()
    return success(catalog)


@router.get("/healthz")
async def healthz():
    return {
        "status": "ok",
        "uptime_sec": round(time.monotonic() - _started_at, 3),
        "time": datetime.now(UTC).strftime("%Y-%m-%dT%H:%M:%SZ"),
    }


@router.get("/readyz")
async def readyz(request: Request):
    checks: dict[str, str] = {}
    ready = True

    async def check(name: str, probe) -> None:
        nonlocal ready
        try:
            with anyio.fail_after(3):
                await probe()
            checks[name] = "ok"
        except Exception as exc:
            checks[name] = str(exc) or type(exc).__name__
            ready = False

    state = request.app.state
    await check("redis", state.redis_repo.ping)

    # abandon_on_cancel=False(기본값)면 anyio가 스레드 완료까지 취소를 차단하므로
    # 위 fail_after(3)가 boto3 호출이 스스로 끝난 뒤에야 적용된다 (계약 A-4의 3초 상한 무효화)
    async def sqs_ping() -> None:
        await anyio.to_thread.run_sync(state.sqs_repo.ping, abandon_on_cancel=True)

    async def s3_ping() -> None:
        await anyio.to_thread.run_sync(state.s3_repo.ping, abandon_on_cancel=True)

    await check("sqs", sqs_ping)
    await check("s3", s3_ping)

    from fastapi.responses import JSONResponse

    return JSONResponse(
        status_code=200 if ready else 503,
        content={"status": "ready" if ready else "not_ready", "checks": checks},
    )
