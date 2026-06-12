# Virtual printer implementation gaps

Engineering-facing record of what the Epson dot-matrix emulation
implements, what it doesn't, and what's worth doing next. Pairs
with `website/docs/0.83/manual/printing/virtual-printer.md` (the
user-facing manual) and the upstream Epson ESC/P Reference Manual.

## Coverage matrix

ESC/P / ESC/P 2 commands organised by area. Status legend:

- **Done** — full behaviour implemented in `src/hardware/printer/printer_dispatch.cpp`.
- **No-op** — opcode recognised, parameters consumed, but no
  rendering effect.
- **Stubbed** — opcode hits a `LOG_ERR` and is deliberately not
  supported.
- **Missing** — opcode not in the dispatch table; hits the
  `Unknown command` warning.

### Text styling

| Command | Spec name | Status | Notes |
| ------- | --------- | ------ | ----- |
| `ESC E` / `ESC F` | Bold on / off | Done | |
| `ESC 4` / `ESC 5` | Italic on / off | Done | |
| `ESC -` | Underline on / off | Done | Score type via `ESC ( -` for double/broken variants. |
| `ESC G` / `ESC H` | Double-strike on / off | Done | Accumulates intensity (darker glyphs). |
| `ESC W` / `SO` / `DC4` | Double-width | Done | Persistent and one-line variants. |
| `ESC w` | Double-height | Done | |
| `FS V` | Double-height (IBM alias) | No-op | Only ESC w handled. |
| `ESC S` / `ESC T` | Superscript / subscript / off | Done — bug | Param index off-by-one suspected. |
| `ESC p` | Proportional on / off | Done | |
| `ESC !` | Master select (style bitfield) | Done | |
| `ESC q` | Character style (fill/outline/shadow) | Missing | |

### Pitch / CPI

| Command | Spec name | Status | Notes |
| ------- | --------- | ------ | ----- |
| `ESC P` | 10 cpi | Done | |
| `ESC M` | 12 cpi | Done | |
| `ESC g` | 15 cpi | Done | |
| `ESC X` | Multipoint scalable (cpi + point size) | Done | |
| `ESC SP` | Intercharacter space | Done | |
| `ESC c` | Horizontal motion index | Done | Resets aggressively when font changes. |
| `FS E` | Character width (IBM) | No-op | |
| `FS S` | High-speed elite | No-op | |

### Line spacing

| Command | Spec name | Status | Notes |
| ------- | --------- | ------ | ----- |
| `ESC 0` | 1/8 inch | Done | |
| `ESC 2` / `FS 2` | 1/6 inch | Done | |
| `ESC 3` | n/180 inch | Done | |
| `ESC A` / `FS A` | n/60 inch | Done | |
| `ESC +` / `FS 3` | n/360 inch | Done | |
| `ESC 1` | 7/72 inch | Missing | Older 9-pin command. |

### Margins and positioning

