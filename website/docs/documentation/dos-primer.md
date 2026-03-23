# A short DOS primer

## Introduction

To use a game console from the 1980s and 90s, you only needed to know three
things: how to hook it up to the TV, how to insert a cartridge, and how to
turn the thing on and off. That's it!

In contrast, IBM PC compatibles running the DOS operating system are much more
complex beasts. While DOSBox vastly simplifies things and takes the major
hurdles out of the equation, you are still expected to have a minimum
understanding of the hardware and the platform to get the most out of it. This
primer intends to give you a very compressed crash-course---just enough to be
able to run DOS games effectively. While this is enough to get started, we
encourage you to expand your knowledge on the subject over time in the areas
you're interested in.


## Modularity

Unlike game consoles and most home computers of the era, IBM PC compatibles have
a highly modular architecture. The CPU, video card, sound card, memory, and
storage are all separate, interchangeable components connected through expansion
slots and standardised buses. This "mix and match" design means that no two PCs
were necessarily identical---the same game might run on wildly different hardware
configurations.

This modularity is what makes DOS configuration more complex than simply
inserting a cartridge: a game needs to know which sound card you have, how much
memory is available, what video standard your graphics card supports, and so on.
DOSBox Staging handles most of this automatically, but understanding the basic
building blocks helps when things need tweaking.


## CPU

The heart of every IBM PC compatible is the CPU. The original IBM PC (1981) used
the Intel 8088, a relatively slow processor. Over the course of the DOS era, CPUs
evolved rapidly through the 8086, 80286, 80386, 80486, and finally the Pentium
family in the mid-1990s. Each generation brought major speed increases and new
capabilities.

Rather than emulating a specific CPU clock speed, DOSBox uses a "cycles" system
that controls how many instructions are executed per millisecond. This is
analogous to CPU speed---more cycles means a faster emulated computer. The
default settings work well for the vast majority of games, but some titles
require specific speed ranges to run correctly.



## Storage

The IBM PC line of computers were initially intended for business use,
therefore hard drives were standard accessory. They were optional on the first
IBM PC models, and starting from the XT released in 1983, 10 to 20 MB hard drives
were included as standard equipment.

The cost of hard drives were initially astronomical, so most home
users could not afford them. This lead to the development of games that could
be run from floppies in the early days of the PC. There is even a special
class of self-booting floppy games that can be run even without an operating
system present---these are referred to as "PC booter" games.

Eventually, hard drives went down in price so most people could afford them.
As a result of this, the vast majority of DOS games and applications can be
installed easily onto the hard drive.

This is in stark contrast with the home computers of the era that used
magnetic tapes and floppies for storage due to cost considerations well into
the 1990s.


## Input peripherals

The standard input peripheral for the IBM PC compatibles was the keyboard for
a long time. Eventually, the mouse gained popularity, but it wasn't a standard
accessory like on the Commodore Amiga or Apple Macintosh line of computers.

Many DOS games also support analog joysticks. Digital joysticks were a rarity
on the platform.


## Memory

The first line of IBM PC XT computers only supported up to 640 KB of memory.
Later on, various solutions were invented to circumvent this limitation and
make extra memory available to DOS programs. The
maximum amount of memory DOS can handle is 64 MB.

Frankly, DOS memory management is a complex, confusing, and often frustrating
topic---no one enjoyed dealing with this stuff back in the day. The good news is
that you don't need to know anything about it to use DOSBox effectively.
The vast majority of games and applications work well with the default 16 MB
of extended memory, and usually there is no advantage to changing this.


## MS-DOS

MS-DOS (Microsoft Disk Operating System) is the operating system that gives the
platform its name. It provides the basic services needed to run programs:
managing files on disk, loading programs into memory, and handling input/output
to hardware devices.

DOS is a command-line operating system---there is no graphical desktop. You
interact with it by typing commands at a prompt (e.g., `C:\>`). Basic navigation
involves commands like `DIR` to list files, `CD` to change directories, and
typing a program's name to run it. This is also how you launch most games:
navigate to the game's directory and run its executable.

DOSBox Staging emulates a DOS-compatible environment, so you don't need to
install MS-DOS separately. The emulated DOS command line works just like the real
thing.


## Windows 3.x

Wait a minute, isn't this supposed to be about DOS?

That's correct, but early Windows versions 1.x to 3.x were not true operating
systems, only operating *environments* implemented as DOS programs. DOSBox fully
supports Windows 3.x, giving you access to the large catalogue of early
CD-ROM games and educational multimedia applications. It also has partial
support for Windows versions 1.x and 2.x.


## Typical era configurations

The DOS era spanned roughly 15 years, and the hardware changed dramatically
during that time. Games are designed for the hardware of their era, so
matching the emulated hardware to the game's time period gives the best
results. The key settings to adjust are [`machine`](system/general.md#machine)
(graphics standard), [`cputype`](system/cpu.md#cputype),
[`cycles`](system/cpu.md#cpu_cycles) (CPU speed), and the sound device.

Here are some typical period-correct starting points:

### Early 1980s

For the earliest PC games and booter titles (1981--1984).

``` ini
[dosbox]
machine = cga

[cpu]
cycles = 120

[speaker]
pcspeaker = on

[sblaster]
sbtype = none
```

### Mid-1980s

EGA games and early sound support (1984--1987).

``` ini
[dosbox]
machine = ega

[cpu]
cycles = 300

[speaker]
pcspeaker = on
```

### Late 1980s

The 386 era, first Sound Blaster cards, VGA graphics (1987--1990).

``` ini
[dosbox]
machine = vgaonly

[cpu]
cycles = 3000

[sblaster]
sbtype = sb1
```

### Early 1990s

486-class machines with Sound Blaster Pro or Gravis UltraSound (1990--1993).

``` ini
[dosbox]
machine = vgaonly
memsize = 16

[cpu]
cycles = 30000

[sblaster]
sbtype = sbpro2
```

### Mid-1990s

Pentium-class machines, Sound Blaster 16, SVGA graphics (1993--1996).

``` ini
[dosbox]
machine = svga_s3

[cpu]
cycles = 120000

[sblaster]
sbtype = sb16
```

### Late 1990s

Fast Pentiums, AWE32, high-colour SVGA (1996--1998).

``` ini
[dosbox]
machine = svga_s3

[cpu]
cycles = 400000
memsize = 32

[sblaster]
sbtype = sb16
```
