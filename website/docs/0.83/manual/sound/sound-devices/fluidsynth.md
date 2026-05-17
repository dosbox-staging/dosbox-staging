# FluidSynth

**FluidSynth** is a built-in software MIDI synthesiser that uses **SoundFont**
(`.sf2`) sample banks to generate audio. Unlike [Sound Canvas](sound-canvas.md)
emulation, it does not attempt to replicate any specific hardware device ---
instead, the character of the music depends entirely on which SoundFont you
load. This makes it very flexible but also means results vary: a good SoundFont
can sound close to an SC-55 on many games, while a poor one can sound
noticeably wrong.

FluidSynth is lighter on CPU than Sound Canvas emulation and is a good
option when you don't have SC-55 ROMs or when you prefer a particular
SoundFont's character.

For background on the General MIDI standard and help deciding which MIDI
device to use for a particular game, see the [MIDI overview](../midi.md#which-midi-device-should-i-use).


## SoundFont setup

Download a SoundFont file and place it in the `soundfonts` directory inside
your DOSBox configuration folder:

<div class="compact" markdown>

| Platform   | SoundFont directory                                      |
| ---------- | -------------------------------------------------------- |
| Windows    | `C:\Users\<USERNAME>\AppData\Local\DOSBox\soundfonts\`   |
| macOS      | `~/Library/Preferences/DOSBox/soundfonts/`               |
| Linux      | `~/.config/dosbox/soundfonts/`                           |

</div>

Then set the [`soundfont`](#soundfont) option to the filename (the `.sf2`
extension can be omitted):

``` ini
[midi]
mididevice = fluidsynth

[fluidsynth]
soundfont = GeneralUser-GS
```

## Recommended SoundFonts

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
Forces](../../../getting-started/star-wars-dark-forces.md#fluidsynth)
chapter of the getting started guide.


## SoundFont locations

If [`soundfont_dir`](#soundfont_dir) is set, DOSBox Staging searches that
directory first. Otherwise, it searches the following directories for
SoundFont files (in order):

**Windows**

1. `%LOCALAPPDATA%\DOSBox\soundfonts\`
2. `C:\soundfonts\`

**macOS**

1. `~/Library/Preferences/DOSBox/soundfonts/`
2. `~/Library/Audio/Sounds/Banks/`

**Linux**

1. `$XDG_DATA_HOME/dosbox/soundfonts/` (defaults to `~/.local/share/dosbox/soundfonts/`)
2. `$XDG_DATA_HOME/soundfonts/` (defaults to `~/.local/share/soundfonts/`)
3. `$XDG_DATA_HOME/sounds/sf2/` (defaults to `~/.local/share/sounds/sf2/`)
4. `$XDG_DATA_DIRS/soundfonts/` (defaults to `/usr/local/share/soundfonts/` and `/usr/share/soundfonts/`)
5. `$XDG_DATA_DIRS/sounds/sf2/` (defaults to `/usr/local/share/sounds/sf2/` and `/usr/share/sounds/sf2/`)
6. `$XDG_CONFIG_HOME/dosbox/soundfonts/` (defaults to `~/.config/dosbox/soundfonts/`)


## Reverb and chorus

The [`fsynth_reverb`](#fsynth_reverb) and [`fsynth_chorus`](#fsynth_chorus)
settings apply reverb and chorus effects *within the FluidSynth synthesiser
engine itself*, before the audio reaches the DOSBox mixer. This is separate
from the mixer-level [`reverb`](../mixer-effects.md#reverb) and
[`chorus`](../mixer-effects.md#chorus) effects, which are applied after
mixing to all audio channels. The FluidSynth effects only affect MIDI
playback through FluidSynth.

The default `auto` mode applies optimised reverb and chorus settings for the
following SoundFonts (detected by filename or embedded metadata):

<div class="compact" markdown>

- [**GeneralUser GS**](https://schristiancollins.com/generaluser.php) --- custom reverb and chorus
- [**AWE32 SynthGS**](https://github.com/mrbumpy409/AWE32-midi-conversions/blob/main/gm-soundfonts/synthgs-sf2_04-compat.sf2) --- custom reverb and chorus
- [**SB Live! 4GMGSMT**](https://github.com/mrbumpy409/AWE32-midi-conversions/blob/main/gm-soundfonts/4gmgsmt-sf2_04-compat.sf2) --- custom reverb and chorus
- [**FluidR3**](https://archive.org/download/fluidr3-gm-gs) --- chorus disabled (the SoundFont has its own)
- [**SC-55 SoundFont**](https://archive.org/download/500-soundfonts-full-gm-sets/) (Trevor0402) --- custom reverb and chorus

</div>

All other SoundFonts get sensible default reverb and chorus settings. You can
disable the FluidSynth effects and rely on the mixer-level effects instead,
use both together, or fine-tune with custom parameters.


## Mixer channel

FluidSynth outputs to the **FSYNTH** [mixer
channel](../mixer.md#list-of-mixer-channels).


## Configuration settings

FluidSynth settings are to be configured in the `[fluidsynth]` section.


##### soundfont

:   Name or path of SoundFont file to use (`default.sf2` by default). The
    SoundFont will be looked up in the following locations in order:

    <div class="compact" markdown>

    - The user-defined SoundFont directory (see [`soundfont_dir`](#soundfont_dir))
    - The `soundfonts` directory in your DOSBox configuration directory
    - Other common system locations

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


##### fsynth_reverb

:   Configure the FluidSynth reverb.

    Possible values:

    - `auto` *default*{ .default } -- Automatically apply optimised settings
      for common SoundFonts, or enable reverb with the default settings for
      all other SoundFonts.
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


##### fsynth_chorus

:   Configure the FluidSynth chorus.

    Possible values:

    - `auto` *default*{ .default } -- Automatically apply optimised settings
      for common SoundFonts, or enable chorus with the default settings for
      all other SoundFonts.
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
      [Custom filter settings](../output-filters.md#custom-filter-settings)
      for details.
