/*
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

#include "drives.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "cross.h"
#include "dos_inc.h"
#include "fs_utils.h"
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

class VFILE_Block;
using vfile_block_t = std::shared_ptr<VFILE_Block>;
using vfile_data_t  = std::shared_ptr<std::vector<uint8_t>>;

class VFILE_Block final {
public:
	std::string name = {};

	vfile_data_t data = nullptr;

	uint16_t date      = 0;
	uint16_t time      = 0;
	unsigned int position = 0;

	bool isdir = false;

	vfile_block_t next = {};

	~VFILE_Block() = default;
};

static vfile_block_t first_file = {};
static vfile_block_t parent_dir = {};

// this gets replaced with std::find_if later
template <typename Predicate>
vfile_block_t find_vfile_by_predicate(vfile_block_t head_file, Predicate predicate_)
{
	auto cur_file = head_file;
	while (cur_file) {
		if (predicate_(cur_file)) {
			return cur_file;
		}
		cur_file = cur_file->next;
	}
	return {};
}

vfile_block_t find_vfile_by_name_and_pos(const std::string& name, unsigned int position)
{
	return find_vfile_by_predicate(first_file, [name, position](vfile_block_t vfile) {
		return position == vfile->position && iequals(name, vfile->name);
	});
}

vfile_block_t find_vfile_by_name_and_dir(const char* name, const char* dir)
{
	assert(name);
	assert(dir);
	unsigned int position = 0;
	if (*dir) {
		for (unsigned int i = 1; i < vfile_pos; i++) {
			if (!strcasecmp(vfilenames[i].shortname.c_str(), dir) ||
			    !strcasecmp(vfilenames[i].fullname.c_str(), dir)) {
				position = i;
				break;
			}
		}
		if (position == 0) {
			return {};
		}
	}
	return find_vfile_by_name_and_pos(name, position);
}

bool vfile_name_and_pos_exists(const std::string& name, unsigned int position)
{
	return find_vfile_by_name_and_pos(name, position).get();
}

char* VFILE_Generate_8x3(const char* name, const unsigned int position)
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
	// Get 8.3 names for LFNs by iterating the numbers
	while (1) {
		const auto str = generate_8x3(lfn.c_str(), num);
		safe_strcpy(sfn, str.length() < DOS_NAMELENGTH_ASCII ? str.c_str() : "");
		if (!*sfn) {
			return sfn;
		}

		// Return if 8.3 name does not already exist
		if (!vfile_name_and_pos_exists(sfn, position)) {
			return sfn;
		}
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
	unsigned int position = 0;
	if (len > 2 && dir[0] == '/' && dir[len - 1] == '/') {
		for (unsigned int i = 1; i < vfile_pos; i++)
			if (!strcasecmp((vfilenames[i].shortname + "/").c_str(),
			                dir + 1) ||
			    !strcasecmp((vfilenames[i].fullname + "/").c_str(),
			                dir + 1)) {
				position = i;
				break;
			}
		if (position == 0)
			return;
	}

	if (vfile_name_and_pos_exists(name, position)) {
		return;
	}
	Filename filename;
	filename.fullname = name;
	filename.shortname = filename_not_strict_8x3(name)
	                           ? VFILE_Generate_8x3(name, position)
	                           : name;
	trim(filename.fullname);
	trim(filename.shortname);
	vfilenames.push_back(filename);
	if (!vfilenames[vfile_pos].shortname.length() ||
	    !vfilenames[vfile_pos].fullname.length())
		return;
	auto new_file  = std::make_shared<VFILE_Block>();
	new_file->name = vfilenames[vfile_pos].shortname;
	vfile_pos++;
	if (data && size > 0) {
		new_file->data = std::make_shared<std::vector<uint8_t>>(data, data + size);
	} else {
		new_file->data = std::make_shared<std::vector<uint8_t>>();
	}
	new_file->date     = fztime || fzdate ? fzdate : default_date;
	new_file->time     = fztime || fzdate ? fztime : default_time;
	new_file->position = position;
	new_file->isdir    = isdir;
	new_file->next = first_file;
	first_file = new_file;
}

void VFILE_Register(const char *name, const std::vector<uint8_t> &blob, const char *dir)
{
	VFILE_Register(name, blob.data(), check_cast<uint32_t>(blob.size()), dir);
}

void VFILE_Update(const char* name, std::vector<uint8_t> blob, const char* dir)
{
	auto vfile = find_vfile_by_name_and_dir(name, dir);

	if (vfile) {
		vfile->data = std::make_shared<std::vector<uint8_t>>(std::move(blob));
	}
}

void VFILE_Remove(const char* name, const char* dir)
{
	auto vfile = find_vfile_by_name_and_dir(name, dir);

	if (vfile) {
		if (vfile.get() == first_file.get()) {
			first_file = vfile->next;
		}
		// Finally release the vfile itself
		vfile.reset();

		return;
	}
}

void VFILE_GetPathZDrive(std::string &path, const std::string &dirname)
{
	struct stat cstat;
	int result = stat(path.c_str(), &cstat);
	if (result == -1 || !(cstat.st_mode & S_IFDIR)) {
		path = get_executable_path().string();
		if (path.length()) {
			path += dirname;
			result = stat(path.c_str(), &cstat);
		}
		if (!path.length() || result == -1 || (cstat.st_mode & S_IFDIR) == 0) {
			path.clear();
			path = (GetConfigDir() / dirname).string();
			result = stat(path.c_str(), &cstat);
			if (result == -1 || (cstat.st_mode & S_IFDIR) == 0)
				path.clear();
		}
	}
}

void VFILE_RegisterZDrive(const std_fs::path &z_drive_path)
{
	// How many levels deep should we register Z: entries? It seems the Z:
	// virtual drive can handle one level.
	constexpr auto max_depth = 1;

	// Keep recursing past permission issues and follow symlinks
	constexpr auto idir_opts = std_fs::directory_options::skip_permission_denied |
	                           std_fs::directory_options::follow_directory_symlink;

	// DOSBox's virtual-file system uses the forward slash as magic
	// indicator when deciding if entries are files or directories.
	constexpr auto dir_indicator = "/";

	// Check if the provided path is invalid
	if (z_drive_path.empty() || !std_fs::is_directory(z_drive_path))
		return;

	std::error_code ec = {};
	using idir = std_fs::recursive_directory_iterator;
	for (auto it = idir(z_drive_path, idir_opts, ec); it != idir(); ++it) {
		if (ec)
			break; // stop itterating if it had a problem

		// Get state of the entry
		const auto is_dir  = it->is_directory(ec);
		const auto is_file = it->is_regular_file(ec);

		// Only proceed if either depth is acceptable.
		const auto dir_depth_ok  = is_dir && it.depth() < max_depth;
		const auto file_depth_ok = is_file && it.depth() <= max_depth;
		if (!dir_depth_ok && !file_depth_ok)
			continue;

		// Get the entry's name without parent directories.
		const auto relative = it->path().lexically_relative(z_drive_path);
		const auto name = relative.filename().string();

		// Get the entry's parent(s) without the name. DOSBox's vfile
		// system expects directories in the root need a "/" parent,
		// where as files in the root need an empty parent.
		auto parent = relative.parent_path().string();
		if (!parent.empty()) {
			parent.insert(0, dir_indicator);
			parent.append(dir_indicator);
		} else if (is_dir) {
			parent = dir_indicator;
		}
		// Load the file's data, if it's a file.
		const auto blob = is_file ? load_resource_blob(it->path(), ResourceImportance::Optional)
		                          : std::vector<uint8_t>();

		// Set global time values for the entry about to be registered
		const auto rawtime  = to_time_t(it->last_write_time(ec));
		const auto timeinfo = localtime(&rawtime);
		fztime = timeinfo ? DOS_PackTime(*timeinfo) : 0;
		fzdate = timeinfo ? DOS_PackDate(*timeinfo) : 0;

		// Register the entry's name, data, and parent
		VFILE_Register(name.c_str(), blob, parent.c_str());
	}
}

class Virtual_File final : public DOS_File {
public:
	Virtual_File(const vfile_data_t& in_data);

	Virtual_File(const Virtual_File &) = delete; // prevent copying
	Virtual_File &operator=(const Virtual_File &) = delete; // prevent assignment

	bool Read(uint8_t* data, uint16_t* size) override;
	bool Write(uint8_t* data, uint16_t* size) override;
	bool Seek(uint32_t* pos, uint32_t type) override;
	void Close() override;
	uint16_t GetInformation() override;
	bool IsOnReadOnlyMedium() const override;

private:
	const vfile_data_t file_data;
	uint32_t file_pos;
};

Virtual_File::Virtual_File(const vfile_data_t& in_data)
        : file_data(in_data),
          file_pos(0)
{
	date = default_date;
	time = default_time;
}

bool Virtual_File::Read(uint8_t* data, uint16_t* bytes_requested)
{
	if (file_pos > file_data->size()) {
		// File has been read beyond the end and is in an invalid state,
		// so inform the caller with a negative rvalue.
		*bytes_requested = 0;
		return false;
	}

	const auto bytes_remaining = file_data->size() - file_pos;
	const auto bytes_to_read = std::min(static_cast<size_t>(bytes_remaining),
	                                    static_cast<size_t>(*bytes_requested));

	const uint8_t* src = file_data->data() + file_pos;
	memcpy(data, src, bytes_to_read);

	file_pos += bytes_to_read;
	*bytes_requested = check_cast<uint16_t>(bytes_to_read);
	return true;
}

bool Virtual_File::Write(uint8_t* /*data*/, uint16_t* /*size*/)
{
	/* Not really writable */
	return false;
}

