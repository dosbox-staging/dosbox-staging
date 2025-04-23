cmake_minimum_required(VERSION 3.24)

set(VCPKG_USE_HOST_TOOLS ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Generate compile_commands.json for use with LSPs")

project(dosbox-staging
		LANGUAGES C CXX
		VERSION 0.83.0
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)

# TODO: Enable certain warnings-as-errors for at least MSVC, Clang & GCC

# Disable the noisy narrowing and conversion warnings by default.
# Use the CHECK_NARROWING() macro to opt-in these checks on a per-file basis.
# See `include/checks.h` for details.
if (MSVC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4244")
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-conversion -Wno-narrowing")
endif()

option(OPT_DEBUG "Enable debugging" $<IF:$<CONFIG:Debug>,ON,OFF>)
option(OPT_HEAVY_DEBUG "Enable heavy debugging" OFF)

if (OPT_HEAVY_DEBUG)
	set(OPT_DEBUG ON CACHE INTERNAL "")
endif()

include(CheckIncludeFile)
include(CheckIncludeFiles)
include(CheckCXXSourceCompiles)
include(CheckSymbolExists)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	set(DOSBOX_PLATFORM_WINDOWS ON)
	set(WIN32 ON)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	set(DOSBOX_PLATFORM_MACOS ON)
	set(MACOSX ON)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	set(DOSBOX_PLATFORM_LINUX ON)
	set(LINUX ON)
else()
	message(FATAL_ERROR "Unknown system ${CMAKE_SYSTEM_NAME}")
endif()

# Enable color output for certain compilers
set(CMAKE_COLOR_DIAGNOSTICS ON)

find_package(Git)
if(Git_FOUND)
	execute_process(
			COMMAND ${GIT_EXECUTABLE} log -1 --format=%h
			WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
			OUTPUT_VARIABLE VCS_TAG
			OUTPUT_STRIP_TRAILING_WHITESPACE
	)
else()
	set(VCS_TAG "dirty")
endif()

check_include_file("unistd.h" HAS_UNISTD)
if (NOT HAS_UNISTD)
	configure_file(src/platform/visualc/unistd.h ${CMAKE_CURRENT_BINARY_DIR}/include/unistd.h @ONLY)
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

check_cxx_source_compiles("${TEST_CODE_BUILTIN_AVAILABLE}" HAVE_BUILTIN_AVAILABLE)

set(TEST_CODE_BUILTIN_CLEAR_CACHE "
int main() {
    char buffer[10];
    __builtin___clear_cache(buffer, buffer + 10);
    return 0;
}
")

check_cxx_source_compiles("${TEST_CODE_BUILTIN_CLEAR_CACHE}" HAVE_BUILTIN_CLEAR_CACHE)

if (CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" OR CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64")
	set(C_TARGETCPU "X86_64")
	set(C_DYNAMIC_X86 ON)
	set(C_DYNREC OFF)
	set(C_FPU_X86 ON)
	set(C_UNALIGNED_MEMORY ON)
elseif (CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64" OR CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
	set(C_TARGETCPU "ARMV8LE")
	set(C_DYNAMIC_X86 OFF)
	set(C_DYNREC ON)
	set(C_FPU_X86 OFF)
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

# More networking (TODO: Option)
set(C_SLIRP OFF)
set(C_NE2000 OFF)

set(C_OPENGL ON)
set(C_FLUIDSYNTH ON)    # TODO: Option
set(C_MT32EMU ON)       # TODO: Option

set(C_TRACY OFF)        # TODO: Option
set(C_DIRECTSERIAL ON)

if (OPT_DEBUG)
	set(C_DEBUG ON)

	if (OPT_HEAVY_DEBUG)
		set(C_HEAVY_DEBUG ON)
	endif()
endif()

# ManyMouse
set(C_MANYMOUSE OFF)    # TODO
set(SUPPORT_XINPUT2 OFF)    # TODO

# macOS
set(C_COREAUDIO "${DOSBOX_PLATFORM_MACOS}")
set(C_COREMIDI "${DOSBOX_PLATFORM_MACOS}")
set(C_COREFOUNDATION "${DOSBOX_PLATFORM_MACOS}")
set(C_CORESERVICES "${DOSBOX_PLATFORM_MACOS}")

# Linux
set(C_ALSA "${DOSBOX_PLATFORM_LINUX}")

# Windows
set(NOMINMAX ON)

set(C_HAS_BUILTIN_EXPECT OFF)   # Needs porting to `[[likely]]` and friends?

# TODO: Check other functions
check_symbol_exists(strnlen "string.h" HAVE_STRNLEN)
check_symbol_exists(mprotect "sys/mman.h" HAVE_MPROTECT)
check_symbol_exists(pthread_jit_write_protect_np "pthread.h" HAVE_PTHREAD_WRITE_PROTECT_NP)
check_symbol_exists(mmap "sys/mman.h" HAVE_MMAP)
check_symbol_exists(sys_icache_invalidate "libkern/OSCacheControl.h" HAVE_SYS_ICACHE_INVALIDATE)
check_symbol_exists(MAP_JIT "sys/mman.h" HAVE_MAP_JIT)

set(_USE_MATH_DEFINES ON)
set(CUSTOM_DATADIR " ")

set(project_name "${PROJECT_NAME}")
set(version "${PROJECT_VERSION}")

configure_file(src/config.h.in.cmake ${CMAKE_CURRENT_BINARY_DIR}/include/config.h @ONLY)

### end of config.h ###

add_executable(dosbox src/main.cpp src/dosbox.cpp)
target_include_directories(dosbox PUBLIC include ${CMAKE_CURRENT_BINARY_DIR}/include)

# TODO: This happens on every execution, better to have a target with dependencies instead
add_custom_target(copy_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/contrib/resources"
        "${CMAKE_CURRENT_BINARY_DIR}${RESOURCE_COPY_PATH}"
)
add_dependencies(dosbox copy_assets)

find_package(PkgConfig REQUIRED)

find_package(SDL2 CONFIG REQUIRED)

#find_package(Tracy CONFIG REQUIRED)
#target_link_libraries(dosbox PRIVATE Tracy::TracyClient)

include_directories(include src/libs src/gui ${CMAKE_CURRENT_BINARY_DIR}/include)

add_subdirectory(src)

target_link_libraries(dosbox PRIVATE
		$<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
		$<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
		libcapture
		libmisc
		libcpu
		libdos
		libfpu
)
