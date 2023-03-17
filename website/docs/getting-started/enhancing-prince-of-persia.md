# Enhancing Prince of Persia


## Fullscreen mode

You can toggle between windowed and fullscreen mode at any time by pressing
++alt+enter++. But what if you always want to play the game
in fullscreen? Wouldn't it be nice to configure DOSBox to start in fullscreen
mode?

You can easily do that by setting `fullscreen = on` in the `[sdl]` section.
Modify your `dosbox.conf` file so it has the following content, then restart
DOSBox Staging.

```ini
[sdl]
fullscreen = on

[autoexec]
c:
prince
exit
```

Note we've also added the `exit` command to the end of the `[autoexec]`
section; with this in place, DOSBox will quit itself after we exit from the
game by pressing ++ctrl+q++. Not strictly necessary, but a nice touch.


!!! note "Toggling options"

    For configuration settings that toggle an option, you can use **`on`**,
    **`enabled`**, **`yes`**, **`true`** or **`1`** for turning the option *on*,
    and **`off`**, **`disabled`**, **`no`**, **`false`** or **`0`** for
    turning it *off*.

!!! important "Making configuration changes"

    You need to quit and restart DOSBox Staging after updating `dosbox.conf`
    for the changes to take effect. We will not mention this every single time
    from now on.


## Custom viewport resolution

That's nice, but depending on the size of your monitor, 320&times;200 VGA
graphics displayed in fullscreen on a 24" or 27" modern monitor might appear
overly blocky. If only we could control the size of the image in fullscreen as
easily as we can in windowed mode!

Well, it turns out we can! You can restrict the viewport size by adding the
`viewport_resolution` setting to the `[sdl]` section. In this case, we're
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
just like old television sets. The complete switch to 16:9 in computer
monitors and laptops happened only by the early 2010s. The low-resolution
320&times;200 VGA mode was designed to completely fill a 4:3 monitor, but
that's only possible if the pixels are not perfect squares, but slightly
tall rectangles. If you do the maths, it turns out they need to be exactly 20% taller
for the 320&times;200 VGA screen mode (1:1.2 pixel aspect ratio). 700 &times;
1.2 = 840, so that's the explanation.

