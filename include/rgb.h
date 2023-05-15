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

#ifndef DOSBOX_RGB_H
#define DOSBOX_RGB_H

#include <array>
#include <cstdint>

#include "support.h"

constexpr uint8_t rgb6_to_8(const uint8_t c)
{
	return (c * 255 + 31) / 63;
}

constexpr auto num_6bit_values = 64; // 2^6

using rgb6_to_8_lut_t = std::array<uint8_t, num_6bit_values>;

static constexpr rgb6_to_8_lut_t generate_rgb6_to_8_lut()
{
	rgb6_to_8_lut_t lut = {};
	for (uint8_t c = 0; c < lut.size(); ++c) {
		lut[c] = check_cast<uint8_t>(rgb6_to_8(c));
	}
	return lut;
}

inline uint8_t rgb6_to_8_lut(const uint8_t c)
{
	assert(c < num_6bit_values);
	static constexpr auto lut = generate_rgb6_to_8_lut();
	return lut[c];
}

#endif
