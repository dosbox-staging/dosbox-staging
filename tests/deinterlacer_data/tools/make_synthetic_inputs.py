#!/usr/bin/env python3
# SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
# SPDX-License-Identifier: GPL-2.0-or-later
"""Generate the synthetic placeholder input frames in ../inputs/.

The frames mimic the two fake-interlacing schemes the deinterlacer
handles: alternating content/black scanlines ("line", 640x480 FMV) and
the KGB/Dune dot pattern ("dot", mode 13h). Content uses a colour
gradient so dimming and neighbour-copy effects are visible in the
goldens.

These are placeholders until real FMV captures replace them (see
../README.md). Uses only the Python standard library.

Run from this directory:

    python3 make_synthetic_inputs.py
"""

import struct
import zlib
from pathlib import Path

INPUTS_DIR = Path(__file__).parent.parent / "inputs"


def write_png(path, width, height, pixels):
    """Write 8-bit truecolour PNG. `pixels` is rows of (r, g, b) tuples."""

    def chunk(tag, data):
        payload = tag + data
        return (struct.pack(">I", len(data)) + payload +
                struct.pack(">I", zlib.crc32(payload) & 0xFFFFFFFF))

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)

    raw = b"".join(
        b"\x00" + bytes(c for px in row for c in px) for row in pixels)

    png = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) +
           chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))

    path.write_bytes(png)
    print(f"wrote {path} ({width}x{height})")


BLACK = (0, 0, 0)


def content_color(x, y, width, height):
    """Bright gradient; every channel >= 0x40 so nothing thresholds as
    background."""
    r = 0x40 + x * (0xFF - 0x40) // max(width - 1, 1)
    g = 0x40 + y * (0xFF - 0x40) // max(height - 1, 1)
    b = 0xCC
    return (r, g, b)


def make_line_frame(width=640, height=480):
    """Line-interlaced frame: leading background rows, an interlaced
    block (even rows content, odd rows black), a solid block that must
    be left untouched, black elsewhere."""
    rows = [[BLACK] * width for _ in range(height)]

    block_first, block_last = 40, height - 82  # even..odd row span
    for y in range(block_first, block_last + 1):
        if y % 2 == 0:
            rows[y] = [content_color(x, y, width, height)
                       for x in range(width)]

    solid_first, solid_last = height - 60, height - 21
    for y in range(solid_first, solid_last + 1):
        rows[y] = [content_color(x, y, width, height) for x in range(width)]

    return rows


def make_dot_frame(width=320, height=200):
    """Dot-interlaced frame (mode 13h): leading black rows, a pattern
    block (even rows: content at even x, black at odd x; odd rows all
    black), a solid block that stops the pattern, black elsewhere."""
    rows = [[BLACK] * width for _ in range(height)]

    block_first, block_last = 10, 149
    for y in range(block_first, block_last + 1):
        if y % 2 == 0:
            rows[y] = [content_color(x, y, width, height) if x % 2 == 0
                       else BLACK for x in range(width)]

    solid_first, solid_last = 160, 189
    for y in range(solid_first, solid_last + 1):
        rows[y] = [content_color(x, y, width, height) for x in range(width)]

    return rows


def scale(rows, x_factor, y_factor):
    """Nearest-neighbour upscale, mimicking baked-in VGA doubling."""
    return [[px for px in row for _ in range(x_factor)]
            for row in rows for _ in range(y_factor)]


def main():
    INPUTS_DIR.mkdir(parents=True, exist_ok=True)

    line = make_line_frame()
    write_png(INPUTS_DIR / "line-640x480.png", 640, 480, line)

    dot = make_dot_frame()
    write_png(INPUTS_DIR / "dot-320x200.png", 320, 200, dot)

    dot_2y = scale(dot, 1, 2)
    write_png(INPUTS_DIR / "dot-320x400.png", 320, 400, dot_2y)

    dot_2x2y = scale(dot, 2, 2)
    write_png(INPUTS_DIR / "dot-640x400.png", 640, 400, dot_2x2y)


if __name__ == "__main__":
    main()
