# General MIDI

**General MIDI (GM)** was introduced in 1991 to solve a simple but annoying
problem: MIDI could tell a synthesiser *what notes to play*, but not *which
instruments to use*. GM standardised the instrument assignments --- program
number 1 is always an acoustic grand piano, program 41 is always a violin, and
so on --- so that music written for one GM device would sound recognisable on
any other.

The Roland SC-55, released the same year, was the first Sound Canvas module
and quickly became the de facto standard for DOS game music. Roland's own **GS
(General Standard)** extension sat on top of GM, adding extra instruments,
percussion kits, and digital effects while remaining fully
backwards-compatible with the core GM specification.

Most DOS game composers wrote and tested their music on Roland Sound Canvas
hardware. Games like [Monkey Island
2](https://www.mobygames.com/game/289/monkey-island-2-lechucks-revenge/),
[Ultima VII](https://www.mobygames.com/game/608/ultima-vii-the-black-gate/),
[Doom](https://www.mobygames.com/game/1068/doom/), and [Duke Nukem
3D](https://www.mobygames.com/game/365/duke-nukem-3d/) all sound their best
when played through an SC-55 or a compatible device. Since different GM
manufacturers recorded their own instrument samples, playback can sound
noticeably different from one device to another, but the SC-55 is the gold
standard for what the composer intended.

## The MT-32 to General MIDI transition

MIDI sound in DOS games evolved through three distinct periods:

- **1987--1991: MT-32 era** --- The [Roland MT-32](roland-mt-32.md) was the
  only MIDI option. Sierra On-Line, LucasArts, and Origin Systems led the
  way with dedicated MT-32 music.
- **1991--1993: transition** --- The SC-55 and General MIDI standard arrived.
  Many games supported both MT-32 and GM, giving players a choice. Games
  like [Indiana Jones and the Fate of Atlantis](https://www.mobygames.com/game/198/indiana-jones-and-the-fate-of-atlantis/)
  and [Star Wars: X-Wing](https://www.mobygames.com/game/195/star-wars-x-wing/)
  sound excellent on either device.
- **1993 onwards: GM dominance** --- General MIDI became the standard. MT-32
  support faded as the SC-55 was cheaper, more widely available, and
  standardised. By 1995, very few new games included MT-32 support.

??? note "Notable games with General MIDI support"

    <div class="compact" markdown>

    - [Day of the Tentacle (1993)](https://www.mobygames.com/game/659/maniac-mansion-day-of-the-tentacle/)
    - [Descent (1995)](https://www.mobygames.com/game/454/descent/)
    - [Doom (1993)](https://www.mobygames.com/game/1068/doom/)
    - [Duke Nukem 3D (1996)](https://www.mobygames.com/game/365/duke-nukem-3d/)
    - [Full Throttle (1995)](https://www.mobygames.com/game/414/full-throttle/)
    - [Gabriel Knight (1993)](https://www.mobygames.com/game/665/gabriel-knight-sins-of-the-fathers/)
    - [Monkey Island 2 (1991)](https://www.mobygames.com/game/289/monkey-island-2-lechucks-revenge/)
    - [Sam & Max Hit the Road (1993)](https://www.mobygames.com/game/672/sam-max-hit-the-road/)
    - [Star Wars: Dark Forces (1995)](https://www.mobygames.com/game/379/star-wars-dark-forces/)
    - [Star Wars: TIE Fighter (1994)](https://www.mobygames.com/game/204/star-wars-tie-fighter/)
    - [System Shock (1994)](https://www.mobygames.com/game/545/system-shock/)
    - [The Dig (1995)](https://www.mobygames.com/game/499/the-dig/)
    - [Ultima VII (1992)](https://www.mobygames.com/game/608/ultima-vii-the-black-gate/)
    - [WarCraft II (1995)](https://www.mobygames.com/game/1339/warcraft-ii-tides-of-darkness/)

    </div>

Determining whether a game supports General MIDI, the Roland MT-32, or both
is not always obvious from the setup utility alone. The community-maintained
[List of MT-32-compatible computer games](https://www.vogonswiki.com/index.php/List_of_MT-32-compatible_computer_games)
on the VOGONS Wiki is the most comprehensive reference for which MIDI devices
each game supports and which produces the best results.

For a more detailed introduction to MIDI and its role in DOS gaming, see the
[MIDI overview](midi.md).

The **Yamaha DB50XG** and **MU-series** (MU50, MU80, MU100, etc.) are
alternative MIDI modules that offer excellent SC-55 compatibility with a more
modern and punchy sonic character. Many enthusiasts prefer them to the SC-55
in certain games. For a detailed comparison of how these modules stack up
against the SC-55, see [this
article](https://blog.johnnovak.net/2023/03/05/grand-ms-dos-gaming-general-midi-showdown/).

DOSBox Staging offers two main ways to get General MIDI playback:


## Sound Canvas emulation

For the most accurate SC-55 reproduction, DOSBox Staging supports Roland Sound
Canvas emulation via the **Nuked SC55 CLAP** audio plugin. This provides
sample-accurate playback of the actual SC-55 sound engine --- as close to the
real hardware as you can get without owning one.

### Sound Canvas setup

To enable Sound Canvas emulation, set the MIDI device and place the required
ROM files:

``` ini
[midi]
mididevice = soundcanvas
```

Download the SC-55 ROM files from
[here](https://archive.org/details/nuked-sc-55-clap-rom-files), unpack the
ZIP archive, then move the contents of the `Nuked-SC55-Resources/ROMs/` folder
into the `soundcanvas-roms` directory inside your DOSBox configuration folder:

<div class="compact" markdown>

| Platform | ROM directory                                           |
|----------|---------------------------------------------------------|
| Windows  | `C:\Users\<USERNAME>\AppData\Local\DOSBox\soundcanvas-roms\` |
| macOS    | `~/Library/Preferences/DOSBox/soundcanvas-roms/`        |
| Linux    | `~/.local/share/dosbox/soundcanvas-roms/`               |

</div>

This is what the contents of `soundcanvas-roms` should look like:

```
soundcanvas-roms
├── SC-55-v1.10
│   ├── sc55_rom1.bin
│   ├── sc55_rom2.bin
│   ├── sc55_waverom1.bin
│   ├── sc55_waverom2.bin
│   └── sc55_waverom3.bin
├── SC-55-v1.20
│   ...
├── SC-55-v1.21
...
```

You can also set a custom ROM directory via the
[`soundcanvas_rom_dir`](#soundcanvas_rom_dir) setting.

!!! note

    Sound Canvas emulation is CPU-intensive. You'll need a mid-range or better
    desktop-class CPU from the last 5--7 years for glitch-free playback. If
    your system struggles, [FluidSynth](#fluidsynth) with a good SoundFont is
    a lighter alternative.

!!! tip

    You can switch between Sound Canvas models on the fly by changing the
    [`soundcanvas_model`](#soundcanvas_model) setting --- no restart needed.


## Sound Canvas revisions

The SC-55 went through several firmware revisions. The most important
difference is **Capital Tone Fallback (CTF)** --- a Roland GS feature that
ensures correct instrument playback when a game requests a GS "variation
tone" that isn't available on the module. Without CTF, the module may play
silence or the wrong instrument. Many DOS games rely on CTF for correct
audio, particularly for percussion and instrument variations.

Due to a patent dispute with Yamaha, Roland was forced to remove CTF from
later firmware revisions. This makes the choice of firmware version
significant for DOS gaming.

DOSBox Staging emulates the following Sound Canvas firmware versions:

<div class="compact" markdown>

| Model | Version | `soundcanvas_model` | CTF | Notes |
|-------|---------|---------------------|-----|-------|
| SC-55 | v1.00 | `sc55_100` | Yes | First release; some instrument mapping bugs |
| SC-55 | v1.10 | `sc55_110` | Yes | Bug fixes |
| SC-55 | v1.20 | `sc55_120` | Yes | Corrected instrument #122; GS reset |
| SC-55 | v1.21 | `sc55_121` | Yes | Fixed NRPN processing bugs --- **recommended** |
| SC-55 | v2.00 | `sc55_200` | No | CTF removed (Yamaha patent dispute) |
| SC-55mk2 | v1.00 | `sc55mk2_100` | No | 28 voices, 354 sounds; no CTF |
| SC-55mk2 | v1.01 | `sc55mk2_101` | No | Minor revisions |

</div>

The **SC-55 v1.21** is the best overall choice for DOS gaming --- it has
Capital Tone Fallback, correct instrument mappings, and all known firmware
bugs fixed. DOSBox Staging's `soundcanvas_model = auto` prefers v1.21 when
its ROM is available. Avoid v2.00 and the Mk II versions for games that
rely on CTF.

??? note "Games that require Capital Tone Fallback (CTF) for correct audio"

    These games use GS variation tones that depend on CTF. On SC-55 v2.00
    or Mk II firmware (which lack CTF), certain instruments or percussion
    will sound incorrect or be silent.

    <div class="compact" markdown>

    - [Blood (1997)](https://www.mobygames.com/game/793/blood/)
    - [Duke Nukem 3D (1996)](https://www.mobygames.com/game/365/duke-nukem-3d/)
    - [Dune II (1992)](https://www.mobygames.com/game/327/dune-ii-the-building-of-a-dynasty/)
    - [Extreme Assault (1997)](https://www.mobygames.com/game/3165/extreme-assault/)
    - [Hexen (1995)](https://www.mobygames.com/game/383/hexen-beyond-heretic/)
    - [Might and Magic IV (1992)](https://www.mobygames.com/game/451/might-and-magic-clouds-of-xeen/)
    - [Might and Magic V (1993)](https://www.mobygames.com/game/452/might-and-magic-darkside-of-xeen/)
    - [Shadow Warrior (1997)](https://www.mobygames.com/game/1779/shadow-warrior/)
    - [The Elder Scrolls: Arena (1994)](https://www.mobygames.com/game/1704/the-elder-scrolls-arena/)
    - [WarCraft II (1995)](https://www.mobygames.com/game/1339/warcraft-ii-tides-of-darkness/)

    </div>


## FluidSynth

**FluidSynth** is a built-in software MIDI synthesiser that uses **SoundFont**
(`.sf2`) sample banks to generate audio. Unlike Sound Canvas emulation, it
does not attempt to replicate any specific hardware device --- instead, the
character of the music depends entirely on which SoundFont you load. This
makes it very flexible but also means results vary: a good SoundFont can sound
close to an SC-55 on many games, while a poor one can sound noticeably wrong.

FluidSynth is lighter on CPU than Sound Canvas emulation and is a good
option when you don't have SC-55 ROMs or when you prefer a particular
SoundFont's character.

### SoundFont setup

Download a SoundFont file and place it in the `soundfonts` directory inside
your DOSBox configuration folder:

<div class="compact" markdown>

| Platform | SoundFont directory                                    |
|----------|--------------------------------------------------------|
| Windows  | `C:\Users\<USERNAME>\AppData\Local\DOSBox\soundfonts\` |
| macOS    | `~/Library/Preferences/DOSBox/soundfonts/`             |
| Linux    | `~/.local/share/dosbox/soundfonts/`                    |

</div>

Then set the [`soundfont`](#soundfont) option to the filename (the `.sf2`
extension can be omitted):

``` ini
[midi]
mididevice = fluidsynth

[fluidsynth]
soundfont = GeneralUser-GS
```

### Recommended SoundFonts

**[GeneralUser GS](https://schristiancollins.com/generaluser.php)** is the
best all-round choice for DOS gaming. It offers well-balanced instruments,
good General MIDI and GS compatibility, and consistently pleasant results
across a wide range of games. This is what we recommend for most users.

Other options worth trying:

- **[FluidR3_GM_GS](https://github.com/musescore/MuseScore/tree/master/share/sound)** ---
  Comes bundled with MuseScore. Fairly close to SC-55 character on many
  titles, though some instruments can sound thin.
- **Creative 4MB GM/GS/MT** (`4GMGSMT.SF2`) --- The SoundFont shipped with
  Creative Labs AWE32/AWE64 sound cards. A nostalgic choice that
  approximates SC-55 characteristics, though at a lower sample quality.

!!! tip

    Some SoundFonts are louder or quieter than others. Use the
    [`soundfont_volume`](#soundfont_volume) setting to normalise the volume,
    or append a percentage to the `soundfont` value (e.g.,
    `soundfont = Arachno 40` to reduce it to 40%).

!!! tip

    You can switch SoundFonts on the fly by changing the
    [`soundfont`](#soundfont) setting while FluidSynth is active --- no restart
    needed. Changing any `[fluidsynth]` setting takes effect immediately and
    triggers a SoundFont reload.

For a hands-on walkthrough of setting up FluidSynth with a specific game,
see the [Star Wars: Dark
Forces](../../../getting-started/star-wars-dark-forces.md#setting-up-general-midi-sound)
chapter of the getting started guide.


## External MIDI devices

If you have a real MIDI device connected to your computer, DOSBox Staging
can send MIDI output directly to it. This is the ultimate option for
enthusiasts who own an actual SC-55 or other hardware synthesiser.


## Mixer channels

Sound Canvas outputs to the **SOUNDCANVAS** [mixer](../mixer.md) channel, and
FluidSynth to the **FSYNTH** channel.


## Configuration settings

### FluidSynth

FluidSynth settings are to be configured in the `[fluidsynth]` section.


##### fsynth_chorus

:   Configure the FluidSynth chorus.

    Possible values:

    - `auto` *default*{ .default } -- Enable chorus, except for known
      problematic SoundFonts.
    - `on` -- Always enable chorus.
    - `off` -- Disable chorus.
    - `<custom>` -- Custom setting via five space-separated values:
      voice-count (0--99), level (0.0--10.0), speed in Hz (0.1--5.0),
      depth (0.0--21.0), and modulation-wave (`sine` or `triangle`).
      For example: `3 1.2 0.3 8.0 sine`

    !!! note

        You can disable the FluidSynth chorus and enable the mixer-level
        chorus on the FluidSynth channel instead, or enable both chorus
        effects at the same time.


##### fsynth_filter

:   Filter for the FluidSynth audio output.

    Possible values:

    - `off` *default*{ .default } -- Don't filter the output.
    - `<custom>` -- Custom filter definition; see
      [Custom filter settings](../analog-output-filters.md#custom-filter-settings)
      for details.


##### fsynth_reverb

:   Configure the FluidSynth reverb.

    Possible values:

    - `auto` *default*{ .default } -- Enable reverb.
    - `on` -- Enable reverb.
    - `off` -- Disable reverb.
    - `<custom>` -- Custom setting via four space-separated values:
      room-size (0.0--1.0), damping (0.0--1.0), width (0.0--100.0),
      and level (0.0--1.0).
      For example: `0.61 0.23 0.76 0.56`

    !!! note

        You can disable the FluidSynth reverb and enable the mixer-level
        reverb on the FluidSynth channel instead, or enable both reverb
        effects at the same time.


##### soundfont

:   Name or path of SoundFont file to use (`default.sf2` by default). The
    SoundFont will be looked up in the following locations in order:

    <div class="compact" markdown>

    - The user-defined SoundFont directory (see [soundfont_dir](#soundfont_dir)).
    - The `soundfonts` directory in your DOSBox configuration directory.
    - Other common system locations.

    </div>

    The `.sf2` extension can be omitted. You can use paths relative to the
    above locations or absolute paths as well.

    !!! note

        Run `MIXER /LISTMIDI` to see the list of available SoundFonts.


##### soundfont_dir

:   Extra user-defined SoundFont directory (unset by default). If set,
    SoundFonts are looked up in this directory first, then in the standard
    system locations.


##### soundfont_volume

:   Set the SoundFont's volume as a percentage (`100` by default). This is
    useful for normalising the volume of different SoundFonts. Valid range is
    1 to 800.


### Sound Canvas

Sound Canvas settings are to be configured in the `[soundcanvas]` section.


##### soundcanvas_filter

:   Filter for the Roland Sound Canvas audio output.

    Possible values:

    - `on` *default*{ .default } -- Filter the output. This applies a 1st order low-pass filter at 11 kHz (`lpf 1 11000`).
    - `off` -- Don't filter the output.
    - `<custom>` -- Custom filter definition; see
      [Custom filter settings](../analog-output-filters.md#custom-filter-settings)
      for details.


##### soundcanvas_model

:   Roland Sound Canvas model to use. One or more CLAP audio plugins that
    implement the supported Sound Canvas models must be present in the
    `plugins` directory in your DOSBox installation or configuration directory.

    Possible values:

    <div class="compact" markdown>

    - `auto` *default*{ .default } -- Pick the best available model.
    - `sc55` -- Pick the best available original SC-55 model.
    - `sc55mk2` -- Pick the best available SC-55mk2 model.
    - `<version>` -- Use the exact specified model version (e.g., `sc55_121`).

    </div>


##### soundcanvas_rom_dir

:   The directory containing the Roland Sound Canvas ROMs (unset by default).
    The directory can be absolute or relative, or leave it unset to use the
    `soundcanvas-roms` directory in your DOSBox configuration directory. Other
    common system locations will be checked as well.
