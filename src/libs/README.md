# Vendored third-party libraries

All code under `src/libs/` is vendored third-party code. It should be kept
as close to upstream as possible — avoid modifying vendored source files.

## Warning suppression

The project uses strict compiler warnings (`-Wall -Wextra -Weffc++` etc.)
for its own code. Two mechanisms suppress warnings from vendored libraries
so they don't obscure our own diagnostics.

### SYSTEM includes

Every library target marks its include directories as SYSTEM
(`target_include_directories(... SYSTEM ...)`). This tells the compiler to
treat the headers as system headers (`-isystem` on GCC/Clang, `/external:I`
on MSVC), suppressing all warnings originating from them.

This is the approach recommended by CMake's developers (Kitware) for
vendored third-party code:

- The [cmake-buildsystem(7)] manual documents SYSTEM as the mechanism for
  suppressing warnings from third-party includes.
- CMake [issue #18040] specifically requested a SYSTEM target property for
  in-tree vendored libraries, resulting in `add_subdirectory(... SYSTEM)`
  and the `SYSTEM` target property in CMake 3.25.
- CMake defaults imported targets' includes to SYSTEM treatment via the
  [SYSTEM target property], meaning the project treats third-party code as
  system headers by design.

[cmake-buildsystem(7)]: https://cmake.org/cmake/help/latest/manual/cmake-buildsystem.7.html
[issue #18040]: https://gitlab.kitware.com/cmake/cmake/-/issues/18040
[SYSTEM target property]: https://cmake.org/cmake/help/latest/prop_tgt/SYSTEM.html

For **header-only libraries**, this is a complete solution — their code
only exists in headers, and every inclusion by project code goes through
`-isystem`, so warnings are fully suppressed with no additional flags.

For **compiled libraries**, SYSTEM suppresses warnings when project code
includes their headers, but not when the library compiles its own sources
(the compiler finds its own headers locally, bypassing `-isystem`). The
`disable_warnings()` mechanism below covers this gap.

### disable_warnings() for compiled libraries

The `disable_warnings()` helper function (defined in this directory's
`CMakeLists.txt`) applies `-w` (GCC/Clang) or `/w` (MSVC) to a target,
suppressing all warnings during its own compilation. Each compiled
vendored library calls `disable_warnings()` in its own `CMakeLists.txt`.

This means vendored source files never need warning-suppression hacks like
`#pragma diagnostic`, `[[maybe_unused]]`, or `(void)param`. If a library
update introduces new warnings, they're automatically suppressed.

## Adding a header-only library

Create a `CMakeLists.txt` in the library's directory:

```cmake
add_library(mylib INTERFACE)
target_include_directories(mylib SYSTEM INTERFACE ..)
```

The `INTERFACE ..` exposes `src/libs/` as the include base so consumers
write `#include "mylib/header.h"`. The `SYSTEM` keyword suppresses
warnings.

Then link it where needed:

```cmake
target_link_libraries(dosboxcommon PRIVATE mylib)
```

And register the subdirectory in this directory's `CMakeLists.txt`:

```cmake
add_subdirectory(mylib)
```

## Adding a compiled library

Create a `CMakeLists.txt` in the library's directory:

```cmake
add_library(mylib STATIC source.cpp)
target_include_directories(mylib SYSTEM PUBLIC ..)

disable_warnings(mylib)
```

`PUBLIC ..` exposes headers to consumers (with SYSTEM). `disable_warnings()`
suppresses warnings during the library's own compilation. Then register the
subdirectory as above.

## Include conventions

- Use double quotes for vendored headers: `#include "libname/header.h"`
- Do not use angle brackets — those are for actual system headers
- Do not use a `libs/` prefix — includes resolve through the library
  targets, not through a global `src/libs` include path
