# Tandy 3 Voice

The [Tandy 1000](../../graphics/adapters.md#tandy-1000), released in 1984, was
essentially a clone of the short-lived [IBM
PCjr](../../graphics/adapters.md#tandy-1000), and it carried over the PCjr's
enhanced sound hardware almost wholesale. Both machines used the same Texas
Instruments SN76489 programmable sound generator chip (or close variants of
it), producing three square-wave voices plus a noise channel. In practice, the
two are functionally identical from a game's perspective, which is why **Tandy
sound** and **PCjr sound** are often used interchangeably. We call it **Tandy
3 Voice** because the Tandy 1000 succeeded where the PCjr failed --- it was
the machine that actually made this sound hardware matter.

## Using Tandy sound

Life was simple in the early 80s --- most games only supported the PC speaker,
but some offered enhanced sound on the Tandy 1000. The enhanced sound hardware
was an integral, non-removable part of the computer, not available as an
add-on card for standard PCs. Most Tandy-compatible games simply check whether
they're running on a Tandy machine, and if so, make use of its integrated
sound and graphics capabilities.

To play Tandy games with Tandy sound, enable the emulation of a generic Tandy
computer:

``` ini
[dosbox]
machine = tandy
```

As explained above, you'll get Tandy 3 Voice sound when emulating an IBM PCjr,
too:

``` ini
[dosbox]
machine = pcjr
```

See [`machine`](../../system/general.md#machine) for more details on machine
types.

Later Tandy 1000 models added support for digital audio as well. Due to
resource conflicts, DOSBox cannot emulate the Tandy DAC and Sound Blaster
digital audio simultaneously. To use the Tandy DAC alongside the Tandy
synthesiser, disable Sound Blaster emulation by setting
[`sbtype`](../sound-devices/adlib-cms-sound-blaster.md#sbtype) to `none` in
the `[sblaster]` section.

``` ini
[dosbox]
machine = tandy

[sblaster]
sbtype = none
```

!!! note

    Tandy 3 Voice sound is sometimes listed as ***Tandy 1000*** or ***IBM
    PCjr*** in a game's sound setup.

    Tandy digital sound may appear as **Tandy with DAC** or **Tandy 1000
    SL/TL/HL series** or similar.


## Games with enhanced Tandy sound

Many games from the mid-1980s included enhanced audio for Tandy and PCjr
hardware. While Tandy graphics are roughly comparable to EGA (16 colours at
320&times;200), the real advantage of setting `machine = tandy` is the
**Tandy 3 Voice sound** --- three-channel synthesis plus optional DAC audio
that is a significant step up from the PC speaker, and the only sound
enhancement available for these games before the AdLib era.

??? note "Notable Tandy/PCjr games"

    Games in this list have enhanced Tandy or PCjr sound. Many also have
    enhanced 16-colour graphics when run with `machine = tandy` or
    `machine = pcjr`.

    <div class="compact" markdown>

    - 4x4 Off-Road Racing
    - 688 Attack Sub
    - Abrams Battle Tank
    - Action Fighter
    - Airborne Ranger
    - Arcticfox
    - Arkanoid
    - Battle Chess
    - BattleTech: The Crescent Hawks' Inception
    - BattleTech: The Crescent Hawks' Revenge
    - Black Cauldron, The
    - Blue Angels: Formation Flight Simulation
    - Championship Baseball
    - Curse of the Azure Bonds
    - Demon Stalkers
    - Donald Duck's Playground
    - Duck Tales: Quest for Gold
    - Falcon
    - GATO
    - Gauntlet
    - Gauntlet II
    - GFL Championship Football
    - Gold Rush!
    - Gunship
    - Hardball
    - Impossible Mission II
    - Indianapolis 500: The Simulation
    - Jet
    - Jordan vs Bird: One on One
    - King's Quest II
    - King's Quest III
    - King's Quest IV (AGI)
    - King's Quest: Quest for the Crown
    - Kings of the Beach
    - Laser Surgeon
    - Leisure Suit Larry
    - Manhunter 2
    - Manhunter: New York
    - Maniac Mansion
    - Maniac Mansion Enhanced
    - Mavis Beacon Teaches Typing!
    - Mickey's Space Adventure
    - Mixed-Up Mother Goose
    - Modem Wars
    - Ninja
    - Outrun
    - Police Quest
    - Pool of Radiance
    - Revenge of Defender
    - Rick Dangerous
    - Robot Rascals
    - Sargon V
    - Sea Hunt
    - Sentinel Worlds I
    - Shogun
    - SimCity
    - Skate or Die
    - Snow Strike
    - Soko-Ban
    - Space Quest II
    - Space Quest III
    - Space Quest: Chapter I
    - Star Rank Boxing II
    - Starflight
    - Strike Fleet
    - Stuart Smith's Adventure Construction Set
    - Test Drive
    - The Crimson Crown
    - The Three Stooges
    - Thexder
    - Transylvania
    - Ultima II
    - Vegas Jackpot
    - Wilderness: A Survival Adventure
    - Will Harvey's Zany Golf
    - Wings of Fury
    - World Tour Golf
    - Zak McKracken
    - Zak McKracken Enhanced

    </div>


## Mixer channels

The Tandy synthesiser outputs to the **TANDY** [mixer
channel](../mixer.md#list-of-mixer-channels), and the DAC to the **TANDYDAC**
channel.


## Configuration settings

Tandy 3 Voice settings are to be configured in the `[speaker]` section.


##### tandy

:   Set the Tandy/PCjr 3 Voice sound emulation.

    Possible values:

    - `auto` *default*{ .default } -- Automatically enable Tandy/PCjr sound
      for the `tandy` and `pcjr` machine types only.
    - `on` -- Enable Tandy/PCjr sound with DAC support, when possible.
      Most games also need the machine set to `tandy` or `pcjr` to work.
    - `psg` -- Only enable the card's three-voice programmable sound generator
      without DAC to avoid conflicts with other cards using DMA 1.
    - `off` -- Disable Tandy/PCjr sound.


##### tandy_fadeout

:   Fade out the Tandy synth output after the last IO port write.

    Possible values:

    - `off` *default*{ .default } -- Don't fade out; residual output will
      play forever.
    - `on` -- Wait 0.5s before fading out over a 0.5s period.
    - `<custom>` -- Custom fade-out definition; see
      [opl_fadeout](adlib-cms-sound-blaster.md#opl_fadeout) for details on
      the format.


##### tandy_filter

:   Filter for the Tandy synth output.

    Possible values:

    - `on` *default*{ .default } -- Filter the output. This applies a 3rd order high-pass filter at 120 Hz, and a 2nd order low-pass filter at 4.8 kHz (`hpf 3 120 lpf 2 4800`).
    - `off` -- Don't filter the output.
    - `<custom>` -- Custom filter definition; see
      [Custom filter settings](../output-filters.md#custom-filter-settings)
      for details.


##### tandy_dac_filter

:   Filter for the Tandy DAC output.

    Possible values:

    - `on` *default*{ .default } -- Filter the output. This applies a 3rd order high-pass filter at 120 Hz, and a 2nd order low-pass filter at 4.8 kHz (`hpf 3 120 lpf 2 4800`).
    - `off` -- Don't filter the output.
    - `<custom>` -- Custom filter definition; see
      [Custom filter settings](../output-filters.md#custom-filter-settings)
      for details.


