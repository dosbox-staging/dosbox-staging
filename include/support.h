/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2021  The DOSBox Staging Team
 *  Copyright (C) 2002-2021  The DOSBox Team
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

#ifndef DOSBOX_SUPPORT_H
#define DOSBOX_SUPPORT_H

#include "dosbox.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <sys/types.h>
#include <thread>
#include <type_traits>
#include <vector>

#ifdef _MSC_VER
#define strcasecmp(a, b) _stricmp(a, b)
#define strncasecmp(a, b, n) _strnicmp(a, b, n)
#endif

// Some C functions operate on characters but return integers,
// such as 'toupper'. This function asserts that a given int
// is in-range of a char and returns it as such.
char int_to_char(int val);

// Given a case-insensitive drive letter (a/A .. z/Z),
// returns a zero-based index starting at 0 for drive A
// through to 26 for drive Z.
uint8_t drive_index(char drive);

// Convert index (0..26) to a drive letter (uppercase).
char drive_letter(uint8_t index);

/*
 *  Converts a string to a finite number (such as float or double).
 *  Returns the number or quiet_NaN, if it could not be parsed.
 *  This function does not attempt to capture exceptions that may
 *  be thrown from std::stod(...)
 */
template<typename T>
T to_finite(const std::string& input) {
	// Defensively set NaN from the get-go
	T result = std::numeric_limits<T>::quiet_NaN();
	size_t bytes_read = 0;
	try {
		const double interim = std::stod(input, &bytes_read);
		if (!input.empty() && bytes_read == input.size())
			result = static_cast<T>(interim);
	}
	// Capture expected exceptions stod may throw
	catch (MAYBE_UNUSED std::invalid_argument &e) {
	} catch (MAYBE_UNUSED std::out_of_range &e) {
	}
	return result;
}

// Returns the filename with the prior path stripped.
// Works with both \ and / directory delimiters.
std::string get_basename(const std::string& filename);

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

inline int iround(double x) {
	assert(std::isfinite(x));
	assert(x >= (std::numeric_limits<int>::min)());
	assert(x <= (std::numeric_limits<int>::max)());
	return static_cast<int>(round(x));
}

// Select the next larger signed integer type
template <typename T>
using next_int_t = typename std::conditional<
        sizeof(T) == sizeof(int8_t),
        int16_t,
        typename std::conditional<sizeof(T) == sizeof(int16_t), int32_t, int64_t>::type>::type;

// Select the next large unsigned integer type
template <typename T>
using next_uint_t = typename std::conditional<
        sizeof(T) == sizeof(uint8_t),
        uint16_t,
        typename std::conditional<sizeof(T) == sizeof(uint16_t), uint32_t, uint64_t>::type>::type;

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

template <typename cast_t, typename check_t>
constexpr cast_t check_cast(const check_t in)
{
	// Ensure the two types are integers, can't handle floats/doubles
	static_assert(std::numeric_limits<cast_t>::is_integer,
	              "The casting type must be an integer type");
	static_assert(std::numeric_limits<check_t>::is_integer,
	              "The argument must be an integer type");

	// ensure the inbound value is within the limits of the casting type
	assert(static_cast<next_int_t<check_t>>(in) >=
	       static_cast<next_int_t<cast_t>>(std::numeric_limits<cast_t>::min()));
	assert(static_cast<next_int_t<check_t>>(in) <=
	       static_cast<next_int_t<cast_t>>(std::numeric_limits<cast_t>::max()));

	return static_cast<cast_t>(in);
}

// Include a message in assert, similar to static_assert:
#define assertm(exp, msg) assert(((void)msg, exp))
// Use (void) to silent unused warnings.
// https://en.cppreference.com/w/cpp/error/assert

// TODO review all remaining uses of this macro
#define safe_strncpy(a,b,n) do { strncpy((a),(b),(n)-1); (a)[(n)-1] = 0; } while (0)

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

// Clamp: given a value that can be compared with the given minimum and maximum
//        values, this function will:
//          * return the value if it's in-between or equal to either bounds, or
//          * return either bound depending on which bound the value is beyond
template <class T> T clamp(const T& n, const T& lower, const T& upper) {
	return std::max<T>(lower, std::min<T>(n, upper));
}

void strreplace(char * str,char o,char n);
char *ltrim(char *str);
char *rtrim(char *str);
char *trim(char * str);
char * upcase(char * str);
char * lowcase(char * str);

inline bool is_empty(const char *str) noexcept
{
	return str[0] == '\0';
}

bool ScanCMDBool(char * cmd,char const * const check);
char * ScanCMDRemain(char * cmd);
char * StripWord(char *&cmd);

bool IsHexWord(char * word);
Bits ConvHexWord(char * word);

std::string replace(const std::string &str, char old_char, char new_char) noexcept;
void trim(std::string& str);
void upcase(std::string &str);
void lowcase(std::string &str);
void strip_punctuation(std::string &str);

// Split a string on an arbitrary character delimiter. Absent string content on
// either side of a delimiter is treated as an empty string. For example:
//   split("abc:", ':') returns {"abc", ""}
//   split(":def", ':') returns {"", "def"}
//   split(":", ':') returns {"", ""}
//   split("::", ':') returns {"", "", ""}
std::vector<std::string> split(const std::string &seq, const char delim);

// Split a string on whitespace, where whitespace can be any of the following:
// ' '    (0x20)  space (SPC)
// '\t'   (0x09)  horizontal tab (TAB)
// '\n'   (0x0a)  newline (LF)
// '\v'   (0x0b)  vertical tab (VT)
// '\f'   (0x0c)  feed (FF)
// '\r'   (0x0d)  carriage return (CR)
// Absent string content on either side of a delimiter is omitted. For example:
//   split("abc") returns {"abc"}
//   split("  a   b   c  ") returns {"a", "b", "c"}
//   split("\t \n abc \r \v def \f \v ") returns {"abc", "def"}
//   split("a\tb\nc\vd e\rf") returns {"a", "b", "c", "d", "e", "f"}
//   split("  ") returns {}
//   split(" ") returns {}
std::vector<std::string> split(const std::string &seq);

bool is_executable_filename(const std::string &filename) noexcept;


// Use ARRAY_LEN macro to safely calculate number of elements in a C-array.
// This macro can be used in a constant expressions, even if array is a
// non-static class member:
//
//   constexpr auto n = ARRAY_LEN(my_array);
//
template <typename T>
constexpr size_t static_if_array_then_zero()
{
	static_assert(std::is_array<T>::value, "not an array type");
	return 0;
}

#define ARRAY_LEN(arr)                                                         \
	(static_if_array_then_zero<decltype(arr)>() +                          \
	 (sizeof(arr) / sizeof(arr[0])))

// Thread-safe replacement for strerror.
//
std::string safe_strerror(int err) noexcept;

void set_thread_name(std::thread &thread, const char *name);

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

#endif
