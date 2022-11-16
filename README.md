# DOSBox Staging

![GPL-2.0-or-later][gpl-badge]
[![Chat][discord-badge]][discord]

This repository attempts to modernize the DOSBox codebase by using current
development practices and tools, fixing issues, and adding features that better
support today's systems.

### Build status

[![Linux x86\_64 build status][build-lin1-badge]][build-linux]
[![Linux other build status][build-lin2-badge]][build-linux-2]
[![Windows build status][build-win-badge]][build-win]
[![macOS build status][build-mac-badge]][build-mac]

### Code quality status

[![Coverity status][coverity-badge]][4]
[![LGTM grade][lgtm-badge]][3]

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
| **Static analysis**            | Yes<sup>[1],[2],[3],[4]</sup>
| **Dynamic analysis**           | Yes
| **clang-format**               | Yes
| **[Development builds]**       | Yes
| **Unit tests**                 | Yes<sup>[6]</sup>
| **Automated regression tests** | WIP

[1]:https://github.com/dosbox-staging/dosbox-staging/actions?query=workflow%3A%22Code+analysis%22
[2]:https://lgtm.com/projects/g/dosbox-staging/dosbox-staging/
[3]:https://scan.coverity.com/projects/dosbox-staging
[4]:https://github.com/dosbox-staging/dosbox-staging/actions?query=workflow%3A%22PVS-Studio+analysis%22
[5]: https://github.com/emilk/loguru
[6]:tests/README.md
[Development builds]:https://dosbox-staging.github.io/downloads/devel/

### For users

| **Feature**                 | **Status**
|-                            |-
| **CD-DA file codecs**       | Yes: Opus, OGG/Vorbus, MP3, FLAC, and WAV
| **Pixel-perfect mode**      | Yes: `output=openglpp` or `output=texturepp`
| **Resizable window**        | Yes, for all `output=opengl` modes
| **Relative window size**    | `windowresolution=small`, `medium`, or `large`
| **Window placement**        | `windowposition = 0,0`, and more<sup>[16]</sup>
| **[OPL] emulator**          |  Nuked OPL, a highly accurate (YMF262, CT1747) emulator <sup>[8]</sup>
| **[CGA]/mono support**      | `machine=cga_mono`<sup>[9]</sup>
| **CGA composite modes**     | `machine=pcjr/tandy/cga` with hotkeys)
| **[Wayland] support**       | Experimental: use `SDL_VIDEODRIVER=wayland`
| **Modem phonebook file**    | `phonebookfile=<name>`
| **Autotype command**        | Yes<sup>[10]</sup>
| **Startup verbosity**       | Yes<sup>[11]</sup>
| **[GUS] enhancements**      | Yes<sup>[12]</sup>
| **Raw mouse input**         | Yes: `raw_mouse_input=true`
| **[FluidSynth][FS] MIDI**   | Yes<sup>[13]</sup>: FluidSynth 2.x
| **[MT-32] emulator**        | Yes: libmt32emu 2.4.2 (Requires ROM files)
| **Expanded S3 support**     | 4 and 8 MiB of RAM<sup>[14]</sup>
| **Portable & layered conf** | By default<sup>[15]</sup>
| **Translations handling**   | Bundled, see section 14 in README
| **[ENet] modem transport**  | Yes: serialport `sock:1` flag or `SERIAL.COM`<sup>[17]</sup>
| **Ethernet via [slirp]**    | Yes: See `[ethernet]` section in conf file
| **IDE support for CDROMs**  | Yes: See `-ide` flag in `IMGMOUNT.COM /help`
| **Networking in Win3.11**   | Yes: Via local shell<sup>[18]</sup>

[OPL]: https://en.wikipedia.org/wiki/Yamaha_YMF262
[CGA]: https://en.wikipedia.org/wiki/Color_Graphics_Adapter
[Wayland]: https://en.wikipedia.org/wiki/Wayland_(display_server_protocol)
[GUS]:   https://en.wikipedia.org/wiki/Gravis_Ultrasound
[MT-32]: https://en.wikipedia.org/wiki/Roland_MT-32
[FS]:    http://www.fluidsynth.org/
[ENet]:  https://github.com/zpl-c/enet
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

## Stable release builds

