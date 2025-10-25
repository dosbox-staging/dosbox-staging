cmake_minimum_required(VERSION 3.25)

set(VCPKG_USE_HOST_TOOLS ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL
    "Generate compile_commands.json for use with LSPs")

project(dosbox-staging
  LANGUAGES C CXX
  VERSION 0.83.0
)

set(DOSBOX_VERSION ${PROJECT_VERSION}-alpha)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)

# TODO Enable certain warnings-as-errors for at least MSVC, Clang & GCC

# Disable the noisy narrowing and conversion warnings by default. Use the
# CHECK_NARROWING() macro to opt-in these checks on a per-file basis. See
# `src/util/checks.h` for details.
if (MSVC)
  # /W4 displays level 1, level 2, and level 3 warnings, and all level 4
  #(informational) warnings that aren't off by default.
  add_compile_options("/wd4244" "/MP" "/W4")

  # Disable cl/clangcl warnings about insecure C functions
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)

else()

  # TODO: GCC 12 (still used in our CI workflow) complains about unrecognized
  # command line options; after switching to a newer release, enable the
  # following warning flags:
  #     -Winvalid-utf8
  #     -Wtrivial-auto-var-init
  #     -Wdeprecated-non-prototype (C only)
  #     -Wflex-array-member-not-at-end (C only)
  #     -Wfree-labels (C only)

  # Warning flags common to C and C++ code
  set(WARNING_FLAGS_COMMON
      "-Wall" "-Wextra" "-Wdeprecated"
      "-Wno-unknown-warning-option" "-Wno-unknown-pragmas"
      "-Wno-conversion" "-Wno-narrowing"
      "-Wdisabled-optimization" "-Wduplicated-branches" "-Wtrampolines" "-Wvla"
      "-Wlogical-op" "-Wduplicated-cond" "-Wpointer-arith"
      "-Wredundant-decls" "-Wfloat-conversion"
  )

  # Warning flags only valid for the C code
  set(WARNING_FLAGS_C
      ${WARNING_FLAGS_COMMON}
      "-Wjump-misses-init"
      "-Wnested-externs"
      "-Wold-style-definition"
      "-Wstrict-prototypes"
      "-Wvariadic-macros"
  )

  # Warning flags only valid for the C++ code
  set(WARNING_FLAGS_CXX
      ${WARNING_FLAGS_COMMON}
      "-Weffc++"
      "-Wextra-semi"
  )

  add_compile_options(
    "$<$<COMPILE_LANGUAGE:C>:${WARNING_FLAGS_C}>"
    "$<$<COMPILE_LANGUAGE:CXX>:${WARNING_FLAGS_CXX}>"
  )

endif()

# GCC needs more memory reserved to fully optimize the CPU emulation code
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  add_compile_options("--param" "max-gcse-memory=250000")
endif()

# TODO: These are sprintf deprecated warnings.
# We probably do want to remove sprintf usage at some point.
if (APPLE)
  add_compile_options("-Wno-deprecated-declarations")
endif()

option(OPT_DEBUGGER "Enable debugger" OFF)
option(OPT_HEAVY_DEBUGGER "Enable heavy debugger" OFF)
if (OPT_HEAVY_DEBUGGER)
  set(OPT_DEBUGGER ON CACHE INTERNAL "")
endif()

include(CheckIncludeFile)
include(CheckIncludeFiles)
include(CheckCXXSourceCompiles)
include(CheckSymbolExists)
include(GNUInstallDirs)

if (WIN32)
  set(DOSBOX_PLATFORM_WINDOWS ON)

elseif (APPLE)
  set(DOSBOX_PLATFORM_MACOS ON)
  set(MACOSX ON)

elseif (LINUX)
  set(DOSBOX_PLATFORM_LINUX ON)

else()
  message(FATAL_ERROR "Unknown system ${CMAKE_SYSTEM_NAME}")
endif()

# Sanitizers
option(OPT_SANITIZER        "Enable general sanitizer checks")
option(OPT_THREAD_SANITIZER "Enable thread sanitizer")

