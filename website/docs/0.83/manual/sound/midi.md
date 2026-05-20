# MIDI

Between about 1988 and 1994, **MIDI music** was *the* ultimate high-end audio
option in DOS gaming, offering never-heard-before realism and CD quality audio
fidelity. Not many could afford the high price tag of MIDI sound modules back
in the day --- they often cost 3 to 5 times as much as a Sound Blaster
card. But thanks to the wonders of emulation, now all DOS enthusiasts can
experience MIDI music nirvana in its full glory!

Because MIDI plays such a pivotal role in DOS gaming history, it is important
to understand its origins and how it works at a basic level.

## A brief introduction to MIDI

The **MIDI standard (Musical Instrument Digital Interface)** was created in
the early 1980s to allow audio equipment from different manufacturers to
communicate with each other. Originally aimed at the professional market, it
soon found its way into home studios and computer games thanks to the home
computer revolution.

MIDI is essentially a communication protocol. When you press a key on a MIDI
keyboard, a short message is sent to a sound module instructing it to play a
certain instrument at a certain pitch. Because the communication is digital,
the keyboard can be replaced with a computer --- a MIDI sequencer program can
send thousands of precisely timed messages, controlling a full orchestra's
worth of instruments.

MIDI compositions are best viewed as the computer music equivalent of sheet
music --- a MIDI score only contains note data and abstract descriptions of
the instruments, while the sound module is responsible for generating the
actual sounds using its on-board samples or synthesis capabilities.


## MIDI in DOS gaming

In the beginning, all DOS games came on floppies. Even if a game came on 10
floppies or more, there was simply not enough space for long musical pieces
stored as digital audio! Given that MIDI scores take very little disk space
compared to digital audio (generally, just a few tens of kilobytes per
composition), MIDI music was an ideal match for DOS games before the
wide-spread adoption of the CD-ROM with its huge 650 to 700 megabytes of
capacity on a single CD. This is similar in principle to how the [Creative
Music System and AdLib](sound-devices/adlib-cms.md) synthesisers were used in games, only these MIDI sound
modules were capable of producing much higher quality music using sampled
sounds of real-world instruments.


## The General MIDI standard

**General MIDI (GM)** was introduced in 1991 to solve a simple but annoying
problem: MIDI could tell a synthesiser *what notes to play*, but not *which
instruments to use*. GM standardised the instrument assignments --- program
number 1 is always an acoustic grand piano, program 41 is always a violin, and
so on --- so that music written for one GM device would sound recognisable on
any other.

The Roland SC-55, released the same year, was the first [Sound
Canvas](sound-devices/sound-canvas.md) module and quickly became the de facto
standard for DOS game music. Roland's own **GS
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
standard for hearing the music as the composer intended.

