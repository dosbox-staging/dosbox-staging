#!/usr/bin/env python3
# SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
# SPDX-License-Identifier: GPL-2.0-or-later

# pylint: disable=import-error
# Pillow is a developer-only dependency for this one-shot fixture
# regenerator; the CI lint environment doesn't install it.

"""Convert PNG images to ESC/P bit-image bytes for the printer render tests.

Reads input PNGs, dithers them to 1-bit (Floyd-Steinberg), and emits a
self-contained ESC/P byte stream that the dosbox_tests printer
harness can replay. Run once when source images change; commit the
resulting .bin alongside the source PNGs.

Usage:
    python3 png_to_escp.py <output.bin> <page_width_px> <png:max_width_px>...

`page_width_px` is the printer's page width in pixels (used to
horizontally centre each image). Each input is given as
`PATH:WIDTH` where `WIDTH` is the maximum pixel width the image
should be resized to (preserves aspect ratio). The images are
stacked vertically in the order given, each centred horizontally.

Output layout:
  ESC @                      reset printer state
  for each image:
    [ESC $ x_60th 0]         horizontal position to centre the image
    [optional ESC J n]       advance vertically before the image
    image rows of 24 pins:
      ESC $ x_60th 0         re-centre at start of each row band
      ESC * 39 nL nH [3*W bytes]
"""

import sys

from PIL import Image


# ESC * density 39: 180x180 dpi, 24-pin (3 bytes per column).
DENSITY = 39
PINS_PER_BAND = 24


def load_one_bit(path: str, max_width: int) -> Image.Image:
    """Return a 1-bit PIL image resized to <= max_width pixels wide."""
    img = Image.open(path)
    if img.mode != 'L':
        # Composite onto white so transparent PNGs render correctly.
        if img.mode == 'RGBA':
            bg = Image.new('RGB', img.size, (255, 255, 255))
            bg.paste(img, mask=img.split()[3])
            img = bg
        img = img.convert('L')
    if img.width > max_width:
        ratio = max_width / img.width
        new_height = int(img.height * ratio)
        img = img.resize((max_width, new_height), Image.LANCZOS)
    return img.convert('1', dither=Image.FLOYDSTEINBERG)


def encode_band(img: Image.Image, y_start: int) -> bytes:
    """Encode rows [y_start, y_start+24) as one ESC * mode 39 call.

    Each output column = 3 bytes:
      byte 0: pins 1-8 (top), MSB = pin 1 (= row y_start).
      byte 1: pins 9-16
      byte 2: pins 17-24 (LSB = pin 24 = row y_start+23)
    """
    width = img.width
    height = img.height
    out = bytearray()
    # ESC * m nL nH
    out.extend([0x1B, ord('*'), DENSITY,
                width & 0xFF,
                (width >> 8) & 0xFF])
    pixels = img.load()
    for x in range(width):
        b0 = b1 = b2 = 0
        for bit_in_band in range(PINS_PER_BAND):
            y = y_start + bit_in_band
            if y >= height:
                break
            # 1-bit PIL: 0 = black ink, 255 = white. We want black to
            # set the dot.
            if pixels[x, y] == 0:
                byte_idx = bit_in_band // 8
                # bit 7 = top of byte (= top pin of this byte)
                bit_pos = 7 - (bit_in_band % 8)
                mask = 1 << bit_pos
                if byte_idx == 0:
                    b0 |= mask
                elif byte_idx == 1:
                    b1 |= mask
                else:
                    b2 |= mask
        out.append(b0)
        out.append(b1)
        out.append(b2)
    return bytes(out)


def encode_image(img: Image.Image,
                 page_width_px: int,
                 gap_above_units: int = 0) -> bytes:
    """Encode a whole image as a stack of 24-pin ESC * bands.

    The image is horizontally centred on a page of `page_width_px`
    pixels (assumed at our 180 dpi density).

    gap_above_units is in 1/180 inch (one vertical unit at our density).
    """
    out = bytearray()
    # ESC J advances the vertical print position by n/180 inch.
    # n is one byte (0-255).
    if gap_above_units > 0:
        out.extend([0x1B, ord('J'), gap_above_units & 0xFF])

    # Centre the image horizontally. ESC $ takes the absolute
    # position in 1/60 inch units; convert pixels → 1/60 inch.
    # At 180 dpi: 1 pixel = 1/180 inch; 1/60 inch = 3 pixels.
    left_margin_px = max(0, (page_width_px - img.width) // 2)
    left_margin_60th = left_margin_px // 3

    for y_start in range(0, img.height, PINS_PER_BAND):
        # ESC $ x_60th 0 — re-centre at start of each row band.
        out.extend([0x1B, ord('$'),
                    left_margin_60th & 0xFF,
                    (left_margin_60th >> 8) & 0xFF])
        out.extend(encode_band(img, y_start))
        out.append(0x0D)  # CR — return to left margin for next band
        # Advance one band height = 24/180 inch = 24 units.
        out.extend([0x1B, ord('J'), PINS_PER_BAND])
    return bytes(out)


def main() -> int:
    """CLI entry point: parse args, encode each PNG, write the .bin."""
    if len(sys.argv) < 4:
        print(__doc__)
        return 1
    output_path = sys.argv[1]
    page_width_px = int(sys.argv[2])
    images = []
    for spec in sys.argv[3:]:
        path, width_str = spec.rsplit(':', 1)
        images.append((path, int(width_str)))

    stream = bytearray()
    # ESC @ — reset the printer.
    stream.extend([0x1B, ord('@')])
    for i, (path, max_w) in enumerate(images):
        img = load_one_bit(path, max_w)
        print(f"  {path}: resized to {img.width}x{img.height}", file=sys.stderr)
        # Add some breathing room between stacked images (20/180 inch).
        gap = 20 if i > 0 else 0
        stream.extend(encode_image(img, page_width_px,
                                   gap_above_units=gap))

    with open(output_path, 'wb') as f:
        f.write(stream)
    print(f"Wrote {len(stream)} bytes to {output_path}", file=sys.stderr)
    return 0


if __name__ == '__main__':
    sys.exit(main())
