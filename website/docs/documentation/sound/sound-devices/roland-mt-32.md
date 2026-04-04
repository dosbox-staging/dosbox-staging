# Roland MT-32

The **Roland MT-32** multi-timbre sound module was released in 1987 by Roland
Corporation, the iconic Japanese manufacturer of electronic musical
instruments. It featured Roland's novel, patented Linear Arithmetic (LA)
Synthesis which combined sample playback with digital synthesis, capable of
producing a wide range of realistic and synthesised sounds. As the little
brother of Roland's flagship Roland D-50 synthesiser released in the same
year, it was aimed at the hobbyist musician market.

Around the same time, Sierra On-Line, the company most famous for pioneering
the graphic adventure genre, was looking for ways to push PC audio to the next
level. They took an interest in the MT-32, which lead to Sierra adding support
for the module to most of their games from 1988 onwards. Other companies soon
started following Sierra's lead, which turned the Roland MT-32 a de facto
standard for high-end audio in DOS gaming in the 1988 to 1992 period until
[General MIDI](general-midi.md) and [CD Audio](cd-da.md) took over.

As the Roland MT-32 was considerably more expensive than other options, such
as the [AdLib](adlib-cms-sound-blaster.md#adlib-music-synthesizer-card), it remained out of reach for most computer users. 


## MT-32 variants

Roland produced several revisions and related models. Understanding the
differences matters because some games sound different --- or even
malfunction --- on the wrong revision.

<div class="compact" markdown>

| Model | Year | `model` value | Key differences |
|-------|------|---------------|-----------------|
| MT-32 "old" (v1.0x) | 1987 | `mt32_old` | Original hardware; some games exploit firmware quirks |
| MT-32 "new" (v2.0x) | 1987 | `mt32_new` | Faster CPU, lower noise floor, revised instrument samples |
| MT-100 | 1988 | --- | MT-32 "new" with built-in sequencer |
| CM-32L | 1989 | `cm32l` | MT-32 "new" compatible + 33 extra sound effects |
| LAPC-I | 1989 | --- | CM-32L on an ISA card (same ROMs) |
| CM-64 | 1989 | --- | CM-32L + CM-32P (added PCM instrument bank) |

</div>


## Choosing between "old" and "new"

The MT-32 was revised within its first year. The "new" revision (ROM v2.0x)
has a faster CPU and lower background noise, but Roland also reorganised
some of the instrument samples. Games composed on the original "old" hardware
(ROM v1.0x) may sound subtly different on the "new" revision --- certain
instruments have a different timbre, and a few games exploit firmware quirks
that were fixed in the newer ROMs.

As a rule of thumb:

- **Early Sierra games** (1988--1990) --- King's Quest IV, Police Quest II,
  Space Quest III, Leisure Suit Larry 2/3 --- were composed on "old" hardware
  and generally sound more authentic on `mt32_old`.
- **Games from 1991 onwards** --- and most LucasArts titles --- work well on
  either revision.
- When in doubt, the VOGONS Wiki
  [compatibility list](https://www.vogonswiki.com/index.php/List_of_MT-32-compatible_computer_games)
  notes the recommended revision for each game.


## The CM-32L

The **Roland CM-32L** released in 1989 --- an MT-32 variant tailored for
gaming --- is generally the best overall choice. It is fully compatible with
MT-32 "new" games and adds 33 extra sound effects (gunshots, explosions, dog
barks, footsteps, etc.) that a handful of games use for enhanced audio. A few
titles only sound complete on a CM-32L (e.g., [Curse of
Enchantia](https://www.mobygames.com/game/740/curse-of-enchantia/) and [Fire &
Ice](https://www.mobygames.com/game/7386/fire-ice/)). LucasArts was one of the
main game studios that typically took advantage of the extra sound effects of
the module.

DOSBox Staging's `model = auto` setting prefers the CM-32L when its ROMs are
available, falling back to the best available MT-32 model otherwise. For
early Sierra titles that sound best on "old" hardware, override with
`model = mt32_old` in a per-game config.


## MIDI before General MIDI


!!! important

    Although the Roland MT-32 comes with a built-in library of 128 synth and
    30 rhythm patches, it is a fully programmable synthesiser. Most games with
    MT-32 support take advantage of the module's progammability and upload
    their own unique custom sounds at startup.

    Most other sound modules that have so-called "MT-32 compatibility" modes
    can only emulate the built-in sounds; custom patches sound completely
    wrong on them. Even Roland's own later sound modules suffer from the same
    problem. In short, most music composed for the Roland MT-32 only sounds
    correct when played back on a Roland MT-32, and nothing else.



!!! note

    Roland MT-32 support might be referred to as **Roland MT-100**, **LAPC-I**
    (sometimes mispelled as **LAPC-1**), **CM-32L**, or **CM-64** in the
    game's setup program. All these refer to the same thing; you only need to
    research which ROM version is recommended for the game and set it
    accordingly.

    Some games offer a **Roland MT-32 with Sound Blaster** option which should
    be generally preferred to the plain Roland MT-32 option as it might enable 
    additional digital music to be played on the Sound Blaster.


??? note "Notable games with MT-32 support"

    <div class="compact" markdown>

    - [Indiana Jones and the Last Crusade (1989)](https://www.mobygames.com/game/197/indiana-jones-and-the-last-crusade-the-graphic-adventure/)
    - [King's Quest IV (1988)](https://www.mobygames.com/game/133/kings-quest-iv-the-perils-of-rosella/)
    - [King's Quest V (1990)](https://www.mobygames.com/game/134/kings-quest-v-absence-makes-the-heart-go-yonder/)
    - [King's Quest VI (1992)](https://www.mobygames.com/game/135/kings-quest-vi-heir-today-gone-tomorrow/)
    - [Leisure Suit Larry 3 (1989)](https://www.mobygames.com/game/146/leisure-suit-larry-iii-passionate-patti-in-pursuit-of-the-pulsating-pectorals/)
    - [Loom (1990)](https://www.mobygames.com/game/176/loom/)
    - [Monkey Island (1990)](https://www.mobygames.com/game/616/the-secret-of-monkey-island/)
    - [Monkey Island 2 (1991)](https://www.mobygames.com/game/289/monkey-island-2-lechucks-revenge/)
    - [Police Quest II (1988)](https://www.mobygames.com/game/142/police-quest-2-the-vengeance/)
    - [Quest for Glory I (1989)](https://www.mobygames.com/game/148/quest-for-glory-i-so-you-want-to-be-a-hero/)
    - [Space Quest III (1989)](https://www.mobygames.com/game/139/space-quest-iii-the-pirates-of-pestulon/)
    - [Space Quest IV (1991)](https://www.mobygames.com/game/140/space-quest-iv-roger-wilco-and-the-time-rippers/)
    - [Ultima VI (1990)](https://www.mobygames.com/game/372/ultima-vi-the-false-prophet/)
    - [Wing Commander (1990)](https://www.mobygames.com/game/1509/wing-commander/)
    - [Wing Commander II (1991)](https://www.mobygames.com/game/377/wing-commander-ii-vengeance-of-the-kilrathi/)

    </div>

Not all games that list "Roland MT-32" in their setup utility actually
support it properly --- and many games with excellent MT-32 music don't
advertise it prominently. The most reliable way to check is the community-maintained
[List of MT-32-compatible computer games](https://www.vogonswiki.com/index.php/List_of_MT-32-compatible_computer_games)
on the VOGONS Wiki, which also notes which MT-32 model version works best for
each title.

For a step-by-step walkthrough of setting up the MT-32 with a specific game,
see the [Beneath a Steel Sky](../../../getting-started/beneath-a-steel-sky.md#setting-up-roland-mt-32-sound)
chapter of the getting started guide.

DOSBox Staging emulates both the MT-32 sound module and the [MPU-401 MIDI
interface](midi.md#roland-mpu-401-midi-interface) needed to communicate with
it. The emulated MPU-401 supports the so-called **Intelligent Mode**, which
most older games require (e.g., all older Sierra adventures). Most later games
only need the much simpler **UART Mode**. Intelligent mode fully supports UART
mode, so it'

``` ini
[midi]
mididevice = mt32
mpu401 = uart

[mt32]
romdir = /path/to/mt32-roms

# Picks the CM-32L if available; best overall model except for old games
model = auto

# Early MT-32 games need this (e.g., older Sierra adventures)
model = mt32_old
```

!!! tip

    Using the layering approach of DOSBox [configuration
    files](../../configuration.md#config-layering), you can pick one specific
    MT-32 model per game.


## Roland ROM images

The emulation of the Roland MT-32 family of devices requires the ROM data from
the original hardware to work. 

!!! important

    The Roland MT-32 ROM images are copyrighted by Roland Corporation,
    therefore they cannot be bundled with DOSBox. You need to provide these
    ROM files yourself to get any sound out of the MT-32 emulation.

ROMs are identified by their checksums, so file names do not matter. Both
interleaved and non-interleaved ROM dumps are supported. Once you have
acquired the necessary ROM sets, it's recommended to copy them to the default
ROM directory:

<div class="compact" markdown>

| Platform | Default MT-32 ROM directory                               |
| -------- | --------------------------------------------------------- |
| Windows  | `C:\Users\<USERNAME>\AppData\DOSBox\mt32-roms\`           |
| macOS    | `/Users/<USERNAME>/Library/Preferences/DOSBox/mt32-roms/` |
| Linux    | `$HOME/.local/share/dosbox/mt32-roms/`                    |

</div>

By doing so, you will make these ROMs globally available for all games.

Alternatively, you can create `mt32-roms` subfolders in your individual game
folders to hold these ROMs, then DOSBox will find them when started from these
game folders.

You can customise where DOSBox looks for these ROMs, see the [romdir](#romdir)
configuration setting for further details.



### Supported ROM sets

The following ROM sets are supported. These originate from the MAME video
game preservation project --- learn more by searching for _"mt32 mame roms"_
and _"cm32l mame roms"_.

<div class="compact" markdown>

| Model               | `model` value            | Control ROM filename                               |
| ------------------- | ------------------------ | -------------------------------------------------- |
| CM-32L/LAPC-I v1.02 | `cm32l_102`              | `cm32l_control.rom`                                |
| CM-32L/LAPC-I  1.00 | `cm32l_100`              | `lapc-i.v1.0.0.ic3.bin`                            |
| MT-32 v2.04 ("new") | `mt32_204` or `mt32_new` | `mt32_2.0.4.ic28.bin`                              |
| MT-32 v1.07 ("old") | `mt32_107` or `mt32_old` | `mt32_1.0.7.ic26.bin`<br>`mt32_1.0.7.ic27.bin`     |
| MT-32 v1.06         | `mt32_106`               | `mt32_1.0.6.ic26.bin`<br>`mt32_1.0.6.ic27.bin`     |
| MT-32 v1.05         | `mt32_105`               | `mt32_1.0.5.ic26.bin`<br>`mt32_1.0.5.ic27.bin`     |
| MT-32 v1.04         | `mt32_104`               | `mt32_1.0.4.ic26.bin`<br>`mt32_1.0.4.ic27.bin`     |
| MT-32 BlueRidge     | `mt32_bluer`             | `blue_ridge__mt32a.bin`<br>`blue_ridge__mt32b.bin` |

| Model                 | PCM ROM filename                                                     |
| --------------------- | -------------------------------------------------------------------- |
| MT-32 (all versions)  | `r15449121.ic37.bin`<br>`r15179844.ic21.bin`<br>`r15179845.ic22.bin` |
| CM-32L (all versions) | `r15179945.ic8.bin`                                                  |

</div>


### Listing the installed ROMs

Run the `MIXER /LISTMIDI` command to see the list of available MT-32 ROMs:

<!-- TODO(JN) screenshot -->

In the above screenshot, `mt32_107` is the currently active model. The green
highlighted **'y'** additionally indicates which directory will be used during
the actual loading. <!-- TODO rewrite this -->

!!! warning

    Both the control and PCM ROMs need to be present for a given
    model. If some model could not be detected, or you're getting `Failed to find
    ROMs for model <model_name>` error at startup, make sure that both ROM sets
    have been copied to your ROM directory for that model.




### ROM lookup paths

If `romdir` is not set, DOSBox Staging searches the following directories for
MT-32 ROM files (in order). You can also place ROMs in an `mt32-roms` subfolder
inside the game's working directory.

**Windows**

1. `%LOCALAPPDATA%\DOSBox\mt32-roms\`
2. `C:\mt32-rom-data\`

**macOS**

1. `~/Library/Preferences/DOSBox/mt32-roms/`
2. `~/Library/Audio/Sounds/MT32-Roms/`
3. `/usr/local/share/mt32-rom-data/`
4. `/usr/share/mt32-rom-data/`

**Linux**

1. `$XDG_DATA_HOME/dosbox/mt32-roms/` (defaults to `~/.local/share/dosbox/mt32-roms/`)
2. `$XDG_DATA_HOME/mt32-rom-data/` (defaults to `~/.local/share/mt32-rom-data/`)
3. `$XDG_DATA_DIRS/mt32-rom-data/` (defaults to `/usr/local/share/mt32-rom-data/` and `/usr/share/mt32-rom-data/`)
4. `$XDG_CONFIG_HOME/dosbox/mt32-roms/` (defaults to `~/.config/dosbox/mt32-roms/`)


## Mixer channel

The Roland MT-32 outputs to the **MT32** [mixer](../mixer.md) channel.


## Configuration settings

Roland MT-32 settings are to be configured in the `[mt32]` section.


##### model

:   Roland MT-32/CM-32L model to use. You must have the ROM files for the
    selected model available (see [romdir](#romdir)). The lookup for the best
    models is performed in order as listed.

    Possible values:

    <div class="compact" markdown>

    - `auto` *default*{ .default } -- Pick the best available model.
    - `cm32l` -- Pick the best available CM-32L model.
    - `mt32_old` -- Pick the best available "old" MT-32 model (v1.0x).
    - `mt32_new` -- Pick the best available "new" MT-32 model (v2.0x).
    - `mt32` -- Pick the best available MT-32 model.
    - `<version>` -- Use the exact specified model version (e.g., `mt32_204`).

    </div>

    !!! note

        Run `MIXER /LISTMIDI` to see the list of available models.


##### mt32_filter

:   Filter for the Roland MT-32/CM-32L audio output.

    Possible values:

    - `off` *default*{ .default } -- Don't filter the output.
    - `<custom>` -- Custom filter definition; see
      [Custom filter settings](../analog-output-filters.md#custom-filter-settings)
      for details.


##### romdir

:   The directory containing the Roland MT-32/CM-32L ROMs (unset by default).
    The directory can be absolute or relative, or leave it unset to use the
    `mt32-roms` directory in your DOSBox configuration directory. Other common
    system locations will be checked as well.

    !!! note

        The file names of the ROM files do not matter; the ROMs are identified
        by their checksums. Both interleaved and non-interleaved ROM files are
        supported.