bool Virtual_File::Seek(uint32_t* new_pos, uint32_t type)
{
	switch (type) {
	case DOS_SEEK_SET:
		if (*new_pos <= file_data->size()) {
			file_pos = *new_pos;
		} else {
			return false;
		}
		break;
	case DOS_SEEK_CUR:
		if ((*new_pos + file_pos) <= file_data->size()) {
			file_pos = *new_pos + file_pos;
		} else {
			return false;
		}
		break;
	case DOS_SEEK_END:
		if (*new_pos <= file_data->size()) {
			file_pos = file_data->size() - *new_pos;
		} else {
			return false;
		}
		break;
	}
	*new_pos = file_pos;
	return true;
}

void Virtual_File::Close()
{
}

uint16_t Virtual_File::GetInformation() {
	return 0x40;	// read-only drive
}

bool Virtual_File::IsOnReadOnlyMedium() const
{
	return true;
}

Virtual_Drive::Virtual_Drive() : search_file()
{
	type = DosDriveType::Virtual;
	safe_strcpy(info, "");
	if (!parent_dir)
		parent_dir = std::make_shared<VFILE_Block>();
}

std::unique_ptr<DOS_File> Virtual_Drive::FileOpen(const char* name, uint8_t flags)
{
	assert(name);
	if (*name == 0) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return nullptr;
	}
	/* Scan through the internal list of files */
	auto vfile = find_vfile_by_name(name);
	if (vfile) {
		/* We have a match */
		auto file   = std::make_unique<Virtual_File>(vfile->data);
		file->flags = flags;
		return file;
	}
	return nullptr;
}

