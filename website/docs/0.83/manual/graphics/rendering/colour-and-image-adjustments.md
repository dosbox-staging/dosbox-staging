# Colour & image adjustments

For a short summary of the rendering section, see [Rendering overview](overview.md).

The [image adjustment](#image-adjustments) controls --- brightness, contrast,
saturation, colour temperature, and more --- work much like the knobs on an
old CRT monitor. They're useful for fine-tuning the picture to your taste or
compensating for differences in display characteristics.

[CRT colour profiles](#crt-colour-profiles) go a step further, emulating the
distinct phosphor colours of different monitor types. On
[wide gamut displays](#wide-gamut-colour-accuracy) these profiles can reproduce
CRT colours that fall outside the standard sRGB colour space more accurately.

For Hercules and CGA mono machines, DOSBox Staging offers authentic
[monochrome display emulation](#monochrome-display-emulation) mimicking
classic amber, green, white, and paperwhite looks.


## CRT colour profiles

Different CRT monitors used different phosphor chemistries, giving each a
distinct colour character. DOSBox Staging can emulate these via the
[`crt_color_profile`](#crt_color_profile) setting.

- **`p22`** --- P22 phosphors were the most common in PC monitors, producing
  warmer, slightly desaturated colours. This is what most people saw when
  playing DOS games.

- **`smpte-c`** --- SMPTE-C broadcast standard phosphors are close to P22 but
  with tighter colour tolerances, used in professional video monitors.

- **`ebu`** --- EBU phosphors are the European broadcast standard, found in
  high-end professional monitors like the Sony BVM/PVM series.

- **`philips`** --- Philips home computer monitors (e.g., the Commodore 1084S)
  had distinctly warm, yellowish whites at roughly 6100K.

- **`trinitron`** --- Sony Trinitron monitors were known for punchy, vivid
  colours with a cool blue-white colour temperature around 9300K.

The `auto` setting picks the profile that matches the era: CGA and EGA games
get P22 (matching the monitors of that era), VGA games also get P22, composite
video gets SMPTE-C, and the arcade shaders use Philips. See the [Automatic
image adjustments](#automatic-image-adjustments) table for the full mapping.


## Monochrome display emulation

For `hercules` and `cga_mono` machine types, the
[`monochrome_palette`](#monochrome_palette) setting offers four classic
terminal looks:

- **`amber`** --- the warm orange-yellow glow of monitors like the IBM 5151
  with an amber phosphor. The most common monochrome display in offices and
  the most comfortable for extended reading.

- **`green`** --- the classic green phosphor look of the original IBM 5151
  green screen, a staple of the early PC era.

- **`white`** --- a cool blue-white typical of later monochrome VGA monitors.

- **`paperwhite`** --- the Hercules-era paperwhite phosphor, a warmer,
  slightly yellowish white that's easier on the eyes than pure white.

You can cycle through the available palettes via hotkeys during gameplay.


## Automatic image adjustments

When [`crt_color_profile`](#crt_color_profile),
[`color_temperature`](#color_temperature), or [`black_level`](#black_level)
are set to `auto`, DOSBox picks period-appropriate values for each video mode.
CGA and EGA games look like they did on the slightly warm, high-colour-temperature
monitors of the 1980s, while VGA games get the cooler, more neutral look of
90s PC monitors.

The selection logic depends on the shader mode:

- **`crt-auto`** selects settings based on the graphics standard of the
  current video mode (CGA, EGA, VGA, etc.), irrespective of the `machine`
  setting. EGA modes that reprogram the VGA's 18-bit DAC palette get VGA
  settings to reflect that they require VGA hardware.

- **`crt-auto-machine`** selects settings based on the emulated machine type
  configured via the `machine` setting.

- **`crt-auto-arcade`** and **`crt-auto-arcade-sharp`** always use fixed
  arcade monitor settings.

- **Regular shaders** (e.g., `sharp`, `bilinear`) select settings based on
  the emulated machine type.

In all modes, composite video always uses composite-specific settings
regardless of the machine type.

| Video mode      | CRT profile | Colour temp (K) | Black level |
|-----------------|-------------|:----------------:|:-----------:|
| Monochrome      | None        | 6500             | 0           |
| Composite       | SMPTE-C     | 6500             | 0.53        |
| CGA / PCjr      | P22         | 9300             | 0.65        |
| EGA / Tandy     | P22         | 9300             | 0.60        |
| VGA / SVGA      | P22         | 7800             | 0           |
| Arcade          | Philips     | 6500             | 0.50        |

!!! note

    The black level values shown are for DCI-P3 colour spaces with gamma
    2.6 or higher. For lower-gamma colour spaces (sRGB, Display P3), the
    black level is halved.


## Wide gamut colour accuracy

CRT monitors used phosphor coatings with colour gamuts that often fall outside
the sRGB colour space used by most modern displays. P22, Trinitron, and
Philips phosphors all have saturated primaries that sRGB cannot reproduce. A
wide gamut display (DCI-P3 or wider) lets DOSBox Staging render these colours
faithfully.

The practical benefit depends on your operating system:

- **macOS** always outputs in the Display P3 colour space. The OS handles the
  conversion to your monitor's actual colour profile automatically, giving you
  the most accurate results out of the box.

- **Windows and Linux** currently output sRGB. A DCI-P3 monitor still helps
  because the Philips and Trinitron profiles push colours toward the gamut
  boundary, but full accuracy requires macOS or a future colour-managed
  rendering path on these platforms.

If you have a wide gamut display, the `auto` defaults for
[`crt_color_profile`](#crt_color_profile) and
[`color_space`](#color_space) give you accurate colours out of the box. The
Philips and Trinitron profiles show the biggest difference on DCI-P3 versus
sRGB displays.


## Image adjustments

The image adjustment system emulates the controls of a CRT monitor. The
[`image_adjustments`](#image_adjustments) setting enables or disables all
adjustments. When disabled, the raw RGB values from the emulated video adapter
are displayed without any processing (except colour space conversion).

The available adjustments are:

- [`brightness`](#brightness) and [`contrast`](#contrast) emulate the
  corresponding CRT monitor knobs --- brightness sets the black point,
  contrast sets the white point.

- [`gamma`](#gamma) applies additional gamma correction relative to the
  emulated monitor's gamma.

- [`digital_contrast`](#digital_contrast) and [`saturation`](#saturation) are
  applied directly to the raw framebuffer RGB values, unlike the CRT-emulating
  brightness/contrast controls.

- [`color_temperature`](#color_temperature) adjusts the white point in Kelvin;
  [`color_temperature_luma_preserve`](#color_temperature_luma_preserve)
  controls how much luminosity is preserved during the adjustment.

- [`red_gain`](#red_gain), [`green_gain`](#green_gain), and
  [`blue_gain`](#blue_gain) adjust individual colour channel gain.

You can tweak these in real-time using
[hotkeys](../../appendices/shortcuts.md#image-adjustments) --- use
"Previous/Next Image Adjustment" to select a setting and "Increase/Decrease"
to adjust its value. The adjusted values are logged so you can copy them into
your config. Alternatively, use the `CONFIG -wc` DOS command to write the
current settings to a config file.


## CGA palette override

The 16-colour CGA/EGA RGBI palette can be overridden with alternative colour
interpretations via the `cga_colors` setting. This affects CGA/EGA-like modes
even on emulated VGA or Tandy graphics adapters.

Several built-in presets are available, including Amiga and Atari ST colours
for Sierra AGI games, various CGA/EGA monitor emulations, and Commodore 64
inspired colours. Custom palettes can also be specified.

The `tandy-warm` preset emulates colours as they appear on an actual Tandy
monitor, resulting in more subdued and pleasant colours --- especially
apparent on the greens.


## Configuration settings


##### image_adjustments

:   Enable image adjustments. When disabled, the image adjustment settings
    have no effect and raw RGB values are used for video output. Colour space
    conversion is always active and cannot be disabled (see
    [`color_space`](#color_space)).

    Possible values: `on` *default*{ .default }, `off`

    !!! note

        - Image adjustments only work in OpenGL output mode.

        - Adjustments are applied to rendered screenshots, but not to raw and
          upscaled screenshots and video captures.

        - Use the `PrevImageAdj` and `NextImageAdj` hotkeys to select an
          image adjustment setting and the `DecImageAdj` and `IncImageAdj`
          hotkeys to adjust settings in real-time. Copy the new settings from
          the logs into your config, or write a new config with `CONFIG -wc`.


##### crt_color_profile

:   Set a CRT colour profile for more authentic video output emulation.

    Possible values:

    <div class="compact" markdown>

    - `auto` *default*{ .default } -- Select an authentic colour profile
      appropriate for the currently active adaptive CRT shader, or the
      current machine type for regular shaders.

    - `none` -- Display raw colours without any colour profile transforms.
      This will result in inaccurate colours and gamma on modern displays.

    - `ebu` -- EBU standard phosphor emulation, used in high-end professional
      CRT monitors such as the Sony BVM/PVM series.

    - `p22` -- P22 phosphor emulation, most common in lower-end CRT monitors.

    - `smpte-c` -- SMPTE "C" phosphor emulation, the standard for American
      broadcast video monitors.

    - `philips` -- Philips CRT monitor colours typical to 15 kHz home
      computer monitors (e.g., the Commodore 1084S). Intended to be used with
      `color_temperature` set to 6500. Output looks yellowish due to the
      ~6100 K colour temperature baked into the profile. Needs a DCI-P3
      display for the most accurate results.

    - `trinitron` -- Typical Sony Trinitron CRT TV and monitor colours.
      Intended to be used with `color_temperature` set to 6500. Output looks
      blueish due to the ~9300 K colour temperature baked into the profile.
      Needs a DCI-P3 display for the most accurate results.

    </div>


##### brightness

:   Set the brightness of the video output (`45` by default). Valid range is
    0 to 100. Emulates the brightness control of CRT monitors that sets the
    black point; higher values raise the blacks.


##### contrast

:   Set the contrast of the video output (`65` by default). Valid range is 0
    to 100. Emulates the contrast control of CRT monitors that sets the white
    point; higher values raise the blacks (lower [`brightness`](#brightness)
    to compensate).


##### gamma

:   Set the gamma of the video output (`0` by default). Valid range is -50
    to 50. Additional gamma adjustment relative to the emulated monitor's
    gamma.


##### digital_contrast

:   Set the digital contrast of the video output (`0` by default). Valid
    range is -50 to 50. Unlike [`contrast`](#contrast), this is applied
    directly to the raw RGB values of the framebuffer image.


##### black_level

:   Raise the black level of the video output. Applied before
    [`brightness`](#brightness) and [`contrast`](#contrast), so it acts as a
    black level boost.

    Possible values:

    - `auto` *default*{ .default } -- Raise the black level for PCjr, Tandy,
      CGA and EGA video modes only for adaptive CRT shaders; use 0 for any
      other shader.

    - `<number>` -- Set the black level raise amount. Valid range is 0 to
      100. 0 does not raise the black level.

    !!! note

        Raising the black level adds visual interest to PCjr, Tandy, CGA, and
        EGA games by enhancing the "black scanline" emulation look.


##### saturation

:   Set the saturation of the video output (`0` by default). Valid range is
    -50 to 50. Applied directly to the raw RGB values of the framebuffer
    image, similarly to [`digital_contrast`](#digital_contrast).


##### color_temperature

:   Set the colour temperature (white point) of the video output.

    Possible values:

    - `auto` *default*{ .default } -- Select an authentic colour temperature
      appropriate for the currently active adaptive CRT shader, or the
      current machine type for regular shaders.

    - `<number>` -- Specify colour temperature in Kelvin (K). Valid range is
      3000 to 10000. 6500 K is the neutral point for most modern displays.
      Values below 6500 produce warmer colours; above 6500 produce cooler
      colours.


##### color_temperature_luma_preserve

:   Preserve image luminosity prior to colour temperature adjustment (`0` by
    default). Valid range is 0 to 100. 0 performs no luminosity preservation;
    100 fully preserves it. Values greater than 0 result in inaccurate colour
    temperatures in brighter shades, so keep this at 0 or close to it if your
    monitor is bright enough.


##### red_gain

:   Set gain factor of the red channel (`100` by default). Valid range is 0
    to 200. 100 means no change.


##### green_gain

:   Set gain factor of the green channel (`100` by default). Valid range is 0
    to 200. 100 means no change.


##### blue_gain

:   Set gain factor of the blue channel (`100` by default). Valid range is 0
    to 200. 100 means no change.


##### color_space

:   Set the colour space of the video output. Wide gamut colour spaces can
    reproduce CRT phosphor colours that fall outside the sRGB gamut --- see
    [Wide gamut colour accuracy](#wide-gamut-colour-accuracy) for details.

    On macOS, this is always `display-p3`; the OS performs the conversion to
    the colour profile set in your system settings. On Windows and Linux, the
    effective output is currently sRGB.

    Possible values:

    - `display-p3` -- Display P3 wide gamut colour space with 6500K white
      point and sRGB gamma.

    !!! note

        Colour space transforms are applied to rendered screenshots, but not
        to raw and upscaled screenshots and video captures (those are always
        in sRGB).


##### monochrome_palette

:   Set the palette for monochrome display emulation. Works only with the
    `hercules` and `cga_mono` machine types.

    Possible values: `amber` *default*{ .default }, `green`, `white`,
    `paperwhite`.

    !!! note

        You can also cycle through the available palettes via hotkeys.


##### cga_colors

:   Set the interpretation of CGA RGBI colours. Affects all machine types
    capable of displaying CGA or better graphics, including the PCjr, Tandy,
    and CGA/EGA modes on VGA adapters.

    Built-in presets:

    <div class="compact" markdown>

    - `default` *default*{ .default } -- The canonical CGA palette, as
      emulated by VGA adapters.

    - `tandy <bl>` -- Emulation of an idealised Tandy monitor with adjustable
      brown level (0--red, 50--brown, 100--dark yellow; defaults to 50).

    - `tandy-warm` -- Emulation of the actual colour output of an unknown
      Tandy monitor. Intended to be used with `crt_color_profile = none` and
      `color_temperature = 6500`.

    - `ibm5153 <c>` -- Emulation of the actual colour output of an IBM 5153
      monitor with a unique contrast control that dims non-bright colours
      only. The contrast can be optionally provided as a second parameter (0
      to 100; defaults to 100). Intended to be used with
      `crt_color_profile = none` and `color_temperature = 6500`.

    - `agi-amiga-v1`, `agi-amiga-v2`, `agi-amiga-v3` -- Palettes used by the
      Amiga ports of Sierra AGI games.

    - `agi-amigaish` -- A mix of EGA and Amiga colours used by the Sarien
      AGI-interpreter.

    - `scumm-amiga` -- Palette used by the Amiga ports of LucasArts EGA
      games.

    - `colodore` -- Commodore 64 inspired colours based on the Colodore
      palette.

    - `colodore-sat` -- Colodore palette with 20% more saturation.

    - `dga16` -- A modern take on the canonical CGA palette with dialled back
      contrast.

    </div>

    You can also set custom colours by specifying 16 space- or comma-separated
    sRGB colour values, either as 3 or 6-digit hex codes (e.g., `#f00` or
    `#ff0000` for full red), or decimal RGB triplets (e.g., `(255, 0, 255)`
    for magenta).

    !!! note

        These colours will be further adjusted by the video output settings
        (see [`crt_color_profile`](#crt_color_profile),
        [`brightness`](#brightness), [`saturation`](#saturation), etc.)


## Leftovers

Nothing identified as dropped from the original `rendering.md` colour and
image adjustment sections. If you spot something missing, note the section
title here for review.
