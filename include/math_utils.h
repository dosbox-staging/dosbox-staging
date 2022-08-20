/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2022  The DOSBox Staging Team
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
constexpr int wrap(int val, int const lower_bound, int const upper_bound)
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
std::function<T()> CreateRandomizer(const T min_value, const T max_value);

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

inline double decibel_to_gain(const double decibel)
{
	return pow(10.0, decibel / 20.0);
}

inline double gain_to_decibel(const double gain)
{
	return 20.0 * log(gain) / log(10.0);
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
