# Printing

Before the internet, getting a document from one person to another usually
meant printing it and physically handing it over. Printers were as essential
to a DOS PC as the keyboard. Most were **dot-matrix printers** ---
electromechanical machines that stamped ink onto paper through a ribbon, one
line at a time. The dominant brand was Epson, and their command language ---
**ESC/P** --- became the de facto standard that almost every DOS application
knew how to speak.

DOSBox Staging emulates that world with a **virtual printer** --- a printer
that exists only inside the emulator. There is no parallel port passthrough to
your host machine and no connection to any real printer. What you get instead
are output files you can open on any modern machine. No drivers, no paper
jams.


## Dot-matrix printing

Most DOS productivity software, games, and business apps printed to a
dot-matrix printer. The virtual Epson printer in DOSBox Staging is modelled on
the **Epson LQ-870** --- a popular 24-pin model from the early 1990s --- but
it handles the full range of ESC/P commands used across the entire DOS era.

To enable it, add this to your `dosbox.conf`:

```ini
[printer]
printer_model = epson-24pin
```

The virtual printer attaches to LPT1 (see [`printer_port`](#printer_port)),
which is where almost every DOS app expects it.


### Step 1 --- Pick a printer driver in your DOS app

The virtual printer comes in two flavours, set via
[`printer_model`](#printer_model):

- `epson-24pin` (default) --- 24-pin Epson LQ-series emulation.
  Speaks ESC/P 2 and ESC/P. Use for almost everything.
- `epson-9pin` --- 9-pin Epson FX/LX-series emulation. Older command
  set (ESC/P, no ESC/P 2) and different vertical-spacing units (n/216
  and n/72 instead of n/180 and n/60). Use this if your DOS app
  specifically targets an FX-80 / FX-100 / LX driver.

Then go into your application's printer setup and pick an Epson driver
that matches what you want to print:

| What you need                          | `printer_model`    | Recommended driver | Year | Generation | Carriage | Colour
| ---                                    | ---                | ---                | ---  | ---        | ---      | ---
| **Modern B&W (default choice)**        | `epson-24pin` | Epson LQ-870       | 1991 | ESC/P 2    | 8 inch   | No
| Modern B&W, wide paper                 | `epson-24pin` | Epson LQ-1170      | 1991 | ESC/P 2    | 13.6 inch| No
| **Colour, narrow paper**               | `epson-24pin` | Epson LQ-860       | 1989 | ESC/P      | 8 inch   | 7-colour ribbon
| Colour, wide paper                     | `epson-24pin` | Epson LQ-2550      | 1988 | ESC/P      | 13.6 inch| 7-colour ribbon
| Older B&W (1985–1990 app)              | `epson-24pin` | Epson LQ-850       | 1988 | ESC/P      | 8 inch   | No
| Older B&W, wide paper                  | `epson-24pin` | Epson LQ-1050      | 1988 | ESC/P      | 13.6 inch| No
| Early-DOS / 9-pin app                  | `epson-9pin`       | Epson FX-80        | 1983 | ESC/P      | 8 inch   | No
| Early-DOS / 9-pin app, wide paper      | `epson-9pin`       | Epson FX-100       | 1983 | ESC/P      | 13.6 inch| No

When in doubt, pick **LQ-870** for B&W or **LQ-860** for colour.

**Choosing between LQ-850, LQ-860, and LQ-870:**

- LQ-860 is the only one of the three with colour. Both LQ-850 and LQ-870
  are B&W; even if your app sends colour commands, they will print black.
- LQ-870 is the newest of the three and speaks ESC/P 2, which adds scalable
  fonts (point sizes from 8 to 32 in 2-point steps) and tighter page-format
  control. Word processors that print headings at a different size from body
  text take advantage of this.
- LQ-850 only speaks classic ESC/P, no ESC/P 2. Fixed-pitch text and basic
  graphics work fine; scalable / variable-size fonts collapse to the
  printer's built-in sizes.

For plain text it almost doesn't matter which of these you pick: we render
text via TrueType fonts scaled to the configured DPI, not by emulating the
print head pin-by-pin, so a 9-pin or 24-pin driver produces the same crisp
output. Pin count and ESC/P generation only matter for graphics and for the
font features above.

!!! warning "Avoid these drivers"

    - **Epson Stylus** (any model) --- inkjet, uses raster-graphics formats
      (`ESC .`, `ESC ( G`) we don't render. Output will be blank or noise.
    - **Epson DLQ-3000** --- 24-pin colour, but its colour goes through the
      ESC/P 2 `ESC ( G` CMYK path rather than `ESC r`. We don't render that
      path, so colour will print black. Text works.
    - **Epson LQ-2500 / LQ-2550** colour --- the basic 7-colour ribbon works
      (we handle `ESC r`); the richer CMYK-mixing commands these models
      support are approximated as ribbon overprint, not full CMYK.
    - **HP LaserJet** --- uses PCL, a different command language. Not
      supported.
    - **Generic Text** / "Epson Standard Printer" --- works but loses all
      formatting.


### Step 2 --- Print something

Print from your app the normal way. That's it. DOSBox Staging intercepts what
the app sends to the printer port and renders it.


### Step 3 --- Find your output

Each printed page is saved as a PNG file --- `page0001.png`, `page0002.png`,
and so on --- in the same folder as your other captures (screenshots, audio,
video). That's the `capture` directory in your current working directory by
default, configurable via
[`capture_dir`](../using-dosbox-staging/capture.md#capture_dir).

To save output somewhere specific:

```ini
[printer]
printer_model = epson-24pin

[capture]
capture_dir = C:\Captures
```

The folder is created automatically if it doesn't exist.

!!! Tip "Troubleshooting"

    If files aren't appearing, check the DOSBox Staging log. It will tell you
    whether the printer initialised correctly and where it's writing output.


### Page ejection

The printer writes each page to disk when it's ejected. By default, a page
ejects automatically after **10 seconds of inactivity** --- so as long
as your app finishes sending data and then pauses, the page will appear on its
own.

If your app doesn't trigger the auto-eject (some apps hold the port open),
press **Ctrl+F2** to eject the page manually. You can remap this key in the
DOSBox Staging mapper UI (look for the `ejectpage` action) if it conflicts
with your app or window manager.

To disable auto-eject entirely and always use the manual key:

```ini
printer_timeout = 0
```

See [`printer_timeout`](#printer_timeout) for details.


### Page size and resolution

The default page size is **A4** at **360 DPI**. Both are configurable (see
[`printer_page_size`](#printer_page_size) and [`printer_dpi`](#printer_dpi)):

```ini
printer_page_size = letter
printer_dpi = 180
```

Available page size presets:

| Preset           | Size             | Notes
| ---              | ---              | ---
| `a4`             | 21 × 29.7 cm     | Default
| `letter`         | 8.5 × 11 inch    | US standard
| `legal`          | 8.5 × 14 inch    |
| `a3` / `tabloid` | Large format     | Wide-carriage printers only
| `fanfold`        | 14.875 × 11 inch | Classic tractor-feed "green bar" paper

You can also set an exact size: `printer_page_size = 9.5x11inch` (narrow
tractor-feed), `21x29.7cm`, etc. Units are `cm`, `inch`, or `in`.

Higher DPI gives sharper output and larger files. 360 is the highest quality
mode of real 24-pin Epson printers; 180 is a reasonable middle ground if file
size matters.


### Fonts

The virtual Epson printer needs TrueType fonts to render text. **Liberation
Serif, Sans, and Mono are bundled** with DOSBox Staging, so text works out of
the box with no setup.

If you want to substitute your own fonts, drop TTF files into
`<config-dir>/fonts/` with these names:

| Filename        | Typeface
| ---             | ---
| `roman.ttf`     | Roman
| `sansserif.ttf` | Sans Serif
| `courier.ttf`   | Courier / monospace
| `script.ttf`    | Script (bundled fallback: Liberation Serif Italic)
| `ocra.ttf`      | OCR-A / OCR-B (bundled fallback: Liberation Mono)

Other Epson typefaces (Prestige, Orator, Roman T, etc.) fall back to the Roman font.


### If your program uses LPT2 or LPT3

Most DOS apps target LPT1, and that's the default. The only reason to change
this is if LPT1 is already taken by an LPT DAC (Disney Sound Source, Covox
Speech Thing, Stereo-on-1):

```ini
[speaker]
lpt_dac = disney

[printer]
printer_model = epson-24pin
printer_port = lpt2
```

If two devices try to claim the same port, the printer backs off and logs a
warning so the DAC keeps working.


---

## PostScript

A small number of DOS apps --- desktop publishing suites like Aldus PageMaker
or Ventura Publisher --- targeted **PostScript** printers rather than
dot-matrix ones. PostScript is a full page-description language used by laser
printers like the Apple LaserWriter.

DOSBox Staging supports these apps with a passthrough mode: it receives
whatever PostScript the app sends and saves it verbatim to a `.ps` file. One
file per print job.

```ini
[printer]
printer_model = postscript
```

In your DOS app's printer setup, pick any PostScript driver: **Apple
LaserWriter**, **Apple LaserWriter II**, or **Generic PostScript Printer**.

Output files are named `doc0001.ps`, `doc0002.ps`, and so on, saved to
[`capture_dir`](../using-dosbox-staging/capture.md#capture_dir). A job
closes when the driver sends the PostScript end-of-job marker (the `^D`
byte). The
[`printer_timeout`](#printer_timeout) and the `ejectpage` Ctrl+F2 binding
**do not apply in PostScript mode** --- PostScript is self-describing and
the driver's own byte stream already delimits pages and jobs. There's no
reason for the emulator to inject form-feeds.

To view or convert the output on your host:

- **Linux / macOS** — PostScript is natively supported. Use `ps2pdf output.ps
  output.pdf` to convert to PDF.

- **Windows (host)** — install [GSview](https://pages.cs.wisc.edu/~ghost/) or
  Ghostscript to view or convert.

- **macOS 14 (Sonoma) and later** — Preview no longer opens PostScript.
  Install Ghostscript via Homebrew (`brew install ghostscript`) and use
  `ps2pdf`.

!!! note

    PostScript mode ignores [`printer_dpi`](#printer_dpi) and
    [`printer_page_size`](#printer_page_size) --- those are defined inside the
    PostScript file itself.

---

## Raw passthrough

Some DOS apps target printers that aren't PostScript and aren't Epson
dot-matrix --- HP LaserJet (PCL), HP DeskJet inkjets (PCL3), pen plotters
(HP-GL), Canon Bubble Jet (BJL), and so on. For those, DOSBox Staging
offers a *raw passthrough* mode that captures the LPT byte stream
verbatim to a `.prn` file with no interpretation.

```ini
[printer]
printer_model = passthrough
```

Output files are named `doc0001.prn`, `doc0002.prn`, and so on, saved to
[`capture_dir`](../using-dosbox-staging/capture.md#capture_dir). One file
per print session.
A session ends after [`printer_timeout`](#printer_timeout) milliseconds
of inactivity, or when you press **Ctrl+F2** to eject manually.

You can replay the captured `.prn` against the real printer driver
later --- send it to a physical printer that speaks the same language,
or convert it offline with a tool like
[GhostPCL](https://www.ghostscript.com/releases/gpcldnld.html) (for
PCL) or [hp2xx](https://www.gnu.org/software/hp2xx/) (for HP-GL).

!!! warning

    Raw passthrough writes *every byte* to the file --- no filtering,
    no page-splitting. We can't split per-page because PCL and ESC/P
    bit-image commands embed length-prefixed raster data that may
    contain `0x0C` (the form-feed character) as a pixel byte. Splitting
    on `0x0C` would chop image data mid-stream and produce broken
    captures. If you want per-page files, run a real PCL or ESC/P
    parser over the `.prn` offline.

---

## Printing from Windows 3.1x

Windows 3.1x supports two printing paths, one giving high-quality PostScript
output and one using the same Epson emulation as DOS apps.

### PostScript (recommended)

This gives the best output quality and works well for documents you want to
convert to PDF or view on the host.

Enable the PostScript virtual printer in your config:

```ini
[printer]
printer_model = postscript
```

In Windows 3.1x, install a PostScript printer driver connected to **LPT1**.
During setup (or afterwards via **Control Panel → Printers**), pick any
PostScript printer — the **QMS ColorScript 100** is a safe choice. For better
compatibility and cleaner PDF conversion, you can instead install the **Adobe
PostScript 3.1.2 driver** — see the [Adobe PS 3.1.2 download on
archive.org](https://archive.org/details/adobeps312).

Output files are named `doc0001.ps`, `doc0002.ps`, and so on, saved to
[`capture_dir`](../using-dosbox-staging/capture.md#capture_dir). See
[PostScript](#postscript) for further details.


### Epson emulation

You can also use the virtual Epson printer from Windows 3.1x, though output
quality is lower than PostScript. Enable it the same way as for DOS apps:

```ini
[printer]
printer_model = epson-24pin
```

In Windows 3.1x, go to **Control Panel → Printers** and install any 24-pin
Epson LQ driver connected to **LPT1** (or the port you've configured via
[`printer_port`](#printer_port)). Use the same recommendations as for DOS
apps:

- **LQ-870** --- late-DOS / Win 3.x B&W default. ESC/P 2, scalable fonts.
- **LQ-860** --- when you need colour. ESC/P, 7-colour ribbon.
- **LQ-1170** or **LQ-2550** --- wide-carriage variants of the above.

If your Windows install media doesn't include an exact match, any other
24-pin LQ driver from a similar era will produce correct output --- you may
lose newer features (an ESC/P-only driver, for example, gives the printer
spooler nothing to scale fonts with), but pages still render correctly.

Each page is written as a PNG to
[`capture_dir`](../using-dosbox-staging/capture.md#capture_dir), exactly as
with DOS apps. Any graphics resolution your driver offers
(180×180, 360×180, 360×360 dpi) renders correctly.


---

## ESC/P compatibility

The virtual Epson printer covers ESC/P fully and the subset of ESC/P 2
commands that DOS-era programs actually used --- which is most of it. A few
things are not emulated:

- **Bar codes** --- ESC/P 2 bar-code commands print blank space instead
- **Outline / shadow text style** (`ESC q`) --- apps that select outline or
  shadow effects print plain text
- **User-defined / downloaded fonts** --- substituted with the nearest
  bundled TrueType font
- **9-pin high-density graphics** --- standard 9-pin bitmap graphics work
  fine; only the 9th-pin vertical density extension doesn't
- **Raster graphics** (`ESC .`, `ESC ( G`) --- used by Stylus inkjets and
  some late-model 24-pin colour printers (DLQ-3000). Not supported.
- **Full CMYK colour** --- 7-colour ribbon palette only; most DOS apps were
  black-and-white anyway. Rules out the richer colour mixing used by the
  LQ-2500 / LQ-2550 and DLQ-3000.
- **48-pin output** --- TLQ-4800 only; obscure 1988 Europe/Pacific
  wide-carriage product, not on any common DOS app driver list.
- **Inkjet formats** (Epson Stylus / MicroWeave raster) --- not emulated;
  these are post-DOS inkjet formats unrelated to dot-matrix ESC/P.






## Configuration settings

Printer settings live in the `[printer]` section.


##### printer_model

:   Type of virtual printer to emulate.

    <div class="compact" markdown>

    - `none` *default*{ .default } — No printer emulation.
    - `epson-9pin` — 9-pin Epson FX/LX-series printer. Use for
      older DOS programs that target an FX-80 / LX-series
      driver. Line spacing will look wrong if you use the
      `epson-24pin` model with these drivers, and you might
      get vertically stretched graphics as well. Writes one
      PNG image per page in the capture directory (see
      [`capture_dir`](../using-dosbox-staging/capture.md#capture_dir))
      on the form feed command (`^L` byte), after
      [`printer_timeout`](#printer_timeout) milliseconds of
      inactivity, or by pressing the `ejectpage` hotkey.
    - `epson-24pin` — 24-pin Epson LQ-series dot-matrix printer
      (ESC/P and ESC/P 2 dialects). Best fit for the Epson
      LQ-870 / LQ-850 / LQ-550 drivers. See `epson-9pin` for
      the output format.
    - `postscript` — Saves the raw PostScript data sent to the
      printer to a file. Use this when your program targets a
      PostScript printer (e.g., Apple LaserWriter). Writes a
      single PostScript file with `.ps` extension to the
      capture directory (see
      [`capture_dir`](../using-dosbox-staging/capture.md#capture_dir)).
      The output file is closed on the end-of-job marker
      (`^D` byte); [`printer_timeout`](#printer_timeout) and
      the `ejectpage` hotkey are ignored.
    - `passthrough` — Raw byte-stream capture; saves the raw
      data sent to the printer to a `.prn` file. Use this for
      printer languages other than PostScript or ESC/P (e.g.
      HP PCL, HP-GL, Canon BJL, etc.), or when you want a raw
      capture for offline processing. One file is created per
      print session; the session ends after
      [`printer_timeout`](#printer_timeout) milliseconds of
      inactivity or by pressing the `ejectpage` hotkey.

    </div>


##### printer_port

:   Parallel port the virtual printer is attached to. Most DOS
    programs target LPT1. Use `lpt2` or `lpt3` if you also need
    an LPT DAC (see [`lpt_dac`](../sound/sound-devices/covox-variants.md#lpt_dac))
    on LPT1.

    <div class="compact" markdown>

    - `lpt1` *default*{ .default } — LPT1 at I/O port `0x378`.
    - `lpt2` — LPT2 at `0x278`.
    - `lpt3` — LPT3 at `0x3bc`.

    </div>


##### printer_dpi

:   Output resolution of the virtual printer in dots per inch
    (`360` by default). Valid range is 60–1200. Higher values
    produce sharper output and larger PNG files. Default matches
    the highest-quality mode of late-model 24-pin Epson printers.

    Out-of-range values fall back to `360` with a warning.

    !!! note

        Only applies to the `epson-24pin` printer model to set
        the size of the PNG output image. Ignored in PostScript
        mode (the PostScript file defines its own resolution).


##### printer_page_size

:   Output page size of the virtual printer (`a4` by default).
    Accepts a named preset or explicit dimensions with an
    optional orientation suffix.

    Presets:

    <div class="compact" markdown>

    - `a3` — 29.7 × 42 cm. Wide-carriage Epson printers only
      (LQ-1170, FX-100, FX-1050, and similar).
    - `a4` *default*{ .default } — 21 × 29.7 cm. The European
      default.
    - `a5` — 14.8 × 21 cm.
    - `letter` — 8.5 × 11 inch. The US default.
    - `legal` — 8.5 × 14 inch.
    - `tabloid` — 11 × 17 inch. The US near-equivalent of A3,
      wide-carriage only.
    - `fanfold` — 14.875 × 11 inch. Common tractor-feed
      continuous paper of the DOS era — the "green-bar" form
      a lot of DOS print code assumes. Wide-carriage only.

    </div>

    Or specify exact dimensions, e.g. `21x29.7cm`, `8.5x11inch`,
    `9.5x11inch` (narrow tractor-feed). The unit suffix is `cm`,
    `inch`, or `in`.

    Append `landscape` or `portrait` to specify the orientation
    (defaults to `portrait` if unspecified):

    ```ini
    printer_page_size = a4 landscape    # 29.7 × 21 cm
    printer_page_size = letter portrait # 8.5 × 11 inch (already portrait, no-op)
    ```

    For DOS apps to actually emit landscape output you also need
    to select a landscape-aware (or wide) printer driver in the
    app's printer setup --- the driver controls what the app
    sends; this setting controls the page we render onto.

    Dimensions must be 1 to 50 inches. Out-of-range or invalid
    values fall back to `a4` with a warning.

    !!! note

        Only applies to the `epson-24pin` printer model to set
        the size of the PNG output image. Ignored in PostScript
        mode (the PostScript file defines its own page size).


##### printer_timeout

:   Inactivity timeout in milliseconds after which an unfinished
    page is auto-ejected. Default `10000` (10 seconds). DOS print
    drivers commonly pause for several seconds mid-job while
    preparing the next batch of bytes; too short a timeout splits
    one logical page across multiple output files. Raise this
    further if you still see split pages, or set to `0` to
    disable auto-eject entirely. You can also eject manually via
    the `ejectpage` mapper action (default Ctrl+F2, fully
    remappable).

    Ignored when `printer_model = postscript` --- the PostScript
    output file closes only on the driver's end-of-job marker.

