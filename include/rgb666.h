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

#ifndef DOSBOX_RGB666_H
#define DOSBOX_RGB666_H

#include <cassert>
#include <cstdint>

#include "rgb888.h"
#include "support.h"

class Rgb666 {
public:
	uint8_t red   = 0;
	uint8_t green = 0;
	uint8_t blue  = 0;

	constexpr Rgb666() = default;

	constexpr Rgb666(const uint8_t r6, const uint8_t g6, const uint8_t b6)
	        : red(r6),
	          green(g6),
	          blue(b6)
	{
		[[maybe_unused]] constexpr auto MaxComponentValue = (1 << 6) - 1;

		assert(r6 <= MaxComponentValue);
		assert(g6 <= MaxComponentValue);
		assert(b6 <= MaxComponentValue);
	}

	constexpr bool operator==(const Rgb666& that) const
	{
		return (red == that.red && green == that.green && blue == that.blue);
	}

	constexpr bool operator!=(const Rgb666& that) const
	{
		return !operator==(that);
	}

	// Scoped conversion helpers
	static constexpr Rgb666 FromRgb888(const Rgb888 rgb888)
	{
		const auto r6 = check_cast<uint8_t>(rgb888.red   >> 2);
		const auto g6 = check_cast<uint8_t>(rgb888.green >> 2);
		const auto b6 = check_cast<uint8_t>(rgb888.blue  >> 2);

		return Rgb666(r6, g6, b6);
	}
};

#endif
