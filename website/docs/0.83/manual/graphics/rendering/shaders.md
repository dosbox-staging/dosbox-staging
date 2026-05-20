# Shaders

DOSBox Staging uses [adaptive CRT shaders](#adaptive-crt-shaders) by default
that emulate the look of period-appropriate monitors. A VGA game gets a
VGA-style CRT look, an EGA game gets an EGA-era monitor, and so on. The
results are very close to what these games looked like on the real hardware
they were designed for. If you prefer a crisp "sharp pixel" look without any
CRT emulation, set [`shader`](#shader) to `sharp`.


## Adaptive CRT shaders

DOSBox Staging includes adaptive CRT shaders that automatically select the
appropriate monitor emulation based on the current video mode:

- **`crt-auto`** *default*{ .default } --- Prioritises developer intent and
  how people experienced games at the time. VGA games appear double-scanned
  (as on a real VGA monitor), EGA games appear single-scanned with thicker
  scanlines, and so on, regardless of the
  [`machine`](../../system/general.md#machine) setting.

- **`crt-auto-machine`** --- Emulates a fixed CRT monitor for the video
  adapter configured via the [`machine`](../../system/general.md#machine)
  setting. CGA and EGA modes on a VGA machine always appear double-scanned
  with chunky pixels, as on a real VGA adapter.

- **`crt-auto-arcade`** --- A fantasy option that emulates a 15 kHz arcade or
  home computer monitor with thick scanlines in low-resolution modes. Fun for
  playing DOS VGA ports of Amiga and Atari ST games.

- **`crt-auto-arcade-sharp`** --- A sharper arcade variant that retains the
  thick scanlines but with the sharpness of a typical PC monitor.

If you're unsure which to use, leave it on `crt-auto`.

For a full explanation of how double scanning affects these shaders, see
[VGA double scanning](#vga-double-scanning).


### 1080p special cases

The adaptive CRT shaders normally require at least 3&times; the vertical
resolution of the emulated DOS video mode to display crisp scanlines without
wavy vertical interference patterns. On a 1080p display, a double-scanned
320&times;200 game has an effective height of 400 lines, which means the
highest usable integer scale is 2&times; in fullscreen, leaving substantial
black borders around a relatively small upscaled image. Two special shaders
activate automatically under `crt-auto` to improve the situation:

- **`vga-1080p-fake-double-scan`** --- Used for 320&times;200 content. Rather
  than treating the source image as 640&times;400, this shader treats it as
  320&times;200 and applies a higher integer scale, then overlays an
  alternating line pattern to simulate the double-scan look. It's not
  pixel-perfect (or rather, scanline-perfect), but the feel is authentic and
  the image fills the screen far better.

- **`vga-1080p`** --- Used for 640&times;480 content. This mode upscales at
  exactly 2&times; to 1280&times;960 with the same alternating line overlay.
  Not fake double scanning --- 640&times;480 is a square-pixel mode that
  doesn't double-scan --- just a purpose-built upscaler that makes the most
  of a 1080p viewport.

Both activate transparently under `crt-auto`. You can also set them explicitly
via the [`shader`](#shader) setting, though in most cases there's no reason
to.

!!! note

    Technically, these two special shaders are actived when then native pixels
    per emulated scaline ratio falls between 2.0 and 3.0 in the viewport. It's
    just easier to call them "1080p shaders" because they're most useful on
    1080p monitors in fullscreen.


## VGA double scanning

[VGA adapters](../adapters.md#vga-and-svga) are the most common for DOS gaming,
so it's essential to understand one of their important peculiarities.

VGA hardware **double-scans** any video mode with fewer than 400 lines --- it
draws each scanline twice, producing two physical scanlines per each
logical pixel row. This affects the most common DOS gaming resolutions:
320&times;200, 320&times;240, and 640&times;350 among others.

This is a fundamental difference from every graphics standard that came before
it. CGA and EGA monitors, home computer monitors, TVs, and arcade monitors
could all display "true" low-resolution video --- one scanline per pixel row,
exactly as the hardware produced it. On those displays, individual scanlines
were thick and clearly visible, giving low-resolution content its
characteristic chunky "fat scanline" look. VGA monitors physically could not
do this. The signal frequency of a true 320&times;200 mode is incompatible
with VGA monitors; double scanning was the only way to display it at all. With
each line drawn twice, the resulting analog signal is literally identical to a
true 640&times;400 mode --- a VGA monitor has no way to distinguish between
the two.

The scanlines on VGA monitors are vanishingly fine, closer in character to a
high-end broadcast monitor than to the bold scanline look of a CGA monitor or
arcade cabinet. That refined, almost scanline-free look *is* what DOS VGA
games looked like on real hardware. Instead of "chunky scanlines", VGA
monitors are known for their "chunky pixels" --- in low-resolution modes, each
320&times;200 "pixel" was drawn as a sharp 2-by-2 pixel rectangle at
640&times;400 resolution.

This means the double-scanned rendering of low-resolution VGA modes is not a
stylistic choice, not a DOSBox Staging quirk, and not something that can be
"fixed" --- it is the hardware reality of VGA. If you're coming from
experience with an Amiga, Atari ST, Commodore 64, a games console, or even a
CGA or EGA PC, this will feel unfamiliar. That's expected. VGA was genuinely
different.

DOSBox Staging replicates this faithfully, and the adaptive CRT shaders are
designed around it.

This has a direct consequence for [integer scaling](aspect-ratios-and-scaling.md#integer-scaling):
a 320&times;200 VGA game has an effective internal resolution of 640&times;400.
A 4&times; integer scale therefore produces a 2560&times;1600 image, not
1280&times;800 (but we have implemented some special quality-of-life
concessions for [1080p monitor users](#1080p-special-cases)).

If you prefer the thick scanline look of a CGA monitor or arcade cabinet ---
a look that was never actually available on VGA hardware --- the
[`crt-auto-arcade`](#adaptive-crt-shaders) and
[`crt-auto-arcade-sharp`](#adaptive-crt-shaders) shaders offer exactly that as
a deliberate fantasy option. It's particularly enjoyable with DOS ports of
Amiga and Atari ST games.

!!! note "Why VGA double-scans low-resolution modes"

    CGA and EGA monitors operated at a lower horizontal sync frequency than
    VGA monitors, which standardised on a fixed higher rate. Existing DOS
    games used low-resolution modes that fell below VGA's operating range, so
    IBM's VGA hardware needed a way to bridge the gap. The solution was
    elegant: draw each scanline twice, doubling the number of output lines per
    frame and lifting the signal into VGA's frequency range. The visual side
    effect --- two physical scanlines per logical pixel row --- gave
    low-resolution VGA games their characteristic "chunky pixel" appearance.

    The real reason for this solution was to simplify the implementation and
    lower the cost of (at that time) high-resolution VGA monitors. Early VGA
    monitors were fixed-sync devices. SVGA monitors eventually evolved into
    multi-sync displays, but this early design decision stuck --- most SVGA
    monitors never gained the ability to sync down to the 15 kHz horizontal
    rates necessary for displaying "true 200-line" content. This is why Amiga
    and Atari ST users needed to buy expensive scan doubler hardware to connect
    their computers to PC SVGA monitors.


## Shader presets

Shaders can be configured with presets that override their default settings
and parameters. Presets are specified using the `SHADER_NAME:PRESET_NAME`
format in the `shader` setting. If no preset is specified, the shader's
built-in defaults are used.

For example:

``` ini
[render]
shader = crt/crt-hyllian:vga-4k
```

The adaptive CRT shaders (`crt-auto`, `crt-auto-machine`, `crt-auto-arcade`,
`crt-auto-arcade-sharp`) automatically select the appropriate preset based on
the graphics standard and viewport resolution. For example, `crt-auto` might
resolve to `crt/crt-hyllian:cga-1080p` for a CGA game at 1080p, or
`crt/crt-hyllian:vga-4k` for a VGA game at 4K resolution. The `sharp` shader
is used as a fallback below 3x vertical scaling.

Preset files are INI-format files that can override shader settings and
parameters:

``` ini
[settings]
force_single_scan = yes
force_no_pixel_doubling = yes

[parameters]
BEAM_MIN_WIDTH     = 0.70
SCANLINES_STRENGTH = 0.65
```

The `[settings]` section can include:

<div class="compact" markdown>

- `force_single_scan` --- Force single scanning for double-scanned modes. See
  [VGA double scanning](#vga-double-scanning).
- `force_no_pixel_doubling` --- Disable pixel doubling. See
  [VGA double scanning](#vga-double-scanning).
- `linear_filtering` --- Enable or disable bilinear texture filtering.

</div>

The `[parameters]` section overrides shader-specific parameters declared in
the shader source.


## Configuration settings


##### shader

:   Set an adaptive CRT monitor emulation shader or a regular shader.
    Shaders are only supported in the OpenGL output mode (see
    [`output`](../display-and-window.md#output)).

    Adaptive CRT shader options:

    <div class="compact" markdown>

    - `crt-auto` *default*{ .default } -- Adaptive CRT shader that
      prioritises developer intent and how people experienced the games at
      the time of release. An appropriate shader variant is auto-selected
      based on the graphics standard of the current video mode and the
      viewport size, irrespective of the
      [`machine`](../../system/general.md#machine) setting.

    - `crt-auto-machine` -- A variation of `crt-auto`; this emulates a fixed
      CRT monitor for the video adapter configured via the
      [`machine`](../../system/general.md#machine) setting.

    - `crt-auto-arcade` -- Emulation of an arcade or home computer monitor
      with a less sharp image and thick scanlines in low-resolution video
      modes. This is a fantasy option that never existed in real life, but it
      can be a lot of fun, especially with DOS ports of Amiga games.

    - `crt-auto-arcade-sharp` -- A sharper arcade shader variant for those
      who like the thick scanlines but want to retain the sharpness of a
      typical PC monitor.

    </div>

    Other shader options include (non-exhaustive list):

    <div class="compact" markdown>

    - `sharp` -- Upscale the image treating the pixels as small rectangles,
      resulting in a sharp image with minimum blur while maintaining the
      correct pixel aspect ratio. Recommended for those who don't want CRT
      shaders.

    - `bilinear` -- Upscale the image using bilinear interpolation (results
      in a blurry image).

    - `nearest` -- Upscale the image using nearest-neighbour interpolation
      (also known as "no bilinear"). This results in the sharpest possible
      image at the expense of uneven pixels, especially with non-square pixel
      aspect ratios (this is less of an issue on high resolution monitors).

    </div>

    The following short aliases are also available: `sharp` (for
    `interpolation/sharp`), `bilinear` (for `interpolation/bilinear`),
    `nearest` (for `interpolation/nearest`).

    The bundled shaders include:

    - **Interpolation**: `sharp`, `bilinear`, `nearest`, `catmull-rom`

    - **CRT**: `crt-hyllian`,
      `vga-1080p`, `vga-1080p-fake-double-scan` (all used by the adaptive CRT
      shaders with different presets)

    - **Scaler**: `advinterp2x`, `advinterp3x`, `advmame2x`, `advmame3x`,
      `xbr-lv2-3d`, `xbr-lv2-noblend`, `xbr-lv3`

    Shaders are located in subdirectories; use the full path (e.g.,
    `interpolation/catmull-rom` or `scaler/xbr-lv3`). The `.glsl` extension
    can be omitted.

    !!! note

        Start DOSBox Staging with the
        [`--list-shaders`](../../using-dosbox-staging/command-line.md#-list-shaders)
        command line option to see the full list of available shaders. You can
        also use an absolute or relative path to a file. In all cases, you may
        omit the shader's `.glsl` file extension.

