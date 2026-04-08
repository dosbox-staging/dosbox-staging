# CPU


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

    | CPU              | Cycles |
    | ---------------- | ------ |
    | 8088 (4.77 MHz)  | 300    |
    | 286-8            | 700    |
    | 286-12           | 1500   |
    | 386SX-20         | 3000   |
    | 386DX-33         | 6000   |
    | 386DX-40         | 8000   |
    | 486DX-33         | 12000  |
    | 486DX/2-66       | 25000  |
    | Pentium 90       | 50000  |
    | Pentium MMX-166  | 100000 |
    | Pentium II 300   | 200000 |

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

:   Number of cycles to subtract with the `Dec Cycles` hotkey (`20` by
    default). Values lower than 100 are treated as a percentage decrease.


##### cycleup

:   Number of cycles to add with the `Inc Cycles` hotkey (`10` by default).
    Values lower than 100 are treated as a percentage increase.
