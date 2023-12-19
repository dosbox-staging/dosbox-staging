# Enhancing Prince of Persia


## Fullscreen mode

You can toggle between windowed and fullscreen mode any time by pressing
++alt+enter++. But what if you always want to play the game in fullscreen?
Wouldn't it be nice to make DOSBox start in fullscreen right away?

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

!!! note "Toggling options"

    For configuration settings that toggle an option, you can use **`on`**,
    **`enabled`**, **`yes`**, **`true`** or **`1`** for turning the option *on*,
    and **`off`**, **`disabled`**, **`no`**, **`false`** or **`0`** for
    turning it *off*.

Note we've also added the `exit` command to the end of the `[autoexec]`
section; with this in place, DOSBox Staging will quit after we exit from the
game by pressing ++ctrl+q++. Not strictly necessary, but a nice touch.


!!! important "Making configuration changes"

    Every time you modify `dosbox.conf`, you need to restart DOSBox Staging 
    for the changes to take effect. We will not mention this every single time
    from now on.

    You can do this by closing the DOSBox Staging application then starting it
    again. But there is a better way: simply press ++ctrl+alt+home++! This is
    a homage to the Ctrl+Alt+Del "three-finger salute" way of soft-rebooting
    IBM PCs. :sunglasses:

    If you're on macOS,  you'll need to press ++cmd+opt+home++ instead, and if
    Apple has decided it for you that your MacBook doesn't need a Home key,
    ++cmd+opt+fn+left++ should do the trick.


## Graphics options

DOSBox emulates an SVGA (Super VGA) display adapter by default; this gives you
good compatibility with most DOS games. DOSBox can also emulate all common
display adapters from the history of the PC compatibles: the Hercules, CGA,
and EGA adapters among several others.

