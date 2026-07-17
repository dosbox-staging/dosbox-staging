# Building with Meson (legacy)

Prior to release 0.83.0, DOSBox Staging used the Meson build system. It has
since been replaced by CMake + vcpkg — see the
[Windows](build-windows.md), [macOS](build-macos.md), and
[Linux](build-linux.md) build guides for current instructions.

These notes are kept only for **bisecting or building old versions** of the
code that predate the CMake migration. They are no longer maintained and won't
work against the current `main` branch.

> [!NOTE]
> Checkouts from before the SDL3 migration depend on **SDL 2.x**, not SDL 3.
> Older checkouts also target an earlier C++ standard. Meson picks up the
> correct settings from the checked-out sources as long as you wipe the build
> directory before each build (see [Bisecting](#bisecting)).


## Prerequisites

- A C/C++ compiler supporting the C++ standard used by the checkout
- Meson >= 0.56 (with Ninja)
- SDL 2.x
- Opusfile

All other dependencies are optional. If a dependency is missing from the
system, Meson's wrap system builds a local static copy automatically, so the
minimal set above is usually enough to get a build going.

### Install dependencies

Linux (Debian/Ubuntu):

```shell
sudo apt install ccache build-essential meson libasound2-dev libatomic1 \
                 libpng-dev libsdl2-dev libasio-dev libopusfile-dev \
                 libfluidsynth-dev libslirp-dev libspeexdsp-dev libxi-dev
```

macOS (Homebrew):

```shell
xcode-select --install
brew install cmake ccache meson libpng sdl2 asio opusfile \
     fluid-synth libslirp pkg-config python3 speexdsp
```


## Configuring and building

Release build (all features enabled):

```shell
meson setup build/release
meson compile -C build/release
```

The binary is `build/release/dosbox`. It relies on resource files relative to
it, so run it from that location.

Debug build:

```shell
meson setup -Dbuildtype=debug build/debug
meson compile -C build/debug
```

Built-in debugger build (use `heavy` instead of `normal` for the heavy
debugger):

```shell
meson setup -Denable_debugger=normal build/debugger
meson compile -C build/debugger
```

Run `meson configure` to list all available setup options, or see
`meson_options.txt` for the project-specific ones.


## Bisecting

When bisecting or building old versions, run `meson setup --wipe` before every
build. This refreshes the build metadata to match the checked-out sources (such
as the C++ language standard), which otherwise causes spurious failures.

A handy alias for versions 0.77.0 and newer:

```shell
alias build_staging='meson setup --wipe build && ninja -C build'
```

Prior to version 0.77.0, the Autotools build system was used. A build script
shipped with those old versions can be used instead (pick your compiler):

```shell
./scripts/build.sh -c clang -t release
./scripts/build.sh -c gcc -t release
```


## Unit tests

```shell
meson setup -Dbuildtype=debug build/debug
meson test -C build/debug
```

If GTest and GMock aren't installed system-wide, Meson downloads them
automatically.


## Troubleshooting

- **Build fails in a subproject** — reset it, e.g. for FluidSynth:
  `meson subprojects update --reset fluidsynth`

- **Meson hangs on a low-memory machine** — limit parallelism by passing `-j1`
  to `meson compile` (useful on 1 GB systems like the Raspberry Pi).

- **Stale build area** — reset with:

    ```shell
    git checkout -f main
    git pull
    git clean -fdx
    ```
