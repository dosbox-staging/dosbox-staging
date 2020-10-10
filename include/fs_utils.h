/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2019-2020  The dosbox-staging team
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

#include <string>

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

#endif
