# A short DOS primer

## The PC is not a console

To use a game console from the 1980s and 90s, you only needed to know three
things: how to hook it up to the TV, how to insert a cartridge, and how to
turn the thing on and off. That's it!

In contrast, IBM PC compatibles running the DOS operating system are much more
complex beasts. While DOSBox Staging vastly simplifies things and takes the
major hurdles out of the equation, you are still expected to have a minimum
understanding of the hardware and the platform to get the most out of it. This
primer intends to give you a very compressed crash-course --- just enough to
be able to run DOS games effectively. While this is enough to get started, we
encourage you to expand your knowledge on the subject over time in the areas
you're interested in.


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

Rather than emulating a specific CPU clock speed, DOSBox Staging uses a "cycles"
system that controls how many instructions are executed per millisecond. This
is analogous to CPU speed --- more cycles means a faster emulated computer.
The default settings work well for the vast majority of games, but some titles
require specific speed ranges to run correctly.


## Video

TODO(CL) brief overview of video adapters and the importance of CRT emulation, 
, calling out tandy as the best overall gaming PC until about 1987


## Audio

TODO(CL) brief overview of the lack of audio until the late 80s, calling out
tandy as the best overall gaming PC until about 1987


## Input peripherals

The standard input peripheral for the IBM PC compatibles was the **keyboard**
for a long time. Eventually, the **mouse** gained popularity, but it wasn't a
standard accessory like on the Commodore Amiga or Apple Macintosh line of
computers.

Many DOS games also support **analog joysticks**. Digital joysticks were a
rarity on the platform.



## Storage

The IBM PC line of computers were initially intended for business use,
therefore **hard drives** were standard accessory. They were optional on the
first IBM PC models, and starting from the XT released in 1983, 10 to 20 MB
hard drives were included as standard equipment. (Yes, you read that right,
that's _megabytes_! Such a hard drive could only store a couple of MP3 files
or high-resolution images...)

The cost of hard drives were initially astronomical, so most home users could
not afford them. This lead to the development of games that could be run from
**floppy disks** in the early days of the PC. There is even a special class of
self-booting floppy games that can be run even without an operating system
present; these are referred to as "PC booter" games.

Eventually, hard drives went down in price so most people could afford them.
As a result of this, the vast majority of DOS games and applications can be
installed easily onto the hard drive.

TODO(CL) mention CD-ROMs as well

This is in stark contrast with the home computers of the era that used
magnetic tapes and floppies for storage due to cost considerations well into
the 1990s.


## Memory

The first line of IBM PC XT computers only supported up to **640 KB of
memory**. Later on, various solutions were invented to circumvent this
limitation and make extra memory available to DOS programs. The maximum amount
of memory DOS can handle is 64 MB, but games rarely could use more than 32 MB.

Frankly, DOS memory management is a complex, confusing, and often frustrating
topic --- no one enjoyed dealing with this stuff back in the day. The good
news is that you don't need to know anything about it to use DOSBox Staging
effectively. The vast majority of games and applications work well with the
default 16 MB of extended memory, and usually there is no advantage to
changing this. 

## MS-DOS

**MS-DOS (Microsoft Disk Operating System)** is the operating system that
gives the platform its name. It provides the basic services needed to run
programs: managing files on disk, loading programs into memory, and handling
input/output to hardware devices.

DOS is a command-line operating system --- there is no graphical desktop. You
interact with it by typing commands at a prompt (e.g., the famous `C:\>`
prompt). Basic navigation involves commands like `DIR` to list files, `CD` to
change directories, and typing a program's name to run it. This is also how
you launch most games: navigate to the game's directory and run its
executable.

DOSBox Staging emulates a DOS-compatible environment, so you don't need to
install MS-DOS separately. The emulated DOS command line works just like the
real thing.


## Windows 3.x

Wait a minute, isn't this supposed to be about DOS?

That's correct, but early Windows versions 1.x to 3.x were not true operating
systems, only operating *environments* implemented as DOS programs. DOSBox
Staging fully supports Windows 3.x, giving you access to the large catalogue
of early CD-ROM games and educational multimedia applications. It also has
partial support for Windows versions 1.x and 2.x.

