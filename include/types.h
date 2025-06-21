// SPDX-FileCopyrightText:  2019-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_TYPES_H
#define DOSBOX_TYPES_H

#include <cinttypes>
#include <cstdint>

/* Work around this bug in /mingw64/x86_64-w64-mingw64/include/inttypes.h:
 * MS runtime does not yet understand C9x standard "ll"
 * length specifier. It appears to treat "ll" as "l".
 * The non-standard I64 length specifier understood by MS
 * runtime functions, so for now we use that.
 */
#if defined(__MINGW64__) && !defined(__clang__)
#ifdef PRIuPTR
#undef PRIuPTR
#endif
#define PRIuPTR "I64u"
#endif

using Bitu   = uintptr_t;
using Bits   = intptr_t;

using Real64 = double;

/* Upstream uses a macro named this way for formatting Bitu values.
 */
#define sBitfs(x) PRI ## x ## PTR

#endif
