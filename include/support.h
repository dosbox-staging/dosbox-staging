/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2024  The DOSBox Staging Team
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
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <thread>
#include <type_traits>
#include <unordered_set>
#include <vector>

#include "std_filesystem.h"

#ifdef _MSC_VER
#define strcasecmp(a, b)     _stricmp(a, b)
#define strncasecmp(a, b, n) _strnicmp(a, b, n)
#endif

#ifdef PAGESIZE
// Some platforms like ppc64 have page sizes of 64K, so uint16_t isn't enough.
constexpr uint32_t host_pagesize = {PAGESIZE};
#else
constexpr uint16_t host_pagesize = 4096;
#endif

constexpr uint16_t dos_pagesize = 4096;

// Some C functions operate on characters but return integers,
// such as 'toupper'. This function asserts that a given int
// is in-range of a char and returns it as such.
char int_to_char(int val);

constexpr bool char_is_negative([[maybe_unused]] char c)
{
#if (CHAR_MIN < 0) // char is signed
	return c < 0;
#else // char is unsigned
	return false;
#endif
}

// Given a case-insensitive drive letter (a/A .. z/Z),
// returns a zero-based index starting at 0 for drive A
// through to 26 for drive Z.
uint8_t drive_index(char drive);

// Convert index (0..26) to a drive letter (uppercase).
char drive_letter(uint8_t index);

char get_drive_letter_from_path(const char* path);

/*
 *  Converts a string to a finite number (such as float or double).
 *  Returns the number or quiet_NaN, if it could not be parsed.
 *  This function does not attempt to capture exceptions that may
 *  be thrown from std::stod(...)
 */
template <typename T>
T to_finite(const std::string& input)
{
	// Defensively set NaN from the get-go
	T result          = std::numeric_limits<T>::quiet_NaN();
	size_t bytes_read = 0;
	try {
		const double interim = std::stod(input, &bytes_read);
		if (!input.empty() && bytes_read == input.size()) {
			result = static_cast<T>(interim);
		}
	}
	// Capture expected exceptions stod may throw
	catch (...) {
		// Do nothing, the return value provides
		// the success or failure indication.
	}
	return result;
}

// Returns the filename with the prior path stripped.
// Works with both \ and / directory delimiters.
std::string get_basename(const std::string& filename);

// Select the nearest unsigned integer capable of holding the given bits
template <int n_bits>
using nearest_uint_t = std::conditional_t<
        n_bits <= std::numeric_limits<uint8_t>::digits, uint8_t,
        std::conditional_t<n_bits <= std::numeric_limits<uint16_t>::digits, uint16_t,
                           std::conditional_t<n_bits <= std::numeric_limits<uint32_t>::digits, uint32_t,
                                              std::conditional_t<n_bits <= std::numeric_limits<uint64_t>::digits,
                                                                 uint64_t, uintmax_t>>>>;

// Select the next larger signed integer type
template <typename T>
using next_int_t = typename std::conditional<
        sizeof(T) == sizeof(int8_t), int16_t,
        typename std::conditional<sizeof(T) == sizeof(int16_t), int32_t, int64_t>::type>::type;

// Select the next large unsigned integer type
template <typename T>
using next_uint_t = typename std::conditional<
        sizeof(T) == sizeof(uint8_t), uint16_t,
        typename std::conditional<sizeof(T) == sizeof(uint16_t), uint32_t, uint64_t>::type>::type;

template <typename cast_t, typename check_t>
constexpr cast_t check_cast(const check_t in)
{
	// Ensure the two types are integers, can't handle floats/doubles
	static_assert(std::numeric_limits<cast_t>::is_integer,
	              "The casting type must be an integer type");
	static_assert(std::numeric_limits<check_t>::is_integer,
	              "The argument must be an integer type");

	// ensure the inbound value is within the limits of the casting type
	assert(std::is_unsigned_v<check_t> ||
	       static_cast<next_int_t<check_t>>(in) >=
	               static_cast<next_int_t<cast_t>>(
	                       std::numeric_limits<cast_t>::min()));

	assert(sizeof(check_t) < sizeof(cast_t) ||
	       static_cast<next_int_t<check_t>>(in) <=
	               static_cast<next_int_t<cast_t>>(
	                       std::numeric_limits<cast_t>::max()));

	return static_cast<cast_t>(in);
}

template <typename T>
std::function<T()> CreateRandomizer(const T min_value, const T max_value);

// Include a message in assert, similar to static_assert:
#define assertm(exp, msg) assert(((void)msg, exp))
// Use (void) to silent unused warnings.
// https://en.cppreference.com/w/cpp/error/assert

// TODO review all remaining uses of this macro
#define safe_strncpy(a, b, n) \
	do { \
		strncpy((a), (b), (n)-1); \
		(a)[(n)-1] = 0; \
	} while (0)

#ifndef HAVE_STRNLEN
constexpr size_t strnlen(const char* str, const size_t max_len)
{
	if (!str) {
		return 0;
	}
	size_t i = 0;
	while (i < max_len && str[i] != '\0') {
		++i;
	}
	return i;
}
#endif

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

