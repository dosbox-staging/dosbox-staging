# MIDI

Between about 1988 and 1994, MIDI music was *the* ultimate high-end audio
option in DOS gaming, offering never-heard-before realism and CD quality audio
fidelity. Not many could afford the high price tag of MIDI sound modules back
in the day --- they often sold for 3 to 5 times higher as a Sound Blaster card.
But thanks to the wonders of emulation, now all DOS enthusiasts can experience
MIDI music nirvana in its full glory!

Because MIDI plays such a pivotal role in DOS gaming history, it is important
to understand its origins and how it works at a basic level.

## A brief introduction to MIDI

The MIDI standard (Musical Instrument Digital Interface) was created in the
early 1980s to allow audio equipment from different manufacturers to
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
capacity on a single CD. This is similar in principle to how the Creative
Music System and AdLib synthesisers were used in games, only these MIDI sound
modules were capable of producing much higher quality music using sampled
sounds of real-world instruments.

Initially, you had to buy the Roland MPU-401 MIDI interface to connect an
external MIDI sound module to your PC. Later on, sound cards started
incorporating integrated MPU-401 compatible interfaces, so you could connect
the sound module directly to your sound card.

DOS games featuring MIDI music support either or both of the following two
major MIDI standards:

- [Roland MT-32 and variants](roland-mt-32.md) --- Released in
  1987, this module was the only option for MIDI music in DOS games until
  about 1992. Most notably, you need the MT-32 for the best music in older
  Sierra and LucasArts adventure games.

- [General MIDI](general-midi.md) --- The world's first General
  MIDI capable module, the Roland Sound Canvas SC-55, was released 1991.
  Games started adding General MIDI support from about 1992 onwards, which
  resulted in the MT-32 losing most of its relevance by the mid-90s.

You can read more about these modules in their respective documentation sections.

