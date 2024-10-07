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
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <random>
#include <stdexcept>
#include <string>

#include "cross.h"
#include "debug.h"
#include "fs_utils.h"
#include "string_utils.h"
#include "video.h"

#include "whereami.h"

char int_to_char(int val)
{
	// To handle inbound values cast from unsigned chars, permit a slightly
	// wider range to avoid triggering the assert when processing
	// international ASCII values between 128 and 255.
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

char get_drive_letter_from_path(const char* path)
{
	if (strlen(path) < 2) {
		return 0;
	}
	if (path[1] != ':') {
		return 0;
	}
	const char drive_letter = toupper(path[0]);
	if (drive_letter >= 'A' && drive_letter <= 'Z') {
		return drive_letter;
	}
	return 0;
}

std::string get_basename(const std::string& filename)
{
	// Guard against corner cases: '', '/', '\', 'a'
	if (filename.length() <= 1) {
		return filename;
	}

	// Find the last slash, but if not is set to zero
	size_t slash_pos = filename.find_last_of("/\\");

	// If the slash is the last character
	if (slash_pos == filename.length() - 1) {
		slash_pos = 0;
	}

	// Otherwise if the slash is found mid-string
	else if (slash_pos > 0) {
		slash_pos++;
	}
	return filename.substr(slash_pos);
}

bool is_executable_filename(const std::string& filename) noexcept
{
	const size_t n = filename.length();
	if (n < 4) {
		return false;
	}
	if (filename[n - 4] != '.') {
		return false;
	}
	std::string sfx = filename.substr(n - 3);
	lowcase(sfx);
	return (sfx == "exe" || sfx == "bat" || sfx == "com");
}

// Scans the provided command-line string for a '/'flag, removes it (if found),
// and then returns a bool if it was indeed found and removed.
bool ScanCMDBool(char* cmd, const char* flag)
{
	if (cmd == nullptr) {
		return false;
	}
	char* scan            = cmd;
	const size_t flag_len = strlen(flag);
	while ((scan = strchr(scan, '/'))) {
		// Found a slash indicating the possible start of a flag.
		// Now see if it's the flag we're looking for:
		scan++;
		if (strncasecmp(scan, flag, flag_len) == 0 &&
		    (scan[flag_len] == ' ' || scan[flag_len] == '\t' ||
		     scan[flag_len] == '/' || scan[flag_len] == '\0')) {

			// Found a match for the flag, now remove it
			memmove(scan - 1, scan + flag_len, strlen(scan + flag_len) + 1);
			trim(scan - 1);
			return true;
		}
	}
	return false;
}

/* This scans the command line for a remaining switch and reports it else
 * returns 0*/
char* ScanCMDRemain(char* cmd)
{
	char *scan, *found;
	;
	if ((scan = found = strchr(cmd, '/'))) {
		while (*scan && !isspace(*reinterpret_cast<unsigned char*>(scan))) {
			scan++;
		}
		*scan = 0;
		return found;
	} else {
		return nullptr;
	}
}

static char e_exit_buf[1024]; // greater scope as else it doesn't always gets
                              // thrown right
void E_Exit(const char* format, ...)
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

// Overloaded function to handle different return types of POSIX and GNU
// strerror_r variants
[[maybe_unused]] static const char* strerror_result(int retval, const char* err_str)
{
	return retval == 0 ? err_str : nullptr;
}

[[maybe_unused]] static const char* strerror_result(const char* err_str,
                                                    [[maybe_unused]] const char* buf)
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

void set_thread_name([[maybe_unused]] std::thread& thread,
                     [[maybe_unused]] const char* name)
{
#if defined(HAVE_PTHREAD_SETNAME_NP) && defined(_GNU_SOURCE)
	assert(strlen(name) < 16);
	pthread_t handle = thread.native_handle();
	pthread_setname_np(handle, name);
#endif
}

void FILE_closer::operator()(FILE* f) noexcept
{
	if (f) {
		fclose(f);
	}
}

FILE* open_file(const char* filename, const char* mode)
{
#if defined(_WIN32) && defined(__STDC_WANT_SECURE_LIB__)
	FILE* f;
	if (0 == fopen_s(&f, filename, mode)) {
		return f;
	}
#else
	return fopen(filename, mode);
#endif
	return nullptr;
}

FILE_unique_ptr make_fopen(const char* fname, const char* mode)
{
	FILE* f = open_file(fname, mode);
	return f ? FILE_unique_ptr(f) : nullptr;
}

// File size in bytes, returns -1 on error
// The file position will be restored
int64_t stdio_size_bytes(FILE* f)
{
	const auto orig_pos = cross_ftello(f);
	if (orig_pos >= 0 && !cross_fseeko(f, 0L, SEEK_END)) {
		const auto end_pos = cross_ftello(f);
		if (end_pos >= 0 && !cross_fseeko(f, orig_pos, SEEK_SET)) {
			return end_pos;
		}
	}
	return -1;
}

static int64_t stdio_size_with_divisor(FILE* f, const int divisor)
{
	auto s = stdio_size_bytes(f);
	if (s >= 0) {
		return static_cast<int64_t>(s / divisor);
	}
	return -1;
}

// File size in KB, returns -1 on error
// The file position will be restored
int64_t stdio_size_kb(FILE* f)
{
	return stdio_size_with_divisor(f, 1024L);
}

// Number of sectors in file, returns -1 on error
// The file position will be restored
int64_t stdio_num_sectors(FILE* f)
{
	return stdio_size_with_divisor(f, 512L);
}

const std_fs::path& GetExecutablePath()
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

static const std::deque<std_fs::path>& GetResourceParentPaths()
{
	static std::deque<std_fs::path> paths = {};
	if (!paths.empty()) {
		return paths;
	}

	auto add_if_exists = [&](const std_fs::path& p) {
		std::error_code ec = {};
		if (std_fs::is_directory(p, ec)) {
			paths.emplace_back(p);
		}
	};

	// First prioritize is local
	// These resources are provided directly off the working path
	add_if_exists(std_fs::path("."));
	constexpr auto resource_dir_name = "resources";
	add_if_exists(std_fs::path(resource_dir_name));

	// Second priority are resources packaged with the executable
#if defined(MACOSX)
	constexpr auto macos_resource_dir_name = "Resources";
	add_if_exists(GetExecutablePath() / ".." / macos_resource_dir_name);
	add_if_exists(GetExecutablePath() / ".." / macos_resource_dir_name);
#else
	add_if_exists(GetExecutablePath() / resource_dir_name);
	add_if_exists(GetExecutablePath() / ".." / resource_dir_name);
#endif
	// macOS, POSIX, and even MinGW/MSYS2/Cygwin:

	// Third priority is a potentially customized --datadir specified at
	// compile time.
	add_if_exists(std_fs::path(CUSTOM_DATADIR) / DOSBOX_PROJECT_NAME);

	// Fourth priority is the user and system XDG data specification
#if !defined(WIN32) && !defined(MACOSX)
	add_if_exists(get_xdg_data_home() / DOSBOX_PROJECT_NAME);

	for (const auto& data_dir : get_xdg_data_dirs()) {
		add_if_exists(data_dir / DOSBOX_PROJECT_NAME);
	}
#endif

	// Fifth priority is a best-effort fallback for --prefix installations
	// into paths not pointed to by the system's XDG_DATA_ variables. Note
	// that This lookup is deliberately relative to the executable to permit
	// portability of the install tree (do not replace this with --prefix,
	// which would destroy this portable aspect).
	//
	add_if_exists(GetExecutablePath() / "../share" / DOSBOX_PROJECT_NAME);

	// Last priority is the user's configuration directory
	add_if_exists(GetConfigDir());

	return paths;
}

// Select either an integer or real-based uniform distribution
template <typename T>
using uniform_distributor_t =
        typename std::conditional<std::is_integral<T>::value, std::uniform_int_distribution<T>,
                                  std::uniform_real_distribution<T>>::type;

template <typename T>
std::function<T()> CreateRandomizer(const T min_value, const T max_value)
{
	static std::random_device rd;        // one-time call to the host OS
	static std::mt19937 generator(rd()); // seed the mersenne_twister once

	return [=]() {
		auto distribute = uniform_distributor_t<T>(min_value, max_value);
		return distribute(generator);
	};
}
// Explicit template instantiations
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
template std::function<int16_t()> CreateRandomizer<int16_t>(const int16_t,
                                                            const int16_t);
template std::function<float()> CreateRandomizer<float>(const float, const float);

// return the first existing resource
std_fs::path GetResourcePath(const std_fs::path& name)
{
	std::error_code ec;

	// handle an absolute path
	if (std_fs::exists(name, ec)) {
		return name;
	}

	// try the resource paths
	for (const auto& parent : GetResourceParentPaths()) {
		const auto resource = parent / name;
		if (std_fs::exists(resource, ec)) {
			return resource;
		}
	}
	return std_fs::path();
}

std_fs::path GetResourcePath(const std_fs::path& subdir, const std_fs::path& name)
{
	return GetResourcePath(subdir / name);
}

static std::vector<std_fs::path> get_directory_entries(const std_fs::path& dir,
                                                       const std::string_view files_ext,
                                                       const bool only_regular_files)
{
	using namespace std_fs;
	std::vector<std_fs::path> files = {};

	// Check if the directory exists
	std::error_code ec = {};
	if (!std_fs::is_directory(dir, ec)) {
		return files;
	}

	// Ensure the extension is valid
	assert(files_ext.length() && files_ext[0] == '.');

	// Keep recursing past permission issues and follow symlinks
	constexpr auto idir_opts = directory_options::skip_permission_denied |
	                           directory_options::follow_directory_symlink;

	for (const auto& entry : recursive_directory_iterator(dir, idir_opts, ec)) {
		if (ec) {
			// problem iterating, so skip the directory
			break;
		}

		if (only_regular_files && !entry.is_regular_file(ec)) {
			// move onto the next entry
			continue;
		}

		if (entry.path().extension() == files_ext) {
			files.emplace_back(entry.path().lexically_relative(dir));
		}
	}

	std::sort(files.begin(), files.end());
	return files;
}

std::map<std_fs::path, std::vector<std_fs::path>> GetFilesInResource(
        const std_fs::path& res_name, const std::string_view files_ext,
        const bool only_regular_files = true)
{
	std::map<std_fs::path, std::vector<std_fs::path>> paths_and_files;

	for (const auto& parent : GetResourceParentPaths()) {
		auto res_path  = parent / res_name;
		auto res_files = get_directory_entries(res_path,
		                                       files_ext,
		                                       only_regular_files);

		paths_and_files.emplace(std::move(res_path), std::move(res_files));
	}

	return paths_and_files;
}

// Get resource lines from a text file
std::vector<std::string> GetResourceLines(const std_fs::path& name,
                                          const ResourceImportance importance)
{
	const auto resource_path = GetResourcePath(name);

	if (auto maybe_lines = get_lines(resource_path); maybe_lines) {
		return std::move(*maybe_lines);
	}

	// The resource didn't exist but it's optional
	if (importance == ResourceImportance::Optional) {
		return {};
	}

	// The resource didn't exist and it was mandatory, so verbosely quit
	assert(importance == ResourceImportance::Mandatory);

	LOG_ERR("RESOURCE: Could not open mandatory resource '%s', tried:",
	        name.string().c_str());

	for (const auto& path : GetResourceParentPaths()) {
		LOG_WARNING("RESOURCE:  - '%s'", (path / name).string().c_str());
	}

	E_Exit("RESOURCE: Mandatory resource failure (see detailed message)");
}

// Get resource lines from a text file
std::vector<std::string> GetResourceLines(const std_fs::path& subdir,
                                          const std_fs::path& name,
                                          const ResourceImportance importance)
{
	return GetResourceLines(subdir / name, importance);
}

// Load a resource blob (from a binary file)
std::vector<uint8_t> LoadResourceBlob(const std_fs::path& name,
                                      const ResourceImportance importance)
{
	const auto resource_path = GetResourcePath(name);

	std::ifstream file(resource_path, std::ios::binary);

	if (!file.is_open()) {
		if (importance == ResourceImportance::Optional) {
			return {};
		}
		assert(importance == ResourceImportance::Mandatory);

		LOG_ERR("RESOURCE: Could not open mandatory resource '%s', tried:",
		        name.string().c_str());

		for (const auto& path : GetResourceParentPaths()) {
			LOG_WARNING("RESOURCE:  - '%s'",
			            (path / name).string().c_str());
		}

		E_Exit("RESOURCE: Mandatory resource failure (see detailed message)");
	}

	// non-const to allow movement out of the function
	std::vector<uint8_t> buffer(std::istreambuf_iterator<char>{file}, {});
	// LOG_DEBUG("RESOURCE: Loaded resource '%s' [%d bytes]",
	//           resource_path.string().c_str(),
	//           check_cast<int>(buffer.size()));
	return buffer;
}

// Load a resource blob (from a binary file)
std::vector<uint8_t> LoadResourceBlob(const std_fs::path& subdir,
                                      const std_fs::path& name,
                                      const ResourceImportance importance)
{
	return LoadResourceBlob(subdir / name, importance);
}

bool path_exists(const std_fs::path& path)
{
	// avoid exceptions
	std::error_code ec;
	return std_fs::exists(path, ec);
}

bool is_writable(const std_fs::path& p)
{
	using namespace std_fs;

	// avoid exceptions
	std::error_code ec = {};
	//
	const auto p_status = status(p, ec);
	if (ec) {
		return false;
	}

	const auto perms = p_status.permissions();

	return ((perms & perms::owner_write) != perms::none ||
	        (perms & perms::group_write) != perms::none ||
	        (perms & perms::others_write) != perms::none);
}

bool is_readable(const std_fs::path& p)
{
	using namespace std_fs;

	// avoid exceptions
	std::error_code ec = {};

	const auto p_status = status(p, ec);
	if (ec) {
		return false;
	}

	const auto perms = p_status.permissions();

	return ((perms & perms::owner_read) != perms::none ||
	        (perms & perms::group_read) != perms::none ||
	        (perms & perms::others_read) != perms::none);
}

bool is_readonly(const std_fs::path& p)
{
	return is_readable(p) && !is_writable(p);
}

bool make_writable(const std_fs::path& p)
{
	using namespace std_fs;

	// Check
	if (is_writable(p)) {
		return true;
	}

	// Apply
	std::error_code ec;
	permissions(p, perms::owner_write, perm_options::add, ec);

	// Result and verification
	if (ec) {
		LOG_WARNING("FILESYSTEM: Failed to add write permissions for '%s': %s",
		            p.string().c_str(),
		            ec.message().c_str());
	} else {
		assert(is_writable(p));
	}

	return (!ec);
}

bool make_readonly(const std_fs::path& p)
{
	using namespace std_fs;

	// Check
	if (is_readonly(p)) {
		return true;
	}

	// Apply
	constexpr auto write_perms = (perms::owner_write | perms::group_write |
	                              perms::others_write);
	std::error_code ec;
	permissions(p, write_perms, perm_options::remove, ec);

	// Result and verification
	if (ec) {
		LOG_WARNING("FILESYSTEM: Failed to remove write permissions for '%s': %s",
		            p.string().c_str(),
		            ec.message().c_str());
	} else {
		assert(is_readonly(p));
	}

	return (!ec);
}

bool is_date_valid(const uint32_t year, const uint32_t month, const uint32_t day)
{
	if (year < 1980 || month > 12 || month == 0 || day == 0) {
		return false;
	}
	// February has 29 days on leap-years and 28 days otherwise.
	const bool is_leap_year = !(year % 4) && (!(year % 400) || (year % 100));

	if (month == 2 &&
	    day > (uint32_t)(is_leap_year ? 29 : DOS_DATE_months[month])) {
		return false;
	}
	if (month != 2 && day > DOS_DATE_months[month]) {
		return false;
	}
	return true;
}

bool is_time_valid(const uint32_t hour, const uint32_t minute, const uint32_t second)
{
	if (hour > 23 || minute > 59 || second > 59) {
		return false;
	}
	return true;
}

template <typename T>
std::pair<std::unique_ptr<T[]>, T*> make_unique_aligned_array(
        const size_t byte_alignment, const size_t req_elems, const T& initial_value)
{
	// Are the inputs valid?
	assert(byte_alignment > 0);
	assert(byte_alignment % sizeof(T) == 0); // multiple of the type-size
	assert(req_elems > 0);

	// Allocate the buffer with enough "space" to accomodate the alignment:
	const auto space_elems = req_elems + byte_alignment / sizeof(T);
	auto buffer = std::make_unique<T[]>(space_elems); // moved on return

	// Convert the number of elements into bytes, to be used by align
	const auto req_bytes = req_elems * sizeof(T);
	auto space_bytes     = space_elems * sizeof(T); // adjusted by align

	// Align the pointer within our buffer
	auto ptr = reinterpret_cast<void*>(buffer.get());
	std::align(byte_alignment, req_bytes, ptr, space_bytes);

	// Verify that the adjust space is sufficient and that the ptr is aligned
	assert(space_bytes >= req_bytes);
	assert(reinterpret_cast<uintptr_t>(ptr) % byte_alignment == 0);

	// Initialize the elements
	const auto obj_ptr = reinterpret_cast<T*>(ptr);
	std::fill_n(obj_ptr, req_elems, initial_value);

	return {std::move(buffer), obj_ptr};
}

// Explicit template instantiations
template std::pair<std::unique_ptr<uint8_t[]>, uint8_t*>
make_unique_aligned_array<uint8_t>(const size_t, const size_t, const uint8_t&);
