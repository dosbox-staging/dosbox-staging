# MIDI

## Overview

Between about 1998 and 1994, MIDI music was *the* ultimate high-end audio
option in DOS gaming, offering never-heard-before realism and CD quality audio
fidelity. Not many could afford the high price tag of MIDI sound modules back
in the day---they often sold for 3 to 5 times higher as a Sound Blaster card.
But thanks to the wonders of emulation, now all DOS enthusiasts can experience
MIDI music nirvana in its full glory!

Because MIDI plays such a pivotal role in DOS gaming history, it is important
to understand its origins and how it works at a basic level.

## A brief introduction to MIDI

The MIDI standard (Musical Instrument Digital Interface) was created in the
early 1980s with the original intention to make communication between audio
equipment produced by different manufacturers easier. It was aimed at the
professional market, but thanks to the home computer revolution of the 80s, it
eventually found its way into home studios, and then into computer games as
well.

To understand MIDI and the motivation behind it, let's look at how a simple
integrated synthesiser works. The synthesiser has a piano keyboard, some sort
of electronic circuitry that generates sounds, and an integrated speaker. When
you press keys on the keyboard, the synthesiser plays sounds through its
little speaker.

This works well if you only have a single synthesiser, but after a while
people wanted to make things more modular. In a more flexible setup, a single
keyboard could control one or more sound modules which would be connected to
an audio mixer. The keyboard and the sound modules could even come from
different manufacturers---for this to work, the introduction of an open
communication standard was needed, and thus MIDI was born.

In the above example, whenever you press a key on the keyboard, a short MIDI
message is transmitted to the sound module, instructing it to play a certain
instrument at a certain pitch. Because the communication is all digital, the
human keyboard player and the keyboard itself can be easily replaced with a
computer. A so-called MIDI sequencer program could send thousands of MIDI
messages at precisely timed intervals, potentially controlling a full
orchestra's worth of musical instruments! It's easy to see how this novel
computer-aided approach to music production and performance had completely
revolutionised the music industry in the 80s.

MIDI compositions are best viewed as the computer music equivalent of sheet
music. A MIDI score only contains note data and abstract descriptions of
the instruments that should play them---the generation of the actual
sounds is entirely the MIDI sound module's responsibility using its on-board
samples or sound synthesis capabilities.

MIDI is a great early example of a successful open-source technology---forty
years after its inception, it's still widely used for a variety of purposes,
ranging from traditional music production to automating theatrical lighting
and live sound show control.


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

- [Roland MT-32 and variants](../sound-devices/roland-mt-32) --- Released in
  1987, this module was the only option for MIDI music in DOS games until
  about 1992. Most notably, you need the MT-32 for the best music in older
  Sierra and LucasArts adventure games.

- [General MIDI](../sound-devices/general-midi) --- The world's first General
  MIDI capable module, the Roland Sound Canvas SC-55, was released 1991.
  Games started adding General MIDI support from about 1992 onwards, which
  resulted in the MT-32 losing most of its relevance by the mid-90s.

You can read more about these modules in their respective documentation sections.


## Setting up MIDI

DOSBox emulates a Roland MPU-401 compatible MIDI interface which is enabled by
default. The emulated MPU-401 operates in the so-called *Intelligent Mode*
out-of-the-box; this provides certain extra features many older games depend
on, and more recent games that do not use these feature are generally not
affected negatively by it.

MIDI related settings are to be configured in the `[midi]` section. The most
common usage scenario is to leave [mididevice](#mididevice) at its default
`auto` setting, then set [midiconfig](#midiconfig) to:

- `mt32` to use the built-in [Roland MT-32](TODO) emulation,
- `fluidsynth` to use the built-in [FluidSynth](#) MIDI synthesiser,
- or to an ID that identifies an external MIDI device.

You can use the `mixer /listmidi` DOS command to see the list of available external
MIDI devices. For example:

TODO image

!!! tip

    To easily switch between a built-in and an external MIDI device per game
    (typically between the built-in MT-32 emulation and Roland's Sound Canvas
    VA running in an external MIDI host program), you can use the [layered
    configuration approach](TODO) to your advantage.

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


## Configuration settings

MIDI related settings are to be configured in the `[midi]` section.


##### mididevice

:   Selects the MIDI device where data from the emulated MPU-401 MIDI
    interface is sent.

    Possible values:

      - `none` --- Disable MIDI output.

      - `auto` *default*{ .default } --- Either one of the built-in MIDI
        synthesisers (if [midiconfig](#midiconfig) is set to `fluidsynth` or
        `mt32`), or a MIDI device external to DOSBox which might be a software
        synthesiser or a physical device (any other [midiconfig](#midiconfig)
        value). See [midiconfig](#midiconfig) for all possible configuration
        combinations.

      - `fluidsynth` --- The built-in FluidSynth MIDI synthesiser (SoundFont
        player). See the [FluidSynth](TODO) configuration settings for further
        details.

      - `mt32` --- The built-in Roland MT-32 synthesiser. See the [Roland
        MT-32](../sound-devices/roland-mt-32) configuration settings for further details.

      - `coreaudio` *(macOS only)* --- Use the built-in macOS MIDI synthesiser.
      - `coremidi` *(macOS only)*  --- Any device that has been configured in
        the macOS Audio MIDI Setup.

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

##### mpu401

:   MPU-401 MIDI interface mode to emulate.

    Possible values:

    - `none` --- Disable MPU-401 emulation.

    - `intelligent` *default*{ .default } --- The intelligent mode was present
      on the original MPU-401 manufactured by Roland and on some clones. It
      provides MIDI sequencer features and a precise MIDI event sending
      mechanism via timestamped messages among a few other things. Applications could offload MIDI related , freeing up
      slow CPUs to do other tasks. Earlier MT-32 games depend on these
      features and will not produce sound in `uart` mode.

    - `uart` --- Use UART mode of the MPU-401 that simply forwards each byte
      written to the MPU to the MIDI device in real-time. Sound Blaster cards
      can only operate in UART mode, and virtually all General MIDI games need
      require only UART mode.

