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
  - [Windows Procedures](#windows-procedures)
    - [Install MSYS2](#install-msys2)
    - [Clone and Build a Windows Binary](#clone-and-build-a-windows-binary)
  - [Linux Procedures](#linux-procedures)
    - [Install Dependencies under Linux](#install-dependencies-under-linux)
    - [Build a Linux Binary](#build-a-linux-binary)
  - [Additional Tips](#additional-tips)
    - [Compiler variations](#compiler-variations)
    - [Build Results, Rebuilding, and Cleaning](#build-results-rebuilding-and-cleaning)
    - [CCache](#ccache)
    - [Optimization Modifiers](#optimization-modifiers)
  - [AutoFDO Procedures](#autofdo-procedures)
    - [Prerequisites for AutoFDO](#prerequisites-for-autofdo)
    - [Record Data for AutoFDO Builds](#record-data-for-autofdo-builds)
    - [Build Using AutoFDO Data](#build-using-autofdo-data)

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

## Additional Tips

### Compiler variations

The compiler, version, and bit-depth can be selected by passing the following
common options to the **list-build-dependencies.sh** and **build.sh** scripts:

- `--compiler clang` or `-c clang` to use CLang instead of GCC
- `--compiler-version 8` or `-v <version>` to specify a particular version of
  compiler (if available in your package manager)
- `--bit-depth 32`, to build a 32-bit binary instead of 64-bit

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

## Meson build snippets

### Make a debug build

Install dependencies listed in README.md. Installing `ccache` is optional,
but highly recommended - Meson will use it to speed up the build process.

Build steps :

``` shell
meson setup build

# for meson >= 0.54.0
meson compile -C build

# for older versions
ninja -C build
```

### Other build types

Meson supports several build types, appropriate for various situations:
`release` for creating optimized release binaries, `debug` (default) for
builds appropriate for development or `plain` for packaging.

``` shell
meson setup -Dbuildtype=release build
```
Detailed documentation: [Meson: Core options][meson-core]

[meson-core]: https://mesonbuild.com/Builtin-options.html#core-options

### Disabling unwanted dependencies

The majority of dependencies are optional and can be disabled during build.

For example, to compile without SDL2\_net and OpenGL dependencies try:

``` shell
meson setup -Duse_sdl2_net=false -Duse_opengl=false build
ninja -C build
```

See file [`meson_options.txt`](meson_options.txt) for list of all available
project-specific build options.

You can also run `meson configure` to see the list of *all* available
build options (including project-specific ones).

### Run unit tests

Prerequisites:

``` shell
# Fedora
sudo dnf install gtest-devel
```
``` shell
# Debian, Ubuntu
sudo apt install libgtest-dev
```
If `gtest` is not available/installed on the OS, Meson will download it
automatically.

Build and run tests:

``` shell
meson setup build
meson test -C build
```

### Run unit tests (with user-supplied gtest sources)

*Appropriate during packaging or when user is behind a proxy or without
internet access.*

Place files described in `subprojects/gtest.wrap` file in
`subprojects/packagecache/` directory, and then:

``` shell
meson setup --wrap-mode=nodownload build
meson test -C build
```

### Build test coverage report

Prerequisites:

``` shell
# Fedora
sudo dnf install gcovr lcov
```

Run tests and generate report:

``` shell
meson setup -Db_coverage=true build
meson test -C build
ninja -C build coverage-html
```

### Static analysis report

Prerequisites:

``` shell
# Fedora
sudo dnf install clang-analyzer
```
``` shell
# Debian, Ubuntu
sudo apt install clang-tools
```

Build and generate report:

``` shell
meson setup build
ninja -C build scan-build
```
