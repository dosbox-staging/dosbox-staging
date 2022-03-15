/*
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

#include "drives.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "dos_inc.h"
#include "support.h"
#include "shell.h"
#include "cross.h"

constexpr int max_vfiles = 500;
unsigned int vfile_pos = 1;
uint16_t fztime = 0;
uint16_t fzdate = 0;
char sfn[DOS_NAMELENGTH_ASCII];
char vfnames[max_vfiles][CROSS_LEN];
char vfsnames[max_vfiles][DOS_NAMELENGTH_ASCII];
void Add_VFiles(const bool add_autoexec);
extern DOS_Shell *first_shell;

struct VFILE_Block {
	const char * name;
	uint8_t * data;
	uint32_t size;
	uint16_t date;
	uint16_t time;
	unsigned int onpos;
	bool isdir;
	VFILE_Block * next;
};

static VFILE_Block *first_file, *parent_dir = NULL;

char *VFILE_Generate_8x3(const char *name, const unsigned int onpos)
{
	if (name == NULL || !*name) {
		strcpy(sfn, "");
		return sfn;
	}
	if (!filename_not_8x3(name)) {
		strcpy(sfn, name);
		upcase(sfn);
		return sfn;
	}
	char lfn[LFN_NAMELENGTH + 1];
	if (strlen(name) > LFN_NAMELENGTH) {
		strncpy(lfn, name, LFN_NAMELENGTH);
		lfn[LFN_NAMELENGTH] = 0;
	} else {
		strcpy(lfn, name);
	}
	if (!strlen(lfn)) {
		strcpy(sfn, "");
		return sfn;
	}
	unsigned int num = 1;
	const VFILE_Block *cur_file;
	while (1) {
		strcpy(sfn, generate_8x3(lfn, num).c_str());
		if (!*sfn)
			return sfn;
		cur_file = first_file;
		bool found = false;
		while (cur_file) {
			if (onpos == cur_file->onpos &&
			    (strcasecmp(sfn, cur_file->name) == 0)) {
				found = true;
				break;
			}
			cur_file = cur_file->next;
		}
		if (!found)
			return sfn;
		num++;
	}
	strcpy(sfn, "");
	return sfn;
}

void VFILE_Register(const char *name, uint8_t *data, const uint32_t size, const char *dir)
{
	if (vfile_pos >= max_vfiles)
		return;
	const auto isdir = !strcmp(dir, "/") || !strcmp(name, ".") || !strcmp(name, "..");
	const auto len = strlen(dir);
	unsigned int onpos = 0;
	if (len > 2 && dir[0] == '/' && dir[len - 1] == '/') {
		for (unsigned int i = 1; i < vfile_pos; i++)
			if (!strcasecmp((std::string(vfsnames[i]) + "/").c_str(),
			                dir + 1) ||
			    !strcasecmp((std::string(vfnames[i]) + "/").c_str(),
			                dir + 1)) {
				onpos = i;
				break;
			}
		if (onpos == 0)
			return;
	}
	const VFILE_Block *cur_file = first_file;
	while (cur_file) {
		if (onpos == cur_file->onpos && strcasecmp(name, cur_file->name) == 0)
			return;
		cur_file = cur_file->next;
	}
	std::string sname = filename_not_strict_8x3(name)
	                            ? VFILE_Generate_8x3(name, onpos)
	                            : name;
	strcpy(vfnames[vfile_pos], name);
	strcpy(vfsnames[vfile_pos], sname.c_str());
	if (!strlen(trim(vfnames[vfile_pos])) || !strlen(trim(vfsnames[vfile_pos])))
		return;
	VFILE_Block *new_file = new VFILE_Block;
	new_file->name = vfsnames[vfile_pos];
	vfile_pos++;
	new_file->data = data;
	new_file->size = size;
	new_file->date = fztime || fzdate ? fzdate : DOS_PackDate(2002, 10, 1);
	new_file->time = fztime || fzdate ? fztime : DOS_PackTime(12, 34, 56);
	new_file->onpos = onpos;
	new_file->isdir = isdir;
	new_file->next = first_file;
	first_file = new_file;
}

void VFILE_Remove(const char *name, const char *dir = "") {
	unsigned int onpos = 0;
	if (*dir) {
		for (unsigned int i = 1; i < vfile_pos; i++)
			if (!strcasecmp(vfsnames[i], dir) ||
			    !strcasecmp(vfnames[i], dir)) {
				onpos = i;
				break;
			}
		if (onpos == 0)
			return;
	}
	VFILE_Block * chan = first_file;
	VFILE_Block * * where = &first_file;
	while (chan) {
		if (onpos == chan->onpos && strcmp(name, chan->name) == 0) {
			*where = chan->next;
			if (chan == first_file)
				first_file = chan->next;
			delete chan;
			return;
		}
		where = &chan->next;
		chan = chan->next;
	}
}

void get_drivez_path(std::string &path, const std::string &dirname)
{
	struct stat cstat;
	int result = stat(path.c_str(), &cstat);
	if (result == -1 || !(cstat.st_mode & S_IFDIR)) {
		path = GetExecutablePath().string();
		if (path.size()) {
			path += dirname;
			result = stat(path.c_str(), &cstat);
		}
		if (!path.size() || result == -1 || (cstat.st_mode & S_IFDIR) == 0) {
			path.clear();
			Cross::CreatePlatformConfigDir(path);
			path += dirname;
			result = stat(path.c_str(), &cstat);
			if (result == -1 || (cstat.st_mode & S_IFDIR) == 0)
				path.clear();
		}
	}
}

void get_datetime(const int result, time_t mtime, uint16_t &time, uint16_t &date)
{
	const struct tm *ltime;
	if (result == 0 && (ltime = localtime(&mtime)) != 0) {
		time = DOS_PackTime((uint16_t)ltime->tm_hour,
		                    (uint16_t)ltime->tm_min,
		                    (uint16_t)ltime->tm_sec);
		date = DOS_PackDate((uint16_t)(ltime->tm_year + 1900),
		                    (uint16_t)(ltime->tm_mon + 1),
		                    (uint16_t)ltime->tm_mday);
	}
}

void drivez_register(const std::string &path, const std::string &dir)
{
	char exePath[CROSS_LEN];
	std::vector<std::string> names;
	if (path.size()) {
		const std_fs::path pathdir = path;
		for (const auto &entry : std_fs::directory_iterator(pathdir)) {
			const auto result = entry.path().filename();
			if (!entry.is_directory())
				names.emplace_back(result.string().c_str());
			else if (result.string() != "." && result.string() != "..")
				names.push_back((result.string() + "/").c_str());
		}
	}
	int result;
	long f_size;
	uint8_t *f_data;
	struct stat temp_stat;
	for (std::string name : names) {
		if (!name.size())
			continue;
		if (name.back() == '/' && dir == "/") {
			result = stat((path + CROSS_FILESPLIT + name).c_str(),
			           &temp_stat);
			if (result)
				result = stat((GetExecutablePath().string() +
				               path + CROSS_FILESPLIT + name)
				                      .c_str(),
				              &temp_stat);
			get_datetime(result, temp_stat.st_mtime, fztime, fzdate);
			VFILE_Register(name.substr(0, name.size() - 1).c_str(),
			               0, 0, dir.c_str());
			fztime = fzdate = 0;
			drivez_register(path + CROSS_FILESPLIT +
			                        name.substr(0, name.size() - 1),
			                dir + name);
			continue;
		}
		FILE *f = fopen((path + CROSS_FILESPLIT + name).c_str(), "rb");
		if (f == NULL) {
			strcpy(exePath, GetExecutablePath().string().c_str());
			strcat(exePath, (path + CROSS_FILESPLIT + name).c_str());
			f = fopen(exePath, "rb");
		}
		f_size = 0;
		f_data = NULL;
		if (f != NULL) {
			result = fstat(fileno(f), &temp_stat);
			get_datetime(result, temp_stat.st_mtime, fztime, fzdate);
			fseek(f, 0, SEEK_END);
			f_size = ftell(f);
			f_data = (uint8_t *)malloc(f_size);
			if (f_data) {
				fseek(f, 0, SEEK_SET);
				fread(f_data, sizeof(char), f_size, f);
				fclose(f);
			}
		}
		if (f_data)
			VFILE_Register(name.c_str(), f_data, f_size,
			               dir == "/" ? "" : dir.c_str());
		fztime = fzdate = 0;
	}
}

class Virtual_File final : public DOS_File {
public:
	Virtual_File(uint8_t *in_data, uint32_t in_size);

	Virtual_File(const Virtual_File &) = delete; // prevent copying
	Virtual_File &operator=(const Virtual_File &) = delete; // prevent assignment

	bool Read(Bit8u * data,Bit16u * size);
	bool Write(Bit8u * data,Bit16u * size);
	bool Seek(Bit32u * pos,Bit32u type);
	bool Close();
	Bit16u GetInformation(void);

private:
	uint32_t file_size;
	uint32_t file_pos;
	uint8_t *file_data;
};

Virtual_File::Virtual_File(uint8_t *in_data, uint32_t in_size)
        : file_size(in_size),
          file_pos(0),
          file_data(in_data)
{
	date = DOS_PackDate(2002,10,1);
	time = DOS_PackTime(12,34,56);
	open = true;
}

bool Virtual_File::Read(Bit8u * data,Bit16u * size) {
	Bit32u left=file_size-file_pos;
	if (left <= *size) {
		memcpy(data, &file_data[file_pos], left);
		*size = (Bit16u)left;
	} else {
		memcpy(data, &file_data[file_pos], *size);
	}
	file_pos += *size;
	return true;
}

bool Virtual_File::Write(Bit8u * /*data*/,Bit16u * /*size*/){
	/* Not really writable */
	return false;
}

