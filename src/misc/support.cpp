/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2022  The DOSBox Staging Team
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
#include <deque>
#include <functional>
#include <fstream>
#include <iterator>
#include <map>
#include <random>
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

// Scans the provided command-line string for a '/'flag, removes it (if found),
// and then returns a bool if it was indeed found and removed.
bool ScanCMDBool(char *cmd, const char * flag)
{
	if (cmd == nullptr)
		return false;
	char *scan = cmd;
	const size_t flag_len = strlen(flag);
	while ((scan = strchr(scan,'/'))) {
		// Found a slash indicating the possible start of a flag.
		// Now see if it's the flag we're looking for:
		scan++;
		if (strncasecmp(scan, flag, flag_len) == 0 &&
		    (scan[flag_len] == ' ' || scan[flag_len] == '\t' ||
		     scan[flag_len] == '/' || scan[flag_len] == '\0')) {

			// Found a match for the flag, now remove it
			memmove(scan - 1, scan + flag_len, strlen(scan + flag_len) + 1);
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

static const std::deque<std_fs::path> &GetResourceParentPaths()
{
	static std::deque<std_fs::path> paths = {};
	if (paths.size())
		return paths;

	// Prioritize portable configuration: allow the entire resource tree or
	// just a single resource to be included in the current working directory.
	paths.emplace_back(std_fs::path("."));
	paths.emplace_back(std_fs::path("resources"));
#if defined(MACOSX)
	paths.emplace_back(GetExecutablePath() / "../Resources");
	paths.emplace_back(GetExecutablePath() / "../../Resources");
#else
	paths.emplace_back(GetExecutablePath() / "resources");
	paths.emplace_back(GetExecutablePath() / "../resources");
#endif
	// macOS, POSIX, and even MinGW/MSYS2/Cygwin:
	paths.emplace_back(std_fs::path("/usr/local/share/dosbox-staging"));
	paths.emplace_back(std_fs::path("/usr/share/dosbox-staging"));
	paths.emplace_back(std_fs::path(CROSS_GetPlatformConfigDir()));
	return paths;
}

template <typename T>
std::function<T()> CreateRandomizer(const T min_value, const T max_value)
{
	static std::random_device rd;        // one-time call to the host OS
	static std::mt19937 generator(rd()); // seed the mersenne_twister once

	// Hand back as many generators as needed without further costs
	// incurred setting up the random number generator.
	return [=]() {
		std::uniform_int_distribution<T> distrib(min_value, max_value);
		return distrib(generator);
	};
}
// Explicit template instantiations
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
template std::function<char()> CreateRandomizer<char>(const char, const char);
template std::function<uint8_t()> CreateRandomizer<uint8_t>(const uint8_t,
                                                            const uint8_t);

std_fs::path GetResourcePath(const std_fs::path &name)
{
	// return the first existing resource
	std::error_code ec;
	for (const auto &parent : GetResourceParentPaths()) {
		const auto resource = parent / name;
		if (std_fs::exists(resource, ec)) {
			return resource;
		}
	}
	return std_fs::path();
}

std_fs::path GetResourcePath(const std_fs::path &subdir, const std_fs::path &name)
{
	return GetResourcePath(subdir / name);
}

static std::vector<std_fs::path> GetFilesInPath(const std_fs::path &dir,
                                                const std::string_view files_ext)
{
	std::vector<std_fs::path> files;

	// Check if the directory exists
	if (!std_fs::is_directory(dir))
		return files;

	// Ensure the extension is valid
	assert(files_ext.length() && files_ext[0] == '.');

	for (const auto &entry : std_fs::recursive_directory_iterator(dir))
		if (entry.is_regular_file() && entry.path().extension() == files_ext)
			files.emplace_back(entry.path().lexically_relative(dir));

	std::sort(files.begin(), files.end());
	return files;
}

std::map<std_fs::path, std::vector<std_fs::path>> GetFilesInResource(
        const std_fs::path &res_name, const std::string_view files_ext)
{
	std::map<std_fs::path, std::vector<std_fs::path>> paths_and_files;
	for (const auto &parent : GetResourceParentPaths()) {
		auto res_path = parent / res_name;
		auto res_files = GetFilesInPath(res_path, files_ext);
		paths_and_files.emplace(std::move(res_path), std::move(res_files));
	}
	return paths_and_files;
}

std::vector<uint8_t> LoadResource(const std_fs::path &name,
                                  const ResourceImportance importance)
{
	const auto resource_path = GetResourcePath(name);
	std::ifstream file(resource_path, std::ios::binary);

	if (!file.is_open()) {
		if (importance == ResourceImportance::Optional) {
			return {};
		}
		assert(importance == ResourceImportance::Mandatory);
		LOG_ERR("RESOURCE: Could not open mandatory resource '%s', tried:", name.string().c_str());
		for (const auto &path : GetResourceParentPaths()) {
			LOG_WARNING("RESOURCE:  - '%s'", (path / name).string().c_str());
		}
		E_Exit("RESOURCE: Mandatory resource failure (see detailed message)");
	}

	const std::vector<uint8_t> buffer(std::istreambuf_iterator<char>{file}, {});
	// DEBUG_LOG_MSG("RESOURCE: Loaded resource '%s' [%d bytes]",
	//               resource_path.string().c_str(),
	//               check_cast<int>(buffer.size()));
	return buffer;
}

std::vector<uint8_t> LoadResource(const std_fs::path &subdir,
                                  const std_fs::path &name,
                                  const ResourceImportance importance)
{
	return LoadResource(subdir / name, importance);
}

bool path_exists(const std_fs::path &path)
{
	std::error_code ec; // avoid exceptions
	return std_fs::exists(path, ec);
}

bool is_writable(const std_fs::path &p)
{
	using namespace std_fs;
	std::error_code ec; // avoid exceptions
	const auto perms = status(p, ec).permissions();
	return ((perms & perms::owner_write) != perms::none ||
	        (perms & perms::group_write) != perms::none ||
	        (perms & perms::others_write) != perms::none);
}

bool is_readable(const std_fs::path &p)
{
	using namespace std_fs;
	std::error_code ec; // avoid exceptions
	const auto perms = status(p, ec).permissions();
	return ((perms & perms::owner_read) != perms::none ||
	        (perms & perms::group_read) != perms::none ||
	        (perms & perms::others_read) != perms::none);
}

bool is_readonly(const std_fs::path &p)
{
	return is_readable(p) && !is_writable(p);
}

bool make_writable(const std_fs::path &p)
{
	// Check
	if (is_writable(p))
		return true;

	// Apply
	std::error_code ec;
	using namespace std_fs;
	permissions(p, perms::owner_write, perm_options::add, ec);

	// Result and verification
	if (ec)
		LOG_WARNING("FILESYSTEM: Failed to add write permissions for '%s': %s",
		            p.string().c_str(), ec.message().c_str());
	else
		assert(is_writable(p));

	return (!ec);
}

bool make_readonly(const std_fs::path &p)
{
	// Check
	if (is_readonly(p))
		return true;

	// Apply
	using namespace std_fs;
	constexpr auto write_perms = (perms::owner_write |
	                              perms::group_write |
	                              perms::others_write);
	std::error_code ec;
	permissions(p, write_perms, perm_options::remove, ec);

	// Result and verification
	if (ec)
		LOG_WARNING("FILESYSTEM: Failed to remove write permissions for '%s': %s",
		            p.string().c_str(), ec.message().c_str());
	else
		assert(is_readonly(p));

	return (!ec);
}

bool is_date_valid(const uint32_t year, const uint32_t month, const uint32_t day)
{
	if (year < 1980 || month > 12 || month == 0 || day == 0)
		return false;
	// February has 29 days on leap-years and 28 days otherwise.
	const bool is_leap_year = !(year % 4) && (!(year % 400) || (year % 100));
	if (month == 2 && day > (uint32_t)(is_leap_year ? 29 : DOS_DATE_months[month]))
		return false;
	if (month != 2 && day > DOS_DATE_months[month])
		return false;
	return true;
}

bool is_time_valid(const uint32_t hour, const uint32_t minute, const uint32_t second)
{
	if (hour > 23 || minute > 59 || second > 59)
		return false;
	return true;
}

double decibel_to_gain(const double decibel)
{
	return pow(10.0, decibel / 20.0);
}

double gain_to_decibel(const double gain)
{
	return 20.0 * log(gain) / log(10.0);
}

float lerp(const float a, const float b, const float t)
{
	return a * (1 - t) + b * t;
}

double lerp(const double a, const double b, const double t)
{
	return a * (1 - t) + b * t;
}

float invlerp(const float a, const float b, const float v)
{
	return (v - a) / (b - a);
}

double invlerp(const double a, const double b, const double v)
{
	return (v - a) / (b - a);
}

float remap(const float in_min, const float in_max, const float out_min,
            const float out_max, const float v)
{
	float t = invlerp(in_min, in_max, v);
	return lerp(out_min, out_max, t);
}

double remap(const double in_min, const double in_max, const double out_min,
             const double out_max, const double v)
{
	double t = invlerp(in_min, in_max, v);
	return lerp(out_min, out_max, t);
}

