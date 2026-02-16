// SPDX-FileCopyrightText:  2021-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_RGB888_H
#define DOSBOX_RGB888_H

#include <cstdint>

#include "misc/support.h"

class Rgb888 {
public:
#pragma pack(push, 1)
	uint8_t red   = 0;
	uint8_t green = 0;
	uint8_t blue  = 0;
#pragma pack(pop)

	constexpr Rgb888() = default;

	constexpr Rgb888(const uint8_t r8, const uint8_t g8, const uint8_t b8)
	        : red(r8),
	          green(g8),
	          blue(b8)
	{}

	constexpr operator int() const
	{
		return (blue << 16) | (green << 8) | (red << 0);
	}

	constexpr bool operator==(const Rgb888& that) const
	{
		return (red == that.red && green == that.green && blue == that.blue);
	}

	// Scoped conversion helpers
	static constexpr Rgb888 FromRgb444(const uint8_t r4, const uint8_t g4,
	                                   const uint8_t b4)
	{
		[[maybe_unused]] constexpr auto MaxValue = (1 << 4) - 1;

		assert(r4 <= MaxValue);
		assert(g4 <= MaxValue);
		assert(b4 <= MaxValue);

		const auto r8 = check_cast<uint8_t>(r4 | (r4 << 4));
		const auto g8 = check_cast<uint8_t>(g4 | (g4 << 4));
		const auto b8 = check_cast<uint8_t>(b4 | (b4 << 4));

		return Rgb888(r8, g8, b8);
	}
};

#endif
