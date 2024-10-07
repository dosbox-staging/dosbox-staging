/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2024  The DOSBox Staging Team
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

#ifndef DOSBOX_MATH_UTILS_H
#define DOSBOX_MATH_UTILS_H

#include <algorithm>
#include <cassert>
#include <cmath>

#include "support.h"

// Clamp: given a value that can be compared with the given minimum and maximum
//        values, this function will:
//          * return the value if it's in-between or equal to either bounds, or
//          * return either bound depending on which bound the value is beyond
template <class T> T clamp(const T& n, const T& lower, const T& upper) {
	return std::max<T>(lower, std::min<T>(n, upper));
}

/*
  Returns a number wrapped between the lower and upper bounds.
   - wrap(-1, 0, 4); // Returns 4
   - wrap(5, 0, 4); // Returns 0

  All credit to Charles Bailey, https://stackoverflow.com/a/707426
*/
constexpr int wrap(int val, const int lower_bound, const int upper_bound)
{
	const auto range_size = upper_bound - lower_bound + 1;
	if (val < lower_bound)
		val += range_size * ((lower_bound - val) / range_size + 1);

	return lower_bound + (val - lower_bound) % range_size;
}

// Unsigned-only integer division with ceiling
template<typename T1, typename T2>
inline constexpr T1 ceil_udivide(const T1 x, const T2 y) noexcept {
	static_assert(std::is_unsigned<T1>::value, "First parameter should be unsigned");
	static_assert(std::is_unsigned<T2>::value, "Second parameter should be unsigned");
	return (x != 0) ? 1 + ((x - 1) / y) : 0;
	// https://stackoverflow.com/a/2745086
}

// Signed-only integer division with ceiling
template<typename T1, typename T2>
inline constexpr T1 ceil_sdivide(const T1 x, const T2 y) noexcept {
	static_assert(std::is_signed<T1>::value, "First parameter should be signed");
	static_assert(std::is_signed<T2>::value, "Second parameter should be signed.");
	return x / y + (((x < 0) ^ (y > 0)) && (x % y));
	// https://stackoverflow.com/a/33790603
}

template <typename T>
std::function<T()> create_randomizer(const T min_value, const T max_value);

inline int iround(double x)
{
	assert(std::isfinite(x));
	assert(x >= (std::numeric_limits<int>::min)());
	assert(x <= (std::numeric_limits<int>::max)());
	return static_cast<int>(round(x));
}

inline int iroundf(const float x)
{
	assert(std::isfinite(x));
	assert(x >= static_cast<float>((std::numeric_limits<int>::min)()));
	assert(x <= static_cast<float>((std::numeric_limits<int>::max)()));
	return static_cast<int>(roundf(x));
}

inline int ifloor(double x)
{
	assert(std::isfinite(x));
	assert(x >= (std::numeric_limits<int>::min)());
	assert(x <= (std::numeric_limits<int>::max)());
	return static_cast<int>(floor(x));
}

inline int ifloor(const float x)
{
	assert(std::isfinite(x));
	assert(x >= static_cast<float>((std::numeric_limits<int>::min)()));
	assert(x <= static_cast<float>((std::numeric_limits<int>::max)()));
	return static_cast<int>(floorf(x));
}

inline int iceil(double x)
{
	assert(std::isfinite(x));
	assert(x >= (std::numeric_limits<int>::min)());
	assert(x <= (std::numeric_limits<int>::max)());
	return static_cast<int>(ceil(x));
}

inline int iceil(const float x)
{
	assert(std::isfinite(x));
	assert(x >= static_cast<float>((std::numeric_limits<int>::min)()));
	assert(x <= static_cast<float>((std::numeric_limits<int>::max)()));
	return static_cast<int>(ceilf(x));
}

