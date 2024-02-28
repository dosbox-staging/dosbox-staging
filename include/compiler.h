/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2019-2021  The DOSBox Staging Team
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

#include "config.h"

// This header wraps compiler-specific features, so they won't need to
// be hacked into the buildsystem.

// Function-like macro __has_cpp_attribute was introduced in C++20, but
// various compilers support it since C++11 as a language extension.
// Thanks to that we can use it for testing support for both language-defined
// and vendor-specific attributes.

#ifndef __has_cpp_attribute // for compatibility with non-supporting compilers
#define __has_cpp_attribute(x) 0
#endif

// Function-like macro __has_attribute was introduced in GCC 5.x and Clang,
// alongside __has_cpp_attribute, and with the same logic.
// See: https://clang.llvm.org/docs/LanguageExtensions.html#has-attribute

#ifdef __has_attribute
#define C_HAS_ATTRIBUTE 1
#else
#define C_HAS_ATTRIBUTE 0
#define __has_attribute(x) 0 // for compatibility with non-supporting compilers
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

// XSTR and STR macros can be used for turning defines into string literals:
//
// #define FOO 4
// printf("It's a " STR(FOO));  // prints "It's a FOO"
// printf("It's a " XSTR(FOO)); // prints "It's a 4"

#define XSTR(s) STR(s)
#define STR(s)  #s

#endif
