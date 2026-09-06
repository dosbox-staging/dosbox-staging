"""MCP resource definitions and URI dispatch for DOSBox sessions."""

from __future__ import annotations

from collections.abc import Awaitable
from collections.abc import Callable
from typing import Any
import json
from urllib.parse import urlparse

from mcp import types

from .client import JSON_MIME_TYPE
from .client import DosboxHttpClient
from .models import MemoryReadResult
from .models import parse_int

SESSION_STATUS_URI = "dosbox://session/status"
SESSION_REGISTERS_URI = "dosbox://session/registers"
SESSION_DOS_INTERNALS_URI = "dosbox://session/dos-internals"
SEGMENTED_MEMORY_TEMPLATE_URI = "dosbox://session/memory/segmented/{segment}/{offset}/{len}"
PHYSICAL_MEMORY_TEMPLATE_URI = "dosbox://session/memory/physical/{addr}/{len}"
ResourceTextReader = Callable[[DosboxHttpClient], Awaitable[str]]


def _to_json_text(payload: Any) -> str:
    """Serialize one resource payload as compact stable JSON text."""
    return json.dumps(payload, separators=(",", ":"), sort_keys=True)


def list_static_resources() -> list[types.Resource]:
    """List the always-addressable MCP resources for one DOSBox session."""
    return [
        types.Resource(
            uri=SESSION_STATUS_URI,
            name="DOSBox Status",
            description="Current debugger stop state and registers.",
            mimeType=JSON_MIME_TYPE,
        ),
        types.Resource(
            uri=SESSION_REGISTERS_URI,
            name="Registers",
            description="Current CPU register snapshot.",
            mimeType=JSON_MIME_TYPE,
        ),
        types.Resource(
            uri=SESSION_DOS_INTERNALS_URI,
            name="DOS Internals",
            description="List-of-lists, swappable area, and first shell pointers.",
            mimeType=JSON_MIME_TYPE,
        ),
    ]


def list_resource_templates() -> list[types.ResourceTemplate]:
    """List parameterized MCP resource templates for one DOSBox session."""
    return [
        types.ResourceTemplate(
            uriTemplate=SEGMENTED_MEMORY_TEMPLATE_URI,
            name="Segmented Memory",
            description="Base64 memory snapshot by segment:offset.",
            mimeType=JSON_MIME_TYPE,
        ),
        types.ResourceTemplate(
            uriTemplate=PHYSICAL_MEMORY_TEMPLATE_URI,
            name="Physical Memory",
            description="Base64 memory snapshot by physical address.",
            mimeType=JSON_MIME_TYPE,
        ),
    ]


def _memory_read_to_dict(result: MemoryReadResult) -> dict[str, Any]:
    """Normalize one memory-read result for JSON serialization."""
    return result.to_dict()


async def _read_status_text(client: DosboxHttpClient) -> str:
    """Read the current debugger status as JSON text."""
    return _to_json_text((await client.get_status()).to_dict())


async def _read_registers_text(client: DosboxHttpClient) -> str:
    """Read the current register snapshot as JSON text."""
    return _to_json_text(await client.get_registers())


async def _read_dos_internals_text(client: DosboxHttpClient) -> str:
    """Read the DOS internals snapshot as JSON text."""
    return _to_json_text(await client.get_dos_internals())


STATIC_RESOURCE_READERS: dict[str, ResourceTextReader] = {
    SESSION_STATUS_URI: _read_status_text,
    SESSION_REGISTERS_URI: _read_registers_text,
    SESSION_DOS_INTERNALS_URI: _read_dos_internals_text,
}


def _parse_session_path(uri: str) -> list[str]:
    """Parse one DOSBox session URI and return its path components."""
    parsed = urlparse(uri)
    if parsed.scheme != "dosbox" or parsed.netloc != "session":
        raise ValueError(f"Unsupported resource URI: {uri}")
    return [part for part in parsed.path.split("/") if part]


async def _read_physical_memory_text(
    path_parts: list[str],
    client: DosboxHttpClient,
) -> str:
    """Read one physical-memory template instance as JSON text."""
    address = parse_int(path_parts[2], "addr")
    length = parse_int(path_parts[3], "len")
    return _to_json_text(
        _memory_read_to_dict(await client.read_memory_physical(address, length))
    )


async def _read_segmented_memory_text(
    path_parts: list[str],
    client: DosboxHttpClient,
) -> str:
    """Read one segmented-memory template instance as JSON text."""
    segment = parse_int(path_parts[2], "segment")
    offset = parse_int(path_parts[3], "offset")
    length = parse_int(path_parts[4], "len")
    return _to_json_text(
        _memory_read_to_dict(await client.read_memory_segmented(segment, offset, length))
    )


async def read_resource_text(uri: str, client: DosboxHttpClient) -> str:
    """Resolve any supported MCP resource URI to JSON text."""
    reader = STATIC_RESOURCE_READERS.get(uri)
    if reader is not None:
        return await reader(client)

    path_parts = _parse_session_path(uri)
    match path_parts:
        case ["memory", "physical", _, _]:
            return await _read_physical_memory_text(path_parts, client)
        case ["memory", "segmented", _, _, _]:
            return await _read_segmented_memory_text(path_parts, client)
        case _:
            raise ValueError(f"Unsupported resource URI: {uri}")
