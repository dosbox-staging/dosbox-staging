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

#include "support.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <climits>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <system_error>
#include <functional>
#include <stdexcept>
#include <string>

#include "cross.h"
#include "debug.h"
#include "fs_utils.h"
#include "video.h"

#include "whereami.h"

char int_to_char(int val)
{
	// To handle inbound values cast from unsigned chars, permit a slightly
	// wider range to avoid triggering the assert when processing international
	// ASCII values between 128 and 255.
	assert(val >= CHAR_MIN && val <= UCHAR_MAX);
	return static_cast<char>(val);
}

uint8_t drive_index(char drive)
{
	const auto drive_letter = int_to_char(toupper(drive));
	// Confirm the provided drive is valid
	assert(drive_letter >= 'A' && drive_letter <= 'Z');
	return static_cast<uint8_t>(drive_letter - 'A');
}

char drive_letter(uint8_t index)
{
	assert(index <= 26);
	return 'A' + index;
}

std::string get_basename(const std::string &filename)
{
	// Guard against corner cases: '', '/', '\', 'a'
	if (filename.length() <= 1)
		return filename;

	// Find the last slash, but if not is set to zero
	size_t slash_pos = filename.find_last_of("/\\");

	// If the slash is the last character
	if (slash_pos == filename.length() - 1)
		slash_pos = 0;

	// Otherwise if the slash is found mid-string
	else if (slash_pos > 0)
		slash_pos++;
	return filename.substr(slash_pos);
}


void upcase(std::string &str) {
	int (*tf)(int) = std::toupper;
	std::transform(str.begin(), str.end(), str.begin(), tf);
}

void lowcase(std::string &str) {
	int (*tf)(int) = std::tolower;
	std::transform(str.begin(), str.end(), str.begin(), tf);
}

bool is_executable_filename(const std::string &filename) noexcept
{
	const size_t n = filename.length();
	if (n < 4)
		return false;
	if (filename[n - 4] != '.')
		return false;
	std::string sfx = filename.substr(n - 3);
	lowcase(sfx);
	return (sfx == "exe" || sfx == "bat" || sfx == "com");
}

std::string replace(const std::string &str, char old_char, char new_char) noexcept
{
	std::string new_str = str;
	for (char &c : new_str)
		if (c == old_char)
			c = new_char;
	return str;
}

void trim(std::string &str)
{
	constexpr char whitespace[] = " \r\t\f\n";
	const auto empty_pfx = str.find_first_not_of(whitespace);
	if (empty_pfx == std::string::npos) {
		str.clear(); // whole string is filled with whitespace
		return;
	}
	const auto empty_sfx = str.find_last_not_of(whitespace);
	str.erase(empty_sfx + 1);
	str.erase(0, empty_pfx);
}

std::vector<std::string> split(const std::string &seq, const char delim)
{
	std::vector<std::string> words;
	if (seq.empty())
		return words;

	// count delimeters to reserve space in our vector of words
	const size_t n = 1u + std::count(seq.begin(), seq.end(), delim);
	words.reserve(n);

	std::string::size_type head = 0;
	while (head != std::string::npos) {
		const auto tail = seq.find_first_of(delim, head);
		const auto word_len = tail - head;
		words.emplace_back(seq.substr(head, word_len));
		if (tail == std::string::npos) {
			break;
		}
		head += word_len + 1;
	}

	// did we reserve the exact space needed?
	assert(n == words.size());

	return words;
}

std::vector<std::string> split(const std::string &seq)
{
	std::vector<std::string> words;
	if (seq.empty())
		return words;

	constexpr auto whitespace = " \f\n\r\t\v";

	// count words to reserve space in our vector
	size_t n = 0;
	auto head = seq.find_first_not_of(whitespace, 0);
	while (head != std::string::npos) {
		const auto tail = seq.find_first_of(whitespace, head);
		head = seq.find_first_not_of(whitespace, tail);
		++n;
	}
	words.reserve(n);

	// populate the vector with the words
	head = seq.find_first_not_of(whitespace, 0);
	while (head != std::string::npos) {
		const auto tail = seq.find_first_of(whitespace, head);
		words.emplace_back(seq.substr(head, tail - head));
		head = seq.find_first_not_of(whitespace, tail);
	}

	// did we reserve the exact space needed?
	assert(n == words.size());

	return words;
}

