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

NativeFileHandle open_native_file(const std_fs::path& path, const bool write_access)
{
	DWORD access = GENERIC_READ;
	if (write_access) {
		access |= GENERIC_WRITE;
	}

	return CreateFileW(path.c_str(),
	                   access,
	                   FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
	                   nullptr,
	                   OPEN_EXISTING,
	                   FILE_ATTRIBUTE_NORMAL,
	                   nullptr);
}

NativeFileHandle create_native_file(const std_fs::path& path,
                                    const std::optional<FatAttributeFlags> attributes)
{
	const auto win32_attributes = (attributes && attributes->_data != 0)
	                                    ? static_cast<DWORD>(attributes->_data)
	                                    : FILE_ATTRIBUTE_NORMAL;

	return CreateFileW(path.c_str(),
	                   GENERIC_READ | GENERIC_WRITE,
	                   FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
	                   nullptr,
	                   CREATE_ALWAYS,
	                   win32_attributes,
	                   nullptr);
}

// ReadFile() and WriteFile() should work with a single call
// However, they take in a 32-bit unsigned DWORD and we take in an int64_t
// "future proof" this with a loop so we can read large files if we ever need to
NativeIoResult read_native_file(const NativeFileHandle handle, uint8_t* buffer,
                                const int64_t num_bytes_requested)
{
	constexpr auto max_dword = std::numeric_limits<DWORD>::max();

	NativeIoResult ret = {};
	ret.num_bytes      = 0;
	ret.error          = false;
	while (ret.num_bytes < num_bytes_requested) {
		int64_t clamped_bytes_requested = num_bytes_requested - ret.num_bytes;
		if (clamped_bytes_requested > max_dword) {
			clamped_bytes_requested = max_dword;
		}
		DWORD num_bytes_read = 0;
		const auto success   = ReadFile(handle,
                                              buffer + ret.num_bytes,
                                              clamped_bytes_requested,
                                              &num_bytes_read,
                                              nullptr);
		ret.num_bytes += num_bytes_read;
		ret.error = !success;
		if (ret.error || num_bytes_read == 0) {
			break;
		}
	}
	return ret;
}

NativeIoResult write_native_file(const NativeFileHandle handle, const uint8_t* buffer,
                                 const int64_t num_bytes_requested)
{
	constexpr auto max_dword = std::numeric_limits<DWORD>::max();

	NativeIoResult ret = {};
	ret.num_bytes      = 0;
	ret.error          = false;
	while (ret.num_bytes < num_bytes_requested) {
		int64_t clamped_bytes_requested = num_bytes_requested - ret.num_bytes;
		if (clamped_bytes_requested > max_dword) {
			clamped_bytes_requested = max_dword;
		}
		DWORD num_bytes_written = 0;
		const auto success      = WriteFile(handle,
                                               buffer + ret.num_bytes,
                                               clamped_bytes_requested,
                                               &num_bytes_written,
                                               nullptr);
		ret.num_bytes += num_bytes_written;
		ret.error = !success;
		if (ret.error || num_bytes_written == 0) {
			break;
		}
	}
	return ret;
}

int64_t seek_native_file(const NativeFileHandle handle, const int64_t offset,
                         const NativeSeek type)
{
	DWORD win32_seek_type = FILE_BEGIN;
	switch (type) {
	case NativeSeek::Set: win32_seek_type = FILE_BEGIN; break;
	case NativeSeek::Current: win32_seek_type = FILE_CURRENT; break;
	case NativeSeek::End: win32_seek_type = FILE_END; break;
	default: assertm(false, "Invalid seek type"); return NativeSeekFailed;
	}

	// Microsoft in their infinite knowledge decided to split the offset
	// into two arguments (a low 32-bit word and a high 32-bit word)
	// They use this LARGE_INTEGER union to handle this on their example code
	// https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-setfilepointer

	LARGE_INTEGER li;
	li.QuadPart = offset;

	// HighPart is both an input and output value
	li.LowPart = SetFilePointer(handle, li.LowPart, &li.HighPart, win32_seek_type);

	// If we have a large offset, INVALID_SET_FILE_POINTER is also valid as
	// the low word So we must also check GetLastError() to know if we failed
	if (li.LowPart == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR) {
		return NativeSeekFailed;
	}

	return li.QuadPart;
}

void close_native_file(const NativeFileHandle handle)
{
	CloseHandle(handle);
}

// Sets the file size to be equal to the current file position
bool truncate_native_file(const NativeFileHandle handle)
{
	return SetEndOfFile(handle);
}

DosDateTime get_dos_file_time(const NativeFileHandle handle)
{
	// Legal defaults if we're unable to populate them
	DosDateTime ret = {};
	ret.time        = 1;
	ret.date        = 1;

	FILETIME write_time = {};
	if (!GetFileTime(handle, nullptr, nullptr, &write_time)) {
		return ret;
	}

	FILETIME local_write_time = {};
	if (!FileTimeToLocalFileTime(&write_time, &local_write_time)) {
		return ret;
	}

	FileTimeToDosDateTime(&local_write_time, &ret.date, &ret.time);
	return ret;
}

void set_dos_file_time(const NativeFileHandle handle, const uint16_t date,
                       const uint16_t time)
{
	FILETIME local_write_time = {};
	if (!DosDateTimeToFileTime(date, time, &local_write_time)) {
		return;
	}

	FILETIME write_time = {};
	if (!LocalFileTimeToFileTime(&local_write_time, &write_time)) {
		return;
	}

	SetFileTime(handle, nullptr, nullptr, &write_time);
}

bool delete_native_file(const std_fs::path& path)
{
	return remove(path.string().c_str()) == 0;
}

#endif
