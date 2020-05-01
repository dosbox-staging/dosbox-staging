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

#include "cross.h"

#include <string>
#include <vector>

#include <limits.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef WIN32
#ifndef _WIN32_IE
#define _WIN32_IE 0x0400
#endif
#include <shlobj.h>
#else
#include <libgen.h>
#endif

#if defined HAVE_PWD_H
#include <pwd.h>
#endif

#include "support.h"

static std::string GetConfigName()
{
	return "dosbox-" CONF_BRAND ".conf";
}

#ifndef WIN32

std::string cached_conf_path;

static std::string ResolveHome(std::string tilde_path)
{
	Cross::ResolveHomedir(tilde_path);
	return tilde_path;
}

#if defined(MACOSX)

static std::string DetermineConfigPath()
{
	const std::string conf_path = ResolveHome("~/Library/Preferences/DOSBox");
	mkdir(conf_path.c_str(), 0700);
	return conf_path;
}

#else

static bool CreateDirectories(const std::string &path)
{
	struct stat sb;
	if (stat(path.c_str(), &sb) == 0) {
		const bool is_dir = ((sb.st_mode & S_IFMT) == S_IFDIR);
		return is_dir;
	}

	std::vector<char> tmp(path.begin(), path.end());
	std::string dname = dirname(tmp.data());

	// Create parent directories recursively
	if (!CreateDirectories(dname))
		return false;

	return (mkdir(path.c_str(), 0700) == 0);
}

static bool PathExists(const std::string &path)
{
	return (access(path.c_str(), F_OK) == 0);
}

static std::string DetermineConfigPath()
{
	const char *xdg_conf_home = getenv("XDG_CONFIG_HOME");
	const std::string conf_home = xdg_conf_home ? xdg_conf_home : "~/.config";
	const std::string conf_path = ResolveHome(conf_home + "/dosbox");
	const std::string old_conf_path = ResolveHome("~/.dosbox");

	if (PathExists(conf_path + "/" + GetConfigName())) {
		return conf_path;
	}

	if (PathExists(old_conf_path + "/" + GetConfigName())) {
		LOG_MSG("WARNING: Config file found in deprecated path! (~/.dosbox)\n"
		        "Backup/remove this dir and restart to generate updated config file.\n"
		        "---");
		return old_conf_path;
	}

	if (!CreateDirectories(conf_path)) {
		LOG_MSG("ERROR: Directory '%s' cannot be created",
		        conf_path.c_str());
		return old_conf_path;
	}

	return conf_path;
}

#endif // !MACOSX

void CROSS_DetermineConfigPaths()
{
	if (cached_conf_path.empty())
		cached_conf_path = DetermineConfigPath();
}

#endif // !WIN32

#ifdef WIN32

void CROSS_DetermineConfigPaths() {}

static void W32_ConfDir(std::string& in,bool create) {
	int c = create?1:0;
	char result[MAX_PATH] = { 0 };
	BOOL r = SHGetSpecialFolderPath(NULL,result,CSIDL_LOCAL_APPDATA,c);
	if(!r || result[0] == 0) r = SHGetSpecialFolderPath(NULL,result,CSIDL_APPDATA,c);
	if(!r || result[0] == 0) {
		char const * windir = getenv("windir");
		if(!windir) windir = "c:\\windows";
		safe_strncpy(result,windir,MAX_PATH);
		char const* appdata = "\\Application Data";
		size_t len = strlen(result);
		if (len + strlen(appdata) < MAX_PATH)
			safe_strcat(result, appdata);
		if(create) mkdir(result);
	}
	in = result;
}
#endif

void Cross::GetPlatformConfigDir(std::string& in) {
#ifdef WIN32
	W32_ConfDir(in,false);
	in += "\\DOSBox";
#else
	assert(!cached_conf_path.empty());
	in = cached_conf_path;
#endif
	if (in.back() != CROSS_FILESPLIT)
		in += CROSS_FILESPLIT;
}

void Cross::GetPlatformConfigName(std::string &in)
{
	in = GetConfigName();
}

void Cross::CreatePlatformConfigDir(std::string &in)
{
#ifdef WIN32
	W32_ConfDir(in,true);
	in += "\\DOSBox";
	mkdir(in.c_str());
#else
	assert(!cached_conf_path.empty());
	in = cached_conf_path.c_str();
	mkdir(in.c_str(), 0700);
#endif
	if (in.back() != CROSS_FILESPLIT)
		in += CROSS_FILESPLIT;
}