[Linux](https://dosbox-staging.github.io/downloads/linux/),
[Windows](https://dosbox-staging.github.io/downloads/windows/),
[macOS](https://dosbox-staging.github.io/downloads/macos/)

## Test builds / development snapshots

[Links to the newest builds](https://dosbox-staging.github.io/downloads/devel/)

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
| [SDL_image 2.0](https://github.com/libsdl-org/SDL_image) (sdl2-image)  | 2.0.x | Screenshot of rendered output to file          | Optional  | yes        | yes   | common            |
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
                 SDL2-devel SDL2_image-devel SDL2_net-devel opusfile-devel \
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
               opusfile fluidsynth libslirp speexdsp libxi
```

``` shell
# openSUSE
sudo zypper install ccache gcc gcc-c++ meson alsa-devel libatomic1 libpng-devel \
                    libSDL2-devel libSDL2_image-devel libSDL2_net-devel \
                    opusfile-devel fluidsynth-devel libmt32emu-devel libslirp-devel \
                    speexdsp libXi-devel
```

``` shell
# Void Linux
sudo xbps-install -S SDL2-devel SDL2_image-devel SDL2_net-devel alsa-lib-devel \
                     fluidsynth-devel libiir1-devel libmt32emu-devel \
                     libpng-devel libslirp-devel opusfile-devel \
                     speexdsp-devel libatomic-devel libXi-devel
```

``` shell
# macOS
xcode-select --install
brew install ccache meson libpng sdl2 sdl2_image sdl2_net opusfile fluid-synth libslirp speexdsp
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

### Windows (MSYS2), macOS (MacPorts), Haiku, others

Instructions for other build systems and operating systems are documented
in [BUILD.md]. Links to OS-specific instructions: [MSYS2], [MacPorts],
[Haiku].

[BUILD.md]: BUILD.md
[MSYS2]:    docs/build-windows.md
[MacPorts]: docs/build-macos.md
[Haiku]:    docs/build-haiku.md

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

For some historical context of why this repo exists you can read
[Vogons thread](https://www.vogons.org/viewtopic.php?p=790065#p790065),
([1](https://imgur.com/a/bnJEZcx), [2](https://imgur.com/a/HnG1Ls4))

[`svn/*`]:     https://github.com/dosbox-staging/dosbox-staging/branches/all?utf8=%E2%9C%93&query=svn%2F
[`svn/trunk`]: https://github.com/dosbox-staging/dosbox-staging/tree/svn/trunk
[`vogons/*`]:  https://github.com/dosbox-staging/dosbox-staging/branches/all?utf8=%E2%9C%93&query=vogons%2F
[`forks/*`]:   https://github.com/dosbox-staging/dosbox-staging/branches/all?utf8=%E2%9C%93&query=forks%2F
[git-notes]:   https://git-scm.com/docs/git-notes

[gpl-badge]:        https://img.shields.io/badge/license-GPL--2.0--or--later-blue
[discord-badge]:    https://img.shields.io/discord/514567252864008206?color=%237289da&logo=discord&logoColor=white&label=discord
[discord]:          https://discord.gg/WwAg3Xf
[build-lin1-badge]: https://img.shields.io/github/workflow/status/dosbox-staging/dosbox-staging/Linux%20builds?label=Linux%20(x86_64)
[build-linux]:      https://github.com/dosbox-staging/dosbox-staging/actions?query=workflow%3A%22Linux+builds%22
[build-lin2-badge]: https://img.shields.io/github/workflow/status/dosbox-staging/dosbox-staging/Platform%20builds?label=Linux%20(arm64,%20S390x)
[build-linux-2]:    https://github.com/dosbox-staging/dosbox-staging/actions?query=workflow%3A%22Platform+builds%22
[build-win-badge]:  https://img.shields.io/github/workflow/status/dosbox-staging/dosbox-staging/Windows%20builds?label=Windows%20(x86,%20x86_64)
[build-win]:        https://github.com/dosbox-staging/dosbox-staging/actions?query=workflow%3A%22Windows+builds%22
[build-mac-badge]:  https://img.shields.io/github/workflow/status/dosbox-staging/dosbox-staging/macOS%20builds?label=macOS%20(arm64,%20x86_64)
[build-mac]:        https://github.com/dosbox-staging/dosbox-staging/actions?query=workflow%3A%22macOS+builds%22
[coverity-badge]:   https://img.shields.io/coverity/scan/dosbox-staging
[lgtm-badge]:       https://img.shields.io/lgtm/grade/cpp/github/dosbox-staging/dosbox-staging
