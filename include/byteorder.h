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

#ifndef DOSBOX_BYTEORDER_H
#define DOSBOX_BYTEORDER_H

/* Usual way of handling endianness and byteswapping is via endian.h
 * (for e.g. htole* functions) or byteswap.h (e.g. bswap_16 macro).
 *
 * However, these headers are non-standard and almost every OS has their own
 * variations. It's easier to use compiler-provided intrinsics and builtins than
 * to rely on OS libraries.
 *
 * This header provides functions deliberately named differently
 * than popular variations on these implementations (e.g. bswap_u16 instead of
 * bswap_16), to avoid conflicts.
 */

#include "config.h"

#include <cstdint>
#include <cstdlib>

#if !defined(_MSC_VER)

/* Aside of MSVC, every C++14-capable compiler provides __builtin_bswap*
 * as compiler intrinsics or builtin functions.
 */

#define bswap_u16(x) __builtin_bswap16(x)
#define bswap_u32(x) __builtin_bswap32(x)
#define bswap_u64(x) __builtin_bswap64(x)

#else

/* MSVCS does not provide __builtin_bswap* functions, but has its own
 * byteswap builtin's.  MSDN lists these as library functions, but MSVC makes
 * good job of inlinig them when compiling with /O2.
 *
 * https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/byteswap-uint64-byteswap-ulong-byteswap-ushort
 */

#define bswap_u16(x) _byteswap_ushort(x)
#define bswap_u32(x) _byteswap_ulong(x)
#define bswap_u64(x) _byteswap_uint64(x)

#endif // MSC_VER

#ifdef WORDS_BIGENDIAN

#define host_to_le16(x) bswap_u16(static_cast<uint16_t>(x))
#define host_to_le32(x) bswap_u32(static_cast<uint32_t>(x))
#define host_to_le64(x) bswap_u64(static_cast<uint64_t>(x))

#define le16_to_host(x) bswap_u16(static_cast<uint16_t>(x))
#define le32_to_host(x) bswap_u32(static_cast<uint32_t>(x))
#define le64_to_host(x) bswap_u64(static_cast<uint64_t>(x))

#else

#define host_to_le16(x) static_cast<uint16_t>(x)
#define host_to_le32(x) static_cast<uint32_t>(x)
#define host_to_le64(x) static_cast<uint64_t>(x)

#define le16_to_host(x) static_cast<uint16_t>(x)
#define le32_to_host(x) static_cast<uint32_t>(x)
#define le64_to_host(x) static_cast<uint64_t>(x)

#endif

/* 'host_to_le' functions allow for byte order conversion on big endian
 * architectures while respecting memory alignment on low endian.
 *
 * We overload these functions to let compiler pick the correct byteswapping
 * function if user is ok with that.
 *
 * The only caveat in here is: MSVC bswap implementations are not constexpr,
 * so these functions won't compile using MSVC for big endian
 * architecture, but it's unlikely to ever be a real problem.
 */

constexpr uint8_t host_to_le(uint8_t x) noexcept { return x; }

#ifdef WORDS_BIGENDIAN

constexpr uint16_t host_to_le(uint16_t x) noexcept { return bswap_u16(x); }
constexpr uint32_t host_to_le(uint32_t x) noexcept { return bswap_u32(x); }
constexpr uint64_t host_to_le(uint64_t x) noexcept { return bswap_u64(x); }

#else

constexpr uint16_t host_to_le(uint16_t x) noexcept { return x; }
constexpr uint32_t host_to_le(uint32_t x) noexcept { return x; }
constexpr uint64_t host_to_le(uint64_t x) noexcept { return x; }

#endif

/* Functions 'le_to_host' and 'host_to_le' are the same function, only the user
 * intent might be different.
 */

#define le_to_host(x) host_to_le(x)

#endif
