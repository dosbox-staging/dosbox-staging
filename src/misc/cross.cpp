/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2021-2024  The DOSBox Staging Team
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

#include "cross.h"

#include <cerrno>
#include <climits>
#include <clocale>
#include <cstdlib>
#include <string>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#if C_COREFOUNDATION
#	include <CoreFoundation/CoreFoundation.h>
#endif

#ifdef WIN32
#	include <winnls.h>

#	ifndef _WIN32_IE
#		define _WIN32_IE 0x0400
#	endif
#	include <shlobj.h>
#else // other than Windows
#	if defined(HAVE_LIBGEN_H)
#		include <libgen.h>
#	endif
#	if defined(HAVE_SYS_XATTR_H)
#		include <sys/xattr.h>
#	endif
#endif

#if defined(HAVE_PWD_H)
#	include <pwd.h>
#endif

#include "fs_utils.h"
#include "string_utils.h"
#include "support.h"
#include "drives.h"

std::string GetPrimaryConfigName()
{
	return DOSBOX_PROJECT_NAME ".conf";
}

std_fs::path GetPrimaryConfigPath()
{
	return GetConfigDir() / GetPrimaryConfigName();
}

#if defined(MACOSX)

static std_fs::path get_or_create_config_dir()
{
	const auto conf_path = resolve_home("~/Library/Preferences/DOSBox");

	constexpr auto success = 0;
	if (create_dir(conf_path, 0700, OK_IF_EXISTS) == success) {

	} else {
		LOG_ERR("CONFIG: Can't create config directory '%s': %s",
		        conf_path.string().c_str(),
		        safe_strerror(errno).c_str());
	}
	return conf_path;
}

#elif defined(WIN32)

static std_fs::path get_or_create_config_dir()
{
	char appdata_path[MAX_PATH] = {0};

	const int create = 1;

	// CSIDL_LOCAL_APPDATA - The file system directory that serves as a data
	// repository for local (nonroaming) applications. A typical path is:
	// C:\Documents and Settings\username\Local Settings\Application Data.
	//
	BOOL success = SHGetSpecialFolderPath(nullptr,
	                                      appdata_path,
	                                      CSIDL_LOCAL_APPDATA,
	                                      create);

	if (!success || appdata_path[0] == 0) {
		// CSIDL_APPDATA - The file system directory that serves as a data
		// repository for local (nonroaming) applications. A typical path is
		// C:\Documents and Settings\username\Local Settings\Application Data.
		//
		success = SHGetSpecialFolderPath(nullptr,
		                                 appdata_path,
		                                 CSIDL_APPDATA,
		                                 create);
	}

	const auto conf_path = std_fs::path(appdata_path) / "DOSBox";

	constexpr auto success_result = 0;
	if (create_dir(conf_path, 0700, OK_IF_EXISTS) != success_result) {
		LOG_ERR("CONFIG: Can't create config directory '%s': %s",
		        conf_path.string().c_str(),
		        safe_strerror(errno).c_str());
	}

	return conf_path;
}

#else // Use generally compatible Linux, BSD, and *nix-compatible calls.

// If an OS can't handle this (ie: Haiku, Android, etc..) then add a
// new pre-processor block for it.

static std_fs::path get_or_create_config_dir()
{
	const auto conf_path = get_xdg_config_home() / "dosbox";
	std::error_code ec   = {};

	if (std_fs::exists(conf_path / GetPrimaryConfigName())) {
		return conf_path;
	}

	auto fallback_to_deprecated = []() {
		const auto old_conf_path = resolve_home("~/.dosbox");
		if (path_exists(old_conf_path / GetPrimaryConfigName())) {
			LOG_WARNING("CONFIG: Falling back to deprecated path (~/.dosbox) due to errors");
			LOG_WARNING("CONFIG: Please investigate the problems and try again");
		}
		return old_conf_path;
	};

	if (!std_fs::exists(conf_path, ec)) {
		if (std_fs::create_directories(conf_path, ec)) {
			return conf_path;
		}
		LOG_ERR("CONFIG: Path '%s' cannot be created (permission issue or broken symlink?)",
		        conf_path.c_str());
		return fallback_to_deprecated();
	}

	// The conf path exists - but is it a directory, file, or symlink(s)?
	assert(std_fs::exists(conf_path, ec));

	if (std_fs::is_directory(conf_path, ec)) {
		return conf_path;
	}

	if (std_fs::is_regular_file(conf_path, ec)) {
		LOG_ERR("CONFIG: Path '%s' exists, but it's a file",
		        conf_path.c_str());
		return fallback_to_deprecated();
	}

	if (std_fs::is_symlink(conf_path, ec)) {
		auto target_path = std_fs::read_symlink(conf_path, ec);

		// If it's a symlink to a symlink, then keep reading them...
		auto num_symlinks_read = 1;

		// ...but bail out if they're circular links
		while (std_fs::is_symlink(target_path, ec) &&
		       num_symlinks_read++ < 100) {
			target_path = std_fs::read_symlink(target_path, ec);
		}
		// If the last symlink points to a directory, then we'll take it
		if (std_fs::is_directory(target_path, ec)) {
			return target_path;
		}
		LOG_ERR("CONFIG: Path '%s' cannot be created because it's symlinked to '%s'",
		        conf_path.c_str(),
		        target_path.c_str());
	} else {
		LOG_ERR("CONFIG: Path '%s' exists, but it's not a directory or a symlink",
		        conf_path.c_str());
	}
	return fallback_to_deprecated();
}