Although the VGA standard was introduced in 1987, it took a good five years
until it gained widespread adoption. Games had to support all commonly used
graphical standards during this transitional period. Prince of Persia,
released in 1990, is such a game; it not only supports all common display
adapters, but it also correctly auto-detects them. This is in contrast with
the majority of DOS games where you need to configure the graphics manually
(we'll discuss how to do that too in later chapters).


### VGA

The art in Prince of Persia was created for VGA first, so quite naturally the
game looks best on a VGA adapter. Most DOS games from the 1990s use the
320&times;200 low-resolution VGA mode. This video mode allows up to 256
colours to be displayed simultaneously on the screen from a total palette of
16 million colours.

This is how the start of the game looks with the [authentic CRT emulation
feature](prince-of-persia#authentic-crt-monitor-emulation) which is enabled by
default. As mentioned previously, make sure to view all screenshots at 100%
magnification to avoid weird artifacts caused by the browser rescaling the
image.

<figure markdown>
  ![Prince of Persia in VGA mode with the default settings](https://archive.org/download/dosbox-staging-v0.82.0-prince-of-persia-vga/pop-vga.png){ loading=lazy }

  <figcaption markdown>
  Prince of Persia in VGA mode with the default settings
  </figcaption>
</figure>


But wait a minute, this doesn't look like anything I'm used to from console
emulators. Where are those visible thick scanlines?

VGA display adapters have a peculiarity that they display not one but *two*
scanlines per pixel in low-resolution video modes having less than about 350
lines of vertical resolution. This means the 320&times;200 VGA mode is really
640&times;400, just pixel and line-doubled. In fact, the vast majority of VGA
monitors from the 80s and 90s are incapable of displaying "true" 200-line
graphics, so it's physically impossible to get the "thick scanline" arcade and
home computer monitor look on a real VGA CRT.

Here's a comparison of the authentic VGA CRT emulation and the "raw sharp pixels"
look at 4K resolution (zoom in to appreciate the differences):

<figure markdown>
  !["True VGA" emulation vs "raw sharp pixels"](https://archive.org/download/dosbox-staging-v0.82.0-prince-of-persia-vga/pop-true-vga.png){ loading=lazy }

  <figcaption markdown>
  Left: authentic double-scanned VGA CRT emulation<br>Right: "raw sharp pixels" look
  </figcaption>
</figure>

Double-scanned VGA emulation looks stellar on a 4K monitor and even
on 1440p, but on 1080p, there is simply not enough vertical resolution to
accurately represent all those 400 tightly-packed scanlines. Therefore, the
best thing the CRT emulation can do on 1080p and lower is to fake that
double-scanned look so it looks approximately similar to the real thing.


### EGA

The next graphics adapter the game supports is the EGA adapter released in
1984. The EGA standard was quite popular until the early 1990s because VGA
cards were initially prohibitively expensive for most hobbyists. The majority
of games with EGA support use the 320&times;200 low-resolution EGA mode that
has a fixed 16-colour palette.

To run Prince of Persia in EGA mode, you simply need to tell DOSBox to emulate
a machine equipped with an EGA adapter. That can be easily done by adding the
following configuration snippet:

```ini
[dosbox]
machine = ega
```

This is how the game looks with the fixed 16-colour EGA palette. Unlike VGA ,
EGA monitors can display true 320&times;200 resolution with the well-known
thick scanline look.

<figure markdown>
  ![Prince of Persia in EGA mode](https://archive.org/download/dosbox-staging-v0.82.0-prince-of-persia-ega/pop-ega.png){ loading=lazy }

  <figcaption markdown>
  Prince of Persia in EGA mode---EGA monitors can display 200-line modes
  without line doubling, so we'll get the "fat scanline" look
  </figcaption>
</figure>


### CGA

The notorious CGA adapter, the first colour graphics adapter created for the
original IBM PC in 1981, is a serious contender for the worst graphics
standard ever invented. In its 320&times;200 low-resolution mode, only 4 colours
can be displayed simultaneously on the screen chosen from a small number of fixed
palettes. Frankly, all combinations look pretty horrifying...

But no matter; the game supports it, so we'll have a
look at it. Make the following adjustment to the `machine` setting, and don't
forget to put on your safety goggles! :sunglasses:

```ini
[dosbox]
machine = cga
```

Ready? Behold the formidable 4-colour CGA graphics---and what 4 colours those
are! Similarly to EGA, CGA is capable of displaying true 200-line modes.

<figure markdown>
  ![Prince of Persia in CGA mode using the 'crt/aperture' shader](https://archive.org/download/dosbox-staging-v0.82.0-prince-of-persia-cga/pop-cga-crt.jpg){ loading=lazy }

  <figcaption markdown>
  Prince of Persia in CGA mode using. Yeah, it's not pretty...
  </figcaption>
</figure>

!!! tip

    To compare how the game looks on different graphics adapters, click on one
    of screenshots to enlarge it, then use the ++left++ and ++right++ arrow
    keys to switch between the images.


### Hercules

The Hercules display adapter was released in 1982 to expand the text-only IBM
PCs with basic graphical capabilities. It only supports monochrome graphics,
but at a higher 720&times;348 resolution. Can you guess how you can enable it?

```ini
[dosbox]
machine = hercules
```

Here's how it looks. Oldschool! The slightly squashed aspect ratio is correct
in this case; this is exactly how the game would look on a PC equipped with a
Hercules card connected to a period-accurate monochrome monitor (but we'll
examine some advanced techniques how to [fix that](#custom-aspect-ratios)).

<figure markdown>
  ![Prince of Persia in Hercules mode using the default 'amber' palette](../assets/images/pop-hercules-amber.png){ loading=lazy }

  <figcaption markdown>
  Prince of Persia in Hercules mode using the default `amber` palette
  </figcaption>
</figure>


Monochrome monitors came in different colours and DOSBox Staging can emulate
all these variations via the `monochrome_palette` setting. The available
options are `amber` (the default), `green`, `white`, and `paperwhite`.

```ini
[render]
monochrome_palette = green
```

You can also cycle between these while the game is running with the ++f11++
key.

<div class="image-grid" markdown>

<figure markdown>
  ![Hercules mode using the 'green' palette](../assets/images/pop-hercules-green.png){ loading=lazy }

  <figcaption markdown>
  Hercules mode using the `green` palette
  </figcaption>
</figure>

<figure markdown>
  ![Hercules mode using the 'white' palette](../assets/images/pop-hercules-white.png){ loading=lazy }

  <figcaption markdown>
  Hercules mode using the `white` palette
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
    nostalgic feelings towards a particular display adapter; b) research
    purposes; c) you like your games to look like the user interface of an
    industrial CNC machine; d) a strong preference for pain. But hey, who are
    we to judge?

    In any case, preserving all relevant aspects of PC gaming history is
    important for the DOSBox Staging project, so these display options are
    always available at your fingertips, should you ever need them.



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
    AWE32 and AWE64 models too with the standard drivers.


## Pausing the game

Prince of Persia can be paused by pressing the ++esc++ key during the game.
That's nice, but not all games have such built-in pause functionality.

Luckily, DOSBox has a pause feature that you can activate in any game with
the ++alt+pause++ shortcut (++cmd+p++ on macOS), then unpause by pressing the
same shortcut again.

But wouldn't it be convenient if DOSBox could auto-pause itself whenever you
switch to a different window? You can easily achieve that by enabling the
`pause_when_inactive` option in the `[sdl]` section.

```ini
[sdl]
pause_when_inactive = yes
```

Restart the game and switch to a different window---DOSBox will pause itself.
Switch back to DOSBox---the game will resume. Nifty, isn't it? 

!!! warning "Pause gotchas"

    All input is disabled in the manually paused state—the only thing you can
    do is press the pause shortcut again to unpause the emulator. In windowed
    mode, the paused state is indicated in the DOSBox Staging window's title
    bar.


## Final configuration

Below is the full config with all the enhancements we've added:

```ini
[sdl]
fullscreen = on
pause_when_inactive = yes

[mixer]
reverb = large
chorus = normal

[autoexec]
c:
prince
exit
```


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
pause_when_inactive = yes

[render]
#glshader = sharp

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
prospect... Fortunately, you don't have to do that because DOSBox Staging can
inform you if something went wrong. It does that via the *logs*, which is just
a continuous stream of timestamped messages in a separate window.

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
glshader = xsharp
```

As expected, we'll see some warnings and errors in the logs after a restart:

![Example DOSBox logs showcasing warnings and errors](https://archive.org/download/dosbox-staging-v0.82.0-logging/pop-logs2.png){ loading=lazy }

Warnings are yellow, errors are red. Errors are generally reserved for more
severe problems. In this example, we have the following:

- The first warning tells us that `blah` is not a valid value for the
  `fullscreen` setting, so DOSBox Staging has reverted to its default value
  of `false`.

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


## Advanced graphics options

You can safely skip these more advanced topics and keep going with the rest of
the guide if you're less interested in the more technical aspects of graphics
emulation. But at least try to read the first section about aspect ratios and
black borders, then revisit the rest later when you're ready to deepen your
understanding of these topics. The [sharp pixels](#sharp-pixels) section might
also be of interest if you don't want to use authentic CRT emulation.


### Aspect ratios, square pixels & black borders

Okay, so that's all good and well, but why doesn't the image fill the
screen completely? Why are there black borders around it?

Computer monitors were originally *not* widescreen but had a 4:3 display
aspect ratio, just like old television sets. The complete switch to 16:9 in
computer monitors and laptops happened only by the early 2010s. The
low-resolution 320&times;200 VGA mode was designed to completely fill a 4:3
CRT screen. When displaying such 4:3 aspect ratio content on a 16:9 modern
flat panel, it's inevitable to get black bars on the sides of the image (this
is called _pillarboxing_).

<figure markdown>
  ![Displaying a 4:3 image on a 16:9 flat screen](../assets/images/monitor-aspect-ratios1.png){ .skip-lightbox style="width: 25rem; margin: 1.5rem;" }

  <figcaption markdown>
  Pillarboxing in action: black bars fill the extra space<br>when the aspect ratio of the screen and the image do not match
  </figcaption>
</figure>

Hang on a second, something is not right here... 320:200 can be simplified to
16:10, which is quite close to 16:9, which means 320&times;200 resolution
content _should_ fill a 16:9 screen almost completely with only very minor
pillarboxing! Sure, that would be true if the pixels of the 320&times;200 VGA
mode were perfect little squares, but they are not. How so?

As mentioned, 320&times;200 graphics completely fills the screen on a 4:3
aspect ratio CRT—that's only possible if the pixels are not perfect squares,
but slightly tall rectangles. With square pixels, you'd get some
_letterboxing_ below the image (the horizontal version of pillarboxing) as
shown on the below image. If you do the maths, it turns out the pixels need to
be exactly 20% taller than wide.

<figure markdown>
  ![Displaying a 4:3 image on a 16:9 flat screen](../assets/images/monitor-aspect-ratios2.png){ .skip-lightbox style="width: 25rem; margin: 1.5rem;" }

  <figcaption markdown>
  Left: 320&times;200 pixel image with square pixels on a 4:3 monitor---there
  is some letterboxing below the image; Right: the same image with 20% taller
  pixels on the same monitor---the image fills the screen completely
  </figcaption>
</figure>

Here's how to derive it: 4:3 can be rewritten as 320:240 if you multiply both
the numerator and denominator by 80. Then 240 divided by 200 is 1.2, so the _pixel
aspect ratio_, or PAR---the mathematical ratio that describes how the width of a
pixel compares to its height---is **1:1.2**, which can be rewritten as
**5:6**.

But hey, not everybody likes maths, especially not in the middle of a gaming
session! Most of the time, you won't need to worry about aspect ratio
correctness because DOSBox Staging handles that automatically for you
out-of-the-box. There is a small but significant number of games where
overriding aspect ratio correction and forcing square pixels yields better
results, but more about that in later chapters.


!!! note "When pixels are not squares"

    Sadly, this very important fact of computing history that monitors were
    *not* widescreen until about 2010 tends to be forgotten nowadays. Older
    DOSBox versions did not perform aspect ratio correction by default which
    certainly did not help matters either. The unfortunate situation is that
    you're much more likely to encounter videos and screenshots of DOS games
    in the wrong aspect ratio (using square pixels) on the Internet today.
    Well, that's only true for more recently created content---if you check
    out any old computer magazine from the 1980s and the 90s, most screenshots
    are shown in the correct aspect ratio (but rarely the magazines got it
    wrong too...)

    In case you're wondering, pixels are completely square in 640&times;480
    and higher resolutions (1:1 pixel aspect ratio), but there exist a few
    more weird video modes with non-square pixels, e.g., the 640&times;350 EGA
    mode with 1:1.3714 PAR, the 640&times;200 EGA mode (1:2.4 PAR), and the
    720&times;348 Hercules graphics mode (1:1.5517 PAR).


### Integer scaling 

That explains the black bars on the sides, but what about the thin
letterboxing above and below the image? Why doesn't the graphics fill the
screen vertically?

Again, we need a little history lesson to understand what's going on. CRT
monitors did not have a fixed pixel-grid like modern flat panels; they were a
lot more flexible and could display *any* resolution within the physical
limits of the monitor. In fact, they did not even have discrete "pixels" in
the sense as modern flat panels do---the image was literally projected onto
the screen, similarly to how movie a projector works.

Modern flat screens, however, do have a fixed native resolution, and the
scanlines of the emulated CRT image need to "line up" with this fixed pixel
grid vertically, otherwise we might get quite unpleasant looking interference
artifacts (these usually manifest as wavy vertical patterns). When DOSBox
Staging enlarges the emulated image to fill the screen, by default it
constrains the vertical scaling factor to integer values; this ensures perfect
alignment with the native pixel grid of the display. The horizontal direction
is rarely a problem, so non-integer horizontal scaling factors are generally
fine. 

Assuming a 4K monitor with 3840&times;2160 native resolution, the
320&times;200 VGA mode double-scanned to 640&times;400 will be enlarged by a
factor of 5 vertically, and by a 5 &times; 4/3 = 6.6667 factor horizontally to
maintain the correct 4:3 aspect ratio of the upscaled image. The final
output will be 2667&times;2000, so those 160 unused pixels in the
vertical direction account for the slight letterboxing above and below the
image.

Now, the higher your monitor resolution, the more you can get away with using
non-integer vertical scaling ratios. It's just that enabling vertical integer
scaling by default is the only surefire way to completely avoid ugly
interefence artifacts on everybody's monitors out-of-the-box. If you play
games in fullscreen on a 4K screen, you can safely disable integer scaling
without any adverse effects up to the 640&times;480 VGA resolution. Just put
the following into your config:

```ini
[render]
integer_scaling = off
```

If you don't have a 4K or monitor or you like to play your games in windowed
mode, you'll need to experiment a bit---some monitor resolution, window size,
and DOS video mode combinations look fine without integer scaling, some will
result in interference patterns.


### Sharp pixels

Okay, enough blabbering about all those near-extinct cathode-ray tube
contraptions, grandpa. Can you just give us sharp pixels and be done with it?

Sure thing, kiddo. Just put this into your config:

```ini
[render]
glshader = sharp
```

By default, integer scaling is disabled when the `sharp` shader is selected,
so if you want to re-enable it, you'll need to add another line to the
`[render]` section:

```ini
[render]
glshader = sharp
integer_scaling = vertical
```

This will yield 100% sharp pixels vertically, no matter what. Horizontally,
there might be a 1-pixel-wide interpolation band at the sides of _some_ of the
pixels, depending on the DOS video mode and the upscale factor. This is the best
possible compromise between maintaining the correct aspect ratio, even
horizontal pixel widths, and good overall sharpness.

The resulting image has quite acceptable horizontal sharpness at 1080p, and
from 1440p upwards, you'll be hard pressed to notice the occasional 1-pixel
interpolation band... unless you're inspecting the image with your nose
pressed against the monitor (well, how about not doing that then?)


### Custom viewport resolution

That's very nice and all, but now the graphics looks as if it was constructed
from brightly coloured little bricks at fullscreen on a 24" or larger modern
display!

Well, 14" VGA monitors were still the most affordable and thus most popular
option until the mid-1990s, close to the end of the DOS era. Before that, CGA
and EGA monitors were typically 12" or 14", and monochrome Hercules monitors
only 10" or 12". And these are just the "nominal" screen sizes---the actual
visible area was about 1.5" to 2" smaller!

To emulate the physical image size you'd get on a typical 14" VGA monitor,
you'd need to restrict the output to about 960&times;720 on a modern 24" widescreen
display (assuming the same normal viewing distance). Basically, you want the
resulting image to measure about 12" (30 cm) diagonally for authentic results. But that might a bit too
small for most people, and 960&times;720 would result in a non-integer
vertical scale factor. It's best to bump it up to 1067&times;800 then:


```ini
[render]
viewport = 1067x800
```

The viewport size is specified in _logical units_ (more on that below). You
can also specify the viewport restriction as a percentage of the size of your
desktop:

```ini
[render]
viewport = 1067x800
```

The video output will be sized to fit within the bounds of this rectangle
while keeping the correct aspect ratio. If integer scaling is also enabled,
the resulting image might be become smaller than the specified viewport size.


!!! note "No, DOS games did not look like Roman mosaics..."

    Running DOS games in fullscreen on a modern 24" widescreen display is
    equivalent to playing them on a 21" CRT monitor! Those huge beasts were
    used by (and most importantly, _priced for_) professionals, so extremely
    few gamers and enthusiasts saw low-resolution art blown up to look like
    Roman mosaics back in the day. Even _yours truly_ performed all the tricks
    in the book to shrink 320&times;200 DOS games as much as possible on his
    19" CRT in the early 2000s...


!!! info "Logical units vs pixels"

    Most operating systems operate in high DPI mode using a 200% scaling
    factor on 4K displays and use "unscaled" _logical units_ for specifying
    window dimensions. The `viewport` setting adopts the same approach; the
    viewport size must be always specified in logical units.

    For example, on a 4K display, a viewport resolution specified as
    `1280x960` logical units will translate to 2560&times;1920 *physical
    pixels* (assuming a typical 200% scaling factor). Then on 1080p with a
    100% scaling factor, it will simply translate to 1280&times;960 pixels.



### Custom aspect ratios

Psssst... Don't tell anyone, but DOSBox Staging features a hidden "screw the
developer's intent" switch as well (also known as "LOL what aspect ratio?"
mode). To unleash its formidable powers, put this casually into your config
when no one's looking:

```ini
[render]
aspect = stretch
```

DOSBox Staging will now happily trample over any authentic aspect ratios and
stretch the image to the full extents of the viewport. Keep resizing the
window and see what happens. Whoopee! Now you can play Prince of Persia
completely stretched to a widescreen monitor like a barbarian!

Jokes aside, this mode might come in handy when playing text adventures in
fullscreen; it's hard to argue the correct aspect ratio matters much for those
games. The same reasoning can also be applied to more abstract graphical
games (although opinions on that are somewhat divisive...)

You can also specify custom aspect ratios in the form of horizontal and
vertical "stretch factors". This emulates the horizontal and vertical stretch
controls of CRT monitors. For example, we can correct the [squashed
look](#hercules) of the Hercules graphics in Prince of Persia with the
following magic incantations:

```ini
[render]
aspect = stretch
viewport = relative 112% 173%
```

<figure markdown>
  ![Prince of Persia in Hercules mode using the 'white' palette and the 'crt/aperture.mono' shader](../assets/images/pop-hercules-aspect-corrected.png){ loading=lazy }

  <figcaption markdown>
  Prince of Persia in Hercules mode with custom stretch factors<br>
  to make the image fill our 4:3 "emulated CRT screen"
  </figcaption>
</figure>


---

Okay, we've pretty much maxed out Prince of Persia for demonstration purposes.
Time to move on to another game!

