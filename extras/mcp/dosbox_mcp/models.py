"""Bridge-side models for the DOSBox debugger surface."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any

REGISTER_NAMES = (
    "eax",
    "ebx",
    "ecx",
    "edx",
    "esi",
    "edi",
    "ebp",
    "esp",
    "eip",
    "flags",
    "cs",
    "ds",
    "es",
    "ss",
    "fs",
    "gs",
)


def parse_int(value: int | str, field_name: str) -> int:
    """Parse decimal or 0x-prefixed integers from JSON fields and URIs."""
    if isinstance(value, int):
        return value
    if isinstance(value, str):
        try:
            return int(value, 0)
        except ValueError as exc:  # pragma: no cover - exercised through tool calls
            raise ValueError(f"{field_name} must be an integer or 0x-prefixed string") from exc
    raise TypeError(f"{field_name} must be an integer or string")


def parse_registers(payload: dict[str, Any]) -> dict[str, int]:
    """Normalize the flat register snapshot shared by the HTTP API surfaces."""
    return {name: int(payload[name]) for name in REGISTER_NAMES}


class DebugStatus:
    """Wrap one normalized debugger status payload."""

    def __init__(self, payload: dict[str, Any]) -> None:
        self._payload = payload

    @classmethod
    def from_dict(cls, payload: dict[str, Any]) -> "DebugStatus":
        """Create one debugger status object from the HTTP JSON payload."""
        return cls(
            {
                "running": payload["running"],
                "paused": payload["paused"],
                "pauseMode": payload["pauseMode"],
                "stopSequence": payload["stopSequence"],
                "reason": payload["reason"],
                "description": payload["description"],
                "registers": parse_registers(payload["registers"]),
            }
        )

    @property
    def stop_sequence(self) -> int:
        """Return the current debugger stop sequence."""
        return int(self._payload["stopSequence"])

    @property
    def paused(self) -> bool:
        """Report whether DOSBox is currently paused."""
        return bool(self._payload["paused"])

    def to_dict(self) -> dict[str, Any]:
        """Return the normalized JSON payload used by the MCP surface."""
        return dict(self._payload)


@dataclass(slots=True)
class MemoryReadResult:
    """Represent one memory-read response."""

    address: int
    data_base64: str

    @classmethod
    def from_dict(cls, payload: dict[str, Any]) -> "MemoryReadResult":
        """Create one memory-read result from the HTTP JSON payload."""
        return cls(
            address=payload["memory"]["addr"],
            data_base64=payload["memory"]["data"],
        )

    def to_dict(self) -> dict[str, Any]:
        """Return the MCP-facing dictionary for this memory snapshot."""
        return {
            "address": self.address,
            "dataBase64": self.data_base64,
        }
