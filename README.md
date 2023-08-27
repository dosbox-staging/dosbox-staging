# DOSBox Staging

![GPL-2.0-or-later][gpl-badge]
[![Chat][discord-badge]][discord]

This repository attempts to modernize the DOSBox codebase by using current
development practices and tools, fixing issues, and adding features that better
support today's systems.

## Build status

[![Linux x86\_64 build status][build-lin1-badge]][build-lin1-ci]
[![Linux other build status][build-lin2-badge]][build-lin2-ci]
[![Windows (VisualStudio) build status][build-win-msvc-badge]][build-win-msvc-ci]
[![Windows (MSYS2) build status][build-win-msys2-badge]][build-win-msys2-ci]
[![macOS build status][build-mac-badge]][build-mac-ci]

## Code quality status

[![Coverity status][coverity-badge]][3]

## Summary of features

### For developers

| **Feature**                    | **Status**
|-                               |-
| **Version control**            | Git
| **Language**                   | C++17
| **SDL**                        | >= 2.0.5
| **Logging**                    | Loguru for C++<sup>[5]</sup>
| **Buildsystem**                | Meson or Visual Studio 2019
| **CI**                         | Yes
| **Static analysis**            | Yes<sup>[1],[3],[4]</sup>
| **Dynamic analysis**           | Yes
| **clang-format**               | Yes
| **[Development builds]**       | Yes
| **Unit tests**                 | Yes<sup>[6]</sup>
| **Automated regression tests** | WIP

[1]: https://github.com/dosbox-staging/dosbox-staging/actions?query=workflow%3A%22Code+analysis%22
[2]: https://lgtm.com/projects/g/dosbox-staging/dosbox-staging/
[3]: https://scan.coverity.com/projects/dosbox-staging
[4]: https://github.com/dosbox-staging/dosbox-staging/actions?query=workflow%3A%22PVS-Studio+analysis%22
[5]: https://github.com/emilk/loguru
[6]: https://github.com/dosbox-staging/dosbox-staging/tree/main/tests
[Development builds]: https://dosbox-staging.github.io/downloads/development-builds/

### For users

| **Feature**                  | **Description**
|-                             |-
| **CD-DA file codecs**        | Opus, OGG/Vorbis, MP3, FLAC, and WAV
| **Integer scaling**          | `integer_scaling = vertical` or `horizontal`; replaced "pixel-perfect" mode<sup>[7]</sup>
| **Resizable window**         | Yes
| **Relative window size**     | `windowresolution = small`, `medium`, or `large` config setting
| **Window placement**         | `windowposition` config setting<sup>[16]</sup>
| **[OPL] emulator**           |  Nuked OPL, a highly accurate (YMF262, CT1747) emulator <sup>[8]</sup>
| **[CGA]/mono support**       | `machine = cga_mono` and `monochrome_palette` config settings<sup>[9]</sup>
| **CGA composite modes**      | For `machine = pcjr`, `tandy`, and `cga`; toggleable via hotkeys
| **[Wayland] support**        | Experimental: use `SDL_VIDEODRIVER=wayland`
| **Modem phonebook file**     | `phonebookfile` config setting
| **Raw mouse input**          | `raw_mouse_input` config setting
| **`AUTOTYPE` command**       | Yes<sup>[10]</sup>
| **`MORE` command**           | Yes<sup>[21]</sup>
| **Startup verbosity**        | Yes<sup>[11]</sup>
| **[GUS] enhancements**       | Yes<sup>[12]</sup>
| **[FluidSynth][FS] MIDI**    | Built-in<sup>[13]</sup>; via FluidSynth 2.x (SoundFonts not included)
| **[MT-32] emulator**         | Built-in; via libmt32emu 2.4.2 (requires user-supplied ROM files)
| **Expanded S3 support**      | 4 and 8 MB of RAM<sup>[14]</sup>
| **Portable & layered conf**  | By default<sup>[15]</sup>
| **Translations handling**    | Bundled, see section 14 in README
| **[ENet] modem transport**   | serialport `sock:1` flag or `SERIAL.COM`<sup>[17]</sup>
| **Ethernet via [slirp]**     | See `[ethernet]` config section
| **IDE support for CD-ROMs**  | See `-ide` flag in `IMGMOUNT.COM /help`
| **Networking in Win3.11**    | Via local shell<sup>[18]</sup>
| **Audio filters**            | See `*_filter` config settings
| **Audio reverb and chorus**  | See `reverb` config setting and `MIXER.COM /help`
| **Audio stereo crossfeed**   | See `chorus` config setting and `MIXER.COM /help`
| **AdLib Gold emulation**     | Via `oplmode = opl3gold`; emulates the surround add-on module too<sup>[19]</sup>
| **Master audio compressor**  | `compressor` config setting<sup>[20]</sup>
| **Dual/multi-mouse input**   | See `[mouse]` config section
| **ReelMagic support**        | See `[reelmagic]` config section

