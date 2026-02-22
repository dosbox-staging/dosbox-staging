// SPDX-FileCopyrightText:  2020-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "utils/fs_utils.h"

#if !defined(WIN32)

#include <cctype>
#include <cerrno>
#include <fcntl.h>
#include <glob.h>
#include <optional>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#if defined(HAVE_SYS_XATTR_H)
#include <sys/xattr.h>
#endif

#include "dos/dos.h"
#include "misc/logging.h"
#include "utils/env_utils.h"
#include "utils/string_utils.h"

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
		case ']': glob_pattern.push_back('\\'); [[fallthrough]];
		default: glob_pattern.push_back(c); continue;
		}
	}
	return glob_pattern;
}

std::string to_native_path(const std::string &path) noexcept
{
	if (local_drive_path_exists(path.c_str()))
		return path;

	// Perhaps path is ok, just using Windows-style delimiters:
	const std::string posix_path = replace(path, '\\', '/');
	if (local_drive_path_exists(posix_path.c_str()))
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
		LOG_DEBUG("FS: glob error (%d) while searching for '%s'",
		          err,
		          path.c_str());
		globfree(&pglob);
		return "";
	}
	if (pglob.gl_pathc > 1) {
		LOG_DEBUG("FS: Searching for path '%s' gives ambiguous results:",
		          path.c_str());

		for (size_t i = 0; i < pglob.gl_pathc; i++) {
			LOG_DEBUG("%s", pglob.gl_pathv[i]);
		}
	}
	const std::string ret = pglob.gl_pathv[0];
	globfree(&pglob);
	return ret;
}

uint16_t local_drive_create_dir(const char* path)
{
	static_assert(sizeof(uint32_t) >= sizeof(mode_t), "");
	const int err = mkdir(path, 0775);
	if (err == 0) {
		return DOSERR_NONE;
	}
	return DOSERR_ACCESS_DENIED;
}

#if !defined(MACOSX)

std_fs::path get_xdg_config_home() noexcept
{
	const auto var       = get_env_var("XDG_CONFIG_HOME");
	const auto conf_home = ((var.size() > 0) ? var : "~/.config");
	return resolve_home(conf_home);
}

std_fs::path get_xdg_data_home() noexcept
{
	const auto var       = get_env_var("XDG_DATA_HOME");
	const auto data_home = ((var.size() > 0) ? var : "~/.local/share");
	return resolve_home(data_home);
}

std::deque<std_fs::path> get_xdg_data_dirs() noexcept
{
	const auto var       = get_env_var("XDG_DATA_DIRS");
	const auto data_dirs = ((var.size() > 0) ? var : "/usr/local/share:/usr/share");

	std::deque<std_fs::path> paths{};
	for (auto& dir : split_with_empties(data_dirs, ':')) {
		trim(dir);
		if (!dir.empty()) {
			paths.emplace_back(resolve_home(dir));
		}
	}
	return paths;
}

#endif

// ***************************************************************************
// Local drive file/directory attribute handling, WINE-compatible
// ***************************************************************************

constexpr int PermissionsRO = S_IRUSR | S_IRGRP | S_IROTH;
constexpr int PermissionsRW = S_IWUSR | S_IWGRP | S_IWOTH | PermissionsRO;

// Attributes 'hidden', 'system', and 'archive' are always taken from the
// host extended attributes.
// XattrReadMask = bitflags for read-only, hidden, system, and archive
constexpr uint8_t XattrReadMask = 0b0010'0111;

// Attributes 'read-only' and 'directory' are stored in extended attributes.
// For files, read-only is checked using host file system permissions.
// For directories, read-only is checked by either file system permission or the extended attribute.
// We do not set file system permissions when a DOS program marks a directory as read-only.
// MS-DOS does not prevent new files from being created inside read-only directories.
// Linux does prevent this so we must not set the host directory read-only.
// XattrWriteMask = bitflag for directory OR'd with XattrReadMask
constexpr uint8_t XattrWriteMask = 0b0001'0000 | XattrReadMask;

// We are storing DOS file attributes in Unix extended attributes, using same
// format as WINE, Samba 3, and DOSEmu 2.
static const std::string XattrName = "user.DOSATTRIB";

constexpr size_t XattrMinLength = 3;
constexpr size_t XattrMaxLength = 4;

static std::string fat_attribs_to_xattr(const FatAttributeFlags fat_attribs)
{
	return format_str("0x%x", fat_attribs._data & XattrWriteMask);
}

static std::optional<FatAttributeFlags> xattr_to_fat_attribs(const std::string& xattr)
{
	constexpr uint8_t HexBase = 16;

	if (xattr.size() <= XattrMaxLength && xattr.starts_with("0x") &&
	    xattr.size() >= XattrMinLength) {
		const auto value = parse_int(xattr.substr(2), HexBase);
		if (value) {
			return *value & XattrReadMask;
		}
	}

	return {};
}