| Command | Spec name | Status | Notes |
| ------- | --------- | ------ | ----- |
| `ESC $` | Absolute horizontal position | Done | |
| `ESC \` | Relative horizontal position | Done | |
| `ESC l` | Left margin | Done | |
| `ESC Q` | Right margin | Done | |
| `ESC N` | Bottom margin | Done | |
| `ESC O` | Cancel bottom margin | Done | |
| `ESC J` | Advance vertically | Done | n/180 inch. |
| `ESC ( V` | Absolute vertical position | Done | |
| `ESC ( v` | Relative vertical position | Done | |
| `ESC ( U` | Set unit | Done | |
| `ESC ( C` / `ESC ( c` | Page length / format | Done | |
| `ESC C` | Page length in lines | Done | |
| `ESC f` | Horizontal / vertical skip | Missing | |

### Graphics — bit-image (the ESC/P side)

| Command | Spec name | Status | Notes |
| ------- | --------- | ------ | ----- |
| `ESC K` | 60-dpi bit-image (8-pin) | Done | |
| `ESC L` | 120-dpi bit-image | Done | |
| `ESC Y` | 120-dpi double-speed | Done | |
| `ESC Z` | 240-dpi bit-image | Done | |
| `ESC *` | Parametric bit-image | Done | Density modes 0,1,2,3,4,6,32,33,38,39,40,71,72,73. |
| `ESC ?` | Reassign bit-image mode | Done | |
| `ESC ^` | 9-pin "9th-pin" graphics | Missing | See dispatch conflict below. |

### Graphics — raster (the ESC/P 2 side)

| Command | Spec name | Status | Notes |
| ------- | --------- | ------ | ----- |
| `ESC .` | Print raster graphics | Missing | The ESC/P 2 high-res format. |
| `ESC . 2` | TIFF / LZW compressed raster | Missing | |
| `ESC ( G` | Enter graphics mode | Missing | Required by ESC . on ESC/P 2. |
| `ESC ( i` | MicroWeave | Missing | Print-quality hint; safe to no-op. |

### Character encoding

| Command | Spec name | Status | Notes |
| ------- | --------- | ------ | ----- |
| `ESC R` | International character set | Done | 15 predefined sets. |
| `ESC t` / `FS I` | Select character table | Done | 4 slots. |
| `ESC ( t` | Assign character table | Done | |
| `ESC %` | Select user-defined set | Stubbed | LOG_ERR. |
| `ESC &` | Define user-defined characters | Stubbed | LOG_ERR. |
| `ESC :` | Copy ROM to RAM | Stubbed | LOG_ERR. |
| `ESC =` / `ESC >` | Set MSB to 0 / 1 | No-op | Flag set but never applied. |
| `ESC #` | Cancel MSB control | No-op | |
| `ESC 6` / `ESC 7` | Enable upper control codes | No-op | Flag set but never applied. |
| `ESC ^` (24-pin form) | Print as char on next byte | No-op | Same opcode as 9-pin graphics — see below. |
| `ESC I` / `ESC m` | Enable printing of control codes | Missing | |

### Misc

| Command | Spec name | Status | Notes |
| ------- | --------- | ------ | ----- |
| `ESC @` | Initialise printer | Done | |
| `ESC EM` | Paper loading / ejecting | Done | |
| `ESC k` | Select typeface | Done | 12+ typefaces; non-Latin fall back to Roman. |
| `ESC x` | LQ or draft | Done | |
| `ESC r` | Print colour | Done | 7 sub-palettes. |
| `ESC ( -` | Line / score | Done | |
| `ESC j` | Reverse paper feed | Done | |
| `ESC FF` | Form feed (escaped form) | No-op | Raw FF (0x0C) form-feed works. |
| `ESC LF` | Reverse line feed | No-op | |
| `ESC [` | Set char height / width / spacing | No-op | 7 parameters consumed, no action. |
| `ESC 8` / `ESC 9` | Paper-out detector | No-op | Mechanical; not relevant. |
| `ESC <` / `ESC U` | Unidirectional mode | No-op | Mechanical; not relevant. |
| `ESC a` | Justification | No-op | |
| `ESC s` | Low-speed mode | No-op | Mechanical; not relevant. |
| `ESC b` | VFU channel tabs | Missing | |
| `ESC /` | Select VFU channel | No-op | |
| `ESC e` | Fixed tab increment | Missing | |
| `ESC ( B` | Bar codes | Stubbed | LOG_ERR. |

## Likely bugs to investigate

These came out of the cross-check with the Epson spec and the
escapy reference. None block the user-facing scope, but each is a
quick win if validated.

- **`ESC S` param-index bug.** The dispatch reads `params[1]` for
  the superscript condition. Looking at the spec and at
  `printer_dispatch.cpp`, this should probably be `params[0]`.
  Test case in `tests/printer_render_tests.cpp` (Phase 2.5)
  exercises this — see what the test says.
- **`ESC =` / `ESC >` MSB control.** The `msb` field is set to 0
  or 1 but no code path applies it during text rendering. The
  flag is dead.
- **`ESC 6` / `ESC 7` upper control codes.** Same shape — the
  `print_upper_contr` flag is set but never read.
- **`VT` (vertical tab, byte 0x0B) fallback.** The dispatch has
  complex behaviour when no vertical tabs are set: acts as CR
  with no tabs, as LF with all 255 slots filled. Spec says plain
  LF. Worth a spec re-read.
- **HMI vs CPI conflict.** `hmi` is set by `ESC c` but many
  unrelated commands set `hmi = -1` (back to "use CPI"). Apps
  that set HMI then switch fonts may see unexpected horizontal
  spacing.

## Major missing-feature areas

### Raster graphics (ESC . / ESC . 2)

The ESC/P 2 high-resolution graphics format. Required for inkjet
emulation; also used by late-LQ 24-pin printers for high-quality
photographic-style output. Different image-data layout from the
classic bit-image format: plane-major rather than column-major,
with optional TIFF / LZW compression.

**Cost**: significant. Probably 2-3 days of work. Need a separate
raster decoder, plane buffer, colour-plane composition.

### Bar codes (ESC ( B)

EAN, Interleaved 2-of-5, UPC, Code 39, Code 128, Postnet. Each
symbology has its own encoding and rendering rules.

**Cost**: medium. Could leverage a third-party barcode library, or
write a per-symbology renderer using the existing glyph-blit path.

### User-defined characters (ESC %, &, :)

Apps download custom glyphs as 24-byte (24-pin) or smaller (9-pin)
bitmaps. We'd need a cache keyed by character code, and the glyph
renderer would consult it before falling back to the TTF font.

**Cost**: medium. Mostly plumbing — the data format is well-defined
and the bitmap blit code already exists in our renderer.

### MicroWeave (ESC ( i)

Inkjet print-quality hint. Has no visible effect on the output
file; we could accept it as a no-op and stop logging a warning.

**Cost**: trivial. Add to the param-recognised switch with `break;`.

### 9-pin ESC ^ graphics — blocked by dispatch conflict

Opcode `0x5E` has TWO completely different meanings:

- On **9-pin printers** (FX-80, FX-100, LX-series): `ESC ^ m nL nH
  [2-byte columns…]` — 60/120-dpi 9-pin graphics, using the 9th
  pin as a high-bit extension.
- On **24-pin printers** (LQ-series): `ESC ^ n [chars…]` — "print
  the next n bytes as characters even if they'd normally be
  interpreted as control codes."

The wire format doesn't disambiguate. The printer's firmware
decides based on which family it is. Our current code uses the
24-pin "print-as-character" interpretation (param-recognised
no-op, since we don't actually exempt the next bytes from control
interpretation — that's a separate gap).

Adding 9-pin graphics would break the 24-pin meaning. To support
both we'd need to introduce a `printer_compatibility = 9pin | 24pin`
config setting (mirroring what the escapy reference project does —
they require explicit printer-model selection). It's a separate
piece of work; nothing in the current scope.

## What we won't implement

These are deliberately out of scope. They model printer-mechanism
state with no effect on the byte stream we save to disk.

- **Paper-out detector** (`ESC 8`, `ESC 9`) — no paper-out condition
  in a virtual printer.
- **Unidirectional mode** (`ESC U`, `ESC <`) — there's no print
  head moving back and forth.
- **Low-speed mode** (`ESC s`) — no mechanical speed to slow down.
- **BEL** (0x07) — silenced. We're a printer, not a teletype.
- **`ESC i` immediate print** — we're not buffering pages.
- **`ESC #` MSB control** — modern apps don't rely on this and the
  behaviour we'd have to emulate is ambiguous.

## Where things live

- Dispatch: `src/hardware/printer/printer_dispatch.cpp`. Each
  opcode is named via the `constexpr auto Esc<Name> = 0xNN;`
  table near the top of the file.
- Glyph + line rendering: `src/hardware/printer/printer_glyph.cpp`.
- PNG output: `src/hardware/printer/printer_png.cpp`.
- PostScript passthrough: `src/hardware/printer/postscript_passthrough.cpp`.
- Config + lifecycle: `src/hardware/printer/printer_glue.cpp`.
- Class definition + state: `src/hardware/printer/printer.h`.
