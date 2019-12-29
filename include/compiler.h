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
