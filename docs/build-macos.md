macOS builds can be created using the Meson buildsystem, compiled using
the Clang or GCC compilers, and provided with dependencies using the Homebrew
or MacPorts package managers.

We're recommending using Homebrew and Clang, because Apple's Core SDKs can be
used only with Apple's fork of Clang compiler.

## Install Dependencies under macOS

Before installing either Homebrew or MacPorts, the Apple's Xcode tools need
to be installed and the license agreed to:

## Xcode Installation

1. Install the command line tools: `xcode-select --install`
2. Accept the license agreement: `sudo xcodebuild -license`
3. Install build dependencies using either Homebrew or MacPorts.

### Install dependencies (Homebrew)

1. Install Homebrew: <https://brew.sh>.
2. Install dependencies:

    ``` shell
    brew install ccache meson libpng sdl2 sdl2_net opusfile fluid-synth
    ```

### Install dependencies (MacPorts)

1. Install MacPorts: <https://www.macports.org/install.php>
2. Install dependencies:

    ``` shell
    sudo port -q install meson ccache libpng libsdl2 libsdl2_net \
                         opusfile fluidsynth
    ```

## Build

Once you have depenendcies installed using either environment, clone and
enter the repository's directory:

``` shell
git clone --recurse-submodules https://github.com/dosbox-staging/dosbox-staging.git
cd dosbox-staging
meson setup build
ninja -C build
```

Detailed instructions and build options are documented in [BUILD.md](/BUILD.md).
