# Build Script

## Introduction

This script builds `dosbox-staging` with your choice of compiler, release
type, and additional options. It runs on MacOS, Linux, Windows, and possibly
other operating systems.

If this is your first time building dosbox-staging, then you will need to
install its development tools and dependencies, which is covered in the
notes below.

- [Build Script](#build-script)
  - [Introduction](#introduction)
  - [Requirements](#requirements)
  - [Windows Procedures](#windows-procedures)
    - [Install MSYS2](#install-msys2)
    - [Clone and Build a Windows Binary](#clone-and-build-a-windows-binary)
  - [macOS Procedures](#macos-procedures)
    - [Install Dependencies under macOS](#install-dependencies-under-macos)
      - [Xcode Installation](#xcode-installation)
      - [Brew Package Manager Installation](#brew-package-manager-installation)
      - [MacPorts Package Manager Installation](#macports-package-manager-installation)
    - [Clone and Build a macOS Binary](#clone-and-build-a-macos-binary)
  - [Linux Procedures](#linux-procedures)
    - [Install Dependencies under Linux](#install-dependencies-under-linux)
    - [Build a Linux Binary](#build-a-linux-binary)
  - [Haiku Procedures](#haiku-procedures)
    - [Install Dependencies under Haiku](#install-dependencies-under-haiku)
    - [Build a Haiku Binary](#build-a-haiku-binary)
  - [Additional Tips](#additional-tips)
    - [Compiler variations](#compiler-variations)
    - [Release types](#release-types)
    - [Build Results, Rebuilding, and Cleaning](#build-results-rebuilding-and-cleaning)
    - [CCache](#ccache)
    - [Optimization Modifiers](#optimization-modifiers)
  - [AutoFDO Procedures](#autofdo-procedures)
    - [Prerequisites for AutoFDO](#prerequisites-for-autofdo)
    - [Record Data for AutoFDO Builds](#record-data-for-autofdo-builds)
    - [Build Using AutoFDO Data](#build-using-autofdo-data)

## Requirements

- **Windows newer than XP**
  - **NTFS-based C:**, because msys2 doesn't work on FAT filesystems
- **MacOS** 10.x
- **Haiku** up-to-date
- **Ubuntu** 16.04 or newer
- **Fedora** up-to-date
- **RedHat or CentOS** 7 or newer
- **Arch-based distribution** up-to-date
- **OpenSUSE Leap** 15 or newer

## Windows Procedures

### Install MSYS2

1. Download and install Chocolatey: <https://chocolatey.org/install>
1. Open a console and run Cholocatey's command line interface (CLI)
   to install msys2 and git:

   `choco install msys2 git --no-progress`

   ``` text
   Chocolatey v0.10.15
   Installing the following packages:
   msys2 git
   By installing you accept licenses for the packages.

   msys2 v20180531.0.0 [Approved]
   msys2 package files install completed. Performing other installation steps.
   Installing to: C:\tools\msys64
   Extracting 64-bit C:\ProgramData\chocolatey\lib\msys2\tools\msys2-base-x86_64.tar.xz
   Extracting C:\tools\msys64\msys2-base-x86_64-20180531.tar to C:\tools\msys64...
   Starting initialization via msys2_shell.cmd
   PATH environment variable does not have C:\tools\msys64 in it. Adding...
   ```

1. Launch a new MSYS2 terminal (not a CMD prompt or Powershell window):
   1. Start Menu > Run ... `c:\tools\msys64\msys2_shell.cmd`
   1. Run all subsequent steps within this terminal.

### Clone and Build a Windows Binary

1. Clone and enter the repository's directory:
   1. `git clone https://github.com/dosbox-staging/dosbox-staging.git`
   1. `cd dosbox-staging`

   Be sure to run all subsequent steps below while inside the repo's directory.

2. (🏁 first-time-only) Install the build tools and runtime dependencies:

   `./scripts/list-build-dependencies.sh -m msys2 -c clang | xargs pacman -S
   --noconfirm`

3. Build an optimized binary with either compiler:

- GCC: `./scripts/build.sh -c gcc -t release --bin-path /mingw64/bin`
- Clang: `./scripts/build.sh -c clang -t release --bin-path /mingw64/bin`

1. To build a debug binary, use `-t debug` in place of `-t release`.

## macOS Procedures

Builds on macOS can be performed with Clang or GCC. For general use, we
recommend building with `Clang` as it supports linking with Apple's CoreMidi
SDK. Developers interested in testing wider compiler coverage might also be
interested in building with GCC. The following sections describe how to install
and build with both compilers.

### Install Dependencies under macOS

Before installing either Brew or MacPorts, the Apple's Xcode tools need to be
installed and the license agreed to:

#### Xcode Installation

1. Install the command line tools: `xcode-select --install`
1. Accept the license agreement: `sudo xcodebuild -license`

#### Install the Brew Package Manager and Dependencies

1. Download and install brew per the instructions here: <https://brew.sh>
1. Update it with: `brew update`
1. Install git with: `brew install git`
1. Clone the repository: `git clone
   https://github.com/dosbox-staging/dosbox-staging.git`
1. Change directories into the repo: `cd dosbox-staging`
1. Install dependencies:
  
   `brew install $(./scripts/list-build-dependencies.sh -m brew -c gcc)`

#### Install the MacPorts Package Manager and Dependencies

1. Build and install MacPorts along with dosbox-staging dependencies with the
   following sequence:

   ``` shell
   git clone --quiet --depth=1 https://github.com/macports/macports-base.git
   cd macports-base
   ./configure
   make -j"$(sysctl -n hw.physicalcpu)"
   sudo make install
   PREFIX="/opt/local"
   PATH="${PREFIX}/sbin:${PREFIX}/bin:${PATH}"
   sudo port -q selfupdate
   ```
1. Clone the repository: `git clone
   https://github.com/dosbox-staging/dosbox-staging.git`
1. Change directories into the repo: `cd dosbox-staging`
1. Install depedencies:

   `sudo port -q install $(./scripts/list-build-dependencies.sh -m macports -c gcc)`

### Build a macOS Binary (common for Brew and MacPorts)

1. Build an optimized binary with various compilers:

- Clang: `./scripts/build.sh -c clang -t release -p /usr/local/bin`
- GCC (brew): `./scripts/build.sh -c gcc -v 9 -t release -p /usr/local/bin`
- GCC (macports): `./scripts/build.sh -c gcc -v mp-9 -t release -p
  /opt/local/bin`

1. To build a debug binary, use `-t debug` in place of `-t release`.

## Linux Procedures

### Install Dependencies under Linux

1. Install git: `sudo apt install -y git`
1. Clone the repository: `git clone
   https://github.com/dosbox-staging/dosbox-staging.git`
1. Change directories into the repo: `cd dosbox-staging`
1. (🏁 first-time-only) Install dependencies based on your package manager.
   In this example, we use Ubuntu 20.04:

   `sudo apt install -y $(./scripts/list-build-dependencies.sh -m apt -c clang)`

   For other supported package managers, run:
   `./scripts/list-build-dependencies.sh`

### Build a Linux Binary

1. Build an optimized binary using various compilers:

- Clang: `./scripts/build.sh -c clang -t release -m lto`
- GCC (default version): `./scripts/build.sh -c gcc -t release -m lto`
- GCC (specific version, ie: 10): `./scripts/build.sh -c gcc -v 10 -t release -m
  lto`

  :warning: Raspberry Pi 4 users should avoid link-time-optimized builds for
  now. Simply drop the `-m lto` in your build line.

1. To build a debug binary, use `-t debug` in place of `-t release -m lto`.

## Haiku Procedures

### Install Dependencies under Haiku

1. Clone the repository: `git clone
   https://github.com/dosbox-staging/dosbox-staging.git`
1. Change directories into the repo: `cd dosbox-staging`
1. (🏁 first-time-only) Install dependencies:

   `pkgman install -y $(./scripts/list-build-dependencies.sh -m haikuports -c
   clang -v 9)`

### Build a Haiku Binary

1. Build an optimized binary using various compilers:

- Clang: `./scripts/build.sh --compiler clang -t release -m lto
  --prefix=$HOME/config/non-packaged`
- GCC: `./scripts/build.sh -c gcc -t release --prefix=$HOME/config/non-packaged`

1. To build a debug binary, use `-t debug` in place of `-t release`.
1. Install the binary: `make install`
1. Edit your configuration file by running: `dosbox -editconf` and make the
   following suggested changes (leave all other settings as-is):
 
   ``` ini
   [sdl]
   windowresolution = 800x600
   output = texturenb
   texture_renderer = software

   [renderer]
   scaler = none
   glshader = none

   [cpu]
   core = normal
   ```

   The state of Haiku's GPU Hardware-acceleration is being discussed here:
   https://discuss.haiku-os.org/t/state-of-accelerated-opengl/4163

1. You may now run `dosbox` inside any directory in your Terminal.

## Additional Tips

### Compiler variations

The compiler, version, and bit-depth can be selected by passing the following
common options to the **list-build-dependencies.sh** and **build.sh** scripts:

- `--compiler clang` or `-c clang` to use CLang instead of GCC
- `--compiler-version 8` or `-v <version>` to specify a particular version of
  compiler (if available in your package manager)
- `--bit-depth 32`, to build a 32-bit binary instead of 64-bit

### Release types

Build release types includes:

- **release**, optimizes the binary and disables some checks, such as
  assertions.
- **debug**, adds debug symbols and disables optimizations for ideal debugging.
  - You can run the resulting binary in the GNU debugger: `gdb /path/to/
    dosbox`, followed by `run mygame.bat`
- **pgotrain** adds Profile Guided Optimization (PGO) tracking instrumentation
  to the compiled binary.
  
  This allows the recording of profile statistics that can be used to compile a
  PGO-optimized binary. Note that PGO optimization is different from
  Automatic Feedback Directed Optimization (AutoFDO) mentioned below.

  After compiling your PGO binary, the build script presents instructions
  describing how to generate and use the profiling data.

- **warnmore**, displays additional helpful C and C++ warnings for developers.
- **fdotrain**, add tracing symbols used to generate AutoFDO sampling data.
- **$SANITIZER TYPE**, builds a binary instrumented with code to catch issues at
  runtime that relate to the type of sanitizer being used. For example: memory
  leaks, threading issues, and so on. This is for Linux and macOS only.
  
  - see `./scripts/build.sh --help` for a list of sanitizer-types that are
    available.
  - Run your binary like normal and it will generate output describing
    problematic behavior
  - Some sanitizers accept runtime options via an environment variables,
    such as `ASAN_OPTIONS`, described here:
    <https://github.com/google/sanitizers/wiki/AddressSanitizerFlags>

### Build Results, Rebuilding, and Cleaning

After building, your `dosbox` or `dosbox.exe` binary will reside inside
`./dosbox-staging/src/`.

The build script records the prior build type and will clean if needed between
builds.  To manually remove all intermediate object files and ephemeral
auto-tools outputs, run `make distclean`.

To additionally remove all files except for the repository files, use `git
clean -fdx`.

### CCache

The build script will make use of ccache, which saves compiled objects for
potential re-use in future builds (hence the name, "cache") to speed up build
times. If you performed the one-time installation step above, then you will
already have ccache installed.

Simply having `ccache` in your path is sufficient to use it; you do not
need to invasively symlink `/usr/bin/gcc` -> `ccache`.

The build script enables ccache's object compression, which significantly
reduces the size of the cache. It will also display cache statistics after each
build. To see more details, run `ccache -s`.

To learn more about ccache run `ccache -h`, and read
<https://ccache.dev/manual/latest.html>

### Optimization Modifiers

The following modifier flags can be added when building a **release** type:

- `-m lto`, optimize the entire object space instead of per-file (Only
  available on Mac and Linux)

- `-m fdo`, performs feedback directed optimizations (FDO) using an AutoFDO
  data set. Export the `FDO_FILE` variable with the full path to your merged
  FDO dataset. For example:

  - GCC: `export FDO_FILE=/full/path/to/current.afdo` and then build with:
  
    `./scripts/builds.sh -c gcc -t release -m fdo -m lto`

  - Clang: `export FDO_FILE=/full/path/to/current.profraw`, and then build
    with:

    `./scripts/builds.sh -c clang -t release -m fdo -m lto`

  The section below describes how to collect an AutoFDO dataset for GCC and
  Clang.

## AutoFDO Procedures

Feedback Directed Optimization (FDO) involves recording performance data from
the Linux kernel and using it to direct the compiler's optimizer.

### Prerequisites for AutoFDO

- An **Intel processor** that supports the last branch record (LBR) instruction.
- A Linux **kernel** built with Branch Profiling tracers enabled:

  ``` shell
  CONFIG_PM_TRACE=y
  CONFIG_TRACE_BRANCH_PROFILING=y
  CONFIG_BRANCH_TRACER=y
  ```

  These can be enable directly in your kernel's `.config` file or using `make
  menuconfig` via the following menu options:
  1. `Kernel hacking  --->`
  1. `[*] Tracers  --->`
  1. `Branch Profiling (Trace likely/unlikely profiler)`
  1. `(X) Trace likely/unlikely profiler`

- The **AutoFDO** software package. It may be available via your package
  manager or built from sources <https://github.com/google/autofdo>.

  - **Note about compiler versions** the autofdo binaries need to be compiled
    with the exact version of the compiler that will later be used to compile
    our final optimized version of dosbox-staging.
  
    So for example, if you install autofdo via package-manager, then it will be
    valid for the default version of gcc and clang also installed by your
    package manager.  Where as if you plan to build with  `gcc-<latest>`, then
    you will need to compile autofdo from sources using `gcc-<latest>` by
    pointing the `CC` and `CXX` environment variables to the newer gcc
    binaries.

  - **Note about clang** If you plan to compile with a version of clang newer
    than your package manager's default version, then you will need to compile
    autofdo from source and configure it with the corresponding version of
    `llvm-config`.  For example, if I want to build with clang-10, then I would
    configure autofdo with: `./configure --with-llvm=/usr/bin/llvm-config-10`.

  - The included `.github/scripts/build-autofdo.sh` script can be used to build
    and install autofdo, for example:

    - default GCC:

      `sudo .github/scripts/build-autofdo.sh`
    - newer GCC:

      ``` shell
      export CC=/usr/bin/gcc-9
      export CXX=/usr/bin/g++-9
      sudo .github/scripts/build-autofdo.sh
      ```

    - Clang version 10:
  
      `sudo .github/scripts/build-autofdo.sh`

- The **pmu-tools** software package, which can be downloaded from
  <https://github.com/andikleen/pmu-tools.> This is a collection of python
  scripts used to assist in capturing sampling data.

### Record Data for AutoFDO Builds

1. Ensure the custom Linux Kernel supporting LBR tracing is running.

2. Build `dosbox-staging` from source using the `fdotrain` target:
   `./scripts/build.h -c gcc -t fdotrain`

3. Record kernel sample profiles while running dosbox-staging:

    `/path/to/pmu-tools/ocperf.py record -F max -o "samples-1.prof" -b -e
    br_inst_retired.near_taken:pp -- /path/to/fdo-trained/dosbox ARGS`

   Where `samples-1.prof` is the file that will be filled with samples.

   Repeat this for multiple training runs, each time saving the output to a new
   `-o samples-N.prof` file.  Ideally you want to exercise all code paths in
   dosbox-staging (core types, video cards, video modes, sound cards, and audio
   codecs).

4. Convert your sample profiles into compiler-specific records using tools
   provided in the `autofdo` package:

   For GCC, run:
   - `create_gcov --binary=/path/to/fdo-trained/dosbox
     --profile=samples-1.prof -gcov=samples-1.afdo -gcov_version=1`

     ... for each `.prof` file, creating a corresponding `.afdo` file.

   - At this point, you now have an `.afdo` file for each `.prof` file. Merge
     the `.afdo`s into a single `curren.afdo` file with:

     `profile_merger -gcov_version=1 -output_file=current.afdo *.afdo`

   For Clang, run:

   - `create_llvm_prof --binary=/path/to/fdo-trained/dosbox
     --profile=samples-1.prof --out=samples-1.profraw`

     ... for each `*.prof` file, creating a corresponding `.profraw` file.

   - At this point, you now have a `.profraw` file for each `.prof` file. Merge
     them into a single `current.profraw` file with:
  
     `llvm-profdata-<version> merge -sample -output=current.profraw *.profraw`

### Build Using AutoFDO Data

You can now use your merged `.afdo` or `.profraw` file to build with the `-m
fdo` modifier by  placing your `current.afdo/.profraw` file in the repo's root
directory, or point to it using the FDO_FILE environment variable, and launch
the build with `./scripts/build.sh -c <gcc or clang> -t release -m lto -m fdo`.