#endif // get_or_create_config_dir()

static std_fs::path cached_config_dir = {};

void InitConfigDir()
{
	if (cached_config_dir.empty()) {
		// Check if a portable layout exists
		const auto portable_conf_path = GetExecutablePath() /
		                                GetPrimaryConfigName();

		std::error_code ec = {};
		if (std_fs::is_regular_file(portable_conf_path, ec)) {
			const auto conf_dir = portable_conf_path.parent_path();

			LOG_MSG("CONFIG: Using portable configuration layout in '%s'",
			        conf_dir.string().c_str());

			cached_config_dir = conf_dir;
		} else {
			cached_config_dir = get_or_create_config_dir();
		}
	}
}

std_fs::path GetConfigDir()
{
	assert(!cached_config_dir.empty());
	return cached_config_dir;
}

std_fs::path resolve_home(const std::string &str) noexcept
{
	if (!str.size() || str[0] != '~') // No ~
		return str;

	std::string temp_line = str;
	if(temp_line.size() == 1 || temp_line[1] == CROSS_FILESPLIT) { //The ~ and ~/ variant
		char * home = getenv("HOME");
		if(home) temp_line.replace(0,1,std::string(home));

#if defined(HAVE_SYS_TYPES_H) && defined(HAVE_PWD_H)
	} else { // The ~username variant
		std::string::size_type namelen = temp_line.find(CROSS_FILESPLIT);
		if(namelen == std::string::npos) namelen = temp_line.size();
		std::string username = temp_line.substr(1,namelen - 1);
		struct passwd* pass = getpwnam(username.c_str());
		if(pass) temp_line.replace(0,namelen,pass->pw_dir); //namelen -1 +1(for the ~)
#endif // USERNAME lookup code
	}
	return temp_line;
}

#if defined(WIN32)

dir_information* open_directory(const char* dirname) {
	if (dirname == nullptr) return nullptr;

	size_t len = strlen(dirname);
	if (len == 0) return nullptr;

	static dir_information dir;

	safe_strncpy(dir.base_path,dirname,MAX_PATH);

	if (dirname[len - 1] == '\\')
		safe_strcat(dir.base_path, "*.*");
	else
		safe_strcat(dir.base_path, "\\*.*");

	dir.handle = INVALID_HANDLE_VALUE;

	return (path_exists(dirname) ? &dir : nullptr);
}

bool read_directory_first(dir_information* dirp, char* entry_name, bool& is_directory) {
	if (!dirp) return false;
	dirp->handle = FindFirstFile(dirp->base_path, &dirp->search_data);
	if (INVALID_HANDLE_VALUE == dirp->handle) {
		return false;
	}

	safe_strncpy(entry_name,dirp->search_data.cFileName,(MAX_PATH<CROSS_LEN)?MAX_PATH:CROSS_LEN);

	if (dirp->search_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) is_directory = true;
	else is_directory = false;

	return true;
}

bool read_directory_next(dir_information* dirp, char* entry_name, bool& is_directory) {
	if (!dirp) return false;
	int result = FindNextFile(dirp->handle, &dirp->search_data);
	if (result==0) return false;

	safe_strncpy(entry_name,dirp->search_data.cFileName,(MAX_PATH<CROSS_LEN)?MAX_PATH:CROSS_LEN);

	if (dirp->search_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) is_directory = true;
	else is_directory = false;

	return true;
}

void close_directory(dir_information* dirp) {
	if (dirp && dirp->handle != INVALID_HANDLE_VALUE) {
		FindClose(dirp->handle);
		dirp->handle = INVALID_HANDLE_VALUE;
	}
}

#else

