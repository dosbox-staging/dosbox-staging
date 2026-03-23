# Rendering

DOSBox Staging uses adaptive CRT shaders by default that emulate the look
of period-appropriate monitors. A VGA game gets a VGA-style CRT look, an
EGA game gets an EGA-era monitor, and so on. The result is surprisingly
close to what these games looked like on the real hardware they were
designed for.

Most DOS games used non-square pixels — a 320×200 image was stretched to
fill a 4:3 CRT monitor. Aspect ratio correction is enabled by default so
games look as they were intended. Without it, everything appears slightly
squished, which is especially noticeable with pixel art-heavy games like
*Monkey Island* or *Loom*.

The image adjustment controls — brightness, contrast, saturation, colour
temperature — work much like the knobs on an old CRT monitor. They're
useful for fine-tuning the picture to your taste or compensating for
differences in display characteristics.

If you prefer a crisp, pixel-perfect look without any CRT emulation, set
`shader = sharp`. For completely unprocessed output, use `shader = none`.


## CRT shaders

DOSBox Staging includes adaptive CRT shaders that automatically select the
appropriate monitor emulation based on the current video mode:

- `crt-auto` (default) --- Prioritises developer intent and how people
  experienced games at the time. VGA games appear double-scanned (as on a real
  VGA monitor), EGA games appear single-scanned with thicker scanlines, and so
  on, regardless of the `machine` setting.

- `crt-auto-machine` --- Emulates a fixed CRT monitor for the video adapter
  configured via the `machine` setting. CGA and EGA modes on a VGA machine
  always appear double-scanned with chunky pixels, as on a real VGA adapter.

- `crt-auto-arcade` --- A fantasy option that emulates a 15 kHz arcade or home
  computer monitor with thick scanlines in low-resolution modes. Fun for
  playing DOS VGA ports of Amiga and Atari ST games.

- `crt-auto-arcade-sharp` --- A sharper arcade variant that retains the thick
  scanlines but with the sharpness of a typical PC monitor.

Here are the shaders in action at 4K resolution (click on images to view at
full size):

<div class="image-grid" markdown>

