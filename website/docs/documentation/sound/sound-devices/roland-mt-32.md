# Roland MT-32

## Overview

The **Roland MT-32 Multi-Timbre Sound Module** was released in 1987 by Roland
Corporation, the iconic Japanese manufacturer of electronic musical
instruments. It featured Roland's novel, patented Linear Arithmetic (LA)
Synthesis which combined sample playback with digital synthesis,  capable of
producing a wide range of realistic and synthesised sounds. As the little
brother of Roland's flagship Roland D-50 synthesiser released in the same
year, it was aimed at the hobbyist musician market.

Around the same time, Sierra On-Line, the company most famous for pioneering
the graphic adventure genre, was looking for ways to push PC audio to the next
level. They took an interest in the MT-32, which lead to Sierra adding support
for the module to most of their games from 1988 onwards. Other companies soon
started following Sierra's lead, which turned the Roland MT-32 a de facto
standard for high-end audio in DOS gaming in the 1988 to 1992 period until
General MIDI and CD Audio took over.

As the Roland MT-32 was considerably more expensive than other options, such
as the AdLib, it remained out of reach for most computer users. 


### MT-32 variants


It was common for PC games released from 1988 to 1992 to support the Roland MT-32. During this era, the MT-32 was followed by the MT-100 which was an MT-32 'new' combined with a Roland PR-100 sequencer. A little later, Roland introduced their "Computer Music" series: the CM-32L and CM-64, both released in 1989.


### MIDI before General MIDI


!!! important

    Although the Roland MT-32 comes with a built-in library of 128 synth and
    30 rhythm patches, it is a fully programmable synthesiser. Most games with
    MT-32 support take advantage of the module's progammability and upload
    their own unique custom sounds at startup.

    Most other sound modules that have so-called "MT-32 compatibility" modes
    can only emulate the built-in sounds; custom patches sound completely
    wrong on them. Even Roland's own later sound modules suffer from the same
    problem. In short, most music composed for the Roland MT-32 only sounds
    correct when played back on a Roland MT-32, and nothing else.



!!! note

    Roland MT-32 support might be referred to as **Roland MT-100**, **LAPC-I**
    (sometimes mispelled as **LAPC-1**), **CM-32L**, or **CM-64** in the
    game's setup program. All these refer to the same thing; you only need to
    research which ROM version is recommended for the game and set it
    accordingly.

    Some games offer a **Roland MT-32 with Sound Blaster** option which should
    be generally preferred to the plain Roland MT-32 option as it might enable 
    additional digital music to be played on the Sound Blaster.



