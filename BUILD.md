# Build Script

## Introduction

This script builds `dosbox-staging` with your choice of compiler, release
type, and additional options. It runs on MacOS, Linux, Windows, and possibly
other operating systems.

If this is your first time building dosbox-staging, then you will need to
install its development tools and dependencies, which is covered in the
notes below.

- [Build Script](#build-script)
  - [Introduction](#introduction)
  - [Linux Procedures](#linux-procedures)
    - [Install Dependencies under Linux](#install-dependencies-under-linux)
    - [Build a Linux Binary](#build-a-linux-binary)
  - [Additional Tips](#additional-tips)
    - [Compiler variations](#compiler-variations)
    - [Build Results, Rebuilding, and Cleaning](#build-results-rebuilding-and-cleaning)
    - [Optimization Modifiers](#optimization-modifiers)

## Linux Procedures

### Install Dependencies under Linux

1. Install git: `sudo apt install -y git`
1. Clone the repository: `git clone
   https://github.com/dosbox-staging/dosbox-staging.git`
1. Change directories into the repo: `cd dosbox-staging`
1. (🏁 first-time-only) Install dependencies based on your package manager.
   In this example, we use Ubuntu 20.04:

   `sudo apt install -y $(./scripts/list-build-dependencies.sh -m apt -c clang)`

   For other supported package managers, run:
   `./scripts/list-build-dependencies.sh`

### Build a Linux Binary

1. Build an optimized binary using various compilers:

- Clang: `./scripts/build.sh -c clang -t release -m lto`
- GCC (default version): `./scripts/build.sh -c gcc -t release -m lto`
- GCC (specific version, ie: 10): `./scripts/build.sh -c gcc -v 10 -t release -m
  lto`

  :warning: Raspberry Pi 4 users should avoid link-time-optimized builds for
  now. Simply drop the `-m lto` in your build line.

1. To build a debug binary, use `-t debug` in place of `-t release -m lto`.

## Additional Tips

### Compiler variations

The compiler, version, and bit-depth can be selected by passing the following
common options to the **list-build-dependencies.sh** and **build.sh** scripts:

- `--compiler clang` or `-c clang` to use CLang instead of GCC
- `--compiler-version 8` or `-v <version>` to specify a particular version of
  compiler (if available in your package manager)
- `--bit-depth 32`, to build a 32-bit binary instead of 64-bit

## Meson build snippets

### Make a debug build

Install dependencies listed in README.md. Installing `ccache` is optional,
but highly recommended - Meson will use it to speed up the build process.

Build steps :

``` shell
meson setup build

# for meson >= 0.54.0
meson compile -C build

# for older versions
ninja -C build
```

### Other build types

Meson supports several build types, appropriate for various situations:
`release` for creating optimized release binaries, `debug` (default) for
builds appropriate for development or `plain` for packaging.

``` shell
meson setup -Dbuildtype=release build
```
Detailed documentation: [Meson: Core options][meson-core]

[meson-core]: https://mesonbuild.com/Builtin-options.html#core-options

### Disabling unwanted dependencies

The majority of dependencies are optional and can be disabled during build.

For example, to compile without SDL2\_net and OpenGL dependencies try:

``` shell
meson setup -Duse_sdl2_net=false -Duse_opengl=false build
ninja -C build
```

See file [`meson_options.txt`](meson_options.txt) for list of all available
project-specific build options.

You can also run `meson configure` to see the list of *all* available
build options (including project-specific ones).

### Run unit tests

Prerequisites:

``` shell
# Fedora
sudo dnf install gtest-devel
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