void Cross::ResolveHomedir(std::string & temp_line) {
	if(!temp_line.size() || temp_line[0] != '~') return; //No ~

	if(temp_line.size() == 1 || temp_line[1] == CROSS_FILESPLIT) { //The ~ and ~/ variant
		char * home = getenv("HOME");
		if(home) temp_line.replace(0,1,std::string(home));
#if defined HAVE_SYS_TYPES_H && defined HAVE_PWD_H
	} else { // The ~username variant
		std::string::size_type namelen = temp_line.find(CROSS_FILESPLIT);
		if(namelen == std::string::npos) namelen = temp_line.size();
		std::string username = temp_line.substr(1,namelen - 1);
		struct passwd* pass = getpwnam(username.c_str());
		if(pass) temp_line.replace(0,namelen,pass->pw_dir); //namelen -1 +1(for the ~)
#endif // USERNAME lookup code
	}
}

void Cross::CreateDir(std::string const& in) {
#ifdef WIN32
	mkdir(in.c_str());
#else
	mkdir(in.c_str(),0700);
#endif
}

bool Cross::IsPathAbsolute(std::string const& in) {
	// Absolute paths
#if defined (WIN32)
	// drive letter
	if (in.size() > 2 && in[1] == ':' ) return true;
	// UNC path
	else if (in.size() > 2 && in[0]=='\\' && in[1]=='\\') return true;
#else
	if (in.size() > 1 && in[0] == '/' ) return true;
#endif
	return false;
}

#if defined (WIN32)

dir_information* open_directory(const char* dirname) {
	if (dirname == NULL) return NULL;

	size_t len = strlen(dirname);
	if (len == 0) return NULL;

	static dir_information dir;

	safe_strncpy(dir.base_path,dirname,MAX_PATH);

	if (dirname[len - 1] == '\\')
		safe_strcat(dir.base_path, "*.*");
	else
		safe_strcat(dir.base_path, "\\*.*");

	dir.handle = INVALID_HANDLE_VALUE;

	return (access(dirname,0) ? NULL : &dir);
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
	safe_strncpy(dir.base_path,dirname,CROSS_LEN);
	return dir.dir?&dir:NULL;
}

bool read_directory_first(dir_information* dirp, char* entry_name, bool& is_directory) {
	if (!dirp) return false;
	return read_directory_next(dirp,entry_name,is_directory);
}

bool read_directory_next(dir_information* dirp, char* entry_name, bool& is_directory) {
	if (!dirp) return false;
	struct dirent* dentry = readdir(dirp->dir);
	if (dentry==NULL) {
		return false;
	}

//	safe_strncpy(entry_name,dentry->d_name,(FILENAME_MAX<MAX_PATH)?FILENAME_MAX:MAX_PATH);	// [include stdio.h], maybe pathconf()
	safe_strncpy(entry_name,dentry->d_name,CROSS_LEN);

#ifdef DIRENT_HAS_D_TYPE
	if(dentry->d_type == DT_DIR) {
		is_directory = true;
		return true;
	} else if(dentry->d_type == DT_REG) {
		is_directory = false;
		return true;
	}
#endif

	//Maybe only for DT_UNKNOWN if DIRENT_HAD_D_TYPE..
	static char buffer[2 * CROSS_LEN + 1] = { 0 };
	static char split[2] = { CROSS_FILESPLIT , 0 };
	buffer[0] = 0;
	safe_strcpy(buffer, dirp->base_path);
	size_t buflen = strlen(buffer);
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

FILE *fopen_wrap(const char *path, const char *mode) {
#if defined(WIN32)
	;
#elif defined (MACOSX)
	;
#else  
#if defined (HAVE_REALPATH)
	char work[CROSS_LEN] = {0};
	strncpy(work,path,CROSS_LEN-1);
	char* last = strrchr(work,'/');
	
	if (last) {
		if (last != work) {
			*last = 0;
			//If this compare fails, then we are dealing with files in / 
			//Which is outside the scope, but test anyway. 
			//However as realpath only works for exising files. The testing is 
			//in that case not done against new files.
		}
		char* check = realpath(work,NULL);
		if (check) {
			if ( ( strlen(check) == 5 && strcmp(check,"/proc") == 0) || strncmp(check,"/proc/",6) == 0) {
//				LOG_MSG("lst hit %s blocking!",path);
				free(check);
				return NULL;
			}
			free(check);
		}
	}

#if 0
//Lightweight version, but then existing files can still be read, which is not ideal	
	if (strpbrk(mode,"aw+") != NULL) {
		LOG_MSG("pbrk ok");
		char* check = realpath(path,NULL);
		//Will be null if file doesn't exist.... ENOENT
		//TODO What about unlink /proc/self/mem and then create it ?
		//Should be safe for what we want..
		if (check) {
			if (strncmp(check,"/proc/",6) == 0) {
				free(check);
				return NULL;
			}
			free(check);
		}
	}
*/
#endif //0 

#endif //HAVE_REALPATH
#endif

	return fopen(path,mode);
}
