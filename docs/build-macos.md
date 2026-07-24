# Building on macOS

macOS builds are created with the CMake build tool and the Clang compiler. The
build tools (CMake, Ccache, etc.) are provided by the Homebrew or MacPorts
package managers, while the library dependencies are built and provided by
[vcpkg](https://github.com/microsoft/vcpkg).

We recommend using CMake with presets because they're CI-tested and produce a
binary using consistent compiler flags. Run `cmake --list-presets` to list the
presets.

We recommend using Homebrew and Clang because Apple's Core SDKs can be used
only with Apple's fork of the Clang compiler.


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

You might need to run `sudo xcodebuild -license` as well to accept the license
agreements again if the CMake build step fails.


## Installing build tools

The library dependencies are handled by vcpkg (see [Installing
vcpkg](#installing-vcpkg) below), so you only need the build tools themselves
from Homebrew or MacPorts.

### Homebrew

1. Install Homebrew: <https://brew.sh>.

2. Install the build tools:

    ```shell
    brew install cmake ccache pkg-config python3
    ```

3. Add `brew` to your shell path:

    ```shell
    echo 'eval "$(/opt/homebrew/bin/brew shellenv)"' >> "$HOME"/.zprofile
    eval "$(/opt/homebrew/bin/brew shellenv)"
    ```

### MacPorts

1. Install MacPorts: <https://www.macports.org/install.php>

2. Install the build tools:

    ```shell
    sudo port install cmake ccache pkgconfig python314
    ```

## Installing vcpkg

The library dependencies are built and provided by
[vcpkg](https://github.com/microsoft/vcpkg). The CMake presets expect the
`VCPKG_ROOT` environment variable to point at your vcpkg checkout.

1. Clone vcpkg into your home directory:

    ```shell
    cd ~
    git clone https://github.com/microsoft/vcpkg.git
    ```

2. Bootstrap it:

    ```shell
    cd vcpkg
    ./bootstrap-vcpkg.sh -disableMetrics
    ```

3. Set `VCPKG_ROOT` in your shell (add it to your shell startup script, usually
   `~/.zprofile`, so it persists across sessions):

    ```shell
    export VCPKG_ROOT=$HOME/vcpkg
    ```

## Building

Once the build tools and vcpkg are installed, clone the repository:

```shell
git clone https://github.com/dosbox-staging/dosbox-staging.git
```

To build the debug version, execute the following from the repo root:

```shell
cmake --preset=debug-macos
cmake --build --preset=debug-macos
```

To build the release version:

```shell
cmake --preset=release-macos
cmake --build --preset=release-macos
```

> [!NOTE]
> The first configure step builds all library dependencies from source via
> vcpkg, which can take a while. Subsequent builds reuse the cached artifacts.

## Troubleshooting

- **No CMAKE_C_COMPILER could be found.** --- Make sure you don't have any
  pending Xcode updates that haven't been completed yet.

- **Random CMake errors**. --- You might need to run `sudo xcodebuild
  -license` to accept the license agreements again. This usually happens after
  an Xcode upgrade.


## Bisecting and building old versions

Versions prior to 0.83.0 used the Meson build system, and versions prior to
0.77.0 used Autotools. See the [Meson build guide](build-meson.md) for
instructions on building these older checkouts.


## Unit tests

Unit tests are built and run with CMake and `ctest`. See
[Running the unit tests](build-testing.md) for details.


## Offline documentation

Self-contained offline HTML documentation can optionally be built as part of the
CMake build. See [Building the offline documentation](build-documentation.md)
for details.


## Sanitizer build

There are two (mutually exclusive) sanitizer settings available:
- `OPT_SANITIZER` — detects memory errors and undefined behaviors
- `OPT_THREAD_SANITIZER` — data race detector

To use any of these, pass the appropriate option when configuring the project,
for example:

```shell
cmake -DOPT_SANITIZER=ON --preset=release-macos
cmake --build --preset=release-macos
```

For more information about sanitizers, check the `clang` documentation on the
`-fsanitize` option.

As sanitizer availability and performance are highly platform-dependent, you
might need to manually adapt the `SANITIZER_FLAGS` variable in `CMakeLists.txt`
to suit your needs.


## Permissions and running

When running DOSBox Staging for the first time, you'll get lots of pop-up
dialogs asking for granting DOSBox Staging access to various folders. Just
allow access to all these folders.


## Installing clang-format

We use [clang-format](https://clang.llvm.org/docs/ClangFormat.html) to ensure
the consistent formatting of our code. See the
[Code formatting](https://github.com/dosbox-staging/dosbox-staging/blob/main/docs/CONTRIBUTING.md#code-formatting)
section of our contributor guidelines for more info.


### Homebrew

```shell
brew install clang-format
```

### MacPorts

Unfortunately, it's not possible to install only clang-format with MacPorts.
The cleanest way is to install the entire [clang-20](https://ports.macports.org/port/clang-20/) package (this will take a while):

```shell
sudo port install clang-20
```

Once installed, clang-format will be available as `clang-format-mp-20`. But
it's usually more convenient to have it available under the standard
`clang-format` name, so we'll need to do a few more extra steps.

DOSBox Staging uses Apple clang, so we don't want to switch over to MacPorts
clang with [clang-select](https://ports.macports.org/port/clang_select/).
The best way is to create a symlink (aliases only work in interactive shells,
not in scripts):

Assuming `~/bin` is in your path (e.g., by having `export
PATH="$HOME/bin:$PATH"` in your `.zshrc`), run the following:

```shell
ln -s /opt/local/bin/clang-format-mp-20 ~/bin/clang-format
```

## Installing extra tools

You'll need a few extra tools installed if you want to generate the website
and documentation locally and run our various scripts used for linting,
packaging, etc.


### Homebrew

TODO


### MacPorts

The instructions are up to date for macOS Sequoia 15.5.

#### mkdocs

Tools used for building our website and documentation (see
[DOCUMENTATION.md](DOCUMENTATION.md)).


> [!NOTE]
> These commands will also set MacPorts Python and Pip as the system default.
> If you don't want that, you'll probably need to do some symlinking (figuring
> that out is an exercise for the reader :sunglasses:).

```shell
sudo port install python313 python_select-313
sudo port select --set python  python313
sudo port select --set python3 python313

sudo port install py-pip
sudo port select --set pip  pip313
sudo port select --set pip3 pip313

pip install -r extras/documentation/mkdocs-package-requirements.txt
```

You'll also need to add the Python programs installed by pip to the path (so
you can just execute `mkdocs` from the shell). Append the following to your
`.zshrc`:

```shell
export PATH="$PATH:$HOME/Library/Python/3.13/bin/"
```

#### sass

Only necessary if you want to make changes to our customised MkDocs Material
theme.

```shell
sudo port install npm11
sudo npm install -g sass
```

#### markdownlint

Used by `scripts/linting/verify-markdown.sh`.

> [!NOTE]
> These commands will also set MacPorts Ruby as the system default.
> If you don't want that, you'll probably need to do some symlinking (figuring
> that out is an exercise for the reader :sunglasses:).

```shell
sudo port install ruby34
sudo port select --set ruby ruby34
sudo gem update --system 3.6.9
sudo gem install mdl
```

#### shellcheck

Used by `scripts/linting/verify-bash.sh`.

```shell
sudo port install shellcheck
```

#### pylint

Used by `scripts/linting/verify-python.sh`. Make sure to set up MacPorts
Python as described in the **mkdocs** section first.

```shell
sudo port install py313-pylint
sudo port select --set pylint pylint313
```

#### GNU sed

Assuming `~/bin` is in your `PATH` (e.g., by having `export
PATH="$HOME/bin:$PATH"` in your `.zshrc`):

```shell
sudo port install gsed
ln -s /opt/local/bin/gsed ~/bin/sed
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


## Setting up local dev environment for code signing

The prerequisite for code signing is an Apple Developer membership. The
following steps describe how to set up the necessary certificates and
credentials in the Keychain for the [notarizer
script](/scripts/packaging/macos/notarize-macos-dmg.sh) for local code
signing.

### Import the dev certificates into the Keychain

1. Start XCode, then go to **Settings / Accounts**.

2. Click on the **+** button in the bottom left corner and select **Apple
   Account**.

3. Select the newly added account and click on **Manage Certificates** in the
   bottom right corner.

4. Click on the **+** button and add all available certificates (probably
   **Apple Development Certificates** is only one needed, but it doesn't hurt
   to add them all).

5. Quit XCode, then open Keychain Access and verify the new certificates and
   keys have been imported. Search for "developer" --- you should see a bunch
   of entries, including **Developer ID Application: <Your Name>
   (_<TEAM_ID>_)**. You might need to wait a minute or two for the entries to
   show up in the Keychain.

Your Team ID is the 10-character alphanumeric code in parentheses on your
certificate, e.g. **Developer ID Application: Your Name (X4MF6H9XZ6)**


### Store the authentication credentials in the Keychain

These instructions are adapted from [here](https://github.com/electron/notarize/blob/main/README.md).

1. Generate an app-specific password for your Apple ID [as described here](https://support.apple.com/en-us/102654).

2. Run the following command to store your credentials securely in the
   Keychain (you can change the new keychain profile name from
   `notary-tool-profile` to anything else you like):

       xcrun notarytool store-credentials "notary-tool-profile" \
           --apple-id "<your-apple-id>" \
           --team-id "<10-char-team-id>"
           --password "<app-specific-pw>"

   You should get the following input if all went well:

       Validating your credentials...
       Success. Credentials validated.
       Credentials saved to Keychain.
       To use them, specify `--keychain-profile "notary-tool-profile"`

3. **[Optional]** Create a new script to set the env vars required by the
   [notarizer script](/scripts/packaging/macos/notarize-macos-dmg.sh) script:

       export DEVELOPER_IDENTITY="Developer ID Application: Your Name (X4MF6H9XZ6)"
       export KEYCHAIN_PROFILE="notary-tool-profile"


## Code signing the application bundle

Once the local dev environment for code signing is set up, you can sign
the application bundle built on GitHub CI with the [notarizer
script](/scripts/packaging/macos/notarize-macos-dmg.sh).

