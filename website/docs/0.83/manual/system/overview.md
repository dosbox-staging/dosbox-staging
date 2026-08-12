# Overview

At its core, DOSBox Staging emulates a PC: a CPU running at a configurable
speed, memory laid out the way DOS software expects, and a DOS environment
sitting on top of both. [CPU](cpu.md), [Memory](memory.md), and [DOS](dos.md)
cover those three pieces. [Machine types](machine-types.md) covers PCjr and
Tandy as complete machines, and [Localisation](localisation.md) covers
language, country, and keyboard settings.

The rest of this page covers general emulator behaviour that doesn't
belong anywhere else: window focus, the screensaver, and disk speed. Most
of this is just about how DOSBox Staging behaves as an application on your
desktop, but disk speed emulation is the exception.

## General behaviour

When you switch away from DOSBox Staging (e.g., by pressing ++alt+tab++), the
emulator keeps running in the background by default, sound and all. If you'd
rather not hear game music while you're doing something else,
[`mute_when_inactive`](#mute_when_inactive) silences the audio output whenever
the window loses focus. [`pause_when_inactive`](#pause_when_inactive) goes a
step further and pauses the entire emulation, which is useful for games that
don't have a built-in pause function.

The [`screensaver`](#screensaver) setting controls whether the OS screensaver
is allowed to activate while DOSBox Staging is running. By default, it's
blocked to prevent the screensaver from kicking in during a long cutscene or a
game that doesn't require constant input.

## Disk speed

By default, DOSBox Staging reads and writes floppy and hard disk data almost
instantly. That's fine for most games, and actually advantageous when
installing games that copy large files to the C: drive, but a couple of
situations call for slowing it down to match real period hardware:

- **PCjr booter games.** Many early PCjr titles [boot directly](../using-dosbox-staging/storage.md#booting-from-images) off floppy
    disk rather than from DOS, and some of these were timed against the PCjr's
    real (slow) floppy drive. Setting
    [`floppy_disk_speed`](#floppy_disk_speed) to a period-correct value can
    help these run and load the way they were designed to.

- **Games that measure disk I/O speed at startup.** A handful of titles ---
  [Deus](https://www.mobygames.com/game/5001/deus/),
  [Ishar 3](https://www.mobygames.com/game/7702/ishar-3-the-seven-gates-of-infinity/),
  [Robinson's Requiem](https://www.mobygames.com/game/4797/robinsons-requiem/),
  [Time Warriors](https://www.mobygames.com/game/22845/time-warriors/), and
  the 1993 CD-ROM version of
  [Dragon's Lair](https://www.mobygames.com/game/1503/dragons-lair/) --- run
  a "speed check" on disk I/O at startup and refuse to run if it looks too
  fast, since DOSBox Staging's default disk I/O is unrealistically fast
  compared to period hardware. Setting `hard_disk_speed = fast` fixes the
  check and, as a side effect, lets you run these games with much higher
  CPU cycles settings (100,000--150,000 range).

- **Authenticity.** If you want the experience of period-correct floppy
  swapping and load times rather than a modern instant-load feel. Throttled
  disk speed also pairs naturally with [disk noise](../sound/disk-noise.md) ---
  the two settings work in tandem, since the clicking and spinning sounds
  only feel authentic when access isn't instantaneous.

These are edge cases --- leave [`floppy_disk_speed`](#floppy_disk_speed) and
[`hard_disk_speed`](#hard_disk_speed) at `maximum` unless a specific game
needs otherwise.


## Configuration settings

### Window & focus behaviour

You can set these in the `[sdl]` configuration sections.

##### mute_when_inactive

:   Mute the sound when the window is inactive.

    Possible values: `on`, `off` *default*{ .default }


##### pause_when_inactive

:   Pause emulation when the window is inactive.

    Possible values: `on`, `off` *default*{ .default }


##### screensaver

:   Use `allow` or `block` to override the `SDL_VIDEO_ALLOW_SCREENSAVER`
    environment variable which usually blocks the OS screensaver while the
    emulator is running.

    Possible values: `auto` *default*{ .default }, `allow`, `block`


### Disk speed

You can set these in the `[dosbox]` configuration sections.

##### floppy_disk_speed

:   Set the emulated floppy disk speed.

    Possible values:

    <div class="compact" markdown>

    - `maximum` *default*{ .default } -- As fast as possible, no slowdown.
    - `fast` -- Extra-high density (ED) floppy speed (~120 kB/s).
    - `medium` -- High density (HD) floppy speed (~60 kB/s).
    - `slow` -- Double density (DD) floppy speed (~30 kB/s).

    </div>


##### hard_disk_speed

:   Set the emulated hard disk speed.

    Possible values:

    <div class="compact" markdown>

    - `maximum` *default*{ .default } -- As fast as possible, no slowdown.
    - `fast` -- Typical mid-1990s hard disk speed (~15 MB/s).
    - `medium` -- Typical early 1990s hard disk speed (~2.5 MB/s).
    - `slow` -- Typical 1980s hard disk speed (~600 kB/s).

    </div>
