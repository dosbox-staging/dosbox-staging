# dosbox-staging
[![](https://github.com/dreamer/dosbox-staging/workflows/Linux%20builds/badge.svg)](https://github.com/dreamer/dosbox-staging/actions?query=workflow%3A%22Linux+builds%22)
[![](https://github.com/dreamer/dosbox-staging/workflows/Windows%20builds/badge.svg)](https://github.com/dreamer/dosbox-staging/actions?query=workflow%3A%22Windows+builds%22)
[![](https://github.com/dreamer/dosbox-staging/workflows/macOS%20builds/badge.svg)](https://github.com/dreamer/dosbox-staging/actions?query=workflow%3A%22macOS+builds%22)
[![](https://img.shields.io/coverity/scan/19891)](https://scan.coverity.com/projects/dosbox-staging)

This repository attempts to modernize the [DOSBox](https://www.dosbox.com/)
project by using current development practices and tools, fixing issues, adding
features that better support today's systems, and sending patches upstream.
Read more at
[Vogons thread](https://www.vogons.org/viewtopic.php?p=790065#p790065).


## Summary of differences compared to upstream:

|                                | dosbox-staging              | DOSBox
|-                               |-                            |-
| **Version control**            | Git                         | [SVN]
| **Language**                   | C++11                       | C++03<sup>[1]</sup>
| **CI**                         | Yes                         | No
| **Static analysis**            | Yes<sup>[2],[3]</sup>       | No
| **Dynamic analysis**           | Yes                         | No
| **Automated regression tests** | No (planned)<sup>[4]</sup>  | No
| **SDL**                        | 1.2 (2.0 WIP)<sup>[5]</sup> | 1.2<sup>＊</sup>

[SVN]:https://sourceforge.net/projects/dosbox/
[1]:https://sourceforge.net/p/dosbox/patches/283/
[2]:https://github.com/dreamer/dosbox-staging/actions?query=workflow%3A%22Code+analysis%22
[3]:https://scan.coverity.com/projects/dosbox-staging
[4]:https://github.com/dreamer/dosbox-staging/issues/23
[5]:https://github.com/dreamer/dosbox-staging/issues/29

Codecs supported for CD Digital Audio emulation (loading CD music via
[cue sheets](https://en.wikipedia.org/wiki/Cue_sheet_(computing))):

|                | dosbox-staging<sup>†</sup> | DOSBox<sup>‡</sup>
|-               |-                           |-
| **Opus**       | Yes (libopus)              | No
| **OGG/Vorbis** | Yes (built-in)             | Yes - SDL\_sound 1.2 (libvorbis)<sup>[6],＊</sup>
| **MP3**        | Yes (built-in)             | Yes - SDL\_sound 1.2 (libmpg123)<sup>[6],＊,§</sup>
| **FLAC**       | Yes (built-in)             | No<sup>§</sup>
| **WAV**        | Yes (built-in)             | Yes - SDL\_sound 1.2 (internal)<sup>[7],＊</sup>
| **AIFF**       | No                         | Yes - SDL\_sound 1.2 (internal)<sup>[7],＊</sup>

<sup>＊ - SDL 1.2 is not actively maintained any more.</sup>  
<sup>† - 22.05 kHz, 44.1 kHz, 48 kHz; mono, stereo</sup>  
<sup>‡ - 44.1 kHz stereo only</sup>  
<sup>§ - SDL\_sound supports it, but the feature might be broken or DOSBox does not indicate support.</sup>  

[6]:https://www.dosbox.com/wiki/MOUNT#Mounting_a_CUE.2FBIN-Pair_as_volume
[7]:https://sourceforge.net/p/dosbox/code-0/HEAD/tree/dosbox/trunk/src/dos/cdrom_image.cpp#l536


## Build instructions

### Linux, macOS, MSYS2, MinGW, other OSes

Read [INSTALL](INSTALL) file for a general summary about dependencies and
configure options. Read [build.md](scripts/build.md) for the comprehensive
compilation guide.

    $ git clone https://github.com/dreamer/dosbox-staging.git
    $ cd dosbox-staging
    $ ./autogen.sh
    $ ./configure
    $ make

You can also use a helper script [`./scripts/build.sh`](scripts/build.sh),
that performs builds for many useful scenarios (LTO, FDO, sanitizer builds,
many others).

### Visual Studio (2019 or newer)

First, you need to setup [vcpkg](https://github.com/microsoft/vcpkg) to
install build dependencies. Once vcpkg is installed and bootstrapped, open
PowerShell, and run:

    PS> .\vcpkg integrate install
    PS> .\vcpkg install libpng sdl1 sdl1-net opusfile

These two steps will ensure that MSVC finds and links all dependencies.

Start Visual Studio, open file: `visualc_net\dosbox.sln` and build all
projects (Ctrl+Shift+B).


## Interop with SVN

This repository is (deliberately) NOT git-svn compatible, this is a pure
Git repo.

Commits landing in SVN upstream are imported to this repo in a timely manner,
to the branches matching `svn/*` pattern, e.g.
[`svn/trunk`](https://github.com/dreamer/dosbox-staging/tree/svn/trunk).
You can safely use those branches to rebase your changes, and prepare patches
using Git [format-patch](https://git-scm.com/docs/git-format-patch) for sending
upstream (it is easier and faster, than preparing patches manually).

Git tags matching pattern `svn/*` are pointing to the commits referenced by SVN
"tag" paths at the time of creation.

Additionally, we attach some optional metadata to the commits imported from SVN
in the form of [Git notes](https://git-scm.com/docs/git-notes). To fetch them,
run:

    $ git fetch origin "refs/notes/*:refs/notes/*"
