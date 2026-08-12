# Machine types

The [`machine`](../graphics/adapters.md#machine) setting does one of two
things, depending on the value. Most values simply select a graphics adapter —
see [Graphics adapters](../graphics/adapters.md) for the full writeup of each
one. The `machine` values map to graphics adapters as follows:

<div class="compact" markdown>

- `hercules` — [Hercules Graphics Card](../graphics/adapters.md#hercules-graphics-card)
- `cga_mono` — [Monochrome CGA](../graphics/adapters.md#monochrome-cga)
- `cga` — [CGA](../graphics/adapters.md#cga)
- `ega` — [EGA](../graphics/adapters.md#ega)
- `svga_paradise` — [Paradise PVGA1A](../graphics/adapters.md#paradise-pvga1a)
- `svga_et3000` — [Tseng Labs ET3000](../graphics/adapters.md#tseng-labs-et3000)
- `svga_et4000` — [Tseng Labs ET4000](../graphics/adapters.md#tseng-labs-et4000)
- `svga_s3` *default*{ .default } — [S3 Trio64](../graphics/adapters.md#s3-trio64), VESA VBE 2.0
- `vesa_oldvbe` — [S3 Trio64](../graphics/adapters.md#s3-trio64), limited to VESA VBE 1.2
- `vesa_nolfb` — [S3 Trio64](../graphics/adapters.md#s3-trio64), VESA VBE 2.0 without the linear framebuffer

</compact>

[IBM PCjr](../graphics/adapters.md#ibm-pcjr) and
[Tandy 1000](../graphics/adapters.md#tandy-1000) are the exception. Setting
`machine = pcjr` or `machine = tandy` doesn't just change the graphics mode —
it emulates a whole home computer, with its own sound chip (see
[Tandy sound](../sound/sound-devices/tandy.md)),
[composite video](../graphics/composite-video.md) output, and memory layout.
This chapter covers the pieces of that emulation that aren't about video
modes, since that half of the story already lives in [Graphics
adapters](../graphics/adapters.md#ibm-pcjr).


## PCjr memory layout

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