if (OPT_SANITIZER AND OPT_THREAD_SANITIZER)
  message(FATAL_ERROR
          "-DOPT_SANITIZER and -DOPT_THREAD_SANITIZER are mutually exclusive")
endif()

if (OPT_SANITIZER)
  if (DOSBOX_PLATFORM_MACOS)
    set(SANITIZER_FLAGS "address,pointer-compare,pointer-subtract,undefined")
  else()
    set(SANITIZER_FLAGS "address,pointer-compare,pointer-subtract,leak,undefined")
  endif()

elseif (OPT_THREAD_SANITIZER)
  if (DOSBOX_PLATFORM_MACOS)
    message(WARNING "Thread sanitizer does not work well on macOS")
  endif()
  set(SANITIZER_FLAGS "thread")
endif()

if (OPT_SANITIZER OR OPT_THREAD_SANITIZER)
  if (MSVC)
    message(FATAL_ERROR "Sanitizers are only supported for CLANG and GCC compilers")
  endif()

  if (DOSBOX_PLATFORM_WINDOWS)
    message(WARNING "Sanitizers were not tested on Windows")
  endif()

  # Disable optimizations, increase amount of debug info
  add_compile_options("-O0")
  add_compile_options("-fno-omit-frame-pointer")
  add_compile_options("-g")

  # Enable selected sanitizer mechanisms
  add_compile_options("-fsanitize=${SANITIZER_FLAGS}")
  add_link_options("-fsanitize=${SANITIZER_FLAGS}")

  # Ask to recover sanitizer output
  add_compile_options("-fsanitize-recover=all")
endif()

# Check host endianness
if (CMAKE_CXX_BYTE_ORDER STREQUAL BIG_ENDIAN)
  set(WORDS_BIGENDIAN ON)
endif()

# Enable color output for certain compilers
set(CMAKE_COLOR_DIAGNOSTICS ON)

find_package(Git)

if (Git_FOUND)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} rev-parse --short=5 HEAD
    WORKING_DIRECTORY         ${CMAKE_CURRENT_LIST_DIR}
    OUTPUT_VARIABLE           BUILD_GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
else()
  set(BUILD_GIT_HASH "?")
endif()

check_include_file("unistd.h" HAS_UNISTD)
if (NOT HAS_UNISTD)
  configure_file(
    src/platform/windows/visualc/unistd.h
    ${CMAKE_CURRENT_BINARY_DIR}/include/unistd.h
  )
endif()

# File-descriptor manipulation routines, such as FD_ZERO, are used
# by Enet, slirp, and ManyMouse's X11 interface. Unfortunately these
# routines aren't universally available.
if (DOSBOX_PLATFORM_WINDOWS)
  check_symbol_exists(FD_ZERO "winsock2.h"   HAVE_FD_ZERO)
else()
  check_symbol_exists(FD_ZERO "sys/select.h" HAVE_FD_ZERO)
endif()

