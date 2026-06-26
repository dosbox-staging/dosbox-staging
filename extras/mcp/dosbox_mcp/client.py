"""HTTP client for the DOSBox Staging debugger API."""

from __future__ import annotations

from typing import Any
import json

import httpx

from .models import DebugStatus
from .models import MemoryReadResult


class DosboxHttpError(RuntimeError):
    """Raised when DOSBox returns an HTTP or transport error."""

    def __init__(self, message: str, status_code: int | None = None) -> None:
        super().__init__(message)
        self.status_code = status_code


class DosboxHttpClient:
    """Thin async client around the DOSBox webserver debugger surface."""

    def __init__(self, base_url: str, timeout_seconds: float = 10.0) -> None:
        self._client = httpx.AsyncClient(
            base_url=base_url.rstrip("/"),
            headers={"Accept": "application/json"},
            timeout=httpx.Timeout(timeout_seconds, read=timeout_seconds),
        )

    async def aclose(self) -> None:
        """Close the underlying HTTP session."""
        await self._client.aclose()

    async def _request_json(
        self,
        method: str,
        path: str,
        *,
        json_body: dict[str, Any] | None = None,
        params: dict[str, Any] | None = None,
        headers: dict[str, str] | None = None,
    ) -> dict[str, Any]:
        """Send one HTTP request and normalize JSON and DOSBox error payloads."""
        try:
            response = await self._client.request(
                method,
                path,
                json=json_body,
                params=params,
                headers=headers,
            )
        except httpx.HTTPError as exc:  # pragma: no cover - depends on environment
            raise DosboxHttpError(str(exc)) from exc

        if response.is_success:
            if not response.content:
                return {}
            try:
                return response.json()
            except json.JSONDecodeError as exc:
                raise DosboxHttpError(
                    f"Expected JSON from {path}, but the response body was invalid"
                ) from exc

        message = response.text
        try:
            payload = response.json()
        except json.JSONDecodeError:
            payload = None
        if isinstance(payload, dict) and "error" in payload:
            message = str(payload["error"])
        raise DosboxHttpError(message, response.status_code)

    async def get_status(self) -> DebugStatus:
        """Fetch the current debugger run/stop state."""
        payload = await self._request_json("GET", "/api/v1/debug/status")
        try:
            return DebugStatus.from_dict(payload)
        except (KeyError, TypeError, ValueError) as exc:
            raise DosboxHttpError("Debugger status payload was malformed") from exc

    async def get_registers(self) -> dict[str, Any]:
        """Fetch the current register snapshot."""
        payload = await self._request_json("GET", "/api/v1/debug/snapshot/registers")
        return payload["registers"]

    async def pause(self, mode: str = "headless") -> dict[str, Any]:
        """Request a debugger pause at the next safe boundary."""
        return await self._request_json(
            "POST",
            "/api/v1/debug/control/pause",
            json_body={"mode": mode},
        )

    async def continue_execution(self) -> dict[str, Any]:
        """Resume execution from a paused debugger state."""
        return await self._request_json(
            "POST",
            "/api/v1/debug/control/continue",
        )

    async def step(self, mode: str) -> dict[str, Any]:
        """Single-step while the debugger is paused."""
        return await self._request_json(
            "POST",
            "/api/v1/debug/control/step",
            json_body={"mode": mode},
        )

    async def get_dos_internals(self) -> dict[str, Any]:
        """Fetch DOS internal pointers and structures."""
        return await self._request_json("GET", "/api/v1/dos/internals")

    async def read_memory_segmented(
        self,
        segment: int,
        offset: int,
        length: int,
    ) -> MemoryReadResult:
        """Read memory through the segmented memory HTTP endpoint."""
        payload = await self._request_json(
            "GET",
            f"/api/v1/memory/{segment}/{offset}/{length}",
            headers={"Accept": "application/json"},
        )
        return MemoryReadResult.from_dict(payload)

    async def read_memory_physical(
        self,
        address: int,
        length: int,
    ) -> MemoryReadResult:
        """Read memory through the physical memory HTTP endpoint."""
        payload = await self._request_json(
            "GET",
            f"/api/v1/memory/{address}/{length}",
            headers={"Accept": "application/json"},
        )
        return MemoryReadResult.from_dict(payload)
