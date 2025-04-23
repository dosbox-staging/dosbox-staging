/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2022-2025  The DOSBox Staging Team
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

#ifndef DOSBOX_CHECKS_H
#define DOSBOX_CHECKS_H

// This file contains macros that enable extra code checks at the file-level.
//
// Checks can include any compiler-toggleable features that the project team
// feels can improve code quality. This might include:
//   - warnings
//   - warnings-as-errors
//   - diagnostics
//   - formatting / structure / design / etc..
//
// These checks are those that (currently) can't be enabled project-wide
// because they would be fatal (i.e., -Werror) or too verbose (i.e.,
// -Wnarrowing generating too many warnings to make the warnings actually
// useful).
//
// This per file approach lets us focus on one file at a time and then declare
// that it's free from the issue(s) we're checking.
//
// Usage:
//     #include "checks.h"
//     CHECK_NARROWING();
//
// How to add new checks:
//
//   1. Assess how much work it would be to enable it at the project level.
//      If the check is too burdensome, then carry on.
//
//   2. The C98 and C++11 standards supports the _Pragma() function to add
//      #pragma strings. We can use it to add compiler-specific features.
//
//   3. Finish the macro with END_MACRO to swalllow the semicolon and avoid
//      name collisions and warnings about unused variables.

#define END_MACRO \
	struct END_MACRO_##__FILE__##__LINE__ {}

// `-Wconversion` enables a few noisy warnings wholesale that we then need
// to disable manually.
#ifdef __GNUC__
#	define CHECK_NARROWING() \
		_Pragma("GCC diagnostic warning \"-Wconversion\"") \
		_Pragma("GCC diagnostic ignored \"-Wsign-conversion\"") \
		_Pragma("GCC diagnostic warning \"-Wnarrowing\"") \
		END_MACRO
#else
#	define CHECK_NARROWING()
#endif

#endif