dir_information* open_directory(const char* dirname) {
	static dir_information dir;
	dir.dir=opendir(dirname);
	safe_strcpy(dir.base_path, dirname);
	return dir.dir?&dir:nullptr;
}

bool read_directory_first(dir_information* dirp, char* entry_name, bool& is_directory) {
	if (!dirp) return false;
	return read_directory_next(dirp,entry_name,is_directory);
}

bool read_directory_next(dir_information* dirp, char* entry_name, bool& is_directory) {
	if (!dirp) return false;
	struct dirent* dentry = readdir(dirp->dir);
	if (dentry==nullptr) {
		return false;
	}

//	safe_strncpy(entry_name,dentry->d_name,(FILENAME_MAX<MAX_PATH)?FILENAME_MAX:MAX_PATH);	// [include stdio.h], maybe pathconf()
	safe_strncpy(entry_name,dentry->d_name,CROSS_LEN);

	// TODO check if this check can be replaced with glibc-defined
	// _DIRENT_HAVE_D_TYPE. Non-GNU systems (BSD) provide d_type field as
	// well, but do they provide define?
	// Alternatively, maybe we can replace whole directory listing with
	// C++17 std::filesystem::directory_iterator.
#ifdef HAVE_STRUCT_DIRENT_D_TYPE
	if (dentry->d_type == DT_DIR) {
		is_directory = true;
		return true;
	} else if (dentry->d_type == DT_REG) {
		is_directory = false;
		return true;
	}
#endif

	// Maybe only for DT_UNKNOWN if HAVE_STRUCT_DIRENT_D_TYPE
	static char buffer[2 * CROSS_LEN + 1] = { 0 };
	static char split[2] = { CROSS_FILESPLIT , 0 };
	buffer[0] = 0;
	safe_strcpy(buffer, dirp->base_path);
	size_t buflen = safe_strlen(buffer);
	if (buflen && buffer[buflen - 1] != CROSS_FILESPLIT)
		safe_strcat(buffer, split);
	safe_strcat(buffer, entry_name);
	struct stat status;

	if (stat(buffer,&status) == 0) is_directory = (S_ISDIR(status.st_mode)>0);
	else is_directory = false;

	return true;
}

void close_directory(dir_information* dirp) {
	if (dirp) closedir(dirp->dir);
}

#endif

// A helper for fopen that will fallback to read-only if read-write isn't possible.
// In the fallback case, is_readonly argument is toggled to true.
// In all cases, a pointer to the file is returned (or nullptr on failure).
FILE *fopen_wrap_ro_fallback(const std::string &filename, bool &is_readonly)
{
	// Try with the requested permissions
	const auto requested_perms = is_readonly ? "rb" : "rb+";
	FILE *fp = fopen(filename.c_str(), requested_perms);
	if (fp || is_readonly) {
		return fp;
	}
	// Fallback to read-only
	assert(!fp && !is_readonly);
	fp = fopen(filename.c_str(), "rb");
	if (fp) {
		is_readonly = true;
		LOG_INFO("FILESYSTEM: Opened %s read-only per host filesystem permissions",
		         filename.c_str());
	}
	// Note: if failed, the caller should provide a context-specific message
	return fp;
}

bool wild_match(const char *haystack, const char *needle)
{
	assert(haystack);
	assert(needle);
	const char *p = needle;
	while(*p != '\0') {
		switch (*p) {
		case '?':
			if (*haystack == '\0')
				return false;
			++haystack;
			break;
		case '*':
		{
			if (p[1] == '\0')
				return true;
			const auto max = strlen(haystack);
			for (size_t i = 0; i < max; i++)
				if (wild_match(haystack + i, p + 1))
					return true;
			return false;
		}
		default:
			if (toupper(*haystack) != *p)
				return false;
			++haystack;
		}
		++p;
	}
	return *haystack == '\0';
}

static bool wildcard_matches_hidden_file(const std::string_view filename,
                                         const std::string_view wildcard)
{
	const auto is_wildcard_first = wildcard.find_first_of("?*") !=
	                               std::string_view::npos;

	// DOS files can be named ".EXT", so at a minimum we only consider files
	// long than this pattern (that also begin with a dot).
	constexpr size_t min_length = 5;

	const auto is_hidden_file = filename.size() >= min_length &&
	                            filename[0] == '.' &&
	                            !(filename == "." || filename == "..");

	return is_wildcard_first && is_hidden_file;
}

