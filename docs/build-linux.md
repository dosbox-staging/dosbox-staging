# Building on Linux

Two build methods are available for Linux - using the [system libraries](#building-using-system-libraries)
provided by your Linux distribution or using the [vcpkg tool](#building-using-vcpkg)
to fetch and compile dependencies.

The vcpkg method is used by the team to provide our official binaries, which are
intended to be run on different distros - they only depend on glibc.

## Packaging

To create a distro package, use the [system libraries](#building-using-system-libraries)
build method. Pass the `-DOPT_TESTS=OFF` option to CMake when configuring the
project to skip building unit tests and get rid of the GTest dependency.

DOSBox Staging ships with some binary files as a part of its assets:

- `contrib/resources/drives/y` - important DOS commands which are not yet
  implemented internally.
  These are pre-built DOS executables, taken from the FreeDOS or other projects.
  To rebuild them, one might need legacy build tools, which are considered
  exotic today (like the OpenWatcom compiler), that's why they are not being
  compiled at build time.
  If shipping such binaries is against your distro policy, feel free to strip
  them away. DOSBox Staging will continue to work normally - the only real
  disadvantage for the end users is that they will have to provide them by
  themselves to run some game/software installers.

- `contrib/resources/freedos-cpi` - DOS screen fonts (CodePage Information
  files, CPI).
  These are bitmap fonts in a native MS-DOS format, taken from the FreeDOS
  project, painted by hand using a specialized font editor (so no source code
  exists for them).
  Probably all the other DOSBox forks use such files in some form; it's just
  most store them as an array of binary data, like [original DOSBox does](https://sourceforge.net/p/dosbox/code-0/HEAD/tree/dosbox/tags/RELEASE_0_74_3/src/dos/dos_codepages.h).
  Running DOSBox Staging without these files is completely unsupported; even if
  it seems to work for you, the internationalization features will malfunction.

- `contrib/resources/freedos-keyboard` - DOS keyboard layout definitions.
  Despite their extensions suggesting a DOS device driver, these are data files,
  not executables. They are, too, taken from the FreeDOS project; the binaries
  were created from the source `*.KEY` text files, using specialized tools,
  written in Pascal - they are, too, part of the FreeDOS project; search for
  `KEYB200S.ZIP`, `KEYB200X.ZIP`, `KC200S.ZIP`, and `KC200X.ZIP` files.
  Probably all the other DOSBox forks use such files in some form; it's just
  most store them as an array of binary data, like [original DOSBox does](https://sourceforge.net/p/dosbox/code-0/HEAD/tree/dosbox/tags/RELEASE_0_74_3/src/dos/dos_keyboard_layout_data.h).
  Running DOSBox Staging without these files is completely unsupported; even if
  it seems to work for you, the internationalization features will malfunction.

## Building using system libraries

These are generic, distro-independent building instructions.

### Install the necessary build tools

- GCC or Clang compiler (the compiler has to support C++20)
- Git
- CMake
- pkg-config

### Install the dependencies (development packages are needed, too)

- SDL 2.x
- SDL2_net
- IIR
- OpusFile
- MT32Emu
- FluidSynth
- ALSA
- libpng
- OpenGL headers
- GTest

### Clone DOSBox Staging

```bash
git clone https://github.com/dosbox-staging/dosbox-staging.git
```

### Configure and build

```bash
cd dosbox-staging
cmake --preset=release-linux
cmake --build --preset=release-linux
```

### Start DOSBox Staging

You can now launch DOSBox Staging with the command:

``` bash
./build/release-linux/dosbox
```

## Building using vcpkg

### Install the necessary build tools

- for Ubuntu:

```bash
sudo apt-get install git build-essential pkg-config cmake curl ninja-build \
             autoconf bison libtool libgl1-mesa-dev libsdl2-dev
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

```bash
cd dosbox-staging
cmake --preset=release-linux-vcpkg
cmake --build --preset=release-linux-vcpkg
```

### Start DOSBox Staging

You can now launch DOSBox Staging with the command:

```bash
./build/release-linux/dosbox
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

Unit tests are built by default, you can start them with the command:

```bash
./build/release-linux/tests/dosbox_tests
```

To disable building unit tests, pass the `-DOPT_TESTS=OFF` option when
configuring the project, for example:

```bash
cmake --preset=release-linux -DOPT_TESTS=OFF
```

## Sanitizer build

There are two (mutually exclusive) sanitizer settings available:
- `OPT_SANITIZER` - detects memory errors and undefined behaviors
- `OPT_THREAD_SANITIZER` - data race detector

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

## Building using Meson (legacy method, to be removed)

> **Note**
>
> The documentation in this section is mostly unmainained now.
> 
> **The Meson build system is going to be removed soon!**

### Minimum build requirements

Install dependencies listed in [README.md](README.md). Although `ccache` is
optional, we recommend installing it because Meson will use it to greatly speed
up builds. The minimum set of dependencies is:

- C/C++ compiler with support for C++20
- SDL >= 2.0.5
- Opusfile
- Meson >= 0.56

All other dependencies are optional and can be disabled while configuring the
build (in `meson setup` step).


### General notes

You can maintain several Meson build configurations using subdirectories, for
example, the **debug** and **release** configurations can reside in
`build/debug` and `build/release`, respectively.

We recommend setting the following Ccache environment variables to maximize
the use of storage and benefit from pre-compiled headers:

```shell
CCACHE_COMPRESS=true
CCACHE_COMPRESSLEVEL=6
CCACHE_SLOPPINESS="pch_defines,time_macros"
```

### Standard release build, all features enabled

``` shell
meson setup build/release
meson compile -C build/release
```

Your binary is `build/release/dosbox`.

The binary is supported by resource files relative to it, so we recommend
running it from it's present location.  However, if you want to package
up the binary along with its dependencies and resource tree, you can run:
`./scripts/packaging/create-package.sh` to learn more.


### Debug build (for code contributors or diagnosing a crash)

``` shell
meson setup -Dbuildtype=debug build/debug
meson compile -C build/debug
```

### Built-in debugger build

``` shell
meson setup -Denable_debugger=normal build/debugger
meson compile -C build/debugger
```

For the heavy debugger, use `heavy` instead of `normal`.


### Make a build with profiling enabled

Staging includes the [Tracy](https://github.com/wolfpld/tracy) profiler, which
is disabled by default. To enable it for Meson builds, set the `tracy` option
to `true`:

``` shell
meson setup -Dtracy=true build/release-tracy
meson compile -C build/release-tracy
```

We have instrumented a very small core of subsystem functions for baseline
demonstration purposes. You can add additional profiling macros to your functions
of interest.

The resulting binary requires the Tracy profiler server to view profiling
data. If using Meson on a *nix system, switch to
`subprojects/tracy.x.x.x.x/profiler/build/unix` and run `make`

Start the instrumented Staging binary, then start the server. You should see
Staging as an available client for Connect.

You can also run the server on a different machine on the network, even on a
different platform.

Please refer to the Tracy documentation for further information.


### Repository and package maintainers

By default, the Meson build system will lookup shared (or dynamic)
libraries using PATH-provided pkg-config or cmake.  If they're not
available or usable, Meson's wrap system will will build local static
libraries to meet this need.

If your environment requires all dependencies be provided by the system,
then you can disable the wrap system using Meson's setup flag:
`-Dwrap_mode=nofallback`.  However, this will require the operating
system satisfy all of DOSBox Staging's library dependencies.

Detailed documentation: [Meson: Core options][meson-core]

[meson-core]: https://mesonbuild.com/Builtin-options.html#core-options


#### Disabling dependencies

The majority of dependencies are optional and can be disabled. For example,
to compile without OpenGL:

``` shell
meson setup -Duse_opengl=false build
meson compile -C build
```

We highly recommend package maintainers only offer DOSBox Staging if all
its features (and dependencies) can be provided.


#### List Meson's setup options

Run `meson configure` to see the full list of Meson setup options as well
as project-specific options. Or, see the file
[`meson_options.txt`](meson_options.txt) for only the project-specific
options.

To query the options set in an existing build directory, simply append
the build directory to the above command. For example:

``` shell
meson configure build
```

Options can be passed to the `meson setup` command using `-Doption=value`
notation or using comma-separated notation (i.e.: `-Doption=value1,value2,value3`)
when the option supports multiple values.


#### If your build fails

1. Check if the `main` branch is also experiencing build failures
   [on GitHub](https://github.com/dosbox-staging/dosbox-staging/actions?query=event%3Apush+is%3Acompleted+branch%3Amain).
   If so, the maintenance team is aware of it and is working on it.

2. Double-check that all your dependencies are installed. Read the
   platform-specific documents above if needed.

3. If the build fails with errors from the compiler (gcc/clang/msvc)
   or linker, then please open a new issue.

4. If Meson reports a problem with a sub-package, try resetting it
   with `meson subprojects update --reset name-of-subpackage`. For example,
   to reset FluidSynth: `meson subprojects update --reset fluidsynth`.

5. If Meson hangs due to low memory availability, make sure to pass
   `-j1` to the `meson compile` command to limit parallel jobs. This is
   useful when compiling on Raspberry Pi like system with only 1GB of memory.

6. If that doesn't help, try resetting your build area with:

    ``` shell
    git checkout -f main
    git pull
    git clean -fdx
    ```

#### Run unit tests

Prerequisites:

``` shell
# Fedora
sudo dnf install gmock-devel gtest-devel
```

``` shell
# Debian, Ubuntu
sudo apt install libgtest-dev libgmock-dev
```

If GTest and GMock are not installed system-wide, Meson will download them
automatically.

Build and run tests:

``` shell
meson setup -Dbuildtype=debug build/debug
meson test -C build/debug
```


#### Run unit tests (with user-supplied gtest sources)

*Appropriate during packaging or when user is behind a proxy or without
internet access.*

Place files described in `subprojects/gtest.wrap` file in
`subprojects/packagecache/` directory, and then:

``` shell
meson setup -Dbuildtype=debug --wrap-mode=nodownload build/debug
meson test -C build/debug
```

Re-running a single GTest test over and over can be done with the below
command; this can be very useful during development.

``` shell
meson compile -C build/debug && ./build/debug/tests/<TEST_NAME>
```

To list the names of all GTest tests:

``` shell
meson test -C build/debug --list | grep gtest
```

To run a single GTest test case:

``` shell
./build/debug/tests/<TEST_NAME> --gtest_filter=<TEST_CASE_NAME>
```

Concrete example:

``` shell
./build/debug/tests/bitops --gtest_filter=bitops.nominal_byte
```


#### Bisecting and building old versions

To automate and ensure successful builds when bisecting or building old
versions, run `meson setup --wipe` on your build area before every build.

This updates the build area with critical metadata to match that of the
checked out sources, such as the C++ language standard.

An alias like the following can be used to build versions 0.77 and newer:

```shell
  alias build_staging='meson setup --wipe build && ninja -C build'
```

Prior to version 0.77, the Autotools build system was used. A build script
available in these old versions can be used (choose one for your compiler):

```shell
./scripts/build.sh -c clang -t release` or `./scripts/build.sh -c gcc -t release
```


#### Make a sanitizer build

Recent compilers can add runtime checks for various classes of issues.
Compared to a debug build, sanitizer builds take longer to compile
and run slower, so are often reserved to exercise new features or
perform a periodic whole-program check-up.

We recommend using Clang's latest stable version. If you're using a Debian or
Ubuntu-based distro, LLVM has a helpful one-time setup script
[here](https://apt.llvm.org/).

The following uses Clang's toolchain to create a sanitizer build that checks
for address and behavior issues, two of the most common classes of issues. See
Meson's list of built-in options for other sanitizer types.

``` shell
meson setup -Dbuildtype=debug --native-file=.github/meson/native-clang.ini \
  -Doptimization=0 -Db_sanitize=address,undefined build/sanitizer
meson compile -C build/sanitizer
```

The directory `build/sanitizer` will contain the compiled files, which
will leave your normal `build/` files untouched.

Run the sanitizer binary as you normally would, then exit and look for
sanitizer messages in the log output.  If none exist, then your program
is running clean.
