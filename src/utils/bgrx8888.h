// SPDX-FileCopyrightText:  2023-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_BGRX8888_H
#define DOSBOX_BGRX8888_H

#include <cstdint>

class Bgrx8888 {
public:
	uint32_t color = 0;

	constexpr Bgrx8888() = default;

	constexpr Bgrx8888(const uint8_t r, const uint8_t g, const uint8_t b)
	        : color(static_cast<uint32_t>(b << 0) |
	                static_cast<uint32_t>(g << 8) |
	                static_cast<uint32_t>(r << 16))
	{}

	constexpr explicit Bgrx8888(const uint32_t c) : color(c) {}

	constexpr uint8_t Red() const
	{
		return static_cast<uint8_t>((color & 0xff0000) >> 16);
	}

	constexpr uint8_t Green() const
	{
		return static_cast<uint8_t>((color & 0xff00) >> 8);
	}

	constexpr uint8_t Blue() const
	{
		return static_cast<uint8_t>(color & 0xff);
	}

	// Cast operator to read-only uint32_t
	constexpr operator uint32_t() const
	{
		return color;
	}
};

#endif // DOSBOX_BGRX8888_H
