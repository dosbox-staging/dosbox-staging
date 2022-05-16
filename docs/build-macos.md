macOS builds can be created using the Meson buildsystem, compiled using
the Clang or GCC compilers, and provided with dependencies using the Homebrew
or MacPorts package managers.

We recommend using Homebrew and Clang, because Apple's Core SDKs can be
used only with Apple's fork of Clang compiler.

## Install Dependencies under macOS

Before installing either Homebrew or MacPorts, the Apple's Xcode tools need
to be installed and the license agreed to:

## Xcode Installation

1. Install the command line tools: `xcode-select --install`
    and accept the license agreement
2. Install software updates:
    **Apple menu** >
    **System Preferences** >
    **Software Update** >
    *"Updates are available: command line tools for Xcode"*
    Click **Update Now** to proceed.
3. Install build dependencies using either Homebrew or MacPorts.

### Install dependencies (Homebrew)

1. Install Homebrew: <https://brew.sh>.
2. Install dependencies and related tools:

    ``` shell
    brew install ccache cmake libpng meson libslirp \
                 opusfile pkg-config python3 sdl2 sdl2_net speexdsp
    ```

3. Add brew to your shell path:

    ``` shell
    echo 'eval "$(/opt/homebrew/bin/brew shellenv)"' >> "$HOME"/.zprofile
    eval "$(/opt/homebrew/bin/brew shellenv)"
    ```

### Install dependencies (MacPorts)

1. Install MacPorts: <https://www.macports.org/install.php>
2. Install dependencies and related tools:

    ``` shell
    sudo port -q install ccache cmake fluidsynth libpng libsdl2 \
              libsdl2_net meson opusfile pkgconfig python310 \
              libslirp speexdsp
    ```

## Build

Once you have depenendcies installed using either environment, clone and
enter the repository's directory:

``` shell
cd
mkdir -p src
cd src
git clone https://github.com/dosbox-staging/dosbox-staging.git
cd dosbox-staging
meson setup build
ninja -C build
```

See more build options in [BUILD.md](/BUILD.md).

## Permissions and Running

1. Allow the terminal to get keyboard events, which will let you
   launch dosbox from the command line.

    In System Settings > Privacy > Input Monitoring > Terminal (enable)

2. Launch DOSBox Staging:

    ``` shell
    cd src/dosbox-staging/
    ./build/dosbox
    ```