The **Yamaha DB50XG** and **MU-series** (MU50, MU80, MU100, etc.) are
alternative MIDI modules that offer excellent SC-55 compatibility with a more
modern and punchy sonic character. Many enthusiasts prefer them to the SC-55
in certain games. For a detailed comparison of how these modules stack up
against the SC-55, see [this
article](https://blog.johnnovak.net/2023/03/05/grand-ms-dos-gaming-general-midi-showdown/).


## The MT-32 to General MIDI transition

MIDI sound in DOS games evolved through three distinct periods:

- **1987--1991: MT-32 era** --- The [Roland MT-32](sound-devices/roland-mt-32.md) was the
  only MIDI option. Sierra On-Line, LucasArts, and Origin Systems led the
  way with dedicated MT-32 music.
- **1991--1993: transition** --- The [SC-55](sound-devices/sound-canvas.md) and General MIDI standard arrived.
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


## Which MIDI device should I use?

DOSBox Staging offers three main ways to get MIDI playback:

- [Roland MT-32 emulation](sound-devices/roland-mt-32.md) --- built-in
  emulation of the MT-32 and CM-32L.
- [Sound Canvas emulation](sound-devices/sound-canvas.md) --- sample-accurate
  SC-55 emulation via the Nuked SC55 CLAP plugin.
- [FluidSynth](sound-devices/fluidsynth.md) --- a built-in software MIDI
  synthesiser using SoundFont files.

Determining whether a game supports the [Sound Canvas](sound-devices/sound-canvas.md) (General MIDI), the [Roland MT-32](sound-devices/roland-mt-32.md), or both
is not always obvious from the setup utility alone. The community-maintained
[List of MT-32-compatible computer games](https://www.vogonswiki.com/index.php/List_of_MT-32-compatible_computer_games)
on the VOGONS Wiki is the most comprehensive reference for which MIDI devices
each game supports and which produces the best results. There is no other
reliable way to know for sure --- game manuals and setup utilities often omit
or misrepresent MIDI support.

As a rough rule of thumb:

- **Pre-1992 games** (especially Sierra and LucasArts adventures) --- try
  [Roland MT-32](sound-devices/roland-mt-32.md) first.
- **1992--1993 games** --- check the VOGONS list; many sound excellent on
  either the MT-32 or General MIDI.
- **Post-1993 games** --- almost always General MIDI. Use [Sound
  Canvas](sound-devices/sound-canvas.md) emulation for the most authentic
  results, or [FluidSynth](sound-devices/fluidsynth.md) as a lighter
  alternative.

!!! warning "Roland MT-32 is not General MIDI"

    Do not confuse the Roland MT-32 family of MIDI sound modules with General
    MIDI modules! Music composed for the MT-32 often sounds *utterly wrong* on a
    General MIDI device, and vice versa. Yes, they both have "MIDI" in their
    names, but that only refers to the MIDI communication protocol. The MT-32
    range of devices are programmable synthesisers, and most MT-32 supporting
    games take advantage of that to create custom sounds. In contrast, General
    MIDI modules feature more realistic-sounding real-world instruments with a
    standardised sound set.

    Quite confusingly, there is a large list of games that claim MT-32
    compatibility but only sound correct on a General MIDI module. Make sure
    to check the [List of games that falsely claim MT-32
    compatibility](https://www.vogonswiki.com/index.php/List_of_MT-32-compatible_computer_games#Games_that_falsely_claim_MT-32_compatibility)
    as well before configuring a game for MT-32 sound.


## Roland MPU-401 MIDI interface

Initially, you had to buy the **Roland MPU-401** MIDI interface to connect an
external MIDI sound module to your PC. This was an external box connected to
your PC which then you hooked up to another external box, your sound module.
Later on, sound cards started incorporating integrated MPU-401 compatible
interfaces, so you could connect the sound module directly to your sound card.

DOSBox Staging emulates a Roland MPU-401 compatible MIDI interface which is
enabled by default. The emulated MPU-401 operates in the so-called
**Intelligent Mode** out-of-the-box; this provides certain extra features many
older games depend on. The other, more recent mode is **UART Mode**. Older
titles often require Intelligent Mode, most more recent ones work fine with
either.

See [`mpu401`](#mpu401) for more details.


## Setting up MIDI

MIDI related settings are to be configured in the `[midi]` section. Set the
[`mididevice`](#mididevice) setting to select which MIDI device to use:

- `mt32` to use the built-in [Roland MT-32](sound-devices/roland-mt-32.md) emulation,
- `fluidsynth` to use the built-in [FluidSynth](sound-devices/fluidsynth.md) MIDI synthesiser,
- `soundcanvas` to use the built-in [Sound Canvas](sound-devices/sound-canvas.md) emulation,
- or `port` (the default) to send MIDI data to an [external MIDI
  device](#using-external-midi-devices) configured via
  [`midiconfig`](#midiconfig).

You can use the `MIXER /LISTMIDI` DOS command to see the list of available
external MIDI devices.

!!! tip

    To easily switch between a built-in and an external MIDI device per game
    (typically between the built-in MT-32 emulation and Roland's Sound Canvas
    VA running in an external MIDI host program), you can use the [layered
    configuration
    approach](../using-dosbox-staging/configuration.md#config-layering) to
    your advantage.

    As an example, to easily switch between the built-in MT-32 emulation and
    an external MIDI device that contains "loopMIDI" in its name, put this
    into your global configuration:

    ```ini
    [midi]
    mididevice = mt32
    midiconfig = loopMIDI
    ```

    Without any further MIDI configuration in your local DOSBox config, this
    will default to using the built-in Roland MT-32 emulation. To switch to
    the external MIDI device, set `mididevice` to `auto` in your local config:

    ```ini
    [midi]
    mididevice = auto
    ```


## Using external MIDI devices

DOSBox Staging can send MIDI data to any MIDI device outside of its own
built-in synthesisers. This includes [physical hardware modules](#physical-midi-devices) connected
via USB, but also [software synthesisers](#virtual-midi-devices) running on
your machine --- anything that appears as a MIDI port to the operating system.

### Physical MIDI devices

If you own a physical MIDI sound module (an MT-32, SC-55, or similar), you
can connect it to your computer and have DOSBox Staging send MIDI data
directly to it. This is the ultimate option for enthusiasts who want
authentic hardware playback.

You'll need a **USB MIDI interface** (e.g., Roland UM-ONE mk2) to connect
the module to a modern computer. The MIDI cable carries note data only ---
you'll also need audio cables from the module's line outputs to your
speakers or audio interface.


### Virtual MIDI devices

You can also route DOSBox's MIDI output to another software synthesiser
running on your machine (such as Roland's Sound Canvas VA, or any DAW or
standalone synth plugin) by using a virtual MIDI loopback driver. This
creates a virtual MIDI cable that connects DOSBox's output to the other
program's input.

- **Windows** --- [loopMIDI](https://www.tobias-erichsen.de/software/loopmidi.html)
  or [loopBE](https://www.nerds.de/en/loopbe1.html) create virtual MIDI
  ports. Install one, create a port, then point your external synthesiser at
  that port for input and set `midiconfig` to the port name in DOSBox.
- **macOS** --- Use the built-in **IAC Driver** in *Audio MIDI Setup* to
  create virtual MIDI ports, or use a third-party tool.
- **Linux** --- ALSA's `snd-virmidi` module or JACK MIDI can create virtual
  ports.


### Finding your MIDI device

Run `MIXER /LISTMIDI` at the DOSBox DOS prompt to list all MIDI output ports
available on your system. Set `mididevice` to `port` and `midiconfig` to the
device name:

``` ini
[midi]
mididevice = port
midiconfig = UM-ONE
```

You don't need to type the full device name --- DOSBox Staging performs a
case-insensitive substring match against all available MIDI port names. For
example, `midiconfig = UM-ONE` will match a port called "Roland UM-ONE MIDI
1". If multiple ports match, the first match is used. You can also use the
numeric port ID shown by `MIXER /LISTMIDI`, but name-based identification is
preferred as the numbers can change when you unplug and reattach USB devices.

``` ini
[midi]
mididevice = port
midiconfig = 2
```


### Platform-specific MIDI

DOSBox Staging uses the native MIDI subsystem on each platform:

- **macOS** --- Uses Apple's **CoreMIDI** framework. All MIDI destinations
  visible in *Audio MIDI Setup* (including virtual ports from apps like
  loopMIDI or DAWs) are available. Use `MIXER /LISTMIDI` to see the list.

- **Linux** --- Uses **ALSA** sequencer ports. You can specify the port by
  name or by ALSA address (e.g., `midiconfig = 14:0`). ALSA prioritises
  synthesiser-type ports when matching by name.

- **Windows** --- Uses the standard **Windows MIDI** API. All MIDI output
  devices registered with the system appear in `MIXER /LISTMIDI`. If no
  device is specified, DOSBox uses the Windows MIDI Mapper (the system
  default).


### macOS CoreAudio synthesiser

On macOS, DOSBox Staging provides a `coreaudio` MIDI device option that uses
Apple's built-in DLS synthesiser. Although not a physical external device, it
is external to DOSBox --- the audio is rendered by macOS's own Audio Unit
synthesiser, bypassing DOSBox's mixer entirely. You can optionally point it at
a SoundFont file:

``` ini
[midi]
mididevice = coreaudio
midiconfig = /path/to/soundfont.sf2
```

The default Apple DLS sound set is fairly basic; loading a good SoundFont
improves results significantly.


### SysEx delays for older hardware

Some older MIDI modules (notably the Roland MT-32 with revision 0 PCB) can
overflow their internal buffers when receiving large System Exclusive (SysEx)
messages. If you experience garbled patches or missing instruments, add
`delaysysex` after the device identifier:

``` ini
[midi]
mididevice = port
midiconfig = UM-ONE delaysysex
```

This adds a small calculated delay between SysEx messages to prevent buffer
overflows.


## MIDI output corrections

By default, DOSBox Staging corrects common MIDI protocol errors in the output
of DOS games. Many games send "All Notes Off" messages instead of proper
per-note Note Off commands. DOSBox converts these to individual Note Off
messages for each active note, which prevents hanging notes and makes recorded
MIDI data easier to edit in a sequencer. This also fixes the infamous hanging
notes issue with the Roland RA-50 external MIDI module, which does not
implement the "All Notes Off" message at all.

The [`raw_midi_output`](#raw_midi_output) setting disables these corrections.
This produces no audible difference on most synthesisers --- it only affects
how the MIDI data stream is represented. Enable it if you need the unmodified
MIDI output for debugging, working with music applications, or when an
external hardware or MIDI recording setup requires the unprocessed stream.


## Configuration settings

MIDI related settings are to be configured in the `[midi]` section.


##### mididevice

:   Selects the MIDI device where data from the emulated MPU-401 MIDI
    interface is sent.

    Possible values:

      - `port` *default*{ .default } --- A MIDI port of the host operating
        system's MIDI interface. You can configure the port to use with the
        [`midiconfig`](#midiconfig) setting. See [Using external MIDI
        devices](#using-external-midi-devices) for details.

      - `fluidsynth` --- The built-in FluidSynth MIDI synthesiser (SoundFont
        player). See [FluidSynth](sound-devices/fluidsynth.md) for details.

      - `mt32` --- The built-in Roland MT-32 synthesiser. See [Roland
        MT-32](sound-devices/roland-mt-32.md#configuration-settings)
        for details.

      - `soundcanvas` --- The internal Roland SC-55 synthesiser (requires a
        CLAP audio plugin). See [Sound
        Canvas](sound-devices/sound-canvas.md) for details.

      - `coreaudio` *(macOS only)* --- Use the built-in macOS CoreAudio
        synthesiser. See [macOS CoreAudio
        synthesiser](#macos-coreaudio-synthesiser) for details. The SoundFont
        to use can be specified with [`midiconfig`](#midiconfig).

      - `none` --- Disable MIDI output.


##### midiconfig

:   Configuration options for the selected MIDI device.

    Depending on the [`mididevice`](#mididevice) setting, the `midiconfig`
    setting is interpreted as follows:

    <div class="compact" markdown>

    | `mididevice` setting | `midiconfig` setting
    | -------------------- | ----------------------
    | `none`               | N/A
    | `fluidsynth`         | N/A
    | `mt32`               | N/A
    | `soundcanvas`        | N/A
    | `port`               | `ID [delaysysex]` --- Use the MIDI device with this ID. The ID can be a numeric port index or a part of the device name (case-insensitive substring match). The `delaysysex` option might be needed for some older external MIDI modules.
    | `coreaudio`          | Path to the SoundFont to use.

    </div>

    With `mididevice = port`, using name-based identification is preferable
    as the numeric IDs can change if you unplug then reattach your external
    MIDI adapters or audio interfaces.

    Use the `MIXER /LISTMIDI` command to list the IDs and names of all MIDI
    devices available on your system.

    !!! important

        If you're using a physical Roland MT-32 with revision 0 PCB, the
        hardware may require a delay in order to prevent its buffer from
        overflowing. In that case, add `delaysysex`, e.g., `midiconfig = 2
        delaysysex`.



##### mpu401

:   MPU-401 MIDI interface mode to emulate.

    Possible values:

    - `none` --- Disable MPU-401 emulation.

    - `intelligent` *default*{ .default } --- The intelligent mode was present
      on the original MPU-401 manufactured by Roland and on some clones. It
      provides MIDI sequencer features, precise event timing via timestamped
      messages, and the ability to offload MIDI-related tasks from slow CPUs.
      Earlier MT-32 games depend on these features and will not produce sound
      in `uart` mode.

    - `uart` --- Use UART mode of the MPU-401 that simply forwards each byte
      written to the MPU to the MIDI device in real-time. Sound Blaster cards
      can only operate in UART mode, and virtually all General MIDI games
      require only UART mode.


##### raw_midi_output

:   Enable raw, unaltered MIDI output (`off` by default). The MIDI drivers of
    many games don't fully conform to the MIDI standard, which makes editing
    their MIDI recordings error-prone in MIDI sequencers. DOSBox corrects the
    MIDI output of such games by default with no audible difference; it only
    affects the data representation.

    Only enable this if you need to capture the raw, unaltered MIDI output of
    a program, e.g., when working with music applications or debugging MIDI
    issues.

    Possible values: `on`, `off` *default*{ .default }
