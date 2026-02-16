// SPDX-FileCopyrightText:  2020-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_FS_UTILS_H
#define DOSBOX_FS_UTILS_H

#include "dosbox_config.h"

#include <cinttypes>
#include <ctime>
#include <deque>
#include <optional>
#include <string>
#include <vector>

#include "misc/std_filesystem.h"

#if defined(WIN32)
	#include <windows.h>

	using NativeFileHandle = HANDLE;
	// Cannot be constexpr due to Win32 macro
	#define InvalidNativeFileHandle INVALID_HANDLE_VALUE

#else // Linux, macOS
	using NativeFileHandle = int;
	constexpr NativeFileHandle InvalidNativeFileHandle = -1;
#endif

constexpr int64_t NativeSeekFailed = -1;

struct NativeIoResult {
	int64_t num_bytes = 0;
	bool error        = false;
};

enum class NativeSeek { Set, Current, End };

struct DosDateTime {
	uint16_t date = 0;
	uint16_t time = 0;
};

// return the lines from the given text file or an empty optional
std::optional<std::vector<std::string>> get_lines(const std_fs::path &text_file);

// Is the candidate a directory or a symlink that points to one?
bool is_directory(const std::string& candidate);

bool is_hidden_by_host(const std::filesystem::path& pathname);

/* Check if the given path corresponds to an existing file or directory.
 */

bool path_exists(const char *path) noexcept;

inline bool path_exists(const std::string &path) noexcept
{
	return path_exists(path.c_str());
}

/* Convert path (possibly in format used by different OS) to a path
 * native for host OS.
 *
 * If path (after conversion) does not correspond to an existing file or
 * directory, then an empty string is returned.
 *
 * On Unix-like systems:
 * - Expand ~ and ~name to paths in appropriate home directory.
 * - Convert Windows-style path separators to Unix-style separators.
 * - If case-insensitive, relative path matches an existing file in the
 *   filesystem, then return case-sensitive path to that file.
 * - If more than one files match, return the first one (alphabetically).
 *
 * On Windows:
 * - If path points to an existing file, then return the path unmodified.
 */

std::string to_native_path(const std::string &path) noexcept;

// Returns a simplified representation of the path, be it relative,
// absolute, or in its original (as-provided) form.
// The shortest valid path is considered the simplest form.
std_fs::path simplify_path(const std_fs::path &path) noexcept;

/* Cross-platform wrapper for following functions:
 *
 * - Unix: mkdir(const char *, mode_t)
 * - Windows: _mkdir(const char *)
 *
 * Normal behaviour of mkdir is to fail when directory exists already,
 * you can override this behaviour by calling:
 *
 *     create_dir(path, 0700, OK_IF_EXISTS)
 */

constexpr uint32_t OK_IF_EXISTS = 0x1;

int create_dir(const std_fs::path& path, uint32_t mode, uint32_t flags = 0x0) noexcept;

// Behaves like fseek, but logs an error stating the module, byte offset, file
// description, filename, and strerror on failure. Returns true on success and
// false on failure. On failure, it closes the file as it's no longer in a good
// state.
//
bool check_fseek(const char* module_name, const char* file_description,
                 const char* filename, FILE*& stream, const long long offset,
                 const int whence);

// Returns a 'check_fseek' function object that behaves like the above. This can
// be used when lots of sequential seeks are needed.
//
inline auto make_check_fseek_func(const std::string& module_name,
                                  const std::string& file_description,
                                  const std_fs::path& filepath)
{
	// Use the lambda copy-operator to keep copies of the arguments inside
	// the lambda, as these arguments would normally go out of scope with
	// respect to the lifetime of the lamda.
	//
	auto check_fseek_lambda = [=](FILE*& stream,
	                              const long long offset,
	                              const int whence) {
		return check_fseek(module_name.c_str(),
		                   file_description.c_str(),
		                   filepath.string().c_str(),
		                   stream,
		                   offset,
		                   whence);
	};
	return check_fseek_lambda;
}

// Convert a filesystem time to a raw time_t value
std::time_t to_time_t(const std_fs::file_time_type &fs_time);

#if !defined(WIN32) && !defined(MACOSX)

/* Get directory for storing user configuration files.
 *
 * User can change this directory by overriding XDG_CONFIG_HOME, otherwise it
 * defaults to "$HOME/.config/".
 *
 * https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html
 */

std_fs::path get_xdg_config_home() noexcept;

/* Get directory for storing user-specific data files.
 *
 * User can change this directory by overriding XDG_DATA_HOME, otherwise it
 * defaults to "$HOME/.local/share/".
 *
 * https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html
 */

std_fs::path get_xdg_data_home() noexcept;

/* Get directories for searching for data files in addition to the XDG_DATA_HOME
 * directory.
 *
 * The directories are ordered according to user preference.
 *
 * User can change this list by overriding XDG_DATA_DIRS, otherwise it defaults
 * to "/usr/local/share/:/usr/share/".
 *
 * https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html
 */

std::deque<std_fs::path> get_xdg_data_dirs() noexcept;

#endif

// ***************************************************************************
// Local drive file/directory attribute handling
// ***************************************************************************

union FatAttributeFlags; // forward declaration

uint16_t local_drive_create_dir(const std_fs::path& path);
bool local_drive_remove_dir(const std_fs::path& path);
uint16_t local_drive_get_attributes(const std_fs::path& path,
                                    FatAttributeFlags& attributes);
uint16_t local_drive_set_attributes(const std_fs::path& path,
                                    const FatAttributeFlags attributes);

// Native file I/O wrappers.
// Currently only used by local drive and overlay drive but suitable for use
// elsewhere.
NativeFileHandle open_native_file(const std_fs::path& path, const bool write_access);
NativeFileHandle create_native_file(const std_fs::path& path,
                                    const std::optional<FatAttributeFlags> attributes);

NativeIoResult read_native_file(const NativeFileHandle handle, uint8_t* buffer,
                                const int64_t num_bytes_requested);
NativeIoResult write_native_file(const NativeFileHandle handle, const uint8_t* buffer,
                                 const int64_t num_bytes_requested);

int64_t seek_native_file(const NativeFileHandle handle, const int64_t offset,
                         const NativeSeek type);

// Returns offset from beginning of file
int64_t get_native_file_position(const NativeFileHandle handle);

void close_native_file(const NativeFileHandle handle);

// Sets the file size to be equal to the current file position
bool truncate_native_file(const NativeFileHandle handle);

DosDateTime get_dos_file_time(const NativeFileHandle handle);
void set_dos_file_time(const NativeFileHandle handle, const uint16_t date, const uint16_t time);

bool delete_native_file(const std_fs::path& path);

// Simple file/directory removal routines, without MS-DOS compatibility hacks
bool delete_file(const std_fs::path& path);
bool remove_dir(const std_fs::path& path);

#endif
