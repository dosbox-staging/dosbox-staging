// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

// Compatibility aliases for upstream DOSBox Bit-type names.
// Used by the ported printer code in this directory.
// Cleanup pass later will remove these and rename call sites to the
// standard fixed-width integer types.

#ifndef DOSBOX_PARPORT_BIT_TYPES_COMPAT_H
#define DOSBOX_PARPORT_BIT_TYPES_COMPAT_H

#include <cstdint>

using Bit8u  = uint8_t;
using Bit16u = uint16_t;
using Bit32u = uint32_t;
using Bit8s  = int8_t;
using Bit16s = int16_t;
using Bit32s = int32_t;

#endif