Earlier DOS games that provide a Roland MT-32 sound option frequently use Normal mode during initialisation, or when communicating with the card. Most later games including those that support General MIDI typically only use the more basic UART mode. This mode is much simpler to implement, so almost all hardware that claims to be MPU-401 compatible will have this mode as standard. The Sound Blaster 16 and up only support MPU-401 UART mode, making them unsuitable as a reliable interface for older DOS games with MT-32 support. The older Sound Blasters do not support MPU-401 at all (the game port's pinouts are not MPU-401 compatible). 



DOSBox supports the MPU-401 (MIDI Processing Unit) interface created in 1984 by the Japanese audio company Roland to enable customers to connect MIDI-compatible devices to their home computers. Initially, this interface support was through standalone cards, but partial support was later incorporated into many third-party sound and joystick add-on cards.


The Roland MT-32 Multi-Timbre Sound Module from 1987 was a programmable synthesiser that supported up to 32 notes played at once using a 16-bit DAC at a sample rate of 32000 HZ. This standalone device required an MPU-401 interface installed on the host machine to connect to and use on a PC.


Also, in 1989 Roland released the first of a series of CM (Computer Module) sound modules. Some of which improved but were also mostly compatible with the MT32 module. Models included the CM-32L, an improved MT-32 with 33 added sound-effect samples. And the CM-64 was a more expensive model of the CM32L with extra functionality geared toward computer musicians.

To complicate things, MPU-401 has two modes, ‘intelligent’ and ‘UART,’ otherwise called dumb mode. Many ‘MPU-401 compatible’ cards and devices only support UART mode. In UART mode, the attached MPU-401 component acts as a dumb playback device.

While the intelligent mode, the attached device handles part of the audio processing. This more complex mode allows computers with the slowest generation of Intel PC CPUs (such as the 8086 and 8088) to handle MIDI playback. But intelligent mode had become redundant when support for these CPUs was abandoned in favour of faster chips that could handle the audio processing.



``` ini
[midi]
mididevice = mt32

[mt32]
model  = auto
romdir = /path/to/mt32-roms
```

!!! tip

    Using the layering approach of DOSBox [configuration files], you can pick
    one specific MT-32 model per game.


## Roland ROM images

The emulation of the Roland MT-32 family of devices requires the ROM data from
the original hardware to work. 

!!! important

    The Roland MT-32 ROM images are copyrighted by Roland Corporation,
    therefore they cannot be bundled with DOSBox. You need to provide these
    ROM files yourself to get any sound out of the MT-32 emulation.

Both versioned and unversioned ROM sets are supported---these are explained in
detail below. Once you have acquired the necessary ROM sets, it's recommended
to copy them to the default ROM directory:

<div class="compact" markdown>

| Platform | Default MT-32 ROM directory                               |
| -------- | --------------------------------------------------------- |
| Windows  | `C:\Users\<USERNAME>\AppData\DOSBox\mt32-roms\`           |
| macOS    | `/Users/<USERNAME>/Library/Preferences/DOSBox/mt32-roms/` |
| Linux    | `$HOME/.local/share/dosbox/mt32-roms/`                    |

</div>

By doing so, you will make these ROMs globally available for all games.

Alternatively, you can create `mt32-roms` subfolders in your individual game
folders to hold these ROMs, then DOSBox will find them when started from these
game folders.

You can customise where DOSBox looks for these ROMs, see the [romdir](romdir)
configuration setting for further details.



### Unversioned ROMs

Unversioned ROMs must be named as follows:

<div class="compact" markdown>

- `CM32L_CONTROL.ROM`
- `CM32L_PCM.ROM`
- `MT32_CONTROL.ROM`
- `MT32_PCM.ROM`

</div>

### Versioned ROMs

Versioned ROMs are part of the MAME video game preservation project. Learn
more by searching for _"mt32 mame roms"_ and _"cm32l mame roms"_. These ROM
sets support the following models:


Interleaved ROM dumps

<div class="compact" markdown>

| Model               | `model` value            | Control ROM filename                               |
| ------------------- | ------------------------ | -------------------------------------------------- |
| CM-32L/LAPC-I v1.02 | `cm32l_102`              | `cm32l_control.rom`                                |
| CM-32L/LAPC-I  1.00 | `cm32l_100`              | `lapc-i.v1.0.0.ic3.bin`                            |
| MT-32 v2.04 ("new") | `mt32_204` or `mt32_new` | `mt32_2.0.4.ic28.bin`                              |
| MT-32 v1.07 ("old") | `mt32_107` or `mt32_old` | `mt32_1.0.7.ic26.bin`<br>`mt32_1.0.7.ic27.bin`     |
| MT-32 v1.06         | `mt32_106`               | `mt32_1.0.6.ic26.bin`<br>`mt32_1.0.6.ic27.bin`     |
| MT-32 v1.05         | `mt32_105`               | `mt32_1.0.5.ic26.bin`<br>`mt32_1.0.5.ic27.bin`     |
| MT-32 v1.04         | `mt32_104`               | `mt32_1.0.4.ic26.bin`<br>`mt32_1.0.4.ic27.bin`     |
| MT-32 BlueRidge     | `mt32_bluer`             | `blue_ridge__mt32a.bin`<br>`blue_ridge__mt32b.bin` |

| Model                 | PCM ROM filename                                                     |
| --------------------- | -------------------------------------------------------------------- |
| MT-32 (all versions)  | `r15449121.ic37.bin`<br>`r15179844.ic21.bin`<br>`r15179845.ic22.bin` |
| CM-32L (all versions) | `r15179945.ic8.bin`                                                  |

</div>


### Listing the installed ROMs

In DOSBox Staging you'll be able to run the command `MIXER /LISTMIDI` to check
which ROMs are being detected and currently used. This should translate into:

<p align="center">
  <img src="https://user-images.githubusercontent.com/1557255/113753268-c0f47480-96c2-11eb-892e-b3107e475bb4.png">
</p>

In the screen above, `mt32_107` is the user's selected model. The green
highlighted **'y'** additionally indicates which directory will be used during
the actual loading.

Note that *both* the control and PCM ROMs need to be present for a given
model. If some model could not be detected, or you're getting `Failed to find
ROMs for model <model_name>` error at startup, make sure that both ROM sets
have been copied to your ROM directory for that model.







### Windows

CROSS_GetPlatformConfigDir() + "mt32-roms\\",
"C:\\mt32-rom-data\\",

### macOS

CROSS_GetPlatformConfigDir() + "mt32-roms/",
CROSS_ResolveHome("~/Library/Audio/Sounds/MT32-Roms/"),
"/usr/local/share/mt32-rom-data/",
"/usr/share/mt32-rom-data/",

### Linux

if XDG_DATA_HOME set:
  $XDG_DATA_HOME/dosbox/mt32-roms/
  $XDG_DATA_HOME/mt32-rom-data/
else:
  $HOME/.local/share/dosbox/mt32-roms/
  $HOME/.local/share/mt32-rom-data/

if XDG_DATA_DIRS set:
  XDG_DATA_DIRS/mt32-rom-data/
else:
  /usr/local/share/mt32-rom-data/
  /usr/share/mt32-rom-data/

if XDG_CONF_HOME set:
  $XDG_CONF_HOME/mt32-roms
else:
  $HOME/.config/mt32-roms


