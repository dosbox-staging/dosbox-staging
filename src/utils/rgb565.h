// SPDX-FileCopyrightText:  2022-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_RGB565_H
#define DOSBOX_RGB565_H

#include <cstdint>

#include "rgb.h"
#include "rgb888.h"
#include "misc/support.h"

class Rgb565 {
public:
	constexpr Rgb565() = default;

	constexpr Rgb565(const uint16_t _pixel)
	{
		pixel = _pixel;
	}

	constexpr Rgb565(const uint8_t r8, const uint8_t g8, const uint8_t b8)
	{
		pixel = Rgb565::PixelFromRgb888(r8, g8, b8);
	}

	constexpr bool operator==(const Rgb565& rhs) const
	{
		return pixel == rhs.pixel;
	}

	Rgb888 ToRgb888() const
	{
		const auto r8 = Red5To8(pixel);
		const auto g8 = Green6To8(pixel);
		const auto b8 = Blue5To8(pixel);
		return Rgb888(r8, g8, b8);
	}

	void ToRgb888(uint8_t& r8, uint8_t& g8, uint8_t& b8) const
	{
		r8 = Red5To8(pixel);
		g8 = Green6To8(pixel);
		b8 = Blue5To8(pixel);
	}

	// Scoped conversion helpers
	static constexpr Rgb565 FromRgb888(const Rgb888 rgb888)
	{
		return Rgb565(Rgb565::PixelFromRgb888(rgb888.red,
		                                      rgb888.green,
		                                      rgb888.blue));
	}

	static uint8_t Red5To8(const uint16_t val)
	{
		const auto red5 = (val & r5_mask) >> r5_offset;
		return rgb5_to_8_lut(check_cast<uint8_t>(red5));
	}

	static uint8_t Green6To8(const uint16_t val)
	{
		const auto green6 = (val & g6_mask) >> g6_offset;
		return rgb6_to_8_lut(check_cast<uint8_t>(green6));
	}

	static uint8_t Blue5To8(const uint16_t val)
	{
		const auto blue5 = (val & b5_mask) >> b5_offset;
		return rgb5_to_8_lut(check_cast<uint8_t>(blue5));
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

	static constexpr uint16_t PixelFromRgb888(const uint8_t r8,
	                                          const uint8_t g8, const uint8_t b8)
	{
		const auto r5 = ((r8 >> 3) << r5_offset) & r5_mask;
		const auto g6 = ((g8 >> 2) << g6_offset) & g6_mask;
		const auto b5 = ((b8 >> 3) << b5_offset) & b5_mask;

		return check_cast<uint16_t>(r5 | g6 | b5);
	}
};

#endif
