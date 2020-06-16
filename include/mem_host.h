/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2020  The dosbox-staging team
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

#ifndef DOSBOX_MEM_HOST_H
#define DOSBOX_MEM_HOST_H

/* These functions read and write fixed-width unsigned integers to/from
 * byte-arrays using DOS/little-endian byte ordering.
 *
 * Values returned or passed to these functions are expected to be integers
 * using native endianess representation for the host machine.
 *
 * They are safe to use even when byte-array address is not aligned according
 * to desired integer width.
 */

#include <cstdint>

#include "byteorder.h"
#include "mem_unaligned.h"

/* Use host_read* functions to replace endian branching and byte swapping code,
 * such as:
 *
 *   #ifdef WORDS_BIGENDIAN
 *   auto x = byteswap(*(uint16_t*)(arr));
 *   #else
 *   auto x = *(uint16_t*)(arr);
 *   #endif
 */

// Read a single-byte value; provided for backwards-compatibility only.
constexpr uint8_t host_readb(const uint8_t *arr) noexcept
{
	return *arr;
}

// Read a 16-bit word from 8-bit DOS/little-endian byte-ordered memory.
static inline uint16_t host_readw(const uint8_t *arr) noexcept
{
	return le16_to_host(read_unaligned_uint16(arr));
}

// Read a 32-bit double-word from 8-bit DOS/little-endian byte-ordered memory.
static inline uint32_t host_readd(const uint8_t *arr) noexcept
{
	return le32_to_host(read_unaligned_uint32(arr));
}

// Read a 64-bit quad-word from 8-bit DOS/little-endian byte-ordered memory.
static inline uint64_t host_readq(const uint8_t *arr) noexcept
{
	return le64_to_host(read_unaligned_uint64(arr));
}

/* Use host_read*_at functions to replace endian branching and byte swapping
 * code such as:
 *
 *   #ifdef WORDS_BIGENDIAN
 *   auto x = byteswap(((uint16_t*)arr)[idx]);
 *   #else
 *   auto x = ((uint16_t*)arr)[idx];
 *   #endif
 */

// Read an array-indexed 16-bit word from 8-bit DOS/little-endian byte-ordered
// memory.
static inline uint16_t host_readw_at(const uint8_t *arr, const uintptr_t idx) noexcept
{
	return host_readw(arr + idx * sizeof(uint16_t));
}

// Read an array-indexed 32-bit double-word from 8-bit DOS/little-endian
// byte-ordered memory.
static inline uint32_t host_readd_at(const uint8_t *arr, const uintptr_t idx) noexcept
{
	return host_readd(arr + idx * sizeof(uint32_t));
}

// Read an array-indexed 64-bit quad-word from 8-bit DOS/little-endian
// byte-ordered memory.
static inline uint64_t host_readq_at(const uint8_t *arr, const uintptr_t idx) noexcept
{
	return host_readq(arr + idx * sizeof(uint64_t));
}

/* Use host_write* functions to replace endian branching and byte swapping
 * code such as:
 *
 *   #ifdef WORDS_BIGENDIAN
 *   *(uint16_t*)arr = byteswap((uint16_t)val);
 *   #else
 *   *(uint16_t*)arr = (uint16_t)val;
 *   #endif
 */

// Write a single-byte value; provided for backwards-compatibility only.
static inline void host_writeb(uint8_t *arr, const uint8_t val) noexcept
{
	*arr = val;
}

// Write a 16-bit word to 8-bit memory using DOS/little-endian byte-ordering.
static inline void host_writew(uint8_t *arr, const uint16_t val) noexcept
{
	write_unaligned_uint16(arr, host_to_le16(val));
}

// Write a 32-bit double-word to 8-bit memory using DOS/little-endian byte-ordering.
static inline void host_writed(uint8_t *arr, const uint32_t val) noexcept
{
	write_unaligned_uint32(arr, host_to_le32(val));
}

// Write a 64-bit quad-word to 8-bit memory using DOS/little-endian byte-ordering.
static inline void host_writeq(uint8_t *arr, const uint64_t val) noexcept
{
	write_unaligned_uint64(arr, host_to_le64(val));
}

/* Use host_write*_at functions to replace endian branching and byte swapping
 * code such as:
 *
 *   #ifdef WORDS_BIGENDIAN
 *   ((uint16_t*)arr)[idx] = byteswap((uint16_t)val);
 *   #else
 *   ((uint16_t*)arr)[idx] = (uint16_t)val;
 *   #endif
 */

// Write a 16-bit array-indexed word to 8-bit memory using DOS/little-endian
// byte-ordering.
static inline void host_writew_at(uint8_t *arr,
                                  const uintptr_t idx,
                                  const uint16_t val) noexcept
{
	host_writew(arr + idx * sizeof(uint16_t), val);
}

// Write a 32-bit array-indexed double-word to 8-bit memory using
// DOS/little-endian byte-ordering.
static inline void host_writed_at(uint8_t *arr,
                                  const uintptr_t idx,
                                  const uint32_t val) noexcept
{
	host_writed(arr + idx * sizeof(uint32_t), val);
}

// Write a 64-bit array-indexed quad-word to 8-bit memory using
// DOS/little-endian byte-ordering.
static inline void host_writeq_at(uint8_t *arr,
                                  const uintptr_t idx,
                                  const uint64_t val) noexcept
{
	host_writeq(arr + idx * sizeof(uint64_t), val);
}

/* Use host_add* functions to replace endian branching and byte swapping
 * code such as:
 *
 *   #ifdef WORDS_BIGENDIAN
 *   *(uint16_t*)arr += byteswap((uint16_t)val);
 *   #else
 *   *(uint16_t*)arr += val;
 *   #endif
 */

// Add to a 16-bit word stored in 8-bit DOS/little-endian byte-ordered memory.
static inline void host_addw(uint8_t *arr, const uint16_t val) noexcept
{
	host_writew(arr, host_readw(arr) + val);
}

// Add to a 32-bit double-word stored in 8-bit DOS/little-endian byte-ordered
// memory.
static inline void host_addd(uint8_t *arr, const uint32_t val) noexcept
{
	host_writed(arr, host_readd(arr) + val);
}

// Add to a 64-bit quad-word stored in 8-bit DOS/little-endian byte-ordered memory.
static inline void host_addq(uint8_t *arr, const uint64_t val) noexcept
{
	host_writeq(arr, host_readq(arr) + val);
}

/* Use host_inc* functions to replace endian branching and byte swapping
 * code such as:
 *
 *   #ifdef WORDS_BIGENDIAN
 *   *(uint16_t*)arr += byteswap((uint16_t)1);
 *   #else
 *   *(uint16_t*)arr += 1;
 *   #endif
 */

// Increment a 16-bit word stored in 8-bit DOS/little-endian byte-ordered memory.
static inline void host_incw(uint8_t *arr) noexcept
{
	host_writew(arr, host_readw(arr) + 1);
}

// Increment a 32-bit double-word stored in 8-bit DOS/little-endian byte-ordered
// memory.
static inline void host_incd(uint8_t *arr) noexcept
{
	host_writed(arr, host_readd(arr) + 1);
}

// Increment a 64-bit quad-word stored in 8-bit DOS/little-endian byte-ordered
// memory.
static inline void host_incq(uint8_t *arr) noexcept
{
	host_writeq(arr, host_readq(arr) + 1);
}

#endif
