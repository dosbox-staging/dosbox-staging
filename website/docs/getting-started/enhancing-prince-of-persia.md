# Enhancing Prince of Persia


## Fullscreen

You can toggle between windowed and fullscreen mode any time by pressing
<kbd>Alt</kbd>+<kbd>Enter</kbd>. But what if you always want to play the game
in fullscreen? Wouldn't it be nice to configure DOSBox to start in fullscreen
mode?

You can easily do that by setting `fullscreen = on` in the `[sdl]` section.
Modify your `dosbox.conf` file so it has the following content, then restart
DOSBox.


```ini
[sdl]
fullscreen = on

[autoexec]
c:
prince

```

!!! note "Toggling options"

    For configuration parameters that toggle an option, you can use **`on`**,
    **`yes`**, **`true`** or **`1`** for turning the option on, and **`off`**,
    **`no`**, **`false`** or **`0`** for turning it off.

!!! important "Making configuration changes"

    You need to quit and restart DOSBox after updating `dosbox.conf` for the
    changes to take effect. We will not mention this every single time from
    now on.


## Viewport resolution

That's nice, but depending on the size of your monitor, 320&times;200 VGA
graphics displayed in fullscreen on a 24" or 27" inch LCD display might appear
overly blocky. If only we could control the size of the image in fullscreen as
easily as we can in windowed mode!

Well, it turns out we can! You can restrict the viewport size by adding the
`viewport_resolution` parameter to the `[sdl]` section. In this case, we're
restricting the viewport to a maximum size of 1120 by 840 pixels. The emulated
video output will be sized so it fits within the bounds of this rectangle
while keeping the correct aspect ratio.

```ini
[sdl]
fullscreen = on
viewport_resolution = 1120x840
```

We're effectively applying a 3.5&times; scaling factor here to the
320&times;200 VGA graphics:

<div class="compact" markdown>
- 320 &times; 3.5 = 1120
- and 200 &times; 3.5 = 700
</div>

Huh, what? Where's the `840` in our config coming from then?

Computer monitors were originally *not* widescreen but had a 4:3 aspect ratio,
just like old TVs. The complete switch to 16:9 in computer monitors and
laptops happened only by the early 2010s. The low-resolution 320&times;200 VGA
mode was designed to completely fill a 4:3 monitor, but that's only possible if the pixels
are not square, but slightly taller. If you do the maths, it turns out they
need to be exactly 20% taller for the 320&times;200 VGA screen mode (1:1.2
pixel aspect ratio). 700 &times; 1.2 = 840, so that's the explanation.

<figure markdown>
  ![Prince of Persia in fullscreen with 1120&times;840 viewport resolution restriction](images/pop-viewport.png){ loading=lazy }

  <figcaption markdown>
  Prince of Persia in fullscreen with 1120&times;840 viewport resolution restriction
  </figcaption>
</figure>

In any case, you don't need to worry about aspect ratio correctness because
DOSBox handles that automatically for you out-of-the-box. It's just good to
have some level of understanding of what's really happening. If you specify
`1120x700`, that would also work, but because aspect ratio correction is
enabled by default, and the image must fit within the bounds of the
specified viewport rectangle, the dimensions of the resulting image will be
933 by 700 pixels (because 700 &times; 4/3 = 933).

But hey, not everybody likes maths, especially in the middle of a DOS gaming
session, so you can also specify the viewport resolution as a percentage of your
desktop resolution:

```ini
viewport_resolution = 80%
```

!!! note "When pixels are not square"

    Sadly, this very important fact of computing history that monitors were
    not widescreen until about 2010 tends to be forgotten nowadays. Older
    DOSBox versions did not perform aspect ratio correction by default, which
    certainly did not help matters either. The unfortunate situation is that
    you're much more likely to encounter videos and screenshots of DOS games
    in the wrong aspect ratio (using square pixels) on the Internet today.
    Well, that's only true for more recently created content---if you check
    out any computer magazine from the 80s and the 90s, all screenshots are
    shown in the correct aspect ratio, of course.

    In case you're wondering, pixels are completely square in 640&times;480
    and higher resolutions (1:1 pixel aspect ratio), but there exist a few
    more weird screen modes with non-rectangular pixels, e.g., the
    640&times;350 EGA mode with 1:1.3714 pixel aspect ratio.


## Changing the display adapter

DOSBox emulates an SVGA (Super VGA) display adapter by default, which gives
you good compatibility with most DOS games. DOSBox can also emulate
all common displays adapters from the history of the PC compatibles: the
Hercules, CGA, and EGA display adapters, among many others.

