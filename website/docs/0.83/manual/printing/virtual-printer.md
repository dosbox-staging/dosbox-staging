# Virtual printer

DOSBox Staging can emulate a printer attached to one of the parallel
ports. Two printer kinds are supported — pick whichever your DOS app
expects:

- **Epson dot-matrix** — interprets ESC/P and ESC/P 2 escape sequences,
  renders the page, and writes a PNG per page. Covers both 9-pin
  (FX-series, ~1981 onwards) and 24-pin (LQ-series, ~1984 onwards)
  Epson printers. This is what most DOS productivity software,
  business apps, and games print through. Bundled Liberation fonts
  mean it works out of the box with no extra setup.

- **PostScript passthrough** — receives raw PostScript from the DOS
  app and writes it verbatim to a `.ps` file. For apps that targeted
  Apple LaserWriter or generic PostScript printers — Aldus PageMaker
  for DOS, Ventura Publisher, professional DTP suites circa 1985-1995.

There is no host-OS printing. The goal is to keep what your DOS
application prints, not to actually put ink on paper.

Out of scope: inkjet printers (Epson Stylus and its raster-graphics
ESC/P 2 variant), laser printers other than PostScript ones, HP
LaserJet / PCL, and anything else released after the DOS era ended
in 1997.


## What we emulate

The Epson dot-matrix mode is best understood as an **Epson LQ-870
24-pin printer with ESC/P 2** — a mainstream 1991-1995 model. We
also handle the wider ESC/P (pre-ESC/P 2) command subset that older
9-pin and 24-pin models used, so a single emulation covers most of
the DOS era.

The PostScript passthrough mode doesn't emulate a specific printer —
PostScript is self-describing, so any PostScript-capable DOS app
talking to us produces a file we can save and you can open later.


## Picking the right driver in your DOS app or Windows 3.x

For dot-matrix mode (`printer_model = epson_dot_matrix`), pick a
driver in your DOS app or Windows 3.x that matches your app's era:

| App vintage | First choice | Alternatives |
| ----------- | ------------ | ------------ |
| 1991-1995 (late-DOS, ESC/P 2) | Epson LQ-870 | LQ-1170 (wide carriage), LQ-570, LQ-450, LQ-100, LQ-150 |
| 1985-1991 (mid-DOS, classic ESC/P 24-pin) | Epson LQ-500 | LQ-510, LQ-550, LQ-800, LQ-850, LQ-1050, LQ-1500 |
| 1981-1990 (early-DOS, 9-pin) | Epson FX-80 | FX-85, FX-100, FX-185, FX-850, FX-1050, LX-80, LX-100, LX-810, LX-850, RX-80 |

For PostScript passthrough mode (`printer_model = postscript`),
pick any PostScript driver your app offers — Apple LaserWriter,
Apple LaserWriter II, or "Generic PostScript Printer". Anything
that emits standard PostScript works.

### Don't pick these

- **Epson Stylus** (Stylus, Stylus Color, Stylus Photo) — inkjet,
  uses an ESC/P 2 raster-graphics format we don't speak. Output
  will be empty or noise.
- **Epson LQ-2550** — early colour 24-pin with rich CMYK control
  beyond our basic colour support. Text works, colour doesn't.
- **HP LaserJet** — uses PCL, a different command language.
- **Epson PostScript drivers** when running in dot-matrix mode —
  switch the `printer_model` setting to `postscript` instead.
- **Generic Text** / "Epson Standard Printer" — works but throws
  away all our formatting.


## How PostScript fits in

This is worth calling out because "PostScript" has two different
meanings in this manual:

- As a **printer model** (`printer_model = postscript`) it's the
  *input* language — what the DOS app sends to us. The app picks
  a PostScript printer driver in its own settings; we receive
  the PostScript bytes and save them to disk.
- It is **not** the output format for dot-matrix mode. Older versions
  of the virtual printer offered "PostScript output" for dot-matrix
  rasterised pages; that's gone. Dot-matrix mode writes PNGs; the
  postscript mode writes PostScript. The output format follows from
  `printer_model`.


## Output files

- Dot-matrix mode writes **one PNG per page**: `page1.png`, `page2.png`,
  and so on, into `printer_output_dir`.
- PostScript mode writes **one PS file per print job**: `doc1.ps`,
  `doc2.ps`, and so on. A job ends when the DOS app sends the
  PostScript end-of-job marker (`^D`, byte `0x04`), when you press
  the eject-page key, or after the inactivity timeout.

If `printer_output_dir` doesn't exist yet, it's created automatically.


## Fonts

The Epson dot-matrix renderer needs TrueType fonts for text output.
Liberation Serif, Liberation Sans, and Liberation Mono are bundled
with dosbox-staging (under the SIL Open Font License), so no setup
is needed — text just works.

If you want to use different fonts, drop your own TTF files into
`<config-dir>/fonts/` under these names and they'll be picked up
instead of the bundled ones:

- `roman.ttf` — used for the Roman typeface.
- `sansserif.ttf` — Sans Serif.
- `courier.ttf` — Courier (also the default monospace).
- `script.ttf` — Script. (Liberation has no script font; the
  bundled file is Liberation Serif Italic as a stand-in.)
- `ocra.ttf` — OCR-A / OCR-B. (Liberation has no OCR font; the
  bundled file is Liberation Mono as a stand-in.)

Other Epson typefaces (Prestige, Orator, Roman T, etc.) fall back
to the Roman font.


## Parallel port routing

