# Composite video

## Overview

Before dedicated RGB monitors became common in the late 1980s, many home PC
users connected their computers to TVs or composite video monitors. The
NTSC composite signal creates what are known as "artifact colours" —
interference patterns that produce up to 16 colours from CGA's usual
4-colour palette. It sounds like a bug, but game developers deliberately
exploited it.

Games like *King's Quest*, *Space Quest*, and many other Sierra AGI titles
were specifically designed to look their best on composite monitors. Viewed
on an RGB monitor, these games appear as garish 4-colour affairs; on
composite, they transform into something surprisingly colourful and
detailed.

Early and late revision CGA cards produce slightly different artifact
patterns. The `era` setting lets you choose which to emulate — this matters
because some games were designed for one revision or the other.

DOSBox Staging automatically enables composite emulation when a game
switches to the 640×200 composite mode. For the more common 320×200 modes,
you'll need to set `composite = on` manually.


## Configuration settings

You can set the CGA composite video parameters in the `[composite]`
configuration section. CGA composite monitor emulation is only available for
`cga`, `pcjr`, and `tandy` [machine](../system/general.md#machine) types.


##### composite

:   Enable CGA composite monitor emulation. This allows the emulation of
    NTSC artifact colours from the raw CGA RGBI image data, just like on a
    real NTSC CGA composite monitor.

    Possible values:

    - `auto` *default*{ .default } -- Automatically enable composite
      emulation for the 640x400 composite mode if the game uses it. You need
      to enable composite mode manually for the 320x200 mode.
    - `on` -- Enable composite emulation in all video modes.
    - `off` -- Disable composite emulation.

    !!! note

        Fine-tune the composite emulation settings (e.g.,
        [hue](#hue)) using the composite hotkeys, then
        copy the new settings from the logs into your config.


##### convergence

:   Set the sharpness of the CGA composite image (`0` by default). Valid
    range is -50 to 50.


##### era

:   Era of CGA composite monitor to emulate.

    Possible values:

    <div class="compact" markdown>

    - `auto` *default*{ .default } -- PCjr uses `new`, and CGA/Tandy use
      `old`.
    - `old` -- Emulate an early NTSC IBM CGA composite monitor model.
    - `new` -- Emulate a late NTSC IBM CGA composite monitor model.

    </div>


##### hue

:   Set the hue of the CGA composite colours (`0` by default). Valid range
    is -360 to 360. For example, adjust until the sky appears blue and the
    grass green in the game. This emulates the tint knob of CGA composite
    monitors which often had to be adjusted for each game.
