/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2021  The DOSBox Staging Team
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

#ifndef DOSBOX_MEM_UNALIGNED_H
#define DOSBOX_MEM_UNALIGNED_H

/* These functions read and write fixed-size unsigned integers to/from
 * byte-buffers, regardless of memory alignment requirements.
 *
 * Use these functions instead of C-style pointer type casts to larger
 * types (which are not alignment-safe).
 *
 * These functions use memcpy to signal intent to the compiler; all modern
 * compilers recognize this and generate well-optimized, safe, inlined
 * instructions (and not a function call). Bad compilers will generate memcpy
 * call (which will be slower, but still safe).
 *
 * Examples used in comments throughout this file are NOT alignment safe - they
 * exist only for illustrative purposes; they will work on x86 architecture in
 * practice, but depend on undefined behaviour, might generate slow code or
 * outright crash when targeting different architectures.
 */

#include <cstdint>
#include <cstring>

#include "byteorder.h"
#include "compiler.h"

/* Use read_unaligned_* functions instead constructs like:
 *
 *   *(uint16_t*)(pointer_to_uint8)
 *
 * or
 *
 *   *(uint16_t*)(pointer_to_uint8 + offset)
 */

// Read a uint16 from unaligned 8-bit byte-ordered memory.
static inline uint16_t read_unaligned_uint16(const uint8_t *arr) noexcept
{
	uint16_t val;
	memcpy(&val, arr, sizeof(val));
	return val;
}

// Read a uint32 from unaligned 8-bit byte-ordered memory.
static inline uint32_t read_unaligned_uint32(const uint8_t *arr) noexcept
{
	uint32_t val;
	memcpy(&val, arr, sizeof(val));
	return val;
}

// Read a uint64 from unaligned 8-bit byte-ordered memory.
static inline uint64_t read_unaligned_uint64(const uint8_t *arr) noexcept
{
	uint64_t val;
	memcpy(&val, arr, sizeof(val));
	return val;
}

// Read a size_t from unaligned 8-bit byte-ordered memory.
static inline size_t read_unaligned_size_t(const uint8_t *arr) noexcept
{
	size_t val;
	memcpy(&val, arr, sizeof(size_t));
	return val;
}

/* Use read_unaligned_*_at functions instead constructs like:
 *
 *   ((uint16_t*)pointer_to_uint8)[idx]
 */

// Read an array-indexed uint16 from unaligned 8-bit byte-ordered memory.
static inline uint16_t read_unaligned_uint16_at(const uint8_t *arr,
                                                const uintptr_t idx) noexcept
{
	return read_unaligned_uint16(arr + idx * sizeof(uint16_t));
}

// Read an array-indexed uint32 from unaligned 8-bit byte-ordered memory.
static inline uint32_t read_unaligned_uint32_at(const uint8_t *arr,
                                                const uintptr_t idx) noexcept
{
	return read_unaligned_uint32(arr + idx * sizeof(uint32_t));
}

// Read an array-indexed uint64 from unaligned 8-bit byte-ordered memory.
static inline uint64_t read_unaligned_uint64_at(const uint8_t *arr,
                                                const uintptr_t idx) noexcept
{
	return read_unaligned_uint64(arr + idx * sizeof(uint64_t));
}

// Read an array-indexed size_t from unaligned 8-bit byte-ordered memory.
static inline size_t read_unaligned_size_t_at(const uint8_t *arr,
                                              const uintptr_t idx) noexcept
{
	return read_unaligned_size_t(arr + idx * sizeof(size_t));
}

/* Use write_unaligned_* functions instead constructs like:
 *
 *   *((uint16_t*)pointer_to_uint8) = val;
 *
 * or
 *
 *   ((uint16_t*)pointer_to_uint8)[0] = val;
 */

// Write a uint16 to unaligned 8-bit memory using byte-ordering.
static inline void write_unaligned_uint16(uint8_t *arr, const uint16_t val) noexcept
{
	memcpy(arr, &val, sizeof(val));
}

// Write a uint32 to unaligned 8-bit memory using byte-ordering.
static inline void write_unaligned_uint32(uint8_t *arr, const uint32_t val) noexcept
{
	memcpy(arr, &val, sizeof(val));
}

// Write a uint64 to unaligned 8-bit memory using byte-ordering.
static inline void write_unaligned_uint64(uint8_t *arr, const uint64_t val) noexcept
{
	memcpy(arr, &val, sizeof(val));
}

/* Use write_unaligned_*_at functions instead constructs like:
 *
 *   ((uint16_t*)pointer_to_uint8)[idx] = val;
 */

// Write an array-indexed uint16 to unaligned 8-bit memory using byte-ordering.
static inline void write_unaligned_uint16_at(uint8_t *arr,
                                             const uintptr_t idx,
                                             const uint16_t val) noexcept
{
	write_unaligned_uint16(arr + idx * sizeof(uint16_t), val);
}

// Write an array-indexed uint32 to unaligned 8-bit memory using byte-ordering.
static inline void write_unaligned_uint32_at(uint8_t *arr,
                                             const uintptr_t idx,
                                             const uint32_t val) noexcept
{
	write_unaligned_uint32(arr + idx * sizeof(uint32_t), val);
}

// Write an array-indexed uint64 to unaligned 8-bit memory using byte-ordering.
static inline void write_unaligned_uint64_at(uint8_t *arr,
                                             const uintptr_t idx,
                                             const uint64_t val) noexcept
{
	write_unaligned_uint64(arr + idx * sizeof(uint64_t), val);
}

/* Use add_to_unaligned_* functions instead of constructs like:
 *
 *   ((uint16_t*)pointer_to_uint8)[0] += val;
 */

// Add to an uint16 value held in unaligned 8-bit byte-ordered memory.
static inline void add_to_unaligned_uint16(uint8_t *arr, const uint16_t val) noexcept
{
	write_unaligned_uint16(arr, read_unaligned_uint16(arr) + val);
}

// Add to an uint32 value held in unaligned 8-bit byte-ordered memory.
static inline void add_to_unaligned_uint32(uint8_t *arr, const uint32_t val) noexcept
{
	write_unaligned_uint32(arr, read_unaligned_uint32(arr) + val);
}

// Add to an uint64 value held in unaligned 8-bit byte-ordered memory.
static inline void add_to_unaligned_uint64(uint8_t *arr, const uint64_t val) noexcept
{
	write_unaligned_uint64(arr, read_unaligned_uint64(arr) + val);
}

/* Use inc_unaligned_* functions instead constructs like:
 *
 *   ((uint16_t*)pointer_to_uint8)[0] += 1;
 */

// Increment an uint16 value held in unaligned 8-bit byte-ordered memory.
static inline void inc_unaligned_uint16(uint8_t *arr) noexcept
{
	write_unaligned_uint16(arr, read_unaligned_uint16(arr) + 1);
}

// Increment an uint32 value held in unaligned 8-bit byte-ordered memory.
static inline void inc_unaligned_uint32(uint8_t *arr) noexcept
{
	write_unaligned_uint32(arr, read_unaligned_uint32(arr) + 1);
}

// Increment an uint64 value held in unaligned 8-bit byte-ordered memory.
static inline void inc_unaligned_uint64(uint8_t *arr) noexcept
{
	write_unaligned_uint64(arr, read_unaligned_uint64(arr) + 1);
}

#endif
