# General

## Overview

The `[dosbox]` section contains the core system settings — what type of
machine to emulate, how much memory to give it, and various behaviours that
affect the overall emulation.

The `machine` setting selects which video adapter to emulate. The default
`svga_s3` (an S3 Trio64 SVGA card) covers the widest range of games.
You'll only need to change it for titles that specifically require CGA, EGA,
Tandy, or Hercules graphics — the
[Getting Started guide](../../getting-started/setting-up-prince-of-persia.md)
walks through a practical example of this.

The default 16 MB of RAM is more than enough for nearly all DOS software.
A few late-era games want 32 MB or even 64 MB, but these are rare
exceptions.

Disk speed can be throttled for games that break when disk access is
instantaneous — some titles use disk loading time as a crude timing
mechanism. Most users won't need to touch these settings at all.


## Video memory delay

The [vmem_delay](#vmem_delay) setting emulates the CPU-throttling effect of
accessing slow video memory via the ISA bus on old video cards. This can fix
flashing graphics and speed issues in Hercules, CGA, EGA, and early VGA games.

Enable it only on a per-game basis as it slows down emulation. The following
games benefit from `vmem_delay = on`:

<div class="compact" markdown>

- **Corncob 3-D**
- **Corncob Deluxe**
- **Crazy Brix** (`vmem_delay = 2000` and `cpu_cycles = 70000`)
- **Future Wars** (also needs `cpu_cycles = 1000`)
- **Gold of the Aztecs, The**
- **Hostages** (also needs `cpu_cycles = 1500`)
- **Operation Stealth** (when VGA or EGA is selected in the game's setup)
- **Quest for Glory II** (fixes too-fast vertical scrolling in the intro)

</div>


## Configuration settings

You can set the general system parameters in the `[dosbox]` configuration
section.


##### allow_write_protected_files

:   Many games open all their files with writable permissions; even files that
    they never modify. This setting lets you write-protect those files while
    still allowing the game to read them. A second use-case: if you're using a
    copy-on-write or network-based filesystem, this setting avoids triggering
    write operations for these write-protected files.

    Possible values: `on` *default*{ .default }, `off`


##### autoexec_section

:   How autoexec sections are handled from multiple config files.

    Possible values:

    <div class="compact" markdown>

    - `join` *default*{ .default } -- Combine them into one big section.
    - `overwrite` -- Use the last one encountered, like other config
      settings.

    </div>


##### automount

:   Mount `drives/[c]` directories as drives on startup, where `[c]` is a
    lower-case drive letter from `a` to `y`. The `drives` folder can be
    provided relative to the current directory or via built-in resources.
    Mount settings can be optionally provided using a `[c].conf` file
    alongside the drive's directory.

    Possible values: `on` *default*{ .default }, `off`


##### dos_rate

:   Override the emulated DOS video mode's refresh rate with a custom rate.

    Possible values:

    - `default` *default*{ .default } -- Don't override; use the emulated
      DOS video mode's refresh rate.
    - `host` -- Override the refresh rate of all DOS video modes with the
      refresh rate of your monitor. This might allow you to play some 70 Hz
      VGA games with perfect [vsync](../graphics/display-and-window.md#vsync)
      on a 60 Hz fixed refresh rate monitor.
    - `<number>` -- Override the refresh rate of all DOS video modes with a
      fixed rate specified in Hz (valid range is from 24.000 to 1000.000).
      This is a niche option for a select few fast-paced mid to late 1990s 3D
      games for high refresh rate gaming.

    !!! important

        Many games will misbehave when overriding the DOS video mode's
        refresh rate with non-standard values. This can manifest in glitchy
        video, sped-up or slowed-down audio, jerky mouse movement, mouse
        button presses not being registered, and even gameplay bugs.
        Overriding the DOS refresh rate is a hack that only works acceptably
        with a small subset of all DOS games (typically mid to late 1990s
        games).


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


##### language

:   Select the DOS messages language.

    Possible values:

    - `auto` *default*{ .default } -- Detect the language from the host OS.
    - `<value>` -- Load a translation from the given file.

    !!! note

        The following language files are available: `de`, `en`, `es`, `fr`,
        `it`, `nl`, `pl`, `pt_BR`, and `ru`. English is built-in; the rest
        is stored in the bundled `resources/translations` directory.


##### machine

:   Set the video adapter or machine to emulate.

    Possible values:

    <div class="compact" markdown>

    - `hercules` -- Hercules Graphics Card (HGC) (see
      [monochrome_palette](../graphics/rendering.md#monochrome_palette)).
    - `cga_mono` -- CGA adapter connected to a monochrome monitor (see
      [monochrome_palette](../graphics/rendering.md#monochrome_palette)).
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
      [vmemsize](#vmemsize)). This is the closest to IBM's original VGA
      adapter.
    - `svga_et3000` -- Tseng Labs ET3000 SVGA card (no VESA VBE; fixed 512K
      vmem).
    - `svga_et4000` -- Tseng Labs ET4000 SVGA card (no VESA VBE; 1MB vmem
      by default, can be set to 256K or 512K with
      [vmemsize](#vmemsize)).
    - `svga_s3` *default*{ .default } -- S3 Trio64 (VESA VBE 2.0; 4MB vmem
      by default, can be set to 512K, 1MB, 2MB, or 8MB with
      [vmemsize](#vmemsize)).
    - `vesa_oldvbe` -- Same as `svga_s3` but limited to VESA VBE 1.2.
    - `vesa_nolfb` -- Same as `svga_s3` (VESA VBE 2.0), plus the "no linear
      framebuffer" hack (needed only by a few games).

    </div>


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


##### memsize

:   Amount of memory the emulated machine has in MB (`16` by default). Best
    leave at the default setting to avoid problems with some games, though a
    few games might require a higher value. There is generally no speed
    advantage when raising this value.


##### shell_config_shortcuts

:   Allow shortcuts for simpler configuration management. E.g., instead of
    `config -set sbtype sb16`, it is enough to execute `sbtype sb16`, and
    instead of `config -get sbtype`, you can just execute the `sbtype`
    command.

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


##### vesa_modes

:   Controls which VESA video modes are available.

    Possible values:

    - `compatible` *default*{ .default } -- Only the most compatible VESA
      modes for the configured video memory size. Recommended with 4 or 8 MB
      of video memory ([vmemsize](#vmemsize)) for the widest compatibility
      with games. 320x200 high colour modes are excluded as they were not
      properly supported until the late '90s. The 256-colour linear
      framebuffer 320x240, 400x300, and 512x384 modes are also excluded as
      they cause timing problems in Build Engine games.

    - `halfline` -- Same as `compatible`, but the 120h VESA mode is replaced
      with a special halfline mode used by Extreme Assault. Use only if
      needed.

    - `all` -- All modes are available, including extra DOSBox-specific VESA
      modes. Use 8 MB of video memory for the best results. Some games
      misbehave in the presence of certain VESA modes; try `compatible` mode
      if this happens. The 320x200 high colour modes available in this mode
      are often required by late '90s demoscene productions.


##### vga_8dot_font

:   Use 8-pixel-wide fonts on VGA adapters.

    Possible values: `on`, `off` *default*{ .default }


##### vga_render_per_scanline

:   Emulate accurate per-scanline VGA rendering. Currently, you need to
    disable this for a few games, otherwise they will crash at startup
    (e.g., Deus, Ishar 3, Robinson's Requiem, Time Warriors).

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


##### vmemsize

:   Video memory in MB (1--8) or KB (256 to 8192). See the
    [machine](#machine) setting for the list of valid options and defaults
    per adapter.

    Possible values: `auto` *default*{ .default } (uses the default for the
    selected video adapter), or a specific size in MB or KB.
