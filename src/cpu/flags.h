/*
 *  Copyright (C) 2024-2024  The DOSBox Staging Team
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

#ifndef DOSBOX_CPU_FLAGS_H
#define DOSBOX_CPU_FLAGS_H

// This header holds flag-related functions available to all the cores (normal,
// dynrec, and dynamic).
//
// Note that within the dynamic core, be sure to add a call to
// 'set_skipflags(false)' before calling these functions to ensure the CPU's
// flags are handled. For an example of this pattern, see the division helper
// functions in core_dyn_x86/helpers.h and the use of set_skipflags in
// core_dyn_x86/decoder.h.


// Set CPU test flags for all IDIV and DIV quotients
template <typename T>
void set_cpu_test_flags_for_division(const T quotient) noexcept;

#endif

