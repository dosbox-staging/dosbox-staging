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
#include <cmath>
#include <cstdint>

#include "support.h"

//***************************************************************************
// Conversion between 8-bit and 5/6-bit RGB values
//***************************************************************************

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

static constexpr auto num_5bit_values = 32; // 2^5
static constexpr auto num_6bit_values = 64; // 2^6

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

//***************************************************************************
// Conversion between sRGB to linear RGB
//***************************************************************************

// Both the input and output ranges are 0.0f to 1.0f
inline float srgb_to_linear(const float c)
{
	return (c <= 0.04045f) ? (c / 12.92f) : powf((c + 0.055f) / 1.055f, 2.4f);
}

// Both the input and output ranges are 0.0f to 1.0f
inline float linear_to_srgb(const float c)
{
	return (c <= 0.0031308f) ? (c * 12.92f)
	                         : (1.055f * powf(c, 1.0f / 2.4f) - 0.055f);
}

//***************************************************************************
// LUT-backed 8-bit sRGB to linear RGB conversion
//***************************************************************************

constexpr auto num_8bit_values = 256; // 2^8

using srgb8_to_lin_lut_t = std::array<float, num_8bit_values>;

static constexpr srgb8_to_lin_lut_t generate_srgb8_to_lin_lut()
{
	srgb8_to_lin_lut_t lut = {};

	for (uint16_t i = 0; i < lut.size(); ++i) {
		const auto srgb = static_cast<float>(i) / (lut.size() - 1);

		lut[i] = srgb_to_linear(srgb);
	}
	return lut;
}

// Input range is 0-255 (8-bit RGB), output range is 0.0f to 1.0f
inline float srgb8_to_linear_lut(const uint8_t c)
{
	static const auto lut = generate_srgb8_to_lin_lut();
	return lut[c];
}

//***************************************************************************
// LUT-backed linear RGB to 8-bit sRGB conversion
//***************************************************************************

static constexpr auto max_8bit_value        = 255;
static constexpr auto lin_to_srgb8_lut_size = 16384;

using lin_to_srgb8_lut_t = std::array<uint8_t, lin_to_srgb8_lut_size>;

static uint16_t lin_to_srgb8_lut_key(float c)
{
	const auto key = static_cast<uint16_t>(
	        round(c * (lin_to_srgb8_lut_size - 1)));

	assert(key >= 0 && key < lin_to_srgb8_lut_size);
	return key;
}

static lin_to_srgb8_lut_t generate_lin_to_srgb8_lut()
{
	lin_to_srgb8_lut_t lut = {};

	for (auto i = 0; i < lin_to_srgb8_lut_size; ++i) {
		const auto lin  = (1.0f / (lin_to_srgb8_lut_size - 1)) * i;
		const auto key  = lin_to_srgb8_lut_key(lin);
		const auto srgb = linear_to_srgb(lin) * max_8bit_value;

		lut[key] = static_cast<uint8_t>(round(srgb));
	}
	return lut;
}

// Input range is 0.0f to 1.0f, output range is 0-255 (8-bit RGB)
inline uint8_t linear_to_srgb8_lut(const float c)
{
	static const auto lut = generate_lin_to_srgb8_lut();

	const auto key = lin_to_srgb8_lut_key(c);
	return lut[key];
}

#endif

