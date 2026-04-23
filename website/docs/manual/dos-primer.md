# A short DOS primer

## The PC is not a console

To use a game console from the 1980s and 90s, you only needed to know three
things: how to hook it up to the TV, how to insert a cartridge, and how to
turn the thing on and off. That's it!

In contrast, IBM PC compatibles running the DOS operating system are much more
complex beasts. While DOSBox Staging vastly simplifies things and takes the
major hurdles out of the equation, you are still expected to have a minimum
understanding of the hardware and the platform to get the most out of it.

This primer intends to give you a very compressed crash-course --- just enough
to be able to run DOS games effectively. While this is enough to get started,
we encourage you to expand your knowledge on the subject over time in the
areas you're interested in.

## Modularity

Unlike game consoles and most home computers of the era, IBM PC compatibles
have a highly modular architecture. The CPU, video card, sound card, memory,
and storage are all separate, interchangeable components connected through
expansion slots and standardised buses. This "mix and match" design means that
no two PCs were necessarily identical --- the same game might run on wildly
different hardware configurations.

This modularity is what makes DOS configuration more complex than simply
inserting a cartridge: a game needs to know which sound card you have, how
much memory is available, what video standard your graphics card supports, and
so on. DOSBox Staging handles most of this automatically, but understanding
the basic building blocks helps when things need tweaking.


## CPU

The heart of every IBM PC compatible is the CPU. The original IBM PC released
in 1981 used the **Intel 8088** processor running at 4.77 MHz. That's
extremely slow by today's standards --- about 150,000 times slower than an
average smartphone from 2025! Over the course of the DOS era, CPUs evolved
rapidly through the **8086**, **80286**, **80386**, **80486**, and finally the
**Pentium** family in the mid-1990s. Each generation brought major speed
increases and new capabilities.

Rather than emulating a specific CPU clock speed, DOSBox Staging uses a
"cycles" system that controls how many instructions, or **CPU cycles**, are
executed per millisecond. This is analogous to CPU speed --- more CPU cycles
per millisecond means a faster emulated computer. The default settings work
well for the vast majority of games, but some titles require specific speed
ranges to run correctly.

See [CPU settings](system/cpu.md) for CPU cycle ranges per DOS-era and how to
fine-tune speed. The [Getting Started guide](../getting-started/index.md)
walks you through a few a practical examples of adjusting the CPU speed for
specific game.

<div class="compact" markdown>

