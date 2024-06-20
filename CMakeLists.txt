cmake_minimum_required(VERSION 3.26)

set(VCPKG_USE_HOST_TOOLS ON)

project(dosbox-staging
		LANGUAGES C CXX
		VERSION 0.82.0
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)

# TODO: Enable certain warnings-as-errors for at least MSVC, Clang & GCC

option(OPT_DEBUG "Enable debugging" $<IF:$<CONFIG:Debug>,ON,OFF>)
option(OPT_HEAVY_DEBUG "Enable heavy debugging" OFF)

if (OPT_HEAVY_DEBUG)
	set(OPT_DEBUG ON CACHE INTERNAL "")
endif()

include(CheckIncludeFile)
include(CheckIncludeFiles)
include(CheckSymbolExists)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	set(DOSBOX_PLATFORM_WINDOWS ON)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
	set(DOSBOX_PLATFORM_MACOS ON)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	set(DOSBOX_PLATFORM_LINUX ON)
else()
	message(FATAL_ERROR "Unknown system ${CMAKE_SYSTEM_NAME}")
endif()

# TODO: Proper compiler simulation detection (i.e. clang on MSVC)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	set(DOSBOX_COMPILER_CLANG ON)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	set(DOSBOX_COMPILER_GCC ON)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" AND CMAKE_CXX_SIMULATE_ID STREQUAL "" AND CMAKE_C_SIMULATE_ID STREQUAL "")
	set(DOSBOX_COMPILER_MSVC ON)
else()
	message(NOTICE "Unknown compiler")
endif()


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

file(READ src/config.h.in CONFIG_CONTENTS)
string(REPLACE "#mesondefine" "#cmakedefine" CONFIG_CONTENTS "${CONFIG_CONTENTS}")
string(REPLACE "#cmakedefine C_TARGETCPU" "#cmakedefine C_TARGETCPU @C_TARGETCPU@" CONFIG_CONTENTS "${CONFIG_CONTENTS}")
string(REPLACE "#cmakedefine CUSTOM_DATADIR" "#cmakedefine CUSTOM_DATADIR \"@CUSTOM_DATADIR@\"" CONFIG_CONTENTS "${CONFIG_CONTENTS}")

set_property(GLOBAL PROPERTY FILE_CONTENTS "${CONFIG_CONTENTS}")

function(setConfig01 VAR VALUE)
	get_property(fc GLOBAL PROPERTY FILE_CONTENTS)
	string(REPLACE "#cmakedefine ${VAR}" "#cmakedefine01 ${VAR}" fc "${fc}")
	set_property(GLOBAL PROPERTY FILE_CONTENTS "${fc}")
	set(${VAR} ${VALUE} PARENT_SCOPE)
endfunction()

if (CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64")
	set(C_TARGETCPU "X86_64")
else()
	message(FATAL_ERROR "Unknown processor ${CMAKE_SYSTEM_PROCESSOR}")
endif()

setConfig01(C_UNALIGNED_MEMORY ON)
setConfig01(C_PER_PAGE_W_OR_X ON)
setConfig01(C_DYNAMIC_X86 ON)
setConfig01(C_DYNREC OFF)
setConfig01(C_FPU ON)
setConfig01(C_FPU_X86 OFF)  # TODO: Only for x86

# Networking (TODO: Option & dependent on SDL2_Net)
setConfig01(C_MODEM ON)
setConfig01(C_IPX ON)

# More networking (TODO: Option)
setConfig01(C_SLIRP OFF)
setConfig01(C_NE2000 OFF)

setConfig01(C_OPENGL ON)
setConfig01(C_FLUIDSYNTH ON)    # TODO: Option
setConfig01(C_MT32EMU ON)       # TODO: Option

setConfig01(C_TRACY OFF)        # TODO: Option
setConfig01(C_DIRECTSERIAL ON)

if (OPT_DEBUG)
	setConfig01(C_DEBUG ON)

	if (OPT_HEAVY_DEBUG)
		setConfig01(C_HEAVY_DEBUG ON)
	endif()
endif()

# ManyMouse
setConfig01(C_MANYMOUSE OFF)    # TODO
setConfig01(SUPPORT_XINPUT2 OFF)    # TODO

# macOS
setConfig01(C_COREAUDIO "${DOSBOX_PLATFORM_MACOS}")
setConfig01(C_COREMIDI "${DOSBOX_PLATFORM_MACOS}")
setConfig01(C_COREFOUNDATION "${DOSBOX_PLATFORM_MACOS}")
setConfig01(C_CORESERVICES "${DOSBOX_PLATFORM_MACOS}")

# Linux
setConfig01(C_ALSA "${DOSBOX_PLATFORM_LINUX}")

# Windows
setConfig01(NOMINMAX ON)

setConfig01(C_HAS_BUILTIN_EXPECT OFF)   # Needs porting to `[[likely]]` and friends?

# TODO: Check other functions
check_symbol_exists(strnlen "string.h" HAVE_STRNLEN)

set(_USE_MATH_DEFINES ON)
set(CUSTOM_DATADIR " ")

set(project_name "${PROJECT_NAME}")
set(version "${PROJECT_VERSION}")

get_property(CONFIG_CONTENTS GLOBAL PROPERTY FILE_CONTENTS)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/config.h.in "${CONFIG_CONTENTS}")
configure_file(${CMAKE_CURRENT_BINARY_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/include/config.h @ONLY)

### end of config.h ###

if (DOSBOX_COMPILER_MSVC)
	add_compile_options("/experimental:newLambdaProcessor") # Avoids error https://developercommunity.visualstudio.com/t/lambda-fails-to-implicitly-capture-constexpr-value/610504
endif()

add_executable(dosbox src/main.cpp src/dosbox.cpp)
target_include_directories(dosbox PUBLIC include ${CMAKE_CURRENT_BINARY_DIR}/include)

# TODO: This happens on every execution, better to have a target with dependencies instead
add_custom_target(copy_assets
		COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_LIST_DIR}/contrib/resources ${CMAKE_CURRENT_BINARY_DIR}
)
add_dependencies(dosbox copy_assets)

find_package(PkgConfig REQUIRED)

find_package(SDL2 CONFIG REQUIRED)

#find_package(Tracy CONFIG REQUIRED)
#target_link_libraries(dosbox PRIVATE Tracy::TracyClient)

include_directories(include ${CMAKE_CURRENT_BINARY_DIR}/include)

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
