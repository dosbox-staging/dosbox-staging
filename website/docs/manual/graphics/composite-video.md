# Composite video

Before dedicated RGB monitors became common in the late 1980s, many home PC
users connected their computers to TVs or composite video monitors. The
NTSC composite signal creates what are known as "artifact colours", which are
interference patterns that produce up to 16 colours from CGA's usual
4-colour palette. It sounds like a bug, but game developers deliberately
exploited it.

Games like [King's Quest](https://www.mobygames.com/game/122/kings-quest/),
[Space
Quest](https://www.mobygames.com/game/114/space-quest-chapter-i-the-sarien-encounter/),
and many other Sierra AGI titles were specifically designed to look their best
on composite monitors. Viewed on an RGB monitor, these games appear as garish
4-colour affairs; on composite, they transform into something surprisingly
colourful and detailed.

Early and late revision CGA cards produce slightly different artifact
patterns. The `era` setting lets you choose which to emulate --- this matters
because some games were designed for one revision or the other.

DOSBox Staging automatically enables composite emulation when a game
switches to the 640×200 composite mode. For the more common 320×200 modes,
you'll need to set `composite = on` manually.

Composite emulation is available for the `cga`, `pcjr`, and `tandy`
[machine types](adapters.md#cga-and-its-descendants).


## Notable composite games

The following games were designed for or look significantly better on a
composite monitor. For these titles, set `composite = on` and adjust the
[hue](#hue) until colours look natural (blue sky, green grass).

??? note "Sierra AGI adventures"

    These are the quintessential composite games --- the artists designed
    all graphics specifically for NTSC artifact colours.

    - [King's Quest (1984)](https://www.mobygames.com/game/122/kings-quest/)
    - [King's Quest II (1985)](https://www.mobygames.com/game/123/kings-quest-ii-romancing-the-throne/)
    - [The Black Cauldron (1986)](https://www.mobygames.com/game/1012/the-black-cauldron/)
    - [Space Quest (1986)](https://www.mobygames.com/game/114/space-quest-chapter-i-the-sarien-encounter/)
    - [Police Quest (1987)](https://www.mobygames.com/game/113/police-quest-in-pursuit-of-the-death-angel/)
    - [Leisure Suit Larry (1987)](https://www.mobygames.com/game/379/leisure-suit-larry-in-the-land-of-the-lounge-lizards/)
    - [Gold Rush! (1988)](https://www.mobygames.com/game/221/gold-rush/)
    - [Mixed-Up Mother Goose (1987)](https://www.mobygames.com/game/1614/mixed-up-mother-goose/)
    - [Donald Duck's Playground (1986)](https://www.mobygames.com/game/5154/donald-ducks-playground/)

??? note "Other games that benefit from composite mode"

    - [California Games (1987)](https://www.mobygames.com/game/86/california-games/)
    - [Pitstop II (1984)](https://www.mobygames.com/game/9157/pitstop-ii/)
    - [Alley Cat (1984)](https://www.mobygames.com/game/190/alley-cat/)
    - [Rick Dangerous (1989)](https://www.mobygames.com/game/2423/rick-dangerous/)
    - [Thexder (1987)](https://www.mobygames.com/game/1247/thexder/)
    - [Silpheed (1988)](https://www.mobygames.com/game/1252/silpheed/)
    - [Windwalker (1989)](https://www.mobygames.com/game/4165/windwalker/)
    - [Zak McKracken and the Alien Mindbenders (1988)](https://www.mobygames.com/game/634/zak-mckracken-and-the-alien-mindbenders/) (CGA version)
    - [Maniac Mansion (1987)](https://www.mobygames.com/game/714/maniac-mansion/) (CGA version)


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
