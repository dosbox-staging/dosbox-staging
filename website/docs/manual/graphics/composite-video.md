# Composite video

Before dedicated RGB monitors became common in the late 1980s, many home PC
users connected their computers to TVs or composite video monitors. The NTSC
composite signal introduces interference patterns that produce unexpected
colours --- a phenomenon known as _artifact colours_ --- expanding CGA's usual
4-colour palette to as many as 16. It sounds like a bug, but developers
deliberately exploited it.

CGA's two graphics modes --- **320&times;200 medium resolution** and
**640&times;200 high resolution** --- both produce artifact colours on a
composite display, though the higher-resolution mode yields a broader and more
distinct palette, and was the more popular choice for games built around
composite output. Counterintuitively, 640×200 is a monochrome mode --- it
outputs only black and white pixels --- but it's precisely that high-frequency
alternating pattern that the composite signal smears into colour.

Composite emulation is available for the `cga`, `pcjr`, and `tandy`
[machine types](adapters.md#cga-and-its-descendants).

## Composite support in games

Sierra On-Line was particularly enthusiastic adopters: games like [King's
Quest](https://www.mobygames.com/game/122/kings-quest/) and [Space
Quest](https://www.mobygames.com/game/114/space-quest-chapter-i-the-sarien-encounter/)
would prompt you at startup to choose between RGB and composite modes, with
the composite graphics being the more colourful and detailed of the two. On
RGB without that mode selected, they appear as garish 4-colour affairs.
Earlier titles like [Microsoft
Decathlon](https://www.mobygames.com/game/1652/olympic-decathlon/) and [Ultima
II : Revenge of the Enchantress](https://www.mobygames.com/game/880/ultima-ii-the-revenge-of-the-enchantress/)
used composite colour from the outset, without bothering to offer an RGB
fallback at all.

DOSBox Staging automatically enables composite emulation when a game switches
to the 640&times;200 composite mode. For the more common 320&times;200 modes,
you'll need to enable the [composite](#composite) setting manually.

??? note "Notable games with composite video support"

    See the linked Google Spreadsheet [in this blog
    post](https://nerdlypleasures.blogspot.com/2023/03/list-of-ibm-composite-artifact-color.html)
    for a complete list.

    <div class="compact" markdown>

    - [Ancient Art of War, The (1984)](https://www.mobygames.com/game/23/the-ancient-art-of-war/)
    - [Bard's Tale II: The Destiny Knight, The (1986)](https://www.mobygames.com/game/1273/the-bards-tale-ii-the-destiny-knight/)
    - [Below the Root (1984)](https://www.mobygames.com/game/602/below-the-root/)
    - [Black Cauldron, The (1986)](https://www.mobygames.com/game/1012/the-black-cauldron/)
    - [Bruce Lee (1985)](https://www.mobygames.com/game/191/bruce-lee/)
    - [California Games (1987)](https://www.mobygames.com/game/86/california-games/)
    - [Demon's Forge (1987)](https://www.mobygames.com/game/1795/the-demons-forge/)
    - [Donald Duck's Playground (1986)](https://www.mobygames.com/game/5154/donald-ducks-playground/)
    - [Dragon Wars (1990)](https://www.mobygames.com/game/2026/dragon-wars/)
    - [Gold Rush! (1988)](https://www.mobygames.com/game/221/gold-rush/)
    - [Jumpman (1984)](https://www.mobygames.com/game/80/jumpman/)
    - [King's Quest (1984)](https://www.mobygames.com/game/122/kings-quest/)
    - [King's Quest II (1985)](https://www.mobygames.com/game/123/kings-quest-ii-romancing-the-throne/)
    - [Leisure Suit Larry (1987)](https://www.mobygames.com/game/379/leisure-suit-larry-in-the-land-of-the-lounge-lizards/)
    - [Maniac Mansion (1987)](https://www.mobygames.com/game/714/maniac-mansion/) (CGA version)
    - [Microsoft Decathlon (1982)](https://www.mobygames.com/game/1652/olympic-decathlon/)
    - [Mixed-Up Mother Goose (1987)](https://www.mobygames.com/game/1614/mixed-up-mother-goose/)
    - [Pitstop II (1984)](https://www.mobygames.com/game/9157/pitstop-ii/)
    - [Seven Cities of Gold, The (1985)](https://www.mobygames.com/game/3451/the-seven-cities-of-gold/)
    - [Space Quest (1986)](https://www.mobygames.com/game/114/space-quest-chapter-i-the-sarien-encounter/)
    - [Transylvania (1986)](https://www.mobygames.com/game/133318/transylvania/)
    - [Ultima II : Revenge of the Enchantress (1983)](https://www.mobygames.com/game/880/ultima-ii-the-revenge-of-the-enchantress/)
    - [Ultima III: Exodus (1985)](https://www.mobygames.com/game/878/exodus-ultima-iii/)
    - [Where in the U.S.A. is Carmen Sandiego? (1986)](https://www.mobygames.com/game/315/where-in-the-usa-is-carmen-sandiego/)
    - [Zak McKracken and the Alien Mindbenders (1988)](https://www.mobygames.com/game/634/zak-mckracken-and-the-alien-mindbenders/) (CGA version)

    </div>


## Tuning the composite colours

Artifact colours are sensitive to the phase of the composite signal, which
means the hue can drift between different cards and setups. Real NTSC monitors
had a physical tint dial for exactly this reason, and IBM even included a
"COLOR ADJUST" trimpot on the PC motherboard to compensate. The [hue](#hue) setting
lets you replicate that adjustment --- if colours look off, this is the first
thing to tweak. Adjust the hue setting for each game until the colours look
as expected (e.g., blue skies, green grass).

Early and late CGA card revisions produce slightly different artifact
patterns, and some games were designed with one revision in mind. The
[era](#era) setting lets you choose which to emulate.


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


##### convergence

:   Set the sharpness of the CGA composite image (`0` by default). Valid
    range is -50 to 50.


