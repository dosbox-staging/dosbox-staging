"""MCP tool definitions and dispatch for DOSBox debugger control."""

from __future__ import annotations

from collections.abc import Callable
from typing import Any

from mcp import types

from .client import DosboxHttpClient
from .models import parse_int


def _json_object_schema(description: str) -> dict[str, Any]:
    """Describe a generic JSON object output schema."""
    return {"type": "object", "description": description}


def build_tools() -> list[types.Tool]:
    """Build the MCP tool list exposed by the DOSBox bridge."""
    integer_or_hex = {
        "oneOf": [
            {"type": "integer"},
            {"type": "string", "pattern": "^0[xX][0-9a-fA-F]+$|^[0-9]+$"},
        ]
    }

    return [
        types.Tool(
            name="debug_pause",
            description="Pause DOSBox at the next safe debugger boundary.",
            inputSchema={
                "type": "object",
                "properties": {
                    "mode": {
                        "type": "string",
                        "enum": ["headless", "interactive"],
                        "default": "headless",
                    }
                },
            },
            outputSchema=_json_object_schema("Pause request result."),
        ),
        types.Tool(
            name="debug_continue",
            description="Resume DOSBox execution from a paused debugger state.",
            inputSchema={"type": "object", "properties": {}},
            outputSchema=_json_object_schema("Continue request result."),
        ),
        types.Tool(
            name="debug_step",
            description="Single-step DOSBox execution while paused.",
            inputSchema={
                "type": "object",
                "properties": {
                    "mode": {
                        "type": "string",
                        "enum": ["into"],
                    }
                },
                "required": ["mode"],
            },
            outputSchema=_json_object_schema("Step request result."),
        ),
        types.Tool(
            name="debug_read_memory",
            description="Read DOSBox memory by segmented or physical address.",
            inputSchema={
                "type": "object",
                "properties": {
                    "segment": integer_or_hex,
                    "offset": integer_or_hex,
                    "address": integer_or_hex,
                    "len": integer_or_hex,
                },
                "required": ["len"],
                "oneOf": [
                    {"required": ["segment", "offset", "len"]},
                    {"required": ["address", "len"]},
                ],
            },
            outputSchema=_json_object_schema("Memory read result with base64 payload."),
        ),
    ]


async def _call_memory_tool(
    arguments: dict[str, Any],
    client: DosboxHttpClient,
) -> dict[str, Any]:
    """Dispatch the memory-read tool to the segmented or physical endpoint."""
    length = parse_int(arguments["len"], "len")
    if "address" in arguments:
        result = await client.read_memory_physical(
            parse_int(arguments["address"], "address"),
            length,
        )
    else:
        result = await client.read_memory_segmented(
            parse_int(arguments["segment"], "segment"),
            parse_int(arguments["offset"], "offset"),
            length,
        )
    return result.to_dict()


async def call_tool(
    name: str,
    arguments: dict[str, Any],
    client: DosboxHttpClient,
    *,
    mark_waiting_for_stop: Callable[[], None] | None = None,
) -> dict[str, Any]:
    """Dispatch one MCP tool invocation to the corresponding HTTP call."""
    result: dict[str, Any]

    if name == "debug_pause":
        result = await client.pause(arguments.get("mode", "headless"))
    elif name == "debug_continue":
        result = await client.continue_execution()
        if mark_waiting_for_stop is not None:
            mark_waiting_for_stop()
    elif name == "debug_step":
        result = await client.step(str(arguments["mode"]))
        if mark_waiting_for_stop is not None:
            mark_waiting_for_stop()
    elif name == "debug_read_memory":
        result = await _call_memory_tool(arguments, client)
    else:
        raise ValueError(f"Unknown tool: {name}")

    return result
