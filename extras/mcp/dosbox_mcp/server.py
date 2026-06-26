"""Low-level MCP server for DOSBox Staging."""

from __future__ import annotations

from collections.abc import AsyncIterator
from contextlib import asynccontextmanager
from typing import Any
import asyncio
import logging

import anyio
from mcp import types
from mcp.server.lowlevel import NotificationOptions
from mcp.server.lowlevel import Server
from mcp.server.session import ServerSession

from . import __version__
from .client import DosboxHttpClient
from .client import DosboxHttpError
from .resources import list_resource_templates
from .resources import list_static_resources
from .resources import read_resource_text
from .tools import build_tools
from .tools import call_tool

logger = logging.getLogger(__name__)

SubscribedSessions = dict[str, set[ServerSession]]


def _status_signature(status_payload: dict[str, Any]) -> tuple[Any, ...]:
    """Capture lifecycle fields only, excluding register churn while running."""
    return (
        status_payload["running"],
        status_payload["paused"],
        status_payload["pauseMode"],
        status_payload["stopSequence"],
        status_payload["reason"],
        status_payload["description"],
    )


class SubscriptionManager:
    """Track MCP resource subscriptions and emit update notifications."""

    subscribed_poll_interval_seconds = 0.100
    stop_wait_poll_interval_seconds = 0.025

    def __init__(self, client: DosboxHttpClient) -> None:
        self._client = client
        self._subscriptions: SubscribedSessions = {}
        self._session_index: dict[ServerSession, set[str]] = {}
        self._lock = asyncio.Lock()
        self._stop_event = asyncio.Event()
        self._poller_task: asyncio.Task[None] | None = None
        self._poll_state = {
            "waiting_for_stop": False,
            "last_status_signature": None,
            "last_stop_sequence": None,
        }

    async def start(self) -> None:
        """Start the background poller that drives resource updates."""
        self._stop_event.clear()
        self._poller_task = asyncio.create_task(self._poll_loop())

    async def stop(self) -> None:
        """Stop the background poller and wait for it to exit."""
        self._stop_event.set()
        if self._poller_task is not None:
            await self._poller_task
            self._poller_task = None

    def mark_waiting_for_stop(self) -> None:
        """Accelerate polling until the next stop sequence is observed."""
        self._poll_state["waiting_for_stop"] = True

    async def add(self, uri: str, session: ServerSession) -> None:
        """Register one resource subscription for the current MCP session."""
        async with self._lock:
            self._subscriptions.setdefault(uri, set()).add(session)
            self._session_index.setdefault(session, set()).add(uri)

    async def remove(self, uri: str, session: ServerSession) -> None:
        """Remove one resource subscription for the current MCP session."""
        async with self._lock:
            sessions = self._subscriptions.get(uri)
            if sessions is not None:
                sessions.discard(session)
                if not sessions:
                    self._subscriptions.pop(uri, None)

            uris = self._session_index.get(session)
            if uris is not None:
                uris.discard(uri)
                if not uris:
                    self._session_index.pop(session, None)

    async def _remove_session(self, session: ServerSession) -> None:
        """Drop all subscriptions for one disconnected or failing session."""
        async with self._lock:
            uris = self._session_index.pop(session, set())
            for uri in uris:
                sessions = self._subscriptions.get(uri)
                if sessions is None:
                    continue
                sessions.discard(session)
                if not sessions:
                    self._subscriptions.pop(uri, None)

    async def _notify_uri(self, uri: str) -> None:
        """Notify every subscriber that one resource URI changed."""
        async with self._lock:
            sessions = list(self._subscriptions.get(uri, ()))

        for session in sessions:
            try:
                await session.send_resource_updated(uri)
            except (
                anyio.BrokenResourceError,
                anyio.ClosedResourceError,
                RuntimeError,
                OSError,
            ) as exc:  # pragma: no cover - transport timing
                logger.debug("Failed to send resource update for %s: %s", uri, exc)
                await self._remove_session(session)

    async def _notify_status_change(self) -> None:
        """Notify every currently subscribed URI after execution advanced."""
        async with self._lock:
            uris = list(self._subscriptions)

        for uri in uris:
            await self._notify_uri(uri)

    async def _poll_loop(self) -> None:
        """Poll DOSBox while subscriptions or stop waits are active."""
        while not self._stop_event.is_set():
            async with self._lock:
                has_subscriptions = bool(self._subscriptions)

            if not has_subscriptions and not self._poll_state["waiting_for_stop"]:
                await self._wait(self.subscribed_poll_interval_seconds)
                continue

            try:
                status = await self._client.get_status()
            except DosboxHttpError as exc:  # pragma: no cover - depends on DOSBox availability
                logger.debug("Status poll failed: %s", exc)
                await self._wait(self.subscribed_poll_interval_seconds)
                continue

            status_payload = status.to_dict()
            status_signature = _status_signature(status_payload)
            last_stop_sequence = self._poll_state["last_stop_sequence"]
            last_status_signature = self._poll_state["last_status_signature"]
            if last_stop_sequence is None:
                self._poll_state["last_stop_sequence"] = status.stop_sequence
                self._poll_state["last_status_signature"] = status_signature
            elif status.stop_sequence != last_stop_sequence:
                self._poll_state["last_stop_sequence"] = status.stop_sequence
                self._poll_state["last_status_signature"] = status_signature
                self._poll_state["waiting_for_stop"] = False
                await self._notify_status_change()
            elif status_signature != last_status_signature:
                self._poll_state["last_status_signature"] = status_signature
                await self._notify_status_change()
            elif status.paused:
                self._poll_state["waiting_for_stop"] = False

            interval = (
                self.stop_wait_poll_interval_seconds
                if self._poll_state["waiting_for_stop"]
                else self.subscribed_poll_interval_seconds
            )
            await self._wait(interval)

    async def _wait(self, timeout_seconds: float) -> None:
        """Wait until stop is requested or the next poll interval elapses."""
        try:
            await asyncio.wait_for(self._stop_event.wait(), timeout=timeout_seconds)
        except TimeoutError:
            pass


