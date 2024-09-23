/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2021-2023  The DOSBox Staging Team
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

#ifndef DOSBOX_CROSS_H
#define DOSBOX_CROSS_H

#include "dosbox.h"

#include <cstdio>
#include <ctime>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <vector>

#if defined (_MSC_VER)						/* MS Visual C++ */
#include <direct.h>
#include <io.h>
#define LONGTYPE(a) a##i64
#else										/* LINUX / GCC */
#include <dirent.h>
#include <unistd.h>
#define LONGTYPE(a) a##LL
#endif

#include "std_filesystem.h"

#define CROSS_LEN 512						/* Maximum filename size */


#if defined (WIN32)
#define CROSS_FILENAME(blah) 
#define CROSS_FILESPLIT '\\'
#else
#define	CROSS_FILENAME(blah) strreplace(blah,'\\','/')
#define CROSS_FILESPLIT '/'
#endif

#define CROSS_NONE	0
#define CROSS_FILE	1
#define CROSS_DIR	2

#if defined (WIN32) && !defined (__MINGW32__)
#define ftruncate(blah,blah2) chsize(blah,blah2)
#endif

/* Large file support */
#if defined(_MSC_VER)
	// MSVC doesn't support the posix fstream functions,
	// typedef their equivalents
	#define cross_ftello _ftelli64
	#define cross_fseeko _fseeki64
	#define cross_off_t __int64
#else
	// All other platforms should have POSIX fstream 'o' support
	// Note: Meson automatically sets preprocessor defines for us

	// Check that off_t is 64 bits
	static_assert(sizeof(off_t) == sizeof(int64_t), "off_t not 64 bits");
	#define cross_ftello ftello
	#define cross_fseeko fseeko
	#define cross_off_t off_t
#endif

// fileno is a POSIX function (not mentioned in ISO/C++), which means it might
// be missing when when using C++11 with strict ANSI compatibility.
// New MSVC issues "deprecation" warning when fileno is used and recommends
// using (platform-specific) _fileno. On other platforms we can use fileno
// because it's either a POSIX-compliant system, or the function is available
// when compiling with GNU extensions.
#if defined (_MSC_VER)
#define cross_fileno(s) _fileno(s)
#else
#define cross_fileno(s) fileno(s)
#endif

namespace cross {

#if defined(WIN32)

struct tm *localtime_r(const time_t *timep, struct tm *result);

#else

constexpr auto localtime_r = ::localtime_r;

#endif

} // namespace cross

// Create or determine the location of the config directory (e.g., in portable
// mode, the config directory is the executable dir). Must be called before
// calling GetConfigDir().
void InitConfigDir();

std_fs::path GetConfigDir();
std::string GetPrimaryConfigName();
std_fs::path GetPrimaryConfigPath();

std_fs::path resolve_home(const std::string &str) noexcept;

#if defined (WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

typedef struct dir_struct {
	HANDLE          handle;
	char            base_path[MAX_PATH+4];
	WIN32_FIND_DATA search_data;
} dir_information;

#else

//#include <sys/types.h> //Included above
#include <dirent.h>

typedef struct dir_struct { 
	DIR*  dir;
	char base_path[CROSS_LEN];
} dir_information;

#endif

dir_information* open_directory(const char* dirname);
bool read_directory_first(dir_information* dirp, char* entry_name, bool& is_directory);
bool read_directory_next(dir_information* dirp, char* entry_name, bool& is_directory);
void close_directory(dir_information* dirp);

FILE *fopen_wrap_ro_fallback(const std::string &filename, bool &is_readonly);

bool wild_match(const char *haystack, const char *needle);
bool WildFileCmp(const char *file, const char *wild, bool long_compare = false);

bool get_expanded_files(const std::string &path,
                        std::vector<std::string> &files,
                        bool files_only,
                        bool skip_native_path = false) noexcept;

int get_num_physical_cpus();

#endif