bool Virtual_File::Seek(Bit32u * new_pos,Bit32u type){
	switch (type) {
	case DOS_SEEK_SET:
		if (*new_pos <= file_size) file_pos = *new_pos;
		else return false;
		break;
	case DOS_SEEK_CUR:
		if ((*new_pos + file_pos) <= file_size) file_pos = *new_pos + file_pos;
		else return false;
		break;
	case DOS_SEEK_END:
		if (*new_pos <= file_size) file_pos = file_size - *new_pos;
		else return false;
		break;
	}
	*new_pos = file_pos;
	return true;
}

bool Virtual_File::Close(){
	return true;
}


Bit16u Virtual_File::GetInformation(void) {
	return 0x40;	// read-only drive
}

Virtual_Drive::Virtual_Drive() : search_file(nullptr)
{
	strcpy(info,"Internal Virtual Drive");
	if (parent_dir == NULL) parent_dir = new VFILE_Block;
}

bool Virtual_Drive::FileOpen(DOS_File * * file,char * name,Bit32u flags) {
	if (*name == 0) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
/* Scan through the internal list of files */
	VFILE_Block * cur_file = first_file;
	while (cur_file) {
		unsigned int onpos = cur_file->onpos;
		if (strcasecmp(name, (std::string(onpos ? vfsnames[onpos] +
		                                                  std::string(1, '\\')
		                                        : "") +
		                      cur_file->name)
		                             .c_str()) == 0) {
			/* We have a match */
			*file = new Virtual_File(cur_file->data, cur_file->size);
			(*file)->flags = flags;
			return true;
		}
		cur_file = cur_file->next;
	}
	return false;
}

