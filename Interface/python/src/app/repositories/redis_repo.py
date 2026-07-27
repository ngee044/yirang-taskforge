"""Redis 상태 조회/기록 어댑터 (redis.asyncio, 네이티브 비동기)."""

from __future__ import annotations

import asyncio

import redis.asyncio as redis

# socket_timeout만으로는 유계가 보장되지 않는다. 연결은 수립되나 응답하지 않는 서버에서는
# redis-py의 내부 재시도가 겹쳐 설정값의 수 배까지 지연되는 것이 실측되었으므로,
# 호출 단위 데드라인을 표준 라이브러리로 강제한다 (NFR-REL-02).
_OPERATION_TIMEOUT_SEC = 2.0


class RedisRepository:
    def __init__(self, host: str, port: int, db: int, password: str) -> None:
        self._client = redis.Redis(
            host=host,
            port=port,
            db=db,
            password=password or None,
            decode_responses=True,
            socket_connect_timeout=2,
            socket_timeout=2,
        )

    async def get(self, key: str) -> str | None:
        async with asyncio.timeout(_OPERATION_TIMEOUT_SEC):
            return await self._client.get(key)

    async def set(self, key: str, value: str, ttl_sec: int) -> None:
        async with asyncio.timeout(_OPERATION_TIMEOUT_SEC):
            await self._client.set(key, value, ex=ttl_sec if ttl_sec > 0 else None)

    async def ping(self) -> None:
        async with asyncio.timeout(_OPERATION_TIMEOUT_SEC):
            await self._client.ping()

    async def close(self) -> None:
        await self._client.aclose()
