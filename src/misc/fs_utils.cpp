// SPDX-FileCopyrightText:  2022-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "utils/fs_utils.h"

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fstream>

#include "dos/dos.h"
#include "dos/dos_system.h"
#include "misc/std_filesystem.h"
#include "utils/checks.h"

CHECK_NARROWING();

bool check_fseek(const char* module_name, const char* file_description,
                 const char* filename, FILE*& stream, const long long offset,
                 const int whence)
{
	assert(stream);
	if (cross_fseeko(stream, offset, whence) == 0) {
		return true;
	}

	assert(module_name);
	assert(file_description);
	assert(filename);
	LOG_ERR("%s: Failed seeking to byte %lld in %s file '%s': %s",
	        module_name,
	        offset,
	        file_description,
	        filename,
	        strerror(errno));

	fclose(stream);
	stream = nullptr;
	return false;
}

bool is_dir(const std_fs::path& path)
{
	std::error_code ec = {};
	return std_fs::is_directory(path, ec);
}

bool is_hidden_by_host(const std_fs::path& pathname)
{
	assert(!pathname.empty());
	const auto filename = pathname.filename().string();

	// Filenames that don't start with dot or are the two directory entries
	// are not hidden by the host
	if (filename.find('.') != 0 || filename == "." || filename == "..") {
		return false;
	}

	assert(filename[0] == '.');
	const auto extension = pathname.extension().string();

	// Consider the file hidden by the host so long as the filename starts
	// with a dot *and* has an extension longer that DOS's three characters
	// or uses any lower-case characters.

	return extension.length() > DOS_EXTLENGTH ||
	       std::any_of(filename.begin(), filename.end(), islower);
}

// return the lines from the given text file or an empty optional
std::optional<std::vector<std::string>> get_lines(const std_fs::path &text_file)
{
	std::ifstream input_file(text_file, std::ios::binary);
	if (!input_file.is_open())
		return {};

	std::vector<std::string> lines = {};

	std::string line = {};
	while (getline(input_file, line)) {
		lines.emplace_back(std::move(line));
		line = {}; // reset after moving
	}

	input_file.close();
	return lines;
}

std_fs::path simplify_path(const std_fs::path &original_path) noexcept
{
	auto ec = std::error_code();

	auto simplest_path   = original_path;
	auto simplest_length = simplest_path.string().length();

	auto keep_simplest = [&](const std_fs::path &candidate_path) {
		// If the candidate is faulty, reset the code and try again
		if (ec) {
			ec.clear();
			return;
		}
		if (candidate_path.empty())
			return;

		const auto candidate_length = candidate_path.string().length();
		if (candidate_length < simplest_length) {
			simplest_path   = candidate_path;
			simplest_length = candidate_length;
		}
		assert(!ec); // we expect to depart this function with a clean code
	};

	keep_simplest(std_fs::absolute(original_path, ec));
	keep_simplest(std_fs::canonical(original_path, ec));
	keep_simplest(std_fs::proximate(original_path, ec));

	return simplest_path;
}

// Convert a filesystem time to a raw time_t value

// the offset between the filesystem time datum and the system datum is fixed,
// so we compute that difference once and then re-use it for the runtime's
// lifetime. This reuse is important because querying the hosts's clock is much
// slower than subtracting a single integer value.
std::time_t to_time_t(const std_fs::file_time_type &fs_time)
{
	using namespace std::chrono;
	constexpr auto fs_datum = std_fs::file_time_type{};

	// Subtracting file_time_type's default() and now() values gives us a
	// unitless scalar. which is then added-to by the system time - which
	// transforms the value into a system-time type.
	static auto fs_to_sys_delta = fs_datum -
	                              std_fs::file_time_type::clock::now() +
	                              system_clock::now();

	// Again, we subtract file_time_type's default() from the actual time
	// giving us another unitless scalar, and then add the previous system
	// time delta to transform it to a system-time type.
	const auto sys_time = time_point_cast<system_clock::duration>(
	        fs_time - fs_datum + fs_to_sys_delta);

	return system_clock::to_time_t(sys_time);
}

// ***************************************************************************
// Local drive file/directory attribute handling
// ***************************************************************************

bool create_dir_if_not_exist(const std_fs::path& path)
{
	std::error_code ec = {};
	if (std_fs::is_directory(path, ec)) {
		return true;
	}
	ec.clear();
	return std_fs::create_directory(path, ec);
}

int64_t get_native_file_position(const NativeFileHandle handle)
{
	return seek_native_file(handle, 0, NativeSeek::Current);
}
