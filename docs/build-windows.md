Windows builds can be created using:

- MSVC compiler, Visual Studio 2019 IDE suite, and vcpkg to provide dependencies
  *(recommended)*.
- The Clang or GCC compilers using the Meson buildsystem running within the
  MSYS2 environment to provide dependencies.

## Build using Visual Studio

1. Install Visual Studio Community 2019: <https://visualstudio.microsoft.com/>.
2. Install vcpkg: <https://github.com/Microsoft/vcpkg#quick-start-windows>.
3. Follow instructions in [README.md](/README.md).

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
    git clone https://github.com/dosbox-staging/dosbox-staging.git
    cd dosbox-staging
    ```

4. Follow instructions in [BUILD.md](/BUILD.md). Disable the dependencies
   that are problematic on your system.
