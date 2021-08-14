/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2021  The DOSBox Staging Team
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

#include "fs_utils.h"

#if !defined(WIN32)

#include <cerrno>
#include <cctype>
#include <glob.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "logging.h"
#include "support.h"

bool path_exists(const char *path) noexcept
{
	return (access(path, F_OK) == 0);
}

static std::string translate_to_glob_pattern(const std::string &path) noexcept
{
	std::string glob_pattern;
	glob_pattern.reserve(path.size() * 4);
	char char_pattern[] = "[aA]";
	for (char c : path) {
		if (isalpha(c)) {
			char_pattern[1] = tolower(c);
			char_pattern[2] = toupper(c);
			glob_pattern.append(char_pattern);
			continue;
		}
		switch (c) {
		case '\\': glob_pattern.push_back('/'); continue;
		case '?':
		case '*':
		case '[':
		case ']': glob_pattern.push_back('\\'); FALLTHROUGH;
		default: glob_pattern.push_back(c); continue;
		}
	}
	return glob_pattern;
}

std::string to_native_path(const std::string &path) noexcept
{
	if (path_exists(path))
		return path;

	// Perhaps path is ok, just using Windows-style delimiters:
	const std::string posix_path = replace(path, '\\', '/');
	if (path_exists(posix_path))
		return posix_path;

	// Convert case-insensitive path to case-sensitive path.
	// glob(3) sorts by default, so if more than one path will match
	// the pattern, return the first one (in alphabetic order) that matches.
	const std::string pattern = translate_to_glob_pattern(path);
	glob_t pglob;
	const int err = glob(pattern.c_str(), GLOB_TILDE, nullptr, &pglob);
	if (err == GLOB_NOMATCH) {
		globfree(&pglob);
		return "";
	}
	if (err != 0) {
		DEBUG_LOG_MSG("FS: glob error (%d) while searching for '%s'",
		              err, path.c_str());
		globfree(&pglob);
		return "";
	}
	if (pglob.gl_pathc > 1) {
		DEBUG_LOG_MSG("FS: Searching for path '%s' gives ambiguous results:",
		              path.c_str());
		for (size_t i = 0; i < pglob.gl_pathc; i++)
			DEBUG_LOG_MSG("%s", pglob.gl_pathv[i]);
	}
	const std::string ret = pglob.gl_pathv[0];
	globfree(&pglob);
	return ret;
}

int create_dir(const char *path, uint32_t mode, uint32_t flags) noexcept
{
	static_assert(sizeof(uint32_t) >= sizeof(mode_t), "");
	const int err = mkdir(path, mode);
	if ((errno == EEXIST) && (flags & OK_IF_EXISTS)) {
		struct stat pstat;
		if ((stat(path, &pstat) == 0) && S_ISDIR(pstat.st_mode))
			return 0;
	}
	return err;
}

#endif
