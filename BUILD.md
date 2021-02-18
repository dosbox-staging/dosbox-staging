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
