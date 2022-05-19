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

1. Install MSYS2: <https://www.msys2.org/wiki/MSYS2-installation/>

2. Install Git: `pacman -S git`

3. Clone and enter the repository's directory:

    ``` shell
    git clone https://github.com/dosbox-staging/dosbox-staging.git
    cd dosbox-staging
    ```

4. Update the pacman database and packages:
    - Open an MSYS2 console from your start menu.
    - Run `pacman -Syu`, answer `Y`, and let it run to completion.
    - Close your terminal when it's done.
    - Re-open the terminal and repeat the process.

5. Install the GCC and Clang runtime groups:

   Note: this will uninstall non-MinGW compilers followed by installing
   the GCC and Clang runtime groups along with Staging's dependencies.

    ``` shell
    pacman -R clang
    pacman -R gcc
    pacman -S $(cat packages/windows-msys2-clang-x86_64.txt packages/windows-msys2-gcc-x86_64.txt)
    ```

   Close your terminal when this finishes.

6. Open a toolchain-specific MinGW terminal:

    - **GCC**: _Start Menu > Programs > MSYS2 > MSYS2 MinGW x64_
    - **Clang**: _Start Menu > Programs > MSYS2 > MSYS2 MinGW Clang x64_

   You can then use those specific toolchains within the
   respecitive terminal.

7. Setup a GCC build from an *MSYS2 MinGW x64* terminal:

   ``` shell
   meson setup build/release-gcc -Dbuildtype=release -Db_asneeded=true \
     --force-fallback-for=fluidsynth,mt32emu,slirp \
     -Dtry_static_libs=fluidsynth,mt32emu,opusfile,png,slirp,speexdsp \
     -Dfluidsynth:try-static-deps=true
   ```

8. Setup a Clang build from an *MSYS2 MinGW Clang x64* terminal:

   ``` shell
   meson setup build/release-clang --native-file=.github/meson/native-clang.ini \
     -Dbuildtype=release -Db_asneeded=true \
     --force-fallback-for=fluidsynth,mt32emu,slirp \
     -Dtry_static_libs=fluidsynth,mt32emu,opusfile,png,slirp,speexdsp \
     -Dfluidsynth:try-static-deps=true
   ```

9. Compile:

   ``` shell
   meson compile -C build/release-gcc
   # or
   meson compile -C build/release-clang
   ```


10. Create a package of the binary and DLLs to a destination directory:

   ``` shell
   ./scripts/create-package.sh -p msys2 build/release-gcc ../dosbox-staging-gcc-pkg
   # or
   ./scripts/create-package.sh -p msys2 build/release-clang ../dosbox-staging-clang-pkg
   ```
