# General

This chapter covers general emulator settings from the `[dosbox]` and `[sdl]`
configuration sections --- what type of machine to emulate, the amount of
memory, and various general emulator options.

The [`machine`](#machine) setting selects which video adapter to emulate. The
default `svga_s3` (an S3 Trio64 SVGA card) covers the widest range of games.
You'll only need to change it for titles that specifically require CGA, EGA,
Tandy, or Hercules graphics. See
[Graphics adapters](../graphics/adapters.md) for a detailed overview of each
adapter, or the
[Getting Started guide](../../getting-started/setting-up-prince-of-persia.md)
for a practical walkthrough.

The default 16 MB of RAM set via [`memsize`](#memsize) is more than enough for
nearly all DOS software. A few late DOS-era games need 32 MB, but these are
rare exceptions.


## General behaviour

When you switch away from DOSBox Staging (e.g., by pressing Alt+Tab), the
emulator keeps running in the background by default --- sound and all. If
you'd rather not hear game music while you're doing something else,
[`mute_when_inactive`](#mute_when_inactive) silences the audio output whenever
the window loses focus.
[`pause_when_inactive`](#pause_when_inactive) goes a step further and pauses the
entire emulation, which is useful for games that don't have a built-in pause
function.

The [`screensaver`](#screensaver) setting controls whether the OS screensaver is
allowed to activate while DOSBox is running. By default, it's blocked to
prevent the screensaver from kicking in during a long cutscene or a game that
doesn't require constant input.


## VESA and VGA options

The [`vesa_modes`](#vesa_modes) setting controls which VESA video modes are
available beyond the standard VGA modes. The default `compatible` setting
provides the safest set of modes --- it excludes 320x200 high colour modes
(which weren't properly supported until the late 1990s) and certain 256-colour
linear framebuffer modes that cause timing problems in **Build Engine** games
([Duke Nukem 3D](https://www.mobygames.com/game/365/duke-nukem-3d/), [Shadow
Warrior](https://www.mobygames.com/game/1779/shadow-warrior/),
[Blood](https://www.mobygames.com/game/793/blood/)). The `all` setting adds
these extra modes and is sometimes needed by late 1990s demoscene productions.
Use `halfline` only for [Extreme
Assault](https://www.mobygames.com/game/1396/extreme-assault/), which requires
a special halfline VESA mode. See [`vesa_modes`](#vesa_modes) for the full
resolution table.

VGA text mode normally uses 9-pixel-wide character cells.
[`vga_8dot_font`](#vga_8dot_font) forces 8-pixel-wide characters instead. Very
few programs need this.

[`vga_render_per_scanline`](#vga_render_per_scanline) is enabled by default and
emulates accurate per-scanline VGA rendering. A few games crash at startup with
it enabled (Deus, Ishar 3, Robinson's Requiem, Time Warriors) --- disable it on
a per-game basis for those titles.


## Video memory delay

The [`vmem_delay`](#vmem_delay) setting emulates the CPU-throttling effect of
accessing slow video memory via the ISA bus on old video cards. This can fix
flashing graphics and speed issues in Hercules, CGA, EGA, and early VGA games.

Enable it only on a per-game basis as it slows down emulation. The following
games benefit from `vmem_delay = on`:

<div class="compact" markdown>

- [Corncob 3-D (1992)](https://www.mobygames.com/game/40284/corncob-3-d-the-other-worlds-campaign/)
- [Corncob Deluxe (1993)](https://www.mobygames.com/game/3480/corncob-deluxe/)
- Crazy Brix (`vmem_delay = 2000` and `cpu_cycles = 70000`)
- [Future Wars (1989)](https://www.mobygames.com/game/2205/future-wars-adventures-in-time/) (also needs `cpu_cycles = 1000`)
- [Gold of the Aztecs, The (1990)](https://www.mobygames.com/game/17245/the-gold-of-the-aztecs/)
- [Hostages (1990)](https://www.mobygames.com/game/6939/hostage-rescue-mission/) (also needs `cpu_cycles = 1500`)
- [Operation Stealth (1990)](https://www.mobygames.com/game/2236/007-james-bond-the-stealth-affair/) (when VGA or EGA is selected in the game's setup)
- [Quest for Glory II (1990)](https://www.mobygames.com/game/169/quest-for-glory-ii-trial-by-fire/) (fixes the too fast vertical scrolling in the intro)

</div>


## Configuration settings

You can set the general system parameters in the `[dosbox]` configuration
section.


### General behaviour

The settings below are configured in the `[sdl]` section.

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


### Video adapter

##### machine

:   Set the video adapter or machine to emulate.

    Possible values:

    <div class="compact" markdown>

    - `hercules` -- Hercules Graphics Card (HGC) (see
      [`monochrome_palette`](../graphics/rendering.md#monochrome_palette)).
    - `cga_mono` -- CGA adapter connected to a monochrome monitor (see
      [`monochrome_palette`](../graphics/rendering.md#monochrome_palette)).
    - `cga` -- IBM Color Graphics Adapter (CGA). Also enables composite
      video emulation (see
      [Composite video](../graphics/composite-video.md)).
    - `pcjr` -- An IBM PCjr machine. Also enables PCjr sound and composite
      video emulation.
    - `tandy` -- A Tandy 1000 machine with TGA graphics. Also enables Tandy
      sound and composite video emulation.
    - `ega` -- IBM Enhanced Graphics Adapter (EGA).
    - `svga_paradise` -- Paradise PVGA1A SVGA card (no VESA VBE; 512K vmem
      by default, can be set to 256K or 1MB with
      [`vmemsize`](#vmemsize)). This is the closest to IBM's original VGA
      adapter.
    - `svga_et3000` -- Tseng Labs ET3000 SVGA card (no VESA VBE; fixed 512K
      vmem).
    - `svga_et4000` -- Tseng Labs ET4000 SVGA card (no VESA VBE; 1MB vmem
      by default, can be set to 256K or 512K with
      [`vmemsize`](#vmemsize)).
    - `svga_s3` *default*{ .default } -- S3 Trio64 (VESA VBE 2.0; 4MB vmem
      by default, can be set to 512K, 1MB, 2MB, or 8MB with
      [`vmemsize`](#vmemsize)).
    - `vesa_oldvbe` -- Same as `svga_s3` but limited to VESA VBE 1.2.
    - `vesa_nolfb` -- Same as `svga_s3` (VESA VBE 2.0), plus the "no linear
      framebuffer" hack (needed only by a few games).

    </div>

##### vmemsize

:   Video memory in MB (1--8) or KB (256 to 8192). See the
    [`machine`](#machine) setting for the list of valid options and defaults
    per adapter.

    Possible values: `auto` *default*{ .default } (uses the default for the
    selected video adapter), or a specific size in MB or KB.


##### vesa_modes

:   Controls which VESA video modes are available.

    Possible values:

    - `compatible` *default*{ .default } -- Only the most compatible VESA
      modes for the configured video memory size. Recommended with 4 or 8 MB
      of video memory ([`vmemsize`](#vmemsize)) for the widest compatibility
      with games. 320x200 high colour modes are excluded as they were not
      properly supported until the late '90s. The 256-colour linear
      framebuffer 320x240, 400x300, and 512x384 modes are also excluded as
      they cause timing problems in Build Engine games.

    - `halfline` -- Same as `compatible`, but the 120h VESA mode is replaced
      with a special halfline mode used by [Extreme Assault](https://www.mobygames.com/game/1396/extreme-assault/). Use only if
      needed.

    - `all` -- All modes are available, including extra DOSBox-specific VESA
      modes. Use 8 MB of video memory for the best results. Some games
      misbehave in the presence of certain VESA modes; try `compatible` mode
      if this happens. The 320x200 high colour modes available in this mode
      are often required by late '90s demoscene productions.


    The following table shows the available resolutions in `compatible` mode
    and the minimum video memory required for each colour depth. Standard VGA
    modes (320&times;200, 640&times;480, etc.) are always available regardless
    of the `vesa_modes` setting.

    <div class="compact" markdown>

    | Resolution      | 4-bit | 8-bit | 16-bit | 24-bit | 32-bit |
    |-----------------|-------|-------|--------|--------|--------|
    | 640&times;400   | ---   | any   | ---    | ---    | 1 MB   |
    | 640&times;480   | any   | any   | 1 MB   | 1 MB   | 2 MB   |
    | 800&times;600   | any   | any   | 1 MB   | ---    | 2 MB   |
    | 1024&times;768  | any   | 1 MB  | 2 MB   | ---    | 4 MB   |
    | 1152&times;864  | ---   | 1 MB  | 2 MB   | 4 MB   | 4 MB   |
    | 1280&times;960  | 1 MB  | 2 MB  | 4 MB   | 4 MB   | 8 MB   |
    | 1280&times;1024 | 1 MB  | 2 MB  | 4 MB   | 4 MB   | 8 MB   |
    | 1600&times;1200 | 1 MB  | 2 MB  | 4 MB   | 8 MB   | 8 MB   |

    </div>

    The `all` mode adds the following on top of `compatible`:

    <div class="compact" markdown>

    - 320&times;200 and 320&times;240 high colour modes (15/16/24/32-bit)
    - 320&times;400 and 320&times;480 modes in various depths
    - 400&times;300 modes (8/15/16/24-bit)
    - 512&times;384 modes (8/15/16/32-bit)
    - 848&times;480 widescreen modes (8/15/16/32-bit)

    </div>


##### vga_8dot_font

:   Use 8-pixel-wide fonts on VGA adapters.

    Possible values: `on`, `off` *default*{ .default }


##### vga_render_per_scanline

:   Emulate accurate per-scanline VGA rendering. Currently, you need to
    disable this for a few games, otherwise they will crash at startup
    (e.g., [Deus](https://www.mobygames.com/game/5001/deus/),
    [Ishar 3](https://www.mobygames.com/game/7702/ishar-3-the-seven-gates-of-infinity/),
    [Robinson's Requiem](https://www.mobygames.com/game/4797/robinsons-requiem/),
    [Time Warriors](https://www.mobygames.com/game/22845/time-warriors/)).

    Possible values: `on` *default*{ .default }, `off`


##### vmem_delay

:   Set video memory access delay emulation.

    Possible values:

    - `off` *default*{ .default } -- Disable video memory access delay
      emulation. This is preferable for most games to avoid slowdowns.
    - `on` -- Enable video memory access delay emulation (3000 ns). This can
      help reduce or eliminate flicker in Hercules, CGA, EGA, and early VGA
      games.
    - `<number>` -- Set access delay in nanoseconds. Valid range is 0 to
      20000 ns; 500 to 5000 ns is the most useful range.

    !!! note

        Only set this on a per-game basis when necessary as it slows down
        the whole emulator.


### Memory

##### memsize

:   Amount of memory the emulated machine has in MB (`16` by default). Best
    leave at the default setting to avoid problems with some games, though a
    few games might require a higher value. There is generally no speed
    advantage when raising this value.


### Disk speed

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


### DOS & shell

##### autoexec_section

:   How autoexec sections are handled from multiple config files.

    Possible values:

    <div class="compact" markdown>

    - `join` *default*{ .default } -- Combine them into one big section.
    - `overwrite` -- Use the last one encountered, like other config
      settings.

    </div>


##### automount

:   Mount `drives/[c]` folders as drives on startup, where `[c]` is a
    lower-case drive letter from `a` to `y`. The `drives` folder can be
    provided relative to the current folder or via built-in resources. Mount
    settings can be optionally provided using a `[c].conf` file alongside the
    drive's folder.

    Possible values: `on` *default*{ .default }, `off`


##### startup_verbosity

:   Controls verbosity prior to displaying the program.

    Possible values:

    <div class="compact" markdown>

    - `auto` *default*{ .default } -- `low` if exec or dir is passed,
      otherwise `high`.
    - `high` -- Show welcome banner and early stdout.
    - `low` -- Show early stdout only.
    - `quiet` -- Don't show welcome banner or early stdout.

    </div>


##### shell_config_shortcuts

:   Allow shortcuts for simpler configuration management. E.g., instead of
    `config -set sbtype sb16`, it is enough to execute `sbtype sb16`, and
    instead of `config -get sbtype`, you can just execute the `sbtype`
    command.

    Possible values: `on` *default*{ .default }, `off`


##### allow_write_protected_files

:   Many games open all their files with writable permissions; even files that
    they never modify. This setting lets you write-protect those files while
    still allowing the game to read them. A second use-case: if you're using a
    copy-on-write or network-based filesystem, this setting avoids triggering
    write operations for these write-protected files.

    Possible values: `on` *default*{ .default }, `off`


##### mcb_fault_strategy

:   How software-corrupted memory chain blocks should be handled.

    Possible values:

    <div class="compact" markdown>

    - `repair` *default*{ .default } -- Repair (and report) faults using
      adjacent blocks.
    - `report` -- Report faults but otherwise proceed as-is.
    - `allow` -- Allow faults to go unreported (hardware behaviour).
    - `deny` -- Quit (and report) when faults are detected.

    </div>