// Determine if two numbers are equal "enough" based on an epsilon value.
// Uses a dynamic adjustment based on the magnitude of the numbers.
// Based on ideas from Bruce Dawson's blog post:
// https://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/
inline bool are_almost_equal_relative(
        const double a, const double b,
        const double epsilon = std::numeric_limits<double>::epsilon())
{
	const auto diff    = std::fabs(a - b);
	const auto largest = std::max(std::fabs(a), std::fabs(b));

	return diff <= largest * epsilon;
}

// Left-shifts a signed value by a given amount, with overflow detection
template <typename T1, typename T2>
constexpr T1 left_shift_signed(T1 value, T2 amount)
{
	// Ensure we're using a signed type
	static_assert(std::is_signed<T1>::value, "T1 must be signed");

	// Ensure the two types are integers
	static_assert(std::numeric_limits<T1>::is_integer, "T1 must be an integer type");
	static_assert(std::numeric_limits<T2>::is_integer, "T2 must be an integer type");

	// Ensure the amount we're shifting isn't negative
	assert(amount >= 0);

	// Ensure the amount we're shifting doesn't exceed the value's bit-size
	assert(amount <= std::numeric_limits<T1>::digits);

#if defined(NDEBUG)
	// For release builds, simply cast the value to the unsigned-equivalent
	// to ensure performance isn't impacted. Debug and UBSAN builds catch issues.
	typedef typename std::make_unsigned<T1>::type unsigned_T1;
	const auto shifted = static_cast<unsigned_T1>(value) << amount;

#else
	// Ensure we can accommodate the value being shifted
	static_assert(sizeof(T1) <= sizeof(next_uint_t<T1>),
	              "T1 cannot be a larger than its next larger type");

	// cast the value to the next larger unsigned-type before shifting
	const auto shifted = static_cast<next_uint_t<T1>>(value) << amount;

	// Ensure the value is in-bounds of its signed limits
	assert(static_cast<next_int_t<T1>>(shifted) >= (std::numeric_limits<T1>::min)());
	assert(static_cast<next_int_t<T1>>(shifted) <= (std::numeric_limits<T1>::max)());
#endif

	// Cast it back to its original type
	return static_cast<T1>(shifted);
}

template <typename T>
constexpr int8_t clamp_to_int8(const T val)
{
	static_assert(!std::is_same_v<T, int8_t>,
	              "clamping unnecessary: val is already an int8_t");

	constexpr auto min_val = static_cast<T>(std::is_signed_v<T> ? INT8_MIN : 0);
	constexpr auto max_val = static_cast<T>(INT8_MAX);
	return static_cast<int8_t>(std::clamp(val, min_val, max_val));
}

template <typename T>
constexpr uint8_t clamp_to_uint8(const T val)
{
	static_assert(!std::is_same_v<T, uint8_t>,
	              "clamping unnecessary: val is already an uint8_t");

	constexpr auto min_val = static_cast<T>(0);
	constexpr auto max_val = static_cast<T>(UINT8_MAX);
	return static_cast<uint8_t>(std::clamp(val, min_val, max_val));
}

template <typename T>
constexpr int16_t clamp_to_int16(const T val)
{
	static_assert(!std::is_same_v<T, int16_t>,
	              "clamping unnecessary: val is already an int16_t");

	static_assert(sizeof(T) >= sizeof(int16_t),
	              "clamping unnecessary: val type fits within int16_t");

	constexpr auto min_val = static_cast<T>(std::is_signed_v<T> ? INT16_MIN : 0);
	constexpr auto max_val = static_cast<T>(INT16_MAX);
	return static_cast<int16_t>(std::clamp(val, min_val, max_val));
}

template <typename T>
constexpr uint16_t clamp_to_uint16(const T val)
{
	static_assert(!std::is_same_v<T, uint16_t>,
	              "clamping unnecessary: val is already an uint16_t");

	static_assert(std::is_signed_v<T> || sizeof(T) > sizeof(uint16_t),
	              "clamping unnecessary: val type fits within uint16_t");

	constexpr auto min_val = static_cast<T>(0);
	constexpr auto max_val = static_cast<T>(UINT16_MAX);
	return static_cast<uint16_t>(std::clamp(val, min_val, max_val));
}

