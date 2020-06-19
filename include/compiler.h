/*
 *  Copyright (C) 2019-2020  The DOSBox Team
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

// Function-like macro __has_cpp_attribute was introduced in C++20, but
// various compilers support it since C++11 as a language extension.
// Thanks to that we can use it for testing support for both language-defined
// and vendor-specific attributes.

#ifndef __has_cpp_attribute // for compatibility with non-supporting compilers
#define __has_cpp_attribute(x) 0
#endif

// When passing the -Wunused flag to GCC or Clang, entities that are unused by
// the program may be diagnosed.  The MAYBE_UNUSED attribute can be used to
// silence such diagnostics when the entity cannot be removed.
//
// The attribute may be applied to the declaration of a class, a typedef,
// a variable, a function or method, a function parameter, an enumeration,
// an enumerator, a non-static data member, or a label.

#if __has_cpp_attribute(maybe_unused)
#define MAYBE_UNUSED [[maybe_unused]]
#elif __has_cpp_attribute(gnu::unused)
#define MAYBE_UNUSED [[gnu::unused]]
#else
#define MAYBE_UNUSED
#endif

// The __attribute__ syntax is supported by GCC, Clang, and IBM compilers.
//
// Provided for backwards-compatibility with old code; to be gradually
// replaced by new C++ attribute syntax.

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
