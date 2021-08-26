Windows builds can be created using:

- MSVC compiler, Visual Studio 2019 IDE suite, and vcpkg to provide dependencies
  *(recommended)*.
- The Clang or GCC compilers using the Meson buildsystem running within the
  MSYS2 environment to provide dependencies.

## Build using Visual Studio

1. Install Visual Studio Community 2019: <https://visualstudio.microsoft.com/>.
2. Install vcpkg: <https://github.com/Microsoft/vcpkg#quick-start-windows>.
3. Follow instructions in [README.md](/README.md).

### Create a Debugger build using Visual Studio

Note that the debugger imposes a significant runtime performance penalty.
If you're not planning to use the debugger then the steps above will help
you build a binary optimized for gaming.

1. Follow the steps above to setup a working build environment.
2. Install and integrate `pdcurses` using vcpkg:

    ``` shell
    cd \vcpkg
    .\vcpkg install --triplet pdcurses
    .\vcpkg integrate install
    ```

3. Edit `src\platform\visualc\config.h` and enable `C_DEBUG` and optionally
  `C_HEAVY_DEBUG` by setting them to `1` instead of `0`.
4. Select a **Release** build type in Visual Studio, and run the build.

## Build using MSYS2

1. Install MSYS2: <https://www.msys2.org/>
2. Install dependencies:

    ``` shell
    pacman -S meson mingw-w64-x86_64-ccache mingw-w64-x86_64-fluidsynth \
              mingw-w64-x86_64-gcc mingw-w64-x86_64-libpng \
              mingw-w64-x86_64-opusfile mingw-w64-x86_64-pkg-config \
              mingw-w64-x86_64-SDL2 mingw-w64-x86_64-SDL2_net \
              mingw-w64-x86_64-zlib
    ```

3. Clone and enter the repository's directory:

    ``` shell
    git clone --recurse-submodules https://github.com/dosbox-staging/dosbox-staging.git
    cd dosbox-staging
    ```

4. Follow instructions in [BUILD.md](/BUILD.md). Disable the dependencies
   that are problematic on your system.