Although the VGA standard was introduced in 1987, it took a good five years
until it gained widespread adoption. Games had to support all commonly used
graphical standards during this transitional period.

Prince of Persia released in 1990 is such a game; it not only supports all
common display adapters, but it also correctly auto-detects them. This is in
contrast with the majority of games where you need to configure the graphics
manually.


### EGA

To run Prince of Persia in EGA mode, you simply need to tell DOSBox to emulate
a machine equipped with an EGA adapter. That can be easily done by adding the
following configuration snippet:

```ini
[sdl]
machine = ega
```

This is how the game looks with the fixed 16-colour EGA palette:

<figure markdown>
  ![](images/pop-ega.png){ loading=lazy }

  <figcaption markdown>
  Prince of Persia in EGA mode
  </figcaption>
</figure>

That's nice, but it's not exactly how the graphics looked on an EGA monitor
back in the day. Pre-VGA monitors had visible scanlines, and we can emulate
that by enabling an OpenGL shader that emulates CRT displays of the era:

```ini
[render]
glshader = crt/aperture
```

This adds some texture and detail to the image that's especially apparent on
the solid-coloured bricks:

<figure markdown>
  ![](images/pop-ega-crt.png){ loading=lazy }

  <figcaption markdown>
  Prince of Persia in EGA mode using the `crt/aperture` shader
  </figcaption>
</figure>

If you have a 4k display, you might want to try the `crt/fakelottes.tweaked`
shader. Another good one is `crt/geom.tweaked`.

The `crt/zfast` shader is a good choice for older or integrated graphics
cards, and the `crt/pi` was specifically optimised for the Raspberry Pi.

You can view the full list of included CRT shaders
[here](https://github.com/dosbox-staging/dosbox-staging/tree/main/contrib/resources/glshaders/crt).


### CGA

The notorious CGA adapter, the first colour graphics adapter created for the
original IBM PC in 1981, is a serious contender for the worst graphics
standard ever invented. But no matter; the game supports it, so we'll have a
look at it. Make the following adjustment to the `machine` setting, and don't
forget to put on your safety glasses! :sunglasses:

```ini
[dosbox]
machine = cga
```

Ready? Behold the formidable 4-colour CGA graphics---and what 4 colours those
are!

<figure markdown>
  ![](images/pop-cga-crt.png){ loading=lazy }

  <figcaption markdown>
  Prince of Persia in CGA mode using the `crt/aperture` shader
  </figcaption>
</figure>


### Hercules

The Hercules display adapter only supports monochrome graphics, but at a higher
720&times;348 resolution. Can you guess how you can enable it?

```ini
[dosbox]
machine = hercules
```

<figure markdown>
  ![](images/pop-hercules-crt-white.png){ loading=lazy }

  <figcaption markdown>
  Prince of Persia in Hercules mode using the `white` palette<br>and the
  `crt/aperture` shader
  </figcaption>
</figure>


Monochrome monitors came in different colours; DOSBox can emulate all these
variations as well via the `monochrome_palette` configuration parameter. The
available colours are `white` (the default), `paperwhite`, `green`, and
`amber`. You can also switch between them while the game is running with the
<kbd>F11</kbd> key.

```ini
[dosbox]
machine = hercules
monochrome_palette = amber
```

The slightly squashed aspect ratio is correct in this case; this is exactly
how the game would look on a PC equipped with a Hercules card connected to a
period-accurate monochrome monitor.

<div class="image-grid" markdown>

<figure markdown>
  ![](images/pop-hercules-crt-amber.png){ loading=lazy }

  <figcaption markdown>
  Hercules mode using the `amber` palette
  </figcaption>
</figure>

<figure markdown>
  ![](images/pop-hercules-crt-green.png){ loading=lazy }

  <figcaption markdown>
  Hercules mode using the `green` palette
  </figcaption>
</figure>

</div>


!!! note "Only for masochists and archeologists!"

    The art in DOS games was usually created for the best graphics standard
    the game supported, then variants for "lesser" standards
    were derived from that. If you were an unlucky person still stuck with a
    Hercules or CGA adapter in 1990, being able to play Prince of Persia with
    *any* graphics on your PC surely beat no game at all!

    Although we've shown how to emulate these earlier graphics standards for
    completeness' sake, there's generally little reason for *not* playing a
    DOS with the best graphics. The list of possible reasons include: a)
    strong nostalgic feelings towards a particular display adapter, b)
    research reasons, c) you like your games to look like the user interface
    of an industrial CNC machine, d) a preference for pain. But hey, who are
    we to judge? :laughing:

    In any case, preserving all relevant aspects of PC gaming history is
    important for the DOSBox project, so these display options are always at
    your fingertips, should you need them.