bool WildFileCmp(const char* file, const char* wild, bool long_compare)
{
	if (!file || !wild || (*file && !*wild) || strlen(wild) > LFN_NAMELENGTH)
		return false;

	// Shell commands (like cp, rm, find) ignore dot files in wildcard
	// patterns on MSYS2, MacOS, Linux, and BSD, so we mirror that behaviour.
	if (wildcard_matches_hidden_file(file, wild)) {
		LOG_WARNING("FS: Skipping hidden file '%s' for pattern '%s'", file, wild);
		return false;
	}

	char file_name[LFN_NAMELENGTH + 1];
	char file_ext[LFN_NAMELENGTH + 1];
	char wild_name[LFN_NAMELENGTH + 1];
	char wild_ext[LFN_NAMELENGTH + 1];
	Bitu r;

	if (long_compare) {
		for (r = 0; r <= LFN_NAMELENGTH; r++)
			file_name[r] = wild_name[r] = file_ext[r] = wild_ext[r] = 0;
	} else {
		strcpy(file_name, "        ");
		strcpy(file_ext, "   ");
		strcpy(wild_name, "        ");
		strcpy(wild_ext, "   ");
	}

	Bitu size = 0;
	size_t elength = 0;
	const char *find_ext;
	find_ext = strrchr(file, '.');
	if (find_ext) {
		size = (std::min)((unsigned int)(long_compare ? LFN_NAMELENGTH
		                                              : DOS_MFNLENGTH),
		                  (unsigned int)(find_ext - file));
		memcpy(file_name, file, size);
		find_ext++;
		elength = strlen(find_ext);
		memcpy(file_ext, find_ext,
		       strnlen(find_ext,
		               long_compare ? LFN_NAMELENGTH : DOS_EXTLENGTH));
	} else {
		size = strlen(file);
		elength = 0;
		memcpy(file_name, file,
		       strnlen(file, long_compare ? LFN_NAMELENGTH : DOS_MFNLENGTH));
	}
	upcase(file_name);
	upcase(file_ext);
	char nwild[LFN_NAMELENGTH + 2];
	strcpy(nwild, wild);
	if (long_compare && strrchr(nwild, '*') && strrchr(nwild, '.') == nullptr)
		strcat(nwild, ".*");
	find_ext = strrchr(nwild, '.');
	if (find_ext) {
		if (long_compare && wild_match(file, nwild))
			return true;

		const uint16_t name_len = long_compare ? LFN_NAMELENGTH
		                                       : (DOS_MFNLENGTH + 1);

		const auto ext_len = check_cast<uint16_t>(find_ext - nwild);

		const auto wild_len = std::min(name_len, ext_len);

		memcpy(wild_name, nwild, wild_len);

		find_ext++;
		memcpy(wild_ext, find_ext,
		       strnlen(find_ext,
		               (long_compare ? LFN_NAMELENGTH : DOS_EXTLENGTH) + 1));
	} else {
		memcpy(wild_name, wild,
		       strnlen(wild,
		               (long_compare ? LFN_NAMELENGTH : DOS_MFNLENGTH) + 1));
	}
	upcase(wild_name);
	upcase(wild_ext);
	/* Names are right do some checking */
	if (long_compare && strchr(wild_name, '*')) {
		if (!strchr(wild, '.'))
			return wild_match(file, wild_name);
		else if (!wild_match(file_name, wild_name))
			return false;
	} else {
		r = 0;
		while (r < (long_compare ? size : DOS_MFNLENGTH)) {
			if (wild_name[r] == '*')
				break;
			if (wild_name[r] != '?' && wild_name[r] != file_name[r])
				return false;
			r++;
		}
		if (wild_name[r] && wild_name[r] != '*')
			return false;
	}
	if (long_compare && strchr(wild_ext, '*'))
		return wild_match(file_ext, wild_ext);
	else {
		r = 0;
		while (r < (long_compare ? elength : DOS_EXTLENGTH)) {
			if (wild_ext[r] == '*')
				return true;
			if (wild_ext[r] != '?' && wild_ext[r] != file_ext[r])
				return false;
			r++;
		}
		if (wild_ext[r] && wild_ext[r] != '*')
			return false;
		return true;
	}
}

