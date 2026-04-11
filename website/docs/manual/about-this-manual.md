# About this manual

Welcome to the **DOSBox Staging user manual** --- your complete guide to
getting the most out of the emulator. It covers everything from basic concepts
to the detailed configuration of every emulated device. You don't need to read
it cover to cover --- most people dip in when they need to configure something
specific or understand how a feature works.


## Start with the Getting Started guide

If you're new to DOSBox Staging --- or even if you've used other DOSBox
versions before --- the [Getting Started guide](../getting-started/index.md)
is the single best place to begin. It walks you through setting up several
games from scratch, teaching the core concepts you'll use every day: mounting
drives, configuring sound and graphics, and tweaking settings per game.

The guide is hands-on and assumes no prior DOS knowledge. Later chapters build
on earlier ones, so work through them in order rather than skipping ahead.
Even experienced users tend to pick up useful techniques from it.


## What's in this manual

### Introduction

The section you're reading now. It gives you the foundation for everything
else that follows.

- [What is DOSBox Staging?](what-is-dosbox-staging.md) --- What DOSBox Staging
  is, how it differs from full PC emulators, and a brief history of the project.

- [A short DOS primer](dos-primer.md) --- Key concepts about how DOS works:
  the command prompt, drive letters, configuration files. Read this if you've
  never used DOS before.

- [The DOS eras](dos-eras.md) --- A timeline of PC hardware from 1981 to
  1998, with recommended DOSBox settings for each period. Useful for matching
  the emulated hardware to a game's era.


### Using DOSBox Staging

Practical information about operating the emulator day-to-day.

- [Configuration files](configuration.md) --- How DOSBox Staging's layered
  configuration system works: global settings, per-game configs, and the
  autoexec section.

- [Command-line usage](command-line.md) --- Flags and options for launching
  DOSBox Staging from a terminal.

- [DOS commands](commands.md) --- Every command available at the DOS prompt,
  from MOUNT and CONFIG to standard DOS commands like DIR and COPY.

- [Storage](storage.md) --- How DOS uses drive letters, mounting directories
  and disk images, floppy/CD-ROM/hard disk handling.

- [Hardware resources](hardware-resources.md) --- I/O addresses, IRQs, and
  DMA channels --- what they are and how to configure them when a game's
  setup utility asks.

- [Capture](capture.md) --- Screenshots, audio recording, video recording,
  and MIDI capture.

- [HTTP API](http-api.md) --- The built-in HTTP API for remote
  control.

- [Running Windows 3.1](windows-31.md) --- Installing and configuring
  Windows 3.1 inside DOSBox Staging, including video, audio, MIDI, and mouse
  driver setup.


### System, graphics, sound, input, networking

These sections document the emulated hardware and its configuration in
detail. Each typically opens with a conversational overview explaining the
hardware and when you'd want to change the defaults, followed by the
detailed configuration reference.

- **[System](system/general.md)** --- Core emulator settings: machine type,
  CPU speed and type, memory, disk speed, and DOS shell options.

- **[Graphics](graphics/adapters.md)** --- Video adapters (CGA through SVGA),
  display settings, CRT shaders, composite video, 3dfx Voodoo, and
  ReelMagic.

- **[Sound](sound/overview.md)** --- All emulated audio devices from the PC
  speaker to the Roland MT-32 and Sound Canvas, plus the mixer, effects, and
  output filters.

- **[Input](input/overview.md)** --- Keyboard, mouse, joystick configuration,
  and the key mapper for remapping controls.

- **[Networking](networking/serial-ports.md)** --- Serial ports, IPX
  networking, and Ethernet emulation.


### Appendices

- [Keyboard shortcuts](appendices/shortcuts.md) --- Quick reference for all
  default hotkeys.

- [Keymapper reference](appendices/keymapper.md) --- Table of every mappable
  action and its default binding.


## Getting help for specific commands

Every DOSBox command has built-in help.

Run `COMMAND /?` at the DOS prompt for detailed usage (for example, `MOUNT
/?`).

You can also run `HELP` to see the most common commands, or `HELP /ALL` for
the complete list.


## If you're not sure where to look

The search bar in the top right corner of this page works well for finding
specific settings or topics.

If you know the name of a configuration setting (like
`cpu_cycles` or `sbtype`), searching for it will take you directly to its
documentation.
