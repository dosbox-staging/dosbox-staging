cmake_minimum_required(VERSION 3.27)

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
# `include/checks.h` for details.
if(MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4244")
  
  # Disable cl/clangcl warnings about insecure C functions
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-conversion -Wno-narrowing")
endif()

option(OPT_DEBUG "Enable debugging" OFF)
option(OPT_HEAVY_DEBUG "Enable heavy debugging" OFF)

if(OPT_HEAVY_DEBUG)
  set(OPT_DEBUG ON CACHE INTERNAL "")
endif()

include(CheckIncludeFile)
include(CheckIncludeFiles)
include(CheckCXXSourceCompiles)
include(CheckSymbolExists)

if(WIN32)
  set(DOSBOX_PLATFORM_WINDOWS ON)

elseif(APPLE)
  set(DOSBOX_PLATFORM_MACOS ON)
  set(MACOSX ON)

elseif(LINUX)
  set(DOSBOX_PLATFORM_LINUX ON)

else()
  message(FATAL_ERROR "Unknown system ${CMAKE_SYSTEM_NAME}")
endif()

# Enable color output for certain compilers
set(CMAKE_COLOR_DIAGNOSTICS ON)

find_package(Git)

if(Git_FOUND)
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
if(NOT HAS_UNISTD)
  configure_file(
    src/platform/visualc/unistd.h
    ${CMAKE_CURRENT_BINARY_DIR}/include/unistd.h
  )
endif()

# File-descriptor manipulation routines, such as FD_ZERO, are used
# by Enet, slirp, and ManyMouse's X11 interface. Unfortunately these
# routines aren't universally available.
if(DOSBOX_PLATFORM_WINDOWS)
  check_symbol_exists(FD_ZERO  "winsock2.h"    HAVE_FD_ZERO)
else()
  check_symbol_exists(FD_ZERO  "sys/select.h"  HAVE_FD_ZERO)
endif()

### config.h shenanigans

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

if(CMAKE_SYSTEM_PROCESSOR  STREQUAL "AMD64"  OR
   CMAKE_SYSTEM_PROCESSOR  STREQUAL "x86_64" OR
   CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64")

  set(C_TARGETCPU        "X86_64")
  set(C_DYNAMIC_X86      ON)
  set(C_DYNREC           OFF)
  set(C_FPU_X86          ON)
  set(C_UNALIGNED_MEMORY ON)

elseif(CMAKE_SYSTEM_PROCESSOR  STREQUAL "arm64"   OR
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

check_include_file("sys/xattr.h" HAVE_SYS_XATTR_H)

set(C_PER_PAGE_W_OR_X ON)
set(C_FPU ON)

# Networking (TODO: Option & dependent on SDL2_Net)
set(C_MODEM ON)
set(C_IPX ON)

set(C_OPENGL ON)
set(C_MT32EMU ON)       # TODO: Option

set(C_TRACY OFF)        # TODO: Option
set(C_DIRECTSERIAL ON)

if(OPT_DEBUG)
  set(C_DEBUG ON)

  if(OPT_HEAVY_DEBUG)
    set(C_HEAVY_DEBUG ON)
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
  elseif(DOSBOX_PLATFORM_MACOS)
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

# macOS
set(C_COREAUDIO      "${DOSBOX_PLATFORM_MACOS}")
set(C_COREMIDI       "${DOSBOX_PLATFORM_MACOS}")
set(C_COREFOUNDATION "${DOSBOX_PLATFORM_MACOS}")
set(C_CORESERVICES   "${DOSBOX_PLATFORM_MACOS}")

# Linux
set(C_ALSA "${DOSBOX_PLATFORM_LINUX}")

# Windows
set(NOMINMAX ON)

# TODO Needs porting to `[[likely]]` and friends?
set(C_HAS_BUILTIN_EXPECT OFF)

# TODO Check other functions
check_symbol_exists(strnlen  "string.h"   HAVE_STRNLEN)
check_symbol_exists(mprotect "sys/mman.h" HAVE_MPROTECT)
check_symbol_exists(mmap     "sys/mman.h" HAVE_MMAP)
check_symbol_exists(MAP_JIT  "sys/mman.h" HAVE_MAP_JIT)

check_symbol_exists(
  pthread_jit_write_protect_np "pthread.h" HAVE_PTHREAD_WRITE_PROTECT_NP
)
check_symbol_exists(
  sys_icache_invalidate "libkern/OSCacheControl.h" HAVE_SYS_ICACHE_INVALIDATE
)

set(_USE_MATH_DEFINES ON)
set(CUSTOM_DATADIR " ")

set(project_name "${PROJECT_NAME}")
set(version      "${PROJECT_VERSION}")

configure_file(
  src/config.h.in.cmake ${CMAKE_CURRENT_BINARY_DIR}/include/config.h
)

### end of config.h ###

# Allow dynamically loading externalised vcpkg dependencies at runtime
# from the $EXE_DIR/lib directory (must be set before 'add_executable')
if (APPLE)
  set(CMAKE_INSTALL_RPATH "@executable_path/lib")
else()
  set(CMAKE_INSTALL_RPATH "$ORIGIN/lib")
endif()
set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)

add_executable(dosbox src/main.cpp src/dosbox.cpp)
target_include_directories(
  dosbox PUBLIC include ${CMAKE_CURRENT_BINARY_DIR}/include
)

# TODO This happens on every execution, better to have a target with
# dependencies instead
include(contrib/resources/Resources.cmake)
add_custom_target(copy_assets SOURCES ${DOSBOX_RESOURCE_FILES_ROOT} 
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/contrib/resources"
    "${CMAKE_CURRENT_BINARY_DIR}${RESOURCE_COPY_PATH}"
)
add_dependencies(dosbox copy_assets)

find_package(PkgConfig REQUIRED)

find_package(SDL2 CONFIG REQUIRED)

#find_package(Tracy CONFIG REQUIRED)
#target_link_libraries(dosbox PRIVATE Tracy::TracyClient)

include_directories(
  include src/libs src/gui ${CMAKE_CURRENT_BINARY_DIR}/include
)

add_subdirectory(src)

if(WIN32)
  target_sources(dosbox PRIVATE src/winres.rc)
endif()

target_link_libraries(dosbox PRIVATE
  $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
  $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
  libcapture
  libmisc
  libcpu
  libdos
  libfpu
)

### Install targets ###

# Set install directories
if(WIN32)
  set(CMAKE_INSTALL_BINDIR ".")
  set(CMAKE_INSTALL_SBINDIR ".")
  set(CMAKE_INSTALL_LIBDIR ".")
  set(CMAKE_INSTALL_INCLUDEDIR ".")
  set(CMAKE_INSTALL_SYSCONFDIR ".")
  set(CMAKE_INSTALL_SHARESTATEDIR ".")
  set(CMAKE_INSTALL_LOCALSTATEDIR ".")
  set(CMAKE_INSTALL_RUNSTATEDIR ".")
  set(CMAKE_INSTALL_DATADIR ".")
  set(CMAKE_INSTALL_INFODIR ".")
  set(CMAKE_INSTALL_LOCALEDIR ".")
  set(CMAKE_INSTALL_MANDIR ".")
  set(CMAKE_INSTALL_DOCDIR "doc")
  set(CMAKE_INSTALL_LIBEXECDIR ".")
elseif(NOT APPLE)
  include(GNUInstallDirs)
endif()

# Install Dosbox target
if(WIN32)
  install(
    TARGETS dosbox
    RUNTIME_DEPENDENCIES
    PRE_EXCLUDE_REGEXES "api-ms-" "ext-ms-"
    POST_EXCLUDE_REGEXES ".*system32/.*\\.dll"
    DIRECTORIES $<TARGET_FILE_DIR:dosbox>
  )
elseif(APPLE)
else()
  install(TARGETS dosbox)
endif()

# Install resource files
# Note, on MAC OS, resources can be installed as part of the bundle
if(NOT APPLE)
  foreach(_RES_FILE ${DOSBOX_RESOURCE_FILES})
    cmake_path(GET _RES_FILE PARENT_PATH OUT_DIR_COMPONENT)
    cmake_path(SET OUT_DIR NORMALIZE "${CMAKE_INSTALL_DATADIR}/$<IF:$<BOOL:${LINUX}>,${CMAKE_PROJECT_NAME},resources>/${OUT_DIR_COMPONENT}")
    install(
      FILES "contrib/resources/${_RES_FILE}" 
      DESTINATION "${OUT_DIR}"
    )  
  endforeach()
endif()

# Install license files
install(
  DIRECTORY licenses/
  DESTINATION "${CMAKE_INSTALL_DATADIR}/$<IF:$<BOOL:${LINUX}>,licenses/${CMAKE_PROJECT_NAME},licenses>"
)

include(extras/icons/Icons.cmake)

# OS Specific install rules
if(LINUX)
  # Install PNG icons
  while(DOSBOX_PNG_ICONS)
    list(POP_FRONT DOSBOX_PNG_ICONS ICON_FILE ICON_SIZE)
    install(
      FILES "${DOSBOX_PNG_ICON_DIR}/${ICON_FILE}"
      DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/${ICON_SIZE}x${ICON_SIZE}/apps"
      RENAME "org.dosbox_staging.dosbox_staging.png"
    )
  endwhile()
  # Install SVG icon
  install(
    FILES "${DOSBOX_SVG_ICON}"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps"
    RENAME "org.dosbox_staging.dosbox_staging.svg"
  )
  # Install desktop file
  install(
    FILES "contrib/linux/org.dosbox_staging.dosbox_staging.desktop"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
  )

  # Install metainfo file
  install(
    FILES "contrib/linux/org.dosbox_staging.dosbox_staging.metainfo.xml"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/metainfo"
  )
endif()