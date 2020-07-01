# dosbox-staging

[![Linux x86_64 build status](https://img.shields.io/github/workflow/status/dosbox-staging/dosbox-staging/Linux%20builds?label=Linux%20builds%20(x86_64))](https://github.com/dosbox-staging/dosbox-staging/actions?query=workflow%3A%22Linux+builds%22)
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
| **Automated regression tests** | No (WIP)                    | No

[SVN]:https://sourceforge.net/projects/dosbox/
[1]:https://sourceforge.net/p/dosbox/patches/283/
[2]:https://github.com/dosbox-staging/dosbox-staging/actions?query=workflow%3A%22Code+analysis%22
[3]:https://github.com/dosbox-staging/dosbox-staging/actions?query=workflow%3A%22PVS-Studio+analysis%22
[4]:https://scan.coverity.com/projects/dosbox-staging

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

[OPL]:https://en.wikipedia.org/wiki/Yamaha_YMF262
[CGA]:https://en.wikipedia.org/wiki/Color_Graphics_Adapter
[Wayland]:https://en.wikipedia.org/wiki/Wayland_(display_server_protocol)
[7]:https://github.com/dosbox-staging/dosbox-staging/commit/d1be65b105de714924947df4a7909e684d283385
[8]:https://www.vogons.org/viewtopic.php?f=9&t=37782
[9]:https://github.com/dosbox-staging/dosbox-staging/commit/ffe3c5ab7fb5e28bae78f07ea987904f391a7cf8
[10]:https://github.com/dosbox-staging/dosbox-staging/commit/239396fec83dbba6a1eb1a0f4461f4a427d2be38

## Development snapshot builds

Pre-release builds can be downloaded from CI build artifacts. Go to [Linux],
[Windows] or [macOS], select the newest build and download the package linked
in the "**Artifacts**" section.

You need to be logged-in on GitHub to access these snapshot builds.

### [Linux]

Snapshots are dynamically-linked x86\_64 builds, you'll need additional
packages installed via your package manager.

#### Fedora

    sudo dnf install SDL2 SDL2_net opusfile

#### Debian (9 or newer), Ubuntu (16.04 LTS or newer)

    sudo apt install libsdl2-2.0-0 libsdl2-net-2.0-0 libopusfile0

#### Arch, Manjaro

    sudo pacman -S sdl2 sdl2_net opusfile


### [Windows]

Our Windows snapshots include both 32-bit (x86) and 64-bit (x64) builds.

A dosbox.exe file in a snapshot package is not signed, therefore Windows 10
might prevent the program from starting.

If Windows displays the message "Windows Defender SmartScreen prevented an
unrecognised app from starting", you have two options to dismiss it:

1) Click "More info", and button "Run anyway" will appear.
2) Right-click on dosbox.exe, select: Properties → General → Security → Unblock

### [macOS]

Due to GitHub CI and Apple SDKs limitations, the snapshots work only on
macOS Catalina (10.15).

dosbox-staging app bundle is **unsigned** - click on app with right mouse
button, select "Open" and the dialog will show a button to run and unsigned app.

[Linux]:https://github.com/dosbox-staging/dosbox-staging/actions?query=workflow%3A%22Linux+builds%22+is%3Asuccess+branch%3Amaster
[Windows]:https://github.com/dosbox-staging/dosbox-staging/actions?query=workflow%3A%22Windows+builds%22+is%3Asuccess+branch%3Amaster
[macOS]:https://github.com/dosbox-staging/dosbox-staging/actions?query=workflow%3A%22macOS+builds%22+is%3Asuccess+branch%3Amaster

## Build instructions

### Linux, macOS, MSYS2, MinGW, other OSes

Read [INSTALL](INSTALL) file for a general summary about dependencies and
configure options. Read [BUILD.md](BUILD.md) for the comprehensive
compilation guide.

``` shell
git clone https://github.com/dosbox-staging/dosbox-staging.git
cd dosbox-staging
./autogen.sh
./configure
make
```

You can also use a helper script [`./scripts/build.sh`](scripts/build.sh),
that performs builds for many useful scenarios (LTO, FDO, sanitizer builds,
many others).

### Visual Studio (2019 or newer)

First, you need to setup [vcpkg](https://github.com/microsoft/vcpkg) to
install build dependencies. Once vcpkg is installed and bootstrapped, open
PowerShell, and run:

``` powershell
PS:\> .\vcpkg integrate install
PS:\> .\vcpkg install --triplet x64-windows libpng sdl2 sdl2-net opusfile
```

These two steps will ensure that MSVC finds and links all dependencies.

Start Visual Studio and open file: `vs\dosbox.sln`. Make sure you have `x64`
selected as the solution platform.  Use Ctrl+Shift+B to build all projects.

## Interop with SVN

This repository is (deliberately) NOT git-svn compatible, this is a pure
Git repo.

Commits landing in SVN upstream are imported to this repo in a timely manner,
to the branches matching [`svn/*`] pattern.
You can safely use those branches to rebase your changes, and prepare patches
using Git [format-patch](https://git-scm.com/docs/git-format-patch) for sending
upstream (it is easier and faster, than preparing patches manually).

Other branch name patterns are also in use, e.g. [`vogons/*`] for various
patches posted on the Vogons forum.

Git tags matching pattern `svn/*` are pointing to the commits referenced by SVN
"tag" paths at the time of creation.

Additionally, we attach some optional metadata to the commits imported from SVN
in the form of [Git notes](https://git-scm.com/docs/git-notes). To fetch them,
run:

``` shell
git fetch origin "refs/notes/*:refs/notes/*"
```

For some historical context of why this repo exists you can read
[Vogons thread](https://www.vogons.org/viewtopic.php?p=790065#p790065).

[`svn/*`]:https://github.com/dosbox-staging/dosbox-staging/branches/all?utf8=%E2%9C%93&query=svn%2F
[`vogons/*`]:https://github.com/dosbox-staging/dosbox-staging/branches/all?utf8=%E2%9C%93&query=vogons%2F
