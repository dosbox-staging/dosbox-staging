---
hide:
  - footer
---

# About

[![DOSBox Staging](../assets/images/dosbox-staging-no-border.svg){ align=right width=33% .about-logo }](https://www.dosbox-staging.org/)

**DOSBox Staging** is a modern continuation of DOSBox with advanced features
and current development practices.

It is a (mostly) drop-in replacement for older DOSBox versions---your
existing configurations will continue to work, and you will have access to
many advanced features.

**DOSBox Staging** comes with sensible defaults, so you'll need to write
a lot less configuration than with older DOSBox versions. Most games and
applications require no tweaking and will work fine with the stock
settings. However, the extensive configuration options and advanced features
are available if you wish to delve deeper. Please refer to the [Feature
highlights](../index.md#feature-highlights) on our front page to learn more
about these.

The key features for developers are summarised [here](https://github.com/dosbox-staging/dosbox-staging?tab=readme-ov-file#key-features-for-developers).


## Goals

- Faithfully emulate the **DOS operating environment** running on IBM PC
  compatibles and the IBM PCjr, with the primary goal of running **all PC
  Booter, DOS and Windows 3.x games** released in the 1981--2000 period. Running
  applications and demoscene productions or more recent DOS software is a
  secondary objective.
- Improve the **out-of-the-box experience**.
- Provide a **self-contained emulator** that requires no extra legacy or "modern
  retro" hardware to function.
- Focus on supporting up-to-date, **current operating systems** and **modern
 hardware**.
- Implement **new features** and **quality-of-life** improvements.
- Deliver a consistent **cross-platform experience**.
- Leverage **upstream** and **community developments** in DOSBox.
- Encourage **new contributors** by removing barriers to entry.
- Prioritise **code quality** to minimise technical
  debt and ease maintenance. This generally means following the
  [Staging Coding Style Guide](https://github.com/dosbox-staging/dosbox-staging/blob/main/CONTRIBUTING.md#coding-style)
  and best practices such as the [C++ Core Guidelines](http://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines).


## Non-goals

- **Support old, end-of-life operating systems** (e.g., Windows 7 or older, OS/2,
  Mac OS X 10.5 or older) and limited CPU/memory hardware, which are
  constraints the original [DOSBox](https://www.dosbox.com/) continues to
  support.

- **Support legacy or "modern retro" hardware.** DOSBox Staging is a
  self-contained emulator; its aim is to emulate all PC hardware it supports in
  software. Legacy and retro hardware are not supported (e.g., ISA boards, CRT monitors, RetroWave OPL3 and similar devices, etc.).

- **Support the use of Windows 9x/Me in the emulator.** Windows 9x/Me emulation
  is supported by projects such as [QEMU](https://www.qemu.org),
  [VirtualBox](https://www.virtualbox.org/) and the
  [DOSBox-X](https://www.dosbox-x.com/) and
  [DOSBox Pure](https://github.com/schellingb/dosbox-pure) forks.

- **Pursue hardware accuracy above all else.** If you’re after a more faithful
  emulation of an entire PC, look into
  [MartyPC](https://github.com/dbalsom/martypc),
  [PCem](https://pcem-emulator.co.uk/), [86Box](https://86box.net),
  [PCBox](https://pcbox.github.io/), [QEMU](https://www.qemu.org/)
  or [VirtualBox](https://www.virtualbox.org/)
  (although DOSBox Staging often matches or surpasses the graphics and
  especially audio emulation fidelity of these other emulators).

- **Be the fastest DOS emulator on x86 hardware.** Linux users interested in
  emulation speed should look at [dosemu2](https://github.com/dosemu2/dosemu2).

- **Act as a general-purpose DOS operating system.** For that, there is
  [FreeDOS](https://www.freedos.org/).


## Relationship to the original DOSBox project

**DOSBox Staging** is separate from, and not supported by, the
SourceForge-hosted [DOSBox](https://www.dosbox.com/) project, or its
development team, the DOSBox Team.

We acknowledge and are thankful for the work shared by all DOSBox
contributors.


## Team

This project is maintained by the **DOSBox Staging team**.


## License

<div>
  <a rel="license" href="http://creativecommons.org/licenses/by-sa/4.0/" style="text-decoration: none">
    <img alt="Creative Commons License" style="width: 1.7rem; display: inline-block;" src="https://mirrors.creativecommons.org/presskit/icons/cc.svg">
    <img alt="Creative Commons License" style="width: 1.7rem; display: inline-block;" src="https://mirrors.creativecommons.org/presskit/icons/by.svg">
    <img alt="Creative Commons License" style="width: 1.7rem; display: inline-block;" src="https://mirrors.creativecommons.org/presskit/icons/sa.svg">
  </a>
</div>

Content on this site is licensed under a
[Creative Commons Attribution-ShareAlike 4.0 International License](https://creativecommons.org/licenses/by-sa/4.0/).

DOSBox Staging is licensed under a [GNU GPL version 2 or later](https://www.gnu.org/licenses/old-licenses/gpl-2.0.html).


## Data privacy policy

DOSBox Staging never collects any personal information about you or your
computer, and it never connects to the Internet or any network without you
asking it to do so in the first place (e.g., by running BBS software or
playing multiplayer DOS games over Ethernet).


## Disclaimer

!!! warning "Free for personal use, but no warranties!"

    Although we do our best to emulate the DOS environment and legacy IBM PC
    hardware as accurately as we can, **we cannot guarantee** DOSBox Staging has
    zero bugs or can run every single DOS software ever written 100%
    correctly.

    **Under no circumstances** should DOSBox Staging be used for professional
    applications, especially where DOS software malfunctioning due to
    emulation bugs or inaccuracies could result in significant financial loss,
    data loss, or putting living beings at risk.

    Neither the members of the DOSBox Staging team nor our contributors can be
    held responsible for such unfortunate accidents resulting from the misuse
    of our software. DOSBox Staging is intended for **personal use only** in
    low-stakes scenarios, such as playing DOS games, watching demoscene
    productions, or researching the history of IBM PC compatibles and the DOS
    software catalogue.

    If you disregard this and get into trouble, **you're on your own!**


!!! danger "IEEE 754 80-bit extended precision floating point emulation"

    One particularly risky area is engineering software that requires accurate
    80-bit extended precision x87 FPU emulation to function correctly. Support
    for 80-bit floats is not available on all platforms that DOSBox Staging
    runs on. The logs will warn you about this at startup:

        FPU: Using reduced-precision floating-point

    Do note, however, that the lack of such log messages **does not** imply or
    guarantee bug-free operation!
