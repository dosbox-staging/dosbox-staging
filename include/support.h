/*
 *  Copyright (C) 2002-2020  The DOSBox Team
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
#include <cstdio>
#include <ctype.h>
#include <limits>
#include <stdexcept>
#include <string.h>
#include <string>

#include <SDL.h>

#ifdef _MSC_VER
#define strcasecmp(a, b) _stricmp(a, b)
#define strncasecmp(a, b, n) _strnicmp(a, b, n)
#endif

/*
 *  Converts a string to a finite number (such as float or double).
 *  Returns the number or quiet_NaN, if it could not be parsed.
 *  This function does not attemp to capture exceptions that may
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
	catch (std::invalid_argument &e) {}
	catch (std::out_of_range &e) {}
	return result;
}

// Returns the filename with the prior path stripped.
// Works with both \ and / directory delimeters.
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

// Include a message in assert, similar to static_assert:
#define assertm(exp, msg) assert(((void)msg, exp))
// Use (void) to silent unused warnings.
// https://en.cppreference.com/w/cpp/error/assert

/// Copy a string into C array
///
/// This function copies string pointed by src to fixed-size buffer dst.
/// At most N bytes from src are copied, where N is size of dst.
/// If exactly N bytes are copied, then terminating null byte is put
/// into buffer, thus buffer overrun is prevented.
///
/// Function returns pointer to buffer to be compatible with std::strcpy.
///
/// Usage:
///
///     char buffer[2];
///     safe_strcpy(buffer, "abc");
///     // buffer is filled with "a"

template<size_t N>
char * safe_strcpy(char (& dst)[N], const char * src) noexcept {
	snprintf(dst, N, "%s", src);
	return & dst[0];
}

template<size_t N>
char * safe_strcat(char (& dst)[N], const char * src) noexcept {
	strncat(dst, src, N - strnlen(dst, N) - 1);
	return & dst[0];
}

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

bool ScanCMDBool(char * cmd,char const * const check);
char * ScanCMDRemain(char * cmd);
char * StripWord(char *&cmd);

bool IsHexWord(char * word);
Bits ConvHexWord(char * word);

void trim(std::string& str);
void upcase(std::string &str);
void lowcase(std::string &str);
void strip_punctuation(std::string &str);

bool starts_with(const std::string &prefix, const std::string &str) noexcept;
bool ends_with(const std::string &suffix, const std::string &str) noexcept;

bool is_executable(std::string filename) noexcept;
#endif