bool Virtual_Drive::FileCreate(DOS_File * * /*file*/,char * /*name*/,Bit16u /*attributes*/) {
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool Virtual_Drive::FileUnlink(char * /*name*/) {
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool Virtual_Drive::RemoveDir(char * /*dir*/) {
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool Virtual_Drive::MakeDir(char * /*dir*/) {
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool Virtual_Drive::TestDir(char * dir) {
	if (!dir[0]) return true;		//only valid dir is the empty dir
	const VFILE_Block* cur_file = first_file;
	while (cur_file) {
		if (cur_file->isdir&&(!strcasecmp(cur_file->name, dir))) return true;
		cur_file=cur_file->next;
	}
	return false;
}

bool Virtual_Drive::FileStat(const char* name, FileStat_Block * const stat_block){
	VFILE_Block * cur_file = first_file;
	while (cur_file) {
		unsigned int onpos = cur_file->onpos;
		if (strcasecmp(name, (std::string(onpos ? vfsnames[onpos] +
		                                                  std::string(1, '\\')
		                                        : "") +
		                      cur_file->name)
		                             .c_str()) == 0) {
			stat_block->attr = (int)(cur_file->isdir
			                                 ? DOS_ATTR_DIRECTORY
			                                 : DOS_ATTR_ARCHIVE);
			stat_block->size = cur_file->size;
			stat_block->date = DOS_PackDate(2002,10,1);
			stat_block->time = DOS_PackTime(12,34,56);
			return true;
		}
		cur_file = cur_file->next;
	}
	return false;
}

bool Virtual_Drive::FileExists(const char* name){
	VFILE_Block * cur_file = first_file;
	while (cur_file) {
		unsigned int onpos = cur_file->onpos;
		if (strcasecmp(name, (std::string(onpos ? vfsnames[onpos] +
		                                                  std::string(1, '\\')
		                                        : "") +
		                      cur_file->name)
		                             .c_str()) == 0)
			return !cur_file->isdir;
		cur_file = cur_file->next;
	}
	return false;
}

bool Virtual_Drive::FindFirst(char *_dir, DOS_DTA &dta, bool fcb_findfirst)
{
	unsigned int onpos = 0;
	if (*_dir) {
		if (FileExists(_dir)) {
			DOS_SetError(DOSERR_FILE_NOT_FOUND);
			return false;
		}
		for (unsigned int i = 1; i < vfile_pos; i++) {
			if (!strcasecmp(vfsnames[i], _dir) ||
			    !strcasecmp(vfnames[i], _dir)) {
				onpos = i;
				break;
			}
		}
		if (!onpos) {
			DOS_SetError(DOSERR_PATH_NOT_FOUND);
			return false;
		}
	}
	dta.SetDirID(onpos);
	Bit8u attr;
	char pattern[DOS_NAMELENGTH_ASCII];
	dta.GetSearchParams(attr, pattern);
	search_file = (attr & DOS_ATTR_DIRECTORY) && onpos > 0 ? parent_dir
	                                                       : first_file;
	if (attr == DOS_ATTR_VOLUME) {
		dta.SetResult(GetLabel(), 0, 0, 0, DOS_ATTR_VOLUME);
		return true;
	} else if ((attr & DOS_ATTR_VOLUME) && !fcb_findfirst) {
		if (WildFileCmp(GetLabel(), pattern)) {
			dta.SetResult(GetLabel(), 0, 0, 0, DOS_ATTR_VOLUME);
			return true;
		}
	} else if ((attr & DOS_ATTR_DIRECTORY) && onpos > 0) {
		if (WildFileCmp(".", pattern)) {
			dta.SetResult(".", 0, DOS_PackDate(2002, 10, 1),
			              DOS_PackTime(12, 34, 56), DOS_ATTR_DIRECTORY);
			return true;
		}
	}
	return FindNext(dta);
}

bool Virtual_Drive::FindNext(DOS_DTA &dta)
{
	Bit8u attr;
	char pattern[DOS_NAMELENGTH_ASCII];
	dta.GetSearchParams(attr, pattern);
	unsigned int pos = dta.GetDirID();
	if (search_file == parent_dir) {
		bool cmp = WildFileCmp("..", pattern);
		if (cmp)
			dta.SetResult("..", 0, DOS_PackDate(2002, 10, 1),
			              DOS_PackTime(12, 34, 56), DOS_ATTR_DIRECTORY);
		search_file = first_file;
		if (cmp)
			return true;
	}
	while (search_file) {
		if (pos == search_file->onpos &&
		    ((attr & DOS_ATTR_DIRECTORY) || !search_file->isdir) &&
		    WildFileCmp(search_file->name, pattern)) {
			dta.SetResult(search_file->name, search_file->size,
			              search_file->date, search_file->time,
			              (int)(search_file->isdir ? DOS_ATTR_DIRECTORY
			                                       : DOS_ATTR_ARCHIVE));
			search_file = search_file->next;
			return true;
		}
		search_file = search_file->next;
	}
	DOS_SetError(DOSERR_NO_MORE_FILES);
	return false;
}

bool Virtual_Drive::GetFileAttr(char *name, Bit16u *attr)
{
	if (*name == 0) {
		*attr=DOS_ATTR_DIRECTORY;
		return true;
	}
	VFILE_Block *cur_file = first_file;
	while (cur_file) {
		unsigned int onpos = cur_file->onpos;
		if (strcasecmp(name, (std::string(onpos ? vfsnames[onpos] +
		                                                  std::string(1, '\\')
		                                        : "") +
		                      cur_file->name)
		                             .c_str()) == 0) {
			*attr = (int)(cur_file->isdir ? DOS_ATTR_DIRECTORY // Maybe
			                              : DOS_ATTR_ARCHIVE); // Read-only?
			return true;
		}
		cur_file=cur_file->next;
	}
	return false;
}

bool Virtual_Drive::SetFileAttr(const char *name, [[maybe_unused]] uint16_t attr)
{
	if (*name == 0) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return true;
	}
	const VFILE_Block *cur_file = first_file;
	while (cur_file) {
		unsigned int onpos = cur_file->onpos;
		if (strcasecmp(name, (std::string(onpos ? vfsnames[onpos] +
		                                                  std::string(1, '\\')
		                                        : "") +
		                      cur_file->name)
		                             .c_str()) == 0) {
			DOS_SetError(DOSERR_ACCESS_DENIED);
			return false;
		}
		cur_file = cur_file->next;
	}
	DOS_SetError(DOSERR_FILE_NOT_FOUND);
	return false;
}

bool Virtual_Drive::Rename(char * /*oldname*/,char * /*newname*/) {
	return false;
}

bool Virtual_Drive::AllocationInfo(Bit16u * _bytes_sector,Bit8u * _sectors_cluster,Bit16u * _total_clusters,Bit16u * _free_clusters) {
	*_bytes_sector = 512;
	*_sectors_cluster = 32;
	*_total_clusters = 32765; // total size is always 500 mb
	*_free_clusters = 0;      // nothing free here
	return true;
}

Bit8u Virtual_Drive::GetMediaByte(void) {
	return 0xF8;
}

bool Virtual_Drive::isRemote(void) {
	return false;
}

bool Virtual_Drive::isRemovable(void) {
	return false;
}

Bits Virtual_Drive::UnMount(void) {
	return 1;
}

char const* Virtual_Drive::GetLabel(void) {
	return "DOSBOX";
}

void Virtual_Drive::EmptyCache(void)
{
	while (first_file != NULL) {
		VFILE_Block *n = first_file->next;
		delete first_file;
		first_file = n;
	}
	vfile_pos = 1;
	PROGRAMS_Destroy(NULL);
	Add_VFiles(first_shell != NULL);
}
