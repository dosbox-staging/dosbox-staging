/*
 *  Copyright (C) 2002-2020  The DOSBox Team
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
#include <string>
#include <sys/stat.h>
#include <sys/types.h>

#if defined (_MSC_VER)						/* MS Visual C++ */
#include <direct.h>
#include <io.h>
#define LONGTYPE(a) a##i64
#define snprintf _snprintf
#define vsnprintf _vsnprintf
#else										/* LINUX / GCC */
#include <dirent.h>
#include <unistd.h>
#define LONGTYPE(a) a##LL
#endif

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

#if defined (WIN32)
#define ftruncate(blah,blah2) chsize(blah,blah2)
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

void CROSS_DetermineConfigPaths();

class Cross {
public:
	static void GetPlatformConfigDir(std::string& in);
	static void GetPlatformConfigName(std::string& in);
	static void CreatePlatformConfigDir(std::string& in);
	static void ResolveHomedir(std::string & temp_line);
	static void CreateDir(std::string const& temp);
	static bool IsPathAbsolute(std::string const& in);
};

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

FILE *fopen_wrap(const char *path, const char *mode);

#endif
