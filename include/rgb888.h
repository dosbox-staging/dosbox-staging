/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2021-2023  The DOSBox Staging Team
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

#ifndef DOSBOX_RGB888_H
#define DOSBOX_RGB888_H

#include <cstdint>

#include "rgb16.h"

class Rgb888 {
public:
#pragma pack(push, 1)
	uint8_t red   = 0;
	uint8_t green = 0;
	uint8_t blue  = 0;
#pragma pack(pop)

	constexpr Rgb888() = default;
	constexpr Rgb888(const uint8_t r, const uint8_t g, const uint8_t b)
	        : red(r),
	          green(g),
	          blue(b)
	{}

	constexpr Rgb888(const uint16_t val)
	        : red(Rgb16::Red5To8(val)),
	          green(Rgb16::Green6To8(val)),
	          blue(Rgb16::Blue5To8(val))
	{}

	constexpr operator int() const
	{
		return (blue << 16) | (green << 8) | (red << 0);
	}

	constexpr static Rgb888 byteswap(const Rgb888& in)
	{
		return Rgb888(in.blue, in.green, in.red);
	}
};

constexpr Rgb888 host_to_le(const Rgb888& in) noexcept
{
	return Rgb888::byteswap(in);
}

#endif
