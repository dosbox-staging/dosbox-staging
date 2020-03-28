# DOSBox Build Script

This script builds DOSBox with your choice of compiler, release type, and
additional options. It runs on MacOS, Linux, and Windows.

If this is the first time you are attempting to build DOSBox, then you need to
first install the development tools and DOSBox's dependencies. To get this list
of packages, run the **./scripts/list-build-dependencies.sh** script.

Usage instructions for the build script and dependencies script are described
below.

## Requirements

- **Windows newer than XP**
  - **NTFS-based C:**, because msys2 doesn't work on FAT filesystems
- **MacOS** 10.x
- **Ubuntu** 16.04 or newer
- **Fedora** up-to-date
- **RedHat or CentOS** 7 or newer
- **Arch-based distribution** up-to-date
- **OpenSUSE Leap** 15 or newer

## Windows Installation and Usage

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

1. Clone and enter the repository's directory:
   1. `git clone https://github.com/dreamer/dosbox-staging.git`
   1. `cd dosbox-staging`
   1. Run all subsequent steps while residing in the repo's directory.

1. (🏁 first-time-only) Install the build tools and package dependencies:
   `./scripts/list-build-dependencies.sh -p msys2 | xargs pacman -S
   --noconfirm`

1. Launch the build script with default settings:
   `./scripts/build/run.sh -c gcc -t release --bin-path /mingw64/bin`

## macOS Installation and Usage

Builds on macOS can be performed with Clang or GCC.

If you only plan on only building with Clang, then follow the Brew installation
steps. If you're interested in building with GCC, then either Brew or MacPorts
will work. Both can be installed without conflicting with eachother.

Before installing either, the Xcode tools need to be installed and the license
agreed to:

### Xcode Installation

1. Install the command line tools: `xcode-select --install`
1. Accept the license agreement: `sudo xcodebuild -license`

### Brew Installation

1. Download and install brew per the instructions here: <https://brew.sh>
1. Update it with: `brew update`
1. Install git with: `brew install git`
1. Install DOSBox dependencies:
   `brew install $(./scripts/list-build-dependencies.sh -p brew)`

### MacPorts Installation

1. Build and install MacPorts along with DOSBox dependencies with the following
   sequence:

   ``` shell
   git clone --quiet --depth=1 https://github.com/macports/macports-base.git
   cd macports-base
   ./configure
   make -j"$(sysctl -n hw.physicalcpu || echo 4)"
   sudo make install
   PREFIX="/opt/local"
   PATH="${PREFIX}/sbin:${PREFIX}/bin:${PATH}"
   sudo port -q selfupdate
   sudo port -q install $(/scripts/list-build-dependencies.sh -p macports)
   ```

### Build DOSBox (common for all of the above)

1. Clone the repository: `git clone
   https://github.com/dreamer/dosbox-staging.git`
1. Change directories into the repo: `cd dosbox-staging`
1. Build DOSBox:

- Clang: `./scripts/build.sh --compiler clang -t release --bin-path
  /usr/local/bin`
- GCC (brew): `./scripts/build.sh --compiler-version 9 -t release --bin-path
  /usr/local/bin`
- GCC (macports): `./scripts/build.sh --compiler-version mp-9 -t release
  --bin-path /opt/local/bin`

## Linux Installation

1. Install git: `sudo apt install -y git`
1. Clone the repository: `git clone
   https://github.com/dreamer/dosbox-staging.git`
1. Change directories into the repo: `cd dosbox-staging`
1. (🏁 first-time-only) Install dependencies based on your package manager; apt
   in this example: `sudo apt install -y $(./scripts/list-build-dependencies.sh
   -p apt)` For other supported package managers, run:
   `./scripts/list-build-dependencies.sh --help`
1. Build DOSBox:

- Clang: `./scripts/build.sh --compiler clang -t release -v 9`
- GCC (default version): `./scripts/build.sh -c gcc -t release`
- GCC (specific version, ie: 9): `./scripts/build.sh -c gcc -v 9 -t release`

## Additional Tips

### Compiler variations
The compiler, version, and bit-depth can be selected by passing the following
common options to the **list-build-dependencies.sh** and **build.sh** scripts:

- `--compiler clang`, to use CLang instead of GCC
- `--compiler-version 8`, to use a specific version of compiler
  (if available in your package manager)
- `--bit-depth 32`, to build a 32-bit binary instead of 64-bit

### Release types
Build release types includes:
- **release**, optimizes the binary and disables some checks, such as
  assertions.
