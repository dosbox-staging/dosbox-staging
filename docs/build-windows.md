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
2. Update packages: <https://www.msys2.org/wiki/MSYS2-installation/>
3. Install dependencies:

    ``` shell
    pacman -S git \
      mingw-w64-x86_64-meson \
      mingw-w64-x86_64-toolchain \
      mingw-w64-x86_64-ccache \
      mingw-w64-x86_64-pkgconf \
      mingw-w64-x86_64-ntldd \
      mingw-w64-x86_64-ncurses \
      mingw-w64-x86_64-glib2 \
      mingw-w64-x86_64-libpng \
      mingw-w64-x86_64-opusfile \
      mingw-w64-x86_64-SDL2 \
      mingw-w64-x86_64-SDL2_net \
      mingw-w64-x86_64-zlib
    ```
    The above dependencies install the MinGW-w64 64 bit dependencies. 
    Replace `x86_64` with `i686` for MinGW-w64 32 bit dependencies. 
    Prefix `clang-` to `x86_64` for clang 64 bit dependencies.

3. Clone and enter the repository's directory:

    ``` shell
    git clone https://github.com/dosbox-staging/dosbox-staging.git
    cd dosbox-staging
    ```
4. If you haven't already done so, switch to `MSYS2 MinGW 64-bit`, 
   `MSYS2 MinGW 64-bit` shells (available in Start menu), or for 
   Clang, you can launch from the command line with 
   `C:\msys64\msys2_shell.cmd -clang64`. Your shell should show 
   `MINGW64 ~`, `MINGW32 ~` or `CLANG64 ~` before proceeding to the 
   next step.

5. Setup the build:
   
   ``` shell
   meson setup build/release -Dbuildtype=release -Db_asneeded=true \
     --force-fallback-for=fluidsynth,mt32emu,slirp \
     -Dtry_static_libs=fluidsynth,mt32emu,opusfile,png,slirp \
     -Dfluidsynth:try-static-deps=true
   ```

6. Compile dosbox-staging:

   ``` shell
   meson compile -C build/release
   ```
   
7. Copy binary and required DLL files to destination directory:

   ``` shell
   ./scripts/create-package.sh -p msys2 build/release ../dosbox-staging-pkg
   ```
