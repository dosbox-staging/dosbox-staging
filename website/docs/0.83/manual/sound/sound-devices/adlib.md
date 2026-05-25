# AdLib (OPL)

This page covers the **AdLib Music Synthesizer Card**, the **AdLib Gold
1000**, and the OPL3-compatible **ESS Enhanced FM (ESFM)** synthesiser --- all
FM synthesisers built around Yamaha's OPL chip family or compatible designs.
Because the OPL chip also appeared on every DOS-era [Sound
Blaster](sound-blaster.md), their settings live the shared `[sblaster]`
configuration section alongside the Sound Blaster's own. DOSBox Staging uses
the cycle-accurate Nuked OPL3 emulator for effectively bit-identical OPL
output. 

DOS games typically split their audio into two streams handled by different
hardware: [synthesised music](../overview.md#synthesised-sound) (notes played
by a chip like OPL) and [digital sound](../overview.md#digital-sound)
(pre-recorded samples used for speech and sound effects). Most DOS games split
audio across two devices: an AdLib-family card for music (covered here) and a
[Sound Blaster](sound-blaster.md) for speech and sound effects.


## AdLib Music Synthesizer Card

The **AdLib Music Synthesizer Card** (typically simply referred to as
**AdLib**) was released by Ad Lib in 1987. It was the first popular sound card
for IBM PC compatibles, capable of producing multichannel music and sound
effects via its Yamaha OPL2 synthesizer chip.

Being such a huge step up from the PC speaker, the AdLib took the market by
storm. As it quickly became a de facto standard, many competing sound card
manufacturers started including full AdLib compatibility on their cards. The
result of this is that virtually all DOS games from 1990 onwards support the
AdLib, at least as a fallback option.

Digital audio is not supported on the card, but some games were able to
approximate it with clever programming tricks (e.g., [F-15 Strike Eagle
II](https://www.mobygames.com/game/1737/f-15-strike-eagle-ii/) by MicroProse).

Use the following settings to emulate the original standalone AdLib Music
Synthesizer Card:

``` ini
[sblaster]
sbtype = none
oplmode = opl2
```

Only very old 80s games will need a standalone AdLib card. More recent games
generally need a [Sound Blaster]() which comes with a built-in AdLib (OPL)
synthesizer. See [Emulated Sound Blaster models](sound-blaster.md#emulated-sound-blaster-models) for details.

!!! note

    When [`oplmode`](#oplmode) is set to `opl2`, [CMS emulation](cms.md) is
    always enabled as well. This is legacy DOSBox behaviour, and will be
    addressed in a future version.


## AdLib Gold 1000

The **AdLib Gold 1000** was released in 1992. It has a Yamaha OPL3 chip and a
stereo audio processor on-board, and it also supports 12-bit digital audio. An
optional surround module was available to add further spatial effects to the
OPL output (e.g., chorus and reverb).

DOSBox Staging fully emulates the card, including the surround module, except
for its digital audio capabilities.

Very few games have direct support for the AdLib Gold 1000, and no known game
makes use of its digital audio features. The single game in existence that
really takes advantage of the AdLib Gold and its surround module is the
adventure game [Dune](https://www.mobygames.com/game/380/dune/) from 1992.

Use the following settings to enable AdLib Gold 1000 emulation. Typically, you
would use the card together with a Sound Blaster for digital audio. The setup
utility of [Dune](https://en.wikipedia.org/wiki/Dune_(video_game)) should
auto-detect AdLib Gold and the surround module correctly with these settings:

``` ini
[sblaster]
sbtype = sb16
oplmode = opl3gold
```


## ESS Enhanced FM Audio (ESFM)

**ESFM** is the OPL3-compatible FM synthesiser found on later ESS AudioDrive
cards. In "legacy mode", ESFM is fully compatible with the Yamaha OPL3 and
yields almost identical output on most materials. What sets it apart is its
"native mode", which offers advanced synthesis features surpassing the OPL3's
capabilities --- it bridges the gap between synthetic-sounding OPL music and
sample-based MIDI music.

Since ESFM was released in 1995, just a few years before the end of the DOS
era, only a handful of games support native mode, butn in the few that do, the
results often sound spectacular.

- To run ESFM in **legacy mode**, use `oplmode = esfm` with any
  [Sound Blaster](sound-blaster.md) model and configure the game for Sound
  Blaster and AdLib/OPL as usual. There's usually not much to be gained from
  this, the output will generally sound identical to OPL2.

- To use **native mode**, set [`sbtype = ess`](sound-blaster.md#sbtype) and
  configure the **ESS Technology ES1688, ES1788, ES1888 Enhanced FM Audio**
  MIDI music driver in the game's setup utility (most games that support ESFM
  natively use the Miles Sound System). For the digital audio driver, select
  the Sound Blaster Pro option (ESS AudioDrive cards are Sound Blaster Pro
  compatible).

``` ini
[sblaster]
sbtype = ess
```

??? note "Games with ESFM support"

    <div class="compact" markdown>

    - [Advanced Civilization (1995)](https://www.mobygames.com/game/5297/advanced-civilization/)
    - [Callahan's Crosstime Saloon (1997)](https://www.mobygames.com/game/2150/callahans-crosstime-saloon/)
    - [Heaven's Dawn (1995)](https://www.mobygames.com/game/45120/heavens-dawn/)
    - [Heroes of Might and Magic II (1996)](https://www.mobygames.com/game/1513/heroes-of-might-and-magic-ii-the-succession-wars/)
    - [Magic Carpet 2 (1995)](https://www.mobygames.com/game/790/magic-carpet-2-the-netherworlds/)
    - [Shannara (1995)](https://www.mobygames.com/game/3208/shannara/)
    - [The 11th Hour (1995)](https://www.mobygames.com/game/567/the-11th-hour/)
    - [The Gene Machine (1996)](https://www.mobygames.com/game/1121/the-gene-machine/)
    - [The Settlers II (1996)](https://www.mobygames.com/game/598/the-settlers-ii-veni-vidi-vici/)
    - [Theme Hospital (1997)](https://www.mobygames.com/game/674/theme-hospital/)
    - [WarCraft II (1995)](https://www.mobygames.com/game/1339/warcraft-ii-tides-of-darkness/)
    - [Z (1996)](https://www.mobygames.com/game/346/z/)

    </div>


!!! tip

    You can also try to "retrofit" the `ESFM.MID` driver from Miles Sound
    System games that have it into earlier ones that don't. For example,
    [Discworld](https://www.mobygames.com/game/184/discworld/) sounds great
    with the ESFM driver from
    [Heaven's Dawn](https://www.mobygames.com/game/45120/heavens-dawn/).


## Mixer channel

All emulated AdLib (OPL) synthesizers outputs to the **OPL** [mixer
channel](../mixer.md#list-of-mixer-channels).


## Audio artifact mitigation

Real OPL hardware exhibited certain audio quirks --- hanging notes and DC bias
in particular --- that were simply part of the experience at the time. DOSBox
Staging faithfully emulates these behaviours, but also provides settings to
mitigate them when strict authenticity is less important than an improved
listening experience. These settings are best applied on a per-game basis.

Some games don't properly send note-off commands to the OPL FM synthesiser,
resulting in notes that play forever. The [`opl_fadeout`](#opl_fadeout) setting
monitors OPL port activity and fades out any remaining sound after the game
stops writing to the chip. Known affected games include [The Bard's Tale](https://www.mobygames.com/game/819/tales-of-the-unknown-volume-i-the-bards-tale/). The
`fade` preset waits 500 ms then fades over 500 ms; custom timing can be
specified as two values in milliseconds.

A small number of games play PCM (sampled) audio through the OPL synthesiser
via clever programming tricks rather than using it for FM synthesis. This can
cause clicking and popping due to DC bias in the output --- the
[`opl_remove_dc_bias`](#opl_remove_dc_bias) setting removes this bias. Only
enable it for known affected games: [Golden
Eagle](https://www.mobygames.com/game/44953/golden-eagle/), [Wizardry
6](https://www.mobygames.com/game/709/wizardry-bane-of-the-cosmic-forge/), and
[Wizardry
7](https://www.mobygames.com/game/1561/wizardry-crusaders-of-the-dark-savant/).

For Sound Blaster startup-pop mitigation, see [Sound Blaster](sound-blaster.md#audio-artifact-mitigation).


## Configuration settings

The AdLib settings are configured in the `[sblaster]` section. The related
Sound Blaster digital-audio settings are covered on the [Sound
Blaster](sound-blaster.md#configuration-settings) page.

### OPL

!!! note

    Some games trigger low-level residual noise from the OPL synth in quiet
    passages. The [Denoiser](../mixer.md#denoiser) removes this without
    degrading sound quality.

##### opl_fadeout

:   Fade out hanging notes on the OPL synth.

    Possible values:

    - `off` *default*{ .default } -- Don't fade out hanging notes.
    - `fade` -- Fade out hanging notes. You should only enable this in games
      that sometimes play hanging notes that never stop (e.g., Bard's Tale).
    - `<custom>` -- A custom fade-out definition in the following format:
      `WAIT FADE`, where `WAIT` is how long after the last I/O port write
      fading begins (between 100 and 5000 milliseconds), and `FADE` is the
      fade-out period (between 10 and 3000 milliseconds). Examples:
        - `300 200` (wait 300 ms before fading out over a 200 ms period)
        - `1000 3000` (wait 1 second before fading out over 3 seconds)



##### opl_filter

:   Filter settings for the OPL output:

    - `auto` *default*{ .default } -- Use the appropriate filter determined by the [`sbtype`](sound-blaster.md#sbtype) setting.
    - `sb1`, `sb2`, `sbpro1`, `sbpro2`, `sb16` -- Use the OPL filter of this Sound Blaster model.
    - `off` -- Don't filter the OPL output.
    - `<custom>` -- Custom filter definition; see
      [Custom filter settings](../output-filters.md#custom-filter-settings)
      for details.

    These are the model specific low-pass filter settings for OPL output: 

    <div class="compact" markdown>

    | `sbtype`             | Filter type           | Cutoff frequency |
    | -------------------- | --------------------- | ---------------- |
    | `sb1`<br>`sb2`       | 1st order (6dB/oct)   | 12kHz 
    | `sbpro1`<br>`sbpro2` | 1st order (6dB/oct)   | 8kHz
    | `sb16`               | no filter             | N/A

    </div>

    <!-- TODO audio samples -->


##### opl_remove_dc_bias

:   Remove DC bias from the OPL output.

    Possible values: `on`, `off` *default*{ .default }

    Some games play digitised music and sound effects using the OPL (AdLib)
    channels by rapidly changing the volume in very crude steps, similar to
    how the Disney Sound Source and Covox LPT DAC operate. This can produce
    annoying pops. Enable this setting to eliminate them.

    Known affected games: Golden Eagle (1991), Wizardry VI: Bane of the
    Cosmic Forge (1990), Wizardry VII (1992).

    Recommended settings for **Wizardry VI** when configured for AdLib sound:

    ``` ini
    [sblaster]
    sbtype = none
    oplmode = opl2
    opl_remove_dc_bias = true
    opl_filter = lpf 2 5500

    [speaker]
    pcspeaker = off

    [autoexec]
    mixer opl 500
    ```


##### oplmode

:   OPL mode to emulate.

    DOSBox Staging uses the highly accurate NukedOPL library to achieve a
    nearly bit-perfect emulation of the Yamaha OPL chips.

    Possible values:

      - `auto` *default*{ .default } -- Use the appropriate model determined
        by [`sbtype`](sound-blaster.md#sbtype).

      - `opl2` -- Yamaha OPL2 (YM3812, mono).

      - `dualopl2` -- Dual OPL2 (two OPL2 chips in stereo configuration).

      - `opl3` -- Yamaha OPL3 (YMF262, stereo).

      - `opl3gold` -- OPL3 and the optional AdLib Gold Surround module. Use
        with [`sbtype = sb16`](sound-blaster.md#sbtype) to emulate the AdLib Gold 1000.

      - `esfm` -- ESS ESFM (enhanced Yamaha OPL3 compatible FM synth).

      - `none`/`off` -- Disable OPL emulation.

    !!! note

        - [`sbtype = none`](sound-blaster.md#sbtype) and `oplmode = opl2` emulates the original AdLib
          card.

        - Only `oplmode = esfm` is not enough to get ESS Enhanced FM music
          in games; you'll also need to set [`sbtype = ess`](sound-blaster.md#sbtype). `oplmode = esfm`
          is useful to get ESFM-flavoured OPL with original Sound Blaster
          models.