- **debug**, adds debug symbols and disables optimizations for ideal debugging.
  - You can run the resulting binary in the GNU debugger: `gdb /path/to/
    dosbox`, followed by `start mygame.bat`
- **profile** add tracking symbols to generate profile statistics
  - Instructions are provided after the build completes, which describe how to
    generate and process the profiling data
- **warnmore**, displays additional helpful C and C++ warnings for developers.
- **fdotrain**, add tracing symbols used to generate feedback sampling data.
- **$SANITIZER TYPE**, to build a binary that performs dynamic code-analysis
  at runtime (Linux and macOS)
  - see `./scripts/build.sh --help` for a list of sanitizer-types that are
    available.
  - Run your binary like normal and it will generate output describing
    problematic behavior
  - Some sanitizers accept runtime options via an environment variables,
    such as `ASAN_OPTIONS`, described here:
    <https://github.com/google/sanitizers/wiki/AddressSanitizerFlags>

### Optimization Modifiers
The following modifier flags can be added when building a **release** type:

- `-m lto`, optimize the entire object space instead of per-file (Only
  available on Mac and Linux)

- `-m fdo`, perform feedback directed optimizations using an AutoFDO data set.
  To specific which data set to use, `export FDO_FILE=/full/path/to/some.afdo`
  for GCC or `export FDO_FILE=/full/path/to/some.profraw` for Clang.

If you want to run multiple back-to-back builds from the same directory with
different settings then add the `--clean` flag to ensure previous objects and
binaries are removed.

After building, your `dosbox` or `dosbox.exe` binary will reside inside `./dosbox-staging/src/`.

### Recording Sampling Data used in FDO builds

Prerequisites:

- An **Intel processor** that supports the last branch record (LBR) instruction.
- A **kernel** built with Branch Profiling tracers enabled:

  ```
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
  manager's or built from sources (https://github.com/google/autofdo).

  **Note** that the autofdo binaries need to be compiled with (or configured
  with when pointing it to `config-llvm`) the exact version of the compiler
  that will later be used to compile our final optimized version of
  dosbox-staging.
  
  So for example, if you install autofdo via package-manager, then it will be
  valid for the default version of gcc and clang also installed by your package
  manager.  Where as if you plan to use gcc-<latest>, then you will need to
  compile autofo from sources using gcc-<latest> as well.

- The **pmu-tools** software package. This can be downloaded from
  https://github.com/andikleen/pmu-tools, and is a set of useful python
  scripts.

Procedures:

1. Ensure the custom Linux Kernel supporting LBR tracing is running.
   
1. Build `dosbox-staging` from source using the `fdotrain` target:
   `./scripts/build.h -c gcc -t fdotrain`

1. Record kernel sample profiles while running dosbox-staging:

    `/path/to/pmu-tools/ocperf.py record -F max -o "samples-1.prof" -b -e
    br_inst_retired.near_taken:pp -- /path/to/fdo-trained/dosbox/binary ARGS`

   Where `samples-1.prof` is the file that will be filled with samples, and
   dosbox is the binary build in step 2.

   Repeat this for multiple training runs, each time saving the output to a new
   `-o samples-N.prof` file.  Ideally you want to exercise all code paths in
   DOSBox (core types, video cards, video modes, sound cards, and audio
   codecs).

1. Convert your sample profiles into compiler-specific records using tools
   provided in the `autofdo` package:
   
   For GCC, run:
   - `create_gcov --binary=/path/to/fdo-trained/dosbox/binary
     --profile=samples-1.prof -gcov=samples-1.afdo -gcov_version=1`
     
     ... for each `*.prof`.

   - When you now have a pile of `*.afdo` files, merge them into a single
     `curren.afdo` file with: `profile_merger -gcov_version=1
     -output_file=current.afdo *.afdo`

   For Clang, run:
   
   - `create_llvm_prof --binary=/path/to/fdo-trained/dosbox/binary
     --profile=samples-1.prof --out=samples-1.profraw

     ... for each `*.prof` file.

   - When you now have a pile of `*.profraw` files, merge them into a single
     `current.profraw` file with: `llvm-profdata-11 merge -sample
     -output=current.profraw *.profraw`

At this point, you have a merged .afdo or .profraw file that you can use in the
`-m fdo` target. Simply place your `current.afdo/.profraw` file in the repo
root directory (or point to it using the FDO_FILE environment variable), and
launch `./scripts/build.sh -c <gcc or clang> -t release -m lto -m fdo`.