@asynccontextmanager
async def build_lifespan(
    _server: Server[dict[str, Any], types.ReadResourceRequest],
    client: DosboxHttpClient,
    subscriptions: SubscriptionManager,
) -> AsyncIterator[dict[str, Any]]:
    """Own the MCP server lifetime for one DOSBox HTTP client instance."""
    await subscriptions.start()
    try:
        yield {"client": client, "subscriptions": subscriptions}
    finally:
        await subscriptions.stop()
        await client.aclose()


def create_server(base_url: str) -> Server[dict[str, Any], types.ReadResourceRequest]:
    """Create one low-level MCP server bound to a DOSBox base URL."""
    client = DosboxHttpClient(base_url)
    subscriptions = SubscriptionManager(client)

    @asynccontextmanager
    async def lifespan(
        server: Server[dict[str, Any], types.ReadResourceRequest],
    ) -> AsyncIterator[dict[str, Any]]:
        """Bind the DOSBox client and subscriptions to one MCP server instance."""
        async with build_lifespan(server, client, subscriptions) as context:
            yield context

    server = Server(
        name="dosbox-mcp",
        version=__version__,
        instructions=(
            "Bridge DOSBox Staging's localhost debugger HTTP API into MCP tools "
            "and resources for reverse engineering and bug hunting."
        ),
        lifespan=lifespan,
    )

    @server.list_tools()
    async def handle_list_tools() -> list[types.Tool]:
        """Return the tool list exposed by the DOSBox bridge."""
        return build_tools()

    @server.call_tool()
    async def handle_call_tool(
        name: str,
        arguments: dict[str, Any],
    ) -> dict[str, Any]:
        """Dispatch one MCP tool call to the DOSBox HTTP client."""
        return await call_tool(
            name,
            arguments or {},
            client,
            mark_waiting_for_stop=subscriptions.mark_waiting_for_stop,
        )

    @server.list_resources()
    async def handle_list_resources() -> list[types.Resource]:
        """Return the static resource list for one DOSBox session."""
        return list_static_resources()

    @server.list_resource_templates()
    async def handle_list_resource_templates() -> list[types.ResourceTemplate]:
        """Return the parameterized resource templates for one DOSBox session."""
        return list_resource_templates()

    @server.read_resource()
    async def handle_read_resource(uri: str) -> str:
        """Read one MCP resource URI through the DOSBox HTTP API."""
        return await read_resource_text(str(uri), client)

    @server.subscribe_resource()
    async def handle_subscribe_resource(uri: str) -> None:
        """Register one MCP resource subscription for the active session."""
        await subscriptions.add(str(uri), server.request_context.session)

    @server.unsubscribe_resource()
    async def handle_unsubscribe_resource(uri: str) -> None:
        """Remove one MCP resource subscription for the active session."""
        await subscriptions.remove(str(uri), server.request_context.session)

    return server


def build_notification_options() -> NotificationOptions:
    """Describe the resource notification support exposed by this server."""
    return NotificationOptions(resources_changed=True)


def build_capabilities(
    server: Server[dict[str, Any], types.ReadResourceRequest],
) -> types.ServerCapabilities:
    """Build the advertised MCP capabilities for the DOSBox bridge."""
    base_capabilities = server.get_capabilities(
        notification_options=build_notification_options(),
        experimental_capabilities={},
    )
    return types.ServerCapabilities(
        experimental=base_capabilities.experimental,
        logging=base_capabilities.logging,
        prompts=base_capabilities.prompts,
        resources=types.ResourcesCapability(subscribe=True, listChanged=True),
        tools=base_capabilities.tools,
        completions=base_capabilities.completions,
        tasks=base_capabilities.tasks,
    )