static std::optional<FatAttributeFlags> get_xattr(const char* path)
{
	char xattr[XattrMaxLength + 1];

#ifdef MACOSX
	constexpr int offset  = 0;
	constexpr int options = 0;

	const auto length = getxattr(path,
	                             XattrName.c_str(),
	                             xattr,
	                             XattrMaxLength,
	                             offset,
	                             options);
#elif defined(HAVE_SYS_XATTR_H)
	const auto length = getxattr(path,
	                             XattrName.c_str(),
	                             xattr,
	                             XattrMaxLength);
#else
	// Platform doesn't support extended attributes
	// So we always set a failed return length.
	constexpr ssize_t length = -1;
#endif
	if (length <= 0) {
		// No extended attribute present
		return {};
	}

	if (static_cast<size_t>(length) > XattrMaxLength) {
		LOG_MSG("DOS: Incorrect '%s' extended attribute in '%s'",
		        XattrName.c_str(), path);
		return {};
	}

	xattr[length] = 0;
	return xattr_to_fat_attribs(std::string(xattr));
}

static bool set_xattr([[maybe_unused]] const char* path,
                      const FatAttributeFlags attributes)
{
	const auto xattr = fat_attribs_to_xattr(attributes);

#ifdef MACOSX
	constexpr int offset  = 0;
	constexpr int options = 0;

	const auto result = setxattr(path,
	                             XattrName.c_str(),
	                             xattr.c_str(),
	                             xattr.size(),
	                             offset,
	                             options);
#elif defined(HAVE_SYS_XATTR_H)
	constexpr int flags = 0;

	const auto result = setxattr(path,
                                     XattrName.c_str(),
                                     xattr.c_str(),
                                     xattr.size(),
                                     flags);
#else
	// Platform doesn't support extended attributes
	// so we always have a failed result code.
	constexpr int result = -1;
#endif
	return (result == 0);
}

static bool set_xattr([[maybe_unused]] const int file_descriptor,
                      const FatAttributeFlags attributes)
{
	const auto xattr = fat_attribs_to_xattr(attributes);

#ifdef MACOSX
	constexpr int offset  = 0;
	constexpr int options = 0;

	const auto result = fsetxattr(file_descriptor,
	                              XattrName.c_str(),
	                              xattr.c_str(),
	                              xattr.size(),
	                              offset,
	                              options);
#elif defined(HAVE_SYS_XATTR_H)
	constexpr int flags = 0;

	const auto result = fsetxattr(file_descriptor,
	                              XattrName.c_str(),
	                              xattr.c_str(),
	                              xattr.size(),
	                              flags);
#else
	// Platform doesn't support extended attributes
	// so we always have a failed result code.
	constexpr int result = -1;
#endif
	return (result == 0);
}

uint16_t local_drive_get_attributes(const char* path,
                                    FatAttributeFlags& attributes)
{
	struct stat status;
	if (stat(path, &status) != 0) {
		attributes = 0;
		return DOSERR_FILE_NOT_FOUND;
	}
	const bool is_directory = status.st_mode & S_IFDIR;
	const bool is_read_only = !(status.st_mode & S_IWUSR);

	const auto result = get_xattr(path);
	if (result) {
		attributes = *result;
	} else {
		attributes         = 0;
		attributes.archive = !is_directory;
	}

	attributes.directory = is_directory;
	if (is_directory) {
		// Directories need to honor the extended attribute.
		// See comment above XattrWriteMask.
		attributes.read_only = attributes.read_only || is_read_only;
	} else {
		attributes.read_only = is_read_only;
	}

	return DOSERR_NONE;
}

uint16_t local_drive_set_attributes(const char* path,
                                    const FatAttributeFlags attributes)
{
	if (!local_drive_path_exists(path)) {
		return DOSERR_FILE_NOT_FOUND;
	}

	bool status = make_writable(path);
	if (status) {
		struct stat file_info;
		bool is_directory = true;
		if (stat(path, &file_info) == 0) {
			is_directory = file_info.st_mode & S_IFDIR;
		}
		status = set_xattr(path, attributes);
		if (status && attributes.read_only && !is_directory) {
			// Sets permissions on the host filesystem.
			// Don't do this for directories.
			// MS-DOS allows new files to be created in read-only directories.
			status = make_readonly(path);
		}
	}

	return status ? DOSERR_NONE : DOSERR_ACCESS_DENIED;
}

NativeFileHandle open_native_file(const char* path, const bool write_access)
{
	return open(path, write_access ? O_RDWR : O_RDONLY);
}

