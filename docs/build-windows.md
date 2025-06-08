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

