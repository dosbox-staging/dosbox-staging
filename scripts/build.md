# DOSBox Build Script

A script that builds DOSBox with your choice of compiler, release types, and
additional optimization options on MacOS, Linux, and Windows.

If this is the first time you are attempting to build DOSBox, then you need to
first install the development tools and DOSBox's development packages prior to
building. To help in this regard, the **list-build-dependencies.sh** script
prints a list of packages that you can use to install these dependencies.

Use of both scripts is described below.

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

1. Download and install Chocolatey: https://chocolatey.org/install
1. Open a console and run Cholocatey's command line interface (CLI) to install msys2 and git:  
   `choco install msys2 git --no-progress`

    ```
    Chocolatey v0.10.15
    Installing the following packages:
    msys2 git
    By installing you accept licenses for the packages.
    
   msys2 v20180531.0.0 [Approved]
   msys2 package files install completed. Performing other installation steps.
   Installing to: C:\tools\msys64
   Extracting 64-bit C:\ProgramData\chocolatey\lib\msys2\tools\msys2-base-x86_64-20180531.tar.xz to C:\tools\msys64...
   C:\tools\msys64
   Extracting C:\tools\msys64\msys2-base-x86_64-20180531.tar to C:\tools\msys64...
   C:\tools\msys64
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
   `./scripts/list-build-dependencies.sh -p msys2 | xargs pacman -S --noconfirm`

1. Launch the build script with default settings:  
   `./scripts/build/run.sh --bin-path /mingw64/bin`


## MacOS Installation and Usage

Builds on Mac can be performed with Clang or GCC.

If you only plan on only building with Clang, then follow the Brew installation steps.
If you're interested in building with GCC, then either Brew or MacPorts will work.
Both can be installed without conflicting with eachother.

Before installing either, the Xcode tools need to be installed and the license agreed to:

###  Xcode Installation
1. Install the command line tools: `xcode-select --install`
1. Accept the license agreement: `sudo xcodebuild -license`


### Brew Installation
1. Download and install brew per the instructions here: https://brew.sh
1. Update it with: `brew update`
1. Install git with: `brew install git`
1. Install DOSBox dependencies: `brew install $(./scripts/list-build-dependencies.sh -p brew)`


### MacPorts Installation

1. Build and install MacPorts along with DOSBox dependencies with the following sequence:

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

1. Clone the repository: `git clone https://github.com/dreamer/dosbox-staging.git`
1. Change directories into the repo: `cd dosbox-staging`
1. Build DOSBox:
    - Clang: `./scripts/build.sh --compiler clang --bin-path /usr/local/bin`
    - GCC (brew): `./scripts/build.sh --compiler-version 9 --bin-path /usr/local/bin`
    - GCC (macports): `./scripts/build.sh --compiler-version mp-9 --bin-path /opt/local/bin`

## Linux Installation

1. (🏁 first-time-only) Install dependencies based on your package manager; apt in this example:  
    `sudo apt install -y $(./scripts/list-build-dependencies.sh -p apt)`  
    For other supported package managers, run:  
    `./scripts/list-build-dependencies.sh --help`

1. Install git: `sudo apt install -y git`
1. Clone the repository: `git clone https://github.com/dreamer/dosbox-staging.git`
1. Change directories into the repo: `cd dosbox-staging`
1. Build DOSBox: `./scripts/build.sh`


## Additional Tips

The compiler, version, and bit-depth can be selected by passing the following common
options to the **list-build-dependencies.sh** and **build.sh** scripts:
* `--compiler clang`, to use CLang instead of GCC
* `--compiler-version 8`, to use a specific version of compiler (if available in your package manager)
* `--bit-depth 32`, to build a 32-bit binary instead of 64-bit

After building, your `dosbox` or `dosbox.exe` binary will reside inside `./dosbox-staging/src/`.

Build flags you might be interested in:
* `--lto`, perform optimizations across the entire object space instead of per-file (Only available on Mac and Linux)
* `--release debug`, to build a binary containing debug symbols
    * You can run the resulting binary in the GNU debugger: `gdb /path/to/dosbox`, followed by `start mygame.bat`
* `--release profile`, to generate performance statistics
    * Instructions are provided after the build completes, which describe how to generate and process the profiling data
* `--release <sanitizer-type>`, to build a binary that performs dynamic code-analysis at runtime (Linux and macOS)
    * see `./scripts/build.sh --help` for a list of sanitizer-types that are available
    * Run your binary like normal and it will generate output describing problematic behavior
    * Some sanitizers accept runtime options via an environment variables, such as `ASAN_OPTIONS`, described here: https://github.com/google/sanitizers/wiki/AddressSanitizerFlags

If you want to run multiple back-to-back builds from the same directory with different settings then
add the `--clean` flag to ensure previous objects and binaries are removed.