<figure markdown>
  <a class="glightbox" href="https://www.dosbox-staging.org/static/images/release-notes/0.81.0/gods.jpg" >
    ![Gods --- 320&times;200 VGA](https://www.dosbox-staging.org/static/images/release-notes/0.81.0/gods-small.jpg){ loading="lazy" .skip-lightbox }
  </a>

  <figcaption markdown>
  Gods --- 320&times;200 VGA
  </figcaption>
</figure>
<figure markdown>
  <a class="glightbox" href="https://www.dosbox-staging.org/static/images/release-notes/0.81.0/ultima-underworld.jpg" >
    ![Ultima Underworld: The Stygian Abyss -- 320&times;200 VGA](https://www.dosbox-staging.org/static/images/release-notes/0.81.0/ultima-underworld-small.jpg){ loading=lazy .skip-lightbox }
  </a>

  <figcaption markdown>
  Ultima Underworld: The Stygian Abyss ---<br>320&times;200 VGA
  </figcaption>
</figure>

</div>
<div class="image-grid" markdown>

<figure markdown>
  <a class="glightbox" href="https://www.dosbox-staging.org/static/images/release-notes/0.81.0/celtic-tales.jpg" >
    ![Celtic Tales: Balor of the Evil Eye -- 640&times;480 VGA](https://www.dosbox-staging.org/static/images/release-notes/0.81.0/celtic-tales-small.jpg){ loading=lazy .skip-lightbox }
  </a>

  <figcaption markdown>
  Celtic Tales: Balor of the Evil Eye --- 640&times;480 VGA
  </figcaption>
</figure>
<figure markdown>
  <a class="glightbox" href="https://www.dosbox-staging.org/static/images/release-notes/0.81.0/dark-seed.jpg" >
    ![Dark Seed --640&times;350 EGA](https://www.dosbox-staging.org/static/images/release-notes/0.81.0/dark-seed-small.jpg){ loading=lazy .skip-lightbox }
  </a>

  <figcaption markdown>
  Dark Seed --- 640&times;350 EGA
  </figcaption>
</figure>

</div>
<div class="image-grid" markdown>

<figure markdown>
  <a class="glightbox" href="https://www.dosbox-staging.org/static/images/release-notes/0.81.0/monkey-island.jpg" >
    ![The Secret of Monkey Island -- 320&times;200 EGA](https://www.dosbox-staging.org/static/images/release-notes/0.81.0/monkey-island-small.jpg){ loading=lazy .skip-lightbox }
  </a>

  <figcaption markdown>
  The Secret of Monkey Island --- 320&times;200 EGA
  </figcaption>
</figure>
<figure markdown>
  <a class="glightbox" href="https://www.dosbox-staging.org/static/images/release-notes/0.81.0/space-quest-iii.jpg" >
    ![Space Quest III: The Pirates of Pestulon -- 320&times;200 EGA](https://www.dosbox-staging.org/static/images/release-notes/0.81.0/space-quest-iii-small.jpg){ loading=lazy .skip-lightbox }
  </a>

  <figcaption markdown>
  Space Quest III: The Pirates of Pestulon --- 320&times;200 EGA
  </figcaption>
</figure>

</div>
<div class="image-grid" markdown>

<figure markdown>
  <a class="glightbox" href="https://www.dosbox-staging.org/static/images/release-notes/0.81.0/wizardry.jpg" >
    ![Wizardry: Proving Grounds of the Mad Overlord -- 320&times;200 CGA](https://www.dosbox-staging.org/static/images/release-notes/0.81.0/wizardry-small.jpg){ loading=lazy .skip-lightbox }
  </a>

  <figcaption markdown>
  Wizardry: Proving Grounds of the Mad Overlord --- 320&times;200 CGA
  </figcaption>
</figure>
<figure markdown>
  <a class="glightbox" href="https://www.dosbox-staging.org/static/images/release-notes/0.81.0/smoker.jpg" >
    ![Smoker by Fairfax --320&times;200 15kHz "arcade" shader](https://www.dosbox-staging.org/static/images/release-notes/0.81.0/smoker-small.jpg){ loading=lazy .skip-lightbox }
  </a>

  <figcaption markdown>
  Smoker by Fairfax ---<br>320&times;200 15kHz "arcade" shader
  </figcaption>
</figure>

</div>
<div class="image-grid" markdown>

<figure markdown>
  <a class="glightbox" href="https://www.dosbox-staging.org/static/images/release-notes/0.81.0/zak.jpg" >
    ![Zak McKracken and the Alien Mindbenders -- 320&times;200 CGA composite](https://www.dosbox-staging.org/static/images/release-notes/0.81.0/zak-small.jpg){ loading=lazy .skip-lightbox }
  </a>

  <figcaption markdown>
  Zak McKracken and the Alien Mindbenders ---<br>320&times;200 CGA composite
  </figcaption>
</figure>
<figure markdown>
  <a class="glightbox" href="https://www.dosbox-staging.org/static/images/release-notes/0.81.0/starblade.jpg" >
    ![Starblade -- 720&times;348 Hercules](https://www.dosbox-staging.org/static/images/release-notes/0.81.0/starblade-small.jpg){ loading=lazy .skip-lightbox }
  </a>

  <figcaption markdown>
  Starblade --- 720&times;348 Hercules
  </figcaption>
</figure>

</div>

If you don't like CRT shaders, set `shader` to `sharp` to get crisp
pixel-perfect output. The deprecated legacy CRT shaders are still available
as a [separate download](https://archive.org/download/dosbox-staging-legacy-svn-shaders/svn-shaders.zip).


## Integer scaling

The `integer_scaling` setting constrains the horizontal or vertical scaling
factor to integer values when upscaling the image. This avoids uneven
scanlines and interference artifacts with CRT shaders.

``` ini
[render]
integer_scaling = vertical
```

The correct aspect ratio is always maintained, so the other dimension's
scaling factor may become fractional. With the `sharp` shader, this is not a
problem as the interpolation band is at most 1 pixel wide at the edges---still
sharp, especially at 1440p or 4K. With CRT shaders, non-integer horizontal
scaling is practically a non-issue.

The default `auto` mode enables vertical integer scaling only for the adaptive
CRT shaders, with refinements: 3.5x and 4.5x scaling factors are also
allowed, and integer scaling is disabled above 5.0x.


## Aspect ratio & viewport

Most DOS games used non-square pixels and were designed for 4:3 CRT displays.
Aspect ratio correction is enabled by default (`aspect = auto`) so games look
as intended.

The `stretch` mode calculates the aspect ratio from the viewport dimensions,
allowing you to force arbitrary aspect ratios. For example, to stretch a game
to fill the entire screen:

``` ini
[sdl]
fullscreen = on

[render]
aspect = stretch
viewport = fit
integer_scaling = off
```

The `relative` viewport mode starts from a 4:3 rectangle and scales it by
horizontal and vertical stretch factors. This is useful for aspect-correcting
lazy Hercules conversions that reused EGA/VGA assets. For example, Prince of
Persia in Hercules mode:

``` ini
[render]
aspect = stretch
viewport = relative 112% 173%
integer_scaling = off
```

Use the _Stretch Axis_, _Inc Stretch_, and _Dec Stretch_ hotkey actions to
adjust stretching in real-time, then copy the logged viewport setting to your
config.

<div class="image-grid" markdown>

<figure markdown>
  ![Prince of Persia (default)](https://www.dosbox-staging.org/static/images/release-notes/0.81.0/pop-hercules.png){ loading=lazy }

  <figcaption markdown>
  Prince of Persia without any custom stretching
  </figcaption>
</figure>

<figure markdown>
  ![Prince of Persia (aspect ratio corrected)](https://www.dosbox-staging.org/static/images/release-notes/0.81.0/pop-hercules-aspect-corrected.jpg){ loading=lazy }

  <figcaption markdown>
  Prince of Persia with `aspect = stretch` and `viewport = relative 112% 173%`
  </figcaption>
</figure>

</div>


## CGA palette override

The 16-colour CGA/EGA RGBI palette can be overridden with alternative colour
interpretations via the `cga_colors` setting. This affects CGA/EGA-like modes
even on emulated VGA or Tandy graphics adapters.

Several built-in presets are available, including Amiga and Atari ST colours
for Sierra AGI games, various CGA/EGA monitor emulations, and Commodore 64
inspired colours. Custom palettes can also be specified.

**Defender of the Crown**

The `tandy-warm` preset emulates colours as they appear on an actual Tandy
monitor, resulting in more subdued and pleasant colours---especially apparent
on the greens.

<div class="image-grid" markdown>

<figure markdown>
  ![Defender of the Crown (default)](https://www.dosbox-staging.org/static/images/release-notes/0.79.0/dotc-default.png){ loading=lazy }

  <figcaption markdown>
  `cga_colors = default`
  </figcaption>
</figure>

<figure markdown>
  ![Defender of the Crown (tandy-warm)](https://www.dosbox-staging.org/static/images/release-notes/0.79.0/dotc-tandy-warm.png){ loading=lazy }

  <figcaption markdown>
  `cga_colors = tandy-warm`
  </figcaption>
</figure>

</div>


**Gold Rush!**

The Amiga/Atari ST ports of Sierra AGI games use a tweaked CGA palette with
improved skin tones and sky colours.

<div class="image-grid" markdown>

<figure markdown>
  ![Gold Rush! (default)](https://www.dosbox-staging.org/static/images/release-notes/0.79.0/gold-rush-default.png){ loading=lazy }

  <figcaption markdown>
  `cga_colors = default`
  </figcaption>
</figure>

<figure markdown>
  ![Gold Rush! (agi-amigaish)](https://www.dosbox-staging.org/static/images/release-notes/0.79.0/gold-rush-agi-amigaish.png){ loading=lazy }

  <figcaption markdown>
  `cga_colors = agi-amigaish`
  </figcaption>
</figure>

</div>


**Zak McKracken and the Alien Mindbenders**

Play the PC version with Tandy sound and Commodore 64 colours!

<div class="image-grid" markdown>

<figure markdown>
  ![Zak McKracken (default)](https://www.dosbox-staging.org/static/images/release-notes/0.79.0/zak-default.png){ loading=lazy }

  <figcaption markdown>
  `cga_colors = default`
  </figcaption>
</figure>

<figure markdown>
  ![Zak McKracken (colodore)](https://www.dosbox-staging.org/static/images/release-notes/0.79.0/zak-colodore.png){ loading=lazy }

  <figcaption markdown>
  `cga_colors = colodore`
  </figcaption>
</figure>

</div>


## Configuration settings

You can set the rendering parameters in the `[render]` configuration section.


### Aspect ratio & scaling

##### aspect

:   Set the aspect ratio correction mode.

    Possible values:

    - `auto` *default*{ .default }, `on` -- Apply aspect ratio correction
      for modern square-pixel flat-screen displays, so DOS video modes with
      non-square pixels appear as they would on a 4:3 display aspect ratio
      CRT monitor the majority of DOS games were designed for. This setting
      only affects video modes that use non-square pixels, such as 320x200
      or 640x400; square pixel modes (e.g., 320x240, 640x480, and 800x600)
      are displayed as-is.

    - `square-pixels`, `off` -- Don't apply aspect ratio correction; all DOS
      video modes will be displayed with square pixels. Most 320x200 games
      will appear squashed, but a minority of titles (e.g., DOS ports of PAL
      Amiga games) need square pixels to appear as the artists intended.

    - `stretch` -- Calculate the aspect ratio from the viewport's dimensions.
      Combined with the [viewport](#viewport) setting, this mode is useful to
      force arbitrary aspect ratios (e.g., stretching DOS games to fullscreen
      on 16:9 displays) and to emulate the horizontal and vertical stretch
      controls of CRT monitors.


##### integer_scaling

:   Constrain the horizontal or vertical scaling factor to the largest
    integer value so the image still fits into the viewport. The configured
    aspect ratio is always maintained according to the [aspect](#aspect) and
    [viewport](#viewport) settings, which may result in a non-integer scaling
    factor in the other dimension. If the image is larger than the viewport,
    the integer scaling constraint is auto-disabled (same as `off`).

    Possible values:

    - `auto` *default*{ .default } -- A special vertical mode auto-enabled
      only for the adaptive CRT shaders (see [shader](#shader)). This mode
      has refinements over standard vertical integer scaling: 3.5x and 4.5x
      scaling factors are also allowed, and integer scaling is disabled above
      5.0x scaling.
    - `vertical` -- Constrain the vertical scaling factor to integer values.
      This is the recommended setting for third-party shaders to avoid uneven
      scanlines and interference artifacts.
    - `horizontal` -- Constrain the horizontal scaling factor to integer
      values.
    - `off` -- No integer scaling constraint is applied; the image fills the
      viewport while maintaining the configured aspect ratio.


##### viewport

:   Set the viewport size. This is the maximum drawable area; the video
    output is always contained within the viewport while taking the configured
    aspect ratio into account (see [aspect](#aspect)).

    Possible values:

    - `fit` *default*{ .default } -- Fit the viewport into the available
      window/screen. There might be padding (black areas) around the image
      with [integer_scaling](#integer_scaling) enabled.

    - `WxH` -- Set a fixed viewport size in WxH format in logical units
      (e.g., `960x720`). The specified size must not be larger than the
      desktop. If it's larger than the window size, it will be scaled to fit
      within the window.

    - `N%` -- Similar to `WxH`, but the size is specified as a percentage of
      the desktop size.

    - `relative H% V%` -- The viewport is set to a 4:3 aspect ratio rectangle
      fit into the available window or screen, then is scaled by the H and V
      horizontal and vertical scaling factors (valid range is from 20% to
      300%). The resulting viewport is allowed to extend beyond the bounds of
      the window or screen. Useful to force arbitrary display aspect ratios
      with [aspect](#aspect) set to `stretch` and to "zoom" into the image.
      This effectively emulates the horizontal and vertical stretch controls
      of CRT monitors.

    !!! note

        - Using `relative` mode with [integer_scaling](#integer_scaling)
          enabled could lead to surprising (but correct) results.

        - Use the `Stretch Axis`, `Inc Stretch`, and `Dec Stretch` hotkey
          actions to adjust the image size in `relative` mode in real-time,
          then copy the new settings from the logs into your config.


### Shaders

##### shader

:   Set an adaptive CRT monitor emulation shader or a regular shader.
    Shaders are only supported in the OpenGL output mode (see
    [output](../graphics/display-and-window.md#output)).

    Adaptive CRT shader options:

    <div class="compact" markdown>

    - `crt-auto` *default*{ .default } -- Adaptive CRT shader that
      prioritises developer intent and how people experienced the games at
      the time of release. An appropriate shader variant is auto-selected
      based on the graphics standard of the current video mode and the
      viewport size, irrespective of the `machine` setting.
    - `crt-auto-machine` -- A variation of `crt-auto`; this emulates a fixed
      CRT monitor for the video adapter configured via the `machine` setting.
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
      correct pixel aspect ratio. This is the recommended option for those
      who don't want to use the adaptive CRT shaders.
    - `bilinear` -- Upscale the image using bilinear interpolation (results
      in a blurry image).
    - `nearest` -- Upscale the image using nearest-neighbour interpolation
      (also known as "no bilinear"). This results in the sharpest possible
      image at the expense of uneven pixels, especially with non-square pixel
      aspect ratios (this is less of an issue on high resolution monitors).
    - `jinc2` -- Upscale the image using jinc 2-lobe interpolation with
      anti-ringing. This blends together dithered colour patterns at the cost
      of image sharpness.

    </div>

    !!! note

        Start DOSBox Staging with the `--list-shaders` command line option to
        see the full list of available shaders. You can also use an absolute
        or relative path to a file. In all cases, you may omit the shader's
        `.glsl` file extension.


### Image adjustments

##### brightness

:   Set the brightness of the video output (`45` by default). Valid range is
    0 to 100. This emulates the brightness control of CRT monitors that sets
    the black point; higher values will result in raised blacks.


##### contrast

:   Set the contrast of the video output (`65` by default). Valid range is 0
    to 100. This emulates the contrast control of CRT monitors that sets the
    white point; higher values will result in raised blacks (lower the
    [brightness](#brightness) control to compensate).


##### digital_contrast

:   Set the digital contrast of the video output (`0` by default). Valid
    range is -50 to 50. This works very differently from the
    [contrast](#contrast) virtual monitor setting; digital contrast is applied
    to the raw RGB values of the framebuffer image.


##### gamma

:   Set the gamma of the video output (`0` by default). Valid range is -50
    to 50. This is additional gamma adjustment relative to the emulated
    virtual monitor's gamma.


##### saturation

:   Set the saturation of the video output (`0` by default). Valid range is
    -50 to 50. This is digital saturation applied to the raw RGB values of
    the framebuffer image, similarly to [digital_contrast](#digital_contrast).


##### black_level

:   Raise the black level of the video output. It is applied before the
    [brightness](#brightness) and [contrast](#contrast) settings which can
    also raise the black level, so it effectively acts as a black level
    boost.

    Possible values:

    - `auto` *default*{ .default } -- Raise the black level for PCjr, Tandy,
      CGA and EGA video modes only for adaptive CRT shaders; for any other
      shader, use 0.
    - `<number>` -- Set the black level raise amount. Valid range is 0 to
      100. 0 does not raise the black level.

    !!! note

        Raising the black level is useful for "black scanline" emulation;
        this adds visual interest to PCjr, Tandy, CGA, and EGA games with
        simple graphics.


##### red_gain

:   Set gain factor of the video output's red channel (`100` by default).
    Valid range is 0 to 200. 100 results in no gain change.


##### green_gain

:   Set gain factor of the video output's green channel (`100` by default).
    Valid range is 0 to 200. 100 results in no gain change.


##### blue_gain

:   Set gain factor of the video output's blue channel (`100` by default).
    Valid range is 0 to 200. 100 results in no gain change.


##### image_adjustments

:   Enable image adjustments. When disabled, the image adjustment settings in
    the render section (e.g., [crt_color_profile](#crt_color_profile),
    [brightness](#brightness), [contrast](#contrast), etc.) have no effect
    and the raw RGB values are used for the video output. The colour space
    conversion is always active, that cannot be disabled (see
    [color_space](#color_space)).

    Possible values: `on` *default*{ .default }, `off`

    !!! note

        - Image adjustments only work in OpenGL output mode.

        - Adjustments are applied to rendered screenshots, but not to raw and
          upscaled screenshots and video captures.

        - Use the `PrevImageAdj` and `NextImageAdj` hotkeys to select an
          image adjustment setting and the `DecImageAdj` and `IncImageAdj`
          hotkeys to adjust the settings in real-time. Copy the new settings
          from the logs into your config, or write a new config with the
          `CONFIG -wc` command.


##### color_space

:   Set the colour space of the video output. On macOS, this is always
    `display-p3`; the OS performs the conversion to the colour profile set in
    your system settings.

    Possible values:

    - `display-p3` -- Display P3 wide gamut colour space with 6500K white
      point and sRGB gamma.

    !!! note

        Colour space transforms are applied to rendered screenshots, but not
        to raw and upscaled screenshots and video captures (those are always
        in sRGB).


##### color_temperature

:   Set the colour temperature (white point) of the video output.

    Possible values:

    - `auto` *default*{ .default } -- Select an authentic colour temperature
      for adaptive CRT shaders; for any other shader, use 6500.
    - `<number>` -- Specify colour temperature in Kelvin (K). Valid range is
      3000 to 10000. The Kelvin value only makes sense if
      [crt_color_profile](#crt_color_profile) is set to `none` or to one of
      the profiles with 6500K white point, otherwise it acts as a relative
      colour temperature adjustment (less than 6500 results in warmer
      colours, more than 6500 in cooler colours).


##### color_temperature_luma_preserve

:   Preserve image luminosity prior to colour temperature adjustment (`0` by
    default). Valid range is 0 to 100. 0 doesn't perform any luminosity
    preservation, 100 fully preserves the luminosity. Values greater than 0
    result in inaccurate colour temperatures in the brighter shades, so it's
    best to set this to 0 or close to 0 if your monitor is bright enough.


### Color palettes

##### cga_colors

:   Set the interpretation of CGA RGBI colours. Affects all machine types
    capable of displaying CGA or better graphics, including the PCjr, the
    Tandy, and CGA/EGA modes on VGA adapters.

    Built-in presets:

    <div class="compact" markdown>

    - `default` *default*{ .default } -- The canonical CGA palette, as
      emulated by VGA adapters.
    - `tandy <bl>` -- Emulation of an idealised Tandy monitor with adjustable
      brown level. The brown level can be provided as an optional second
      parameter (0--red, 50--brown, 100--dark yellow; defaults to 50).
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

    You can also set custom colours by specifying 16 space or comma separated
    sRGB colour values, either as 3 or 6-digit hex codes (e.g., `#f00` or
    `#ff0000` for full red), or decimal RGB triplets (e.g., `(255, 0, 255)`
    for magenta).

    !!! note

        These colours will be further adjusted by the video output settings
        (see [crt_color_profile](#crt_color_profile),
        [brightness](#brightness), [saturation](#saturation), etc.)


##### monochrome_palette

:   Set the palette for monochrome display emulation. Works only with the
    `hercules` and `cga_mono` machine types.

    Possible values: `amber` *default*{ .default }, `green`, `white`,
    `paperwhite`.

    !!! note

        You can also cycle through the available palettes via hotkeys.


##### crt_color_profile

:   Set a CRT colour profile for more authentic video output emulation. All
    profiles have a built-in colour temperature (white point) that you can
    tweak further with the [color_temperature](#color_temperature) setting.

    Possible values:

    <div class="compact" markdown>

    - `auto` *default*{ .default } -- Select an authentic colour profile for
      adaptive CRT shaders; for any other shader, use `none`.
    - `none` -- Display raw colours without any colour profile transforms.
    - `ebu` -- EBU standard phosphor emulation, used in high-end professional
      CRT monitors, such as the Sony BVM/PVM series (6500K white point).
    - `p22` -- P22 phosphor emulation, the most commonly used in lower-end
      CRT monitors (6500K white point).
    - `smpte-c` -- SMPTE "C" phosphor emulation, the standard for American
      broadcast video monitors (6500K white point).
    - `philips` -- Philips CRT monitor colours typical to 15 kHz home
      computer monitors, such as the Commodore 1084S (~6100K white point).
      Needs a wide gamut DCI-P3 display for the best results.
    - `trinitron` -- Typical Sony Trinitron CRT TV and monitor colours
      (~9300K white point). Needs a wide gamut DCI-P3 display for the best
      results.

    </div>


### Other

##### deinterlacing

:   Remove black lines from interlaced videos. Use with games that display
    video content with alternating black lines. This trick worked well on CRT
    monitors to increase perceptual resolution while saving storage space, but
    it resulted in brightness loss.

    Possible values:

    <div class="compact" markdown>

    - `off` *default*{ .default } -- Disable deinterlacing.
    - `on` -- Enable deinterlacing at `medium` strength.
    - `light` -- Light deinterlacing. Black scanlines are softened to mimic
      the CRT look.
    - `medium` -- Medium deinterlacing. Best balance between removing black
      lines, increasing brightness, and keeping the higher resolution look.
    - `strong` -- Strong deinterlacing. Image brightness is almost completely
      restored at the expense of diminishing the higher resolution look.
    - `full` -- Full deinterlacing. Completely removes black lines and
      maximises brightness, but the image will appear blockier.

    </div>

    !!! note

        Enabling vertical [integer_scaling](#integer_scaling) is recommended
        on lower resolution displays to avoid interference artifacts when
        using lower deinterlacing strengths. Alternatively, use `full`
        strength to completely eliminate all potential interference patterns.
