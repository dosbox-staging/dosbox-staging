# DOSBox build script

A helper-script that builds DOSBox with varying compilers, release types, and versions
on MacOS, Linux, and Windows. The script can optionally install the necessary build
tools and depedencies.

## Requirements

- **Windows newer than XP**
    - **NTFS-based C:**, because msys2 doesn't work on FAT filesystems
	- **Chocolately**, to install msys2

- **MacOS** 10.x
	- **brew**, as the package manager

- **Ubuntu** 16.04 or newer
	- **apt**, as the package manager
	- **sudo**, to permit installation of depedencies (optional) 

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

1. (optional) If you're interested in a specific branch:
   * enter the repo directory: `cd dosbox-staging`
   * list the available branches: `git branch --all`
   * switch to a specific branch: `git checkout NAME_OF_BRANCH`

1. Launch the build script with default settings:
   ``` shell
   cd dosbox-staging
   SET CWD=%cd%
   bash -lc "$CWD/scripts/build.sh --install-deps --bin-path /mingw64/bin --src-path $CWD"
   ```

## MacOS Installation and Usage

1. Download and install brew: https://brew.sh
1. Install git: `brew install git`
1. Clone the repository: `git clone https://github.com/dreamer/dosbox-staging.git`
1. Change directories into the repo: `cd dosbox-staging`
1. Build DOSbox: `./scripts/build.sh --install-deps`

## Linux (Ubuntu/Debian-based) Installation and Usage

1. Install git: `sudo apt install -y git`
1. Clone the repository: `git clone https://github.com/dreamer/dosbox-staging.git`
1. Change directories into the repo: `cd dosbox-staging`
1. Build DOSbox: `sudo ./scripts/build.sh --install-deps`
   * Note: sudo is only needed with the --install-deps flags. If you are more
     comfortable installing packages yourself, you can see the list of packages
     with: `grep -A28 dependencies-linux scripts/build.sh`

## Additional Tips

You can omit the `--install-deps` flags on subsequent runs now that all build tools and library dependencies have been installed on your system.

After building, your binary will reside inside the src/ directory.

You can see addition build options by running the script with the -h or --help options. Flags you might be interested in:
* `--compiler clang`, to use CLang instead of GCC
* `--bit-depth 32`, to build a 32-bit binary instead of 64-bit
* `--release debug`, to build a binary containing debug symbols (instead of optimized)
* `--lto`, perform optimizations across the entire object space instead of per-file (Only available on Mac and Linux)
* `--clean`, purge the previous build. Very important if running multiple builds`

The above flags are othogonal and thus can be mixed-and-matched as desired.

If you want to run multiple back-to-back builds from the same directory with different settings,
add the `--clean` flag to ensure previous objects and binaries are removed.