set(TEST_CODE_BUILTIN_AVAILABLE "
int main() {
    if (__builtin_available(macOS 11, *)) {
        return 0;
    }
    return 0;
}
")

check_cxx_source_compiles("${TEST_CODE_BUILTIN_AVAILABLE}"
                          HAVE_BUILTIN_AVAILABLE)

set(TEST_CODE_BUILTIN_CLEAR_CACHE "
int main() {
    char buffer[10];
    __builtin___clear_cache(buffer, buffer + 10);
    return 0;
}
")

check_cxx_source_compiles("${TEST_CODE_BUILTIN_CLEAR_CACHE}"
                          HAVE_BUILTIN_CLEAR_CACHE)

if (CMAKE_SYSTEM_PROCESSOR  STREQUAL "AMD64"  OR
    CMAKE_SYSTEM_PROCESSOR  STREQUAL "x86_64" OR
    CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64")

  set(C_TARGETCPU        "X86_64")
  set(C_DYNAMIC_X86      ON)
  set(C_DYNREC           OFF)
  set(C_FPU_X86          ON)
  set(C_UNALIGNED_MEMORY ON)

elseif (CMAKE_SYSTEM_PROCESSOR  STREQUAL "arm64"   OR
        CMAKE_SYSTEM_PROCESSOR  STREQUAL "aarch64" OR
        CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")

  set(C_TARGETCPU        "ARMV8LE")
  set(C_DYNAMIC_X86      OFF)
  set(C_DYNREC           ON)
  set(C_FPU_X86          OFF)
  set(C_UNALIGNED_MEMORY OFF)

else()
  message(FATAL_ERROR "Unknown processor ${CMAKE_SYSTEM_PROCESSOR}")
endif()

check_include_file("libgen.h"     HAVE_LIBGEN_H)
check_include_file("netinet/in.h" HAVE_NETINET_IN_H)
check_include_file("stdlib.h"     HAVE_STDLIB_H)
check_include_file("strings.h"    HAVE_STRINGS_H)
check_include_file("sys/types.h"  HAVE_SYS_TYPES_H)
check_include_file("sys/xattr.h"  HAVE_SYS_XATTR_H)

check_symbol_exists(getpeername  sys/socket.h  HAVE_GETPEERNAME)
check_symbol_exists(getsockname  sys/socket.h  HAVE_GETSOCKNAME)
if (HAVE_GETPEERNAME AND HAVE_GETSOCKNAME)
  set(HAVE_SYS_SOCKET_H ON)
endif()

set(C_PER_PAGE_W_OR_X ON)
set(C_FPU ON)

# Networking (TODO: Option & dependent on SDL2_Net)
set(C_MODEM ON)
set(C_IPX ON)

option(OPT_OPENGL "Enable OpenGL render backend" ON)
if (OPT_OPENGL)
  set(C_OPENGL ON)
endif()

set(C_MT32EMU ON)       # TODO: Option

set(C_TRACY OFF)        # TODO: Option
set(C_DIRECTSERIAL ON)

if (OPT_DEBUGGER)
  set(C_DEBUGGER ON)

  if (OPT_HEAVY_DEBUGGER)
    set(C_HEAVY_DEBUGGER ON)
  endif()
endif()

# ManyMouse - optional XInput support
option(OPT_MANYMOUSE
       "Use ManyMouse library for single-computer multiplayer gaming in The Settlers I/II"
       ON
)

# ManyMouse - optional XInput support
option(OPT_XINPUT
       "Let ManyMouse use the X Input protocol"
       OFF
)

# ManyMouse
if (OPT_MANYMOUSE)
  if (NOT HAVE_FD_ZERO)
    message(WARNING "ManyMouse requires FD_ZERO support")

  elseif (DOSBOX_PLATFORM_MACOS)
    include (${CMAKE_CURRENT_SOURCE_DIR}/src/libs/manymouse/cmake/FindIOKit.cmake)
    if (NOT ${IOKit_FOUND})
      message(WARNING "ManyMouse requires IOKit")
    else()
      set(C_MANYMOUSE ON)
    endif()

  else()
    set(C_MANYMOUSE ON)
  endif()

  if (C_MANYMOUSE AND OPT_XINPUT)
    check_include_file("X11/extensions/XInput2.h" HAVE_XINPUT)
    include (${CMAKE_ROOT}/Modules/FindX11.cmake)
    if ((NOT ${X11_Xinput_FOUND}) OR (NOT HAVE_XINPUT))
      message(WARNING "XInput not found.")
    else()
      set(SUPPORT_XINPUT2 ON)
    endif()
  endif()
endif()

# Tests
option(OPT_TESTS "Enable tests" ON)

if (OPT_TESTS)
  enable_testing()
endif()

# macOS
set(C_COREAUDIO      "${DOSBOX_PLATFORM_MACOS}")
set(C_COREMIDI       "${DOSBOX_PLATFORM_MACOS}")
set(C_COREFOUNDATION "${DOSBOX_PLATFORM_MACOS}")
set(C_CORESERVICES   "${DOSBOX_PLATFORM_MACOS}")

# Linux
set(C_ALSA "${DOSBOX_PLATFORM_LINUX}")

# Windows
if (DOSBOX_PLATFORM_WINDOWS)
  # Prevent <windows.h> from clobbering std::min and std::max
  set(NOMINMAX ON)
  # Enable mathematical constants (such as M_PI) in Windows math.h header
  set(_USE_MATH_DEFINES ON)
endif()

check_symbol_exists(strnlen       "string.h"       HAVE_STRNLEN)
check_symbol_exists(clock_gettime "time.h"         HAVE_CLOCK_GETTIME)
check_symbol_exists(mprotect      "sys/mman.h"     HAVE_MPROTECT)
check_symbol_exists(mmap          "sys/mman.h"     HAVE_MMAP)
check_symbol_exists(MAP_JIT       "sys/mman.h"     HAVE_MAP_JIT)
check_symbol_exists(setpriority   "sys/resource.h" HAVE_SETPRIORITY)

check_symbol_exists(
  pthread_setname_np "pthread.h" HAVE_PTHREAD_SETNAME_NP
)
check_symbol_exists(
  pthread_jit_write_protect_np "pthread.h" HAVE_PTHREAD_WRITE_PROTECT_NP
)
check_symbol_exists(
  sys_icache_invalidate "libkern/OSCacheControl.h" HAVE_SYS_ICACHE_INVALIDATE
)

set(CUSTOM_DATADIR "${CMAKE_INSTALL_FULL_DATADIR}")

set(project_name "${PROJECT_NAME}")
set(version      "${PROJECT_VERSION}")

configure_file(
  src/dosbox_config.h.in.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/include/dosbox_config.h
)

# Allow dynamically loading externalised vcpkg dependencies at runtime
# from the $EXE_DIR/lib directory (must be set before 'add_executable')
if (APPLE)
  set(CMAKE_INSTALL_RPATH "@executable_path/lib")
else()
  set(CMAKE_INSTALL_RPATH "$ORIGIN/lib")
endif()
set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)

# Find common external dependencies
find_package(PkgConfig REQUIRED)
find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_net REQUIRED)
find_package(iir REQUIRED)
find_package(PNG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(MT32Emu REQUIRED)
find_package(FluidSynth REQUIRED CONFIG)
if (C_ALSA)
  find_package(ALSA REQUIRED)
endif()
pkg_check_modules(SPEEXDSP REQUIRED IMPORTED_TARGET speexdsp)

add_executable(dosbox src/main.cpp src/dosbox.cpp)
target_include_directories(
  dosbox PUBLIC include ${CMAKE_CURRENT_BINARY_DIR}/include
)

# Bundle licenses, but skip the ones that are not relevant for
# binary distribution or allow us to not distribute the license text.
set(LICENSE_FILES
    licenses/BSD-2-Clause.txt
    licenses/BSD-3-Clause.txt
    licenses/DEBUG.COM.txt
    licenses/GPL-2.0.txt
    licenses/LGPL-2.1.txt
    licenses/MIT.txt
    licenses/UNICODE.txt
    licenses/Zlib.txt)

# Set the directory with resources to bundle
set(RESOURCES_PATH "resources")

# Generate asset copy commands and add them as a dependency
include(cmake/add_copy_assets.cmake)
add_copy_assets()

#find_package(Tracy CONFIG REQUIRED)
#target_link_libraries(dosbox PRIVATE Tracy::TracyClient)

include_directories(
  src
  src/libs
  ${CMAKE_CURRENT_BINARY_DIR}/include
)

add_library(libdosboxcommon STATIC)


# Add tests before src so that the dosbox_tests target is
# available for target_sources()
if (OPT_TESTS)
  add_subdirectory(tests)
endif()

add_subdirectory(src)

# libatomic is part of the GCC runtime library.
# This is used by GCC and Clang by default on Linux.
# Mac and Windows don't need this except for maybe MSYS2.
# This is required for Linux to use all features of std::atomic.
find_library(LIBATOMIC libatomic.so.1)

if(LIBATOMIC)
  target_link_libraries(libdosboxcommon PRIVATE ${LIBATOMIC})
endif()

target_link_libraries(libdosboxcommon PRIVATE
  $<IF:$<TARGET_EXISTS:iir::iir>,iir::iir,iir::iir_static>
  loguru
  libdecoders
)

target_link_libraries(dosbox PRIVATE
  libdosboxcommon
  $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
  $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
)

# Add installation rules
include(cmake/add_install_rules.cmake)
add_install_rules()
