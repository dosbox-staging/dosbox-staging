/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2023-2023  The DOSBox Staging Team
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

#ifndef DOSBOX_BGRX8888_H
#define DOSBOX_BGRX8888_H

#include <cstdint>

// A class that holds the colour channels as an array of four 8-bit values
// in byte order (also known as memory order), regardless of host
// endianness. The bytes in memory are always Blue, Green, Red, and finally
// a placeholder.
//
class Bgrx8888 {
public:
	constexpr Bgrx8888() = default;

	Bgrx8888(const uint8_t b8, const uint8_t g8, const uint8_t r8)
	{
		Set(b8, g8, r8);
	}

	void Set(const uint8_t b8, const uint8_t g8, const uint8_t r8)
	{
		colour.components = {b8, g8, r8, 0};
	}

	constexpr uint8_t Blue8() const
	{
		return colour.components.b8;
	}

	constexpr uint8_t Green8() const
	{
		return colour.components.g8;
	}

	constexpr uint8_t Red8() const
	{
		return colour.components.r8;
	}

	// Cast operator to read-only uint32_t
	constexpr operator uint32_t() const
	{
		return colour.bgrx8888;
	}

private:
	union {
		struct {
			// byte order
			uint8_t b8 = 0;
			uint8_t g8 = 0;
			uint8_t r8 = 0;
			uint8_t x8 = 0;
		} components;
		uint32_t bgrx8888 = 0;
	} colour = {};
};

static_assert(sizeof(Bgrx8888) == sizeof(uint32_t));

#endif
