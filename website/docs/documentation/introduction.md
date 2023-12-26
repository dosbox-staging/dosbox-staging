# Introduction

## What is DOSBox Staging?

**DOSBox Staging** is a tool that allows you to run software written for the
MS-DOS operating system and the Windows 3.1x operating environment on modern
computers. The emulation for MS-DOS is built in; you don't need to install the
DOS operating system itself.

Although DOSBox was initially intended for running DOS games only, now it has
good compatibility with application programs too. It gives you access to the
vast catalogue of DOS software from the early 1980s IBM PC XT
period to the Pentium era up until the late 90s. It can also emulate the
popular-in-its-day Tandy 1000 PC clone, the short-lived IBM PCjr machine, and
it can even load early self-booting "PC booter" games.

A wide range of IBM PC compatible hardware can be emulated to a high degree of
accuracy, including Hercules, CGA, EGA, VGA and SVGA display adapters; audio
subsystems such as the PC speaker, Creative Music System, various AdLib and
Sound Blaster models, the ultra-rare AdLib Gold 1000 (including the optional
surround module), Gravis UltraSound, Tandy 1000 audio, Roland MT-32, and
General MIDI among a few others.

Just imagine how expensive and impractical it would be to collect all this
hardware and keep them in working order! Besides the convenience factor, old
hardware isn't getting any younger, and how long until all old electronics
fade into oblivion? At that point, all we're left with will be emulation.
Therefore, projects like DOSBox are crucial in preserving a significant period
of our collective digital history, keeping the MS-DOS legacy alive to ensure
future generations can study, enjoy, and learn from all the great games and
applications of the past.


## Is it an emulator?

Not quite. "True" computer emulators are built to mimic the entire target
system as closely as possible at the hardware level---ideally, the emulated
system should be indistinguishable from the real thing. When using such an
emulator, you'd need to install an operating system of your choosing onto the
emulated hardware before running games and applications. Naturally, you
wouldn't be restricted to just MS-DOS; you could install any operating system
supported by the hardware you're emulating, such as OS/2, BeOS or 386BSD in
case of an IBM PC compatible machine.

DOSBox, on the other hand, aims to emulate *only* the MS-DOS operating system,
plus a range of IBM PC compatible hardware components to allow the running of
DOS software. The minimum goal is that supported software should run *at
least* as well as on a real machine, but DOSBox can do better and offer an
improved experience in many cases. It emphasizes the faithful emulation of
computer subsystems critical to gaming (audio and video cards, in particular)
while taking a more relaxed approach where ultimate fidelity does not matter
as much (e.g., emulating hard drive storage). Ultimately, this hybrid approach
leads to a more user-friendly MS-DOS experience and an increased emulator
performance.

To elaborate on the differences between the two approaches, let's delve
into a bit more detail!


### Complete computer emulation

To play a DOS game in an IBM PC compatible emulator such as
[PCem](https://pcem-emulator.co.uk/) or [86Box](https://86box.net/), you would
need to start by selecting a motherboard, CPU, graphics card, sound card, and
hard drive. After this, you would need to format the hard drive, install MS-DOS
from virtual floppy images, set up video and sound card drivers, and
only then could you finally proceed to installing and playing the game.

Naturally, this being a complete computer emulator, every time you want to
play the game, you would need to turn on the emulated machine, wait until it
performs its power-on self-test and boots into MS-DOS, and then you could
launch the game. If you want to switch to a different CPU or sound card later,
you must redo some of these setup steps. In the end, you might end up with
many virtual machines for all the different hardware combinations you want to
use.


### The DOSBox way

With DOSBox, it's a lot simpler: in most cases, you'll only need to copy the
game files into a folder and possibly write a few lines of configuration. No
booting time is involved; the game will start immediately, not "knowing" any
better if it's running inside DOSBox, a complete computer emulator, or on
period-accurate hardware from the 1980s or 90s!

Need a different sound card? Just change a line or two in the config---DOSBox
will take care of the rest.

Is the processor too fast or slow? Change a single config value, or better yet,
adjust the speed while playing the game. 

DOSBox makes it easy to experiment with various settings---which is important,
as most DOS games offer a variety of sound and graphics options well worth
playing with. Due to its simplicity, it makes it easy to maintain an extensive
collection of games, each fine-tuned for the best possible experience,
according to your preferences. 


## A bit of history

The story of DOSBox began around the year 2000. Before Windows 2000 and XP,
earlier versions of Windows were based on MS-DOS, naturally leading to
excellent compatibility with DOS software. Windows 2000 (released at the end
of 1999) had dropped most of this support, keeping only a rudimentary
compatibility layer that was not enough to run many earlier DOS applications
satisfactorily, in particular games. Two Dutch programmers, **Peter "Qbix"
Veenstra** and **Sjoerd "Harekiet" van der Berg**, took it upon themselves to
remedy this precarious situation with the original goal of bringing DOS gaming
back to Windows. The fruit of their efforts, **DOSBox v0.1** (the first public
version), was released on 31 January 2002.

In the years to come, DOSBox became the de facto tool to run MS-DOS games
on various modern platforms. Virtually all commercial re-releases of old
games on online gaming marketplaces use DOSBox under the hood.

From 2010 onwards, development slowed down, leading to a proliferation of
custom builds, forks, and spin-off projects. Some had been more successful and
longer-lived than others, but ultimately this led to the fragmentation of the
community and a lack of direction regarding where the project was going.

**DOSBox Staging** was started in 2020 by **Patryk "dreamer" Obara** and
**kcgen** as a modern continuation of the original DOSBox 
to unify these
efforts, bring modern development practices to the project with long-term
maintainability in mind, and revitalise DOSBox development in general.
Subsequently, the project got the blessing of original DOSBox author **Sjoerd
"Harekiet" van der Berg**, and if you're reading this, it means it's doing
rather well! :sunglasses:

