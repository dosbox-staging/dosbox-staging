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

## Summary of differences compared to upstream

### For developers

|                                | DOSBox Staging                | DOSBox
|-                               |-                              |-
| **Version control**            | Git                           | [SVN]
| **Language**                   | C++17                         | C++03<sup>[1]</sup>
| **SDL**                        | >= 2.0.5                      | 1.2<sup>＊</sup>
| **Logging**                    | Loguru for C++<sup>[6]</sup>  | Yes, in-house class
| **Buildsystem**                | Meson or Visual Studio 2019   | Autotools or Visual Studio 2003
| **CI**                         | Yes                           | No
| **Static analysis**            | Yes<sup>[2],[3],[4],[5]</sup> | No
| **Dynamic analysis**           | Yes                           | No
| **clang-format**               | Yes                           | No
| **[Development builds]**       | Yes                           | No
| **Unit tests**                 | Yes<sup>[6]</sup>             | No
| **Automated regression tests** | WIP                           | No

[SVN]:https://sourceforge.net/projects/dosbox/
[1]:https://sourceforge.net/p/dosbox/patches/283/
[2]:https://github.com/dosbox-staging/dosbox-staging/actions?query=workflow%3A%22Code+analysis%22
[3]:https://lgtm.com/projects/g/dosbox-staging/dosbox-staging/
[4]:https://scan.coverity.com/projects/dosbox-staging
[5]:https://github.com/dosbox-staging/dosbox-staging/actions?query=workflow%3A%22PVS-Studio+analysis%22
[6]:tests/README.md
[7]: https://github.com/emilk/loguru
[Development builds]:https://dosbox-staging.github.io/downloads/devel/

### Feature differences

DOSBox Staging supports features not included in DOSBox SVN.
Below is a detailed comparison of feature differences between
DOSBox Staging and DOSBox SVN.

Codecs supported for CD-DA emulation:

|                | DOSBox Staging<sup>†</sup> | DOSBox SVN<sup>‡</sup>
|-               |-                           |-
| **Opus**       | Yes (libopus)              | No
| **OGG/Vorbis** | Yes (built-in)             | Yes - SDL\_sound 1.2 (libvorbis)<sup>[6],＊</sup>
| **MP3**        | Yes (built-in)             | Yes - SDL\_sound 1.2 (libmpg123)<sup>[6],＊,§</sup>
| **FLAC**       | Yes (built-in)             | No<sup>§</sup>
| **WAV**        | Yes (built-in)             | Yes - SDL\_sound 1.2 (built-in)<sup>[7],＊</sup>
| **AIFF**       | No                         | Yes - SDL\_sound 1.2 (built-in)<sup>[7],＊</sup>

<sup>＊- SDL 1.2 was last updated 2013-08-17 and SDL\_sound 2008-04-20</sup>\
<sup>† - 8/16/24 bit-depth, 22.05/44.1/48 kHz, and mono or stereo</sup>\
<sup>‡ - 44.1 kHz stereo only</sup>\
<sup>§ - Broken or unsupported in either SDL\_sound or DOSBox</sup>

[6]:https://www.dosbox.com/wiki/MOUNT#Mounting_a_CUE.2FBIN-Pair_as_volume
[7]:https://sourceforge.net/p/dosbox/code-0/HEAD/tree/dosbox/trunk/src/dos/cdrom_image.cpp#l536

Feature differences between release binaries (or unpatched sources):

