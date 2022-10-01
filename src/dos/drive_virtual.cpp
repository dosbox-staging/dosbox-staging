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
#include <fstream>

#include "cross.h"
#include "dos_inc.h"
#include "shell.h"
#include "string_utils.h"
#include "support.h"

constexpr auto default_date = DOS_PackDate(2002, 10, 1);
constexpr auto default_time = DOS_PackTime(12, 34, 56);

struct Filename {
	std::string fullname = {};
	std::string shortname = {};
};
std::vector<Filename> vfilenames = {Filename{"", ""}};
constexpr int max_vfiles = 500;
unsigned int vfile_pos = 1;
uint16_t fztime = 0;
uint16_t fzdate = 0;
char sfn[DOS_NAMELENGTH_ASCII];
void Add_VFiles(const bool add_autoexec);
extern DOS_Shell *first_shell;

struct VFILE_Block {
	const char * name = nullptr;
	uint8_t * data = nullptr;
	uint32_t size = 0;
	uint16_t date = 0;
	uint16_t time = 0;
	unsigned int onpos = 0;
	bool isdir = false;
	VFILE_Block * next = nullptr;
};

static VFILE_Block *first_file = nullptr;
static VFILE_Block *parent_dir = nullptr;

char *VFILE_Generate_8x3(const char *name, const unsigned int onpos)
{
	if (!name || !*name) {
		reset_str(sfn);
		return sfn;
	}
	if (!filename_not_8x3(name)) {
		assert(strlen(name) < DOS_NAMELENGTH_ASCII);
		safe_strcpy(sfn, name);
		upcase(sfn);
		return sfn;
	}
	std::string lfn = name;
	if (lfn.length() >= LFN_NAMELENGTH)
		lfn.erase(LFN_NAMELENGTH);
	unsigned int num = 1;
	const VFILE_Block *cur_file;
	// Get 8.3 names for LFNs by iterating the numbers
	while (1) {
		const auto str = generate_8x3(lfn.c_str(), num);
		safe_strcpy(sfn, str.length() < DOS_NAMELENGTH_ASCII ? str.c_str() : "");
		if (!*sfn)
			return sfn;
		cur_file = first_file;
		bool found = false;
		while (cur_file) {
			// If 8.3 name already exists, try next number
			if (onpos == cur_file->onpos &&
			    (strcasecmp(sfn, cur_file->name) == 0)) {
				found = true;
				break;
			}
			cur_file = cur_file->next;
		}
		// Return if 8.3 name does not already exist
		if (!found)
			return sfn;
		num++;
	}
	reset_str(sfn);
	return sfn;
}

