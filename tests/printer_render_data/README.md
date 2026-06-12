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

The `Graphics_GridmongerLogo` test consumes a pre-encoded ESC/P
byte stream stored at
`inputs/gridmonger_picture.bin`. Source PNGs live in
`inputs/gridmonger_logo.png` and `inputs/warrior_emblem.png`. To
regenerate the `.bin` after changing the source images:

```sh
python3 -m venv /tmp/pil_env
/tmp/pil_env/bin/pip install Pillow
cd tests/printer_render_data
/tmp/pil_env/bin/python tools/png_to_escp.py \
    inputs/gridmonger_picture.bin \
    inputs/gridmonger_logo.png:600 \
    inputs/warrior_emblem.png:400
```

The width suffix (`:600`, `:400`) limits the resized pixel width;
height follows from aspect ratio. The script Floyd-Steinberg
dithers each image to 1-bit then packs it into 24-pin ESC * bands.
See the script's docstring for details.