The printer attaches to one of the three parallel ports: LPT1 (the
default, at `0x378`), LPT2 (`0x278`), or LPT3 (`0x3bc`). Most DOS
applications target LPT1, so the default is usually what you want.

If you've already got an LPT DAC (Disney Sound Source, Covox Speech
Thing, or Stereo-on-1) on LPT1, route the printer to LPT2 or LPT3:

```ini
[speaker]
lpt_dac = disney

[printer]
printer_model = epson_dot_matrix
printer_port = lpt2
```

If both devices try to claim the same port, the printer detects the
conflict at startup, logs a warning, and disables itself so the DAC
keeps working.


## Ejecting a page

The printer auto-ejects a page after a brief inactivity timeout
(`printer_timeout`, default 500 ms). You can also force an eject at
any time with the **Eject page** action — bound to **Ctrl+F2** by
default. The binding is fully remappable from the dosbox-staging
mapper UI (the action is named `ejectpage`); use a different key
combination if Ctrl+F2 conflicts with your DOS app or window
manager.

In PostScript passthrough mode the eject key also closes the
current `.ps` file; the next byte from the DOS app starts a new
file.


## Reconfiguring at runtime

You can change any `[printer]` setting at the DOS prompt with
`config -set printer <key> = <value>`. The change takes effect
immediately — the entire printer subsystem is torn down and
re-initialised with the new settings.

Don't reconfigure while a print job is in progress. The contract
is the same as turning off a real printer mid-job: the current
page is lost. Wait until you're back at the DOS prompt with no
active output.


## Configuration settings

Printer settings live in the `[printer]` section.


##### printer_model

:   Which kind of virtual printer to emulate.

    Possible values:

    <div class="compact" markdown>

    - `none` *default*{ .default } — Printer disabled.
    - `epson_dot_matrix` — Epson dot-matrix emulation (ESC/P and
      ESC/P 2). Writes one PNG per page. Best fit for the
      Epson LQ-870 driver in your DOS app.
    - `postscript` — Passthrough PostScript printer. The DOS app
      sends raw PostScript and we save it verbatim to a `.ps`
      file. Use this when your DOS app targets a PostScript
      printer.

    </div>


##### printer_port

:   Which parallel port the virtual printer attaches to. Most DOS
    apps target LPT1.

    Possible values:

    <div class="compact" markdown>

    - `lpt1` *default*{ .default } — LPT1 at I/O port `0x378`.
    - `lpt2` — LPT2 at `0x278`.
    - `lpt3` — LPT3 at `0x3bc`.

    </div>


##### printer_dpi

:   *Only applies when `printer_model = epson_dot_matrix`. Ignored
    in PostScript mode (the PostScript file defines its own
    resolution).*

    Output resolution in dots per inch. Higher values produce
    sharper output and larger PNG files. The default of `360`
    matches the highest-quality mode of late-model 24-pin Epson
    printers.


##### printer_page_size

:   *Only applies when `printer_model = epson_dot_matrix`. Ignored
    in PostScript mode (the PostScript file defines its own
    page size).*

    Output page size. Accepts a named preset or explicit
    dimensions.

    Presets:

    <div class="compact" markdown>

    - `A3` — 29.7 × 42 cm. Wide-carriage Epson printers only
      (LQ-1170, FX-100, FX-1050, and similar).
    - `A4` *default*{ .default } — 21 × 29.7 cm. The European
      default.
    - `A5` — 14.8 × 21 cm.
    - `Letter` — 8.5 × 11 inch. The US default.
    - `Legal` — 8.5 × 14 inch.
    - `Tabloid` — 11 × 17 inch. The US near-equivalent of A3,
      wide-carriage only.
    - `Fanfold` — 14.875 × 11 inch. Common tractor-feed
      continuous paper of the DOS era — the "green-bar" form
      a lot of DOS print code assumes. Wide-carriage only.

    </div>

    Or specify exact dimensions, e.g. `21x29.7cm`, `8.5x11inch`,
    `9.5x11inch` (narrow tractor-feed). The unit suffix is `cm`,
    `inch`, or `in`.


##### printer_output_dir

:   Directory where output files are written. PNG files in
    dot-matrix mode, `.ps` files in PostScript mode. Default is
    the current working directory.

    The directory is auto-created if it doesn't exist.


##### printer_timeout

:   Inactivity timeout in milliseconds. The printer auto-ejects
    an unfinished page after this many milliseconds with no
    incoming data.

    Default is `500`. Set to `0` to disable auto-eject; you'll
    need to press the eject-page key (default Ctrl+F2, fully
    remappable via the mapper UI) manually.


## Known limitations

- **No bar codes.** Apps that print bar codes via the ESC/P 2
  bar-code command will print blank space where the bar code
  should be.
- **No user-defined fonts.** Apps that download custom characters
  to the printer will see the closest equivalent from the active
  TrueType font.
- **No 9-pin high-density graphics.** Most 9-pin apps use plain
  bitmap graphics modes that work fine; only apps that exploit
  the 9th-pin extension for higher vertical density will see
  reduced graphics quality.
- **Limited colour.** Ribbon-style overprinting with 7 fixed
  sub-palettes, not full CMYK control. Most DOS-era apps printed
  in black-and-white anyway.
- **A handful of pitch / control-code commands are recognised
  but ignored.** Apps that depend on these may print slightly
  differently but won't fail.
- **No raster graphics or MicroWeave.** These are the high-res
  formats used by Stylus inkjet printers (post-1993). We don't
  emulate inkjets.
