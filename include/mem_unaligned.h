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
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef DOSBOX_MEM_UNALIGNED_H
#define DOSBOX_MEM_UNALIGNED_H

/*  These functions read and write 16, 32, and 64 bit uint's to
 *  and from 8-bit memory positions using defined alignment. Use
 *  these functions instead of C-style pointer type casts to larger
 *  types, which are not alignment-safe.
 */

#include <cstdint>
#include <cstring>

#include "byteorder.h"

/*  Read a uint16 from unaligned 8-bit byte-ordered memory. Use this
 *  instead of *(uint16_t*)(8_bit_ptr) or *(uint16_t*)(8_bit_ptr + offset)
 */
static inline uint16_t read_unaligned_uint16(const uint8_t *arr)
{
	uint16_t val;
	memcpy(&val, arr, sizeof(val));
	return val;
}

/*  Read an array-indexed uint16 from unaligned 8-bit byte-ordered memory.
 *  Use this instead of ((uint16_t*)arr)[idx]
 */
static inline uint16_t read_unaligned_uint16_at(const uint8_t *arr, const uintptr_t idx)
{
	return read_unaligned_uint16(arr + idx * sizeof(uint16_t));
}

// Write a uint16 to unaligned 8-bit memory using byte-ordering.
static inline void write_unaligned_uint16(uint8_t *arr, uint16_t val)
{
	memcpy(arr, &val, sizeof(val));
}

// Write an array-indexed uint16 to unaligned 8-bit memory using byte-ordering.
static inline void write_unaligned_uint16_at(uint8_t *arr, const uintptr_t idx, const uint16_t val)
{
	write_unaligned_uint16(arr + idx * sizeof(uint16_t), val);
}

// Increment a uint16 value held in unaligned 8-bit byte-ordered memory.
static inline void incr_unaligned_uint16(uint8_t *arr, const uint16_t incr)
{
	write_unaligned_uint16(arr, read_unaligned_uint16(arr) + incr);
}

// Read a uint32 from unaligned 8-bit byte-ordered memory.
static inline uint32_t read_unaligned_uint32(const uint8_t *arr)
{
	uint32_t val;
	memcpy(&val, arr, sizeof(val));
	return val;
}

// Read an array-indexed uint32 from unaligned 8-bit byte-ordered memory.
static inline uint32_t read_unaligned_uint32_at(const uint8_t *arr, const uintptr_t idx)
{
	return read_unaligned_uint32(arr + idx * sizeof(uint32_t));
}

// Write a uint32 to unaligned 8-bit memory using byte-ordering.
static inline void write_unaligned_uint32(uint8_t *arr, uint32_t val)
{
	memcpy(arr, &val, sizeof(val));
}

// Write an array-indexed uint32 to unaligned 8-bit memory using byte-ordering.
static inline void write_unaligned_uint32_at(uint8_t *arr, const uintptr_t idx, const uint32_t val)
{
	write_unaligned_uint32(arr + idx * sizeof(uint32_t), val);
}

// Increment a uint32 value held in unaligned 8-bit byte-ordered memory.
static inline void incr_unaligned_uint32(uint8_t *arr, const uint32_t incr)
{
	write_unaligned_uint32(arr, read_unaligned_uint32(arr) + incr);
}

// Read a uint64 from unaligned 8-bit byte-ordered memory.
static inline uint64_t read_unaligned_uint64(const uint8_t *arr)
{
	uint64_t val;
	memcpy(&val, arr, sizeof(val));
	return val;
}

// Write a uint64 to unaligned 8-bit memory using byte-ordering.
static inline void write_unaligned_uint64(uint8_t *arr, uint64_t val)
{
	memcpy(arr, &val, sizeof(val));
}

#endif
