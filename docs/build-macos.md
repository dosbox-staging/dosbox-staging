# Building on macOS

macOS builds can be created using the Meson buildsystem, compiled using the
Clang or GCC compilers, and provided with dependencies using the Homebrew or
MacPorts package managers.

We recommend using Homebrew and Clang because Apple's Core SDKs can be used
only with Apple's fork of the Clang compiler.

> [!NOTE]
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


## Installing dependencies

### Homebrew

1. Install Homebrew: <https://brew.sh>.

2. Install the minimum set of dependencies and related tools:

    ```shell
    brew install cmake ccache meson sdl2 asio opusfile \
                 pkg-config python3
    ```

3. Add `brew` to your shell path:

    ```shell
    echo 'eval "$(/opt/homebrew/bin/brew shellenv)"' >> "$HOME"/.zprofile
    eval "$(/opt/homebrew/bin/brew shellenv)"
    ```

### MacPorts

1. Install MacPorts: <https://www.macports.org/install.php>

2. Install the minimum set of dependencies and related tools:

    ```shell
    sudo port install cmake ccache meson libsdl2 asio opusfile \
                      glib2 pkgconfig python311
    ```

## Building

Once you have dependencies installed using either environment, clone the
repository and enter its directory:

```shell
git clone https://github.com/dosbox-staging/dosbox-staging.git
cd dosbox-staging
meson setup build
meson compile -C build
```


## Offline documentation

Self-contained offline HTML documentation can optionally be built as part of the
CMake build. The output appears in the build directory at
`build/<preset>/Resources/docs/` — this is the same documentation bundled with
the release packages.

Documentation building is **off by default**. To enable it:

```bash
cmake --preset=debug-macos -DOPT_DOCUMENTATION=ON
cmake --build --preset=debug-macos
```

To rebuild just the documentation after editing content:

```bash
cmake --build --preset debug-macos --target rebuild_documentation
```

### Prerequisites

Python 3 with the `venv` module is required. Both ship with the Xcode Command
Line Tools and Homebrew — no extra packages are needed on macOS.

### Best-effort

If Python is not available or is missing required modules (`venv`, `ensurepip`),
the build proceeds normally without documentation — a warning is shown during
CMake configuration, but the build is **never aborted**.

### Caching

There are two independent cache layers that make successive builds fast:

1. **Python venv and pip packages** — stored in the build directory at
   `_mkdocs_venv/`. The virtual environment is created once per build directory.
   pip only re-runs when `extras/documentation/mkdocs-package-requirements.txt`
   is modified.

2. **Downloaded external assets** — the mkdocs-material privacy plugin caches
   downloaded web fonts, images, and scripts in `website/.cache/` in the source
   tree (this directory is git-ignored). Because it lives outside the build
   directory, it persists across clean builds and across different build
   configurations (debug, release, etc.).

> [!IMPORTANT]
> The privacy plugin only downloads assets from a small set of trusted
> sources: **Google Fonts** (fonts.googleapis.com, fonts.gstatic.com),
> **www.dosbox-staging.org** (our GitHub Pages website, completely under our
> control), and a few well-known CDNs used by the MkDocs Material theme
> (cdn.jsdelivr.net, unpkg.com, mirrors.creativecommons.org). No content from
> untrusted origins is ever fetched. The build uses the system CA certificate
> bundle instead of Python's built-in certifi bundle, so VPNs that perform
> SSL inspection work without issues. If the build fails with certificate
> errors, set the `SSL_CERT_FILE` environment variable to your
> organisation's CA bundle path.

### Rebuilding after documentation changes

Changes to markdown files under `website/docs/` do not automatically trigger a
rebuild — globbing hundreds of files into CMake's dependency tracking would be
impractical. To rebuild after editing documentation content:

```bash
# Option 1: Use the dedicated rebuild target
cmake --build --preset debug-macos --target rebuild_documentation

# Option 2: Invalidate the build stamp (triggers rebuild on next normal build)
touch website/mkdocs.yml
```

### Forcing a full rebuild

To rebuild documentation from scratch, delete the build stamp from the build
directory:

```bash
rm build/debug-macos/_mkdocs_build_stamp
```

### Cleaning all documentation caches

To remove all MkDocs caches from the source tree (`website/.cache`,
`website/__pycache__`, `website/site`):

```bash
cmake --build --preset debug-macos --target clean-manual
```


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

The instructions are up to date for macOS Sequioa 15.5.

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

### Sanitizer build (CMake)

There are two (mutually exclusive) sanitizer settings available:
- `OPT_SANITIZER` - detects memory errors and undefined behaviors
- `OPT_THREAD_SANITIZER` - data race detector

To use any of these, pass the appropriate option when configuring the sources,
for example:

```bash
cmake -DOPT_SANITIZER=ON --preset=release-macos
cmake --build --preset=release-macos
```

For more information about sanitizers check the `clang` documentation on the
`-fsanitize` option.

As sanitizer availability and performance are highly dependent on the concrete
platform (CPU, OS, compiler), you might need to manually adapt the
`SANITIZER_FLAGS` variable in the `CMakeLists.txt` file to suit your needs.


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

Once the the local dev environment for code signing is set up, you can sign
the application bundle built on GitHub CI with the [notarizer
script](/scripts/packaging/macos/notarize-macos-dmg.sh).

