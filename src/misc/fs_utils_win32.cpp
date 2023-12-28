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

#include "fs_utils.h"

#if defined(WIN32)

#include <direct.h>
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>

#include "compiler.h"
#include "dos_inc.h"
#include "dos_system.h"

bool path_exists(const char *path) noexcept
{
	return (_access(path, 0) == 0);
}

std::string to_native_path(const std::string &path) noexcept
{
	if (path_exists(path))
		return path;
	return "";
}

int create_dir(const std_fs::path& path, [[maybe_unused]] uint32_t mode,
               uint32_t flags) noexcept
{
	const auto path_str = path.string();
	const int err       = mkdir(path_str.c_str());
	if ((errno == EEXIST) && (flags & OK_IF_EXISTS)) {
		struct _stat pstat;
		if ((_stat(path_str.c_str(), &pstat) == 0) &&
		    ((pstat.st_mode & _S_IFMT) == _S_IFDIR))
			return 0;
	}
	return err;
}

// ***************************************************************************
// Local drive file/directory attribute handling
// ***************************************************************************

constexpr uint8_t WindowsAttributesMask = 0x3f;

FILE* local_drive_create_file(const std_fs::path& path,
                              const FatAttributeFlags attributes)
{
	// NOTE: If changing this implementation, test if 'Crystal Caves' game
	// can save a game to the same slot it was loaded from. Previous
	// implementation, utilizing 'CreateFileW', did not work in such case,
	// game was producing FILE ERROR - see the issue:
	// https://github.com/dosbox-staging/dosbox-staging/issues/3262

	FILE* file_pointer = fopen(path.string().c_str(), "wb+");
	if (file_pointer) {
		if (!SetFileAttributesW(path.c_str(), attributes._data)) {
			fclose(file_pointer);
			return nullptr;
		}
	}

	return file_pointer;
}

uint16_t local_drive_get_attributes(const std_fs::path& path,
                                    FatAttributeFlags& attributes)
{
	const auto win32_attributes = GetFileAttributesW(path.c_str());
	if (win32_attributes == INVALID_FILE_ATTRIBUTES) {
		attributes = 0;
		return static_cast<uint16_t>(GetLastError());
	}

	attributes = win32_attributes & WindowsAttributesMask;
	return DOSERR_NONE;
}

uint16_t local_drive_set_attributes(const std_fs::path& path,
                                    const FatAttributeFlags attributes)
{
	if (!SetFileAttributesW(path.c_str(), attributes._data)) {
		return static_cast<uint16_t>(GetLastError());
	}

	return DOSERR_NONE;
}

#endif
