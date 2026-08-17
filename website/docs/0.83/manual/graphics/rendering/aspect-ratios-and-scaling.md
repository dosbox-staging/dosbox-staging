# Aspect ratios & scaling

Computer monitors were traditionally 4:3 displays, much like old television
sets. However, modern flat-screen monitors are usually 16:9 or even wider, so
stretching DOS era graphics to fullscreen on a modern monitor would result in
a distorted image. There is a further complication: modern screens use square
pixels, but the most common DOS graphics modes use slighly elongated "tall
pixels".

DOSBox Staging displays all graphics with the correct proportions out-of-the
box, but this often results in the image not completely filling the screen.
Read on to understand why authentic video emulation and filling the screen are
mutually exclusive goals, and to learn about the available options if you want
to diverge from authentic emulation.


## Why is there a black border?

Most DOS games were designed for 4:3 CRT displays and used non-square pixels.
For example, the standard 320×200 VGA mode was intended to fill a 4:3 screen,
so its pixels are slightly taller than they are wide (20% taller, to be exact,
so with a **pixel aspect ratio (PAR)** of 1:1.2).

Therefore, when DOSBox Staging displays a 4:3 aspect ratio image while
preserving its original proportions, the unused space appears as black bars on
the sides. This is called **pillarboxing**.

{{ figure(
    "https://www.dosbox-staging.org/static/images/manual/monitor-aspect-ratios1.png",
    "Pillarboxing in action: black bars fill the extra space<br>when the aspect ratio of the screen and the image do not match",
    lightbox=False,
    style="width: 25rem; margin: 1.5rem 0;"
) }}

At first glance, this might seem strange for 320×200 VGA: 320×200 is 16:10
when we only consider the pixel dimensions of the image, which is fairly close
to 16:9. But 320×200 VGA was not designed to be displayed with square pixels.
Its pixels are taller than they are wide --- 20% taller, with a 1:1.2 (5:6)
ixel aspect ratio (PAR) --- so the image is intended to appear "stretched"
into a 4:3 aspect ratio rectangle.

The illustration below shows why this matters. With square pixels, a 320×200
image is too wide to fill a 4:3 display without leaving space below it. With
the intended 20% taller pixels, the same 320×200 image fills the 4:3 screen.

{{ figure(
    "https://www.dosbox-staging.org/static/images/manual/monitor-aspect-ratios2.png",
    "Left: 320&times;200 pixel image with square pixels on a 4:3 monitor --- there is some letterboxing below the image; Right: the same image with 20% taller pixels on the same monitor --- the image fills the screen completely.",
    lightbox=False,
    style="width: 25rem; margin: 1.5rem 0;"
) }}

TODO concrete game image examples

## Aspect ratio correction