void strip_punctuation(std::string &str) {
	str.erase(
		std::remove_if(
			str.begin(),
			str.end(),
			[](unsigned char c){ return std::ispunct(c); }),
		str.end());
}

/*
	Ripped some source from freedos for this one.

*/


/*
 * replaces all instances of character o with character c
 */


void strreplace(char * str,char o,char n) {
	while (*str) {
		if (*str==o) *str=n;
		str++;
	}
}
char *ltrim(char *str) {
	while (*str && isspace(*reinterpret_cast<unsigned char*>(str))) str++;
	return str;
}

char *rtrim(char *str) {
	char *p;
	p = strchr(str, '\0');
	while (--p >= str && isspace(*reinterpret_cast<unsigned char*>(p))) {};
	p[1] = '\0';
	return str;
}

char *trim(char *str) {
	return ltrim(rtrim(str));
}

char * upcase(char * str) {
    for (char* idx = str; *idx ; idx++) *idx = toupper(*reinterpret_cast<unsigned char*>(idx));
    return str;
}

char * lowcase(char * str) {
	for(char* idx = str; *idx ; idx++)  *idx = tolower(*reinterpret_cast<unsigned char*>(idx));
	return str;
}

bool ScanCMDBool(char * cmd, char const * const check)
{
	if (cmd == nullptr)
		return false;
	char *scan = cmd;
	const size_t c_len = strlen(check);
	while ((scan = strchr(scan,'/'))) {
		/* found a / now see behind it */
		scan++;
		if (strncasecmp(scan,check,c_len)==0 && (scan[c_len]==' ' || scan[c_len]=='\t' || scan[c_len]=='/' || scan[c_len]==0)) {
		/* Found a math now remove it from the string */
			memmove(scan-1,scan+c_len,strlen(scan+c_len)+1);
			trim(scan-1);
			return true;
		}
	}
	return false;
}

/* This scans the command line for a remaining switch and reports it else returns 0*/
char * ScanCMDRemain(char * cmd) {
	char * scan,*found;;
	if ((scan=found=strchr(cmd,'/'))) {
		while ( *scan && !isspace(*reinterpret_cast<unsigned char*>(scan)) ) scan++;
		*scan=0;
		return found;
	} else return 0;
}

char * StripWord(char *&line) {
	char * scan=line;
	scan=ltrim(scan);
	if (*scan=='"') {
		char * end_quote=strchr(scan+1,'"');
		if (end_quote) {
			*end_quote=0;
			line=ltrim(++end_quote);
			return (scan+1);
		}
	}
	char * begin=scan;
	for (char c = *scan ;(c = *scan);scan++) {
		if (isspace(*reinterpret_cast<unsigned char*>(&c))) {
			*scan++=0;
			break;
		}
	}
	line=scan;
	return begin;
}

Bits ConvHexWord(char * word) {
	Bitu ret=0;
	while (char c=toupper(*reinterpret_cast<unsigned char*>(word))) {
		ret*=16;
		if (c>='0' && c<='9') ret+=c-'0';
		else if (c>='A' && c<='F') ret+=10+(c-'A');
		word++;
	}
	return ret;
}

static char e_exit_buf[1024]; // greater scope as else it doesn't always gets
                              // thrown right
void E_Exit(const char *format, ...)
{
#if C_DEBUG && C_HEAVY_DEBUG
	DEBUG_HeavyWriteLogInstruction();
#endif
	va_list msg;
	va_start(msg, format);
	vsnprintf(e_exit_buf, ARRAY_LEN(e_exit_buf), format, msg);
	va_end(msg);
	ABORT_F("%s", e_exit_buf);
}

