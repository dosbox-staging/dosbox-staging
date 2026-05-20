# Aspect ratios & scaling

For a short summary of the rendering section, see [Rendering overview](overview.md).

Most DOS games used non-square pixels and were designed for 4:3 CRT displays.
The standard 320&times;200 VGA mode fills a 4:3 screen completely, which is
only possible if each pixel is a slightly tall rectangle --- exactly 20% taller
than wide, giving a pixel aspect ratio (PAR) of 1:1.2 (or 5:6). You can derive
this from the display: 4:3 scales to 320:240, and 240 / 200 = 1.2.

Aspect ratio correction is enabled by default (`aspect = auto`) so games look
as intended. Without it, 320&times;200 content appears squished on modern
square-pixel displays.

A small number of DOS games need square pixels (`aspect = square-pixels`).
These are typically European games ported from the PAL Amiga, where the
original art was designed for square pixels at 320&times;256. Studios known
for this include Revolution Software
([Beneath a Steel Sky](https://www.mobygames.com/game/386/beneath-a-steel-sky/),
[Lure of the Temptress](https://www.mobygames.com/game/1134/lure-of-the-temptress/)),
Delphine Software ([Another World](https://www.mobygames.com/game/564/out-of-this-world/),
[Flashback](https://www.mobygames.com/game/555/flashback-the-quest-for-identity/)),
and Coktel Vision ([Gobliiins](https://www.mobygames.com/game/1154/gobliiins/)
series). The tell-tale sign is scanned or hand-drawn artwork that appears
vertically stretched with aspect ratio correction enabled.

Pixels are square (1:1 PAR) in 640&times;480 and higher resolutions. A few
other modes have their own non-square PARs: 640&times;350 EGA (1:1.37 PAR),
640&times;200 EGA (1:2.4 PAR), and 720&times;348 Hercules (1:1.55 PAR).
DOSBox Staging handles all of these automatically. For a more detailed
explanation of pixel aspect ratios, see [Why is there a black border?](#why-is-there-a-black-border).


## Integer scaling

The `integer_scaling` setting constrains the horizontal or vertical scaling
factor to integer values when upscaling the image. The correct aspect ratio is
always maintained, so the other dimension's scaling factor may become
fractional.

The `vertical` setting avoids uneven scanlines and interference artifacts with
CRT shaders or the [deinterlacing](special-features.md#deinterlacing) feature.
You could just use this, but there is an enhanced vertical scaling setting
called `auto` that works in tandem with the
[adaptive CRT shaders](shaders.md#adaptive-crt-shaders) (this is the default).
This mode activates vertical integer scaling only for the CRT shaders.

Note that the [1080p special-case shaders](shaders.md#1080p-special-cases)
alter the effective source resolution for 320&times;200 and 640&times;480
content, which affects the scaling ratios you'd otherwise expect.

Generally, the CRT shaders need at least 3 times the vertical resolution of
the emulated video mode to function. For example, the 800&times;600 SVGA mode
needs at least 1800 pixels vertically (and 2400 pixels horizontally, given
this mode has square (1:1) pixel aspect ratio). If the viewport is smaller,
DOSBox Staging will turn off CRT emulation and revert to the `sharp` shader.

You can test this by resizing your window with the default settings --- you'll
see the image "snap" between the integer scaling ratios, and you can see
messages about the automatic shader switches in the log window. You'll also
notice that the upscaled image might have some letterboxing, pillarboxing, or
both, depending on the window size. This is an unavoidable consequence of
integer scaling.

The `auto` mode has some refinements over the standard `vertical` mode: 3.5x
and 4.5x scaling factors are also allowed, and integer scaling is disabled
above 5.0x.

The `horizontal` integer scaling mode is included more for completeness' sake,
but it can be useful on low-resolution displays to maximise horizontal text
sharpness in text-heavy games.

!!! note "About fractional scaling ratios"

    With the `sharp` shader, fractional scaling is not a problem as the
    interpolation band is at most 1 pixel wide at the edges, which is sharp,
    especially at 1440p or 4K. With CRT shaders, non-integer horizontal
    scaling is practically a non-issue --- the CRT shading artifacts (i.e.,
    the phosphor mask pattern) will effectively mask any minor unevenness even
    on low-resolution displays.


## Why is there a black border?

Computer monitors were originally not widescreen but had a 4:3 display aspect
ratio, just like old television sets. The complete switch to 16:9 in computer
monitors and laptops happened only by the early 2010s. The low-resolution
320&times;200 VGA mode was designed to completely fill a 4:3 CRT screen. When
displaying such 4:3 content on a 16:9 modern flat panel, you'll get black bars
on the sides of the image --- this is called **pillarboxing**.

{{ figure(
    "https://www.dosbox-staging.org/static/images/manual/monitor-aspect-ratios1.png",
    "Pillarboxing in action: black bars fill the extra space<br>when the aspect ratio of the screen and the image do not match",
    lightbox=False,
    style="width: 25rem; margin: 1.5rem 0;"
) }}

Hang on --- something is not right here! 320:200 simplifies to 16:10, which is
close enough to 16:9 that the image should fill a widescreen monitor almost
completely, with only minor pillarboxing. That would be true if the pixels of
the 320&times;200 VGA mode were perfect squares --- but they're not.

As mentioned, 320&times;200 graphics completely fill the screen on a 4:3 CRT,
but that's only possible if the pixels are slightly tall rectangles rather than
squares. If you do the maths, they need to be exactly 20% taller than wide.
Here's how to derive it: 4:3 can be rewritten as 320:240 by multiplying both
sides by 80. Then 240 divided by 200 is 1.2, so the pixel aspect ratio (PAR)
is 1:1.2, or 5:6.

{{ figure(
    "https://www.dosbox-staging.org/static/images/manual/monitor-aspect-ratios2.png",
    "Left: 320&times;200 pixel image with square pixels on a 4:3 monitor --- there is some letterboxing below the image; Right: the same image with 20% taller pixels on the same monitor --- the image fills the screen completely.",
    lightbox=False,
    style="width: 25rem; margin: 1.5rem 0;"
) }}

But hey, not everybody likes maths, especially not in the middle of a gaming
session! Most of the time, you won't need to worry about aspect ratio
correctness because DOSBox Staging handles that automatically for you. There
is a small but significant number of games, though, where forcing square pixels
yields better results (one good example is
[Beneath a Steel Sky](../../../getting-started/beneath-a-steel-sky.md#aspect-ratio-correction)
from the [Getting Started guide](../../../getting-started/index.md)).

!!! note "When pixels are not squares"

    The fact that monitors weren't widescreen until around 2010 tends to be
    forgotten nowadays, and older DOSBox versions didn't perform aspect ratio
    correction by default, which certainly didn't help matters either. The
    unfortunate situation is that you're much more likely to encounter videos
    and screenshots of DOS games in the wrong aspect ratio (using square
    pixels) on the internet today --- at least for more recently created
    content. If you check out any old computer magazine from the 1980s or
    '90s, most screenshots are shown in the correct aspect ratio (though
    occasionally the magazines got it wrong too).

    In case you're wondering: pixels are perfectly square in 640&times;480 and
    higher resolutions (1:1 pixel aspect ratio), but there are a few more
    video modes with non-square pixels --- the 640&times;350 EGA mode (1:1.3714
    PAR), the 640&times;200 EGA mode (1:2.4 PAR), and the 720&times;348
    Hercules graphics mode (1:1.5517 PAR).


## Sharp pixels

_"Okay, enough blabbering about all those near-extinct, mythical cathode-ray
tube contraptions, grandpa. Can't you just give us sharp pixels and be done
with it?"_

Sure thing, kiddo. Just put this into your config:

```ini
[render]
shader = sharp
```

Integer scaling is disabled by default when the `sharp` shader is selected. To
re-enable it:

```ini
[render]
shader = sharp
integer_scaling = vertical
```

This gives you 100% sharp pixels vertically. Horizontally, there may be a
1-pixel-wide interpolation band at the edges of some pixels depending on the
DOS video mode and upscale factor --- but this is the best possible compromise
between correct aspect ratio, even pixel widths, and overall sharpness. At
1080p the result is quite acceptable, and from 1440p upwards you'd have to
press your nose against the screen to notice it.


## Custom viewport size

All well and good, but at fullscreen on a large modern display the graphics
can look like they were constructed from brightly coloured bricks.

14" VGA monitors were the most popular option until the mid-1990s. Before
that, CGA and EGA monitors were typically 12--14", and monochrome Hercules
monitors only 10--12" --- and those are nominal sizes, with the actual visible
area roughly 1.5 to 2 inches smaller.

To approximate the physical image size of a typical 14" VGA monitor on a
modern 24" widescreen display, you'd want to restrict the output to around
960&times;720 --- though that's a non-integer vertical scale factor, so it's
better to bump it to 1067&times;800:

```ini
[render]
viewport = 1067x800
```

The viewport size is specified in _logical units_ (more on that below). You
can also specify the viewport as a percentage of your desktop size:

```ini
[render]
viewport = 89%
```

The video output will be sized to fit within that rectangle while maintaining
the correct aspect ratio. Why 89%? It's a value that produces optimal image
sizes for DOS resolutions between 320&times;200 and 640&times;480 on most
common modern displays --- with integer scaling enabled, 320&times;200 content
will have roughly the physical size of a 15" CRT, and 640&times;480 that of a
19" monitor. Good upper limits that balance modern comfort with something
close to the original experience.

Running DOS games in fullscreen on a modern 24" widescreen is the equivalent
of playing them on a 21" CRT --- a huge professional monitor that very few
gamers ever owned. Low-resolution art was never meant to be seen that large.

!!! info "Logical units vs pixels"

    Most operating systems operate in high DPI mode using a 200% scaling
    factor on 4K displays, and use "unscaled" logical units for specifying
    window dimensions. The viewport setting adopts the same approach --- the
    viewport size must always be specified in logical units.

    For example, on a 4K display, a viewport resolution of 1280&times;960
    logical units will translate to 2560&times;1920 physical pixels (assuming
    a typical 200% scaling factor). On a 1080p display with 100% scaling, it
    will simply translate to 1280&times;960 pixels.


!!! danger "The maths behind 89% (for the curious)"

    This is hardcore mode --- only the brave and the mathematically inclined
    shall prevail! 🤓 🧠 📚

    - Assuming a 1920&times;1080 desktop resolution, `viewport = 89%`
      restricts the maximum viewport size to 1709&times;961 logical units.

    - The viewport size is a potential maximum. With integer scaling disabled
      and aspect ratio correction enabled, a 4:3 DOS image will come out at
      1281&times;961 logical units (because 1281 / 961 ≈ 4/3).

    - On a 1080p monitor this gives you 1281&times;961 pixels; on a 4K monitor
      with 200% DPI scaling, the resulting image will be 2562&times;1922
      pixels.

    - With integer scaling enabled and 320&times;200 VGA double-scanned to
      640&times;400, dividing 961 by 400 gives 2.4025 --- so a 2&times;
      vertical integer scaling factor is used. The resulting image will be
      1067&times;800 on 1080p and 2133&times;1600 on 4K, measuring about
      14.4" diagonally --- just above the viewable size of a typical 15" CRT.

    - For the 640&times;480 VGA mode, 961 / 480 ≈ 2.0021, so a 2&times;
      factor is used again. The resulting image will be 1280&times;960 on
      1080p and 2560&times;1920 on 4K, measuring roughly 17.3" diagonally
      --- almost exactly the viewable area of a typical 19" CRT.

    Q.E.D. :sunglasses:


## Custom aspect ratios

DOSBox Staging also has a "stretch everything" mode for when aspect ratio
authenticity isn't the priority.

The `stretch` aspect mode calculates the aspect ratio from the viewport
dimensions, allowing you to force arbitrary aspect ratios. For example, to
stretch a game to fill the entire screen:

```ini
[sdl]
fullscreen = on

[render]
aspect = stretch
viewport = fit
integer_scaling = off
```

It's hard to argue it matters much for text adventures or abstract games ---
though for anything with carefully drawn art, opinions vary.

The `relative` viewport mode starts from a 4:3 rectangle and scales it by
horizontal and vertical stretch factors. With this, you can emulate the
horizontal and vertical stretch controls of a CRT. This is useful for
aspect-correcting lazy Hercules conversions that reused EGA/VGA assets.

For example, to correct the [squashed
look](../adapters.md#hercules-graphics-card) of Hercules graphics in [Prince
of Persia](https://www.mobygames.com/game/196/prince-of-persia/):

```ini
[render]
aspect = stretch
viewport = relative 112% 173%
integer_scaling = off
```

{{ figure(
    "https://www.dosbox-staging.org/static/images/manual/pop-hercules-aspect-corrected.jpg",
    "Prince of Persia in Hercules mode with custom stretch factors<br>to make the image fill our 4:3 \"emulated CRT screen\"."
) }}

Use the **Stretch Axis**, **Inc Stretch**, and **Dec Stretch** hotkey actions
to adjust stretching in real-time (you'll need to map them in the [key
mapper](../../input/keymapper.md) first), then copy the logged viewport
setting to your config.


## Configuration settings


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
      Combined with the [`viewport`](#viewport) setting, this mode is useful
      to force arbitrary aspect ratios (e.g., stretching DOS games to
      fullscreen on 16:9 displays) and to emulate the horizontal and vertical
      stretch controls of CRT monitors.


##### integer_scaling

:   Constrain the horizontal or vertical scaling factor to the largest integer
    value so the image still fits into the viewport. The configured aspect
    ratio is always maintained according to the [`aspect`](#aspect) and
    [`viewport`](#viewport) settings, which may result in a non-integer
    scaling factor in the other dimension. If the image is larger than the
    viewport, the integer scaling constraint is auto-disabled (same as `off`).

    Possible values:

    - `auto` *default*{ .default } -- A special vertical mode auto-enabled
      only for the CRT shaders (see [`shader`](shaders.md#shader)). This mode
      has refinements over standard vertical integer scaling: 3.5x and 4.5x
      scaling factors are also allowed, and integer scaling is disabled above
      5.0x scaling.

    - `vertical` -- Constrain the vertical scaling factor to integer values.
      This is the recommended setting for 3rd party CRT shaders with scanline
      emulation to avoid uneven scanlines and interference artifacts. For the
      built-in CRT shaders, use `auto`. This mode is also recommended on
      low-resolution displays with
      [`deinterlacing`](special-features.md#deinterlacing) enabled.

    - `horizontal` -- Constrain the horizontal scaling factor to integer
      values. Might be useful on low-resolution displays to optimise for
      horizontal text sharpness.

    - `off` -- Apply no integer scaling constraint; the image fills the
      viewport while maintaining the configured aspect ratio.


##### viewport

:   Set the viewport size. This is the maximum drawable area; the video
    output is always contained within the viewport while taking the configured
    aspect ratio into account (see [`aspect`](#aspect)).

    Possible values:

    - `fit` *default*{ .default } -- Fit the viewport into the available
      window/screen. There might be padding (black areas) around the image
      with [`integer_scaling`](#integer_scaling) enabled.

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
      with [`aspect`](#aspect) set to `stretch` and to "zoom" into the image.
      This effectively emulates the horizontal and vertical stretch controls
      of CRT monitors.

    !!! note

        - Using `relative` mode with [`integer_scaling`](#integer_scaling)
          enabled could lead to surprising (but correct) results.

        - Use the `Stretch Axis`, `Inc Stretch`, and `Dec Stretch` hotkey
          actions to adjust the image size in `relative` mode in real-time,
          then copy the new settings from the logs into your config.


## Leftovers

The following content was present in one or both source files but didn't have
an obvious home in the restructured page. Review and decide whether to merge,
move, or cut.

---

### Note: higher-resolution displays and integer scaling off

From the old `aspect-ratios.md`:

> The higher your monitor resolution, the more you can get away with
> non-integer vertical scaling. If you play in fullscreen on a 4K screen, you
> can safely disable integer scaling up to 640&times;480 without any adverse
> effects:
>
> ```ini
> [render]
> integer_scaling = off
> ```
>
> On lower-resolution monitors or in windowed mode, you'll need to experiment
> --- some combinations look fine, others will produce interference patterns.

This is a useful practical tip. Consider merging it into the [Integer
scaling](#integer-scaling) section above, perhaps as a note box.
