# Virtual printer

DOSBox Staging can emulate an **Epson ESC/P2 dot-matrix printer** attached to
a parallel port. The emulated printer accepts the same escape sequences
real Epson printers did in the 1990s and renders the output to image or
PostScript files in a folder you choose. There's no host-OS printing -- the
goal is to let you keep what your DOS application prints, not to actually
put ink on paper.

Most serious DOS productivity software (WordPerfect, Lotus 1-2-3,
[PrintShop](https://www.mobygames.com/game/267/the-print-shop-deluxe/),
older versions of Microsoft Word, etc.) shipped with Epson drivers, so
picking "Epson LQ" or "Epson FX" in the application's printer settings
usually just works.


## Output formats

The printer can write to two file formats:

- **PNG** *default*{ .default } -- one PNG image per page. Easiest for
  casual viewing, paste into a document, email, etc.
- **PostScript** -- all pages from a single print job combined into one
  `.ps` file when `multipage = on`. Open with Preview on macOS or
  Ghostscript on Linux and Windows.

Both formats are rasterised from a font rendered with FreeType.
The text isn't selectable in the output -- it's pixels, not characters.


## Fonts

The printer needs TTF font files in the staging configuration directory.
The expected file names are:

- `roman.ttf`
- `sansserif.ttf`
- `courier.ttf`
- `script.ttf`
- `ocra.ttf`

If a font file is missing, the printer logs an error at startup and
disables itself. DOSBox Staging doesn't ship any fonts of its own. Any
TrueType font with a permissive licence works -- the DejaVu and
Liberation families are good choices.


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
printer = on
printer_port = lpt2
```

If both devices try to claim the same port, the printer detects the
conflict at startup, logs a warning, and disables itself so the DAC keeps
working.


## Ejecting a page

The printer auto-ejects (form-feeds) a page after a brief inactivity
timeout. You can also force a manual eject with **Ctrl+F2** at any time --
useful with `multipage = on` to close the current PostScript document.


## Configuration settings

Printer settings are to be configured in the `[printer]` section.


##### printer

:   Enable the virtual ESC/P2 dot-matrix printer.

    Possible values:

    <div class="compact" markdown>

    - `off` *default*{ .default } -- Don't run the virtual printer.
    - `on` -- Run the virtual printer on the port chosen by
      `printer_port`.

    </div>


##### printer_port

:   Which parallel port the virtual printer attaches to. Most DOS apps
    target LPT1.

    Possible values:

    <div class="compact" markdown>

    - `lpt1` *default*{ .default } -- LPT1 at I/O port `0x378`.
    - `lpt2` -- LPT2 at `0x278`.
    - `lpt3` -- LPT3 at `0x3bc`.

    </div>


##### dpi

:   Output resolution in dots per inch. Higher values produce sharper
    output and larger files. The default of `360` matches the highest
    resolution mode of late-model 24-pin Epson printers.


##### width

:   Paper width in tenths of an inch. The default `85` is 8.5 inches
    (US Letter). Use `82` for A4.


##### height

:   Paper height in tenths of an inch. The default `110` is 11 inches
    (US Letter). Use `117` for A4.


##### printoutput

:   Output file format.

    Possible values:

    <div class="compact" markdown>

    - `png` *default*{ .default } -- One PNG image per page.
    - `ps` -- PostScript Level 2. Multi-page documents land in one `.ps`
      file when `multipage = on`.

    </div>


##### docpath

:   Directory where output pages are written. Default is the current
    working directory.


##### multipage

:   Combine all pages from a single print job into one PostScript file.
    Press Ctrl+F2 to close the document and start a new one. Only
    meaningful when `printoutput = ps`.

    Possible values:

    <div class="compact" markdown>

    - `off` *default*{ .default } -- Each page is its own file.
    - `on` -- Pages accumulate until you press Ctrl+F2.

    </div>


##### timeout

:   Inactivity timeout in milliseconds. The printer auto-ejects an
    unfinished page after this many milliseconds with no incoming data.
    Default is `500`. Set to `0` to disable the auto-eject (you'll need
    to press Ctrl+F2 manually).