std::unique_ptr<DOS_File> Virtual_Drive::FileCreate(const char* /*name*/,
                                                    FatAttributeFlags /*attributes*/)
{
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return nullptr;
}

bool Virtual_Drive::FileUnlink(const char * /*name*/) {
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool Virtual_Drive::RemoveDir(const char * /*dir*/) {
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool Virtual_Drive::MakeDir(const char * /*dir*/) {
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool Virtual_Drive::TestDir(const char * dir) {
	assert(dir);
	if (!dir[0]) return true;		//only valid dir is the empty dir

	return find_vfile_dir_by_name(dir).get();
}

bool Virtual_Drive::FileExists(const char* name){
	assert(name);
	auto vfile = find_vfile_by_name(name);
	if (vfile) {
		return !vfile->isdir;
	}
	return false;
}

bool Virtual_Drive::FindFirst(const char *_dir, DOS_DTA &dta, bool fcb_findfirst)
{
	assert(_dir);
	unsigned int position = 0;
	if (*_dir) {
		if (FileExists(_dir)) {
			DOS_SetError(DOSERR_FILE_NOT_FOUND);
			return false;
		}
		for (unsigned int i = 1; i < vfile_pos; i++) {
			if (iequals(vfilenames[i].shortname, _dir) ||
			    iequals(vfilenames[i].fullname, _dir)) {
				position = i;
				break;
			}
		}
		if (!position) {
			DOS_SetError(DOSERR_PATH_NOT_FOUND);
			return false;
		}
	}
	dta.SetDirID(position);

	FatAttributeFlags attr = {};
	char pattern[DOS_NAMELENGTH_ASCII];
	dta.GetSearchParams(attr, pattern);
	search_file = attr.directory && position > 0 ? parent_dir : first_file;
	if (attr == FatAttributeFlags::Volume) {
		dta.SetResult(GetLabel(), 0, 0, 0, FatAttributeFlags::Volume);
		return true;
	} else if (attr.volume && !fcb_findfirst) {
		if (WildFileCmp(GetLabel(), pattern)) {
			dta.SetResult(GetLabel(), 0, 0, 0, FatAttributeFlags::Volume);
			return true;
		}
	} else if (attr.directory && position > 0) {
		if (WildFileCmp(".", pattern)) {
			dta.SetResult(".",
			              0,
			              default_date,
			              default_time,
			              FatAttributeFlags::Directory);
			return true;
		}
	}
	return FindNext(dta);
}

vfile_block_t find_vfile_by_atribute_pattern_and_pos(vfile_block_t head_file,
                                                     FatAttributeFlags attr,
                                                     const char* pattern,
                                                     unsigned int pos)
{
	return find_vfile_by_predicate(head_file, [pos, attr, pattern](vfile_block_t vfile) {
		return pos == vfile->position && (attr.directory || !vfile->isdir) &&
		       WildFileCmp(vfile->name.c_str(), pattern);
	});
}

bool Virtual_Drive::FindNext(DOS_DTA& dta)
{
	FatAttributeFlags attr = {};
	char pattern[DOS_NAMELENGTH_ASCII];
	dta.GetSearchParams(attr, pattern);
	unsigned int pos = dta.GetDirID();
	if (search_file == parent_dir) {
		bool cmp = WildFileCmp("..", pattern);
		if (cmp)
			dta.SetResult("..",
			              0,
			              default_date,
			              default_time,
			              FatAttributeFlags::Directory);
		search_file = first_file;
		if (cmp)
			return true;
	}
	search_file = find_vfile_by_atribute_pattern_and_pos(search_file,
	                                                     attr,
	                                                     pattern,
	                                                     pos);
	if (search_file) {
		FatAttributeFlags search_attr = {FatAttributeFlags::ReadOnly};
		search_attr.directory = search_file->isdir;

		dta.SetResult(search_file->name.c_str(),
		              search_file->data->size(),
		              search_file->date,
		              search_file->time,
		              search_attr);
		search_file = search_file->next;
		return true;
	}
	DOS_SetError(DOSERR_NO_MORE_FILES);
	return false;
}

bool Virtual_Drive::GetFileAttr(const char* name, FatAttributeFlags* attr)
{
	*attr = {};
	assert(name);
	if (*name == 0) {
		attr->directory = true;
		attr->read_only = true;
		return true;
	}
	const auto vfile = find_vfile_by_name(name);
	if (vfile) {
		attr->directory = vfile->isdir;
		attr->read_only = true;
		return true;
	}
	return false;
}

bool Virtual_Drive::SetFileAttr(const char* name,
                                [[maybe_unused]] FatAttributeFlags attr)
{
	assert(name);
	if (*name == 0) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return true;
	}
	DOS_SetError(vfile_name_exists(name) ? DOSERR_ACCESS_DENIED
	                                     : DOSERR_FILE_NOT_FOUND);
	return false;
}

bool Virtual_Drive::Rename([[maybe_unused]] const char * oldname, [[maybe_unused]] const char * newname) {
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

bool Virtual_Drive::IsRemote() {
	return false;
}

bool Virtual_Drive::IsRemovable() {
	return false;
}

Bits Virtual_Drive::UnMount() {
	return 1;
}

const char* Virtual_Drive::GetLabel()
{
	return "DOSBOX";
}

bool Virtual_Drive::is_name_equal(const vfile_block_t vfile, const char* name) const
{
	unsigned int position = vfile->position;
	return iequals(name,
	               position ? vfilenames[position].shortname + "\\" + vfile->name
	                        : vfile->name);
}

vfile_block_t Virtual_Drive::find_vfile_by_name(const char* name) const
{
	return find_vfile_by_predicate(first_file, [this, name](vfile_block_t vfile) {
		return is_name_equal(vfile, name);
	});
}

vfile_block_t Virtual_Drive::find_vfile_dir_by_name(const char* dir) const
{
	return find_vfile_by_predicate(first_file, [dir](vfile_block_t vfile) {
		return vfile->isdir && iequals(vfile->name, dir);
	});
}

bool Virtual_Drive::vfile_name_exists(const std::string& name) const
{
	return find_vfile_by_name(name.c_str()).get();
}

void Virtual_Drive::EmptyCache()
{
	while (first_file) {
		first_file = first_file->next;
	}
	vfile_pos = 1;
	PROGRAMS_Destroy(nullptr);
	vfilenames = {Filename{"", ""}};
	Add_VFiles(first_shell != nullptr);
}
