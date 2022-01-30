## Minimum build requirements

  - C/C++ compiler with support for C++17
  - SDL >= 2.0.5
  - Opusfile
  - Meson >= 0.54.2, or Visual Studio Community Edition 2019 or 2022
  - OS that is mostly POSIX-compliant or up-to-date Windows system

All other dependencies are optional and can be disabled while configuring the
build (in `meson setup` step).

## OS-specific instructions

Instructions in this article assume you're using Linux or BSD but will work
on any modern system. Documentation for programmers using other systems:
[Windows], [macOS], [Haiku].

[Windows]: docs/build-windows.md
[macOS]: docs/build-macos.md
[Haiku]: docs/build-haiku.md

## Make a build with the built-in debugger

On Linux, BSD, macOS, or MSYS2: install the `ncurses` development library
with headers included (as opposed to the bare library), and then:

``` shell
# setup the default debugger
meson setup -Dbuildtype=release -Denable_debugger=normal build/debugger
# -or- setup the heavy debugger
meson setup -Dbuildtype=release -Denable_debugger=heavy build/debugger
# build
ninja -C build/debugger
```

If using Visual Studio, install `pdcurses` using vcpkg and change
the `C_DEBUG` and optionally the `C_HEAVY_DEBUG` lines inside
`src/platform/visualc/config.h`.

Default debugger:

``` c++
#define C_DEBUG 1
#define C_HEAVY_DEBUG 0
```

Heavy debugger:

``` c++
#define C_DEBUG 1
#define C_HEAVY_DEBUG 1
```

Then perform a release build.


## Meson build snippets

### Make a debug build

Install dependencies listed in [README.md](README.md).  Although `ccache` is
optional, we recommend installing it because Meson will use it to greatly speed
up builds.

Build steps:

``` shell
meson setup build
ninja -C build
```
Directory `build` will contain all compiled files.

### Other build types

Meson supports several build types, appropriate for various situations:
`release` for creating optimized release binaries, `debug` (default) for
for development or `plain` for packaging.

``` shell
meson setup -Dbuildtype=release build
```

For those interested in performing many different build types, separate
build/ directories (or subdirectories) can be used. This allows builds to
be organized by type as well as allows easy side-by-side comparison of
builds.

One thing to note: If you use the VSCode editor with the clangd plugin,
this plugin assumes Meson setup's "compile_commands.json" output file
always resides in the hardcoded build/ directory. To work-around this bug,
feel free to symlink this file from your active build directory into
the hardcoded build/ location.

Detailed documentation: [Meson: Core options][meson-core]

[meson-core]: https://mesonbuild.com/Builtin-options.html#core-options

### Disabling unwanted dependencies

The majority of dependencies are optional and can be disabled during build.

For example, to compile without OpenGL dependency try:

``` shell
meson setup -Duse_opengl=false build
ninja -C build
```

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
notation or using comma-separated notation (ie: `-Doption=value1,value2,value3`)
when the option supports multiple values.


### Run unit tests

Prerequisites:

``` shell
# Fedora
sudo dnf install gmock-devel gtest-devel
```
``` shell
# Debian, Ubuntu
sudo apt install libgtest-dev
```
If `gtest` is not available/installed on the OS, Meson will download it
automatically.

Build and run tests:

``` shell
meson setup build
meson test -C build
```

### Run unit tests (with user-supplied gtest sources)

*Appropriate during packaging or when user is behind a proxy or without
internet access.*

Place files described in `subprojects/gtest.wrap` file in
`subprojects/packagecache/` directory, and then:

``` shell
meson setup --wrap-mode=nodownload build
meson test -C build
```

### Build test coverage report

Prerequisites:

``` shell
# Fedora
sudo dnf install gcovr lcov
```

Run tests and generate report:

``` shell
meson setup -Db_coverage=true build
meson test -C build
ninja -C build coverage-html
```

Open the report with your browser:

``` shell
firefox build/meson-logs/coveragereport/index.html"
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
meson setup build
ninja -C build scan-build
```

### Make a sanitizer build

Recent compilers can add runtime checks for various classes of issues.
Compared to a debug build, sanitizer builds take longer to compile
and run slower, so are often reserved to exercise new features or
perform a periodic whole-program checkup.

If you're running Debian or Ubuntu, we recommend using Clang's latest
stable version using the setup script here: https://apt.llvm.org/

The following command uses Clang to create a sanitizer build checking
for address and behavior issues, two of the most common classes of
issues. See Meson's list of built-in options for other sanitizer types.

``` shell
meson setup --native-file=.github/meson/native-clang.ini \
  -Doptimization=0 -Db_sanitize=address,undefined build/sanitizer
ninja -C build/sanitizer
```

The directory `build/sanitizer` will contain the compiled files, which
will leave your normal `build/` files untouched.

Run the sanitizer binary as you normally would, then exit and look for
sanitizer mesasge in the log output.  If none exist, then your program
is running clean.