NativeFileHandle create_native_file(const char* path,
                                    const std::optional<FatAttributeFlags> attributes)
{
	const auto file_descriptor = open(path,
	                                  O_CREAT | O_RDWR | O_TRUNC,
	                                  PermissionsRW);
	if (attributes && file_descriptor != InvalidNativeFileHandle) {
		set_xattr(file_descriptor, *attributes);
	}

	return file_descriptor;
}

// POSIX does not guarantee to read or write all bytes requested at once.
// On Linux, it will read the entire chunk in one call (assuming it's a normal
// file). On Windows, it does not always do so (although we're not using these
// functions on Windows). On Mac and the various BSDs, who knows. So be safe and
// do a loop.
NativeIoResult read_native_file(const NativeFileHandle handle, uint8_t* buffer,
                                const int64_t num_bytes_requested)
{
	NativeIoResult ret = {};
	ret.num_bytes      = 0;
	ret.error          = false;
	while (ret.num_bytes < num_bytes_requested) {
		const auto num_bytes_read = read(handle,
		                                 buffer + ret.num_bytes,
		                                 num_bytes_requested - ret.num_bytes);
		if (num_bytes_read <= 0) {
			ret.error = num_bytes_read < 0;
			break;
		}
		ret.num_bytes += num_bytes_read;
	}
	return ret;
}

NativeIoResult write_native_file(const NativeFileHandle handle, const uint8_t* buffer,
                                 const int64_t num_bytes_requested)
{
	NativeIoResult ret = {};
	ret.num_bytes      = 0;
	ret.error          = false;
	while (ret.num_bytes < num_bytes_requested) {
		const auto num_bytes_written = write(handle,
		                                     buffer + ret.num_bytes,
		                                     num_bytes_requested -
		                                             ret.num_bytes);
		if (num_bytes_written <= 0) {
			ret.error = num_bytes_written < 0;
			break;
		}
		ret.num_bytes += num_bytes_written;
	}
	return ret;
}

int64_t seek_native_file(const NativeFileHandle handle, const int64_t offset,
                         const NativeSeek type)
{
	int posix_seek_type = SEEK_SET;
	switch (type) {
	case NativeSeek::Set: posix_seek_type = SEEK_SET; break;
	case NativeSeek::Current: posix_seek_type = SEEK_CUR; break;
	case NativeSeek::End: posix_seek_type = SEEK_END; break;
	default: assertm(false, "Invalid seek type"); return NativeSeekFailed;
	}

	const auto position = lseek(handle, offset, posix_seek_type);
	if (position < 0) {
		return NativeSeekFailed;
	}

	return position;
}

void close_native_file(const NativeFileHandle handle)
{
	close(handle);
}

// Sets the file size to be equal to the current file position
bool truncate_native_file(const NativeFileHandle handle)
{
	const auto current_position = lseek(handle, 0, SEEK_CUR);
	if (current_position < 0) {
		return false;
	}
	return ftruncate(handle, current_position) == 0;
}

DosDateTime get_dos_file_time(const NativeFileHandle handle)
{
	// Legal defaults if we're unable to populate them
	DosDateTime ret = {};
	ret.time        = 1;
	ret.date        = 1;

	struct stat file_info = {};
	if (fstat(handle, &file_info) == -1) {
		return ret;
	}

	struct tm datetime = {};
	if (!cross::localtime_r(&file_info.st_mtime, &datetime)) {
		return ret;
	}

	ret.time = DOS_PackTime(datetime);
	ret.date = DOS_PackDate(datetime);

	return ret;
}

void set_dos_file_time(const NativeFileHandle handle, const uint16_t date, const uint16_t time)
{
	auto datetime = DOS_UnpackDateTime(date, time);

	const auto unix_seconds = mktime(&datetime);
	if (unix_seconds == -1) {
		return;
	}

	struct timespec unix_times[2] = {};

	// Last access time
	// We don't really care about this but apparently it must be updated as well
	unix_times[0].tv_sec = unix_seconds;
	unix_times[0].tv_nsec = 0;

	// Last modification time
	unix_times[1].tv_sec = unix_seconds;
	unix_times[1].tv_nsec = 0;

	// I like hating on Win32 API but at least they have better function names...
	// F U too Mr. Timens
	futimens(handle, unix_times);
}

bool delete_native_file(const char* path)
{
	return unlink(path) == 0;
}

bool local_drive_remove_dir(const char* path)
{
	return rmdir(path) == 0;
}

bool local_drive_path_exists(const char* path)
{
	return access(path, F_OK) == 0;
}

bool local_drive_rename_file_or_directory(const char* old_path, const char* new_path)
{
	return rename(old_path, new_path) == 0;
}

#endif
