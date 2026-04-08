# CPU

## Overview

The `cpu_cycles` setting controls the speed of the emulated CPU — it's the
single most important performance setting in DOSBox Staging. Getting it
right can mean the difference between a game that plays perfectly and one
that stutters, crawls, or races through its intro sequence.

Too many cycles and you'll get sound stuttering and input lag as your host
CPU struggles to keep up. Too few and the game turns into a slideshow.
Finding the sweet spot for each game is part of the charm of retro gaming.

As a rough guide: early 8088-era games (*Alley Cat*, *Sopwith*) need around
300 cycles. 386-era titles want roughly 3,000–6,000. 486-class games like
*Doom* are happy at around 25,000. Late Pentium-era titles such as *Quake*
can eat 50,000 cycles or more.

The `auto` setting for `core` is the sensible default — it uses the faster
`dynamic` recompiler for protected-mode games and the more accurate `normal`
interpreter for real-mode programs. You'll rarely need to override this.


## Configuration examples

**Fixed speed globally** --- Roughly emulates an i486DX2-66 in both real and
protected mode:

``` ini
[cpu]
cpu_cycles = 25000
cpu_cycles_protected = auto
```

**Different real and protected mode speeds** --- 20k cycles for real mode,
400k for protected mode, with throttling enabled so the host CPU doesn't get
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
    when the emulated CPU is "too fast", the effective speed varies across
    different host machines, and audio glitches are common. It's best to use
    the lowest fixed cycles value that runs the game at an acceptable speed.


## Pentium MMX

Setting `cputype = pentium_mmx` enables Pentium MMX instruction set emulation
for late-90s demoscene productions and games with MMX-specific enhancements
(e.g., **Extreme Assault**, **Z.A.R.**). It also enables the MMX-only
real-time resonant filters in **Impulse Tracker**.


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

    TODO(CL) add year or release to the table for each CPU (extra last column)

    | Emulated CPU      |  MHz | Cycles
    |-------------------|-----:|-------:
    | 8088              | 4.77 |    300
    | 286               |    8 |    700
    | 286               |   12 |   1500
    | 286               |   25 |   3000
    | 386DX             |   25 |   4500
    | 386DX             |   33 |   6000
    | 386DX             |   40 |   9000
    | 486DX             |   33 |  12000
    | 486DX2            |   66 |  25000
    | 486DX4            |  100 |  35000
    | Intel Pentium     |   90 |  50000
    | Intel Pentium     |  100 |  60000
    | Intel Pentium     |  120 |  75000
    | Intel Pentium     |  133 |  80000
    | Intel Pentium MMX |  166 | 100000
    | Intel Pentium II  |  300 | 200000
    | Intel Pentium III |  866 | 400000

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
      behaviour, and RDTSC instruction support. Recommended for Windows 3.x
      games (e.g., Betrayal in Antara).
    - `pentium_mmx` -- Same as `pentium` plus MMX instruction set support.
      Very few games use MMX instructions; it's mostly only useful for
      demoscene productions.

    </div>


##### cycledown

:   Number of cycles to subtract with the `Dec Cycles`
    [hotkey](../shortcuts.md) (`20` by default). Values lower than 100 are
    treated as a percentage decrease.


##### cycleup

:   Number of cycles to add with the `Inc Cycles` [hotkey](../shortcuts.md)
    (`10` by default). Values lower than 100 are treated as a percentage
    increase.
