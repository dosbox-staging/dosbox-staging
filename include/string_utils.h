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

#ifndef DOSBOX_STRING_UTILS_H
#define DOSBOX_STRING_UTILS_H

#include "dosbox.h"

#include <cassert>
#include <climits>
#include <cstdarg>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

template <size_t N>
int safe_sprintf(char (&dst)[N], const char* fmt, ...)
        GCC_ATTRIBUTE(format(printf, 2, 3));

template <size_t N>
int safe_sprintf(char (&dst)[N], const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	const int ret = vsnprintf(dst, N, fmt, args);
	va_end(args);
	return ret;
}

/* Copy a string into C array
 *
 * This function copies string pointed by src to a fixed-size buffer dst.
 * At most N bytes from src are copied, where N is size of dst. If exactly
 * N bytes are copied, then terminating null byte is put into dst, thus
 * buffer overrun is prevented.
 *
 * Function returns pointer to buffer to be compatible with strcpy.
 *
 * This is a safer drop-in replacement for strcpy function (when used to fill
 * buffers, whose size is known at compilation time), however some caveats
 * still apply:
 *
 * - src cannot be null, otherwise the behaviour is undefined
 * - dst and src strings must not overlap, otherwise the behaviour is undefined
 * - src string must be null-terminated, otherwise the behaviour is undefined
 *
 * Usage:
 *
 *     char buffer[2];
 *     safe_strcpy(buffer, "abc");
 *     // buffer is filled with "a"
 */
template <size_t N>
char* safe_strcpy(char (&dst)[N], const char* src) noexcept
{
	assert(src != nullptr);
	assert(src < &dst[0] || src > &dst[N - 1]);
	const auto rcode = safe_sprintf(dst, "%s", src);
	return (rcode >= 0) ? &dst[0] : nullptr;
}

template <size_t N>
char* safe_strcat(char (&dst)[N], const char* src) noexcept
{
	strncat(dst, src, N - strnlen(dst, N) - 1);
	return &dst[0];
}

template <size_t N>
size_t safe_strlen(char (&str)[N]) noexcept
{
	static_assert(N != 0, "zero-length arrays are not supported");
	return strnlen(str, N - 1);
}

std::string strip_prefix(const std::string_view str,
                         const std::string_view prefix) noexcept;

std::string strip_suffix(const std::string_view str,
                         const std::string_view suffix) noexcept;

bool find_in_case_insensitive(const std::string& needle, const std::string& haystack);

// Safely terminate a C string at the given offset
//
// Convert code like: stuff[n] = 0;
//  - "is stuff an integer array?"
//  - "are we setting a counter back to 0?"
//  - "is stuff a valid array?"
//
// To: terminate_str_at(stuff, n);
// which is self-documenting about the type (stuff is a string),
// intent (terminating), and where it's being applied (n);
//
template <typename T, typename INDEX_T>
void terminate_str_at(T* str, INDEX_T i) noexcept
{
	// Check that we're only operating on bona-fide C strings
	static_assert(std::is_same_v<T, char> || std::is_same_v<T, wchar_t>,
	              "Can only reset a *char or *wchar_t with the string-terminator");

	// Check that we don't underflow with a negative index
	assert(std::is_unsigned_v<INDEX_T> || i >= 0);

	// Check that we don't dereferrence a null pointer
	assert(str != nullptr);
	str[i] = '\0';
}

// reset a C string with the string-terminator character
template <typename T>
void reset_str(T* str) noexcept
{
	terminate_str_at(str, 0);
}

// Is the ASCII character within the upper nibble?
constexpr bool is_upper_ascii(const char c)
{
	constexpr uint8_t upper_ascii_first = 128;
	constexpr uint8_t upper_ascii_last  = 255;

#if (CHAR_MIN < 0) // char is signed
	static_assert(std::is_signed_v<char> && CHAR_MAX < upper_ascii_last);
	return static_cast<uint8_t>(c) >= upper_ascii_first;

#else // char is unsigned
	static_assert(std::is_unsigned_v<char> && CHAR_MAX == upper_ascii_last);
	return c >= upper_ascii_first;

#endif
}

// Is it an ASCII control character?
constexpr bool is_control_ascii(const char c)
{
	// clang-format off
	// ASCII control characters span the bottom 5 bits plus the DEL character.
	// Ref:https://en.wikipedia.org/wiki/ASCII#Control_characters
	[[maybe_unused]] 
	constexpr char controls_first = 0b0'0000;
	constexpr char controls_last  = 0b1'1111;
	constexpr char delete_char    = 0b111'1111;
	// clang-format on

#if (CHAR_MIN < 0) // char is signed
	static_assert(std::is_signed_v<char> && CHAR_MAX == delete_char);
	return (c >= controls_first && c <= controls_last) ||
#else // char is unsigned
	static_assert(std::is_unsigned_v<char> && CHAR_MAX > delete_char);
	return c <= controls_last ||
#endif
	       // return continues ...
	       c == delete_char;
}

// Is the character within the printable ASCII range?
constexpr bool is_printable_ascii(const char c)
{
	// The printable ASCII range spans the Space to ESC characters.
	// Ref:https://en.wikipedia.org/wiki/ASCII#Printable_characters
	constexpr char space_char  = ' ';
	constexpr char escape_char = 0b111'1110;
	return c >= space_char && c <= escape_char;
}

