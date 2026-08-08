# Machine types

Most `machine` values just pick a graphics adapter (see [Graphics
adapters](adapters.md)) with no other side effects. **PCjr** and **Tandy**
are the exception: they're emulations of specific, complete home computers
that happen to include their own graphics *and* sound hardware *and* their
own memory layout. This chapter covers the parts of PCjr/Tandy emulation
that aren't just "which video modes are available" — that half of the
story is in [Graphics adapters](adapters.md#ibm-pcjr) and
[Composite video](composite-video.md).

<!-- TODO: this file currently only has the leftover PCjr memory-layout
     setting. It needs real content:
     - Why PCjr/Tandy are "full machines" rather than adapter choices
       (bundled sound chip, non-standard memory map, etc.)
     - A short comparison of PCjr vs Tandy differences
     - Cross-links to adapters.md (graphics side) and the Tandy sound
       device docs (sound side)
     - Whether cputype/cpu defaults differ meaningfully for these machines
       (they generally don't, but worth confirming and saying so explicitly) -->

## PCjr memory

The [`pcjr_memory_config`](#pcjr_memory_config) setting controls memory layout
on the emulated PCjr. The default `expanded` provides 640 KB and is compatible
with most games. A few very old PCjr titles ([Jumpman](https://www.mobygames.com/game/80/jumpman/), [Troll](https://www.mobygames.com/game/14214/troll/)) require the
`standard` 128 KB layout.

## Configuration settings

You can set PCjr-specific parameters in the `[dos]` configuration section.

##### pcjr_memory_config

:   Set PCjr memory layout.

    Possible values:

    <div class="compact" markdown>

    - `expanded` *default*{ .default } -- 640 KB total memory with
      applications residing above 128 KB. Compatible with most games.
    - `standard` -- 128 KB total memory with applications residing below
      96 KB. Required for some older games (e.g., Jumpman, Troll).

    </div>
