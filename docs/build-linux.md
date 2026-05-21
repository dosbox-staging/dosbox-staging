# Building on Linux

Two build methods are available for Linux — using the [system libraries](#building-using-system-libraries)
provided by your Linux distribution or using the [vcpkg tool](#building-using-vcpkg)
to fetch and compile dependencies.

Both library options are available using CMake presets, which we highly
recommend because they're CI-tested and produce a binary using consistent
compiler flags. Run `cmake --list-presets` to list the presets.

The vcpkg presets are used by the team to provide our official binaries, which
are intended to be run on different distros — they only depend on glibc.

## Packaging

To create a distro package, use the [system libraries](#building-using-system-libraries)
build method. Pass the `-DOPT_TESTS=OFF` option to CMake when configuring the
project to skip building unit tests and get rid of the GTest dependency.

DOSBox Staging ships with some binary files as a part of its assets:

- `resources/drives/y` — important DOS commands which are not yet
  implemented internally.
  These are pre-built DOS executables, taken from the FreeDOS or other projects.
  To rebuild them, one might need legacy build tools, which are considered
  exotic today (like the OpenWatcom compiler), that's why they are not being
  compiled at build time.
  If shipping such binaries is against your distro policy, feel free to strip
  them away. DOSBox Staging will continue to work normally - the only real
  disadvantage for the end users is that they will have to provide them by
  themselves to run some game/software installers.

- `resources/freedos-cpi` — DOS screen fonts (CodePage Information
  files, CPI).
  These are bitmap fonts in a native MS-DOS format, taken from the FreeDOS
  project, painted by hand using a specialized font editor (so no source code
  exists for them).
  Probably all the other DOSBox forks use such files in some form; it's just
  most store them as an array of binary data, like [original DOSBox does](https://sourceforge.net/p/dosbox/code-0/HEAD/tree/dosbox/tags/RELEASE_0_74_3/src/dos/dos_codepages.h).
  Running DOSBox Staging without these files is completely unsupported; even if
  it seems to work for you, the internationalization features will malfunction.

- `resources/freedos-keyboard` — DOS keyboard layout definitions.
  Despite their extensions suggesting a DOS device driver, these are data files,
  not executables. They are, too, taken from the FreeDOS project; the binaries
  were created from the source `*.KEY` text files, using specialized tools,
  written in Pascal — they are, too, part of the FreeDOS project; search for
  `KEYB200S.ZIP`, `KEYB200X.ZIP`, `KC200S.ZIP`, and `KC200X.ZIP` files.
  Probably all the other DOSBox forks use such files in some form; it's just
  most store them as an array of binary data, like [original DOSBox does](https://sourceforge.net/p/dosbox/code-0/HEAD/tree/dosbox/tags/RELEASE_0_74_3/src/dos/dos_keyboard_layout_data.h).
  Running DOSBox Staging without these files is completely unsupported; even if
  it seems to work for you, the internationalization features will malfunction.

## Building using system libraries

These are generic, distro-independent building instructions.

### Install the necessary build tools

- GCC or Clang compiler (the compiler has to support C++23)
- Git
- CMake
- pkg-config
- Python 3 with the `venv` module (only needed if you want to build the
  [offline documentation](#offline-documentation); install `python3-venv` on
  Debian/Ubuntu)

### Install the dependencies (development packages are needed, too)

- ALSA
- FluidSynth
- GTest
- IIR
- libpng
- MT32Emu
- OpenGL headers
- OpusFile
- SDL 2.x
- SDL2_image
- asio
- SpeexDSP
- zlib-nG

### Clone DOSBox Staging

```bash
git clone https://github.com/dosbox-staging/dosbox-staging.git
```

### Configure and build

To create the debug build:

```bash
cd dosbox-staging
cmake --preset=debug-linux
cmake --build --preset=debug-linux
```

To create the optimised release build:

```bash
cd dosbox-staging
cmake --preset=release-linux
cmake --build --preset=release-linux
```

### Start DOSBox Staging

Once built, you can launch DOSBox Staging with the following commands.

Debug build:

``` bash
./build/debug-linux/dosbox
```

Release build:

``` bash
./build/release-linux/dosbox
```

## Building using vcpkg

### Install the necessary build tools

- for Ubuntu:

```bash
sudo apt-get install git build-essential pkg-config cmake curl ninja-build \
             autoconf autoconf-archive automake bison libtool libgl1-mesa-dev \
             libsdl2-dev python3-venv
```

### Install the vcpkg tool

- clone the vcpkg tool into your home directory:

```bash
cd ~
git clone https://github.com/microsoft/vcpkg.git
```

- bootstrap the vcpkg tool:

```bash
cd vcpkg
./bootstrap-vcpkg.sh -disableMetrics
```

- set the vcpkg path in your compile shell (it is recommended to add the command
  to your shell startup script, usually `~/.bashrc`):

```bash
export VCPKG_ROOT=$HOME/vcpkg
```

### Clone DOSBox Staging

```bash
cd ~
git clone https://github.com/dosbox-staging/dosbox-staging.git
```

### Configure and build

To create the debug build:

```bash
cd dosbox-staging
cmake --preset=debug-linux-vcpkg
cmake --build --preset=debug-linux-vcpkg
```

To create the optimised release build:

```bash
cd dosbox-staging
cmake --preset=release-linux-vcpkg
cmake --build --preset=release-linux-vcpkg
```

### Start DOSBox Staging

Once built, you can launch DOSBox Staging with the following commands.

Debug build:

``` bash
./build/debug-linux-vcpkg/dosbox
```

Release build:

``` bash
./build/release-linux-vcpkg/dosbox
```

## Bisecting and building old versions

Prior to release 0.83.0, the Meson build system was used. The following commands
can be used to configure and build the project:

```bash
meson setup -Dbuildtype=release build
meson compile -C build
```

Prior to release 0.77.0, the Autotools build system was used. A build script
available in these old versions can be used (choose one for your compiler):

```bash
./scripts/build.sh -c clang -t release` or `./scripts/build.sh -c gcc -t release
```

## Unit tests

Unit tests are built by default. To disable building unit tests, pass the
`-DOPT_TESTS=OFF` option when configuring the project, for example:

```bash
cmake --preset=release-linux -DOPT_TESTS=OFF
```

To run the entire test suite, execute the following (use the same CMake preset
you used for building):

```bash
ctest -j 8 --preset debug-linux
```

The `-j 8` option runs the tests in parallel on 8 CPU cores. You can adjust
this to suit your system.

To run all test cases in a single test suite, pass in the name of the suite
with the `-R` option:

```bash
ctest -j 8 --preset debug-linux -R DOS_FilesTest
```

You can narrow this down to run a single test case only:

```bash
ctest -j 8 --preset debug-linux -R DOS_FilesTest.DOS_MakeName_Basic_Failures
```

To run a group of tests, you can use wildcards and regexes. E.g. to run all
test cases in the `DOS_FilesTest` suite with names starting with
`DOS_MakeName_`:

```bash
ctest -j 8 --preset debug-linux -R "DOS_FilesTest.DOS_MakeName_*"
```

Pass in the `-V` option to see the DOSBox Staging log output:

```bash
ctest -j 8 --preset debug-linux -R DOS_FilesTest.DOS_MakeName_Basic_Failures -V
```

You might want to run the test executable directly to get coloured output, and
the option to start an interactive `gdb` session if a test crashes. For
example:

```
build/debug-linux/tests/dosbox_tests --gtest_filter=DOS_FilesTest.DOS_MakeName_Basic_Failures
```

See the [ctest documentation](https://cmake.org/cmake/help/v3.31/manual/ctest.1.html)
for the full list of available options.


## Offline documentation

Self-contained offline HTML documentation can optionally be built as part of
the CMake build. The output appears in the build directory at
`build/<preset>/resources/docs/` — this is the same documentation bundled
with the release packages.

Documentation building is **off by default**. To enable it:

```bash
cmake --preset=debug-linux -DOPT_DOCUMENTATION=ON
cmake --build --preset=debug-linux
```

To rebuild just the documentation after editing content:

```bash
cmake --build --preset debug-linux --target rebuild_documentation
```

### Prerequisites

Python 3 with the `venv` module is required. On Debian and Ubuntu, the `venv`
module is shipped in a separate package that may not be installed by default:

```bash
sudo apt-get install python3-venv
```

No other manual setup is needed — the build automatically creates a Python
virtual environment in the build directory and installs all MkDocs dependencies
into it.

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
cmake --build --preset debug-linux --target rebuild_documentation

# Option 2: Invalidate the build stamp (triggers rebuild on next normal build)
touch website/mkdocs.yml
```

### Forcing a full rebuild

To rebuild documentation from scratch, delete the build stamp from the build
directory:

```bash
rm build/debug-linux/_mkdocs_build_stamp
```

### Cleaning all documentation caches

To remove all MkDocs caches from the source tree (`website/.cache`,
`website/__pycache__`, `website/site`):

```bash
cmake --build --preset debug-linux --target clean-manual
```


## Sanitizer build

There are two (mutually exclusive) sanitizer settings available:
- `OPT_SANITIZER` — detects memory errors and undefined behaviors
- `OPT_THREAD_SANITIZER` — data race detector

To use any of these, pass the appropriate option when configuring the project,
for example:

```bash
cmake -DOPT_SANITIZER=ON --preset=release-linux
cmake --build --preset=release-linux
```

For more information about sanitizers, check the `GCC` or `clang` documentation
on the `-fsanitize` option.

As sanitizer availability and performance are are highly platform-dependent,
you might need to manually adapt the `SANITIZER_FLAGS` variable in
`CMakeLists.txt` file to suit your needs.