// Is the character within the standard ASCII range?
constexpr bool is_ascii(const char c)
{
	return is_printable_ascii(c) || is_control_ascii(c);
}

// Is the character with the extended printable ASCII range?
constexpr bool is_extended_printable_ascii(const char c)
{
	// The extended ASCII range spans all 8-bits, leaving the extended
	// printable portion of those being the non-control characters.
	// Ref:https://en.wikipedia.org/wiki/ASCII#8-bit_codes
	return !is_control_ascii(c);
}

bool is_hex_digits(const std::string_view s) noexcept;

bool is_digits(const std::string_view s) noexcept;

void strreplace(char* str, char o, char n);
void ltrim(std::string& str);
char* ltrim(char* str);
char* rtrim(char* str);
char* trim(char* str);
char* upcase(char* str);
char* lowcase(char* str);

inline bool is_empty(const char* str) noexcept
{
	return str[0] == '\0';
}

// case-insensitive comparisons
bool ciequals(const char a, const char b);

// case-insensitive comparison for combinations of
// const char *, const std::string&, and const string_view
template <typename T1, typename T2>
constexpr bool iequals(T1&& a, T2&& b)
{
	using str_t1 = std::conditional_t<std::is_same_v<T1, const std::string&>,
	                                  const std::string&,
	                                  const std::string_view>;

	using str_t2 = std::conditional_t<std::is_same_v<T2, const std::string&>,
	                                  const std::string&,
	                                  const std::string_view>;

	const str_t1 str_a = std::forward<T1>(a);
	const str_t2 str_b = std::forward<T2>(b);

	return std::equal(str_a.begin(), str_a.end(), str_b.begin(), str_b.end(), ciequals);
}

// Performs a "natural" comparison between A and B, which is case-insensitive
// and treats number sequenences as whole numbers. Returns true if A < B. This
// function can be used with higher order sort routines, like std::sort.
//
// Examples:
// - ("abc_2", "ABC_10") -> true, because abc_ matches and 2 < 10.
// - ("xyz_2", "ABC_10") -> false, because 'x' > 'a'.
// - ("abc123", "abc123=") -> true, simply because the first is shorter.
bool natural_compare(const std::string& a, const std::string& b);

char* strip_word(char*& line);
std::string strip_word(std::string& line);

std::string replace(const std::string& str, char old_char, char new_char) noexcept;
void trim(std::string& str, const std::string_view trim_chars = " \r\t\f\n");
void upcase(std::string& str);
void lowcase(std::string& str);
void strip_punctuation(std::string& str);

// Split a string on an arbitrary character delimiter. Absent string content on
// either side of a delimiter is treated as an empty string. For example:
//   split("abc:", ':') returns {"abc", ""}
//   split(":def", ':') returns {"", "def"}
//   split(":", ':') returns {"", ""}
//   split("::", ':') returns {"", "", ""}
std::vector<std::string> split_with_empties(std::string_view seq, char delim);

// Split a string on any character found in delim.
// Delim defaults to all whitespace characters:
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
std::vector<std::string> split(std::string_view seq,
                               std::string_view delims = " \f\n\r\t\v");

std::string join_with_commas(const std::vector<std::string>& items,
                             const std::string_view and_conjunction = "and",
                             const std::string_view end_punctuation = ".");

// Parse the string as an integer or decimal value and return it as a float.
// This API should give us enough numerical range and accuracy for any
// text-based inputs.
//
// For example:
//  - parse_value("100")  returns 100.0f
//  - parse_value("100a") returns empty
//  - parse_value("x10")  returns empty
//  - parse_value("txt")  returns empty
//
std::optional<float> parse_float(const std::string& s);

// Parse the string as an integer and return it as a integer.
//
// For example:
//  - parse_value("100")  returns 100
//  - parse_value("100a") returns empty
//  - parse_value("x10")  returns empty
//  - parse_value("txt")  returns empty
//
std::optional<int> parse_int(const std::string& s, const int base = 10);

// Returned percentage values are unscaled.
std::optional<float> parse_percentage_with_percent_sign(const std::string_view s);
std::optional<float> parse_percentage_with_optional_percent_sign(const std::string_view s);

template <typename... Args>
std::string format_str(const std::string& format, const Args&... args) noexcept
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-security"

	// Perform a non-writing format to determine the size
	const auto required_size = std::snprintf(nullptr, 0, format.c_str(), args...);
	if (required_size <= 0) {
		return {};
	}

	// snprintf's length parameter specifies the maximum number of
	// characters to be written without the trailing null. However, it still
	// writes the trailing null into the buffer, so we need to include that
	// in our allocation.
	const auto out_size = static_cast<size_t>(required_size) +
	                      static_cast<size_t>(1);
	std::string result(out_size, '\0');

	std::snprintf(result.data(), result.size(), format.c_str(), args...);
#pragma clang diagnostic pop

	// The buffer should now have the determined output length plus the
	// terminating zero
	assert(out_size == result.size());

	// Chop off the terminating zero of the C string in the buffer
	result.pop_back();
	return result;
}

template<size_t N>
std::string safe_tostring(char (&str)[N]) noexcept
{
	return std::string(str, safe_strlen(str));
}

inline std::string safe_tostring(const char* str, const std::size_t maxlen) noexcept
{
	return std::string(str, strnlen(str, maxlen));
}

std::string replace_all(const std::string& str, const std::string& from,
                        const std::string& to);

#endif