| *Feature*                   | *DOSBox Staging*                                     | *DOSBox SVN*
|-                            |-                                                     |-
| **Pixel-perfect mode**      | Yes: `output=openglpp` or `output=texturepp`         | N/A
| **Resizable window**        | Yes: for all `output=opengl` modes                   | N/A
| **Relative window size**    | Yes: `windowresolution=small`, `medium`, or `large`  | `windowresolution=X%`
| **Window placement**        | Yes: `windowposition = 0,0`, and more<sup>[16]</sup> | Manual placement
| **[OPL] emulators**         | compat, fast, mame, nuked<sup>[8]</sup>              | compat, fast, mame
| **[CGA]/mono support**      | Yes: `machine=cga_mono`)<sup>[9]</sup>               | Only CGA with colour
| **CGA composite modes**     | Yes: `machine=pcjr/tandy/cga` with hotkeys)          | N/A
| **[Wayland] support**       | Experimental: use `SDL_VIDEODRIVER=wayland`          | N/A
| **Modem phonebook file**    | Yes: `phonebookfile=<name>`                          | N/A
| **Autotype command**        | Yes<sup>[10]</sup>                                   | N/A
| **Startup verbosity**       | Yes<sup>[11]</sup>                                   | N/A
| **[GUS] enhancements**      | Yes<sup>[12]</sup>                                   | N/A
| **Raw mouse input**         | Yes: `raw_mouse_input=true`                          | N/A
| **[FluidSynth][FS] MIDI**   | Yes<sup>[13]</sup>: FluidSynth 2.x                   | Only external synths
| **[MT-32] emulator**        | Yes<sup>＊</sup>: libmt32emu 2.4.2                   | N/A
| **Expanded S3 support**     | 4 and 8 MiB of RAM<sup>[14]</sup>                    | 2 MiB of RAM
| **Portable & layered conf** | By default<sup>[15]</sup>                            | With`-userconf` and `-conf`
| **Translations handling**   | Bundled, see section 14 in README                    | With path to .lng file
| **[ENet] modem transport**  | Yes: serialport `sock:1` flag or `SERIAL.COM`<sup>[17]</sup> | N/A
| **Ethernet via [slirp]**    | Yes: See `[ethernet]` section in conf file           | N/A
| **IDE support for CDROMs**  | Yes: See `-ide` flag in `IMGMOUNT.COM /help`         | N/A
| **Networking in Win3.11**   | Yes: Via local shell<sup>[18]</sup>                  | Yes: bootable HDD image


<sup>＊- Requires original ROM files</sup>

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

### Linux, macOS

Install build dependencies appropriate for your OS:

``` shell
# Fedora
sudo dnf install ccache gcc-c++ meson alsa-lib-devel libpng-devel \
                 SDL2-devel SDL2_net-devel opusfile-devel fluidsynth-devel \
                 mt32emu-devel libslirp-devel speexdsp-devel
```

``` shell
# Debian, Ubuntu
sudo apt install ccache build-essential libasound2-dev libpng-dev \
                 libsdl2-dev libsdl2-net-dev libopusfile-dev \
                 libfluidsynth-dev libslirp-dev libspeexdsp-dev

# Install Meson on Debian-10 "Buster" or Ubuntu-20.04 and older
sudo apt install python3-setuptools python3-pip
sudo pip3 install --upgrade meson ninja

# Install Meson on Debian-11 "Bullseye" or Ubuntu-21.04 and newer
sudo apt install meson
```

``` shell
# Arch, Manjaro
sudo pacman -S ccache gcc meson alsa-lib libpng sdl2 sdl2_net opusfile \
               fluidsynth libslirp speexdsp
```

``` shell
# openSUSE
sudo zypper install ccache gcc gcc-c++ meson alsa-devel libpng-devel \
                    libSDL2-devel libSDL2_net-devel opusfile-devel \
                    fluidsynth-devel libmt32emu-devel libslirp-devel \
                    speexdsp
```

``` shell
# macOS
xcode-select --install
brew install ccache meson libpng sdl2 sdl2_net opusfile fluid-synth libslirp speexdsp
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
    meson setup \
        -Dbuildtype=release \
        -Ddefault_library=static \
        -Db_asneeded=true \
        -Dtry_static_libs=png \
        -Dfluidsynth:enable-floats=true \
        -Dfluidsynth:try-static-deps=true \
      build
    ```

    The above enables all of DOSBox Staging's functional features. If you're
    interested in seeing all of Meson's setup options, run: `meson configure`.


  - Compile the sources. This is necessary every time you want a new build:

    ``` shell
    meson compile -C build
    ```

    Your binary is: `build/dosbox` -- have fun!


### Windows - Visual Studio (2019 or newer)

First, you need to setup [vcpkg] to install build dependencies. Once vcpkg
is bootstrapped, open PowerShell and run:

``` powershell
PS:\> .\vcpkg integrate install
PS:\> .\vcpkg install --triplet x64-windows libpng sdl2 sdl2-net libmt32emu opusfile fluidsynth gtest speexdsp
```

These two steps will ensure that MSVC finds and links all dependencies.

Start Visual Studio and open file: `vs\dosbox.sln`. Make sure you have `x64`
selected as the solution platform.  Use Ctrl+Shift+B to build all projects.

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

Commits landing in SVN upstream are imported to this repo in a timely manner,
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
