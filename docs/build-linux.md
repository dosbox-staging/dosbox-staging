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

### Installing build tools

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
- SDL 3.x
- SDL3_image
- asio
- SpeexDSP
- zlib-nG

### Clone DOSBox Staging

```shell
git clone https://github.com/dosbox-staging/dosbox-staging.git
```

### Configure and build

To create the debug build:

```shell
cd dosbox-staging
cmake --preset=debug-linux
cmake --build --preset=debug-linux
```

To create the optimised release build:

```shell
cd dosbox-staging
cmake --preset=release-linux
cmake --build --preset=release-linux
```

### Start DOSBox Staging

Once built, you can launch DOSBox Staging with the following commands.

Debug build:

```shell
./build/debug-linux/dosbox
```

Release build:

```shell
./build/release-linux/dosbox
```

## Building using vcpkg

### Installing build tools

- for Ubuntu:

```shell
sudo apt-get install git build-essential pkg-config cmake curl ninja-build \
             autoconf autoconf-archive automake bison libtool libgl1-mesa-dev \
             libsdl3-dev python3-venv
```

### Install the vcpkg tool

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
   `~/.bashrc`, so it persists across sessions):

    ```shell
    export VCPKG_ROOT=$HOME/vcpkg
    ```

### Clone DOSBox Staging

```shell
cd ~
git clone https://github.com/dosbox-staging/dosbox-staging.git
```

### Configure and build

To create the debug build:

```shell
cd dosbox-staging
cmake --preset=debug-linux-vcpkg
cmake --build --preset=debug-linux-vcpkg
```

To create the optimised release build:

```shell
cd dosbox-staging
cmake --preset=release-linux-vcpkg
cmake --build --preset=release-linux-vcpkg
```

### Start DOSBox Staging

Once built, you can launch DOSBox Staging with the following commands.

Debug build:

```shell
./build/debug-linux-vcpkg/dosbox
```

Release build:

```shell
./build/release-linux-vcpkg/dosbox
```

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
cmake -DOPT_SANITIZER=ON --preset=release-linux
cmake --build --preset=release-linux
```

For more information about sanitizers, check the `GCC` or `clang` documentation
on the `-fsanitize` option.

As sanitizer availability and performance are highly platform-dependent, you
might need to manually adapt the `SANITIZER_FLAGS` variable in `CMakeLists.txt`
to suit your needs.