Aspect ratio correction (controlled by the [`aspect`](#aspect) setting)
accounts for these non-square pixels when displaying the game on a modern
square-pixel screen. It is enabled by default so most games look exactly as
intended out-of-the-box.

A small number of games are better displayed with square pixels (`aspect =
square-pixels`). These are typically DOS ports of European games originally
developed for PAL Commodore Amiga home computers, where the original artwork
was designed for square pixels at 320×256. Examples include [Beneath a Steel
Sky](https://www.mobygames.com/game/386/beneath-a-steel-sky/), [Lure of the
Temptress](https://www.mobygames.com/game/1134/lure-of-the-temptress/),
[Another World](https://www.mobygames.com/game/564/out-of-this-world/),
[Flashback](https://www.mobygames.com/game/555/flashback-the-quest-for-identity/),
and the [Gobliiins](https://www.mobygames.com/game/1154/gobliiins/) series. A
tell-tale sign is artwork that looks vertically stretched with aspect ratio
correction enabled. See [Beneath a Steel
Sky](../../../getting-started/beneath-a-steel-sky.md#aspect-ratio-correction)
in the [Getting Started guide](../../../getting-started/index.md) for an
example.


!!! note "When pixels are not squares"

    The fact that older monitors were not widescreen is easy to forget, and
    older DOSBox versions did not enable aspect ratio correction by default.
    As a result, many modern screenshots and videos of DOS games are shown
    with the wrong proportions, particularly for 320×200 modes.

    Well, at least on today's internet --- if you check out any old computer
    magazine from the 1980s or 90s, most screenshots are shown in the correct
    aspect ratio (though occasionally the magazines got it wrong too).

    Pixels are perfectly square in 640×480 and most higher resolutions (1:1
    pixel aspect ratio), but a few other DOS video modes also use non-square
    pixels: 640×350 EGA (1:1.3714 PAR), 640×200 EGA (1:2.4 PAR), and 720×348
    Hercules (1:1.5517 PAR). DOSBox Staging handles these modes automatically.


!!! info "Weird pixel aspect ratios"

    Pixels are square (1:1 pixel aspect ratio, or PAR) in **640&times;480** and
    higher resolutions. A few other modes have their own non-square PARs:
    **640&times;350 EGA** (1:1.37 PAR), **640&times;200 EGA** (1:2.4 PAR), and
    **720&times;348 Hercules** (1:1.55 PAR). DOSBox Staging handles all of these
    automatically.


## Integer scaling

The `integer_scaling` setting (set to `auto` by default) constrains the
horizontal or vertical scaling factor to integer values when upscaling the
image. The correct aspect ratio is always maintained, so the other dimension's
scaling factor may become fractional.

The `vertical` setting avoids uneven scanlines and interference artifacts with
CRT shaders or the [deinterlacing](special-features.md#deinterlacing) feature.

For the built-in CRT shaders, `auto` is recommended instead (this is the
default). It enables vertical integer scaling only when a CRT shader is active
and allows a few additional scaling ratios for better use of available screen
space (e.g., 3.5x and 4.5x scaling factors). Moreover, it turns integer
scaling off automatically when it's safe to do so (e.g., for low-resolution
games in fullscreen on a 4K monitor).

The `horizontal` mode is included mainly for completeness, but can be useful
on low-resolution displays to maximise horizontal text sharpness in
text-heavy games.

You can disable integer scaling completely with the `off` setting. The higher
your monitor resolution, the less noticeable the effects of non-integer
scaling become with CRT shaders. If you play in fullscreen on a 4K screen, you
can generally disable integer scaling up to 640×480 with the CRT shaders
without noticeable adverse effects.

```ini
[render]
integer_scaling = off
```

On lower-resolution monitors or in windowed mode, you'll need to experiment
--- some combinations look fine, while others produce interference patterns.

Note that the [1080p special-case shaders](shaders.md#1080p-special-cases)
alter the effective source resolution for 320×200 and 640×480 content, which
affects the scaling ratios you'd otherwise expect.

CRT shaders generally need at least three times the vertical resolution of the
emulated video mode to produce the intended CRT effect. For example, 800×600
SVGA requires at least 1800 vertical pixels, or 2400 horizontal pixels because
it uses square pixels. If the viewport is smaller, DOSBox Staging disables CRT
emulation and falls back to the `sharp` shader.

You can see this behaviour by resizing the window with the default settings.
The image will "snap" between scaling ratios, and the log window will report
automatic shader switches. Because the adaptive CRT shaders also take the
viewport size into account, resizing the window or switching between
windowed and fullscreen modes can change the selected shader.

Integer scaling can also leave letterboxing, pillarboxing, or both around the
image. This is unavoidable when the image cannot be enlarged by a whole-number
factor while also filling the available space.

See [Why is there a black border?](#why-is-there-a-black-border) for an
explanation of why aspect-ratio correction can also leave black areas around
the image.

!!! note "About fractional scaling ratios"

    With the `sharp` shader, fractional scaling is not a problem as the
    interpolation band is at most 1 pixel wide at the edges, so the result remains
    sharp, especially at 1440p or 4K. With CRT shaders, non-integer horizontal
    scaling is practically a non-issue --- the CRT shading artifacts (i.e., the
    phosphor mask pattern) will effectively mask any minor unevenness even on
    low-resolution displays.


## Sharp pixels

If you prefer to disable the CRT emulation altogether to make the pixels look
like sharp little rectangeles, put this into your config:

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

This gives you perfectly sharp pixels vertically. Horizontally, there may be a
1-pixel-wide interpolation band at the edges of some pixels, depending on the
DOS video mode and scaling factor. This is an unavoidable consequence of
maintaining the correct aspect ratio while keeping the image as sharp as
possible.

At 1080p, the result is generally quite acceptable. At 1440p and 4K, the
minor horizontal interpolation is difficult to notice at normal viewing
distances.


## Custom viewport size

At fullscreen on a large modern display, low-resolution DOS graphics can look
much larger than they did on the monitors they were designed for.

14" VGA monitors were the most popular option until the mid-1990s. Before
that, CGA and EGA monitors were typically 12--14", while monochrome Hercules
monitors were usually 10--12". These are nominal sizes; the actual visible
area was roughly 1.5 to 2 inches smaller.

To approximate the physical image size of a typical 14" VGA monitor on a
modern 24" or larger widescreen display, you could restrict the output to
around 960×720. However, this gives a non-integer vertical scaling factor, so
1067×800 is a better choice:

```ini
[render]
viewport = 1067x800
```

The viewport size is specified in **logical units** (see the note below). You
can also specify it as a percentage of your desktop:

```ini
[render]
viewport = 89%
```

The video output is fitted inside the viewport while maintaining the correct
aspect ratio. 89% is a useful general-purpose value for DOS resolutions
between 320×200 and 640×480 on modern displays. With integer scaling enabled,
it gives the graphics the physical size roughly comparable to the CRTs they
were commonly displayed on.

Running DOS games fullscreen on a modern 24" widescreen display is roughly
equivalent to playing them on a 21" CRT --- a large professional monitor that
very few gamers owned. Low-resolution artwork was not designed to be viewed at
that size, the pixels would look too blocky.

!!! info "Logical units vs pixels"

    Most operating systems support high-DPI scaling, where window dimensions
    are specified in logical units rather than physical pixels. The viewport
    setting uses the same approach.

    For example, on a 4K display with a typical 200% scaling factor, a
    viewport of 1280&times;960 logical units corresponds to 2560&times;1920
    physical pixels. On a 1080p display with 100% scaling, it corresponds to
    1280&times;960 physical pixels.


!!! note "Why 89%?"

    `viewport = 89%` is based on the amount of vertical space available on a
    typical 1920&times;1080 desktop. It gives a maximum viewport of about 961
    logical pixels vertically.

    With integer scaling enabled, this is enough for a 2&times; vertical scale
    of both 320&times;200 content after VGA double-scanning (resulting in a
    640&times;400 image) and 640&times;480 content. The resulting image sizes
    are approximately 1067&times;800 and 1280&times;960 logical pixels
    respectively in fullscreen mode.

    On a 1080p display, these correspond directly to physical pixels. On a 4K
    display with 200% DPI scaling, they are rendered at twice the physical
    resolution while retaining the same apparent size.

    The result is roughly comparable to the physical image sizes of the CRTs
    commonly used with these resolutions: around 15" for 320&times;200 and
    around 19" for 640&times;480. This makes `89%` a useful general-purpose
    starting point.


## Custom aspect ratios

DOSBox Staging also provides a "stretch everything" mode for cases where
aspect ratio authenticity isn't the priority.

The `stretch` [`aspect`](#aspect) mode calculates the aspect ratio from the
viewport dimensions, allowing you to force arbitrary aspect ratios. For
example, to stretch a game to fill the entire screen:

```ini
[sdl]
fullscreen = on

[render]
aspect = stretch
viewport = fit
integer_scaling = off
```

This can make sense for text adventures or abstract games where the exact
shape of the graphics is less important. For games with carefully drawn
artwork, however, stretching will distort the image.

The `relative` viewport mode starts with a 4:3 rectangle and scales it
independently in the horizontal and vertical directions. This effectively
emulates the horizontal and vertical stretch controls found on CRT monitors.
It can also be useful for correcting games that use the wrong aspect ratio,
such as some Hercules conversions that reused EGA or VGA artwork with only a
minimal colour conversion applied.

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

You can adjust the stretch in real time with the **Stretch Axis**, **Inc
Stretch**, and **Dec Stretch** hotkey actions. Map these actions in the [key
mapper](../../input/keymapper.md), adjust the image until it looks right, then
copy the resulting viewport setting from the log into your config.


## Configuration settings

You can set these in the `[render]` configuration section.

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