## Authentic VGA emulation

If you're the lucky owner of a high-resolution monitor with at least 2000
pixels of native vertical resolution (4K or better monitors belong to this
category), you can enable authentic VGA emulation that more accurately
resembles the output of a CRT VGA monitor.

VGA display adapters have a peculiarity that they display not one but *two*
scanlines per pixel. This means the 320&times;200 low-resolution VGA
mode is really 640&times;400, just pixel and line-doubled.

We can turn on this special line-doubling behaviour by selecting the `vgaonly`
machine type. Additionally, we'll use a CRT shader and exact 4-times scaling
by setting the viewport resolution to 1280&times;960. This is important for
getting even scanlines, and it doesn't work on regular 1080p monitors because
there's simply not enough vertical resolution for the shader to work with.

!!! note "Logical vs physical pixels"

    Wait a minute, 1280&times;960 fits into an 1920x1080 screen, doesn't it?
    Why did you say we need a 4K display for this then?

    Most operating systems operate in high-DPI mode using a 200% scaling
    factor when a 4K display is connected. This means that the pixel
    dimensions used by the applications no longer represent *physical pixels*,
    but rather *logical* or *device-independent pixels*. On a 4K monitor,
    assuming a typical 200% scaling factor, our viewport resolution specified
    in 1280&times;960 *logical pixels* will translate to 2560&times;1920
    *physical pixels*. A vertical resolution of 1920 pixels is certainly
    sufficient for displaying those 400 scanlines; the shader can use 4.8
    pixels per scanline to do its magic.


These are the relevant configuration settings to enable authentic
double-scanned VGA emulation:

```ini
[dosbox]
machine = vgaonly

[sdl]
viewport_resolution = 1280x960

[render]
glshader = crt/aperture
```

TODO pictures


## Sound enhancements

We've been discussing graphics at great lenghts, but what about sound? Can't
we do something cool to the sound as well?

Yes, we can! DOSBox Staging has an exciting feature to add chorus and reverb
effects to the the output of any of the emulated sound devices.

Create a new `[mixer]` section in your config with the following content:


```ini
[mixer]
reverb = large
chorus = normal
```

Now the intro music sounds a lot more spacious a pleasant to listen to,
especially in headphones. There's even some reverb added to the in-game
footsteps and sound effects, although to a much lesser extent.

You might want to experiment with the `small` and `medium` reverb settings,
and the `light` and `strong` chorus settings as well.

!!! note "Purist alert!"

    For the purists among you: adding reverb and chorus to the OPL sound is
    something you can do on certain Sound Blaster AWE32 and AWE64 models on real
    hardware too, using the standard Sound Blaster drivers.


## Auto-pause

Prince of Persia can be paused by pressing the <kbd>Esc</kbd> key during the
game. But wouldn't it be useful if DOSBox could auto-pause itself whenever you
switch to a different window? You can easily achieve that by enabling the
`pause_when_inactive` option in the `[sdl]` section.

```ini
[sdl]
fullscreen = on
viewport_resolution = 1120x840
pause_when_inactive = yes
```

Restart the game and switch to a different window---DOSBox has paused itself.
Switch back to DOSBox---the game resumes. Nifty, isn't it? This is quite
handy for those games that have no pause functionality, or when you need to
take a break during a long cutscene that cannot be paused.


## Final configuration

Below is the full configuration with all the enhancements we've added, including authentic VGA emulation on 4K displays.

```ini
[sdl]
fullscreen = on
viewport_resolution = 1280x960
pause_when_inactive = yes

[dosbox]
machine = vgaonly

[render]
glshader = crt/aperture

[mixer]
reverb = large
chorus = normal

[autoexec]
c:
prince
```

If you're on a 1080p display, just delete the whole `[dosbox]` and `[render]`
sections to remove the VGA double-scan emulation and the CRT shader.


## Configuration comments

Instead of deleting those lines, you can prefix them with a `#` character
(<kbd>Shift</kbd>+<kbd>3</kbd> on the US keyboard layout) to turn them into
comments.

Comments are lines starting with a `#` character; DOSBox ignores them when
reading the configuration. Normally, you would use them to add, well,
comments to your config, but "commenting out" a line is a quick way to
disable it without actually removing it.


```ini
[sdl]
fullscreen = on
viewport_resolution = 1280x960
pause_when_inactive = yes

#[dosbox]
#machine = vgaonly

#[render]
#glshader = crt/aperture

[mixer]
reverb = large
chorus = normal

[autoexec]
c:
prince
```

Well, we've pretty much maxed out Prince of Persia for demonstration purposes,
time to move on to another game!
