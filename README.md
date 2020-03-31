# dosbox-staging

[![Linux build status](https://img.shields.io/github/workflow/status/dreamer/dosbox-staging/Linux%20builds?label=Linux%20builds)](https://github.com/dreamer/dosbox-staging/actions?query=workflow%3A%22Linux+builds%22)
[![Windows build status](https://img.shields.io/github/workflow/status/dreamer/dosbox-staging/Windows%20builds?label=Windows%20builds)](https://github.com/dreamer/dosbox-staging/actions?query=workflow%3A%22Windows+builds%22)
[![macOS build status](https://img.shields.io/github/workflow/status/dreamer/dosbox-staging/macOS%20builds?label=macOS%20builds)](https://github.com/dreamer/dosbox-staging/actions?query=workflow%3A%22macOS+builds%22)

This repository attempts to modernize the [DOSBox](https://www.dosbox.com/)
project by using current development practices and tools, fixing issues, adding
features that better support today's systems, and sending patches upstream.
Read more at
[Vogons thread](https://www.vogons.org/viewtopic.php?p=790065#p790065).

## Summary of differences compared to upstream

|                                | dosbox-staging              | DOSBox
|-                               |-                            |-
| **Version control**            | Git                         | [SVN]
| **Language**                   | C++11                       | C++03<sup>[1]</sup>
| **CI**                         | Yes                         | No
| **Static analysis**            | Yes<sup>[C],[P],[S]</sup>   | No
| **Dynamic analysis**           | Yes                         | No
| **Automated regression tests** | No (WIP)                    | No
| **SDL**                        | 2.0                         | 1.2<sup>＊</sup>

[SVN]:https://sourceforge.net/projects/dosbox/
[1]:https://sourceforge.net/p/dosbox/patches/283/
[4]:https://github.com/dreamer/dosbox-staging/issues/23
[C]:https://github.com/dreamer/dosbox-staging/actions?query=workflow%3A%22Code+analysis%22
[P]:https://github.com/dreamer/dosbox-staging/actions?query=workflow%3A%22PVS-Studio+analysis%22
[S]:https://scan.coverity.com/projects/dosbox-staging

**dosbox-staging** does not support audio playback using physical CDs.
Using CD Digital Audio emulation (loading CD music via
[cue sheets](https://en.wikipedia.org/wiki/Cue_sheet_(computing)) or
mounting [ISO images](https://en.wikipedia.org/wiki/ISO_image)) is
preferred instead.

Codecs supported for CD-DA emulation:

|                | dosbox-staging<sup>†</sup> | DOSBox<sup>‡</sup>
|-               |-                           |-
| **Opus**       | Yes (libopus)              | No
| **OGG/Vorbis** | Yes (built-in)             | Yes - SDL\_sound 1.2 (libvorbis)<sup>[5],＊</sup>
| **MP3**        | Yes (built-in)             | Yes - SDL\_sound 1.2 (libmpg123)<sup>[5],＊,§</sup>
| **FLAC**       | Yes (built-in)             | No<sup>§</sup>
| **WAV**        | Yes (built-in)             | Yes - SDL\_sound 1.2 (internal)<sup>[6],＊</sup>
| **AIFF**       | No                         | Yes - SDL\_sound 1.2 (internal)<sup>[6],＊</sup>

<sup>＊- SDL 1.2 was last updated 2013-08-17 and SDL\_sound 2008-04-20</sup>\
<sup>† - 22.05 kHz, 44.1 kHz, 48 kHz; mono, stereo</sup>\
<sup>‡ - 44.1 kHz stereo only</sup>\
<sup>§ - Broken or unsupported in either SDL\_sound or DOSBox</sup>

[5]:https://www.dosbox.com/wiki/MOUNT#Mounting_a_CUE.2FBIN-Pair_as_volume
[6]:https://sourceforge.net/p/dosbox/code-0/HEAD/tree/dosbox/trunk/src/dos/cdrom_image.cpp#l536

Other feature differences:

|                        | dosbox-staging                               | DOSBox
|-                       |-                                             |-
| **Pixel-perfect mode** | Yes (`output=texturepp`)<sup>[7]</sup>       | N/A
| **[OPL] emulators**    | compat, fast, mame, nuked<sup>[8]</sup>      | compat, fast, mame
| **[CGA]/mono support** | Yes (`machine=cga_mono`)<sup>[9]</sup>       | Only CGA with colour
| **[Wayland] support**  | Experimental (use `SDL_VIDEODRIVER=wayland`) | N/A

[OPL]:https://en.wikipedia.org/wiki/Yamaha_YMF262
[CGA]:https://en.wikipedia.org/wiki/Color_Graphics_Adapter
[Wayland]:https://en.wikipedia.org/wiki/Wayland_(display_server_protocol)
[7]:https://github.com/dreamer/dosbox-staging/commit/d1be65b105de714924947df4a7909e684d283385
[8]:https://www.vogons.org/viewtopic.php?f=9&t=37782
[9]:https://github.com/dreamer/dosbox-staging/commit/ffe3c5ab7fb5e28bae78f07ea987904f391a7cf8

## Development snapshot builds

Pre-release builds can be downloaded from CI build artifacts. Go to
[Linux](https://github.com/dreamer/dosbox-staging/actions?query=workflow%3A%22Linux+builds%22+is%3Asuccess+branch%3Amaster),
[Windows](https://github.com/dreamer/dosbox-staging/actions?query=workflow%3A%22Windows+builds%22+is%3Asuccess+branch%3Amaster)
or [macOS](https://github.com/dreamer/dosbox-staging/actions?query=workflow%3A%22macOS+builds%22+is%3Asuccess+branch%3Amaster),
select the newest build and download the package linked in the
"**Artifacts**" section.

You need to be logged-in on GitHub to access these snapshot builds.

### [Linux](https://github.com/dreamer/dosbox-staging/actions?query=workflow%3A%22Linux+builds%22+is%3Asuccess+branch%3Amaster)

Snapshots are dynamically-linked x86\_64 builds, you'll need additional
packages installed via your package manager.

#### Fedora

    sudo dnf install SDL2 SDL2_net opusfile

#### Debian, Ubuntu

    sudo apt install libsdl2-2.0 libsdl2-net-2.0 libopusfile0

#### Arch, Manjaro

    sudo pacman -S sdl2 sdl2_net opusfile

### [Windows](https://github.com/dreamer/dosbox-staging/actions?query=workflow%3A%22Windows+builds%22+is%3Asuccess+branch%3Amaster)

A dosbox.exe file in a snapshot package is not signed, therefore Windows 10
might prevent the program from starting.

If Windows displays the message "Windows Defender SmartScreen prevented an
unrecognised app from starting", you have two options to dismiss it:

1) Click "More info", and button "Run anyway" will appear.
2) Right-click on dosbox.exe, select: Properties → General → Security → Unblock

Windows packages are built for "x86" architecture (in practice it means i686).

### [macOS](https://github.com/dreamer/dosbox-staging/actions?query=workflow%3A%22macOS+builds%22+is%3Asuccess+branch%3Amaster)

Due to GitHub CI and Apple SDKs limitations, the snapshots work only on
macOS Catalina (10.15).

dosbox-staging app bundle is **unsigned** - click on app with right mouse
button, select "Open" and the dialog will show a button to run and unsigned app.

## Build instructions

### Linux, macOS, MSYS2, MinGW, other OSes

Read [INSTALL](INSTALL) file for a general summary about dependencies and
configure options. Read [build.md](scripts/build.md) for the comprehensive
compilation guide.

``` shell
git clone https://github.com/dreamer/dosbox-staging.git
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
PS:\> .\vcpkg install libpng sdl2 sdl2-net opusfile
```

These two steps will ensure that MSVC finds and links all dependencies.

Start Visual Studio, open file: `vs\dosbox.sln` and build all projects
(Ctrl+Shift+B).

## Interop with SVN

This repository is (deliberately) NOT git-svn compatible, this is a pure
Git repo.

Commits landing in SVN upstream are imported to this repo in a timely manner,
to the branches matching
[`svn/*`](https://github.com/dreamer/dosbox-staging/branches/all?utf8=%E2%9C%93&query=svn%2F)
pattern.
You can safely use those branches to rebase your changes, and prepare patches
using Git [format-patch](https://git-scm.com/docs/git-format-patch) for sending
upstream (it is easier and faster, than preparing patches manually).

Other branch name patterns are also in use, e.g.
[`vogons/*`](https://github.com/dreamer/dosbox-staging/branches/all?utf8=%E2%9C%93&query=vogons%2F)
for various patches posted on the Vogons forum.

Git tags matching pattern `svn/*` are pointing to the commits referenced by SVN
"tag" paths at the time of creation.

Additionally, we attach some optional metadata to the commits imported from SVN
in the form of [Git notes](https://git-scm.com/docs/git-notes). To fetch them,
run:

``` shell
git fetch origin "refs/notes/*:refs/notes/*"
```

