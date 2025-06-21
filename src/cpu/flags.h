// SPDX-FileCopyrightText:  2024-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

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

