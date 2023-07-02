# Build instructions

## Minimum build requirements

Install dependencies listed in [README.md](README.md).  Although `ccache` is
optional, we recommend installing it because Meson will use it to greatly speed
up builds. The minimum set of dependencies is:

- C/C++ compiler with support for C++17
- SDL >= 2.0.5
- Opusfile
- Meson >= 0.56, or Visual Studio Community Edition 2019 or 2022
- OS that is mostly POSIX-compliant or up-to-date Windows system


All other dependencies are optional and can be disabled while configuring the
build (in `meson setup` step).

## General notes

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

## OS-specific instructions

Instructions in this article assume you're using Linux or BSD but will work
on any modern system. Documentation for programmers using other systems:
[Windows], [macOS], [Haiku], [NixOS] .

[Windows]: docs/build-windows.md
[macOS]: docs/build-macos.md
[Haiku]: docs/build-haiku.md
[NixOS]: docs/build-nix.md

## Standard release build, all features enabled

``` shell
meson setup build/release
meson compile -C build/release
```

Your binary is `build/release/dosbox`.

If you're using Visual Studio, use the x86-64 release build target.

The binary is supported by resource files relative to it, so we recommend
running it from it's present location.  However, if you want to package
up the binary along with its dependencies and resource tree, you can run:
`./scripts/create-package.sh` to learn more.

## Debug build (for code contributors or diagnosing a crash)

``` shell
meson setup -Dbuildtype=debug build/debug
meson compile -C build/debug
```

## Built-in debugger build

``` shell
meson setup -Denable_debugger=normal build/debugger
meson compile -C build/debugger
```

For the heavy debugger, use `heavy` instead of `normal`.

If using Visual Studio set the `C_DEBUG` and optionally the
`C_HEAVY_DEBUG` values to `1` inside `src/platform/visualc/config.h`,
and then perform a release build.

## Make a build with profiling enabled

Staging includes the [Tracy](https://github.com/wolfpld/tracy) profiler, which
is disabled by default. To enable it for Meson builds, set the `tracy` option
to `true`:

``` shell
meson setup -Dtracy=true build/release-tracy
meson compile -C build/release-tracy
```

If using Visual Studio, select the `Tracy` build configuration.

We have instrumented a very small core of subsystem functions for baseline
demonstration purposes. You can add additional profiling macros to your functions
of interest.

The resulting binary requires the Tracy profiler server to view profiling
data. If using Meson on a *nix system, switch to
`subprojects/tracy.x.x.x.x/profiler/build/unix` and run `make`

If using Windows, binaries are available from the [releases](https://github.com/wolfpld/tracy/releases)
page, or you can build it locally with Visual Studio using the solution at
`subprojects/tracy.x.x.x.x/profiler/build/win32/Tracy.sln`

Start the instrumented Staging binary, then start the server. You should see
Staging as an available client for Connect.

You can also run the server on a different machine on the network, even on a
different platform, profiling Linux from Windows or vice versa, for example.

Please refer to the Tracy documentation for further information.

## Repository and package maintainers

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

### Disabling dependencies

The majority of dependencies are optional and can be disabled. For example,
to compile without OpenGL:

``` shell
meson setup -Duse_opengl=false build
meson compile -C build
```

We highly recommend package maintainers only offer DOSBox Staging if all
its features (and dependencies) can be provided.

### List Meson's setup options

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

### If your build fails

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

5. If that doesn't help, try resetting your build area with:

    ``` shell
    git checkout -f main
    git pull
    git clean -fdx
    ```

### Run unit tests

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

### Run unit tests (with user-supplied gtest sources)

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

### Build test coverage report

Prerequisite: Install Clang's `lcov` package and/or the GCC-equivalent `gcovr` package.

Build and run the tests:

``` shell
meson setup -Dbuildtype=debug -Db_coverage=true build/cov
meson compile -C build/cov
meson test -C build/cov
```

Generate and view the coverage report:

``` shell
cd build/cov
ninja coverage-html
cd meson-logs/coveragereport
firefox index.html
```

### Static analysis report

Prerequisites:

``` shell
# Fedora
sudo dnf install clang-analyzer
```

``` shell
# Debian, Ubuntu
sudo apt install clang-tools
```

Build and generate report:

``` shell
meson setup -Dbuildtype=debug build/debug
meson compile -C build/debug scan-build
```

### Make a sanitizer build

Recent compilers can add runtime checks for various classes of issues.
Compared to a debug build, sanitizer builds take longer to compile
and run slower, so are often reserved to exercise new features or
perform a periodic whole-program check-up.

- **Linux users:**:  We recommend using Clang's latest
stable version. If you're using a Debian or Ubuntu-based distro,
LLVM has a helpful one-time setup script [here](https://apt.llvm.org/).

- **Windows users:** Start by setting up MSYS2 as described in the
_docs/build-windows.md_document. Ensure you've opened an MSYS2 MinGW
Clang x64 terminal before proceeding and that `clang --version`
reports version 13.x (or greater).

The following uses Clang's toolchain to create a sanitizer build
that checks for address and behavior issues, two of the most common
classes of issues. See Meson's list of built-in options for other
sanitizer types.

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
