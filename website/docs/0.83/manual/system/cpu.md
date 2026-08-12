# CPU

DOSBox Staging doesn't emulate any specific real-world CPU. Instead, it
emulates a generic x86 processor whose speed can be adjusted to match the
requirements of different DOS programs.

This approach works because DOS gaming spans almost two decades of Intel CPUs
--- the **8088** (1981), **286** (1984), **386** (1985), **486** (1989),
**Pentium** (1993), and **Pentium MMX** (1996) --- each generation faster than
the last, but all built on the same underlying x86 instruction set. This is
exactly why DOSBox Staging can get away with emulating one generic,
configurable-speed CPU instead of a specific historical chip: the instruction
set is backward compatible across the whole range, and what actually varies
from game to game is usually just how fast those instructions are expected to
execute.

!!! tip "The legendary Intel backwards compatibility"

    Later Intel CPUs are nearly fully backward compatible with earlier ones
    --- in principle, a Pentium can run code written for the original 8088. A
    handful of obscure or undocumented opcodes behave differently across
    generations, but for practical purposes, decades of Intel x86 software
    just keep working on newer hardware.


## CPU cycles

The [`cpu_cycles`](#cpu_cycles) and
[`cpu_cycles_protected`](#cpu_cycles_protected) settings determine how many
CPU instructions per millisecond DOSBox Staging attempts to emulate. Getting
these settings right is the single most important configuration setting in
DOSBox Staging: it can mean the difference between a game that plays perfectly
and one that stutters, crawls, or races through its animations and intro
sequences.

DOSBox Staging has no reliable way of knowing how fast a 1993 flight simulator
or a 1985 platformer expects to run, so the cycles settings often need to be
adjusted manually. Too many cycles and you'll get sound stuttering and input
lag as your host CPU struggles to keep up. Too few and the game turns into a
slideshow. Finding the sweet spot for each game is part of the charm of retro
gaming --- see [Finding the correct cycles setting for a
game](#finding-the-correct-cycles-setting-for-a-game) below for a practical
approach, or the [Getting Started
guide](../../getting-started/beneath-a-steel-sky.md#adjusting-the-emulated-cpu-speed)
for a hands-on walkthrough.


## Protected and real mode

DOS games released before about 1993 use **real mode** --- the only mode the
Intel 286 and earlier CPUs had, and a legacy 16-bit holdover on the Intel 386
and later. Games from 1993 onwards almost exclusively use **protected mode**,
the 386's native full 32-bit operation. The important distinction is that most
CPU-hungry games are protected mode games (e.g., FPS games and flight
simulators), while older real mode games generally have much less demanding
performance requirements.

There is no universally reliable way to determine how much emulated CPU
performance a DOS program needs. Rather than trying to guess from complex
runtime behaviour, DOSBox Staging uses a straightforward heuristic: it detects
whether the program is running in real or protected mode and chooses the
corresponding cycles values. The [`cpu_cycles`](#cpu_cycles) setting specifies the real
mode cycles value and [`cpu_cycles_protected`](#cpu_cycles_protected) the
protected mode value.

The defaults are:

- **Real mode**: 3000 cycles/ms (roughly a 386SX at 20 MHz) 
- **Protected mode**: 60,000 cycles/ms (roughly a Pentium at 90 MHz)

The conservative real mode default prevents older speed-sensitive games from
the 1980s and early 1990s from running too fast. The higher protected mode
default gives demanding games from the mid to late 1990s plenty of headroom.

You can easily tell which mode a game is using by watching the displayed
cycles value in the DOSBox Staging window's title bar --- it will show 3000 in
real mode and 60,000 in protected mode (assuming default settings). You can
also spot protected mode games by the presence of DOS extenders such as
`DOS4GW.EXE`, `PMODEW.EXE`, or `CWSDPMI.EXE` in their game directories.


## CPU throttling

If you're running DOSBox Staging on a slower computer that struggles with high
cycles settings (this usually manifests as game slowdowns and audio
stuttering), it's worth trying the [`cpu_throttle`](#cpu_throttle) setting.

When enabled, DOSBox Staging dynamically reduces the number of emulated CPU
cycles if your host CPU cannot keep up. As a result, the effective cycles per
millisecond can vary over time, which may cause timing issues in some DOS
programs. This is why the setting is disabled by default.


## CPU emulation core

The [`core`](#core) setting controls how emulated CPU instructions are
executed. The default `auto` mode chooses between two implementations
depending on whether a program is running in [real or protected
mode](#protected-and-real-mode): the faster `dynamic` recompiler for
protected mode programs, and the more accurate `normal` interpreter for
real mode programs.

You'll rarely need to override this behaviour --- see the [`core`](#core)
setting reference below for the other available options and when they're
useful.

If you're running a real mode program at a high
[`cpu_cycles`](#cpu_cycles) setting, consider setting `core = dynamic`
manually for improved performance. As a rule of thumb, the dynamic core is
recommended above about 20,000 cycles.


## Finding the correct cycles setting for a game

While the defaults get most games running, manual tweaking is often needed
to make a game run smoothly. Setting the cycles too high wastes host CPU
power that could be used for glitch-free audio emulation --- there's no
benefit to emulating a faster CPU than the game actually needs.

Use the [`cpu_cycles`](#cpu_cycles) table as a starting point --- look up what
CPU the game was designed for, set the corresponding cycles value, then
fine-tune from there:

- **Early 8088-era games** from the early 1980s
  ([Alley Cat](https://www.mobygames.com/game/190/alley-cat/),
  [Sopwith](https://www.mobygames.com/game/1380/sopwith/)) need around 140 to
  300 cycles. Often you'll need to zero in on a specific cycles value (e.g.
  170) to ensure correct gameplay and music playback speed --- even a 10--20
  cycles difference can make a big impact.

- **286/386-era games** from the mid-to-late 1980s usually run fine at
  1,500--6,000 cycles (286 to 386DX/33 level). These games are usually
  much less speed sensitive --- the default 3,000 cycles value
  should generally serve you well, but you might want to go a bit lower 
  if the animations are too fast, and higher if the gameplay is sluggish.

- **2D games** from the 1990s generally run well at 25,000 cycles
  (486DX2/66 level). You might want to drop to the 10,000--15,000 range if the
  game runs too fast. 

- **3D games** typically need 50,000--100,000 cycles (Pentium range).
  [Doom](https://www.mobygames.com/game/1068/doom/) needs around 60,000 for a
  completely smooth experience. Late Pentium-era titles such as
  [Quake](https://www.mobygames.com/game/374/quake/) sit at the upper end of
  this or beyond, depending on the screen resolution.

- **3D SVGA gaming** at 640&times;480 or above usually requires 200,000 to
  400,000 cycles. It's recommended to enable the
  [`cpu_throttle`](#cpu_throttle) setting for these games.

You can fine-tune the cycles setting while playing the game with the
[`cycleup`](#cycleup) and [`cycledown`](#cycledown)
[hotkeys](../appendices/shortcuts.md#cpu): by default, ++ctrl+f11++ decreases
cycles by 20% and ++ctrl+f12++ increases them by 10% (++cmd+f11++ and
++cmd+f12++ on macOS). Once you've found a good value, update your config to
match the cycles value shown in the DOSBox Staging window's title bar. If
you'd prefer the hotkeys to adjust the cycles by a different amount, you can
change this with the [`cycleup`](#cycleup) and [`cycledown`](#cycledown)
settings (also see the [Configuration settings](#configuration-settings)
below).

Always aim for the lowest cycle value that gives adequate performance — going
higher only increases the chance of audio glitches and wastes host CPU
resources.

!!! warning "Speed-sensitive games"

    Some games, particularly those from the 1980s and early 1990s, are
    sensitive to CPU speed. Setting cycles too high can cause them to run too
    fast, behave erratically, or crash outright (e.g., "integer divide by 0"
    errors). A few games may even fail sound card detection and refuse to
    start if the emulated CPU is too fast. If a game misbehaves, try lowering
    the cycles before investigating other causes. Lastly, a small number of
    games misbehave if you change the cycles setting while the game is
    running.


For a hands-on walkthrough of finding the right cycles setting for specific
games, the [Getting Started guide](../../getting-started/index.md)
walks you through a few practical examples:

<div class="compact" markdown>

- [Passport to Adventure](../../getting-started/passport-to-adventure.md#cpu-sensitive-games)
- [Beneath a Steel Sky](../../getting-started/beneath-a-steel-sky.md#adjusting-the-emulated-cpu-speed)
- [Star Wars Dark Forces](../../getting-started/star-wars-dark-forces.md#setting-the-emulated-cpu-speed)

</div>



## Configuration examples

**Fixed speed globally** --- Roughly emulates a 486DX2/66 in both real and
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

!!! danger "Beware of `max` cycles!"

    Using `max` cycles can cause problems because it does not specify a fixed
    emulated CPU speed. Instead, DOSBox Staging continually increases the
    cycle rate until the host CPU becomes the limiting factor
    ([`cpu_throttle`](#cpu_throttle) is implicitly always enabled). The
    resulting cycle rate therefore depends on the performance of the host
    machine, and fluctuates as the available CPU capacity changes.

    This also means the same game can behave differently on different computers.
    A game that runs correctly with `max` on a slower computer might run too fast,
    crash, or otherwise misbehave on a faster computer because DOSBox Staging can
    emulate more cycles. Even on the same computer, fluctuations in the effective
    cycle rate can cause timing problems or audio glitches.

    For reliable results, avoid using `max` for games that are sensitive to CPU
    speed. Instead, use the lowest fixed cycles value that provides adequate
    performance. This gives the game a consistent emulated CPU speed regardless of
    the host computer.




## CPU type

Separately from the emulated CPU speed (cycles), the [`cputype`](#cputype)
setting controls which CPU architecture is emulated --- essentially, which
instructions the emulated processor understands. The default `auto` is a
generic 386/486 emulation that's compatible with the vast majority of DOS
games. You'll only need to touch `cputype` for a handful of special cases:

- **Windows 3.1** runs best with [`cputype`](#cputype) set to `pentium`, which
  adds RDTSC instruction support (e.g., [Betrayal in
  Antara](https://www.mobygames.com/game/1763/betrayal-in-antara/)).

- **Demoscene productions** occasionally need `pentium_mmx` for their MMX
  instruction support (e.g., [heaven seven by
  Exceed](https://www.pouet.net/prod.php?which=5)). Very few actual games use
  MMX.

- **Self-modifying code or anti-debugging tricks** need `386_prefetch` and
  also `core = normal`. Known games needing this include
  [Contra](https://www.mobygames.com/game/60474/contra/), [FIFA
  International
  Soccer](https://www.mobygames.com/game/155/fifa-international-soccer/),
  [Terminator 1](https://www.mobygames.com/game/1543/the-terminator/), and
  [X-Men: Madness in The
  Murderworld](https://www.mobygames.com/game/6162/x-men-madness-in-murderworld/).

If a game runs incorrectly on `auto`, check whether it falls into one of
these categories before assuming the problem is cycles-related.


## FPU

DOSBox Staging always emulates a **floating-point unit (FPU)** alongside the
CPU --- there's no setting to disable it.

The overwhelming majority of DOS games use integer arithmetic and never touch
the FPU, but a handful rely on it heavily.
[Quake](https://www.mobygames.com/game/374/quake/) is the classic example, and
flight and space simulators often use the FPU extensively for 3D rendering and
physics calculations, too.

Windows 3.1 applications also tend to make greater use of the FPU than typical
DOS software.

!!! danger "IEEE 754 80-bit extended precision floating point emulation"

    One particularly risky area is engineering software that requires accurate
    80-bit extended precision x87 FPU emulation to function correctly. Support
    for 80-bit floats is not available on all platforms that DOSBox Staging
    runs on. The logs will warn you about this at startup:

        FPU: Using reduced-precision floating-point

    Do note, however, that the lack of such log messages **does not** imply or
    guarantee bug-free operation!

!!! warning "No warranties!"

    Although we do our best to emulate the DOS environment and legacy IBM PC
    hardware as accurately as we can, **we cannot guarantee** DOSBox Staging
    has zero bugs or can run every single DOS software ever written 100%
    correctly. This is especially true for engineering software that relies on
    accurate x87 FPU emulation which we **do not provide** in our emulator!

    **Under no circumstances** should DOSBox Staging be used for professional
    applications, especially where DOS software malfunctioning due to
    emulation bugs or inaccuracies could result in significant financial loss,
    data loss, or putting living beings at risk.

    Neither the members of the DOSBox Staging team nor our contributors can be
    held responsible for such unfortunate accidents resulting from the misuse
    of our software. DOSBox Staging is intended for **personal use only** in
    low-stakes scenarios, such as playing DOS games, watching demoscene
    productions, or researching the history of IBM PC compatibles and the DOS
    software catalogue.

    If you disregard this and get into trouble, **you’re on your own!**

## Idle CPU usage

The [`cpu_idle`](#cpu_idle) setting reduces host CPU usage when DOSBox is idle
(e.g., waiting for input at the DOS prompt) by emulating the HLT instruction.
It's enabled by default. Disable it if it conflicts with DOS power management
tools like DOSidle or FDAPM.


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


##### cpu_cycles

:   Speed of the emulated CPU (`3000` by default). If
    [`cpu_cycles_protected`](#cpu_cycles_protected) is on `auto`, this sets the
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

    - `auto` -- Use the [`cpu_cycles`](#cpu_cycles) setting.
    - `<number>` -- Emulate a fixed number of cycles per millisecond (roughly
      equivalent to MIPS). Valid range is from 50 to 2000000.
    - `max` -- Emulate as many cycles as your host CPU can handle on a single
      core.

    The default is `60000`.

    !!! note

        See [`cpu_cycles`](#cpu_cycles) for further info.


##### cpu_throttle

:   Throttle down the number of emulated CPU cycles dynamically if your host
    CPU cannot keep up. Only affects fixed cycles settings. When enabled, the
    number of cycles per millisecond can vary; this might cause issues in
    some DOS programs.

    Possible values: `on`, `off` *default*{ .default }


##### cycledown

:   Number of cycles to subtract with the `Dec Cycles`
    [hotkey](../appendices/shortcuts.md) (`20` by default). Values lower than
    100 are treated as a percentage decrease.


##### cycleup

:   Number of cycles to add with the `Inc Cycles`
    [hotkey](../appendices/shortcuts.md) (`10` by default). Values lower than
    100 are treated as a percentage increase.


##### cpu_idle

:   Reduce host CPU usage when the DOS shell or an application is idle. With
    [`cpu_cycles`](#cpu_cycles) set to `max`, this can reduce host CPU core
    usage from ~100% to around 25% when the shell is idle.

    This also makes the shell more multitasking-friendly, which benefits
    [Windows 3.1](../using-dosbox-staging/windows-31.md) DOS prompts.

    The feature works by emulating the HLT CPU instruction, so it might
    interfere with third-party DOS power management tools such as DOSidle and
    FDAPM --- disable it if you use those.

    Possible values: `on` *default*{ .default }, `off`