/* Overloaded function to handle different return types of POSIX and GNU
 * strerror_r variants */
[[maybe_unused]] static const char *strerror_result(int retval, const char *err_str)
{
	return retval == 0 ? err_str : nullptr;
}
[[maybe_unused]] static const char *strerror_result(const char *err_str, [[maybe_unused]] const char *buf)
{
	return err_str;
}

std::string safe_strerror(int err) noexcept
{
	char buf[128];
#if defined(WIN32)
	// C11 version; unavailable in C++14 in general.
	strerror_s(buf, ARRAY_LEN(buf), err);
	return buf;
#else
	return strerror_result(strerror_r(err, buf, ARRAY_LEN(buf)), buf);
#endif
}

void set_thread_name([[maybe_unused]] std::thread& thread, [[maybe_unused]] const char *name)
{
#if defined(HAVE_PTHREAD_SETNAME_NP) && defined(_GNU_SOURCE)
	assert(strlen(name) < 16);
	pthread_t handle = thread.native_handle();
	pthread_setname_np(handle, name);
#endif
}

bool ends_with(const std::string &str, const std::string &suffix) noexcept
{
	return (str.size() >= suffix.size() &&
	        str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0);
}

// Search for the needle in the haystack, case insensitive.
bool find_in_case_insensitive(const std::string &needle, const std::string &haystack)
{
	const auto it = std::search(haystack.begin(), haystack.end(),
	                            needle.begin(), needle.end(),
	                            [](char ch1, char ch2) {
		                            return std::toupper(ch1) ==
		                                   std::toupper(ch2);
	                            });
	return (it != haystack.end());
}

void FILE_closer::operator()(FILE *f) noexcept
{
	if (f) {
		fclose(f);
	}
}

FILE_unique_ptr make_fopen(const char *fname, const char *mode)
{
	FILE *f = fopen(fname, mode);
	return f ? FILE_unique_ptr(f) : nullptr;
}

const std_fs::path &GetExecutablePath()
{
	static std_fs::path exe_path;
	if (exe_path.empty()) {
		int length = wai_getExecutablePath(nullptr, 0, nullptr);
		std::string s;
		s.resize(check_cast<uint16_t>(length));
		wai_getExecutablePath(&s[0], length, nullptr);
		exe_path = std_fs::path(s).parent_path();
		assert(!exe_path.empty());
	}
	return exe_path;
}

bool is_path_a_root_path(const std_fs::path &test_path, std_fs::path &root_path)
{

// Special handling for GCC on Windows
#if defined(WIN32) && defined(__GNUC__) && !defined(__clang__)
	const bool is_root_path = (test_path.string().length() == 3 && test_path.string()[1] == ':');
	root_path = is_root_path ? test_path : std_fs::path("");
	return is_root_path;
#endif

	// Try to discover the root path dynamically, which means we can handle
	// all variety of filesystems like MorphOS, AmigaOS, etc.
	std::error_code ec;
	const auto as_canonical_path = std_fs::canonical(test_path, ec);
	if (!as_canonical_path.empty() && as_canonical_path.has_root_directory())
		root_path = as_canonical_path.root_directory();
	else if (!as_canonical_path.empty() && as_canonical_path.has_root_path())
		root_path = as_canonical_path.root_path();
	else if (test_path.has_root_directory())
		root_path = test_path.root_directory();
	else if (test_path.has_root_path())
		root_path = test_path.root_path();
	else if (std_fs::current_path(ec).has_root_path())
		root_path = std_fs::current_path(ec).root_path();

	// Otherwise fallback to our best guess of the root path
	if (root_path.empty())
#if defined(WIN32)
		root_path = "C:/";
#else
		root_path = "/";
#endif

	assert(!root_path.empty());
	return std_fs::equivalent(test_path, root_path, ec);
}
