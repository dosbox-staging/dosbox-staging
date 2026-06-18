# DOSBox Staging config migration — design notes

Companion to `config-findings.md` (which captures research output: rule
data, per-section deltas, warning text). This doc captures the
implementation design and current status. Read both before touching
the migrator code.

## Goal

A one-shot CLI tool inside the dosbox-staging binary that walks a
directory recursively, rewrites each `.conf` it finds to the current
settings format, and creates timestamped `.bak` backups. Comments,
whitespace, section order, and untouched lines are preserved
byte-for-byte; only lines that match a migration rule are rewritten.

## CLI surface

```
dosbox --migrate-config <path>          # dry-run, prints summary + warnings
dosbox --migrate-config <path> --migrate-write   # apply changes, with backup
```

`<path>` can be a single `.conf` file or a directory. Directory walks
recurse, skipping symlinks and hidden directories. Each candidate file
is sniffed for known DOSBox section headers before being considered.

Both flags are output-only actions — they cause the binary to exit
without starting the emulator.

## Pipeline

Three pure stages plus orchestration:

1. **Parse** (`ParseFile` / `ParseBuffer` in `rewriter.cpp`) — read
   file as bytes, split into lines preserving EOLs, classify each line,
   build a `ParsedFile` structure of sections containing entries (raw
   lines or attached units).

2. **Apply** (`ApplyMigrations`) — walk sections, look up rules per
   `(section, key)`, rewrite or remove or queue cross-section moves.
   Then insert queued moves into target sections.

3. **Serialise** (`SerialiseFile`) — re-emit the structure as bytes.
   Untouched lines emit verbatim; rewritten lines use canonical form.

`migrate.cpp` orchestrates: find candidate files → for each:
parse → apply → serialise → if `--migrate-write`: backup + atomic
write via `.tmp` rename.

## File layout

```
src/config/migrate/
├── parsed_file.h     — data structures (header-only)
├── line_classifier   — per-line tagging
├── file_walker       — recursive walk + sniff for DOSBox configs
├── rules             — Rule type, table, Custom handlers, Apply()
├── cycles_parser     — port of ConfigureCyclesLegacy
├── rewriter          — Parse + Apply + Serialise
└── migrate           — public entry, orchestration, I/O, backup
```

CMake: registered from `src/config/CMakeLists.txt` with inlined paths
(no nested CMakeLists.txt), matching the `src/audio/clap/` pattern.

## Key design choices

### Early dispatch

The migrator runs before any module config registration in `main()`.
No emulator state is set up. This avoids pre-migration noise from the
normal config loader (which would otherwise emit deprecation warnings
about the file the user is trying to migrate).

### Line classifier

Distinguishes `Blank` / `SectionHeader` / `Comment` /
`CommentedOutSetting` / `Setting` / `DosCommand` / `Unknown`. Preserves
the original line ending bytes (`\n`, `\r\n`, `\r`, or empty for EOF).

Crucially: `# key = value` is recognised as `CommentedOutSetting`
(distinct from a plain `Comment`). This matters for the
comment-attachment algorithm — commented-out settings act as
block-breakers.

### Comment-attachment

Comments immediately above a setting form an "attached unit" with it.
On cross-section moves, the unit moves as a whole. Algorithm and edge
cases are documented in `config-findings.md` under "Migrator behaviour:
comment attachment".

Block-breakers when walking back from a setting:
- Real setting line
- Blank line
- Commented-out setting
- Section header

Inline trailing comments (`setting = value  # note`) live on the
setting's own line and move with it automatically.

### Rule data model

`Rule` (in `rules.h`):
- `from_section`, `from_key` — match criteria
- `action` — `Rename` / `Move` / `Remove` / `ValueMap` / `Split` / `Custom`
- `to_section`, `to_key`, `split_key`, `split_value`, `value_map`,
  `custom`, `warning` — payload depending on action

Simple actions are data-driven; anything value-conditional, multi-emit,
or with per-value warnings uses `Action::Custom` with a function
pointer (`CustomFn`).

Custom handlers live in `rules.cpp` grouped by section. The cycles
parser is its own file because of size.

### Cross-section moves

Source unit is erased and queued for the target section. After all
rules are processed, queued units are inserted at the END of the
target section's entries but **before any trailing blank lines**, so
the section-to-section separator stays intact.

