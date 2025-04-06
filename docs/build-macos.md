# Building on macOS

macOS builds can be created using the Meson buildsystem, compiled using the
Clang or GCC compilers, and provided with dependencies using the Homebrew or
MacPorts package managers.

We recommend using Homebrew and Clang because Apple's Core SDKs can be used
only with Apple's fork of the Clang compiler.

> **Note**
>
> CMake support is currently an experimental internal-only, work-in-progress
> feature; it's not ready for public consumption yet. Please ignore the
> `CMakeLists.txt` files in the source tree.


## Installing Xcode

Before installing either Homebrew or MacPorts, Apple's Xcode Command Line
Tools need to be installed and the license agreed to.

1. Install the command line tools: `xcode-select --install` and accept the
   license agreement

2. Install software updates:
    **Apple menu** &gt;
    **System Preferences** &gt;
    **Software Update** &gt;
    *"Updates are available: command line tools for Xcode"*.
    Click **Update Now** to proceed.

3. Install build dependencies using either Homebrew or MacPorts.


### Installing dependencies – Homebrew

1. Install Homebrew: <https://brew.sh>.

2. Install the minimum set of dependencies and related tools:

    ``` shell
    brew install cmake ccache meson sdl2 sdl2_net opusfile \
         pkg-config python3
    ```

3. Add `brew` to your shell path:

    ``` shell
    echo 'eval "$(/opt/homebrew/bin/brew shellenv)"' >> "$HOME"/.zprofile
    eval "$(/opt/homebrew/bin/brew shellenv)"
    ```

### Installing dependencies – MacPorts

1. Install MacPorts: <https://www.macports.org/install.php>

2. Install the minimum set of dependencies and related tools:

    ```shell
    sudo port -q install cmake ccache meson libsdl2 libsdl2_net opusfile \
              glib2 pkgconfig python311
    ```

## Building

Once you have dependencies installed using either environment, clone the
repository and enter its directory:

```shell
cd
mkdir -p src
cd src
git clone https://github.com/dosbox-staging/dosbox-staging.git
cd dosbox-staging
meson setup build
meson compile -C build
```

See more build options in [BUILD.md](/BUILD.md).


## Permissions and running

1. Allow the Terminal app to get keyboard events, which will let you
   launch DOSBox Staging from the command line.

   In **System Settings** &gt; **Privacy** &gt; **Input Monitoring** &gt; **Terminal** (enable)

2. Launch DOSBox Staging:

    ```shell
    cd src/dosbox-staging/
    ./build/dosbox
    ```

