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

#ifndef DOSBOX_MEM_HOST_H
#define DOSBOX_MEM_HOST_H

#include <cstdint>

#include "byteorder.h"
#include "mem_unaligned.h"

/*  These functions read and write 16, 32, and 64 bit uint's to
 *  and from unaligned 8-bit memory in DOS/little-endian ordering.
 */

// Read and write single-byte values
constexpr uint8_t host_readb(const uint8_t *var)
{
	return *var;
}

static inline void host_writeb(uint8_t *var, const uint8_t val)
{
	*var = val;
}

/*  Read a 16-bit WORD from 8-bit DOS/little-endian byte-ordered memory.
 *  Use this instead of endian branching and byte swapping, such as:
 *  #if BIG_ENDIAN byteswap(*(uint16_t*)(arr)) #else *(uint16_t*)(arr) 
 */
static inline uint16_t host_readw(const uint8_t *arr)
{
	return le16_to_host(read_unaligned_uint16(arr));
}

/*  Read an array-indexed 16-bit WORD from 8-bit DOS/little-endian byte-ordered
 *  memory. Use this instead of endian branching and byte swapping, such as:
 *  #if BIG_ENDIAN byteswap(((uint16_t*)arr)[i]) #else ((uint16_t*)arr)[i]
 */
static inline uint16_t host_readw_at(const uint8_t *arr, const uintptr_t idx)
{
	return host_readw(arr + idx * sizeof(uint16_t));
}

// Write a 16-bit WORD to 8-bit memory using DOS/little-endian byte-ordering.
static inline void host_writew(uint8_t *arr, uint16_t val)
{
	write_unaligned_uint16(arr, host_to_le16(val));
}

// Write a 16-bit array-indexed WORD to 8-bit memory using DOS/little-endian byte-ordering.
static inline void host_writew_at(uint8_t *arr, const uintptr_t idx, const uint16_t val)
{
	host_writew(arr + idx * sizeof(uint16_t), val);
}

// Increment a 16-bit WORD held in 8-bit DOS/little-endian byte-ordered memory.
static inline void host_incrw(uint8_t *arr, const uint16_t incr)
{
	host_writew(arr, host_readw(arr) + incr);
}

// Read a 32-bit DWORD from 8-bit DOS/little-endian byte-ordered memory.
static inline uint32_t host_readd(const uint8_t *arr)
{
	return le32_to_host(read_unaligned_uint32(arr));
}

// Read a 32-bit array-indexed DWORD from 8-bit DOS/little-endian byte-ordered memory. 
static inline uint32_t host_readd_at(const uint8_t *arr, const uintptr_t idx)
{
	return host_readd(arr + idx * sizeof(uint32_t));
}

// Write a 32-bit DWORD to 8-bit memory using DOS/little-endian byte-ordering.
static inline void host_writed(uint8_t *arr, uint32_t val)
{
	write_unaligned_uint32(arr, host_to_le32(val));
}

// Write a 32-bit array-indexed DWORD to 8-bit memory using DOS/little-endian byte-ordering.
static inline void host_writed_at(uint8_t *arr, const uintptr_t idx, const uint32_t val)
{
	host_writed(arr + idx * sizeof(uint32_t), val);
}

// Increment a 32-bit DWORD held in 8-bit DOS/little-endian byte-ordered memory.
static inline void host_incrd(uint8_t *arr, const uint32_t incr)
{
	host_writed(arr, host_readd(arr) + incr);
}

// (Provided for backward compatibility. The goal is to migrate these calls to the more
// inituively named incr_* functions)
static inline void host_addw(uint8_t *arr, const uint16_t incr){ host_incrw(arr, incr); }
static inline void host_addd(uint8_t *arr, const uint32_t incr){ host_incrd(arr, incr); }

// Read and write a 64-bit Quad-WORD to 8-bit memory using DOS/little-endian byte-ordering.
static inline uint64_t host_readq(uint8_t *arr)
{
    return le64_to_host(read_unaligned_uint64(arr));
}

static inline void host_writeq(uint8_t *arr, uint64_t val){
    write_unaligned_uint64(arr, host_to_le64(val));
}

#endif