void VFILE_Register(const char *name,
                    const uint8_t *data,
                    const uint32_t size,
                    const char *dir)
{
	assert(name);
	if (vfile_pos >= max_vfiles)
		return;
	const auto isdir = !strcmp(dir, "/") || !strcmp(name, ".") ||
	                   !strcmp(name, "..");
	const auto len = strlen(dir);
	unsigned int onpos = 0;
	if (len > 2 && dir[0] == '/' && dir[len - 1] == '/') {
		for (unsigned int i = 1; i < vfile_pos; i++)
			if (!strcasecmp((vfilenames[i].shortname + "/").c_str(),
			                dir + 1) ||
			    !strcasecmp((vfilenames[i].fullname + "/").c_str(),
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
	Filename filename;
	filename.fullname = name;
	filename.shortname = filename_not_strict_8x3(name)
	                             ? VFILE_Generate_8x3(name, onpos)
	                             : name;
	trim(filename.fullname);
	trim(filename.shortname);
	vfilenames.push_back(filename);
	if (!vfilenames[vfile_pos].shortname.length() ||
	    !vfilenames[vfile_pos].fullname.length())
		return;
	VFILE_Block *new_file = new VFILE_Block;
	new_file->name = strdup(vfilenames[vfile_pos].shortname.c_str());
	vfile_pos++;
	new_file->data = data ? new (std::nothrow) uint8_t[size] : nullptr;
	if (new_file->data)
		memcpy(new_file->data, data, size);
	new_file->size = size;
	new_file->date = fztime || fzdate ? fzdate : default_date;
	new_file->time = fztime || fzdate ? fztime : default_time;
	new_file->onpos = onpos;
	new_file->isdir = isdir;
	new_file->next = first_file;
	first_file = new_file;
}

void VFILE_Register(const char *name, const std::vector<uint8_t> &blob, const char *dir)
{
	VFILE_Register(name, blob.data(), check_cast<uint32_t>(blob.size()), dir);
}

void VFILE_Remove(const char *name, const char *dir = "")
{
	assert(name);
	assert(dir);
	unsigned int onpos = 0;
	if (*dir) {
		for (unsigned int i = 1; i < vfile_pos; i++)
			if (!strcasecmp(vfilenames[i].shortname.c_str(), dir) ||
			    !strcasecmp(vfilenames[i].fullname.c_str(), dir)) {
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

void z_drive_getpath(std::string &path, const std::string &dirname)
{
	struct stat cstat;
	int result = stat(path.c_str(), &cstat);
	if (result == -1 || !(cstat.st_mode & S_IFDIR)) {
		path = GetExecutablePath().string();
		if (path.length()) {
			path += dirname;
			result = stat(path.c_str(), &cstat);
		}
		if (!path.length() || result == -1 || (cstat.st_mode & S_IFDIR) == 0) {
			path.clear();
			Cross::CreatePlatformConfigDir(path);
			path += dirname;
			result = stat(path.c_str(), &cstat);
			if (result == -1 || (cstat.st_mode & S_IFDIR) == 0)
				path.clear();
		}
	}
}

template <typename TP>
time_t to_time_t(TP tp)
{
	using namespace std::chrono;
	auto sctp = time_point_cast<system_clock::duration>(
	        tp - TP::clock::now() + system_clock::now());
	return system_clock::to_time_t(sctp);
}

void z_drive_register(const std::string &path, const std::string &dir)
{
	std::vector<std::string> names;
	const std_fs::path pathdir = path;
	if (path.length()) {
		for (const auto &entry : std_fs::directory_iterator(pathdir)) {
			const auto name = entry.path().filename();
			if (!entry.is_directory())
				names.emplace_back(name.string().c_str());
			else if (name.string() != "." && name.string() != "..")
				names.push_back((name.string() + "/").c_str());
		}
	}
	std_fs::path fullname;
	for (std::string name : names) {
		if (!name.length())
			continue;
		fullname = pathdir / name;
		if (!std_fs::exists(fullname)) {
			fullname = GetExecutablePath() / fullname;
			if (!std_fs::exists(fullname))
				continue;
		}
		fztime = fzdate = 0;
		const auto filetime = to_time_t(std_fs::last_write_time(fullname));
		if (const struct tm *ltime = localtime(&filetime); ltime != 0) {
			fztime = DOS_PackTime(*ltime);
			fzdate = DOS_PackDate(*ltime);
		}
		if (name.back() == '/' && dir == "/") {
			name.pop_back();
			VFILE_Register(name.c_str(), nullptr, 0, dir.c_str());
			fztime = fzdate = 0;
			z_drive_register((pathdir / name).string(), dir + name + "/");
			continue;
		}
		std::ifstream file(fullname, std::ios::in | std::ios::binary);
		if (file.is_open()) {
			const auto size = (uint32_t)std_fs::file_size(fullname);
			std::string content(size, '\0');
			file.read(content.data(), size);
			file.close();
			VFILE_Register(name.c_str(),
			               (uint8_t *)content.c_str(), size,
			               dir == "/" ? "" : dir.c_str());
		}
		fztime = fzdate = 0;
	}
}

class Virtual_File final : public DOS_File {
public:
	Virtual_File(uint8_t *in_data, uint32_t in_size);

	Virtual_File(const Virtual_File &) = delete; // prevent copying
	Virtual_File &operator=(const Virtual_File &) = delete; // prevent assignment

	bool Read(uint8_t * data,uint16_t * size);
	bool Write(uint8_t * data,uint16_t * size);
	bool Seek(uint32_t * pos,uint32_t type);
	bool Close();
	uint16_t GetInformation();

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
	date = default_date;
	time = default_time;
	open = true;
}

bool Virtual_File::Read(uint8_t * data,uint16_t * size) {
	uint32_t left=file_size-file_pos;
	if (left <= *size) {
		memcpy(data, &file_data[file_pos], left);
		*size = (uint16_t)left;
	} else {
		memcpy(data, &file_data[file_pos], *size);
	}
	file_pos += *size;
	return true;
}

bool Virtual_File::Write(uint8_t * /*data*/,uint16_t * /*size*/){
	/* Not really writable */
	return false;
}

bool Virtual_File::Seek(uint32_t * new_pos,uint32_t type){
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


uint16_t Virtual_File::GetInformation() {
	return 0x40;	// read-only drive
}

Virtual_Drive::Virtual_Drive() : search_file(nullptr)
{
	type = DosDriveType::Virtual;
	safe_strcpy(info, "");
	if (!parent_dir)
		parent_dir = new VFILE_Block;
}

bool Virtual_Drive::FileOpen(DOS_File * * file,char * name,uint32_t flags) {
	assert(name);
	if (*name == 0) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
/* Scan through the internal list of files */
	VFILE_Block * cur_file = first_file;
	while (cur_file) {
		unsigned int onpos = cur_file->onpos;
		if (strcasecmp(name, (std::string(onpos ? vfilenames[onpos].shortname +
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

bool Virtual_Drive::FileCreate(DOS_File * * /*file*/,char * /*name*/,uint16_t /*attributes*/) {
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
	assert(dir);
	if (!dir[0]) return true;		//only valid dir is the empty dir
	const VFILE_Block* cur_file = first_file;
	while (cur_file) {
		if (cur_file->isdir && !strcasecmp(cur_file->name, dir))
			return true;
		cur_file = cur_file->next;
	}
	return false;
}

bool Virtual_Drive::FileStat(const char* name, FileStat_Block * const stat_block){
	assert(name);
	VFILE_Block * cur_file = first_file;
	while (cur_file) {
		unsigned int onpos = cur_file->onpos;
		if (strcasecmp(name, (std::string(onpos ? vfilenames[onpos].shortname +
		                                                  std::string(1, '\\')
		                                        : "") +
		                      cur_file->name)
		                             .c_str()) == 0) {
			stat_block->attr = (int)(cur_file->isdir
			                                 ? DOS_ATTR_DIRECTORY
			                                 : DOS_ATTR_ARCHIVE);
			stat_block->size = cur_file->size;
			stat_block->date = default_date;
			stat_block->time = default_time;
			return true;
		}
		cur_file = cur_file->next;
	}
	return false;
}

bool Virtual_Drive::FileExists(const char* name){
	assert(name);
	VFILE_Block * cur_file = first_file;
	while (cur_file) {
		unsigned int onpos = cur_file->onpos;
		if (strcasecmp(name, (std::string(onpos ? vfilenames[onpos].shortname +
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
	assert(_dir);
	unsigned int onpos = 0;
	if (*_dir) {
		if (FileExists(_dir)) {
			DOS_SetError(DOSERR_FILE_NOT_FOUND);
			return false;
		}
		for (unsigned int i = 1; i < vfile_pos; i++) {
			if (!strcasecmp(vfilenames[i].shortname.c_str(), _dir) ||
			    !strcasecmp(vfilenames[i].fullname.c_str(), _dir)) {
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
	uint8_t attr;
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
			dta.SetResult(".", 0, default_date,
			              default_time, DOS_ATTR_DIRECTORY);
			return true;
		}
	}
	return FindNext(dta);
}

bool Virtual_Drive::FindNext(DOS_DTA &dta)
{
	uint8_t attr;
	char pattern[DOS_NAMELENGTH_ASCII];
	dta.GetSearchParams(attr, pattern);
	unsigned int pos = dta.GetDirID();
	if (search_file == parent_dir) {
		bool cmp = WildFileCmp("..", pattern);
		if (cmp)
			dta.SetResult("..", 0, default_date,
			              default_time, DOS_ATTR_DIRECTORY);
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

bool Virtual_Drive::GetFileAttr(char *name, uint16_t *attr)
{
	assert(name);
	if (*name == 0) {
		*attr = DOS_ATTR_DIRECTORY;
		return true;
	}
	VFILE_Block *cur_file = first_file;
	while (cur_file) {
		unsigned int onpos = cur_file->onpos;
		if (strcasecmp(name, (std::string(onpos ? vfilenames[onpos].shortname +
		                                                  std::string(1, '\\')
		                                        : "") +
		                      cur_file->name)
		                             .c_str()) == 0) {
			*attr = (int)(cur_file->isdir ? DOS_ATTR_DIRECTORY // Maybe
			                              : DOS_ATTR_ARCHIVE); // Read-only?
			return true;
		}
		cur_file = cur_file->next;
	}
	return false;
}

bool Virtual_Drive::SetFileAttr(const char *name, [[maybe_unused]] uint16_t attr)
{
	assert(name);
	if (*name == 0) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return true;
	}
	const VFILE_Block *cur_file = first_file;
	while (cur_file) {
		unsigned int onpos = cur_file->onpos;
		if (strcasecmp(name, (std::string(onpos ? vfilenames[onpos].shortname +
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

bool Virtual_Drive::Rename([[maybe_unused]] char * oldname, [[maybe_unused]] char * newname) {
	return false;
}

bool Virtual_Drive::AllocationInfo(uint16_t * _bytes_sector,uint8_t * _sectors_cluster,uint16_t * _total_clusters,uint16_t * _free_clusters) {
	*_bytes_sector = 512;
	*_sectors_cluster = 32;
	*_total_clusters = 32765; // total size is always 500 mb
	*_free_clusters = 0;      // nothing free here
	return true;
}

uint8_t Virtual_Drive::GetMediaByte() {
	return 0xF8;
}

bool Virtual_Drive::isRemote() {
	return false;
}

bool Virtual_Drive::isRemovable() {
	return false;
}

Bits Virtual_Drive::UnMount() {
	return 1;
}

char const* Virtual_Drive::GetLabel() {
	return "DOSBOX";
}

void Virtual_Drive::EmptyCache()
{
	while (first_file != nullptr) {
		VFILE_Block *n = first_file->next;
		delete first_file;
		first_file = n;
	}
	vfile_pos = 1;
	PROGRAMS_Destroy(nullptr);
	vfilenames = {Filename{"", ""}};
	Add_VFiles(first_shell != nullptr);
}
