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

#ifndef DOSBOX_RGB555_H
#define DOSBOX_RGB555_H

#include <cstdint>

#include "rgb.h"
#include "rgb888.h"

class Rgb555 {
public:
	constexpr Rgb555() = default;

	constexpr Rgb555(const uint16_t _pixel)
	{
		pixel = _pixel;
	}

	constexpr Rgb555(const uint8_t r8, const uint8_t g8, const uint8_t b8)
	{
		pixel = Rgb555::PixelFromRgb888(r8, g8, b8);
	}

	constexpr bool operator==(const Rgb555& rhs) const
	{
		return pixel == rhs.pixel;
	}

	constexpr bool operator!=(const Rgb555& that) const
	{
		return !operator==(that);
	}

	Rgb888 ToRgb888() const
	{
		const auto r8 = Red5To8(pixel);
		const auto g8 = Green5To8(pixel);
		const auto b8 = Blue5To8(pixel);
		return Rgb888(r8, g8, b8);
	}

	void ToRgb888(uint8_t& r8, uint8_t& g8, uint8_t& b8) const
	{
		r8 = Red5To8(pixel);
		g8 = Green5To8(pixel);
		b8 = Blue5To8(pixel);
	}

	// Scoped conversion helpers
	static constexpr Rgb555 FromRgb888(const Rgb888 rgb888)
	{
		return Rgb555(Rgb555::PixelFromRgb888(rgb888.red,
		                                      rgb888.green,
		                                      rgb888.blue));
	}

	static uint8_t Red5To8(const uint16_t val)
	{
		const auto red5 = (val & r5_mask) >> r5_offset;
		return rgb5_to_8_lut(static_cast<uint8_t>(red5));
	}

	static uint8_t Green5To8(const uint16_t val)
	{
		const auto green5 = (val & g5_mask) >> g5_offset;
		return rgb5_to_8_lut(static_cast<uint8_t>(green5));
	}

	static uint8_t Blue5To8(const uint16_t val)
	{
		const auto blue5 = (val & b5_mask) >> b5_offset;
		return rgb5_to_8_lut(static_cast<uint8_t>(blue5));
	}

	// Allow read-write to the underlying data because the class holds no
	// state and it's impossible to assign an invalid value.
	uint16_t pixel = 0;

private:
	// Scoped constants
	static constexpr uint16_t r5_mask = 0b0111'1100'0000'0000;
	static constexpr uint16_t g5_mask = 0b0000'0011'1110'0000;
	static constexpr uint16_t b5_mask = 0b0000'0000'0001'1111;

	static constexpr uint8_t r5_offset = 10;
	static constexpr uint8_t g5_offset = 5;
	static constexpr uint8_t b5_offset = 0;

	static constexpr uint16_t PixelFromRgb888(const uint8_t r8,
	                                          const uint8_t g8, const uint8_t b8)
	{
		const auto r5 = ((r8 >> 3) << r5_offset) & r5_mask;
		const auto g5 = ((g8 >> 2) << g5_offset) & g5_mask;
		const auto b5 = ((b8 >> 3) << b5_offset) & b5_mask;

		return static_cast<uint16_t>(r5 | g5 | b5);
	}
};

#endif
