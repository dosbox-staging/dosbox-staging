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

#define MAX_VFILES 500
unsigned int vfpos = 1;

extern char sfn[DOS_NAMELENGTH_ASCII];
char vfnames[MAX_VFILES][CROSS_LEN], vfsnames[MAX_VFILES][DOS_NAMELENGTH_ASCII];

struct VFILE_Block {
	const char * name;
	Bit8u * data;
	Bit32u size;
	Bit16u date;
	Bit16u time;
	unsigned int onpos;
	bool isdir;
	VFILE_Block * next;
};

static VFILE_Block *first_file, *parent_dir = NULL;

char *VFILE_Generate_SFN(const char *name, unsigned int onpos)
{
	if (!filename_not_8x3(name)) {
		strcpy(sfn, name);
		upcase(sfn);
		return sfn;
	}
	char lfn[LFN_NAMELENGTH + 1];
	if (name == NULL || !*name)
		return NULL;
	if (strlen(name) > LFN_NAMELENGTH) {
		strncpy(lfn, name, LFN_NAMELENGTH);
		lfn[LFN_NAMELENGTH] = 0;
	} else
		strcpy(lfn, name);
	if (!strlen(lfn))
		return NULL;
	unsigned int k = 1, i, t = 10000;
	const VFILE_Block *cur_file;
	while (k < 10000) {
		GenerateSFN(lfn, k, i, t);
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
		k++;
	}
	return 0;
}

uint16_t fztime = 0, fzdate = 0;
void VFILE_Register(const char *name, Bit8u *data, Bit32u size, const char *dir)
{
	bool isdir = !strcmp(dir, "/") || !strcmp(name, ".") || !strcmp(name, "..");
	unsigned int onpos = 0;
	char fullname[CROSS_LEN], fullsname[CROSS_LEN];
	if (strlen(dir) > 2 && dir[0] == '/' && dir[strlen(dir) - 1] == '/') {
		for (unsigned int i = 1; i < vfpos; i++)
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
	                            ? VFILE_Generate_SFN(name, onpos)
	                            : name;
	strcpy(vfnames[vfpos], name);
	strcpy(vfsnames[vfpos], sname.c_str());
	if (!strlen(trim(vfnames[vfpos])) || !strlen(trim(vfsnames[vfpos])))
		return;
	VFILE_Block *new_file = new VFILE_Block;
	new_file->name = vfsnames[vfpos];
	vfpos++;
	new_file->data = data;
	new_file->size = size;
	new_file->date = fztime || fzdate ? fzdate : DOS_PackDate(2002, 10, 1);
	new_file->time = fztime || fzdate ? fztime : DOS_PackTime(12, 34, 56);
	new_file->onpos = onpos;
	new_file->isdir = isdir;
	new_file->next = first_file;
	first_file = new_file;
}

void VFILE_Remove(const char *name,const char *dir = "") {
	unsigned int onpos = 0;
	if (*dir) {
		for (unsigned int i = 1; i < vfpos; i++)
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

void get_drivez_path(std::string &path, std::string dirname)
{
	struct stat cstat;
	int res = stat(path.c_str(), &cstat);
	if (res == -1 || !(cstat.st_mode & S_IFDIR)) {
		path = GetExecutablePath().string();
		if (path.size()) {
			path += dirname;
			res = stat(path.c_str(), &cstat);
		}
		if (!path.size() || res == -1 || (cstat.st_mode & S_IFDIR) == 0) {
			path = "";
			Cross::CreatePlatformConfigDir(path);
			path += dirname;
			res = stat(path.c_str(), &cstat);
			if (res == -1 || (cstat.st_mode & S_IFDIR) == 0)
				path = "";
		}
	}
}

void drivez_register(std::string path, std::string dir)
{
	char exePath[CROSS_LEN];
	std::vector<std::string> names;
	if (path.size()) {
		const std_fs::path dir = path;
		for (const auto &entry : std_fs::directory_iterator(dir)) {
			const auto result = entry.path().filename();
			if (!entry.is_directory())
				names.emplace_back(result.string().c_str());
			else if (result.string() != "." && result.string() != "..")
				names.push_back((result.string() + "/").c_str());
		}
	}
	int res;
	long f_size;
	uint8_t *f_data;
	struct stat temp_stat;
	const struct tm *ltime;
	for (std::string name : names) {
		if (!name.size())
			continue;
		if (name.back() == '/' && dir == "/") {
			res = stat((path + CROSS_FILESPLIT + name).c_str(),
			           &temp_stat);
			if (res)
				res = stat((GetExecutablePath().string() +
				            path + CROSS_FILESPLIT + name)
				                   .c_str(),
				           &temp_stat);
			if (res == 0 &&
			    (ltime = localtime(&temp_stat.st_mtime)) != 0) {
				fztime = DOS_PackTime((uint16_t)ltime->tm_hour,
				                      (uint16_t)ltime->tm_min,
				                      (uint16_t)ltime->tm_sec);
				fzdate = DOS_PackDate((uint16_t)(ltime->tm_year + 1900),
				                      (uint16_t)(ltime->tm_mon + 1),
				                      (uint16_t)ltime->tm_mday);
			}
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
			res = fstat(fileno(f), &temp_stat);
			if (res == 0 &&
			    (ltime = localtime(&temp_stat.st_mtime)) != 0) {
				fztime = DOS_PackTime((uint16_t)ltime->tm_hour,
				                      (uint16_t)ltime->tm_min,
				                      (uint16_t)ltime->tm_sec);
				fzdate = DOS_PackDate((uint16_t)(ltime->tm_year + 1900),
				                      (uint16_t)(ltime->tm_mon + 1),
				                      (uint16_t)ltime->tm_mday);
			}
			fseek(f, 0, SEEK_END);
			f_size = ftell(f);
			f_data = (uint8_t *)malloc(f_size);
			fseek(f, 0, SEEK_SET);
			fread(f_data, sizeof(char), f_size, f);
			fclose(f);
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
			stat_block->attr = cur_file->isdir?DOS_ATTR_DIRECTORY:DOS_ATTR_ARCHIVE;
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
		for (unsigned int i = 1; i < vfpos; i++) {
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
			              search_file->isdir ? DOS_ATTR_DIRECTORY
			                                 : DOS_ATTR_ARCHIVE);
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
			*attr = cur_file->isdir ? DOS_ATTR_DIRECTORY
			                        : DOS_ATTR_ARCHIVE; // Maybe
			                                            // readonly ?
			return true;
		}
		cur_file=cur_file->next;
	}
	return false;
}

bool Virtual_Drive::SetFileAttr(const char *name, uint16_t attr)
{
	(void)attr; // UNUSED
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

void PROGRAMS_Destroy(Section*);
void Add_VFiles(bool add_autoexec);
extern DOS_Shell *first_shell;
void Virtual_Drive::EmptyCache(void) {
	while (first_file != NULL) {
		VFILE_Block *n = first_file->next;
		delete first_file;
		first_file = n;
	}
	vfpos=1;
	PROGRAMS_Destroy(NULL);
	Add_VFiles(first_shell);
}