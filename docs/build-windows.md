# Building on Windows

Windows builds can be created using:

- Visual Studio 2022 IDE suite with Clang/LLVM compiler, and vcpkg to provide
  dependencies. This is the fully-supported toolchain used to create release
  builds.

- The Clang or GCC compilers using the Meson build system running within the
  MSYS2 environment to provide dependencies (deprecated, support will be probably
  removed in the future).

> **Note**
>
> CMake support is currently an experimental internal-only, work-in-progress
> feature; it's not ready for public consumption yet. Please ignore the
> `CMakeLists.txt` files in the source tree.


## Build using Visual Studio

1. Install Visual Studio Community 2022: <https://visualstudio.microsoft.com/vs/community/>.
    - Select "C++ Clang tools for Windows" in the Visual Studio installer

2. Install/configure vcpkg.
    - Recent versions of Visual Studio already include it[^1], but it needs to
      be initialized. Open a Visual Studio Developer Command Prompt and run the
      following command:

        ``` shell
        vcpkg integrate install
        ```

    - Existing [standalone versions](<https://github.com/Microsoft/vcpkg#quick-start-windows>) should work as well.

3. Follow instructions in [README.md](/README.md).

### Create a Debugger build using Visual Studio

Note that the debugger imposes a significant runtime performance penalty.
If you're not planning to use the debugger then the steps above will help
you build a binary optimized for gaming.

1. Edit `src\platform\visualc\config.h` and enable `C_DEBUG` and optionally
  `C_HEAVY_DEBUG` by setting them to `1` instead of `0`.

2. Select a **Release** build type in Visual Studio, and run the build.


## Build using MSYS2 (deprecated)

> **Note**
>
> MSYS2 builds are not actively maintained or tested by the development
> team. We will probably remove this section in the future.

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
   - Re-open the terminal and repeat the process one last time: Run `pacman -Syu`, answer `Y`, and let it run to completion.

5. Install the GCC and Clang runtime groups:

   Note: this will uninstall non-MinGW compilers followed by installing
   the GCC and Clang runtime groups along with Staging's dependencies.

    ``` shell
    cd dosbox-staging
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
   meson setup build/release-gcc
   ```

8. Setup a Clang build from an *MSYS2 MinGW Clang x64* terminal:

   ``` shell
   meson setup build/release-clang --native-file=.github/meson/native-clang.ini
   ```

   If building for Vista use instead:

   ``` shell
   meson setup -Duse_slirp=false build/release-clang --native-file=.github/meson/native-clang.ini
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

[^1]: <https://devblogs.microsoft.com/cppblog/vcpkg-is-now-included-with-visual-studio/>.
