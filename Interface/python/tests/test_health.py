"""Readiness의 전체 시간 예산과 공개 오류 응답을 검증합니다."""

from __future__ import annotations

import json
import threading
from collections.abc import Awaitable, Callable
from types import SimpleNamespace

import anyio
import pytest
from fastapi import FastAPI, Request
from fastapi.responses import JSONResponse

from app.api import routes


def make_request(
    redis_ping: Callable[[], Awaitable[None]],
    sqs_ping: Callable[[], None],
    s3_ping: Callable[[], None],
) -> Request:
    app = FastAPI()
    app.state.redis_repo = SimpleNamespace(ping=redis_ping)
    app.state.sqs_repo = SimpleNamespace(ping=sqs_ping)
    app.state.s3_repo = SimpleNamespace(ping=s3_ping)
    return Request({"type": "http", "app": app})


def test_readyz_healthy() -> None:
    async def redis_ping() -> None:
        return None

    def sync_ping() -> None:
        return None

    response = anyio.run(routes.readyz, make_request(redis_ping, sync_ping, sync_ping))

    assert response.status_code == 200
    assert json.loads(response.body) == {
        "status": "ready",
        "checks": {"redis": "ok", "sqs": "ok", "s3": "ok"},
    }


def test_readyz_hides_dependency_errors(caplog: pytest.LogCaptureFixture) -> None:
    async def redis_ping() -> None:
        raise RuntimeError("redis.internal:6379")

    def sync_ping() -> None:
        raise RuntimeError("private-bucket.internal")

    response = anyio.run(routes.readyz, make_request(redis_ping, sync_ping, sync_ping))

    assert response.status_code == 503
    assert json.loads(response.body) == {
        "status": "not_ready",
        "checks": {"redis": "unavailable", "sqs": "unavailable", "s3": "unavailable"},
    }
    assert "redis.internal:6379" in caplog.text
    assert "private-bucket.internal" in caplog.text


def test_readyz_starts_all_probes_before_waiting_for_completion() -> None:
    async def verify() -> None:
        started = {name: threading.Event() for name in ("redis", "sqs", "s3")}
        release_sync = threading.Event()
        release_async = anyio.Event()
        responses: list[JSONResponse] = []

        async def redis_ping() -> None:
            started["redis"].set()
            await release_async.wait()

        def sqs_ping() -> None:
            started["sqs"].set()
            release_sync.wait()

        def s3_ping() -> None:
            started["s3"].set()
            release_sync.wait()

        async def request_readyz() -> None:
            responses.append(await routes.readyz(make_request(redis_ping, sqs_ping, s3_ping)))

        async with anyio.create_task_group() as tasks:
            tasks.start_soon(request_readyz)
            try:
                with anyio.fail_after(1):
                    while not all(event.is_set() for event in started.values()):
                        await anyio.sleep(0.001)
                assert not responses
            finally:
                release_async.set()
                release_sync.set()

        assert responses[0].status_code == 200

    anyio.run(verify)


def test_readyz_times_out_stalled_probes(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(routes, "_READINESS_TIMEOUT_SEC", 0)

    async def redis_ping() -> None:
        await anyio.sleep_forever()

    def sync_ping() -> None:
        pytest.fail("an expired deadline must not start a synchronous probe")

    response = anyio.run(routes.readyz, make_request(redis_ping, sync_ping, sync_ping))

    assert response.status_code == 503
    assert json.loads(response.body) == {
        "status": "not_ready",
        "checks": {"redis": "unavailable", "sqs": "unavailable", "s3": "unavailable"},
    }
