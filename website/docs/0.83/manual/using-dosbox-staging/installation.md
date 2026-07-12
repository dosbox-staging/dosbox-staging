# Installation

DOSBox Staging is available for  [Windows](#windows), [macOS](#macos), and
[Linux](#linux). On every platform it's a quick, self-contained install ---
you're just adding a standalone emulator, not modifying anything already on
your system. Follow the instructions for your operating system below.

!!! note "GOG and Steam DOS games"

    If you already play DOS games through **GOG.com** or **Steam**, installing
    DOSBox Staging is completely safe and won't interfere with them. Each of those
    games ships with its own private copy of DOSBox inside the game's folder and
    launches that copy directly.

    DOSBox Staging installs separately in its own location and never touches
    or overrides those bundled copies, so your existing GOG and Steam games
    will keep working exactly as before. Of course, you can point a game at
    DOSBox Staging later for better compatibility, but that's a deliberate
    opt-in --- nothing about the installation does it for you.

## Windows

The latest release of DOSBox Staging is compatible with any Windows laptop or
desktop running **64-bit Windows 8, 10 or 11**. A dedicated graphics adapter
is recommended for laptops.

We recommend downloading the [latest stable
version](../../../releases/windows.md#current-stable-version) from our
website. Both installer and portable ZIP package variants are available.

If you're new to DOSBox Staging, we recommend the installer and accepting the
default install options --- that will give you an **Open with DOSBox Staging**
context menu item in Windows Explorer on Windows 8 and 10 to start the
emulator from a given folder more easily.

Make sure to read the section about dealing with [Microsoft
Defender](starting.md#windows-defender) before launching DOSBox Staging for
the first time.


!!! warning "Windows 11 note"

    The Windows Explorer context menu integration only works in
    Windows 8 and 10 currently.

!!! note "Legacy Windows support"

    If you're still on a legacy Windows version, you can try one of our [older
    unsupported
    releases](../../../releases/windows.md#legacy-windows-support). Note that
    these older versions are provided as-is --- we don't provide any support
    for them. Also note they don't come with a manual; many things from the
    current manual and [Getting Started
    guide](../../getting-started/introduction.md) are not applicable to older
    releases.

!!! tip "Tip for power users"

    The installer is built with [Inno Setup](https://jrsoftware.org/isinfo.php). For the available
    command-line install parameters, please see [Inno's documentation page](https://jrsoftware.org/ishelp/index.php?topic=setupcmdline).


## macOS

The latest version of DOSBox Staging requires **macOS 11 (Big Sur)** or later,
and supports both Intel and Apple silicon Macs.

Download the [latest stable
version](../../../releases/macos.md#current-stable-version) from our macOS
releases page, then simply drag the DOSBox Staging icon into your Applications
folder.

!!! note "Legacy macOS support"

    If you're still on a legacy macOS version, you can try one of our [older
    unsupported releases](../../../releases/macos.md#older-releases). Note
    that these older versions are provided as-is --- we don't provide any
    support for them. Also note they don't come with a manual; many things
    from the current manual and [Getting
    Started](../../getting-started/introduction.md) guide are not applicable
    to older releases.


## Linux

The latest DOSBox Staging release is compatible with any laptop or desktop
released after 2010 running a modern x86_64 Linux distribution. A dedicated
graphics adapter is recommended for laptops.

Our official build runs on most desktop Linux distributions (x86_64 only for
now). It only depends on the C/C++, ALSA, and OpenGL system libraries; all
other libraries are statically linked.

Please run the `install-icons.sh` script included with the release to install
the application icons.

!!! warning "Unofficial repackaged releases"

    We do not recommend using 3rd party repackagings of DOSBox Staging by
    external teams (i.e., distribution-specific repository packages,
    containerised images, etc.) The core DOSBox Staging team has no time and
    energy to support all these unofficial packages.

    If you still decide to use them and run into problems, please ask the
    person or team who did the repackaging, _not_ the core DOSBox Staging
    team. Read [Unofficial repackaged
    releases](../../../releases/linux.md#unofficial-repackaged-releases) to
    learn more.
