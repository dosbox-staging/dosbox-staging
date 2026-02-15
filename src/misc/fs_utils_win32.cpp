// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "utils/fs_utils.h"

#if defined(WIN32)

// Windows header needed for PathFileExistsW
#include "Shlwapi.h"

#include "dos/dos.h"
#include "dos/dos_system.h"

std::string to_native_path(const std::string &path) noexcept
{
	if (local_drive_path_exists(path.c_str()))
		return path;
	return "";
}

uint16_t local_drive_create_dir(const char* path)
{
	wchar_t utf16_path[MAX_PATH] = {};
	if (!codepage437_to_utf16(path, utf16_path)) {
		return DOSERR_ACCESS_DENIED;
	}
	if (CreateDirectoryW(utf16_path, nullptr)) {
		return DOSERR_NONE;
	}
	return DOSERR_ACCESS_DENIED;
}

// ***************************************************************************
// Local drive file/directory attribute handling
// ***************************************************************************

constexpr uint8_t WindowsAttributesMask = 0x3f;

uint16_t local_drive_get_attributes(const char* path,
                                    FatAttributeFlags& attributes)
{
	wchar_t utf16_path[MAX_PATH] = {};
	if (!codepage437_to_utf16(path, utf16_path)) {
		return DOSERR_ACCESS_DENIED;
	}
	const auto win32_attributes = GetFileAttributesW(utf16_path);
	if (win32_attributes == INVALID_FILE_ATTRIBUTES) {
		attributes = 0;
		return static_cast<uint16_t>(GetLastError());
	}

	attributes = win32_attributes & WindowsAttributesMask;
	return DOSERR_NONE;
}

uint16_t local_drive_set_attributes(const char* path,
                                    const FatAttributeFlags attributes)
{
	wchar_t utf16_path[MAX_PATH] = {};
	if (!codepage437_to_utf16(path, utf16_path)) {
		return DOSERR_ACCESS_DENIED;
	}
	if (!SetFileAttributesW(utf16_path, attributes._data)) {
		return static_cast<uint16_t>(GetLastError());
	}

	return DOSERR_NONE;
}

NativeFileHandle open_native_file(const char* path, const bool write_access)
{
	wchar_t utf16_path[MAX_PATH] = {};
	if (!codepage437_to_utf16(path, utf16_path)) {
		return InvalidNativeFileHandle;
	}
	DWORD access = GENERIC_READ;
	if (write_access) {
		access |= GENERIC_WRITE;
	}

	return CreateFileW(utf16_path,
	                   access,
	                   FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
	                   nullptr,
	                   OPEN_EXISTING,
	                   FILE_ATTRIBUTE_NORMAL,
	                   nullptr);
}

