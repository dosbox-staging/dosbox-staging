# HTTP API

DOSBox Staging can expose an **HTTP REST API** that gives external tools
access to the internal state of the emulated machine. Use cases include
displaying game information in external overlays, game modding, memory
inspection, bug investigation, function hooking, and building cheat tools.

All endpoints access the internal state atomically between steps of the
emulated CPU. They are never executed during natively implemented DOS
functions or interrupts, so the internal data structures are guaranteed to be
consistent in any given snapshot.

The API only binds to `localhost` by default --- never expose it to untrusted
networks, as it gives full control over the emulator.

!!! warning

    This feature is intended for developers and advanced users. Do not
    enable it unless you know what you're doing.

!!! note

    The HTTP API has been primarily tested with `core = normal`. Your
    experience with other CPU cores may vary.


## Enabling the API

Set [`webserver_enabled`](#webserver_enabled) to `on` in your config and
restart DOSBox:

``` ini
[webserver]
webserver_enabled = on
```

Then open `http://localhost:8086/` in a browser while DOSBox is running to
see the built-in API documentation.


## API endpoints

### CPU registers

`GET /api/v1/cpu/state`

:   Returns all x86 CPU registers: `eax`, `ebx`, `ecx`, `edx`, `esi`,
    `edi`, `esp`, `ebp`, `eip`, `flags`, `cs`, `ds`, `es`, `ss`, `fs`,
    `gs`.


### Reading memory

`GET /api/v1/memory/:offset/:len`
`GET /api/v1/memory/:segment/:offset/:len`

:   Read memory at the given address. The optional segment parameter can be
    the name of a segment register (`CS`, `SS`, `DS`, `ES`, `FS`, `GS`) or a
    numeric value. All URL parameters accept hex values with the `0x` prefix.

    By default, the raw binary data is returned. Set
    `Accept: application/json` to receive a JSON response with the data
    encoded in Base64.

    Maximum read size is 128 MiB per request.

    Example --- download the entire conventional memory area:

    ```
    GET /api/v1/memory/0/0x100000
    ```


### Writing memory

`PUT /api/v1/memory/:offset`
`PUT /api/v1/memory/:segment/:offset`

:   Write memory to the given address. Path parameters work the same as for
    reading.

    Accepts raw binary data with `Content-Type: application/octet-stream`, or
    a JSON object with a Base64-encoded `data` field with
    `Content-Type: application/json`.

    Supports atomic compare-and-swap: set the `If-Match` header to
    Base64-encoded expected data. Returns HTTP 412 (Precondition Failed) if
    the current memory contents don't match.


### Memory allocation

`POST /api/v1/memory/allocate`

:   Allocate memory from the emulated machine.

    Request body:

    ``` json
    {
        "size": 1024,
        "area": "conv",
        "strategy": "best_fit"
    }
    ```

    - `area` --- `conv` (conventional), `UMA` (upper memory), or `XMS`.
    - `strategy` --- `best_fit` (default), `first_fit`, or `last_fit`. The
      XMS allocator only supports `best_fit`.

    Response:

    ``` json
    {
        "addr": 655360
    }
    ```

    Returns HTTP 503 if allocation fails.


`POST /api/v1/memory/free`

:   Free previously allocated memory.

    Request body:

    ``` json
    {
        "addr": 655360
    }
    ```

    Returns HTTP 400 on invalid addresses.


### Drive cycling

`POST /api/v1/drives/:letter/cycle`

:   Cycle to the next disk image on the specified drive. The `:letter`
    parameter is a single drive letter (case-insensitive).

    See [Disk swapping](using-dosbox-staging/storage.md#disk-swapping)
    for background on multi-disk setups.

    Response:

    ``` json
    {
        "drive": "A",
        "current_disk_image_index": 1,
        "total_disk_images": 3,
        "disk_image_paths": [
            "disk1.img",
            "disk2.img",
            "disk3.img"
        ]
    }
    ```

    - **400** --- invalid drive letter
    - **422** --- drive has no disk images attached


### DOS information

`GET /api/v1/dos/internals`

:   Returns pointers to internal DOS data structures:

    - `listOfLists` --- pointer to the DOS list of lists (INT 21h AH=52h)
    - `dosSwappableArea` --- DOS swappable area pointer (INT 21h AX=5D06h)
    - `firstShell` --- pointer to the first shell's PSP (start of usable
      memory)


### System information

`GET /api/v1/dosbox/info`

:   Returns DOSBox Staging version and relevant filesystem paths.

### Shutdown

`POST /api/v1/dosbox/shutdown`

:   Request a graceful shutdown of DOSBox Staging.

## Example tools

The `extras/api/` directory in the DOSBox Staging source tree contains
ready-to-use HTML tools and a JavaScript API wrapper. To use them, copy the
files to the `webserver` directory inside your DOSBox
[configuration](using-dosbox-staging/configuration.md#primary-configuration) folder.


### Memory Monitor

Monitor and manipulate memory locations live. As an example, run the
[Commander Keen Episode 4 Demo](https://www.dosgames.com/game/commander-keen-4-secret-of-the-oracle/)
with the default configuration and import the following config string into
the Memory Monitor:

```
W3siYWRkciI6IjB4YzhlZmUiLCJ0eXBlIjoiY3N0cmluZyIsImxhYmVsIjoiRmlsZSJ9LHsiYWRkciI6IjB4MzgyZjgiLCJ0eXBlIjoidWludDgiLCJsYWJlbCI6IkFtbW8ifSx7ImFkZHIiOiIweDAzODMwYSIsInR5cGUiOiJ1aW50OCIsImxhYmVsIjoiTGl2ZXMifV0==
```

You will now see the current values for lives and ammo --- and yes, you can
edit them.


### Memory Scanner

A lightweight version of tools like Cheat Engine. It identifies where
specific values are stored in memory by iteratively tracking changes:

1. Search for a known value (e.g., your current ammo count).
2. Perform an action in-game that changes that value.
3. Filter the results. Repeat until you've isolated the memory address.


### Memory Viewer

A memory viewer with a built-in x86 disassembler.


### JavaScript API wrapper

The `api.js` file provides a JavaScript class for building custom tools on
top of the API. It handles Base64 encoding/decoding, segment register
resolution, and compare-and-swap operations. TypeScript type definitions are
available in `api.d.ts`.


The API server listens on
[`webserver_bind_address`](#webserver_bind_address) (default `127.0.0.1`,
localhost only) and [`webserver_port`](#webserver_port) (default `8086`). The
default binding restricts access to the local machine. Do not bind to `0.0.0.0`
or an external interface unless you understand the security implications --- the
API provides full control over DOSBox internals, including memory access.


## Configuration settings

You can set the web server parameters in the `[webserver]` configuration
section.


##### webserver_bind_address

:   Bind to the given IP address (`127.0.0.1` by default). This API gives
    full control over DOSBox; do not ever expose this to untrusted hosts. By
    default only local connections are allowed.


##### webserver_enabled

:   Enable the HTTP REST API that exposes internal state and memory. Open
    `http://localhost:8086` in a browser (or use the configured port) to view
    the API documentation.

    Possible values: `on`, `off` *default*{ .default }


##### webserver_port

:   TCP port to bind to (`8086` by default).
