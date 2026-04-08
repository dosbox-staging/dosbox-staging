# General MIDI

**General MIDI (GM)** was introduced in 1991 to solve a simple but annoying
problem: MIDI could tell a synthesiser *what notes to play*, but not *which
instruments to use*. GM standardised the instrument assignments — program
number 1 is always an acoustic grand piano, program 41 is always a violin, and
so on --- so that music written for one GM device would sound recognisable on
any other.

The Roland SC-55, released the same year, was the first Sound Canvas module
and quickly became the de facto standard for DOS game music. Roland's own
"GS" (General Standard) extension sat on top of GM, adding extra
instruments, percussion kits, and digital effects while remaining fully
backwards-compatible with the core GM specification.

Most DOS game composers wrote and tested their music on Roland Sound Canvas
hardware. Games like [Monkey Island
2](https://www.mobygames.com/game/289/monkey-island-2-lechucks-revenge/),
[Ultima VII](https://www.mobygames.com/game/608/ultima-vii-the-black-gate/),
[Doom](https://www.mobygames.com/game/1068/doom/), and [Duke Nukem
3D](https://www.mobygames.com/game/365/duke-nukem-3d/) all sound their best
when played through an SC-55 or a compatible device. Since different GM
manufacturers recorded their own instrument samples, playback can sound
noticeably different from one device to another — but the SC-55 is the gold
standard for what the composer intended.

For a more detailed introduction to MIDI and its role in DOS gaming, see the
[MIDI overview](midi.md).

The **Yamaha DB50XG** and **MU-series** (MU50, MU80, MU100, etc.) are
alternative MIDI modules that offer excellent SC-55 compatibility with a more
modern and punchy sonic character. Many enthusiasts prefer them to the SC-55
in certain games. For a detailed comparison of how these modules stack up
against the SC-55, see [this
article](https://blog.johnnovak.net/2023/03/05/grand-ms-dos-gaming-general-midi-showdown/).

DOSBox Staging offers two main ways to get General MIDI playback:


### Sound Canvas emulation

For the most accurate SC-55 reproduction, DOSBox Staging supports Roland Sound
Canvas emulation via the Nuked SC55 CLAP audio plugin. This provides
sample-accurate playback of the actual SC-55 sound engine --- as close to the
real hardware as you can get without owning one.


### FluidSynth

For a step-by-step walkthrough of setting up FluidSynth with a specific game,
see the [Star Wars: Dark
Forces](../../../getting-started/star-wars-dark-forces.md#setting-up-general-midi-sound)
chapter of the getting started guide.

Alternatively, you can use FluidSynth which is a MIDI synthesiser based on the SoundFont
2 specification. TODO 


### External MIDI devices

If you have a real MIDI device connected to your computer, DOSBox Staging
can send MIDI output directly to it. This is the ultimate option for
enthusiasts who own an actual SC-55 or other hardware synthesiser.


### Mixer channels

Sound Canvas outputs to the **SOUNDCANVAS** mixer channel. and FluidSynth to
the **FSYNTH** mixer channel.


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

    - `on` *default*{ .default } -- Filter the output (1st order low-pass at 11 kHz).
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
