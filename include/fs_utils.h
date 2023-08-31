/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2023  The DOSBox Staging Team
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

#ifndef DOSBOX_FS_UTILS_H
#define DOSBOX_FS_UTILS_H

#include "config.h"

#include <cinttypes>
#include <ctime>
#include <deque>
#include <optional>
#include <string>
#include <vector>

#include "std_filesystem.h"

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

FILE* local_drive_create_file(const std_fs::path& path,
                              const FatAttributeFlags attributes);
uint16_t local_drive_create_dir(const std_fs::path& path);
uint16_t local_drive_get_attributes(const std_fs::path& path,
                                    FatAttributeFlags& attributes);
uint16_t local_drive_set_attributes(const std_fs::path& path,
                                    const FatAttributeFlags attributes);

#endif