Many games support multiple MIDI devices, and the best choice is not always
obvious. The community-maintained [List of MT-32-compatible computer
games](https://www.vogonswiki.com/index.php/List_of_MT-32-compatible_computer_games)
on the VOGONS Wiki is the definitive reference for which MIDI device to use
with each game. There is no other reliable way to know for sure --- game
manuals and setup utilities often omit or misrepresent MIDI support.


## Setting up MIDI

DOSBox emulates a Roland MPU-401 compatible MIDI interface which is enabled by
default. The emulated MPU-401 operates in the so-called *Intelligent Mode*
out-of-the-box; this provides certain extra features many older games depend
on, and more recent games that do not use these feature are generally not
affected negatively by it.

MIDI related settings are to be configured in the `[midi]` section. The most
common usage scenario is to leave [mididevice](#mididevice) at its default
`auto` setting, then set [midiconfig](#midiconfig) to:

- `mt32` to use the built-in [Roland MT-32](roland-mt-32.md) emulation,
- `fluidsynth` to use the built-in [FluidSynth](general-midi.md#fluidsynth) MIDI synthesiser,
- or to an ID that identifies an external MIDI device.

You can use the `mixer /listmidi` DOS command to see the list of available
external MIDI devices.

!!! tip

    To easily switch between a built-in and an external MIDI device per game
    (typically between the built-in MT-32 emulation and Roland's Sound Canvas
    VA running in an external MIDI host program), you can use the [layered
    configuration approach](../../configuration.md#config-layering) to your
    advantage.

    As an example, to easily switch between the built-in MT-32 emulation and
    an external MIDI device that contains "loopMIDI" in its name, put this
    into your global configuration:

    ```ini
    [midi]
    mididevice = mt32
    midiconfig = loopMIDI
    ```

    Without any futher MIDI configuration in your local DOSBox config, this
    will default to using the built-in Roland MT-32 emulation. To switch to
    the external MIDI device, set `mididevice` to `auto` in your local config:

    ```ini
    [midi]
    mididevice = auto
    ```


## Using external MIDI hardware

If you own a physical MIDI sound module (an MT-32, SC-55, or similar), you
can connect it to your computer and have DOSBox Staging send MIDI data
directly to it. This is the ultimate option for enthusiasts who want
authentic hardware playback.

You'll need a **USB MIDI interface** (e.g., Roland UM-ONE mk2) to connect
the module to a modern computer. The MIDI cable carries note data only ---
you'll also need audio cables from the module's line outputs to your
speakers or audio interface.

To set it up:

- Connect the USB MIDI interface and power on the sound module.
- Run `MIXER /LISTMIDI` at the DOSBox DOS prompt to list available MIDI
  devices and find your interface's name or ID.
- Set `mididevice` to `port` and `midiconfig` to the device name (or
  numeric ID):

``` ini
[midi]
mididevice = port
midiconfig = UM-ONE
```

Name-based identification is preferred over numeric IDs, as the numbers
can change when you unplug and reattach USB devices.


## Configuration settings

MIDI related settings are to be configured in the `[midi]` section.


##### midiconfig

:   Configuration options for the selected MIDI device.

    Depending on the [mididevice](#mididevice) setting, the `midiconfig`
    setting is interpreted as follows:

    <div class="compact" markdown>

    | `mididevice` setting | `midiconfig` setting   
    | -------------------- | ----------------------
    | `none`               | N/A
    | `fluidsynth`         | N/A
    | `mt32`               | N/A
    | `auto`               | <ul><li>`mt32` --- Use the built-in MT-32 synthesiser</li><li>`fluidsynth` --- Use the built-in FluidSynth MIDI synthesiser</li><li>`ID [delaysysex]` --- Use MIDI device external to DOSBox with this ID. The `delaysysex` option might be needed for some older external MIDI modules.</li></ul>
    | `coreaudio`          | Name of the SoundFont to use.
    | `coremidi`           | N/A

    </div>

    In `auto` mode, the `ID` can be either the numeric identifier of the MIDI
    device (MIDI port) or a part of its name. Using the name-based
    identification is preferable as the numbers can change if you unplug then
    reattach your external MIDI adapters or audio interfaces.

    Use the `mixer /listmidi` command to list the IDs and names of all MIDI
    devices available on your system.

    !!! important

        If you're using a physical Roland MT-32 with revision 0 PCB, the
        hardware may require a delay in order to prevent its buffer from
        overflowing. In that case, add `delaysysex`, e.g., `midiconfig = 2
        delaysysex`.


##### mididevice

:   Selects the MIDI device where data from the emulated MPU-401 MIDI
    interface is sent.

    Possible values:

      - `port` *default*{ .default } --- A MIDI port of the host operating
        system's MIDI interface. You can configure the port to use with the
        [midiconfig](#midiconfig) setting.

      - `fluidsynth` --- The built-in FluidSynth MIDI synthesiser (SoundFont
        player). See the [FluidSynth](general-midi.md#fluidsynth) configuration
        settings for further details.

      - `mt32` --- The built-in Roland MT-32 synthesiser. See the [Roland
        MT-32](roland-mt-32.md#configuration-settings)
        configuration settings for further details.

      - `soundcanvas` --- The internal Roland SC-55 synthesiser (requires a
        CLAP audio plugin). See the [Sound
        Canvas](general-midi.md#sound-canvas) configuration settings for
        further details.

      - `coreaudio` *(macOS only)* --- Use the built-in macOS MIDI synthesiser.
        The SoundFont to use can be specified with [midiconfig](#midiconfig).

      - `none` --- Disable MIDI output.


##### mpu401

:   MPU-401 MIDI interface mode to emulate.

    Possible values:

    - `none` --- Disable MPU-401 emulation.

    - `intelligent` *default*{ .default } --- The intelligent mode was present
      on the original MPU-401 manufactured by Roland and on some clones. It
      provides MIDI sequencer features and a precise MIDI event sending
      mechanism via timestamped messages among a few other things. Applications could offload MIDI-related tasks, freeing up
      slow CPUs to do other tasks. Earlier MT-32 games depend on these
      features and will not produce sound in `uart` mode.

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

