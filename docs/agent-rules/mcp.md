# MCP Bridge Rules

## When To Use It

Use the bridge when you need debugger control or machine-state inspection from
outside DOSBox:

- pause execution
- continue execution
- single-step one instruction
- read registers
- read DOS internals
- read memory through segmented or physical addresses

Do not treat it as a general automation surface for unrelated DOSBox tasks.

## Choose The Right Surface

- Use a **tool** when you want to change execution state or perform one
  explicit read action.
- Use a **static resource** when you want the latest snapshot of a stable,
  named view such as status or registers.
- Use a **resource template** when you need a parameterized memory read and may
  want to subscribe to that exact URI later.
- In practice, start with `dosbox://session/status` before and after pause,
  step, or continue operations.

## Tools

### `debug_pause`

Pause DOSBox at the next safe debugger boundary.

- `mode` may be `headless` or `interactive`
- prefer `headless` unless you explicitly want the debugger UI
- result is the HTTP pause response object

### `debug_continue`

Resume execution from a paused debugger state.

- use only when DOSBox is paused
- calls are rejected while DOSBox is running
- the bridge then polls until it sees the next stop or another lifecycle change

### `debug_step`

Step one instruction while paused.

- only `mode: "into"` is supported
- calls are rejected while DOSBox is running
- the bridge treats this like continue-then-wait-for-next-stop

### `debug_read_memory`

Read memory by either segmented or physical address.

Accepted arguments:

- segmented form: `segment`, `offset`, `len`
- physical form: `address`, `len`
- numeric fields accept either integers or `0x`-prefixed strings

Returned shape:

- `address`: effective physical address used for the read
- `dataBase64`: memory bytes encoded as base64

Use this tool for one-off reads. Prefer resource templates when the memory
location itself is the object you want to track or revisit. The memory
resource templates return the same logical payload shape.

## Static Resources

All resource reads return compact JSON text. Read them as JSON payloads encoded
inside MCP text content, not as already-parsed native objects.

### `dosbox://session/status`

Use this when you need the current debugger lifecycle state plus registers in
one payload.

This is the primary coordination resource for debugger control.

Returned fields:

- `running`
- `paused`
- `pauseMode`
- `stopSequence`
- `reason`
- `description`
- `registers`

`registers` is the same flat register dictionary exposed by the registers
resource.

### `dosbox://session/registers`

Use this when you only need the current register snapshot.

Returned fields:

- `eax`, `ebx`, `ecx`, `edx`
- `esi`, `edi`, `ebp`, `esp`
- `eip`, `flags`
- `cs`, `ds`, `es`, `ss`, `fs`, `gs`

All register values are normalized to integers.

### `dosbox://session/dos-internals`

Use this for DOS housekeeping structures rather than CPU state.

This resource mirrors the existing DOS internals HTTP payload and is the right
place to inspect items such as:

- list-of-lists pointers
- swappable area pointers
- first-shell pointers

Prefer this resource over raw memory reads when the existing DOS internals
snapshot already contains the structure you need.

## Resource Templates

### `dosbox://session/memory/segmented/{segment}/{offset}/{len}`

Reads memory using segment:offset addressing.

Use it when the address is naturally expressed in segmented form or when the
segment value itself matters for the task.

### `dosbox://session/memory/physical/{addr}/{len}`

Reads memory by physical address.

Use it when you already know the effective address or are working from a
physical-memory view.

Both memory templates resolve to JSON text with:

- `address`: effective physical address used by the read
- `dataBase64`: memory bytes encoded as base64

## Subscription Behavior

Subscriptions are lifecycle-driven, not high-frequency register streams.

- The bridge polls debugger status only while subscriptions are active or while
  waiting for the next stop after `debug_continue` or `debug_step`.
- Updates are emitted when debugger lifecycle state changes, including stop and
  resume transitions.
- Do not expect notifications for every register change while DOSBox is
  running.

`stopSequence` is the most useful field for detecting a newly observed stop.

## Safety

- Treat the underlying DOSBox webserver and this bridge as highly privileged.
- Assume any caller can pause execution and inspect memory.
- Keep usage on loopback-only DOSBox endpoints.
