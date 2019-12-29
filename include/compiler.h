/*
 *  Copyright (C) 2019  The DOSBox Team
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

#ifndef DOSBOX_COMPILER_H
#define DOSBOX_COMPILER_H

// This header wraps compiler-specific features, so they won't need to
// be hacked into the buildsystem.

// Modern C++ compilers have better support for feature testing using GNU
// extension __has_attribute but C++20 introduces even better alternative:
// standard-defined __has_cpp_attribute, which will remove the need for
// defining C_HAS_* macros on a buildsystem level (at some point).

// The __attribute__ syntax is supported by GCC, Clang, and IBM compilers.
//
// TODO: C++11 introduces standard syntax for implementation-defined attributes,
//       it should allow for removal of C_HAS_ATTRIBUTE from the buildsystem.
//       However, the vast majority of GCC_ATTRIBUTEs in DOSBox code need
//       to be reviewed, as many of them seem to be incorrectly/unnecessarily
//       used.

#if C_HAS_ATTRIBUTE
#define GCC_ATTRIBUTE(x) __attribute__ ((x))
#else
#define GCC_ATTRIBUTE(x) /* attribute not supported */
#endif

// GCC_LIKELY macro is incorrectly named, because other compilers support
// this feature as well (e.g. Clang, Intel); leave it be for now, at
// least until full support for C++20 [[likely]] attribute will start arriving
// in new compilers.
//
// Note: '!!' trick is used, to convert non-boolean values to 1 or 0
// to prevent accidental incorrect usage (e.g. when user wraps macro
// around a pointer or an integer, expecting usual C semantics).

#if C_HAS_BUILTIN_EXPECT
#define GCC_LIKELY(x)   __builtin_expect(!!(x), 1)
#define GCC_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define GCC_LIKELY
#define GCC_UNLIKELY
#endif

#endif /* DOSBOX_COMPILER_H */
