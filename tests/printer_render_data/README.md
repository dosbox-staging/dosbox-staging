# Printer render test fixtures

Reference PNG images for `tests/printer_render_tests.cpp`. Each test
feeds a known ESC/P byte sequence into the `Printer` class, renders
to PNG, and compares the output against the matching file in
`expected/`.

## Regenerating references

After an intentional behaviour change (a bug fix, a renderer update,
a font swap), the references need to be regenerated:

1. Delete the affected `.png` files in `expected/`.
2. Run the tests: `build/<preset>/tests/<dir>/dosbox_tests --gtest_filter='PrinterRenderTest.*'`.
3. The test harness writes the new candidates to `expected/` and
   fails — that's expected.
4. **Inspect the candidates visually.** They're committed-as-output
   and CI compares against them; a wrong reference passes forever.
5. `git add` the new references and commit.

## Tolerance

- Graphics-only tests (no font involvement) compare exactly.
- Text tests use a small per-pixel tolerance (~1-1.5%) to survive
  cross-platform Freetype version variance. The bundled Liberation
  fonts pin most of the variability, but anti-aliasing details
  still shift between Freetype releases.

## Picture-input tests

The `Graphics_Pc5150_Density*` tests replay a black-and-white
photograph (`inputs/PC5150_withwoman.jpg`) across every ESC * bit-image
density the printer supports: the six 8-pin modes (0, 1, 2, 3, 4, 6),
the five 24-pin modes (32, 33, 38, 39, 40), and the three 48-pin modes
(71, 72, 73). Each `inputs/PC5150_density<N>.bin` is the source JPEG
Floyd-Steinberg-dithered to 1-bit and packed as ESC * mode `<N>`.

To regenerate after changing the source image or the encoder:

```sh
python3 -m venv /tmp/pil_env
/tmp/pil_env/bin/pip install Pillow
cd tests/printer_render_data
/tmp/pil_env/bin/python tools/png_to_escp.py \
    inputs/PC5150_density39.bin \
    <page_width_px> \
    inputs/PC5150_withwoman.jpg:<max_width_px>
```

The script emits one density at a time. To regenerate the full set,
change the `DENSITY` constant at the top of `tools/png_to_escp.py`
between runs (and update the band-encoding for 8-pin and 48-pin
modes — the shipped script only encodes the 24-pin layout). See the
script's docstring for details.