[OPL]: https://en.wikipedia.org/wiki/Yamaha_YMF262
[CGA]: https://en.wikipedia.org/wiki/Color_Graphics_Adapter
[Wayland]: https://en.wikipedia.org/wiki/Wayland_(display_server_protocol)
[GUS]:   https://en.wikipedia.org/wiki/Gravis_Ultrasound
[MT-32]: https://en.wikipedia.org/wiki/Roland_MT-32
[FS]:    https://www.fluidsynth.org/
[ENet]:  https://github.com/zpl-c/enet
[7]:     https://github.com/dosbox-staging/dosbox-staging/issues/2448
[8]:     https://www.vogons.org/viewtopic.php?f=9&t=37782
[9]:     https://github.com/dosbox-staging/dosbox-staging/commit/ffe3c5ab7fb5e28bae78f07ea987904f391a7cf8
[10]:    https://github.com/dosbox-staging/dosbox-staging/commit/239396fec83dbba6a1eb1a0f4461f4a427d2be38
[11]:    https://github.com/dosbox-staging/dosbox-staging/pull/477
[12]:    https://github.com/dosbox-staging/dosbox-staging/wiki/Gravis-UltraSound-Enhancements
[13]:    https://github.com/dosbox-staging/dosbox-staging/issues/262#issuecomment-734719260
[14]:    https://github.com/dosbox-staging/dosbox-staging/pull/1244
[15]:    https://github.com/dosbox-staging/dosbox-staging/blob/972ad1f7016648b4557113264022176770878726/README#L422
[16]:    https://github.com/dosbox-staging/dosbox-staging/pull/1272
[17]:    https://github.com/dosbox-staging/dosbox-staging/pull/1398
[18]:    https://github.com/dosbox-staging/dosbox-staging/pull/1447
[19]:    https://github.com/dosbox-staging/dosbox-staging/pull/1715
[20]:    https://github.com/dosbox-staging/dosbox-staging/pull/1831
[21]:    https://github.com/dosbox-staging/dosbox-staging/pull/2020

## Stable release builds

