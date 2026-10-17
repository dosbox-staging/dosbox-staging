# CLAP synth

!!! tip "New to MIDI in DOS games?"

    If you're not familiar with how MIDI works in DOS games, read the overview
    in the [MIDI](../midi.md) section first --- it explains how DOS games use
    MIDI and the difference between the [Roland MT-32](roland-mt-32.md) family
    of devices and [Roland Sound Canvas / General
    MIDI](roland-sound-canvas.md).

The **CLAP synth** MIDI device lets you play MIDI music through a software
synthesiser plugin in **CLAP** format. DOSBox Staging loads the plugin
directly and mixes its audio with the game's other sounds, so you don't need
an external MIDI host program or a virtual MIDI cable.

The plugin determines which instruments you hear and which MIDI standards are
supported. For **General MIDI** games, choose a plugin that supports General
MIDI. A plugin that plays only a single instrument will not provide the full
set of instruments those games expect. Likewise, a General MIDI plugin will
not normally play [Roland MT-32](roland-mt-32.md) music correctly.

If you want to try a different MIDI sound module or software synthesiser,
CLAP plugins provide an alternative to the built-in
[FluidSynth](fluidsynth.md) SoundFont player. Some plugins need additional
files, such as sample banks or ROM images; follow the plugin's own setup
instructions to install these.

!!! note "Sound Canvas uses its own MIDI device"

    The bundled Nuked SC55 CLAP plugin is handled by the dedicated
    [Sound Canvas](roland-sound-canvas.md) MIDI device. Select
    `mididevice = soundcanvas` to use it. It is excluded from the CLAP synth's
    plugin list and cannot be selected with `mididevice = clap`.


## Plugin setup

Download the **CLAP version** of your chosen plugin for your operating system
and processor. Other plugin formats, such as VST or Audio Unit, cannot be
loaded by this MIDI device.

Place the `.clap` file in the `plugins` folder inside your DOSBox
configuration folder, creating the folder if needed:

<div class="compact" markdown>

| Platform | Plugin folder
| -------- | ---------------------------------------------------------
| Windows  | `C:\Users\<USERNAME>\AppData\Local\DOSBox\plugins\`
| macOS    | `/Users/<USERNAME>/Library/Preferences/DOSBox/plugins/`
| Linux    | `$HOME/.config/dosbox/plugins/`

</div>

On macOS, a `.clap` plugin can be a bundle that looks like a single file in
Finder. Copy the whole bundle. You can also use the `plugins` folder in your
DOSBox Staging installation.

Restart DOSBox Staging after installing a plugin, then run `MIXER /LISTMIDI`
at the DOS prompt. Under `clap`, this lists plugin libraries and the synths
they contain. Each synth has a name and a numeric index within its library.
When CLAP synth is active, the selected synth is highlighted and marked with
an asterisk (`*`).

Set [`mididevice`](../midi.md#mididevice) to `clap` in the `[midi]` section,
then select your plugin in the `[clap]` section:

- [`clap_library`](#clap_library) selects the `.clap` library file. You can
  omit the extension or use part of the filename.
- [`clap_plugin`](#clap_plugin) selects a synth within that library by name
  or by the numeric index shown by `MIXER /LISTMIDI`.

Both names use case-insensitive partial matching, and the first match is
used. Set both options if you need to distinguish between similarly named
plugins. You can also set only `clap_library` to use the first synth in that
library, or only `clap_plugin` to search all libraries by synth name. A numeric
index always requires `clap_library`. At least one of these settings must be
set.

For example, a Windows setup using
[S-MU2000](https://github.com/tarboh/S-MU2000), which emulates the Yamaha
MU2000 sound module, could look like this:

``` ini
[midi]
mididevice = clap

[clap]
clap_library = S-MU2000.clap
clap_plugin = S-MU2000
clap_env = "S_MU2000_ROMS=C:\My ROMs\MU2000"
clap_sample_rate = 44100
```

This example requires the S-MU2000 CLAP plugin and its ROM files. Follow the
plugin's [ROM setup instructions](https://github.com/tarboh/S-MU2000#roms)
and replace the example path with your ROM folder. The environment variable
name and sample rate are specific to this plugin; other plugins may need
different settings or none at all.

In the game's sound setup utility, select the music device supported by your
plugin, such as **General MIDI**. The [`midiconfig`](../midi.md#midiconfig)
setting is not used by CLAP synth.


## Plugin compatibility

The plugin must accept MIDI messages on its first note input and provide a
stereo first audio output. DOSBox Staging sends MIDI to that input and mixes
that stereo output; additional audio outputs are not mixed into the game's
sound.

DOSBox Staging does not display the plugin's graphical interface or provide
controls for its presets and parameters. Choose a plugin that can play music
with its default settings, configuration files, or environment variables.

If a plugin cannot be found or loaded, check that its format, operating
system, and processor match your DOSBox Staging build, and that any required
data files are installed. The log contains details of plugin loading errors.


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

CLAP synth outputs to the **CLAP** [mixer
channel](../mixer.md#list-of-mixer-channels). For example, `MIXER CLAP 75`
sets its volume to 75%. You can also apply [mixer
effects](../mixer-effects.md) to this channel.


## Configuration settings

CLAP synth settings are to be configured in the `[clap]` section.


##### clap_library

:   CLAP plugin library to use (unset by default). Libraries are searched in the
    'plugins' directory in your DOSBox installation or configuration directory.
    Use a case-insensitive partial filename, with or without the '.clap' extension.
    Leave unset to search all libraries by plugin name.
    Use 'MIXER /LISTMIDI' to list plugins.


##### clap_plugin

:   CLAP MIDI synth to use (unset by default). Use a case-insensitive partial plugin
    name or its numeric index within the library. An index requires 'clap_library'.
    The first matching plugin is used. Leave unset to use the first plugin in the
    selected library. At least one of 'clap_library' or 'clap_plugin' must be set.
    The plugin must accept MIDI on its first note input and provide a stereo first
    audio output. Plugins handled by a dedicated MIDI synth (e.g., 'soundcanvas')
    are excluded.


##### clap_env

:   Environment variables to set before loading the CLAP plugin (unset by default).
    Separate NAME=VALUE assignments with spaces. Quote an entire assignment if its
    value contains spaces, for example: "ROM_PATH=C:\My ROMs".
    Existing environment variables are overwritten.


##### clap_sample_rate

:   Sample rate of the CLAP MIDI synth in Hz ('auto' by default). Leave unset or
    use 'auto' to match the mixer rate, or specify a rate from 8000 to 192000 Hz.
    Choose the plugin's native sample rate when known; the mixer resamples its
    output as needed.
