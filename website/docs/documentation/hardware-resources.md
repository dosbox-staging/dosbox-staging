# Hardware resources

DOS has no automatic hardware detection or plug-and-play. Expansion cards like
sound cards and network adapters each occupy a set of **hardware resources**:
an I/O base address, and often an IRQ and DMA channel. When a game or driver
asks you to configure a device, these values must match the DOSBox Staging
configuration exactly --- otherwise you'll get silence, garbled audio, or a
crash.

On real hardware, these values were set via physical jumpers or software
utilities, and getting them wrong was one of the most common sources of
frustration in DOS. DOSBox Staging uses sensible defaults, but you still need
to enter the right values when a game's setup utility asks.


## What these settings mean

**I/O base address** (also called the port) is a hardware address the CPU uses
to communicate with a device --- think of it as a "mailbox number" for the
card. The CPU sends commands to this address and reads data back from it. Each
device needs its own unique address. Common examples: `220` for the Sound
Blaster, `240` for the Gravis UltraSound, `300` for the NE2000 network card.

**IRQ** (Interrupt Request) is how a device signals the CPU that it needs
attention --- for example, when a sound card has finished playing a buffer and
needs more data. Each device needs its own IRQ number; sharing an IRQ between
two active devices will cause one or both to malfunction.

**DMA** (Direct Memory Access) lets a device transfer data directly to or from
system memory without involving the CPU for every byte. Sound cards use DMA to
stream audio data efficiently. The original IBM PC provided DMA channels 0--3
for 8-bit transfers; the AT added channels 4--7 for 16-bit transfers.

**High DMA** is used by 16-bit sound cards (Sound Blaster 16 and later) for
16-bit audio. These cards need *two* DMA channels: a low DMA channel for
8-bit audio backward compatibility with older games, and a high DMA channel
for native 16-bit audio. If a game's setup asks for "16-bit DMA" or "HDMA",
this is what it means.


## Default resource assignments

### Configurable devices

These devices have I/O, IRQ, and/or DMA settings you can change in the DOSBox
Staging configuration:

| Device | I/O base | IRQ | DMA | High DMA | Config section |
|--------|----------|-----|-----|----------|----------------|
| [Sound Blaster](sound/sound-devices/adlib-cms-sound-blaster.md) | 220 | 7 | 1 | 5 | `[sblaster]` |
| [Gravis UltraSound](sound/sound-devices/gravis-ultrasound.md) | 240 | 5 | 3 | --- | `[gus]` |
| [IBM Music Feature Card](sound/sound-devices/imfc.md) | 2A20 | 3 | --- | --- | `[imfc]` |
| [Innovation SSI-2001](sound/sound-devices/innovation.md) | 280 | --- | --- | --- | `[innovation]` |
| [NE2000 Ethernet](networking/ethernet.md) | 300 | 3 | --- | --- | `[ethernet]` |

!!! tip

    When a game's setup utility asks for sound card settings, the most common
    combination is: **Address 220, IRQ 7, DMA 1** for Sound Blaster. If it
    also asks for a **High DMA** (sometimes labelled "16-bit DMA" or "HDMA"),
    enter **5**.


### Devices with fixed settings

These devices use hardcoded resource assignments that cannot be changed in the
configuration:

