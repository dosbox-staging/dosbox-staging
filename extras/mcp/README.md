# DOSBox Staging MCP Bridge

This package exposes the DOSBox Staging debugger HTTP API as an MCP stdio
server for agents and other MCP clients.

## Requirements

- DOSBox Staging built with:
  - `OPT_DEBUGGER=ON`
- DOSBox configured with:

```ini
[webserver]
webserver_enabled = on
```

- A local DOSBox instance listening on `127.0.0.1:8086`, or another loopback
  base URL passed to the bridge

## Install

From this directory:

```powershell
python -m pip install -r requirements.txt
python -m pip install .
```

## Run

The low-level MCP Python SDK is used here, so run the bridge directly instead
of through `mcp run` or `mcp dev`:

```powershell
python -m dosbox_mcp
```

To target a different loopback DOSBox webserver URL:

```powershell
python -m dosbox_mcp --base-url http://127.0.0.1:8086
```

## Exposed MCP Surface

Tools:

- `debug_pause`
- `debug_continue`
- `debug_step` (`mode: "into"`)
- `debug_read_memory`

Resources:

- `dosbox://session/status`
- `dosbox://session/registers`
- `dosbox://session/dos-internals`

Resource templates:

- `dosbox://session/memory/segmented/{segment}/{offset}/{len}`
- `dosbox://session/memory/physical/{addr}/{len}`

## Subscription Behavior

The bridge polls `GET /api/v1/debug/status` only when:

- at least one resource is subscribed, or
- a previous `debug_continue` or `debug_step` call is waiting for the next stop

Polling intervals:

- `100 ms` while subscribed
- `25 ms` while waiting for a stop after continue or step

When the debugger lifecycle state changes, the bridge emits
resource-updated notifications for every currently subscribed resource URI,
including the static resources above and subscribed instances of the
resource templates.

This includes `stopSequence` changes and resume transitions that change the
reported running or paused state.

## Safety

This bridge assumes DOSBox is bound to localhost only. Do not expose the
underlying DOSBox webserver to untrusted networks; it can stop execution,
read memory, and control the full emulator session.
