/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2024  The DOSBox Staging Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef DOSBOX_CONFIG_H
#define DOSBOX_CONFIG_H

// This file is a template for the CMake build process.
//
// Placeholders surrounded by '@' characters are substituted with concrete
// values at the start of the build process, and a file `config.cpp` is
// written to the build output directory.

// Git hash of the build
#define BUILD_GIT_HASH "@git_hash@"

/* Operating System
 */

// Defined if compiling for OS from BSD family
#cmakedefine BSD

// Defined if compiling for Linux (non-Android)
#cmakedefine LINUX

// Defined if compiling for macOS
#cmakedefine MACOSX

// Defined if compiling for Windows (any option)
#ifndef WIN32
#cmakedefine WIN32
#endif

/* CPU and FPU emulation
 *
 * These defines are mostly relevant to modules src/cpu/ and src/fpu/
 */

// The type of cpu this target has
#cmakedefine C_TARGETCPU @C_TARGETCPU@

// Define to 1 if target CPU supports unaligned memory access
#cmakedefine01 C_UNALIGNED_MEMORY

// Define to 1 if the target platform needs per-page dynamic core write or execute (W^X) tagging
#cmakedefine01 C_PER_PAGE_W_OR_X

// Define to 1 to use x86/x86_64 dynamic cpu core
// Can not be used together with C_DYNREC
#cmakedefine01 C_DYNAMIC_X86

// Define to 1 to use recompiling cpu core
// Can not be used together with C_DYNAMIC_X86
#cmakedefine01 C_DYNREC

// Define to 1 to enable floating point emulation
#cmakedefine01 C_FPU

// Define to 1 to use  fpu core implemented in x86 assembler
#cmakedefine01 C_FPU_X86

// TODO Define to 1 to use inlined memory functions in cpu core
#define C_CORE_INLINE 1

/* Emulator features
 *
 * Turn on or off optional emulator features that depend on external libraries.
 * This way it's easier to port or package on a new platform, where these
 * libraries might be missing.
 */

// Define to 1 to enable internal modem emulation (using SDL2_net)
#cmakedefine01 C_MODEM

// Define to 1 to enable IPX over Internet networking (using SDL2_net)
#cmakedefine01 C_IPX

// Enable serial port passthrough support
#cmakedefine01 C_DIRECTSERIAL

// Define to 1 to use opengl display output support
#cmakedefine01 C_OPENGL

// Define to 1 to enable FluidSynth integration (built-in MIDI synth)
#cmakedefine01 C_FLUIDSYNTH

// Define to 1 to enable libslirp Ethernet support
#cmakedefine01 C_SLIRP

// Define to 1 to enable Novell NE 2000 NIC emulation
#cmakedefine01 C_NE2000

// Define to 1 to enable the Tracy profiling server
#cmakedefine01 C_TRACY

// Define to 1 to enable internal debugger (using ncurses or pdcurses)
#cmakedefine01 C_DEBUG

// Define to 1 to enable heavy debugging (requires C_DEBUG)
#cmakedefine01 C_HEAVY_DEBUG

// Define to 1 to enable MT-32 emulator
#cmakedefine01 C_MT32EMU

// Define to 1 to enable mouse mapping support
#cmakedefine01 C_MANYMOUSE

// ManyMouse optionally supports the X Input 2.0 protocol (regardless of OS). It
// uses the following define to definitively indicate if it should or shouldn't
// use the X Input 2.0 protocol. If this is left undefined, then ManyMouse makes
// an assumption about availability based on OS type.
#cmakedefine01 SUPPORT_XINPUT2

// Compiler supports Core Audio headers
#cmakedefine01 C_COREAUDIO

// Compiler supports Core MIDI headers
#cmakedefine01 C_COREMIDI

// Compiler supports Core Foundation headers
#cmakedefine01 C_COREFOUNDATION

// Compiler supports Core Services headers
#cmakedefine01 C_CORESERVICES

// Define to 1 to enable ALSA MIDI support
#cmakedefine01 C_ALSA

/* Compiler features and extensions
 *
 * These are defines for compiler features we can't reliably verify during
 * compilation time.
 */

#cmakedefine01 C_HAS_BUILTIN_EXPECT

/* Defines for checking availability of standard functions and structs.
 *
 * Sometimes available functions, structs, or struct fields differ slightly
 * between operating systems.
 */

// Define to 1 when zlib-ng support is provided by the system
#cmakedefine01 C_SYSTEM_ZLIB_NG

// Defined if function clock_gettime is available
#cmakedefine HAVE_CLOCK_GETTIME

// Defined if function __builtin_available is available
#cmakedefine HAVE_BUILTIN_AVAILABLE

// Defined if function __builtin___clear_cache is available
#cmakedefine HAVE_BUILTIN_CLEAR_CACHE

// Defined if function mprotect is available
#cmakedefine HAVE_MPROTECT

// Defined if function mmap is available
#cmakedefine HAVE_MMAP

// Defined if mmap flag MAPJIT is available
#cmakedefine HAVE_MAP_JIT

// Defined if function pthread_jit_write_protect_np is available
#cmakedefine HAVE_PTHREAD_WRITE_PROTECT_NP

// Defined if function sys_icache_invalidate is available
#cmakedefine HAVE_SYS_ICACHE_INVALIDATE

// Defined if function pthread_setname_np is available
#cmakedefine HAVE_PTHREAD_SETNAME_NP

// Defind if function setpriority is available
#cmakedefine HAVE_SETPRIORITY

// Defind if function strnlen is available
#cmakedefine HAVE_STRNLEN

// field d_type in struct dirent is not defined in POSIX
// Some OSes do not implement it (e.g. Haiku)
#cmakedefine HAVE_STRUCT_DIRENT_D_TYPE



/* Available headers
 *
 * Checks for non-POSIX headers and POSIX headers not supported on Windows.
 */

#cmakedefine HAVE_LIBGEN_H
#cmakedefine HAVE_NETINET_IN_H
#cmakedefine HAVE_PWD_H
#define HAVE_STDLIB_H
#cmakedefine HAVE_STRINGS_H
#cmakedefine HAVE_SYS_SOCKET_H
#define HAVE_SYS_TYPES_H
#cmakedefine HAVE_SYS_XATTR_H

/* Hardware-related defines
 */

// Define to 1 when host/target processor uses big endian byte ordering
#cmakedefine WORDS_BIGENDIAN

// Non-4K page size (only on selected architectures)
#cmakedefine PAGESIZE

/* Windows-related defines
 */

// Prevent <windows.h> from clobbering std::min and std::max
#cmakedefine NOMINMAX 1

// Enables mathematical constants (such as M_PI) in Windows math.h header
// https://docs.microsoft.com/en-us/cpp/c-runtime-library/math-constants
#cmakedefine _USE_MATH_DEFINES

// Modern MSVC provides POSIX-like routines, so prefer that over built-in
#cmakedefine HAVE_STRNLEN

// Holds the "--datadir" specified during project setup. This can
// be used as a fallback if the user hasn't populated their
// XDG_DATA_HOME or XDG_DATA_DIRS to include the --datadir.
#cmakedefine CUSTOM_DATADIR

#endif