bool get_expanded_files(const std::string &path,
                        std::vector<std::string> &paths,
                        bool files_only,
                        bool skip_native_path) noexcept
{
	if (!skip_native_path) {
		const auto real_path = to_native_path(path);
		if (real_path.length()) {
			paths.push_back(real_path);
			return true;
		}
	}

	std::vector<std::string> files;
	const std_fs::path p = path;
	auto dir = p.parent_path();
	const auto dir_str = dir.string();
	const auto native_dir = to_native_path(dir_str);
	if (dir_str.size() && native_dir.empty())
		return false;

	dir = native_dir.empty() ? "." : native_dir;

	// Keep recursing past permission issues and follow symlinks
	constexpr auto idir_opts = std_fs::directory_options::skip_permission_denied |
	                           std_fs::directory_options::follow_directory_symlink;

	std::error_code ec = {};
	for (const auto &entry : std_fs::directory_iterator(dir, idir_opts, ec)) {
		if (ec)
			break; // problem iterating, so skip the directory

		// caller wants only files but this entry isn't, so skip it
		if (files_only && !entry.is_regular_file(ec))
			continue;

		const auto filename = entry.path().filename();

		constexpr auto long_compare = true;
		if (WildFileCmp(filename.string().c_str(),
		                p.filename().string().c_str(),
		                long_compare)) {
			files.push_back((dir / filename).string());
		}
	}

	if (files.size()) {
		sort(files.begin(), files.end());
		paths.insert(paths.end(), files.begin(), files.end());
		return true;
	} else {
		return false;
	}
}

#if C_COREFOUNDATION
std::string cfstr_to_string(CFStringRef source)
{
	if (!source)
		return {};

	// Try to get the internal char-compatible buffer
	constexpr auto encoding = kCFStringEncodingUTF8;
	const auto buf = CFStringGetCStringPtr(source, encoding);
	if (buf)
		return buf;

	// If the char-compatible buffer doesn't exist; it's probably wide-encoded
	const auto source_len = CFStringGetLength(source);

	// How much space is needed to decode to ASCII?
	const auto target_len = CFStringGetMaximumSizeForEncoding(source_len,
	                                                          encoding);

	// Prepare our target string, including trailing terminator
	std::string target(target_len, '\0');

	// Decode from the source into the target
	const auto extracted = CFStringGetCString(source,
	                                          target.data(),
	                                          target.length(),
	                                          encoding);

	if (!extracted)
		target.clear();

	return target;
}
#endif

#if defined(LINUX)

#include <fcntl.h>

int get_num_physical_cpus()
{
	constexpr int MaxCpus  = 128;
	bool core_ids[MaxCpus] = {};
	int num_cores          = 0;

	for (int i = 0; i < MaxCpus; ++i) {
		std::string path = "/sys/devices/system/cpu/cpu" +
		                   std::to_string(i) + "/topology/core_id";
		int fd = open(path.c_str(), O_RDONLY);

		if (fd != -1) {
			char core_id_str[6] = {};
			if (read(fd, core_id_str, 5) > 0) {
				int core_id = atoi(core_id_str);
				if (core_id >= 0 && core_id < MaxCpus &&
				    !core_ids[core_id]) {
					core_ids[core_id] = true;
					++num_cores;
				}
			}
			close(fd);
		}
	}

	return std::max(num_cores, 1);
}

#elif defined(MACOSX)

#include <sys/sysctl.h>

int get_num_physical_cpus()
{
	int num_cpus = 0;
	size_t size  = sizeof(num_cpus);
	sysctlbyname("hw.physicalcpu", &num_cpus, &size, nullptr, 0);
	return std::max(num_cpus, 1);
}

#elif defined(WIN32)

#include <Windows.h>

int get_num_physical_cpus()
{
	// Size of the buffer as input, gets modified on function call to size of return data
	DWORD buffer_size = 100 * 1024;
	uint8_t* cpu_info = (uint8_t*)malloc(buffer_size);
	if (!GetLogicalProcessorInformationEx(RelationProcessorCore,
	                                      (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)cpu_info,
	                                      &buffer_size)) {
		return 1;
	}

	// Return value is a variably sized struct and requires type-punning to read
	// Whoever made this API was probably on substances that are illegal in many countries
	DWORD bytes_read = 0;
	int num_entries  = 0;
	while (bytes_read < buffer_size) {
		PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX info =
		        (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)(cpu_info +
		                                                   bytes_read);
		bytes_read += info->Size;
		++num_entries;
	}
	free(cpu_info);

	// There should be one entry per core so all we care about is the number of entries we got back
	return std::max(num_entries, 1);
}

#else

#include <SDL.h>

int get_num_physical_cpus()
{
	// Some other OS, fall back to SDL
	// This will actually be logical CPUs but oh well
	return SDL_GetCPUCount();
}

#endif

// ***************************************************************************
// Local time support
// ***************************************************************************

namespace cross {

#if defined(WIN32)
struct tm *localtime_r(const time_t *timep, struct tm *result)
{
	const errno_t err = localtime_s(result, timep);
	return (err == 0 ? result : nullptr);
}
#endif

} // namespace cross
