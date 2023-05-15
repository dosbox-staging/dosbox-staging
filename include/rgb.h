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
	// Yields identical values to `(c * 255 + 31) / 63` over the whole input
	// range
	return check_cast<uint8_t>((c * 259 + 33) >> 6);
}

constexpr uint8_t rgb5_to_8(const uint8_t c)
{
	// Yields identical values to `(c * 255 + 15) / 31` over the whole input
	// range
	return check_cast<uint8_t>((c * 527 + 23) >> 6);
}

constexpr uint8_t rgb8_to_6(const uint8_t c)
{
	return check_cast<uint8_t>((c * 253 + 505) >> 10);
}

constexpr uint8_t rgb8_to_5(const uint8_t c)
{
	return check_cast<uint8_t>((c * 249 + 1014) >> 11);
}

constexpr auto num_5bit_values = 32; // 2^5
constexpr auto num_6bit_values = 64; // 2^6

using rgb5_to_8_lut_t = std::array<uint8_t, num_5bit_values>;
using rgb6_to_8_lut_t = std::array<uint8_t, num_6bit_values>;

static constexpr rgb5_to_8_lut_t generate_rgb5_to_8_lut()
{
	rgb5_to_8_lut_t lut = {};
	for (uint8_t c = 0; c < lut.size(); ++c) {
		lut[c] = check_cast<uint8_t>(rgb5_to_8(c));
	}
	return lut;
}

static constexpr rgb6_to_8_lut_t generate_rgb6_to_8_lut()
{
	rgb6_to_8_lut_t lut = {};
	for (uint8_t c = 0; c < lut.size(); ++c) {
		lut[c] = check_cast<uint8_t>(rgb6_to_8(c));
	}
	return lut;
}

inline uint8_t rgb5_to_8_lut(const uint8_t c)
{
	assert(c < num_5bit_values);
	static constexpr auto lut = generate_rgb5_to_8_lut();
	return lut[c];
}

inline uint8_t rgb6_to_8_lut(const uint8_t c)
{
	assert(c < num_6bit_values);
	static constexpr auto lut = generate_rgb6_to_8_lut();
	return lut[c];
}

#endif