- [Passport to Adventure](../getting-started/passport-to-adventure.md#cpu-sensitive-games)
- [Beneath a Steel Sky](../getting-started/beneath-a-steel-sky.md#adjusting-the-emulated-cpu-speed)
- [Star Wars Dark Forces](../getting-started/star-wars-dark-forces.md#setting-the-emulated-cpu-speed)

</div>


## Video

The original IBM PC supported only two graphics standards: monochrome
[Hercules](graphics/adapters.md#hercules-graphics-card-hgc) and
[CGA](graphics/adapters.md#cga-and-its-descendants) (Color Graphics Adapter)
with just four colours at 320&times;200. [EGA](graphics/adapters.md#ega)
(Enhanced Graphics Adapter) raised this to 16 colours in 1984, and
[VGA](graphics/adapters.md#vga-and-svga) (Video Graphics Array) arrived in
1987 with 256 colours at 320&times;200 --- the resolution most DOS games are
remembered for. Later [SVGA](graphics/adapters.md#vga-and-svga) cards pushed
to higher resolutions and colour depths.

The [Tandy 1000](dos-eras.md#ega-and-tandy-19841987) line deserves special
mention: it offered 16-colour graphics and three-voice sound in an affordable
package, making it the best overall gaming PC until about 1987. Many mid-1980s
games look and sound noticeably better in Tandy mode.

Because DOS games were designed for CRT monitors, DOSBox Staging includes [CRT
emulation shaders](graphics/rendering.md#adaptive-crt-shaders) that reproduce
the look of the original hardware. This makes a surprisingly large difference
--- pixel art that looks harsh and blocky on a modern flat panel comes alive
with the subtle blending and scanlines of the CRT emulation.

See [Graphics adapters](graphics/adapters.md) for details on each emulated
adapter, and [Rendering](graphics/rendering.md) for shader and display
options. The Getting Started guide covers
[choosing a graphics adapter](../getting-started/enhancing-prince-of-persia.md#graphics-options)
and [aspect ratios](graphics/aspect-ratios.md)
with practical examples.


## Audio

The original IBM PC had no dedicated sound hardware, just a tiny [PC
speaker](sound/sound-devices/pc-speaker.md) designed to produce simple beeps
and square-wave tones. Clever programmers pushed it further and managed to
coerce it to play back digitised sound samples, but it was never designed for
quality audio reproduction.

The [Tandy 1000](sound/sound-devices/tandy.md) was again ahead of its time,
providing three-voice sound via a built-in chip that made it the best gaming
PC for sound until dedicated sound cards arrived.

Real audio came to the PC platform in the late 1987 with the
[AdLib](sound/sound-devices/adlib-cms-sound-blaster.md#fm-synthesisers) card
using FM synthesis, followed by Creative's [Sound
Blaster](sound/sound-devices/adlib-cms-sound-blaster.md) in 1989 which added
digital audio playback. The Sound Blaster became the de facto standard, and
most DOS games from 1990 onwards support it. For high-end music, the [Roland
MT-32](sound/sound-devices/roland-mt-32.md) sound module offered stunning
realistic-sounding audio from 1988 onwards, though it was expensive and
remained a luxury item.

The [Gravis UltraSound (GUS)](sound/sound-devices/gravis-ultrasound.md),
released in 1992, took a different approach by using wavetable synthesis with
actual sampled sounds stored in its onboard RAM, producing much more realistic
audio than FM synthesis. It gained a cult following, particularly in the
demoscene, but never achieved the Sound Blaster's market dominance.

[General MIDI](sound/sound-devices/general-midi.md) standardised a common set
of 128 instrument sounds, allowing games to sound consistent across different
MIDI-compatible devices. The [Roland Sound Canvas
SC-55](sound/sound-devices/general-midi.md#sound-canvas-emulation), released
in 1991, became the de facto reference device for General MIDI game music and
is still considered the gold standard for many DOS game soundtracks.

By the mid-1990s, [CD-ROM audio (CD-DA)](sound/sound-devices/cd-da.md) gave
games access to studio-quality 16-bit 44.1 kHz music played
directly from the game disc. This bypassed the sound card entirely for music,
delivering a dramatic leap in audio fidelity.

See [Sound overview](sound/overview.md) for a guide to selecting the best
audio option for each era. The Getting Started guide walks through
[configuring sound
devices](../getting-started/passport-to-adventure.md#sound-options) and
[setting up Roland MT-32
sound](../getting-started/beneath-a-steel-sky.md#setting-up-roland-mt-32-sound)
step by step.


## Input peripherals

The standard input peripheral for the IBM PC compatibles was the **[keyboard](input/keyboard.md)**
for a long time. Eventually, the **[mouse](input/mouse.md)** gained popularity, but it wasn't a
standard accessory like on the Commodore Amiga or Apple Macintosh line of
computers.

Many DOS games also support **[analog joysticks](input/joystick.md)**. Digital joysticks were a
rarity on the platform.

See [Input overview](input/overview.md) for keyboard, mouse, and joystick
configuration, and the [Key mapper](input/keymapper.md) for remapping
controls.


## Storage

The IBM PC line of computers were initially intended for business use,
therefore [hard drives](storage.md#hard-disks) were standard accessory. They
were optional on the first IBM PC models, and starting from the XT released in
1983, 10 to 20 MB hard drives were included as standard equipment. (Yes, you
read that right, that's _megabytes_! Such a hard drive could only store a
couple of MP3 files or high-resolution images...)

The cost of hard drives were initially astronomical, so most home users could
not afford them. This lead to the development of games that could be run from
[floppy disks](storage.md#floppy-disks) in the early days of the PC. There is
even a special class of self-booting floppy games that can be run even without
an operating system present; these are referred to as ["PC
booter"](storage.md#booting-from-images) games.

Eventually, hard drives went down in price so most people could afford them.
As a result of this, the vast majority of DOS games and applications can be
installed easily onto the hard drive.

By the mid-1990s, [CD-ROM drives](storage.md#cd-roms) became standard
equipment, vastly expanding storage capacity. Many later DOS games shipped on
CD-ROMs, enabling full-motion video, CD audio soundtracks, and voice acting.

This is in stark contrast with the home computers of the era that used
magnetic tapes and floppies for storage due to cost considerations well into
the 1990s.

See [Storage](storage.md) for details on drive letters, mounting directories
and disk images, and the different media types. The Getting Started guide
demonstrates [setting up a game directory](../getting-started/setting-up-prince-of-persia.md#the-c-drive)
and [mounting a CD-ROM image](../getting-started/beneath-a-steel-sky.md#mounting-a-cd-rom-image).


## Memory

The first line of IBM PC XT computers only supported up to **640 KB of
memory**. Later on, various solutions were invented to circumvent this
limitation and make extra memory available to DOS programs. The maximum amount
of memory DOS can handle is 64 MB, but games rarely could use more than 32 MB.

Frankly, DOS memory management is a complex, confusing, and often frustrating
topic --- no one enjoyed dealing with this stuff back in the day. The good
news is that you don't need to know much about it to use DOSBox Staging
effectively. The vast majority of games and applications work well with the
default 16 MB of memory, and usually there is no advantage to
changing this.

See [Memory management](system/dos.md#memory-management) to learn more.


## MS-DOS

**MS-DOS (Microsoft Disk Operating System)** is the operating system that
gives the platform its name. It provides the basic services needed to run
programs: managing files on disk, loading programs into memory, and handling
input/output to hardware devices.

MS-DOS was released in 1981 alongside the original IBM PC and went through
many versions, the last being 6.22 in 1994. Each version added features (e.g.,
subdirectory support in 2.0 in 1983, memory management in 5.0 in 1991), but all
maintained backward compatibility.

DOS is a command-line operating system, there is no graphical desktop. You
interact with it by typing commands at a prompt (e.g., the famous `C:\>`
prompt). Basic navigation involves commands like `DIR` to list files, `CD` to
change directories, and typing a program's name to run it. This is also how
you launch most games: navigate to the game's directory and run its
executable.

DOSBox Staging emulates a DOS-compatible environment, so you don't need to
install MS-DOS separately. The emulated DOS command line works just like the
real thing.

The [Getting started guide](../getting-started/setting-up-prince-of-persia.md)
gives you a gentle introduction to using DOS to configure and launch games.
See [DOS commands](commands.md) for a list of all available commands, and
[DOS](system/dos.md) to learn how to customise the emulated DOS environment.


## Windows 3.1

Wait a minute, isn't this supposed to be about DOS?

That's correct, but early Windows versions 1.0 to 3.1 were not true operating
systems, only operating *environments* implemented as DOS programs. DOSBox
Staging fully supports Windows 3.1, giving you access to the large catalogue
of early CD-ROM games and educational multimedia applications. It also has
partial support for Windows versions 1.0 and 2.0.

Head over to [Running Windows 3.1](windows-31.md) to learn more.
