// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_CONFIG_H
#define DOSBOX_CONFIG_H

// Current DOSBox Staging version without 'v' prefix (e.g., 0.79.1,
// 0.81.1-alpha)
#define DOSBOX_VERSION "0.83.0-alpha"

// Minimum 5-char long Git hash of the build; can be longer to guarantee
// uniqueness (e.g., da3c5, c22ef8)
#define BUILD_GIT_HASH "38d4d4"


// Operating System
//

// Defined if compiling for OS from BSD family
/* #undef BSD */

// Defined if compiling for Linux (non-Android)
#define LINUX

// Defined if compiling for macOS
/* #undef MACOSX */

// Defined if compiling for Windows (any option)
#ifndef WIN32
/* #undef WIN32 */
#endif


// CPU and FPU emulation
//
// These defines are mostly relevant to modules src/cpu/ and src/fpu/

// The type of cpu this target has
#define C_TARGETCPU X86_64

// Define to 1 if target CPU supports unaligned memory access
#define C_UNALIGNED_MEMORY 1

// Define to 1 if the target platform needs per-page dynamic core write or execute (W^X) tagging
#define C_PER_PAGE_W_OR_X 1

// Define to 1 to use x86/x86_64 dynamic cpu core
// Can not be used together with C_DYNREC
#define C_DYNAMIC_X86 1

// Define to 1 to use recompiling cpu core
// Can not be used together with C_DYNAMIC_X86
#define C_DYNREC 0

// Define to 1 to enable floating point emulation
#define C_FPU 1

// Define to 1 to use  fpu core implemented in x86 assembler
#define C_FPU_X86 1

// TODO Define to 1 to use inlined memory functions in cpu core
#define C_CORE_INLINE 1


// Emulator features
//
// Turn on or off optional emulator features that depend on external libraries.
// This way it's easier to port or package on a new platform, where these
// libraries might be missing.

// Define to 1 to enable internal modem emulation (using SDL2_net)
#define C_MODEM 1

// Define to 1 to enable IPX over Internet networking (using SDL2_net)
#define C_IPX 1

// Enable serial port passthrough support
#define C_DIRECTSERIAL 1

// Define to 1 to use opengl display output support
#define C_OPENGL 1

// Define to 1 to enable the Tracy profiling server
#define C_TRACY 0

// Define to 1 to enable internal debugger (using ncurses or pdcurses)
#define C_DEBUG 0

// Define to 1 to enable heavy debugging (requires C_DEBUG)
#define C_HEAVY_DEBUG 0

// Define to 1 to enable MT-32 emulator
#define C_MT32EMU 1

// Define to 1 to enable mouse mapping support
#define C_MANYMOUSE 1

// ManyMouse optionally supports the X Input 2.0 protocol (regardless of OS). It
// uses the following define to definitively indicate if it should or shouldn't
// use the X Input 2.0 protocol. If this is left undefined, then ManyMouse makes
// an assumption about availability based on OS type.
#define SUPPORT_XINPUT2 0

// Compiler supports Core Audio headers
#define C_COREAUDIO 0

// Compiler supports Core MIDI headers
#define C_COREMIDI 0

// Compiler supports Core Foundation headers
#define C_COREFOUNDATION 0

// Compiler supports Core Services headers
#define C_CORESERVICES 0

// Define to 1 to enable ALSA MIDI support
#define C_ALSA 1


// Defines for checking availability of standard functions and structs.
//
// Sometimes available functions, structs, or struct fields differ slightly
// between operating systems.

// Define to 1 when zlib-ng support is provided by the system
#define C_SYSTEM_ZLIB_NG 0

// Defined if synchronous I/O multiplexing is available
#define HAVE_FD_ZERO

// Defined if function clock_gettime is available
#define HAVE_CLOCK_GETTIME

// Defined if function __builtin_available is available
/* #undef HAVE_BUILTIN_AVAILABLE */

// Defined if function __builtin___clear_cache is available
#define HAVE_BUILTIN_CLEAR_CACHE

// Defined if function mprotect is available
#define HAVE_MPROTECT

// Defined if function mmap is available
#define HAVE_MMAP

// Defined if mmap flag MAPJIT is available
/* #undef HAVE_MAP_JIT */

// Defined if function pthread_jit_write_protect_np is available
/* #undef HAVE_PTHREAD_WRITE_PROTECT_NP */

// Defined if function sys_icache_invalidate is available
/* #undef HAVE_SYS_ICACHE_INVALIDATE */

// Defined if function pthread_setname_np is available
/* #undef HAVE_PTHREAD_SETNAME_NP */

// Defind if function setpriority is available
#define HAVE_SETPRIORITY

// Defind if function strnlen is available
#define HAVE_STRNLEN

// field d_type in struct dirent is not defined in POSIX
// Some OSes do not implement it (e.g. Haiku)
/* #undef HAVE_STRUCT_DIRENT_D_TYPE */


// Available headers
//
// Checks for non-POSIX headers and POSIX headers not supported on Windows.

#define HAVE_LIBGEN_H
#define HAVE_NETINET_IN_H
#define HAVE_PWD_H
#define HAVE_STDLIB_H
#define HAVE_STRINGS_H
#define HAVE_SYS_SOCKET_H
#define HAVE_SYS_TYPES_H
#define HAVE_SYS_XATTR_H


// Hardware-related defines
//

// Define to 1 when host/target processor uses big endian byte ordering
/* #undef WORDS_BIGENDIAN */


// Windows-related defines
//

// Prevent <windows.h> from clobbering std::min and std::max
/* #undef NOMINMAX */

// Enables mathematical constants (such as M_PI) in Windows math.h header
// https://docs.microsoft.com/en-us/cpp/c-runtime-library/math-constants
/* #undef _USE_MATH_DEFINES */

// Modern MSVC provides POSIX-like routines, so prefer that over built-in
#define HAVE_STRNLEN

// Holds the CMAKE_INSTALL_DATADIR specified during project setup. This can
// be used as a fallback if the user hasn't populated their
// XDG_DATA_HOME or XDG_DATA_DIRS.
#define CUSTOM_DATADIR "/usr/local/share"


#endif
