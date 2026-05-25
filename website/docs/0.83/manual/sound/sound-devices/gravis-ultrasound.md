# Gravis UltraSound

The **Gravis UltraSound (or just GUS)** was released in 1992 by an unlikely
manufacturer: Canadian joystick company Advanced Gravis. Its audio
capabilities were far ahead of anything else on the consumer market ---
wavetable synthesis, stereo sound, and up to 32 channels of simultaneous
playback.

The catch? The GUS made no attempt at backwards compatibility with AdLib or
Sound Blaster cards. Programs had to be written specifically for it. Many DOS
gamers kept a [Sound Blaster](sound-blaster.md) alongside their GUS
for titles that lacked native support --- and in DOSBox Staging, you can do the
same by enabling both devices in your configuration.

Another quirk: unlike most sound cards, the GUS shipped with no built-in
instrument sounds. All voices had to be loaded from disk via "patch files" at
driver load time. Due to licensing restrictions, these patch files can't be
distributed with DOSBox Staging, so you'll need to obtain them separately.

Where the GUS truly shone was in the demoscene and tracker music community.
[Second Reality](https://www.pouet.net/prod.php?which=5) by Future Crew,
widely considered one of the greatest DOS demos ever made, was designed to
sound its best on a GUS. Games with native GUS support, like [Star Control
II](https://www.mobygames.com/game/179/star-control-ii/), also benefited
enormously from its superior audio capabilities.


## How programs use the GUS

Software used the GUS in several distinct ways. Understanding the difference
is important because each requires a different DOSBox configuration.


### Native support

These games use the card's hardware mixing directly, uploading their own
samples to the GUS's on-board RAM --- no patch files or drivers needed. This
is the GUS at its best: crystal-clear multi-channel audio mixed in hardware
with a very low noise floor. Native GUS games need only `gus = on` in the
config. Select "Gravis UltraSound" (or similar) in the game's setup utility.

``` ini
[gus]
gus = on
```

??? note "Notable native GUS games"

    <div class="compact" markdown>

    - [Al-Qadim: The Genie's Curse (1994)](https://www.mobygames.com/game/6731/al-qadim-the-genies-curse/)
    - [Command & Conquer (1995)](https://www.mobygames.com/game/564/command-conquer/)
    - [Discworld (1995)](https://www.mobygames.com/game/1201/discworld/)
    - [Epic Pinball (1993)](https://www.mobygames.com/game/263/epic-pinball/)
    - [Jazz Jackrabbit (1994)](https://www.mobygames.com/game/1487/jazz-jackrabbit/)
    - [One Must Fall: 2097 (1994)](https://www.mobygames.com/game/2862/one-must-fall-2097/)
    - [Simon the Sorcerer (1993)](https://www.mobygames.com/game/749/simon-the-sorcerer/)
    - [Star Control II (1992)](https://www.mobygames.com/game/179/star-control-ii/)
    - [Tyrian (1995)](https://www.mobygames.com/game/2064/tyrian/)
    - [Zone 66 (1993)](https://www.mobygames.com/game/1638/zone-66/)

    </div>

All demoscene productions with GUS support also fall into this category ---
they upload their own instrument samples and use the hardware mixer directly.


### GUS MIDI with patch files

These games have a built-in GUS driver that sends MIDI commands and loads
instrument samples (patch files) from the [`ultradir`](#ultradir) directory to
use as wavetable instruments. The GUS acts as a MIDI synthesiser --- the result
can sound very good, somewhere between FM synthesis and true General MIDI. The
patch files must be placed in a `MIDI` subdirectory under
[`ultradir`](#ultradir) (default `C:\ULTRASND`).

``` ini
[gus]
gus = on
ultradir = C:\ULTRASND
```

??? note "Notable GUS MIDI games"

    <div class="compact" markdown>

    - [Dark Sun: Shattered Lands (1993)](https://www.mobygames.com/game/1498/dark-sun-shattered-lands/)
    - [Doom (1993)](https://www.mobygames.com/game/1068/doom/)
    - [Dune II (1992)](https://www.mobygames.com/game/321/dune-ii-the-building-of-a-dynasty/)
    - [Gabriel Knight: Sins of the Fathers (1993)](https://www.mobygames.com/game/1079/gabriel-knight-sins-of-the-fathers/)
    - [Heretic (1994)](https://www.mobygames.com/game/1594/heretic/)
    - [Hexen (1995)](https://www.mobygames.com/game/866/hexen-beyond-heretic/)
    - [Lands of Lore: The Throne of Chaos (1993)](https://www.mobygames.com/game/501/lands-of-lore-the-throne-of-chaos/)
    - [Rise of the Triad (1995)](https://www.mobygames.com/game/768/rise-of-the-triad-dark-war/)
    - [Warcraft: Orcs & Humans (1994)](https://www.mobygames.com/game/1591/warcraft-orcs-humans/)
    - [Warcraft II: Tides of Darkness (1995)](https://www.mobygames.com/game/481/warcraft-ii-tides-of-darkness/)

    </div>


### UltraMID

These games have no GUS-specific code at all --- they send standard MIDI
commands as if talking to a [Roland MT-32](roland-mt-32.md) or [General
MIDI](../midi.md#the-general-midi-standard) device. The `ULTRAMID.EXE` TSR
provided by Gravis intercepts these MIDI commands and translates them into GUS
hardware calls, loading instrument samples from the patch files on disk. Load
`ULTRAMID.EXE` before starting the game:

``` ini
[gus]
gus = on
ultradir = C:\ULTRASND

[autoexec]
C:\ULTRASND\ULTRAMID.EXE
```

??? note "Notable UltraMID games"

    <div class="compact" markdown>

    - [Betrayal at Krondor (1993)](https://www.mobygames.com/game/236/betrayal-at-krondor/)
    - [Eye of the Beholder (1991)](https://www.mobygames.com/game/1441/eye-of-the-beholder/)
    - [Hocus Pocus (1994)](https://www.mobygames.com/game/1237/hocus-pocus/)
    - [Indiana Jones and the Fate of Atlantis (1992)](https://www.mobygames.com/game/1358/indiana-jones-and-the-fate-of-atlantis/)
    - [Master of Orion (1993)](https://www.mobygames.com/game/466/master-of-orion/)
    - [Monkey Island 2: LeChuck's Revenge (1991)](https://www.mobygames.com/game/616/monkey-island-2-lechucks-revenge/)
    - [Sam & Max Hit the Road (1993)](https://www.mobygames.com/game/373/sam-max-hit-the-road/)
    - [The 7th Guest (1993)](https://www.mobygames.com/game/561/the-7th-guest/)
    - [Ultima Underworld: The Stygian Abyss (1992)](https://www.mobygames.com/game/690/ultima-underworld-the-stygian-abyss/)
    - [X-COM: UFO Defense (1994)](https://www.mobygames.com/game/1358/x-com-ufo-defense/)

    </div>

!!! tip

    If a game's setup utility offers both "Gravis UltraSound" and
    "General MIDI" (or similar), the GUS option usually sounds better ---
    it means the game has a built-in GUS driver. Fall back to the General
    MIDI option with UltraMID if the GUS option doesn't work.

For a comprehensive list of games with GUS support, consult the official
**GLIST.TXT** compatibility list maintained by Gravis:

<div class="compact" markdown>

- [gravisultrasound.com](http://www.gravisultrasound.com/files/documentation/GLIST.TXT)
- [retronn.de mirror](https://retronn.de/ftp/driver/Gravis/UltraSound/GAMES/GLIST.TXT)
- [dk.toastednet.org mirror](http://dk.toastednet.org/GUS/TEXT/GLIST.TXT)

</div>


## Setting up the GUS environment

For games that need the patch files ([GUS MIDI](#gus-midi-with-patch-files)
and [UltraMID](#ultramid) titles), you'll need to set up the GUS directory
structure. The expected layout is:

```
C:\ULTRASND\
├── MIDI\
│   ├── ACBASS.PAT
│   ├── ACPIANO.PAT
│   ├── ... (instrument patch files)
│   └── DEFAULT.CFG
└── ULTRAMID.EXE
```

The patch files cannot be distributed with DOSBox Staging due to licensing
restrictions. Search online for "GUS patch files" or "GUS MIDI patches" to
find them. Several freely available patch sets exist.

Once the files are in place, ensure `ultradir` points to the correct location
in your DOSBox config:

``` ini
[gus]
gus = on
ultradir = C:\ULTRASND
```

!!! note

    [Native GUS](#native-gus) games and all demos do *not* need the patch
    files --- they upload their own instrument samples. You only need the
    `ULTRASND` directory setup for [GUS MIDI](#gus-midi-with-patch-files)
    and [UltraMID](#ultramid) games.


## Hardware configuration

The [`gusbase`](#gusbase), [`gusirq`](#gusirq), and [`gusdma`](#gusdma)
settings configure the I/O base address, interrupt, and DMA channel of the
emulated GUS. The defaults (base address 240, IRQ 5, DMA 3) are chosen to
avoid conflicts with the Sound Blaster card, allowing both to coexist. Some
games and demos expect the GUS factory defaults of base address 220, IRQ 11,
DMA 1. Note that certain versions of the DOS/4GW extender cannot handle IRQs
above 7, so IRQ 11 may cause problems with those titles.


## GUS-only configuration

If you want to emulate a system with *only* a GUS and no Sound Blaster (as
some purists had in the 1990s), disable the other sound devices:

``` ini
[sblaster]
sbtype = none
oplmode = none

[gus]
gus = on
```

This can be useful for games where the GUS and Sound Blaster interfere with
each other, or when you want to ensure a game uses its GUS code path
exclusively.


## Mixer channel

The Gravis UltraSound outputs to the **GUS** [mixer channel](../mixer.md#list-of-mixer-channels).


## Configuration settings

Gravis UltraSound settings are to be configured in the `[gus]` section.

!!! note

    The default settings of base address 240, IRQ 5, and DMA 3 have been
    chosen so the GUS can coexist with a Sound Blaster card. This works fine
    for the majority of programs, but some games and demos expect the GUS
    factory defaults of base address 220, IRQ 11, and DMA 1. The default
    IRQ 11 is also problematic with specific versions of the DOS4GW extender
    that cannot handle IRQs above 7.


##### gus

:   Enable Gravis UltraSound emulation. Many games and all demos upload their
    own sounds, but some rely on the instrument patch files included with the
    GUS for MIDI playback (see [`ultradir`](#ultradir) for details). Some games
    also require `ULTRAMID.EXE` to be loaded prior to starting the game.

    Possible values: `on`, `off` *default*{ .default }


##### gusbase

:   The I/O base address of the Gravis UltraSound.

    Possible values: `210`, `220`, `230`, `240` *default*{ .default }, `250`,
    `260`.


##### gusirq

:   The IRQ number of the Gravis UltraSound.

    Possible values: `2`, `3`, `5` *default*{ .default }, `7`, `11`, `12`,
    `15`.


##### gusdma

:   The DMA channel of the Gravis UltraSound.

    Possible values: `1`, `3` *default*{ .default }, `5`, `6`, `7`.


##### ultradir

:   Path to the UltraSound directory (`C:\ULTRASND` by default). This should
    have a `MIDI` subdirectory containing the patches (instrument files)
    required by some games for MIDI music playback. Not all games need these
    patches; many GUS-native games and all demos upload their own custom sounds
    instead.


##### gus_filter

:   Filter for the Gravis UltraSound audio output.

    Possible values:

    - `on` *default*{ .default } -- Filter the output. This applies a 1st order low-pass filter at 8 kHz (`lpf 1 8000`).
    - `off` -- Don't filter the output.
    - `<custom>` -- Custom filter definition; see
      [Custom filter settings](../output-filters.md#custom-filter-settings)
      for details.


