# CPU

The [cpu_cycles](#cpu_cycles) setting controls the speed of the emulated CPU;
it's the single most important performance setting in DOSBox Staging. Getting
it right can mean the difference between a game that plays perfectly and one
that stutters, crawls, or races through its intro sequence.

Too many cycles and you'll get sound stuttering and input lag as your host CPU
struggles to keep up. Too few and the game turns into a slideshow. Finding the
sweet spot for each game is part of the charm of retro gaming.

As a rough guide: early 8088-era games
([Alley Cat](https://www.mobygames.com/game/190/alley-cat/),
[Sopwith](https://www.mobygames.com/game/1380/sopwith/)) need around 300
cycles. 386-era titles want roughly 3,000–6,000. 486-class games like
[Doom](https://www.mobygames.com/game/1068/doom/) are playable at around
25,000 but might need up 50,000 cycles for a completely smooth experience.
Late Pentium-era titles such as [Quake](https://www.mobygames.com/game/374/quake/)
need 50,000 to 100,000 cycles or even more. See the [cpu_cycles](#cpu_cycles)
table for further details, or the
[Getting Started guide](../../getting-started/beneath-a-steel-sky.md#adjusting-the-emulated-cpu-speed)
for a practical walkthrough of finding the right speed.

The `auto` setting for [core](#core) is the most sensible default — it uses the faster
`dynamic` recompiler for protected-mode games and the more accurate `normal`
interpreter for real-mode programs. You'll rarely need to override this.

## Protected and real mode

DOS games released before about 1993 use **real mode** (the legacy 16-bit
programming model of the Intel 386), while games from 1993 onwards almost
exclusively use **protected mode** (full 32-bit operation). The important
distinction is that most CPU-hungry games are protected mode games (e.g., FPS
games and flight simulators), while older real mode games generally have much
less demanding performance requirements.

DOSBox Staging detects the current mode automatically and applies different
default speeds:

- **Real mode**: 3000 cycles/ms (roughly a 386SX at 20 MHz)
- **Protected mode**: 60,000 cycles/ms (roughly a Pentium at 90 MHz)

The conservative real mode default prevents older speed-sensitive games from
running too fast. The higher protected mode default gives demanding games the
headroom they need.

You can easily tell which mode a game uses by watching the cycles value in the
DOSBox Staging window's title bar --- it will show 3000 in real mode and
60,000 in protected mode (assuming default settings). You can also spot
protected mode games by the presence of DOS extenders such as `DOS4GW.EXE`,
`PMODEW.EXE`, or `CWSDPMI.EXE` in their game directories.


## Finding the correct cycles setting for a game

While the defaults get most games *running*, manual tweaking is often needed
to make a game run *smoothly*. Setting the cycles too high wastes host CPU
power that could be used for glitch-free audio emulation --- there's no
benefit to emulating a faster CPU than the game actually needs.

Use the [cpu_cycles](#cpu_cycles) table as a starting point: look up what CPU
the game was designed for, set the corresponding cycles value, then fine-tune
from there:

- **Early DOS games** from the 1980s usually run fine at 3,000 cycles (386SX/20
  level)
- For **2D games** from the 1990s, 25,000 cycles (486DX2/66 level) handles
  virtually everything
- **3D games** typically need 50,000--100,000 cycles (Pentium range)
- **3D SVGA gaming** at 640&times;480 or above may require 200,000+ cycles

You can adjust the cycles on the fly while playing with ++ctrl+f11++ to
decrease by 20% and ++ctrl+f12++ to increase by 10% (++cmd+f11++ and
++cmd+f12++ on macOS). Once you've found a good value, update your config
to match.

Always aim for the *lowest* cycles value that gives adequate performance ---
overdoing it only increases the chance of audio glitches and wastes host CPU
resources.

!!! warning "Speed-sensitive games"

    Some games, particularly those from the 1980s and early 1990s, are
    sensitive to CPU speed. Setting cycles too high can cause them to run too
    fast, behave erratically, or crash outright (e.g., "integer divide by 0"
    errors). A few games may even fail sound card detection and refuse to
    start if the emulated CPU is too fast. If a game misbehaves, try
    lowering the cycles before investigating other causes.


For a hands-on walkthrough of finding the right cycles setting for specific
games, the [Getting Started guide](../getting-started/index.md)
walks you through a few a practical examples:

<div class="compact" markdown>

- [Passport to Adventure](../getting-started/passport-to-adventure.md#cpu-sensitive-games)
- [Beneath a Steel Sky](../getting-started/beneath-a-steel-sky.md#adjusting-the-emulated-cpu-speed)
- [Star Wars Dark Forces](../getting-started/star-wars-dark-forces.md#setting-the-emulated-cpu-speed)

</div>



## Configuration examples

**Fixed speed globally** --- Roughly emulates an 486DX2/66 in both real and
protected mode:

``` ini
[cpu]
cpu_cycles = 25000
cpu_cycles_protected = auto
```

**Different real and protected mode speeds** --- 20,000 cycles for real mode,
400,000 for protected mode, with throttling enabled so the host CPU doesn't get
overloaded:

``` ini
[cpu]
cpu_cycles = 20000
cpu_cycles_protected = 400000
cpu_throttle = on
```

**Max speed** --- As fast as your host CPU can handle. Useful when compiling
programs, rendering 3D images, or playing late-90s 3D games at high
resolutions:

``` ini
[cpu]
cpu_cycles = max
cpu_cycles_protected = max
```

!!! warning

    Using `max` cycles can cause problems: some games crash or misbehave
    when the emulated CPU is "too fast". The effective speed varies across
    different host machines, and audio glitches are common. It's best to use
    the lowest fixed cycles value that runs the game at an acceptable speed.


## Configuration settings

You can set the CPU emulation parameters in the `[cpu]` configuration
section.


##### core

:   Type of CPU emulation core to use.

    Possible values:

    - `auto` *default*{ .default } -- `normal` core for real mode programs,
      `dynamic` core for protected mode programs. Most programs will run
      correctly with this setting.

    - `normal` -- The DOS program is interpreted instruction by instruction.
      This yields the most accurate timings, but puts 3--5 times more load on
      the host CPU compared to the `dynamic` core. Therefore, it's generally
      only recommended for real mode programs that don't need a fast emulated
      CPU or are timing-sensitive. The `normal` core is also necessary for
      programs that self-modify their code.

    - `simple` -- The `normal` core optimised for old real mode programs; it
      might give you slightly better compatibility with older games.
      Auto-switches to the `normal` core in protected mode.

    - `dynamic` -- The instructions of the DOS program are translated to host
      CPU instructions in blocks and are then executed directly. This puts
      3--5 times less load on the host CPU compared to the `normal` core, but
      the timings might be less accurate. The `dynamic` core is a necessity
      for demanding DOS programs (e.g., 3D SVGA games). Programs that
      self-modify their code might misbehave or crash on the `dynamic` core;
      use the `normal` core for such programs.


##### cpu_cycles

:   Speed of the emulated CPU (`3000` by default). If
    [cpu_cycles_protected](#cpu_cycles_protected) is on `auto`, this sets the
    cycles for both real and protected mode programs.

    Possible values:

    - `<number>` -- Emulate a fixed number of cycles per millisecond (roughly
      equivalent to MIPS). Valid range is from 50 to 2000000.
    - `max` -- Emulate as many cycles as your host CPU can handle on a single
      core. The number of cycles per millisecond can vary; this might cause
      issues in some DOS programs.

    Ballpark cycles values for common CPUs (treat as starting points, then
    fine-tune per game):

    <div class="compact" markdown>

    | Emulated CPU      |  MHz | Cycles | Year
    |-------------------|-----:|-------:|-----:
    | 8088              | 4.77 |    300 | 1981
    | 286               |    8 |    700 | 1984
    | 286               |   12 |   1500 | 1986
    | 286               |   25 |   3000 | 1988
    | 386DX             |   25 |   4500 | 1988
    | 386DX             |   33 |   6000 | 1989
    | 386DX             |   40 |   9000 | 1991
    | 486DX             |   33 |  12000 | 1990
    | 486DX2            |   66 |  25000 | 1992
    | 486DX4            |  100 |  35000 | 1994
    | Intel Pentium     |   90 |  50000 | 1994
    | Intel Pentium     |  100 |  60000 | 1994
    | Intel Pentium     |  120 |  75000 | 1995
    | Intel Pentium     |  133 |  80000 | 1995
    | Intel Pentium MMX |  166 | 100000 | 1997
    | Intel Pentium II  |  300 | 200000 | 1997
    | Intel Pentium III |  866 | 400000 | 2000

    </div>

    !!! note

        - Setting the CPU speed to `max` or to high fixed values may result
          in sound drop-outs and general lagginess.

        - Set the lowest fixed cycles value that runs the game at an
          acceptable speed for the best results.


##### cpu_cycles_protected

:   Speed of the emulated CPU for protected mode programs only.

    Possible values:

    - `auto` -- Use the [cpu_cycles](#cpu_cycles) setting.
    - `<number>` -- Emulate a fixed number of cycles per millisecond (roughly
      equivalent to MIPS). Valid range is from 50 to 2000000.
    - `max` -- Emulate as many cycles as your host CPU can handle on a single
      core.

    The default is `60000`.

    !!! note

        See [cpu_cycles](#cpu_cycles) for further info.


##### cpu_idle

:   Reduce the CPU usage in the DOS shell and in some applications when
    DOSBox is idle. This is done by emulating the HLT CPU instruction, so it
    might interfere with other power management tools such as DOSidle and
    FDAPM when enabled.

    Possible values: `on` *default*{ .default }, `off`


##### cpu_throttle

:   Throttle down the number of emulated CPU cycles dynamically if your host
    CPU cannot keep up. Only affects fixed cycles settings. When enabled, the
    number of cycles per millisecond can vary; this might cause issues in
    some DOS programs.

    Possible values: `on`, `off` *default*{ .default }


##### cputype

:   CPU type to emulate. You should only change this if the program doesn't
    run correctly on `auto`.

    Possible values:

    <div class="compact" markdown>

    - `auto` *default*{ .default } -- The fastest and most compatible
      setting. Technically, this is `386_fast` plus 486 CPUID, 486 CR
      register behaviour, and extra 486 instructions.
    - `386` -- 386 CPUID and 386-specific page access level calculation.
    - `386_fast` -- Same as `386` but with loose page privilege checks which
      is much faster.
    - `386_prefetch` -- Same as `386_fast` plus accurate CPU prefetch queue
      emulation. Requires `core = normal`. This setting is necessary for
      programs that self-modify their code or employ anti-debugging tricks.
      Games that require `386_prefetch` include Contra, FIFA International
      Soccer (1994), Terminator 1, and X-Men: Madness in The Murderworld.
    - `486` -- 486 CPUID, 486+ specific page access level calculation, 486 CR
      register behaviour, and extra 486 instructions.
    - `pentium` -- Same as `486` but with Pentium CPUID, Pentium CR register
      behaviour, and RDTSC instruction support. Recommended for Windows 3.1
      games (e.g., Betrayal in Antara).
    - `pentium_mmx` -- Same as `pentium` plus MMX instruction set support.
      Very few games use MMX instructions; it's mostly only useful for
      demoscene productions.

    </div>


##### cycledown

:   Number of cycles to subtract with the `Dec Cycles`
    [hotkey](../appendices/shortcuts.md) (`20` by default). Values lower than
    100 are treated as a percentage decrease.


##### cycleup

:   Number of cycles to add with the `Inc Cycles`
    [hotkey](../appendices/shortcuts.md) (`10` by default). Values lower than
    100 are treated as a percentage increase.
