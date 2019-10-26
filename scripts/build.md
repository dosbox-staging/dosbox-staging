# DOSBox build script

A helper-script that builds DOSBox with varying compilers, release types, and versions
on MacOS, Linux, and Windows.

A second script, **list-build-dependencies.sh** prints a list of package and library
dependencies used to build and link DOSBox, customized for your hardware, operating system,
and selected compiler and its version.  You can use this script's output to install
those necessary packages.

## Requirements

- **Windows newer than XP**
    - **NTFS-based C:**, because msys2 doesn't work on FAT filesystems
	- **Chocolately**, to install msys2

- **MacOS** 10.x
	- **brew**, as the package manager

- **Ubuntu** 16.04 or newer
	- **apt**, as the package manager
	- **sudo**, to permit installation of depedencies (optional) 

- **Build dependencies (all operating systems)**
    - Per those listed by the accompanying list-build-dependencies.sh script

## Windows Installation and Usage

1. Download and install Chocolatey: https://chocolatey.org/install
1. Open a console and run Cholocatey's command line interface (CLI) to install msys2:  
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

1. Open a new console and clone the DOSBox staging repository:  
   `git clone https://github.com/dreamer/dosbox-staging.git`

1. [optional] Install the build tools and package dependencies, if not yet done so:
   ``` shell
   cd dosbox-staging
   SET CWD=%cd%
   bash -lc "pacman -S --noconfirm $($CWD/scripts/list-build-dependencies.sh)"
   ```

1. Launch the build script with default settings:
   ``` shell
   cd dosbox-staging
   SET CWD=%cd%
   bash -lc "$CWD/scripts/build.sh --bin-path /mingw64/bin --src-path $CWD"
   ```

## MacOS Installation and Usage

1. Download and install brew: https://brew.sh
1. Install git: `brew install git`
1. Clone the repository: `git clone https://github.com/dreamer/dosbox-staging.git`
1. Change directories into the repo: `cd dosbox-staging`
1. [optional] Install the build tools and package dependencies, if not yet done so:
   ``` shell
   brew update
   brew install $(./scripts/list-build-dependencies.sh)
   ```
1. Build DOSBox: `./scripts/build.sh`

## Linux (Ubuntu/Debian-based) Installation and Usage

1. Install git: `sudo apt install -y git`
1. Clone the repository: `git clone https://github.com/dreamer/dosbox-staging.git`
1. Change directories into the repo: `cd dosbox-staging`
1. [optional] Install the build tools and package dependencies, if not yet done so:
   ``` shell
   sudo apt update -y
   sudo apt install -y $(./scripts/list-build-dependencies.sh)
   ```
1. Build DOSBox: `./scripts/build.sh`

## Additional Tips

The compiler, version, and bit-depth can be selected by passing the following common
options to the **list-build-dependencies.sh** and **build.sh** scripts:
* `--compiler clang`, to use CLang instead of GCC
* `--compiler-version 8`, to use a specific version of compiler (if available in your package manager)
* `--bit-depth 32`, to build a 32-bit binary instead of 64-bit

After building, your binary will reside inside the src/ directory.

Build flags you might be interested in:
* `--release debug`, to build a binary containing debug symbols (instead of **fast** or **small**)
* `--lto`, perform optimizations across the entire object space instead of per-file (Only available on Mac and Linux)

The above flags are othogonal and thus can be mixed-and-matched as desired.

If you want to run multiple back-to-back builds from the same directory with different settings then
add the `--clean` flag to ensure previous objects and binaries are removed.

