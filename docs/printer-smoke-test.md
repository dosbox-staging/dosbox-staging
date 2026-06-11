# Virtual printer smoke test

End-to-end check that bytes written to `LPT1:` from a DOS app reach the
virtual ESC/P2 printer and a PNG lands in the configured output
directory.

The path under test:

```
DOS app  ->  INT 21h fn 40h (write to handle)
         ->  device_LPT1::Write
         ->  IO_WriteB on data + control ports (per-byte strobe)
         ->  PRINTER_WriteData / PRINTER_WriteControl
         ->  Printer::PrintChar
         ->  page renderer (Freetype + libpng)
         ->  pageN.png in docpath
```

## Prerequisites

The renderer needs five TTF fonts in `<config-dir>/FONTS/`. The names
are fixed and the renderer falls back to `roman.ttf` if a font is
missing. On macOS the staging config dir is
`~/Library/Preferences/DOSBox`; on Linux it's
`$XDG_CONFIG_HOME/dosbox`; on Windows it's
`%LOCALAPPDATA%\DOSBox`.

| Filename       | Used for                       |
| -------------- | ------------------------------ |
| `roman.ttf`    | default + Typeface::Roman      |
| `sansserif.ttf`| Typeface::SansSerif            |
| `courier.ttf`  | Typeface::Courier              |
| `script.ttf`   | Typeface::Script               |
| `ocra.ttf`     | Typeface::OcrA / Typeface::OcrB|

On macOS you can symlink to bundled system fonts:

```sh
mkdir -p ~/Library/Preferences/DOSBox/FONTS
cd ~/Library/Preferences/DOSBox/FONTS
ln -sf "/System/Library/Fonts/Supplemental/Times New Roman.ttf" roman.ttf
ln -sf "/System/Library/Fonts/Supplemental/Arial.ttf"           sansserif.ttf
ln -sf "/System/Library/Fonts/Supplemental/Courier New.ttf"     courier.ttf
ln -sf "/System/Library/Fonts/Supplemental/Times New Roman.ttf" script.ttf
ln -sf "/System/Library/Fonts/Supplemental/Courier New.ttf"     ocra.ttf
```

## Test fixture

Create a fixture directory with one input file and an empty output
directory:

```sh
mkdir -p /tmp/printer_test/out
printf 'Hello world from DOSBox printer!\r\nThis is line 2.\r\n\x0c' \
  > /tmp/printer_test/test.prn
```

The trailing `\x0c` is a form-feed (FF). The ESC/P2 dispatch treats FF
as "eject current page", which triggers the PNG write. Without it, the
page only flushes after `timeout` ms of inactivity or on `PRINTER_Reset`.

## Conf file

Write `/tmp/test_printer_e2e.conf`:

```ini
[printer]
printer = on
printer_port = lpt1
dpi = 360
width = 85
height = 110
printoutput = png
docpath = /tmp/printer_test/out
multipage = off
timeout = 500

[autoexec]
mount c: /tmp/printer_test
c:
type test.prn > LPT1
exit
```

Note `LPT1` (no trailing colon). `type foo > LPT1:` and
`copy foo LPT1:` both go through `DOS_Canonicalize`, which mangles the
trailing colon to a path component that misses the device lookup.
Without the colon the redirect path opens the device directly.

## Run

```sh
rm -f /tmp/printer_test/out/*
build/debug-macos/Debug/dosbox --conf /tmp/test_printer_e2e.conf \
                               --noprimaryconf --exit
ls /tmp/printer_test/out/
```

Expect a single `page1.png` of about 17 KB at 3060x3960 (8.5" x 11" at
360 DPI).

## What to check

- `page1.png` exists and is non-empty.
- Opening it shows the two lines of text rendered in a TrueType-ish
  monospace.
- The host stderr from the run contains `PRINTER: Enabled` (the lazy
  Printer construction message). If it's missing, the strobe never
  reached `PRINTER_WriteControl`.

## Common failure modes

| Symptom                                  | Likely cause                                         |
| ---------------------------------------- | ---------------------------------------------------- |
| No PNG at all                            | Form-feed missing from input; raise `timeout` or add `\x0c` |
| `PRINTER: Enabled` missing               | Bytes never reached the parallel port (BIOS / device-layer regression) |
| `Unable to load font` warnings           | `FONTS/` directory missing or fonts misnamed         |
| PNG produced but all-white               | Fonts didn't load (glyphs not blitted)               |
