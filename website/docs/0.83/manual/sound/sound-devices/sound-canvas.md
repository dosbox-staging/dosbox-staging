# Sound Canvas

The **Roland Sound Canvas SC-55**, released in 1991, was the first General MIDI
sound module and quickly became the de facto standard for DOS game music. Most
composers wrote and tested their music on Sound Canvas hardware, making it the
gold standard for how the soundtrack was meant to sound.

For background on the General MIDI and GS standards, and help deciding which
MIDI device to use for a particular game, see the [MIDI overview](../midi.md#which-midi-device-should-i-use).

DOSBox Staging emulates the SC-55 via the **Nuked SC55 CLAP** audio plugin,
providing sample-accurate playback of the actual SC-55 sound engine --- as
close to the real hardware as you can get without owning one.


## Setup

To enable Sound Canvas emulation, set the MIDI device and place the required
ROM files:

``` ini
[midi]
mididevice = soundcanvas
```

Download the SC-55 ROM files from
[here](https://archive.org/details/nuked-sc-55-clap-rom-files), unpack the
ZIP archive, then move the contents of the `Nuked-SC55-Resources/ROMs/` folder
into the `soundcanvas-roms` folder inside your DOSBox configuration folder:

<div class="compact" markdown>

| Platform | ROM folder
|----------|---------------------------------------------------------
| Windows  | `C:\Users\<USERNAME>\AppData\Local\DOSBox\soundcanvas-roms\`
| macOS    | `~/Library/Preferences/DOSBox/soundcanvas-roms/`
| Linux    | `~/.config/dosbox/soundcanvas-roms/`

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

You can also set a custom ROM folder via the
[`soundcanvas_rom_dir`](#soundcanvas_rom_dir) setting.

!!! note

    Sound Canvas emulation is CPU-intensive. You'll need a mid-range or better
    desktop-class CPU from the last 5--7 years for glitch-free playback. If
    your system struggles, [FluidSynth](fluidsynth.md) with a good SoundFont is
    a lighter alternative.

!!! tip

    You can switch between Sound Canvas models on the fly by changing the
    [`soundcanvas_model`](#soundcanvas_model) setting --- no restart needed.

For a hands-on walkthrough of setting up Sound Canvas emulation with a specific
game, see the [Star Wars: Dark
Forces](../../../getting-started/star-wars-dark-forces.md#sound-canvas-emulation)
chapter of the getting started guide.


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


## Mixer channel

Sound Canvas outputs to the **SOUNDCANVAS** [mixer
channel](../mixer.md#list-of-mixer-channels).


## Configuration settings

Sound Canvas settings are to be configured in the `[soundcanvas]` section.


##### soundcanvas_model

:   Roland Sound Canvas model to use. One or more CLAP audio plugins that
    implement the supported Sound Canvas models must be present in the
    `plugins` folder in your DOSBox installation or configuration folder.

    Possible values:

    <div class="compact" markdown>

    - `auto` *default*{ .default } -- Pick the best available model.
    - `sc55` -- Pick the best available original SC-55 model.
    - `sc55mk2` -- Pick the best available SC-55mk2 model.
    - `<version>` -- Use the exact specified model version (e.g., `sc55_121`).

    </div>


##### soundcanvas_rom_dir

:   The folder containing the Roland Sound Canvas ROMs (unset by default).
    The folder can be absolute or relative, or leave it unset to use the
    `soundcanvas-roms` folder in your DOSBox configuration folder. Other
    common system locations will be checked as well.


##### soundcanvas_filter

:   Filter for the Roland Sound Canvas audio output.

    Possible values:

    - `on` *default*{ .default } -- Filter the output. This applies a 1st order low-pass filter at 11 kHz (`lpf 1 11000`).
    - `off` -- Don't filter the output.
    - `<custom>` -- Custom filter definition; see
      [Custom filter settings](../output-filters.md#custom-filter-settings)
      for details.