template <typename T>
constexpr int32_t clamp_to_int32(const T val)
{
	static_assert(!std::is_same_v<T, int32_t>,
	              "clamping unnecessary: val is already an int32_t");

	static_assert(sizeof(T) >= sizeof(int32_t),
	              "clamping unnecessary: val type fits within int32_t");

	constexpr auto min_val = static_cast<T>(std::is_signed_v<T> ? INT32_MIN : 0);
	constexpr auto max_val = static_cast<T>(INT32_MAX);
	return static_cast<int32_t>(std::clamp(val, min_val, max_val));
}

template <typename T>
constexpr uint32_t clamp_to_uint32(const T val)
{
	static_assert(!std::is_same_v<T, uint32_t>,
	              "clamping unnecessary: val is already an uint32_t");

	static_assert(std::is_signed_v<T> || sizeof(T) > sizeof(uint32_t),
	              "clamping unnecessary: val type fits within uint32_t");

	constexpr auto min_val = static_cast<T>(0);
	constexpr auto max_val = static_cast<T>(UINT32_MAX);
	return static_cast<uint32_t>(std::clamp(val, min_val, max_val));
}

constexpr uint8_t low_nibble(const uint8_t byte)
{
	return byte & 0x0f;
}

constexpr uint8_t high_nibble(const uint8_t byte)
{
	return (byte & 0xf0) >> 4;
}

constexpr uint8_t low_byte(const uint16_t word)
{
	return static_cast<uint8_t>(word & 0xff);
}

constexpr uint8_t high_byte(const uint16_t word)
{
	return static_cast<uint8_t>((word & 0xff00) >> 8);
}

inline float decibel_to_gain(const float decibel)
{
	return powf(10.0f, decibel / 20.0f);
}

inline float gain_to_decibel(const float gain)
{
	return 20.0f * logf(gain) / logf(10.0f);
}

// A wrapper to convert a scalar gain to a percentage.
// This avoids having a bunch of magic *100.0 throughout the code.
constexpr float gain_to_percentage(const float gain)
{
	return gain * 100.0f;
}

// A wrapper to convert a percentage into a scalar gain.
// This avoids having a bunch of magic /100.0 throughout the code.
constexpr float percentage_to_gain(const float percentage)
{
	return percentage / 100.0f;
}

template <typename T>
constexpr T lerp(const T a, const T b, const T t)
{
	return a * (1 - t) + b * t;
}

template <typename T>
constexpr T invlerp(const T a, const T b, const T v)
{
	return (v - a) / (b - a);
}

template <typename T>
constexpr T remap(const T in_min, const T in_max, const T out_min,
                  const T out_max, const T v)
{
	const auto t = invlerp(in_min, in_max, v);
	return lerp(out_min, out_max, t);
}

inline std::vector<uint8_t> ascii_to_bcd(const std::string& string)
{
	std::vector<uint8_t> bcd = {};
	const size_t iterations = string.size() / 2;
	for (size_t i = 0; i < iterations; ++i) {
		const uint8_t high_nibble = string[i * 2] - '0';
		const uint8_t low_nibble = string[(i * 2) + 1] - '0';
		bcd.push_back((high_nibble << 4) | (low_nibble & 0xF));
	}
	if (string.size() % 2) {
		bcd.push_back(static_cast<uint8_t>(string.back()) << 4);
	}
	return bcd;
}

// Explicit instantiations for lerp, invlerp, and remap

template float lerp<float>(const float a, const float b, const float t);
template double lerp<double>(const double a, const double b, const double t);

template float invlerp<float>(const float a, const float b, const float v);
template double invlerp<double>(const double a, const double b, const double v);

template float remap<float>(const float in_min, const float in_max,
                            const float out_min, const float out_max, const float v);

template double remap<double>(const double in_min, const double in_max,
                              const double out_min, const double out_max,
                              const double v);
							
#endif
