# Hardware resources

Traditionally, DOS had no facilities to detect and auto-configure connected
hardware --- [plug and play
(PnP)](https://en.wikipedia.org/wiki/Plug_and_play), familiar from modern
operating systems, only started appearing at the very end of the DOS era.
Expansion cards like sound cards and network adapters each occupy a set of
hardware resources: an **I/O base address**, and often an **IRQ** and
**DMA channel**. On real hardware, these values were most often set via
physical jumpers, and since a typical machine had several expansion cards
configured this way, care had to be taken to give each card its own resources
and avoid conflicts --- getting them wrong was one of the most common sources
of frustration in DOS.

When a game or driver asks you to configure a device, these values must match
the DOSBox Staging configuration exactly --- otherwise you'll get silence,
garbled audio, or a crash. DOSBox Staging uses sensible defaults, but you
still need to enter the right values when a game's setup utility asks.


## What these settings mean

**I/O base address** (also called **port**) is a hardware address the CPU uses
to communicate with a device --- think of it as a "mailbox number" for the
card. The CPU sends commands to this address and reads data back from it. Each
device needs its own unique address. Common examples: `220` for the Sound
Blaster, `240` for the Gravis UltraSound, `300` for the NE2000 network card.
I/O base addresses are
[hexadecimal](https://en.wikipedia.org/wiki/Hexadecimal) numbers --- letters
from A to F can also appear in addition to decimal digits. `220` is sometimes
also written as `220h`, `220H`, or `0x220`.

**IRQ** (Interrupt Request) is how a device signals the CPU that it needs
attention --- for example, when a sound card has finished playing a buffer and
needs more data. Each device needs its own IRQ number; sharing an IRQ between
two active devices will cause one or both to malfunction. Unlike I/O base
addresses, IRQ numbers are plain decimal numbers.

**DMA** (Direct Memory Access) lets a device transfer data directly to or from
system memory without involving the CPU for every byte. Sound cards use DMA to
stream audio data efficiently. Like IRQ numbers, DMA channel numbers are
decimal. The original IBM PC provided DMA channels 0--3 for 8-bit transfers;
the IBM PC AT added channels 5--7 for 16-bit transfers (channel 4 is reserved
for internal use).

**High DMA** is used only by Sound Blaster 16 and later cards for 16-bit
audio. These cards need two DMA channels: a low DMA channel for 8-bit audio
backward compatibility with older games, and a high DMA channel for native
16-bit audio. If a game's setup asks for "16-bit DMA", "High DMA", or "HDMA", this is what
it means. Only channels 5, 6, and 7 are usable as a high DMA channel.


## Default resource assignments

### Configurable devices

These devices have I/O, IRQ, and/or DMA settings you can change in the DOSBox
Staging configuration:

| Device                                                           | I/O base   | IRQ   | DMA   | High DMA   | Config settings                                                                                                                                                                                                                |
| --------                                                         | ---------- | ----- | ----- | ---------- | ---------------                                                                                                                                                                                                                |
| [Sound Blaster](../sound/sound-devices/sound-blaster.md)         | 220        | 7     | 1     | 5          | [`sbbase`](../sound/sound-devices/sound-blaster.md#sbbase), [`irq`](../sound/sound-devices/sound-blaster.md#irq), [`dma`](../sound/sound-devices/sound-blaster.md#dma), [`hdma`](../sound/sound-devices/sound-blaster.md#hdma) |
| [Gravis UltraSound](../sound/sound-devices/gravis-ultrasound.md) | 240        | 5     | 3     | ---        | [`gusbase`](../sound/sound-devices/gravis-ultrasound.md#gusbase), [`gusirq`](../sound/sound-devices/gravis-ultrasound.md#gusirq), [`gusdma`](../sound/sound-devices/gravis-ultrasound.md#gusdma)                               |
| [IBM Music Feature Card](../sound/sound-devices/imfc.md)         | 2A20       | 3     | ---   | ---        | [`imfc_base`](../sound/sound-devices/imfc.md#imfc_base), [`imfc_irq`](../sound/sound-devices/imfc.md#imfc_irq)                                                                                                                 |
| [NE2000 Ethernet](../networking/ethernet.md)                     | 300        | 3     | ---   | ---        | [`nicbase`](../networking/ethernet.md#nicbase), [`nicirq`](../networking/ethernet.md#nicirq)                                                                                                                                   |

If you're using the defaults and a game's setup utility asks for sound card
settings, just enter the values from the table above. Most games only ask for
Address, IRQ, and DMA; if one also asks for **High DMA** (sometimes labelled
"16-bit DMA" or "HDMA"), use the High DMA column.


### Devices with fixed settings

These devices use hardcoded resource assignments that cannot be changed in the
configuration. If a game asks the settings of these devices, use the values
from this table.

| Device                                                                                      | I/O base              | IRQ           | DMA
| --------                                                                                    | ----------            | -----         | -----
| [AdLib / OPL](../sound/sound-devices/adlib.md#adlib-music-synthesizer-card) (FM synth)      | 388                   | ---           | ---
| [MPU-401](../sound/midi.md) (MIDI interface)                                                | 330                   | 9             | ---
| [Game port](../input/joystick.md) (joystick)                                                | 201                   | ---           | ---
| [Tandy DAC](../sound/sound-devices/tandy.md)                                                | C4                    | 7             | 1
| [Tandy / PCjr PSG](../sound/sound-devices/tandy.md)                                         | C0                    | ---           | ---
| [PC speaker](../sound/sound-devices/pc-speaker.md)                                          | 61                    | ---           | ---
| [Parallel port DACs](../sound/sound-devices/covox-variants.md) (Covox, Disney, Stereo-on-1) | 378                   | ---           | ---
| [Innovation SSI-2001](../sound/sound-devices/innovation.md)                                 | 280                   | ---           | ---
| [IBM PS/1 Audio](../sound/sound-devices/ibm-ps1audio.md)                                    | 200--205              | ---           | ---
| Serial ports (COM1 / COM2 / COM3 / COM4)                                                    | 3F8 / 2F8 / 3E8 / 2E8 | 4 / 3 / 4 / 3 | ---

!!! note

    The AdLib's fixed I/O address at 388 is separate from the Sound Blaster's
    configurable base address. Games can access the OPL chip through either
    address; this is how the Sound Blaster maintains AdLib backward
    compatibility.


## DMA and IRQ conflict resolution

DOSBox Staging automatically resolves DMA conflicts between the [Gravis
UltraSound](../sound/sound-devices/gravis-ultrasound.md), [Sound
Blaster](../sound/sound-devices/sound-blaster.md), and [Tandy
DAC](../sound/sound-devices/tandy.md). If enabling a device would make it
share a DMA channel with a device that's already active, the newly-enabled
device takes over that channel and the previously-active device is
automatically disabled --- the last device enabled wins. The logs inform you
about this.

The most common case is the Tandy DAC and Sound Blaster, which share the same
default DMA channel (1) and IRQ (7). For example, with `machine = tandy`, the
Tandy DAC uses DMA 1. If you then enable `sbtype = sb1`, the Sound Blaster
claims DMA 1 and the Tandy DAC is automatically disabled, leaving the Sound
Blaster active. The same last-enabled-wins behaviour applies to conflicts
between the Sound Blaster and the Gravis UltraSound.

If you'd rather keep the Tandy 3-voice PSG music working alongside the Sound
Blaster --- rather than relying on auto-disable to pick one --- set
`tandy = psg` in the `[speaker]` section. This keeps the Tandy PSG (which has
no DMA or IRQ requirements) but disables the Tandy DAC outright, freeing DMA 1
and IRQ 7 for the Sound Blaster from the start.

``` ini
[speaker]
tandy = psg

[sblaster]
sbtype = sbpro2
```

Additionally, the Tandy PSG occupies I/O ports starting at `C0`, which overlaps
with the second DMA controller's address range. DOSBox Staging handles this by
shutting down the second DMA controller (channels 4--7) when Tandy sound is
active. This means **high DMA is unavailable on Tandy/PCjr machines**, so
Sound Blaster 16 16-bit audio will not work. Use `sbtype = sbpro2` or earlier
when combining Sound Blaster with Tandy sound.


## Common issues

### Game asks for settings not shown in its setup

Some games only offer a limited set of IRQ or DMA values. If the DOSBox
defaults aren't among the options, change the DOSBox Staging config to match
what the game expects. For example, if a game only offers IRQ 5 for the Sound
Blaster, set `irq = 5` in the `[sblaster]` section.

The same applies to I/O base addresses. If a game only supports address `240`
for its Sound Blaster setup, set `sbbase = 240` in the `[sblaster]` section.


### No sound despite correct settings

Double-check that the I/O address, IRQ, *and* DMA channel all match between
the game's configuration and the DOSBox Staging config. Many games silently fail if
even one value is wrong. Also verify that the correct
[`sbtype`](../sound/sound-devices/sound-blaster.md#sbtype) is
selected --- some games only work with specific Sound Blaster models.

For Sound Blaster 16 games with no 16-bit audio, make sure the high DMA
channel is also set correctly in both the game and DOSBox Staging config.


### Garbled or stuttering audio

This can happen when two emulated devices share the same IRQ or DMA channel.
Ensure each enabled device uses unique values. Check the tables above for
potential conflicts, especially if you have multiple sound devices enabled.


### Games that hardcode settings

A number of early DOS games (roughly 1988--1992) assume fixed Sound Blaster
settings and don't provide a proper setup utility, or only partially detect
the card. The most commonly hardcoded values are **I/O 220**, **IRQ 7**, **DMA
1** --- the Sound Blaster's original factory defaults, and also the DOSBox
Staging defaults. If a game with no setup option produces no sound, these
assumptions are usually the reason, and the defaults should work. Some games
and demos assume the factory defaults on the Gravis UltraSound, too.

This assumption became a lasting problem because Creative Labs later changed
the Sound Blaster's factory-default IRQ from 7 to 5, partly to avoid conflicts
with the parallel port (also commonly on IRQ 7). By then hundreds of games had
already shipped assuming IRQ 7, and many never gained a way to reconfigure it.
Some games only check for a couple of hardcoded IRQ values instead of reading
the actual configuration --- [Stellar
7](https://www.mobygames.com/game/938/stellar-7/), for instance, only
auto-detects IRQ 3 or 7.

Symptoms aren't always total silence --- some games drop specific samples or
effects rather than failing outright.
[Gods](https://www.mobygames.com/game/501/gods/), for example, is known to
lose samples if the Sound Blaster IRQ isn't set to 7.

If a game with no setup option produces no sound, or drops samples or music
unexpectedly, try DOSBox Staging's defaults before troubleshooting further.


## Further reading

<div class="compact" markdown>
- [Sound overview](../sound/overview.md) --- general guide to DOS game audio
- [AdLib](../sound/sound-devices/adlib.md)
- [Sound Blaster](../sound/sound-devices/sound-blaster.md)
- [Gravis UltraSound](../sound/sound-devices/gravis-ultrasound.md)
- [IBM Music Feature Card](../sound/sound-devices/imfc.md)
- [Innovation SSI-2001](../sound/sound-devices/innovation.md)
- [MIDI](../sound/midi.md) --- MPU-401 MIDI interface
- [Serial ports](../networking/serial-ports.md) --- COM port configuration
- [Ethernet](../networking/ethernet.md) --- NE2000 network card configuration
</div>