If the target section doesn't exist in the file, it's created **before
`[autoexec]`** (or at EOF if there's no `[autoexec]`). `[autoexec]` is
always last in a DOSBox config.

When the source unit is erased, an immediately-following blank line is
consumed too — avoids double-blanks in the source section.

### WARNING emission

Emitted as `# WARNING: <text>` comment lines above the rewritten
setting (or above the commented-out original for removed settings).
WARNINGs from cross-section moves attach to the unit and travel with
it. Currently single-line; long texts may wrap awkwardly on narrow
terminals.

### Byte preservation

Untouched lines are emitted verbatim using their preserved
`raw + eol`. Rewritten lines use canonical formatting:
`key = value` (one space around `=`). A rule that matches but produces
the same key+value with no warning is detected as a no-op
(`same_emit_as_input`) so the original bytes pass through unchanged.

### Atomic write

In `--migrate-write` mode:
1. Copy file to `<path>.bak.<YYYYMMDD-HHMMSS>` (refuse if exists)
2. Write content to `<path>.tmp` (binary mode, full write before close)
3. `std::filesystem::rename` `<path>.tmp` → `<path>` (atomic on POSIX)

If any step fails, the temp file is cleaned up and the original is
untouched.

## Cycles parser

`cycles_parser.cpp` is a from-scratch port of `ConfigureCyclesLegacy`
in `src/cpu/cpu.cpp:3421`. Recognised input forms:

```
cycles =                    (empty — drop)
cycles = <N>                (plain int)
cycles = fixed <N>
cycles = max [<P>%] [limit <M>]
cycles = auto [<N>] [<P>%] [limit <M>]
```

Maps to `cpu_cycles` + `cpu_cycles_protected` per the table in
`config-findings.md`. `cpu_throttle` is not emitted (default is `off`).

Lossy modifiers (`<P>%` percent cap, `limit <M>`) are dropped with a
WARNING. Out-of-range N is clamped to `[50, 2_000_000]` with a
WARNING. N ≤ 0 falls back to the legacy default of 3000 cycles with a
WARNING. Unparseable input is preserved unchanged with a WARNING so
the runtime legacy parser can handle it.

## Known limitations

- **`cputype = 386` non-idempotency.** Pre-0.82 `386` semantically meant
  today's `386_fast`; on main, plain `386` is a different (slower)
  core. The rule rewrites `386 → 386_fast`. The migrator can't tell
  pre-0.82 from current, so re-running on an already-migrated file
  bumps the value again. Intended one-shot use.

- **WARNINGs not wrapped.** Long warning texts emit on single lines.

- **No diff output in dry-run.** Prints per-file summary + WARNINGs
  but no unified diff.

- **No validation pass.** Doesn't re-parse the migrated output to
  confirm it loads cleanly under the current config schema.

- **Disk-image rewrites out of scope.** `IMGMOUNT`'d `.img`/`.iso`
  files are never opened.

## Phase 2 (deferred)

Rewrite DOS-command lines inside `[autoexec]` section bodies AND in
`.bat` files reachable via MOUNT host paths. The same rule table
applies — three contexts to recognise:

1. `config -set "section key=value"` (and its variants)
2. Bare-setting shortcuts (`sbtype sb16`, `irq = 5`)
3. MOUNT / IMGMOUNT / MIXER syntax (`-usecd` removal, channel-name
   aliases, etc.)

Gated on a `--migrate-batch` flag. Not started.

## Outstanding M-confidence items

Verification needed before locking the spec (from `config-findings.md`):

1. `[dos] country` numeric→string preservation: do v0.81+ still parse
   numeric country codes?
2. `[serial]` modem params: was `baudrate:` renamed to `bps:`? Parser
   still accepts both?
3. `[mouse] mouse_sensitivity` MultiVal `"X,Y"` form — still parses
   on main?
4. `[mixer] crossfeed` numeric → preset bucketing thresholds — the
   bucket boundaries (0/25/50/100) are editorial, not from code.

## Testing strategy

Real configs drive tests, not synthetic ones. When a user supplies a
config:

1. Run the migrator against it
2. Iterate on rules / pipeline until output is right
3. Promote the (input, expected output) pair to a unit test under
   `tests/`

Project uses GoogleTest (see `tests/*_tests.cpp` for the pattern).

## Status as of 2026-06-18

Five commits ahead of `origin/main`:

| Commit | Description |
|---|---|
| `docs:` | Add config migration research notes (`config-findings.md`) |
|        | Add config migration scaffolding |
|        | Populate config migration rules (excluding cycles) |
|        | Add legacy cycles parser to config migrator |
|        | Add config migrator rewriter pipeline |

The migrator is end-to-end functional. Awaiting real-config test drive
to pin down edge cases and turn them into unit tests.
