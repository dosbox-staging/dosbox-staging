# CLAP MIDI synth

!!! tip "New to MIDI in DOS games?"

    If you're not familiar with how MIDI works in DOS games, read the overview
    in the [MIDI](../midi.md) section first --- it explains how DOS games use
    MIDI and the difference between the [Roland MT-32](roland-mt-32.md) family
    of devices and [Roland Sound Canvas / General
    MIDI](roland-sound-canvas.md).

The **CLAP MIDI synth** sound device lets you play MIDI music through a
software synthesiser plugin in [**CLAP** (CLever Audio
Plugin)](https://cleveraudio.org/) format. This lets you use any General
MIDI-capable synthesiser plugin in DOSBox Staging.

If you want to try a different MIDI sound module or software synthesiser,
CLAP plugins provide an alternative to the built-in
[FluidSynth](fluidsynth.md) SoundFont player. Some plugins need additional
files, such as sample banks or ROM images; follow the plugin's own setup
instructions to install these.

!!! warning

    You must use a General MIDI-capable CLAP plugin, otherwise you will hear
    utterly wrong music and sound. The advice given in the [MT-32 vs General
    MIDI](../midi.md#mt-32-vs-general-midi) section applies here too --- all
    CLAP synthesisers support MIDI (as a communication protocol), but only a
    small subset of them support the General MIDI standard.

!!! note "Sound Canvas uses its own MIDI device"

    The bundled Nuked SC55 CLAP plugin is handled by the dedicated
    [Sound Canvas](roland-sound-canvas.md) MIDI device. Select
    `mididevice = soundcanvas` to use it. It is excluded from the CLAP MIDI
    synth's plugin list and cannot be selected with `mididevice = clap`.


## Plugin setup

Download the CLAP plugin of your choice for your operating system, then place
the `.clap` file in the `plugins` folder inside your DOSBox configuration
folder, creating the folder if needed:

<div class="compact" markdown>

| Platform | Plugin folder
| -------- | ---------------------------------------------------------
| Windows  | `C:\Users\<USERNAME>\AppData\Local\DOSBox\plugins\`
| macOS    | `/Users/<USERNAME>/Library/Preferences/DOSBox/plugins/`
| Linux    | `$HOME/.config/dosbox/plugins/`

</div>

Each `.clap` file is a plugin **library**, and a library can contain one or
more **plugins**.

Restart DOSBox Staging after installing a plugin, then run `MIXER /LISTMIDI`
at the DOS prompt. Under the `clap` section, you will see the plugin libraries
listed, along with the plugins each one contains. Each plugin has a name and a
numeric index within its library. When the CLAP MIDI synth is active, the
selected plugin is highlighted and marked with an asterisk (`*`).

To enable a CLAP plugin, set [`mididevice`](../midi.md#mididevice) to `clap` in
the `[midi]` section, then use one of these methods to select your plugin in
the `[clap]` section:

- Set only [`clap_library`](#clap_library) to the filename of the `.clap`
  library (the `.clap` extension is optional). The first plugin in that
  library is used.

- Set only [`clap_plugin`](#clap_plugin) to a plugin name. All libraries are
  searched for a plugin with that name.

- Set both [`clap_library`](#clap_library) and [`clap_plugin`](#clap_plugin)
  to select a specific plugin in a specific library.

- Set [`clap_library`](#clap_library), and set [`clap_plugin`](#clap_plugin)
  to the plugin's numeric index as shown by `MIXER /LISTMIDI`.

Names are matched case-insensitively, and a partial name is enough. If several
names match, the first match is used.

For example, a Windows setup using
[S-MU2000](https://github.com/tarboh/S-MU2000), which emulates the Yamaha
MU2000 sound module, could look like this:

``` ini
[midi]
mididevice = clap

[clap]
clap_library = S-MU2000
clap_env = "S_MU2000_ROMS=C:\My ROMs\MU2000"
```

This example requires the [S-MU2000 CLAP
plugin](https://github.com/tarboh/S-MU2000) and its ROM files. Follow the
plugin's [ROM setup instructions](https://github.com/tarboh/S-MU2000#roms) and
replace the example path with your ROM folder. The environment variable name
is specific to this plugin; other plugins may need a different
[`clap_env`](#clap_env) setting, or none at all.

Finally, select the **General MIDI** or **Roland Sound Canvas** music device
in the game's sound setup utility.

!!! note

    The [`midiconfig`](../midi.md#midiconfig) setting is not used by the CLAP
    MIDI synth.


## Plugin compatibility

The plugin must accept MIDI messages on its first note input and provide a
stereo first audio output. DOSBox Staging sends MIDI to that input and mixes
that stereo output; additional audio outputs are not mixed into the game's
sound.

DOSBox Staging does not display the plugin's graphical interface or provide
controls for its presets and parameters. Choose a plugin that can play music
using only its default settings, configuration files, or environment
variables.

If a plugin cannot be found or loaded, check that it is in CLAP format and that
its operating system and processor match your DOSBox Staging build, and that
any required data files are installed. The log contains details of plugin
loading errors.


## Environment variables

Some plugins use **environment variables** --- named settings passed to the
plugin --- to locate ROMs or other data files. The [`clap_env`](#clap_env)
setting lets you supply these without changing your operating system's
settings.

Use the variable names documented by the plugin. Separate assignments with
spaces, and put double quotes around an entire `NAME=VALUE` assignment when
the value contains spaces, as in the S-MU2000 example above. These assignments
overwrite existing environment variables in the running DOSBox Staging
process.


## Sample rate

By default, [`clap_sample_rate`](#clap_sample_rate) is `auto`, which uses the
[mixer sample rate](../mixer.md#rate). If your plugin has a known native
sample rate, set it explicitly for the best results. DOSBox Staging resamples
the plugin's audio to the mixer rate when necessary.


## Mixer channel

The CLAP MIDI synth outputs to the **CLAP** [mixer
channel](../mixer.md#list-of-mixer-channels).


## Configuration settings

Configure the CLAP MIDI synth in the `[clap]` section.


##### clap_library

:   CLAP plugin library file to use (unset by default). A CLAP library is a
    single `.clap` file that can contain one or more plugins. Libraries are
    searched for in the `plugins` directory in your DOSBox installation or
    configuration directory.

    Use a case-insensitive partial filename; the `.clap` extension can be
    omitted. Leave unset to search all libraries by plugin name, which requires
    [`clap_plugin`](#clap_plugin) to be set. Use `MIXER /LISTMIDI` to list all
    plugins in all libraries.


##### clap_plugin

:   CLAP MIDI synth to use (unset by default). This is a plugin inside a CLAP
    library, which may contain several plugins. At least one of
    [`clap_library`](#clap_library) or [`clap_plugin`](#clap_plugin) must be
    set.

    Use a case-insensitive partial plugin name or its numeric index within the
    library. An index requires [`clap_library`](#clap_library) to be set. If
    several plugins match, the first one is used. If only `clap_library` is
    set, the first plugin in that library is used. If only `clap_plugin` is
    set, all libraries are searched for a plugin with that name.

    The plugin must accept MIDI on its first note input and provide a stereo
    first audio output. Plugins handled by a dedicated MIDI synth (e.g.,
    `soundcanvas`) are excluded.


##### clap_env

:   Environment variables to set before loading the CLAP plugin (unset by
    default). Separate `NAME=VALUE` assignments with spaces. Quote an entire
    assignment if its value contains spaces, for example:
    `"ROM_PATH=C:\My ROMs"`. Existing environment variables are overwritten.


##### clap_sample_rate

:   Sample rate of the CLAP MIDI synth in Hz (`auto` by default). Leave unset
    or use `auto` to match the mixer rate, or specify a rate from 8000 to
    192000 Hz. Choose the plugin's native sample rate when known; the mixer
    resamples its output as needed.