[Linux](https://dosbox-staging.github.io/downloads/linux/),
[Windows](https://dosbox-staging.github.io/downloads/windows/),
[macOS](https://dosbox-staging.github.io/downloads/macos/)

## Test builds / development snapshots

[Development builds].

## Get the source

- Clone the repository (one-time step):

    ``` shell
    git clone https://github.com/dosbox-staging/dosbox-staging.git
    ```

## Build instructions

Read [BUILD.md] for the comprehensive compilation guide.

DOSBox Staging has the following library dependencies:

| Package (libname)                                                | Min Version | Provides feature                               | Presence  | Meson-wrap | VCPKG | repo availability |
|------------------------------------------------------------------|-------------|------------------------------------------------|-----------|------------|-------|-------------------|
| [FluidSynth](https://www.fluidsynth.org/) (fluidsynth)           | 2.2.3       | General MIDI playback                          | Optional  | yes        | yes   | common            |
| [Google Test+Mock](https://github.com/google/googletest) (gmock) | 1.8.0       | Framework for unit testing (development)       | Optional  | yes        | yes   | common            |
| [IIR](https://github.com/berndporr/iir1) (iir1)                  | 1.9.3       | Audio filtering                                | Mandatory | yes        | yes   | rare              |
| [Munt](https://github.com/munt/munt) (libmt32emu)                | 2.5.3       | Roland MT-32 and CM-32L playback               | Optional  | yes        | yes   | rare              |
| [libpng](http://www.libpng.org/pub/png/libpng.html) (libpng)     | n/a         | PNG-encoding of screen captures                | Optional  | yes        | yes   | very common       |
| [Opus File](https://opus-codec.org/) (opusfile)                  | n/a         | CDDA playback for Opus-encoded track files     | Mandatory | yes        | yes   | common            |
| [SDL 2.0](https://github.com/libsdl-org/SDL) (sdl2)              | 2.0.5       | OS-agnostic API for video, audio, and eventing | Mandatory | yes        | yes   | common            |
| [SDL_net 2.0](https://github.com/libsdl-org/SDL_net) (sdl2-net)  | 2.0.0       | Network API for emulated serial and IPX        | Optional  | yes        | yes   | common            |
| [slirp](https://gitlab.freedesktop.org/slirp) (libslirp)         | 4.6.1       | Unprivileged virtual TCP/IP stack for Ethernet | Optional  | yes        | yes   | less-common       |
| [SpeexDSP](https://github.com/xiph/speexdsp) (speexdsp)          | n/a         | Audio resampling                               | Mandatory | yes        | yes   | common            |
| [Tracy Profiler](https://github.com/wolfpld/tracy) (tracy)       | n/a         | Event profile (development)                    | Optional  | yes        | yes   | rare              |
| [Zlib](https://z-lib.org/) (zlib)                                | 1.2.11      | ZMBV video capture                             | Optional  | yes        | yes   | very common       |

### Linux, macOS

Install build dependencies appropriate for your OS:

``` shell
# Fedora
sudo dnf install ccache gcc-c++ meson alsa-lib-devel libatomic libpng-devel \
                 SDL2-devel SDL2_net-devel opusfile-devel \
                 fluidsynth-devel iir1-devel mt32emu-devel libslirp-devel \
                 speexdsp-devel libXi-devel
```

``` shell
# Debian, Ubuntu
sudo apt install ccache build-essential libasound2-dev libatomic1 libpng-dev \
                 libsdl2-dev libsdl2-image-dev libsdl2-net-dev libopusfile-dev \
                 libfluidsynth-dev libslirp-dev libspeexdsp-dev libxi-dev

# Install Meson on Debian-10 "Buster" or Ubuntu-20.04 and older
sudo apt install python3-setuptools python3-pip
sudo pip3 install --upgrade meson ninja

# Install Meson on Debian-11 "Bullseye" or Ubuntu-21.04 and newer
sudo apt install meson
```

``` shell
# Arch, Manjaro
sudo pacman -S ccache gcc meson alsa-lib libpng sdl2 sdl2_image sdl2_net \
               opusfile fluidsynth libslirp speexdsp libxi pkgconf
```

``` shell
# openSUSE
sudo zypper install ccache gcc gcc-c++ meson alsa-devel libatomic1 libpng-devel \
                    libSDL2-devel libSDL2_net-devel \
                    opusfile-devel fluidsynth-devel libmt32emu-devel libslirp-devel \
                    speexdsp libXi-devel
```

``` shell
# Void Linux
sudo xbps-install -S SDL2-devel SDL2_net-devel alsa-lib-devel \
                     fluidsynth-devel libiir1-devel libmt32emu-devel \
                     libpng-devel libslirp-devel opusfile-devel \
                     speexdsp-devel libatomic-devel libXi-devel
```

``` shell
# NixOS 
# With Home Manager on home.nix (Recommended Permanent Installation)
home.packages = [ pkg-config gcc_multi cmake ccache SDL2 SDL2_net \ 
                  fluidsynth glib gtest libGL libGLU libjack2 libmt32emu libogg \ 
                  libpng libpulseaudio libslirp libsndfile meson ninja opusfile \
                  libselinux speexdsp stdenv alsa-lib xorg.libXi irr1 ]

# Note: the same package list will work with environment.systemPackages 
# on configuration.nix
```

``` shell
# macOS
xcode-select --install
brew install cmake ccache meson libpng sdl2 sdl2_image sdl2_net opusfile \
     fluid-synth libslirp pkg-config python3 speexdsp
```

### Build and stay up-to-date with the latest sources

- Checkout the main branch:

    ``` shell
    # commit or stash any personal code changes
    git checkout main -f
    ```

- Pull the latest updates. This is necessary every time you want a new build:

    ``` shell
    git pull
    ```

- Setup the build. This is a one-time step either after cloning the repo or
    cleaning your working directories:

    ``` shell
    meson setup build
    ```

    The above enables all of DOSBox Staging's functional features. If you're
    interested in seeing all of Meson's setup options, run: `meson configure`.

- Compile the sources. This is necessary every time you want a new build:

    ``` shell
    meson compile -C build
    ```

    Your binary is: `build/dosbox`

    The binary depends on local resources relative to it, so we suggest
    symlinking to the binary from your PATH, such as into ~/.local/bin/
    -- Have fun!

### Windows - Visual Studio (2019 or newer)

First, you need to setup [vcpkg] to install build dependencies. Once vcpkg
is bootstrapped, open PowerShell and run:

``` powershell
PS:\> .\vcpkg integrate install
```

This step will ensure that MSVC can use vcpkg to build, find and links all
dependencies.

Start Visual Studio and open file: `vs\dosbox.sln`. Make sure you have `x64`
selected as the solution platform.  Use Ctrl+Shift+B to build all projects.

Note, the first time you build a configuration, dependencies will be built
automatically and stored in the `vcpkg_installed` directory. This can take
a significant length of time.

[vcpkg]: https://github.com/microsoft/vcpkg

### Windows (MSYS2), macOS (MacPorts), Haiku, Nix0S, others

Instructions for other build systems and operating systems are documented
in [BUILD.md]. Links to OS-specific instructions: [MSYS2], [MacPorts],
[Haiku], [NixOS].

[BUILD.md]: BUILD.md
[MSYS2]:    docs/build-windows.md
[MacPorts]: docs/build-macos.md
[Haiku]:    docs/build-haiku.md
[NixOS]:    docs/build-nix.md

## Imported branches, community patches, old forks

Upstream commits are imported to this repo in a timely manner,
see branch [`svn/trunk`].

- [`svn/*`] - branches from SVN
- [`forks/*`] - code for various abandoned DOSBox forks
- [`vogons/*`] - community patches posted on the Vogons forum

Git tags matching pattern `svn/*` are pointing to the commits referenced by SVN
"tag" paths at the time of creation.

Additionally, we attach some optional metadata to the commits in the form of
[Git notes][git-notes]. To fetch them, run:

``` shell
git fetch origin "refs/notes/*:refs/notes/*"
```

[`svn/*`]:     https://github.com/dosbox-staging/dosbox-staging/branches/all?utf8=%E2%9C%93&query=svn%2F
[`svn/trunk`]: https://github.com/dosbox-staging/dosbox-staging/tree/svn/trunk
[`vogons/*`]:  https://github.com/dosbox-staging/dosbox-staging/branches/all?utf8=%E2%9C%93&query=vogons%2F
[`forks/*`]:   https://github.com/dosbox-staging/dosbox-staging/branches/all?utf8=%E2%9C%93&query=forks%2F
[git-notes]:   https://git-scm.com/docs/git-notes

[gpl-badge]:     https://img.shields.io/badge/license-GPL--2.0--or--later-blue
[discord-badge]: https://img.shields.io/discord/514567252864008206?color=%237289da&logo=discord&logoColor=white&label=discord
[discord]:       https://discord.gg/WwAg3Xf

[build-lin1-badge]: https://img.shields.io/github/actions/workflow/status/dosbox-staging/dosbox-staging/linux.yml?label=Linux%20%28x86_64%29
[build-lin1-ci]:    https://github.com/dosbox-staging/dosbox-staging/actions/workflows/linux.yml?query=branch%3Amain

[build-lin2-badge]: https://img.shields.io/github/actions/workflow/status/dosbox-staging/dosbox-staging/platforms.yml?label=Linux%20%28other%29
[build-lin2-ci]:    https://github.com/dosbox-staging/dosbox-staging/actions/workflows/platforms.yml?query=branch%3Amain

[build-win-msys2-badge]: https://img.shields.io/github/actions/workflow/status/dosbox-staging/dosbox-staging/windows-msys2.yml?label=Windows%20%28MSYS2%29
[build-win-msys2-ci]:    https://github.com/dosbox-staging/dosbox-staging/actions/workflows/windows-msys2.yml?query=branch%3Amain

[build-win-msvc-badge]: https://img.shields.io/github/actions/workflow/status/dosbox-staging/dosbox-staging/windows-msvc.yml?label=Windows%20%28Visual%20Studio%29
[build-win-msvc-ci]:    https://github.com/dosbox-staging/dosbox-staging/actions/workflows/windows-msvc.yml?query=branch%3Amain

[build-mac-badge]: https://img.shields.io/github/actions/workflow/status/dosbox-staging/dosbox-staging/macos.yml?label=macOS%20%28x86_64%2C%20arm64%29
[build-mac-ci]:    https://github.com/dosbox-staging/dosbox-staging/actions/workflows/macos.yml?query=branch%3Amain

[coverity-badge]: https://img.shields.io/coverity/scan/dosbox-staging


## Website & documentation

Please refer to the [documentation guide](DOCUMENTATION.md) before making
changes to the website or the documentation.

