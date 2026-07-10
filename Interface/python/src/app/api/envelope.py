"""세 인터페이스가 공유하는 응답 envelope {success, data, error}."""

from __future__ import annotations

from typing import Any

from fastapi.responses import JSONResponse


def success(data: Any, status_code: int = 200) -> JSONResponse:
    return JSONResponse(status_code=status_code, content={"success": True, "data": data})


def failure(code: str, message: str, status_code: int) -> JSONResponse:
    return JSONResponse(
        status_code=status_code,
        content={"success": False, "error": {"code": code, "message": message}},
    )
