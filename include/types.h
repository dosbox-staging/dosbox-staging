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

#ifndef DOSBOX_TYPES_H
#define DOSBOX_TYPES_H

#include <cinttypes>
#include <cstdint>

using Bit8u  = uint8_t;
using Bit16u = uint16_t;
using Bit32u = uint32_t;
using Bit64u = uint64_t;
using Bitu   = uintptr_t;

using Bit8s  = int8_t;
using Bit16s = int16_t;
using Bit32s = int32_t;
using Bit64s = int64_t;
using Bits   = intptr_t;

using Real64 = double;

// TODO: remove
#define sBitfs(x) PRIuPTR

#endif /* DOSBOX_TYPES_H */
