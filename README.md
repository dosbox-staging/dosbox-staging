# dosbox-staging

[![Linux x86\_64 build status](https://img.shields.io/github/workflow/status/dosbox-staging/dosbox-staging/Linux%20builds?label=Linux%20builds%20(x86_64))](https://github.com/dosbox-staging/dosbox-staging/actions?query=workflow%3A%22Linux+builds%22)
[![Linux other build status](https://img.shields.io/github/workflow/status/dosbox-staging/dosbox-staging/Platform%20builds?label=Linux%20builds%20(ARM,%20S390x,%20ppc64le))](https://github.com/dosbox-staging/dosbox-staging/actions?query=workflow%3A%22Platform+builds%22)
[![Windows build status](https://img.shields.io/github/workflow/status/dosbox-staging/dosbox-staging/Windows%20builds?label=Windows%20builds)](https://github.com/dosbox-staging/dosbox-staging/actions?query=workflow%3A%22Windows+builds%22)
[![macOS build status](https://img.shields.io/github/workflow/status/dosbox-staging/dosbox-staging/macOS%20builds?label=macOS%20builds)](https://github.com/dosbox-staging/dosbox-staging/actions?query=workflow%3A%22macOS+builds%22)

This repository attempts to modernize the DOSBox codebase by using current
development practices and tools, fixing issues, and adding features that better
support today's systems.

## Summary of differences compared to upstream

### For developers

|                                | dosbox-staging              | DOSBox
|-                               |-                            |-
| **Version control**            | Git                         | [SVN]
| **Language**                   | C++14                       | C++03<sup>[1]</sup>
| **SDL**                        | 2.0                         | 1.2<sup>＊</sup>
| **CI**                         | Yes                         | No
| **Static analysis**            | Yes<sup>[2],[3],[4]</sup>   | No
| **Dynamic analysis**           | Yes                         | No
| **clang-format**               | Yes                         | No
| **[Development builds]**       | Yes                         | No
| **Automated regression tests** | No (WIP)                    | No

[SVN]:https://sourceforge.net/projects/dosbox/
[1]:https://sourceforge.net/p/dosbox/patches/283/
[2]:https://github.com/dosbox-staging/dosbox-staging/actions?query=workflow%3A%22Code+analysis%22
[3]:https://github.com/dosbox-staging/dosbox-staging/actions?query=workflow%3A%22PVS-Studio+analysis%22
[4]:https://scan.coverity.com/projects/dosbox-staging
[Development builds]:https://dosbox-staging.github.io/downloads/devel/

### Feature differences

**dosbox-staging** does not support audio playback using physical CDs.
Using CD Digital Audio emulation (loading CD music via
[cue sheets](https://en.wikipedia.org/wiki/Cue_sheet_(computing)) or
mounting [ISO images](https://en.wikipedia.org/wiki/ISO_image)) is
preferred instead.

Codecs supported for CD-DA emulation:

|                | dosbox-staging<sup>†</sup> | DOSBox SVN<sup>‡</sup>
|-               |-                           |-
| **Opus**       | Yes (libopus)              | No
| **OGG/Vorbis** | Yes (built-in)             | Yes - SDL\_sound 1.2 (libvorbis)<sup>[5],＊</sup>
| **MP3**        | Yes (built-in)             | Yes - SDL\_sound 1.2 (libmpg123)<sup>[5],＊,§</sup>
| **FLAC**       | Yes (built-in)             | No<sup>§</sup>
| **WAV**        | Yes (built-in)             | Yes - SDL\_sound 1.2 (built-in)<sup>[6],＊</sup>
| **AIFF**       | No                         | Yes - SDL\_sound 1.2 (built-in)<sup>[6],＊</sup>

<sup>＊- SDL 1.2 was last updated 2013-08-17 and SDL\_sound 2008-04-20</sup>\
<sup>† - 22.05 kHz, 44.1 kHz, 48 kHz; mono, stereo</sup>\
<sup>‡ - 44.1 kHz stereo only</sup>\
<sup>§ - Broken or unsupported in either SDL\_sound or DOSBox</sup>

[5]:https://www.dosbox.com/wiki/MOUNT#Mounting_a_CUE.2FBIN-Pair_as_volume
[6]:https://sourceforge.net/p/dosbox/code-0/HEAD/tree/dosbox/trunk/src/dos/cdrom_image.cpp#l536

Other differences:

|                          | dosbox-staging                               | DOSBox SVN
|-                         |-                                             |-
| **Pixel-perfect mode**   | Yes (`output=texturepp`)<sup>[7]</sup>       | N/A
| **Resizable window**     | Experimental (`windowresolution=resizable`)  | N/A
| **Relative window size** | N/A                                          | `windowresolution=X%`
| **[OPL] emulators**      | compat, fast, mame, nuked<sup>[8]</sup>      | compat, fast, mame
| **[CGA]/mono support**   | Yes (`machine=cga_mono`)<sup>[9]</sup>       | Only CGA with colour
| **[Wayland] support**    | Experimental (use `SDL_VIDEODRIVER=wayland`) | N/A
| **Modem phonebook file** | Yes (`phonebookfile=<name>`)                 | N/A
| **Autotype command**     | Yes<sup>[10]</sup>                           | N/A
| **Startup verbosity**    | Yes<sup>[11]</sup>                           | N/A
| **GUS Enhancements**     | Yes<sup>[12]</sup>                           | N/A

[OPL]:https://en.wikipedia.org/wiki/Yamaha_YMF262
[CGA]:https://en.wikipedia.org/wiki/Color_Graphics_Adapter
[Wayland]:https://en.wikipedia.org/wiki/Wayland_(display_server_protocol)
[7]:https://github.com/dosbox-staging/dosbox-staging/commit/d1be65b105de714924947df4a7909e684d283385
[8]:https://www.vogons.org/viewtopic.php?f=9&t=37782
[9]:https://github.com/dosbox-staging/dosbox-staging/commit/ffe3c5ab7fb5e28bae78f07ea987904f391a7cf8
[10]:https://github.com/dosbox-staging/dosbox-staging/commit/239396fec83dbba6a1eb1a0f4461f4a427d2be38
[11]: https://github.com/dosbox-staging/dosbox-staging/pull/477
[12]: https://github.com/dosbox-staging/dosbox-staging/wiki/Gravis-UltraSound-Enhancements

## Stable release builds

[Linux](https://dosbox-staging.github.io/downloads/linux/),
[Windows](https://dosbox-staging.github.io/downloads/windows/),
[macOS](https://dosbox-staging.github.io/downloads/macos/)

## Test builds / development snapshots

[Links to the newest builds](https://dosbox-staging.github.io/downloads/devel/)

## Build instructions

Read [INSTALL](INSTALL) file for a general summary about dependencies and
configure options and [BUILD.md](BUILD.md) for the comprehensive
compilation guide.  You can also use helper script
[`./scripts/build.sh`](scripts/build.sh), that performs builds for many
useful scenarios (LTO, FDO, sanitizer builds, many others).

### Linux, macOS

Install build dependencies appropriate for your OS:

``` shell
# Fedora
sudo dnf install gcc-c++ automake alsa-lib-devel libpng-devel SDL2-devel \
                 SDL2_net-devel opusfile-devel fluidsynth-devel
```

``` shell
# Debian, Ubuntu
sudo apt install build-essential automake libasound2-dev libpng-dev \
                 libsdl2-dev libsdl2-net-dev libopusfile-dev libfluidsynth-dev
```

``` shell
# Arch, Manjaro
sudo pacman -S gcc automake alsa-lib libpng sdl2 sdl2_net opusfile fluidsynth
```

``` shell
# macOS
xcode-select --install
brew install autogen automake libpng sdl2 sdl2_net opusfile
```
*Note: FluidSynth as a library is not available on macOS via brew.
Use `--disable-fluidsynth` configure flag to disable the feature.*

Compilation flags suggested for local optimised builds:

``` shell
git clone https://github.com/dosbox-staging/dosbox-staging.git
cd dosbox-staging
./autogen.sh
./configure CPPFLAGS="-DNDEBUG" \
            CFLAGS="-O3 -march=native" \
            CXXFLAGS="-O3 -march=native"
make -j$(nproc)
```

See [CONTRIBUTING.md](CONTRIBUTING.md#build-dosbox-staging) for compilation
flags more suited for development.

### Windows - Visual Studio (2019 or newer)

First, you need to setup [vcpkg](https://github.com/microsoft/vcpkg) to
install build dependencies. Once vcpkg is bootstrapped, open PowerShell,
and run:

``` powershell
PS:\> .\vcpkg integrate install
PS:\> .\vcpkg install --triplet x64-windows libpng sdl2 sdl2-net opusfile fluidsynth
```

These two steps will ensure that MSVC finds and links all dependencies.

Start Visual Studio and open file: `vs\dosbox.sln`. Make sure you have `x64`
selected as the solution platform.  Use Ctrl+Shift+B to build all projects.

### Windows (MSYS2, MinGW), macOS (MacPorts), Haiku OS, others

Instructions for other build systems and operating systems are documented
in [BUILD.md](BUILD.md).

## Imported branches and community patches

Commits landing in SVN upstream are imported to this repo in a timely manner,
to the branches matching [`svn/*`] pattern, e.g. [`svn/trunk`].

Other branch name patterns are also in use: [`vogons/*`] for various
patches posted on the Vogons forum, [`munt/*`] for patches from the Munt
project, etc.

Git tags matching pattern `svn/*` are pointing to the commits referenced by SVN
"tag" paths at the time of creation.

Additionally, we attach some optional metadata to the commits imported from SVN
in the form of [Git notes](https://git-scm.com/docs/git-notes). To fetch them,
run:

``` shell
git fetch origin "refs/notes/*:refs/notes/*"
```

For some historical context of why this repo exists you can read
[Vogons thread](https://www.vogons.org/viewtopic.php?p=790065#p790065),
([1](https://imgur.com/a/bnJEZcx), [2](https://imgur.com/a/HnG1Ls4))

[`svn/*`]:https://github.com/dosbox-staging/dosbox-staging/branches/all?utf8=%E2%9C%93&query=svn%2F
[`svn/trunk`]:https://github.com/dosbox-staging/dosbox-staging/tree/svn/trunk
[`vogons/*`]:https://github.com/dosbox-staging/dosbox-staging/branches/all?utf8=%E2%9C%93&query=vogons%2F
[`munt/*`]:https://github.com/dosbox-staging/dosbox-staging/branches/all?utf8=%E2%9C%93&query=munt%2F
