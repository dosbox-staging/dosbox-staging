// SPDX-FileCopyrightText:  2023-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_RGB_H
#define DOSBOX_RGB_H

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>

#include "misc/support.h"

static constexpr auto Rgb5Max = 31;
static constexpr auto Rgb6Max = 63;
static constexpr auto Rgb8Max = 255;

//***************************************************************************
// Conversion between 8-bit and 5/6-bit RGB values
//***************************************************************************

constexpr uint8_t rgb6_to_8(const uint8_t c)
{
	// Yields identical values to `(c * 255 + 31) / 63` over the whole input
	// range
	assert(c <= Rgb6Max);
	return check_cast<uint8_t>((c * 259 + 33) >> 6);
}

constexpr uint8_t rgb5_to_8(const uint8_t c)
{
	// Yields identical values to `(c * 255 + 15) / 31` over the whole input
	// range
	assert(c <= Rgb5Max);
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

using rgb5_to_8_lut_t = std::array<uint8_t, Rgb5Max + 1>;
using rgb6_to_8_lut_t = std::array<uint8_t, Rgb6Max + 1>;

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
	assert(c <= Rgb5Max);
	static constexpr auto lut = generate_rgb5_to_8_lut();
	return lut[c];
}

inline uint8_t rgb6_to_8_lut(const uint8_t c)
{
	assert(c <= Rgb6Max);
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

using srgb8_to_lin_lut_t = std::array<float, Rgb8Max + 1>;

static srgb8_to_lin_lut_t generate_srgb8_to_lin_lut()
{
	srgb8_to_lin_lut_t lut = {};

	for (uint16_t i = 0; i < lut.size(); ++i) {
		const auto srgb = static_cast<float>(i) / (lut.size() - 1); //-V609

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

static constexpr auto LinToSrgb8LutSize = 16384;

using lin_to_srgb8_lut_t = std::array<uint8_t, LinToSrgb8LutSize>;

static uint16_t lin_to_srgb8_lut_key(float c)
{
	const auto key = static_cast<uint16_t>(roundf(c * (LinToSrgb8LutSize - 1)));

	assert(key < LinToSrgb8LutSize);
	return key;
}

static lin_to_srgb8_lut_t generate_lin_to_srgb8_lut()
{
	lin_to_srgb8_lut_t lut = {};

	for (int16_t i = 0; i < LinToSrgb8LutSize; ++i) {
		const auto lin  = (1.0f / (LinToSrgb8LutSize - 1)) * i;
		const auto key  = lin_to_srgb8_lut_key(lin);
		const auto srgb = linear_to_srgb(lin) * Rgb8Max;

		lut[key] = static_cast<uint8_t>(roundf(srgb));
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