NativeFileHandle create_native_file(const char* path,
                                    const std::optional<FatAttributeFlags> attributes)
{
	wchar_t utf16_path[MAX_PATH] = {};
	if (!codepage437_to_utf16(path, utf16_path)) {
		return InvalidNativeFileHandle;
	}
	const auto win32_attributes = (attributes && attributes->_data != 0)
	                                    ? static_cast<DWORD>(attributes->_data)
	                                    : FILE_ATTRIBUTE_NORMAL;

	return CreateFileW(utf16_path,
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

	// FileTimeToLocalFileTime does seem to correctly account for DST but
	// we're doing this anyway for consistency. See the comment in
	// set_dos_file_time.
	SYSTEMTIME write_systime = {};
	if (!FileTimeToSystemTime(&write_time, &write_systime)) {
		return ret;
	}

	SYSTEMTIME local_write_systime = {};
	if (!SystemTimeToTzSpecificLocalTimeEx(nullptr,
	                                       &write_systime,
	                                       &local_write_systime)) {
		return ret;
	}

	FILETIME local_write_time = {};
	if (!SystemTimeToFileTime(&local_write_systime, &local_write_time)) {
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

	// We cannot use LocalFileTimeToFileTime because it uses the *current*
	// DST state when converting from local time to UTC instead of DST state
	// during the date that was passed.
	SYSTEMTIME local_write_systime = {};
	if (!FileTimeToSystemTime(&local_write_time, &local_write_systime)) {
		return;
	}

	SYSTEMTIME write_systime = {};
	if (!TzSpecificLocalTimeToSystemTimeEx(nullptr,
	                                       &local_write_systime,
	                                       &write_systime)) {
		return;
	}

	FILETIME write_time = {};
	if (!SystemTimeToFileTime(&write_systime, &write_time)) {
		return;
	}

	SetFileTime(handle, nullptr, nullptr, &write_time);
}

// Includes a Windows specific hack.
// We move the file to be deleted to a temp file before delete.
// This allows a file with the same name to be created even if there are open
// file handles. See bug report about the game Abuse:
// https://github.com/dosbox-staging/dosbox-staging/issues/4123
bool delete_native_file(const char* path)
{
	wchar_t utf16_path[MAX_PATH] = {};
	if (!codepage437_to_utf16(path, utf16_path)) {
		return false;
	}

	// This one should never throw as we've already converted to UTF-16 succesfully
	std_fs::path stdpath(utf16_path);

	// Prefix of the temp file. Only uses 3 characters.
	// $ as convention to designate temp file followed by DB for DosBox.
	const wchar_t* prefix = L"$DB";

	// Zero for UniqueNumber uses system time.
	// Zero also means it creates the file and ensures it is unique.
	constexpr UINT UniqueNumber = 0;

	// Maximum return size of GetTempFileNameW() is MAX_PATH plus a null
	// terminator.
	wchar_t temp_file[MAX_PATH + 1] = {};

	// Create a temporary file inside the same directory as the file to be
	// deleted. Returns unique identifier on success and 0 on failure.
	if (GetTempFileNameW(stdpath.parent_path().c_str(), prefix, UniqueNumber, temp_file) ==
	    0) {
		LOG_ERR("FS: Failed to create temp file. Deleting '%s' directly.",
		        stdpath.string().c_str());
		// We failed to create a temp file but we should still try to
		// delete the original file.
		return DeleteFileW(stdpath.c_str());
	}

	// Above function creates the temp file so we replace it here with the
	// flag MOVEFILE_REPLACE_EXISTING.
	if (MoveFileExW(stdpath.c_str(), temp_file, MOVEFILE_REPLACE_EXISTING)) {
		// We can immediately delete the temporary file.
		// Any open handles can be still be read from or written to.
		// A new file can be created with the original file name.
		if (!DeleteFileW(temp_file)) {
			LOG_ERR("FS: Failed to delete temporary file: '%s'",
			        std_fs::path(temp_file).string().c_str());
			// Failed to delete the temp file but the original file
			// was moved. We should still return success so that the
			// filesystem removes the original file from the cache.
		}
		return true;
	}

	// We failed to move the file. We need to delete both the temp file and
	// the original file.
	LOG_ERR("FS: Failed to move '%s' to temp file '%s' before delete.",
	        stdpath.string().c_str(),
	        std_fs::path(temp_file).string().c_str());
	if (!DeleteFileW(temp_file)) {
		LOG_ERR("FS: Failed to delete temporary file: '%s'",
		        std_fs::path(temp_file).string().c_str());
	}
	return DeleteFileW(stdpath.c_str());
}

bool local_drive_remove_dir(const char* path)
{
	wchar_t utf16_path[MAX_PATH] = {};
	if (!codepage437_to_utf16(path, utf16_path)) {
		return false;
	}
	if (RemoveDirectoryW(utf16_path)) {
		return true;
	}
	// Windows specific hack: MS-DOS allows removal of read-only directories.
	// Windows does not. If we have a read-only directory, we need to make it not read-only.
	DWORD attributes = GetFileAttributesW(utf16_path);
	if (attributes == INVALID_FILE_ATTRIBUTES) {
		return false;
	}
	if ((attributes & FILE_ATTRIBUTE_READONLY) == 0) {
		return false;
	}
	attributes &= ~FILE_ATTRIBUTE_READONLY;
	if (!SetFileAttributesW(utf16_path, attributes)) {
		return false;
	}
	if (RemoveDirectoryW(utf16_path)) {
		return true;
	}
	// Removal still failed. Restore original attributes.
	attributes |= FILE_ATTRIBUTE_READONLY;
	SetFileAttributesW(utf16_path, attributes);
	return false;
}

bool local_drive_path_exists(const char* path)
{
	wchar_t utf16_path[MAX_PATH] = {};
	if (!codepage437_to_utf16(path, utf16_path)) {
		return false;
	}
	return PathFileExistsW(utf16_path);
}

bool local_drive_rename_file_or_directory(const char* old_path, const char* new_path)
{
	wchar_t utf16_old_path[MAX_PATH] = {};
	if (!codepage437_to_utf16(old_path, utf16_old_path)) {
		return false;
	}
	wchar_t utf16_new_path[MAX_PATH] = {};
	if (!codepage437_to_utf16(new_path, utf16_new_path)) {
		return false;
	}
	return MoveFileW(utf16_old_path, utf16_new_path);
}

#endif
