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


## CPU



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

Franky, DOS memory managent is a complex, confusing, and often frustrating topic---no one
enjoyed dealing with this stuff back in the day. The good news is that you
don't need to know anything it to use DOSBox effectively.
The vast majority of games and applications work well with the default 16 MB
of extended memory, and usually there is no advantage to changing this.


## MS-DOS



## Windows 3.x

Wait a minute, isn't this supposed to be about DOS?

That's correct, but early Windows versions 1.x to 3.x were not true operating
systems, only operating *environments* implemented as DOS programs. DOSBox fully
supports Windows 3.x, giving you access to the large catalogue of early
CD-ROM games and educational multimedia applications. It also has partial
support for Windows versions 1.x and 2.x.






##

early-80s: 8086 cpu, 120 cycles, pc speaker, 1 mb ram, cga
mid-80s: 8086 cpu, 300 cycles, pc speaker, ibm ps1 audio, ega
late 80s: 386, 3000 cycles, sb1, vgaonly (backward compatible w/ ega)
early 90s demo: 486, 20000 cycles, gus, vgaonly, 8 mb
early 90s gamer, 486, 30000 cycles, sbpro2, vgaonly, 16 mb
mid 90s gamer: pentium, 120000 cycles, sb16 / adlib-gold, svga_s3 2mb, 16 mb
late 90s: pentium, 400000 cycles, svga_s3 8mb. awe32, 32 mb 