<figure markdown>
  ![Prince of Persia in fullscreen with 1120&times;840 viewport resolution restriction](https://archive.org/download/dosbox-staging-v0.82.0-prince-of-persia-vga/pop-viewport.png){ loading=lazy }

  <figcaption markdown>
  Prince of Persia in fullscreen with 1120&times;840 viewport resolution restriction
  </figcaption>
</figure>

In any case, you don't need to worry about aspect ratio correctness because
DOSBox Staging handles that automatically for you out of the box. It's just
good to have some level of understanding of what's really happening. If you
specify `1120x700`, that would also work, but because aspect ratio correction
is enabled by default, and the image must fit within the bounds of the
specified viewport rectangle, the dimensions of the resulting image will be
933 by 700 pixels (because 700 &times; 4/3 = 933).

But hey, not everybody likes maths, especially in the middle of a DOS gaming
session, so you can also specify the viewport resolution as a percentage of your
desktop resolution:

```ini
viewport_resolution = 80%
```

!!! note "When pixels are not squares"

    Sadly, this very important fact of computing history that monitors were
    *not* widescreen until about 2010 tends to be forgotten nowadays. Older
    DOSBox versions did not perform aspect ratio correction by default, which
    certainly did not help matters either. The unfortunate situation is that
    you're much more likely to encounter videos and screenshots of DOS games
    in the wrong aspect ratio (using square pixels) on the Internet today.
    Well, that's only true for more recently created content---if you check
    out old computer magazine from the 1980s and the 90s, most screenshots are
    shown in the correct aspect ratio (rarely, the magazines got it wrong
    too...)

    In case you're wondering, pixels are completely square in 640&times;480
    and higher resolutions (1:1 pixel aspect ratio), but there exist a few
    more weird screen modes with non-square pixels, e.g., the 640&times;350
    EGA mode with 1:1.3714 pixel aspect ratio (PAR), the 640&times;200 EGA
    mode with 1:2.4 PAR, and the 720&times;348 Hercules graphics mode with
    1:1.5517 PAR.


## Graphics adapter options

DOSBox emulates an SVGA (Super VGA) display adapter by default, which gives
you good compatibility with most DOS games. DOSBox can also emulate
all common display adapters from the history of the PC compatibles: the
Hercules, CGA, and EGA display adapters, among many others.

Although the VGA standard was introduced in 1987, it took a good five years
until it gained widespread adoption. Games had to support all commonly used
graphical standards during this transitional period. Prince of Persia,
released in 1990, is such a game; it not only supports all common display
adapters, but it also correctly auto-detects them. This is in contrast with
the majority of DOS games where you need to configure the graphics manually.


### VGA

The art in Prince of Persia was created for VGA first, so quite naturally the
game looks best on a VGA adapter. Most DOS games from the 1990s use the
320&times;200 low-resolution VGA mode that allows up to 256 colours to be
displayed on the screen, chosen from a palette of 16 million colours.

<figure markdown>
  ![Prince of Persia in VGA mode with the default settings](https://archive.org/download/dosbox-staging-v0.82.0-prince-of-persia-vga/pop-vga.png){ loading=lazy }

  <figcaption markdown>
  Prince of Persia in VGA mode with the default settings
  </figcaption>
</figure>


### EGA

Most games with EGA support use the 320&times;200 low-resolution EGA mode that
has a fixed 16-colour palette.

To run Prince of Persia in EGA mode, you simply need to tell DOSBox to emulate
a machine equipped with an EGA adapter. That can be easily done by adding the
following configuration snippet:

```ini
[dosbox]
machine = ega
```

This is how the game looks with the fixed 16-colour EGA palette:

<figure markdown>
  ![Prince of Persia in EGA mode](https://archive.org/download/dosbox-staging-v0.82.0-prince-of-persia-ega/pop-ega.png){ loading=lazy }

  <figcaption markdown>
  Prince of Persia in EGA mode
  </figcaption>
</figure>

That's nice, but it's not exactly how the graphics looked on an EGA monitor
back in the day. Pre-VGA monitors had visible scanlines, and we can emulate
that by enabling an OpenGL shader that emulates the CRT monitors
(Cathode-Ray Tube) of the era:

```ini
[render]
glshader = crt/aperture
```

This adds some texture and detail to the image that's especially apparent on
the solid-coloured bricks:

<figure markdown>
  ![Prince of Persia in EGA mode using the 'crt/aperture' shader](https://archive.org/download/dosbox-staging-v0.82.0-prince-of-persia-ega/pop-ega-crt.jpg){ loading=lazy }

  <figcaption markdown>
  Prince of Persia in EGA mode using the `crt/aperture` shader
  </figcaption>
</figure>

If you have a 4K display, you might want to try the `crt/fakelottes.tweaked`
shader. Another good one is `crt/geom.tweaked`.

The `crt/zfast` shader is a good choice for older or integrated graphics
cards, and the `crt/pi` was specifically optimised for the Raspberry Pi.

You can view the full list of included CRT shaders
[here](https://github.com/dosbox-staging/dosbox-staging/tree/main/contrib/resources/glshaders/crt).


### CGA

The notorious CGA adapter, the first colour graphics adapter created for the
original IBM PC in 1981, is a serious contender for the worst graphics
standard ever invented. In its 320&times;200 low-resolution mode, 4 colours
can be displayed simultaneously on the screen chosen from a number of fixed
palettes. Frankly, all combinations look pretty horrible...

But no matter; the game supports it, so we'll have a
look at it. Make the following adjustment to the `machine` setting, and don't
forget to put on your safety glasses! :sunglasses:

```ini
[dosbox]
machine = cga
```

Ready? Behold the formidable 4-colour CGA graphics---and what 4 colours those
are!

<figure markdown>
  ![Prince of Persia in CGA mode using the 'crt/aperture' shader](https://archive.org/download/dosbox-staging-v0.82.0-prince-of-persia-cga/pop-cga-crt.jpg){ loading=lazy }

  <figcaption markdown>
  Prince of Persia in CGA mode using the `crt/aperture` shader
  </figcaption>
</figure>

!!! tip

    To compare how the game looks on different graphics adapters, click on one
    of the CRT shader screenshots to enlarge it, then use the ++left++ and
    ++right++ arrow keys to switch between the images.


!!! warning

    Screenshots of CRT shaders in action *need* to be viewed at 100%
    magnification; otherwise, you might see strange wavy interference patterns
    on them caused by the browser rescaling the images. These unwanted
    artifacts can sometimes
    resemble repeating rainbow-like patterns, and rest assured, that's *not*
    how CRT emulation is supposed to look in DOSBox Staging!

    To view a screenshot featuring a CRT shader properly, click on the image
    to enlarge it, then if your mouse cursor looks like a magnifying glass,
    click on the image again to display it with 100% magnification.


### Hercules

The Hercules display adapter was released in 1982 to expand the text-only IBM
PCs with basic graphical capabilities. It only supports monochrome graphics,
but at a higher 720&times;348 resolution. Can you guess how you can enable it?

```ini
[dosbox]
machine = hercules
```

If you wish to use a CRT shader, the `crt/aperture.mono` shader variant has
been specifically created for emulating monochrome monitors (it switches off
the RGB phosphor and aperture grille emulation of the base `crt/aperture`
shader).

```ini
[render]
glshader = crt/aperture.mono
```

<figure markdown>
  ![Prince of Persia in Hercules mode using the 'white' palette and the 'crt/aperture.mono' shader](https://archive.org/download/dosbox-staging-v0.82.0-prince-of-persia-hercules/pop-hercules-crt-white.png){ loading=lazy }

  <figcaption markdown>
  Prince of Persia in Hercules mode using the `white` palette<br>and the `crt/aperture.mono` shader
  </figcaption>
</figure>


Monochrome monitors came in different colours; DOSBox Staging can emulate all
these variations via the `monochrome_palette` configuration setting.
The available colours are `white` (the default), `paperwhite`, `green`, and
`amber`. You can also switch between them while the game is running with the
++f11++ key on Windows and Linux, and ++alt+f11++ on macOS.

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
  ![Hercules mode using the 'amber' palette](https://archive.org/download/dosbox-staging-v0.82.0-prince-of-persia-hercules/pop-hercules-crt-amber.png){ loading=lazy }

  <figcaption markdown>
  Hercules mode using the `amber` palette
  </figcaption>
</figure>

<figure markdown>
  ![Hercules mode using the 'green' palette](https://archive.org/download/dosbox-staging-v0.82.0-prince-of-persia-hercules/pop-hercules-crt-green.png){ loading=lazy }

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
    *any* graphics surely beat no game at all!

    Although we've shown how to emulate these earlier graphics standards for
    completeness' sake, there's generally little reason for *not* playing a
    DOS game with the best graphics. The list of possible reasons include: a)
    strong nostalgic feelings towards a particular display adapter; b)
    research reasons; c) you like your games to look like the user interface
    of an industrial CNC machine; d) a preference for pain. But hey, who are
    we to judge?

    In any case, preserving all relevant aspects of PC gaming history is
    important for the DOSBox Staging project, so these display options are
    always available at your fingertips, should you ever need them.


## "True VGA" emulation

If you're the lucky owner of a high-resolution monitor with at least 2000
pixels of native vertical resolution (4K or better monitors belong to this
category), you can use "true VGA" emulation that more accurately resembles the
output of an actual CRT VGA monitor.

VGA display adapters have a peculiarity that they display not one but *two*
scanlines per pixel in all low-resolution screen modes that have less than 350
lines. This means the 320&times;200 low-resolution VGA mode is really
640&times;400, just pixel and line-doubled.

To emulate this, we'll use a CRT shader and exact 4-times scaling by setting
the viewport resolution to 1280&times;960. This is important for getting even
scanlines, and it doesn't work on regular 1080p monitors because there's
simply not enough vertical resolution for the shader to work with.

!!! note "Logical vs physical pixels"

    Wait a minute, 1280&times;960 fits into a 1920x1080 screen, doesn't it?
    Why did you say we need a 4K display for this then?

    Most operating systems operate in high-DPI mode using a 200% scaling
    factor on 4K displays. This means that the pixel dimensions used by the
    applications no longer represent *physical pixels*, but rather *logical*
    or *device-independent pixels*. On a 4K display, assuming a typical 200%
    scaling factor, our viewport resolution specified in 1280&times;960
    *logical pixels* will translate to 2560&times;1920 *physical pixels*. A
    vertical resolution of 1920 pixels is certainly sufficient for displaying
    those 400 scanlines; the shader can use 4.8 pixels per scanline to do its
    magic (generally, you need at least 3 pixels per "emulated scanline" to
    achieve good results).


These are the relevant configuration settings to enable authentic
double-scanned VGA emulation:

```ini
[sdl]
viewport_resolution = 1280x960

[render]
glshader = crt/aperture
```

Here's a comparison of how true VGA emulation looks at 4K resolution compared
to the standard "raw sharp pixel" look (zoom in to appreciate the differences):

<figure markdown>
  !["True VGA" emulation vs "raw sharp pixels"](https://archive.org/download/dosbox-staging-v0.82.0-prince-of-persia-vga/pop-true-vga.png){ loading=lazy }

  <figcaption markdown>
  Left: "True VGA" emulation; Right: default output ("raw sharp pixels")
  </figcaption>
</figure>



## Chorus & reverb

We've been discussing graphics at great length, but what about sound? Can't
we do something cool to the sound as well?

Yes, we can! DOSBox Staging has an exciting feature to add chorus and reverb
effects to the output of any of the emulated sound devices.

Create a new `[mixer]` section in your config with the following content:


```ini
[mixer]
reverb = large
chorus = normal
```

Now the intro music sounds a lot more spacious and pleasant to listen to,
especially through headphones. There's even some reverb added to the in-game
footsteps and sound effects, although to a much lesser extent.

You might want to experiment with the `small` and `medium` reverb presets, and
the `light` and `strong` chorus settings as well.

!!! note "Purist alert!"

    To calm the purists among you: adding reverb and chorus to the OPL
    synthesiser's output is something you can do on certain Sound Blaster
    AWE32 and AWE64 models on real hardware too via the standard drivers.


## Auto-pause

Prince of Persia can be paused by pressing the ++esc++ key during the game.
But wouldn't it be convenient if DOSBox could auto-pause itself whenever you
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

Below is the full config with all the enhancements we've added, including
authentic VGA CRT monitor emulation on 4K displays.

```ini
[sdl]
fullscreen = on
viewport_resolution = 1280x960
pause_when_inactive = yes

[render]
glshader = crt/aperture

[mixer]
reverb = large
chorus = normal

[autoexec]
c:
prince
exit
```

If you're not on a high-DPI display, just delete the whole `[render]` section
to disable the CRT shader.


## Configuration comments

Instead of deleting those lines, you can prefix them with a `#` character
(++shift+3++ on the US keyboard layout) to turn them into *comments*.

Comments are lines starting with a `#` character; DOSBox ignores them when
reading the configuration. Normally, you would use them to add, well,
comments to your config, but "commenting out" a line is a quick way to
disable it without actually removing the line.


```ini
[sdl]
fullscreen = on
viewport_resolution = 1280x960
pause_when_inactive = yes

[render]
#glshader = crt/aperture

[mixer]
reverb = large
chorus = normal

[autoexec]
c:
prince
exit
```

Note we haven't commented out the `[render]` section itself. That's fine;
empty sections are allowed and do no harm.


## Inspecting the logs

What if you used a wrong value in your config, made a typo, or misremembered
the name of a setting? How would you know about it? Having to triple-check
every single character of the config when something goes wrong is not a good
prospect... Fortunately, you don't have to do that because DOSBox can inform
you if something went wrong. It does that via the *logs*, which is just a
continuous stream of timestamped messages in a separate window.

The behaviour of the log window is different on each platform:

<h3>Windows</h3>

The log window is opened by default when you start DOSBox Staging . You can
hide the log window by passing the `-noconsole` argument to the DOSBox
Staging executable.

<h3>macOS</h3>

DOSBox Staging does not open the log window by default when started via its
application icon, or with the *Start DOSBox Staging* icon you copied from the
`.dmg` installer archive earlier.

To make the log window appear, you need to use another shortcut icon called
_Start DOSBox Staging (logging)_, which you can copy into your game folder
from the same `.dmg` archive (note the word "logging" in the name of the
icon).

<h3>Linux</h3>

Start DOSBox Staging from the terminal, and the logs will appear there.

---

Under normal circumstances, the logs contain lots of interesting technical
details about what DOSBox Staging is currently doing, what exact settings it
uses, and so on. For example, this is the start of the logs when running the
game on macOS:

![Example DOSBox logs](https://archive.org/download/dosbox-staging-v0.82.0-logging/pop-logs1.png){ loading=lazy }

That's all good and well, but what's even better, if an invalid configuration
setting has been detected, or something went wrong when running the game,
DOSBox Staging will inform us about it in the logs!

Let's now intentionally screw a few things up in the config to trigger some
errors:

<div class="compact" markdown>
- Set `machine` to `mcp`
- Include a bogus config setting in the `[sdl]` section: `no_such_config_param = 42`
- Set `fullscreen` to `blah`
- Put the letter `x` in front of the shader's name
</div>

This is how the freshly ruined parts of our config should look like:

```ini
[sdl]
fullscreen = blah
no_such_config_param = 42

[dosbox]
machine = mcp

[render]
glshader = xcrt/aperture
```

As expected, we'll see some warnings and errors in the logs after a restart:

![Example DOSBox logs showcasing warnings and errors](https://archive.org/download/dosbox-staging-v0.82.0-logging/pop-logs2.png){ loading=lazy }

Warnings are yellow, errors are red. Errors are generally reserved for more
severe problems. In this example, we have the following:

- The first warning tells us that `blah` is not a valid value for the
  `fullscreen` setting, so DOSBox has reset it to its default value of
  `false`.

- Then it reprimands us about `no_such_config_param` that we made up.

- The third warning complains about an invalid `machine` value being used
  (sorry to disappoint, but DOSBox Staging can't emulate the Master
  Control Program from Tron yet!)

- Then we have an error because we're trying to use a non-existing shader.

- This is followed by yet another rather large warning block that very
  helpfully lists the names of all available shaders on our system. That's
  pretty handy!

This is a common theme with the logs. When DOSBox Staging displays some
warnings or errors, these messages often include suggestions and tips to help
you resolve the issues. Note that these can appear not only at startup but
also during running a game, depending on what DOSBox Staging is currently
doing.

While you might prefer to hide the log window for aesthetic reasons, it's
wiser to just let it be so you can take a peek at it if something goes
sideways. Without having access to the logs, your chances of fixing problems
are significantly reduced as basically you would be just stumbling in the
dark. You've been warned!

---

Okay, we've pretty much maxed out Prince of Persia for demonstration purposes.
Time to move on to another game!

