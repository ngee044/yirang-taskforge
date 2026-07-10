"""Redis 상태 조회/기록 어댑터 (redis.asyncio, 네이티브 비동기)."""

from __future__ import annotations

import redis.asyncio as redis


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
        return await self._client.get(key)

    async def set(self, key: str, value: str, ttl_sec: int) -> None:
        await self._client.set(key, value, ex=ttl_sec if ttl_sec > 0 else None)

    async def ping(self) -> None:
        await self._client.ping()

    async def close(self) -> None:
        await self._client.aclose()
