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


## Permissions and running

1. Allow the Terminal app to get keyboard events, which will let you
   launch DOSBox Staging from the command line.

   In **System Settings** &gt; **Privacy** &gt; **Input Monitoring** &gt; **Terminal** (enable)

2. Launch DOSBox Staging:

    ```shell
    cd src/dosbox-staging/
    ./build/dosbox
    ```


## Using FluidSynth and Slirp during local development

FluidSynth and Slirp are difficult and time-consuming to build, therefore they
are built in separate project as libraries which are then loaded
dynamically at runtime (think of it as a plugin system). These dynamic
libraries are injected into our official release packages in the CI builds
(including the dev builds), but you'll need to ensure they're available for
local development. See the README of the
[dosbox-staging-ext](https://github.com/dosbox-staging/dosbox-staging-ext)
project for further info.

This is one possible solution to make these libraries available for local
development. You only have to do this if you're working on something related
to the FluidSynth MIDI synth or NE2000 networking via Slirp.

1. Download the latest release ZIP from the
   [dosbox-staging-ext](https://github.com/dosbox-staging/dosbox-staging-ext)
   project.

2. Unpack the ZIP package; you'll find two sets of libraries inside for debug
   and release builds.

3. Copy the contents of the ZIP to a location outside of the CMake build
   directory. E.g., you can put them into `$REPO_DIR/lib`.

4. Assuming you're using the `debug-macos` CMake preset, do the following
   _after_ doing a successful build:

  ```
  cd $REPO_DIR
  ln -s $PWD/lib/debug build/debug-macos/Debug/lib
  xattr -r -d com.apple.quarantine lib                                                                         [
  ```

  > [!IMPORTANT]
  > The `xattr` command is very important! Without that, macOS Gatekeeper
  > won't let DOSBox Staging load the dynamic libraries at runtime when
  > enabling FluidSynth by setting `mididevice = fluidsynth` or NE2000
  > networking by `ne2000 = on`.

You'll only need to do this once after a successful build. If you delete the
CMake build folder, redo step 4.