| Device | I/O base | IRQ | DMA |
|--------|----------|-----|-----|
| [AdLib / OPL](sound/sound-devices/adlib-cms-sound-blaster.md#adlib-music-synthesizer-card) (FM synth) | 388 | --- | --- |
| [MPU-401](sound/sound-devices/midi.md) (MIDI interface) | 330 | 9 | --- |
| [Tandy DAC](sound/sound-devices/tandy.md) | C4 | 7 | 1 |
| [Tandy / PCjr PSG](sound/sound-devices/tandy.md) | C0 | --- | --- |
| [PC speaker](sound/sound-devices/pc-speaker.md) | 61 | --- | --- |
| [Parallel port DACs](sound/sound-devices/covox-variants.md) (Covox, Disney, Stereo-on-1) | 378 | --- | --- |
| [IBM PS/1 Audio](sound/sound-devices/ibm-ps1audio.md) | 200--205 | --- | --- |
| Serial ports (COM1 / COM2 / COM3 / COM4) | 3F8 / 2F8 / 3E8 / 2E8 | 4 / 3 / 4 / 3 | --- |

!!! note

    The AdLib's fixed I/O address at 388 is separate from the Sound Blaster's
    configurable base address. Games can access the OPL chip through either
    address; this is how the Sound Blaster maintains AdLib backward
    compatibility.


## Conflicts and the Tandy DAC

The most common resource conflict in DOSBox Staging is between the **Tandy
DAC** and the **Sound Blaster**. The Tandy DAC is hardcoded to IRQ 7 and DMA
1 --- the same defaults as the Sound Blaster. On real Tandy hardware, later
Sound Blaster models included IRQ sharing to work around this, but in
practice, running both simultaneously is unreliable.

If you're using `machine = tandy` and also want Sound Blaster support, set
`tandy = psg` in the `[speaker]` section. This keeps the Tandy 3-voice PSG
synthesiser (which has no DMA or IRQ requirements) but disables the Tandy DAC,
freeing DMA 1 and IRQ 7 for the Sound Blaster.

``` ini
[speaker]
tandy = psg

[sblaster]
sbtype = sbpro2
```

Additionally, the Tandy PSG occupies I/O ports starting at C0, which overlaps
with the second DMA controller's address range. DOSBox handles this by
shutting down the second DMA controller (channels 4--7) when Tandy sound is
active. This means **high DMA is unavailable on Tandy/PCjr machines**, so
Sound Blaster 16 16-bit audio will not work. Use `sbtype = sbpro2` or earlier
when combining Sound Blaster with Tandy sound.


## Common issues

### Game asks for settings not shown in its setup

Some games only offer a limited set of IRQ or DMA values. If the DOSBox
defaults aren't among the options, change the DOSBox config to match what the
game expects. For example, if a game only offers IRQ 5 for the Sound Blaster,
set `irq = 5` in the `[sblaster]` section.

The same applies to I/O base addresses. If a game only supports address `240`
for its Sound Blaster setup, set `sbbase = 240` in the `[sblaster]` section.


### No sound despite correct settings

Double-check that the I/O address, IRQ, *and* DMA channel all match between
the game's configuration and the DOSBox config. Many games silently fail if
even one value is wrong. Also verify that the correct
[`sbtype`](sound/sound-devices/adlib-cms-sound-blaster.md#sbtype) is selected
--- some games only work with specific Sound Blaster models.

For Sound Blaster 16 games with no 16-bit audio, make sure the high DMA
channel is also set correctly in both the game and DOSBox config.


### Garbled or stuttering audio

This can happen when two emulated devices share the same IRQ or DMA channel.
Ensure each enabled device uses unique values. Check the tables above for
potential conflicts, especially if you have multiple sound devices enabled.


### Games that hardcode settings

A number of early DOS games (roughly 1988--1991) assume fixed Sound Blaster
settings and don't provide a setup utility at all. The most commonly hardcoded
values are I/O 220, IRQ 7, DMA 1 --- which happen to be the DOSBox defaults.
If a game with no setup option produces no sound, these assumptions are usually
the reason, and the defaults should work.


## Further reading

- [AdLib, CMS & Sound Blaster](sound/sound-devices/adlib-cms-sound-blaster.md)
  --- detailed Sound Blaster configuration and model selection
- [Gravis UltraSound](sound/sound-devices/gravis-ultrasound.md) --- GUS
  resource settings and usage modes
- [Sound overview](sound/overview.md) --- general guide to DOS game audio
- [Ethernet](networking/ethernet.md) --- NE2000 network card configuration