// Scans the provided command-line string for the '/'flag and removes it from
// the string, returning if the flag was found and removed.
bool ScanCMDBool(char* cmd, const char* flag);
char* ScanCMDRemain(char* cmd);

bool is_executable_filename(const std::string& filename) noexcept;

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

#define ARRAY_LEN(arr) \
	(static_if_array_then_zero<decltype(arr)>() + \
	 (sizeof(arr) / sizeof(arr[0])))

// Thread-safe replacement for strerror.
//
std::string safe_strerror(int err) noexcept;

void set_thread_name(std::thread& thread, const char* name);

constexpr uint8_t DOS_DATE_months[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

bool is_date_valid(const uint32_t year, const uint32_t month, const uint32_t day);

bool is_time_valid(const uint32_t hour, const uint32_t minute, const uint32_t second);

struct FILE_closer {
	void operator()(FILE* f) noexcept;
};
using FILE_unique_ptr = std::unique_ptr<FILE, FILE_closer>;

FILE* open_file(const char* filename, const char* mode);

// Opens and returns a std::unique_ptr to a FILE, which automatically closes
// itself when it goes out of scope
FILE_unique_ptr make_fopen(const char* fname, const char* mode);

int64_t stdio_size_bytes(FILE* f);
int64_t stdio_size_kb(FILE* f);
int64_t stdio_num_sectors(FILE* f);

const std_fs::path& GetExecutablePath();
std_fs::path GetResourcePath(const std_fs::path& name);
std_fs::path GetResourcePath(const std_fs::path& subdir, const std_fs::path& name);

std::map<std_fs::path, std::vector<std_fs::path>> GetFilesInResource(
        const std_fs::path& res_name, const std::string_view files_ext,
        const bool only_regular_files);

enum class ResourceImportance { Mandatory, Optional };

std::vector<uint8_t> LoadResourceBlob(const std_fs::path& subdir,
                                      const std_fs::path& name,
                                      const ResourceImportance importance);
std::vector<uint8_t> LoadResourceBlob(const std_fs::path& name,
                                      const ResourceImportance importance);

std::vector<std::string> GetResourceLines(const std_fs::path& subdir,
                                          const std_fs::path& name,
                                          const ResourceImportance importance);
std::vector<std::string> GetResourceLines(const std_fs::path& name,
                                          const ResourceImportance importance);

bool path_exists(const std_fs::path& path);

bool is_writable(const std_fs::path& path);
bool is_readable(const std_fs::path& path);
bool is_readonly(const std_fs::path& path);

bool make_writable(const std_fs::path& path);
bool make_readonly(const std_fs::path& path);

template <typename container_t>
bool contains(const container_t& container,
              const typename container_t::value_type& value)
{
	return std::find(container.begin(), container.end(), value) !=
	       container.end();
}

template <typename container_t>
bool contains(const container_t& container, const typename container_t::key_type& key)
{
	return container.find(key) != container.end();
}

// remove duplicates from an unordered container using std::remove_if (C++17)

// The std::remove_if algorithm moves 'failed' elements to the end of the
// container, and then returns an iterator to the new end of the container.
// We can then chop-off all the unwanted elements using a single std::erase
// call (minimizing memory thrashing).

template <typename container_t, typename value_t = typename container_t::value_type>
void remove_duplicates(container_t& c)
{
	std::unordered_set<value_t> s;
	auto val_is_duplicate = [&s](const value_t& value) {
		return s.insert(value).second == false;
	};
	const auto end = std::remove_if(c.begin(), c.end(), val_is_duplicate);
	c.erase(end, c.end());
}

// remove empty() values from a container using std::remove_if (C++17)
template <typename container_t>
void remove_empties(container_t& c)
{
	auto is_empty = [](const auto& item) { return item.empty(); };

	auto new_c_end = std::remove_if(c.begin(), c.end(), is_empty);
	c.erase(std::move(new_c_end), c.end());
}

// Convenience function to cast to the underlying type of an enum class
template <typename enum_t>
constexpr auto enum_val(enum_t e)
{
	static_assert(std::is_enum_v<enum_t>);
	return static_cast<std::underlying_type_t<enum_t>>(e);
}

// Create a buffer (held in a unique_ptr) and aligned pointer, similar to
// "make_unique" but with the desired alignment, array size, and optional
// initialer value, similar to other managed containers' constructors.
//
// For example: auto [buf, ptr] = make_unique_aligned_array<int>(32, 10, -99);
//
// The ptr is 32-byte aligned, points to the first of 10 ints, all of which all
// initialized with the value -99. The buffer will automatically be deallocated
// when it goes out of scope.
//
template <typename T>
std::pair<std::unique_ptr<T[]>, T*> make_unique_aligned_array(
        const size_t byte_alignment, const size_t req_elems, const T& init_val = {});

// This struct can be used in combination with std::visit and std::variant to
// define code for each type specified in the variant.
//
// Example:
//
// std::variant<bool, int> data = true;
// std::string typename = std::visit(Overload{
//     [](bool) { return "bool"; },
//     [](int)  { return "int"; },
// }, data);
//
// typename now contains the value "bool".
template <typename... Ts>
struct Overload : Ts... {
	using Ts::operator()...;
};
template <class... Ts>
Overload(Ts...) -> Overload<Ts...>;

#endif
