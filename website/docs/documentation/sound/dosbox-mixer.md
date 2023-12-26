# DOSBox mixer

## Overview


## The MIXER command

To manage all these mixer channels, DOSBox provides the handy `MIXER` command
that you can use from the DOS command line. If you run `MIXER` without any
arguments, it prints out the current state of the mixer. This is what we get
with the default configuration:

As we can see, we have four channels enabled by default:

- `MASTER` -- This is the final summed stereo output of all other
    mixer channels. The `MASTER` channel is always enabled, always comes first,
    and is always stereo.

- `OPL` -- Stereo output of the Yamaha OPL3 synthesiser on the emulated
    Sound Blaster 16 card.
- `PCSPEAKER` -- Mono output of the ubiquitous small loudspeaker built into most old IBM PC compatibles.
- `SB` -- Stereo digital audio output of the emulated Sound Blaster 16 card.

There is an important thing to note here: some emulated sound devices contain
multiple sub-devices, each having their own mixer channel. The Sound Blaster 16
is such a device---its digital output is routed to the
`SB` channel, while the output of the onboard Yamaha OPL3 synthesiser chip has
its dedicated `OPL` channel.

This is actually similar to how the Sound Blaster software mixer works on later Sound
Blaster cards. The `MIXER` command extends this concept so you can adjust the
parameters of all emulated audio devices via a unified interface.

If you can't remember all this, don't worry---just run `MIXER /?` to display
a handy little summary all available options.

## Changing channel settings

### Volume

### Reverb and chorus

### Crossfeed

### Advanced usage 

You may change the settings of more than one channel in a single command.
For example, the command `MIXER x30 opl 150 r50 c30 sb x10 reverse` accomplishes the
following:

- Set the global crossfeed to 30 (`x30`)
- Set the volume of the `OPL` channel to 150%, the reverb level to 50, and the
    chorus level to 30 (`fm 150 r50 c30`)
- Set the crossfeed of the `SB` channel to 10 and use reverse stereo (`sb x10
    reverse`)

## Listing available MIDI output devices

## List of mixer channels

<div class="compact" markdown>

The below table lists all possible mixer channels with links to the respective
sections of the manual where they're described in detail.

| Channel name  | Device                                       | Stereo? |
| ------------- | -------------------------------------------- | --------|
| `CDAUDIO`     | [CD-DA digital audio]()                      | Yes
| `CMS`         | [Creative Music System (C/MS) synthesiser]() | Yes
| `COVOX`       | [Covox Speech Thing digital audio]()         | No
| `DISNEY`      | [Disney Sound Source (DSS) digital audio]()  | No
| `FSYNTH`      | [FluidSynth MIDI synthesiser]()              | Yes
| `GUS`         | [Gravis UltraSound digital audio]()          | Yes
| `MASTER`      | [Master output channel]()                    | Yes
| `INNOVATION`  | [Innovation SSI-2001 synthesiser]()          | No
| `MT32`        | [Roland MT-32 MIDI synthesiser]()            | Yes
| `OPL`         | [Yamaha OPL synthesiser]()                   | Yes[^stereo]
| `PCSPEAKER`   | [PC speaker]()                               | No
| `PS1`         | [IBM PS/1 Audio synthesiser]()               | No
| `PS1DAC`      | [IBM PS/1 Audio digital audio]()             | No
| `SB`          | [Sound Blaster digital audio]()              | Yes[^stereo]
| `STON1`       | [Stereo-on-1 digital audio]()                | Yes
| `TANDY`       | [Tandy 1000 synthesiser]()                   | No
| `TANDYDAC`    | [Tandy 1000 digital audio]()                 | No

</div>

[^stereo]: The output can either mono or stereo, depending on the selected model.
[^modem]: Used by the modem emulation; not a regular sound output device.


## Mixer signal flow diagram

