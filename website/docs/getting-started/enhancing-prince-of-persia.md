# Enhancing Prince of Persia


## Fullscreen mode

You can toggle between windowed and fullscreen mode at any time by pressing
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

    You can do this by closing the DOSBox Staging application and then
    starting it again. But there is a better way: simply press
    ++ctrl+alt+home++! This is a homage to the Ctrl+Alt+Del "three-finger
    salute" way of soft-rebooting IBM PCs. :sunglasses:

    If you're on macOS,  you'll need to press ++cmd+opt+home++ instead, and if
    Apple has decided for you that your MacBook doesn't need a Home key,
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
feature](setting-up-prince-of-persia.md#authentic-crt-monitor-emulation) which is enabled by
default. As mentioned previously, make sure to view all screenshots at 100%
magnification to avoid weird artifacts caused by the browser rescaling the
image.

<figure markdown>
  <a class="glightbox" href="https://www.dosbox-staging.org/static/images/getting-started/pop-vga.jpg" >
    ![Prince of Persia in VGA mode with the default settings](https://www.dosbox-staging.org/static/images/getting-started/pop-vga-small.jpg){ loading=lazy .skip-lightbox }
  </a>

  <figcaption markdown>
  Prince of Persia in VGA mode with the default settings
  </figcaption>
</figure>

But wait a minute, this doesn't look like anything I'm used to from console
emulators. Where are those visible thick scanlines?

VGA display adapters have a peculiarity in that they display not one but *two*
scanlines per pixel in low-resolution video modes having less than about 350
lines of vertical resolution. This means the 320&times;200 VGA mode is really
640&times;400, just pixel and line-doubled. In fact, the vast majority of VGA
monitors from the 1980s and '90s are incapable of displaying "true" 200-line
graphics, so it's physically impossible to get the "thick scanline" arcade and
home computer monitor look on a real VGA CRT.

Double-scanned VGA emulation looks stellar on a 4K monitor and even on 1440p,
but on 1080p, there is simply not enough vertical resolution to accurately
represent all those 400 tightly-packed scanlines. Therefore, the best thing
the CRT emulation can do on 1080p and lower is to fake that double-scanned
look so it looks approximately similar to the real thing.

!!! tip "Aspect ratio correction"

    Computer monitors were originally not widescreen but had a 4:3 display
    aspect ratio, just like old television sets. DOS games were normally
    designed to completely fill a 4:3 CRT screen. DOSBox Staging displays the
    emulated image in the correct aspect ratio by default, which means you'll
    see *pillarboxing* (vertical black bars) at the two sides in fullscreen on
    modern widescreen displays. This is normal.

    However, there is also *letterboxing* (horizontal black bars) above and
    below the image. We'll explain why that's necessary in the last [advanced
    graphics options](advanced-graphics-options.md) chapter. Feel free to take
    a detour if you wish to gain an understanding of these details now.

    Alternatively, if you're on a 4K (UHD, 3840x2160) or better screen, you
    can put this into your `[render]` section to get rid of pillarboxing
    altogether:

    ```ini
    [render]
    integer_scaling = off
    ```


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

This is how the game looks with the fixed 16-colour EGA palette. Unlike VGA,
EGA monitors can display true 320&times;200 resolution with the well-known
thick scanline look.

<figure markdown>
  <a class="glightbox" href="https://www.dosbox-staging.org/static/images/getting-started/pop-ega.jpg" >
    ![Prince of Persia in EGA mode](https://www.dosbox-staging.org/static/images/getting-started/pop-ega-small.jpg){ loading=lazy .skip-lightbox }
  </a>

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
  <a class="glightbox" href="https://www.dosbox-staging.org/static/images/getting-started/pop-cga.jpg" >
    ![Prince of Persia in CGA mode. Yeah, it's not pretty...](https://www.dosbox-staging.org/static/images/getting-started/pop-cga-small.jpg){ loading=lazy .skip-lightbox }
  </a>

  <figcaption markdown>
  Prince of Persia in CGA mode. Yeah, it's not pretty...
  </figcaption>
</figure>

!!! tip

    To compare how the game looks on different graphics adapters, click on one
    of the screenshots to enlarge it, then use the ++left++ and ++right++
    arrow keys to switch between the images.


### Hercules

The Hercules display adapter was released in 1982 to expand the text-only IBM
PCs with basic graphical capabilities. It only supports monochrome graphics
but at a higher 720&times;348 resolution. Can you guess how you can enable it?

```ini
[dosbox]
machine = hercules
```

Here's how it looks. Oldschool! The slightly squashed aspect ratio is correct
in this case; this is exactly how the game would look on a PC equipped with a
Hercules card connected to a period-accurate monochrome monitor (but we'll
examine some advanced techniques on [how to fix that](advanced-graphics-options.md#custom-aspect-ratios)).

<figure markdown>
  <a class="glightbox" href="https://www.dosbox-staging.org/static/images/getting-started/pop-hercules-amber.png" >
    ![Prince of Persia in Hercules mode using the default 'amber' palette](https://www.dosbox-staging.org/static/images/getting-started/pop-hercules-amber.png){ loading=lazy .skip-lightbox }
  </a>

  <figcaption markdown>
  Prince of Persia in Hercules mode using the default `amber` palette
  </figcaption>
</figure>


Monochrome monitors come in different colours and DOSBox Staging can emulate
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
  <a class="glightbox" href="https://www.dosbox-staging.org/static/images/getting-started/pop-hercules-green.png" >
    ![Hercules mode using the 'green' palette](https://www.dosbox-staging.org/static/images/getting-started/pop-hercules-green.png){ loading=lazy .skip-lightbox }
  </a>

  <figcaption markdown>
  Hercules mode using the `green` palette
  </figcaption>
</figure>

<figure markdown>
  <a class="glightbox" href="https://www.dosbox-staging.org/static/images/getting-started/pop-hercules-white.png" >
    ![Hercules mode using the 'white' palette](https://www.dosbox-staging.org/static/images/getting-started/pop-hercules-white.png){ loading=lazy .skip-lightbox }
  </a>

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
    completeness' sake, there's generally little reason *not* to play a
    DOS game with the best graphics. The list of possible reasons includes a)
    nostalgic feelings towards a particular display adapter; b) research
    purposes; c) you like your games to look like the user interface of an
    industrial CNC machine; d) a strong preference for pain. But hey, who are
    we to judge?

    In any case, preserving all relevant aspects of PC gaming history is
    important for the DOSBox Staging project, so these display options are
    always available at your fingertips, should you ever need them.


### Sharp pixels

If you _really_ prefer sharp pixels over authentic CRT emulation, you only
need to add a single line to the `[render]` config section:

```ini
[render]
glshader = sharp
```

That's it! Switching to the `sharp` shader will also make the image fill the screen
vertically, so no more letterboxing. The reason for this (and a lot more) will
be explained in the [advanced graphics options](advanced-graphics-options.md)
chapter.

Keep in mind, though, that the rest of the guide assumes you're using the CRT
emulation.


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


## Joystick support

Okay, so if we want to play the game with the keyboard, we need to press
++ctrl+k++ to enter keyboard mode every single time we start the game. As
mentioned, that's because by default DOSBox Staging either lets you use your
gamepad as the joystick, or it emulates a PC joystick even if you don't have a
physical game controller plugged in. Either way, the game will always "see" a
joystick at startup and will therefore auto-switch to joystick mode.

From this follows that if we disable the joystick in the config, the game
will have no other choice than to default to keyboard mode:

```ini
[joystick]
joysticktype = disabled
```

But if you want to play the game with your gamepad instead, you can do that
too! DOSBox Staging auto-maps most common game controllers, so you can control
the game with the left analog stick and the **X** button out-of-the-box with most
gamepads.

You'll need to hit diagonals often to jump and crouch. If your left stick can
only move in a circle, the following setting might make this easier:

```ini
[joystick]
circularinput = on
```

You can also adjust the deadzone of your stick if you experience controller
drift. It's 10% by default, so let's increase it to 15% instead:

```ini
[joystick]
deadzone = 15
```

!!! important  "The PC is not a console!"

    Note that Prince of Persia requires a lot of precise control, so playing it
    with the analog stick makes the game quite a bit harder and somewhat
    frustrating. This is a common theme with many DOS games; most of them were
    really optimised for keyboard controls, or require you to use the keyboard
    anyway to perform some essential actions.

    While playing DOS games from the couch with a controller in your hand
    might be cool, for the best results, just stick with the keyboard
    controls. Apart from some space and flight simulators, joystick support
    was often an afterthought in DOS games.


!!! warning "Issues with multiple controllers"

    You might get some weird behaviour or even crashes if you have multiple
    game controllers plugged in. If that's the case, please disconnect all
    game controllers except the one you want to use and restart DOSBox
    Staging. We're aiming to improve multi-controller support in the future.


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

[joystick]
# if you prefer the keyboard; remove it if you want to play with your gamepad
joysticktype = disabled

# optional gamepad adjustments
#circularinput = on
#deadzone = 15

[render]
# for 4K+ monitors only
#integer_scaling = off

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
inform you if something goes wrong. It does that via the *logs*, which are just
a continuous stream of timestamped messages in a separate window.

The behaviour of the log window is different on each platform:

<h3>Windows</h3>

The log window is opened by default when you start DOSBox Staging. You can
hide the log window by passing the `--noconsole` argument to the DOSBox
Staging executable.

<h3>macOS</h3>

DOSBox Staging does not open the log window by default when started via its
application icon, or with the *Start DOSBox Staging* icon you copied from the
`.dmg` installer archive earlier.

To make the log window appear, you need to use another shortcut icon called
_Start DOSBox Staging (logging)_, which you can copy into your game folder
from the same `.dmg` archive (note the word "logging" in the name of the
icon).

!!! note

    Remember, for the first time you'll need to right-click or ++ctrl++-click
    on this icon, select the topmost *Open* menu item, then press the
    *Open* button in the appearing dialog. After the first start, you can simply
    double-click on it.


<h3>Linux</h3>

Start DOSBox Staging from the terminal, and the logs will appear there.

---

Under normal circumstances, the logs contain lots of interesting technical
details about what DOSBox Staging is currently doing, what exact settings it
uses, and so on. For example, this is the start of the logs when running the
game on macOS:

![Example DOSBox logs](https://www.dosbox-staging.org/static/images/getting-started/pop-logs1.png){ loading=lazy }

That's all good and well, but what's even better is if an invalid
configuration setting has been detected, or something went wrong when running
the game, DOSBox Staging will inform us about it in the logs!

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

![Example DOSBox logs showcasing warnings and errors](https://www.dosbox-staging.org/static/images/getting-started/pop-logs2.png){ loading=lazy }

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

---

Okay, we've pretty much maxed out Prince of Persia for demonstration purposes.
Time to move on to another game!

