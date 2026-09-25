"""CLI entry point for the DOSBox Staging MCP bridge."""

from __future__ import annotations

from argparse import ArgumentParser
import asyncio
import logging
import sys

from mcp.server import stdio
from mcp.server.models import InitializationOptions

from . import __version__
from .server import build_capabilities
from .server import create_server


def parse_args() -> tuple[str, int]:
    """Parse CLI arguments and return the base URL and log level."""
    parser = ArgumentParser(
        prog="dosbox-mcp",
        description="Expose the DOSBox Staging debugger HTTP API as an MCP stdio server.",
    )
    parser.add_argument(
        "--base-url",
        default="http://127.0.0.1:8086",
        help="DOSBox webserver base URL.",
    )
    parser.add_argument(
        "--log-level",
        default="INFO",
        help="Python logging level written to stderr.",
    )
    args = parser.parse_args()
    return args.base_url, getattr(logging, str(args.log_level).upper(), logging.INFO)


async def run_stdio(base_url: str) -> None:
    """Run the MCP bridge over stdio."""
    server = create_server(base_url)
    async with stdio.stdio_server() as (read_stream, write_stream):
        await server.run(
            read_stream,
            write_stream,
            InitializationOptions(
                server_name="dosbox-mcp",
                server_version=__version__,
                capabilities=build_capabilities(server),
            ),
        )


def main() -> None:
    """Configure logging and launch the stdio bridge."""
    base_url, log_level = parse_args()
    logging.basicConfig(
        level=log_level,
        stream=sys.stderr,
        format="%(asctime)s %(levelname)s %(name)s: %(message)s",
    )
    asyncio.run(run_stdio(base_url))
