/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2022-2023  The DOSBox Staging Team
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

#ifndef DOSBOX_RGB565_H
#define DOSBOX_RGB565_H

#include <cstdint>

#include "rgb.h"
#include "rgb888.h"

class Rgb565 {
public:
	// Default constructor
	constexpr Rgb565() = default;

	constexpr Rgb565(const uint16_t _pixel)
	{
		pixel = _pixel;
	}

	// Construct from separate RGB 8-bit values
	constexpr Rgb565(const uint8_t r8, const uint8_t g8, const uint8_t b8)
	{
		FromRgb888(r8, g8, b8);
	}

	// Equality comparison
	constexpr bool operator==(const Rgb565& rhs) const
	{
		return pixel == rhs.pixel;
	}

	// Update from separate RGB 8-bit values
	constexpr void FromRgb888(const uint8_t r8, const uint8_t g8, const uint8_t b8)
	{
		const auto r5 = ((r8 >> 3) << r5_offset) & r5_mask;
		const auto g6 = ((g8 >> 2) << g6_offset) & g6_mask;
		const auto b5 = ((b8 >> 3) << b5_offset) & b5_mask;

		pixel = static_cast<uint16_t>(r5 | g6 | b5);
	}

	Rgb888 ToRgb888() const
	{
		const auto r8 = Red5To8(pixel);
		const auto g8 = Green6To8(pixel);
		const auto b8 = Blue5To8(pixel);
		return Rgb888(r8, g8, b8);
	}

	// Write to separate RGB 8-bit values
	void ToRgb888(uint8_t& r8, uint8_t& g8, uint8_t& b8) const
	{
		r8 = Red5To8(pixel);
		g8 = Green6To8(pixel);
		b8 = Blue5To8(pixel);
	}

	// Scoped conversion helper: RGB Red 5-bit to 8-bit
	static uint8_t Red5To8(const uint16_t val)
	{
		const auto red5 = (val & r5_mask) >> r5_offset;
		return rgb5_to_8_lut(static_cast<uint8_t>(red5));
	}

	// Scoped conversion helper: RGB Green 6-bit to 8-bit
	static uint8_t Green6To8(const uint16_t val)
	{
		const auto green6 = (val & g6_mask) >> g6_offset;
		return rgb6_to_8_lut(static_cast<uint8_t>(green6));
	}

	// Scoped conversion helper: RGB Blue 5-bit to 8-bit
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
	static constexpr uint16_t r5_mask = 0b1111'1000'0000'0000;
	static constexpr uint16_t g6_mask = 0b0000'0111'1110'0000;
	static constexpr uint16_t b5_mask = 0b0000'0000'0001'1111;

	static constexpr uint8_t r5_offset = 11;
	static constexpr uint8_t g6_offset = 5;
	static constexpr uint8_t b5_offset = 0;
};

#endif
