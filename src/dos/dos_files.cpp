/*
 *  Copyright (C) 2020-2024  The DOSBox Staging Team
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

#include "dos_inc.h"

#include <array>
#include <cctype>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "dosbox.h"
#include "bios.h"
#include "mem.h"
#include "regs.h"
#include "drives.h"
#include "cross.h"
#include "string_utils.h"
#include "support.h"

#define DOS_FILESTART 4

#define FCB_SUCCESS     0
#define FCB_READ_NODATA	1
#define FCB_READ_PARTIAL 3
#define FCB_ERR_NODATA  1
#define FCB_ERR_EOF     3
#define FCB_ERR_WRITE   1

std::array<std::unique_ptr<DOS_File>, DOS_FILES> Files = {};

std::array<std::shared_ptr<DOS_Drive>, DOS_DRIVES> Drives = {};

enum class FileSharingMode
{
	Compatibility,
	DenyReadWrite,
	DenyWrite,
	DenyRead,
	DenyNone
};

// https://stanislavs.org/helppc/int_21-3d.html
union FileFlagBits
{
	uint8_t data = 0;
	bit_view<0, 3> access_mode;
	bit_view<4, 3> sharing_mode;
};

struct FileAccessMode
{
	bool read = false;
	bool write = false;
};

struct FileOpenFlags
{
	FileSharingMode sharing = FileSharingMode::Compatibility;
	FileAccessMode access = {};
};

static FileOpenFlags parse_file_flags(const uint8_t flags)
{
	FileOpenFlags ret = {};

	const FileFlagBits flag_bits = {flags};

	switch (flag_bits.access_mode) {
		case OPEN_READ:
		case OPEN_READ_NO_MOD:
			ret.access.read = true;
			ret.access.write = false;
			break;
		case OPEN_WRITE:
			ret.access.read = false;
			ret.access.write = true;
			break;
		case OPEN_READWRITE:
			ret.access.read = true;
			ret.access.write = true;
			break;
		default:
			// Assume read/write access for the purpose of file locking checks
			// This should throw an error when the file actually tries to open
			ret.access.read = true;
			ret.access.write = true;
	}

	switch (flag_bits.sharing_mode) {
		case 0b000:
			ret.sharing = FileSharingMode::Compatibility;
			break;
		case 0b001:
			ret.sharing = FileSharingMode::DenyReadWrite;
			break;
		case 0b010:
			ret.sharing = FileSharingMode::DenyWrite;
			break;
		case 0b011:
			ret.sharing = FileSharingMode::DenyRead;
			break;
		case 0b100:
			ret.sharing = FileSharingMode::DenyNone;
			break;
		default:
			// If we somehow get something invalid, assume compatibiliy mode.
			ret.sharing = FileSharingMode::Compatibility;
	}

	return ret;
}

static bool single_directional_sharing_check(const FileOpenFlags& new_file, const FileOpenFlags& existing_file)
{
	switch (new_file.sharing) {
		case FileSharingMode::Compatibility:
			return existing_file.sharing == FileSharingMode::Compatibility;
		case FileSharingMode::DenyReadWrite:
			return false;
		case FileSharingMode::DenyWrite:
			if (existing_file.sharing == FileSharingMode::Compatibility) {
				return false;
			} else {
				return !existing_file.access.write;
			}
		case FileSharingMode::DenyRead:
			if (existing_file.sharing == FileSharingMode::Compatibility) {
				return false;
			} else {
				return !existing_file.access.read;
			}
		case FileSharingMode::DenyNone:
			return existing_file.sharing != FileSharingMode::Compatibility;
	}
	assertm(false, "Invalid enum value");
	return false;
}

static bool file_modes_are_compatible(const FileOpenFlags& new_file, const FileOpenFlags& existing_file)
{
	// Needs a two-way check
	// Run this function twice with arguments reversed the second time
	return single_directional_sharing_check(new_file, existing_file)
		&& single_directional_sharing_check(existing_file, new_file);
}

static bool file_is_locked(const char *file_name, const uint8_t drive, const uint8_t flags)
{
	const FileOpenFlags new_file = parse_file_flags(flags);

	for (int i = 0; i < DOS_FILES; ++i) {
		if (Files[i] && Files[i]->GetDrive() == drive && Files[i]->IsName(file_name)) {
			const FileOpenFlags existing_file = parse_file_flags(Files[i]->flags);
			if (!file_modes_are_compatible(new_file, existing_file)) {
				return true;
			}
		}
	}

	return false;
}

static bool regions_overlap(const uint32_t pos1, const uint32_t len1, const uint32_t pos2, const uint32_t len2)
{
	return !((pos1 >= pos2 + len2) || (pos1 + len1 <= pos2));
}

static bool region_is_locked(const int file_handle, const uint32_t pos, const uint32_t len)
{
	for (int i = 0; i < DOS_FILES; ++i) {
		// Ignore locks held by the current file handle,
		// Need to check other handles pointing to the same file.
		if (i != file_handle && Files[i]) {
			const auto drive_match =
				(Files[i]->GetDrive() == Files[file_handle]->GetDrive());

			const auto filename_match =
				(Files[i]->IsName(Files[file_handle]->GetName()));

			if (drive_match && filename_match) {
				for (const auto &lock : Files[i]->region_locks) {
					if (regions_overlap(pos, len, lock.pos, lock.len)) {
						return true;
					}
				}
			}
		}
	}
	return false;
}

uint8_t DOS_GetDefaultDrive(void) {
//	return DOS_SDA(DOS_SDA_SEG,DOS_SDA_OFS).GetDrive();
	uint8_t d = DOS_SDA(DOS_SDA_SEG,DOS_SDA_OFS).GetDrive();
	if( d != dos.current_drive ) LOG(LOG_DOSMISC,LOG_ERROR)("SDA drive %d not the same as dos.current_drive %d",d,dos.current_drive);
	return dos.current_drive;
}

void DOS_SetDefaultDrive(uint8_t drive) {
//	if (drive<=DOS_DRIVES && ((drive<2) || Drives[drive])) DOS_SDA(DOS_SDA_SEG,DOS_SDA_OFS).SetDrive(drive);
	if (drive<DOS_DRIVES && ((drive<2) || Drives[drive])) {dos.current_drive = drive; DOS_SDA(DOS_SDA_SEG,DOS_SDA_OFS).SetDrive(drive);}
}

bool DOS_MakeName(const char* const name, char* const fullname, uint8_t* drive)
{
	if(!name || *name == 0 || *name == ' ') {
		/* Both \0 and space are seperators and
		 * empty filenames report file not found */
		DOS_SetError(DOSERR_FILE_NOT_FOUND);
		return false;
	}
	const char * name_int = name;
	char tempdir[DOS_PATHLENGTH];
	char upname[DOS_PATHLENGTH];
	Bitu r,w;
	uint8_t c;
	*drive = DOS_GetDefaultDrive();
	/* First get the drive */
	if (name_int[1]==':') {
		*drive=(name_int[0] | 0x20)-'a';
		name_int+=2;
	}
	if (*drive>=DOS_DRIVES || !Drives[*drive]) { 
		DOS_SetError(DOSERR_PATH_NOT_FOUND);
		return false; 
	}
	r=0;w=0;
	while (r < DOS_PATHLENGTH && name_int[r] != 0) {
		c=name_int[r++];
		if ((c>='a') && (c<='z')) c-=32;
		else if (c==' ') continue; /* should be separator */
		else if (c=='/') c='\\';
		upname[w++]=c;
	}
	while (r>0 && name_int[r-1]==' ') r--;
	if (r>=DOS_PATHLENGTH) { DOS_SetError(DOSERR_PATH_NOT_FOUND);return false; }
	upname[w]=0;
	/* Now parse the new file name to make the final filename */
	if (upname[0]!='\\') strcpy(fullname,Drives[*drive]->curdir);
	else fullname[0]=0;
	uint32_t lastdir=0;uint32_t t=0;
	while (fullname[t]!=0) {
		if ((fullname[t]=='\\') && (fullname[t+1]!=0)) lastdir=t;
		t++;
	};
	r=0;w=0;
	tempdir[0]=0;
	bool stop=false;
	while (!stop) {
		if (upname[r]==0) stop=true;
		if ((upname[r]=='\\') || (upname[r]==0)){
			tempdir[w]=0;
			if (tempdir[0]==0) { w=0;r++;continue;}
			if (strcmp(tempdir,".")==0) {
				tempdir[0]=0;			
				w=0;r++;
				continue;
			}

			int32_t iDown;
			bool dots = true;
			int32_t templen=(int32_t)strlen(tempdir);
			for(iDown=0;(iDown < templen) && dots;iDown++)
				if(tempdir[iDown] != '.')
					dots = false;

			// only dots?
			if (dots && (templen > 1)) {
				int32_t cDots = templen - 1;
				for(iDown=(int32_t)strlen(fullname)-1;iDown>=0;iDown--) {
					if(fullname[iDown]=='\\' || iDown==0) {
						lastdir = iDown;
						cDots--;
						if(cDots==0)
							break;
					}
				}
				fullname[lastdir]=0;
				t=0;lastdir=0;
				while (fullname[t]!=0) {
					if ((fullname[t]=='\\') && (fullname[t+1]!=0)) lastdir=t;
					t++;
				}
				tempdir[0]=0;
				w=0;r++;
				continue;
			}
			

			lastdir=(uint32_t)strlen(fullname);

			if (lastdir!=0) strcat(fullname,"\\");
			char * ext=strchr(tempdir,'.');
			if (ext) {
				if(strchr(ext+1,'.')) { 
				//another dot in the extension =>file not found
				//Or path not found depending on wether 
				//we are still in dir check stage or file stage
					if(stop)
						DOS_SetError(DOSERR_FILE_NOT_FOUND);
					else
						DOS_SetError(DOSERR_PATH_NOT_FOUND);
					return false;
				}
				
				ext[4] = 0;
				if((strlen(tempdir) - strlen(ext)) > 8) memmove(tempdir + 8, ext, 5);
			} else tempdir[8]=0;

			const auto tempdir_len = strlen(tempdir);
			for (size_t i = 0; i < tempdir_len; ++i) {
				c = tempdir[i];
				if ((c >= 'A') && (c <= 'Z')) {
					continue;
				}
				if ((c >= '0') && (c <= '9')) {
					continue;
				}
				if (is_upper_ascii(c)) {
					continue;
				}
				switch (c) {
				case '$':	case '#':	case '@':	case '(':	case ')':
				case '!':	case '%':	case '{':	case '}':	case '`':	case '~':
				case '_':	case '-':	case '.':	case '*':	case '?':	case '&':
				case '\'':	case '+':	case '^':	case 246:	case 255:	case 0xa0:
				case 0xe5:	case 0xbd:	case 0x9d:
					break;
				default:
					LOG(LOG_FILES,LOG_NORMAL)("Makename encountered an illegal char %c hex:%X in %s!",c,c,name);
					DOS_SetError(DOSERR_PATH_NOT_FOUND);return false;
					break;
				}
			}

			if (strlen(fullname)+strlen(tempdir)>=DOS_PATHLENGTH) {
				DOS_SetError(DOSERR_PATH_NOT_FOUND);return false;
			}
		   
			strcat(fullname,tempdir);
			tempdir[0]=0;
			w=0;r++;
			continue;
		}
		tempdir[w++]=upname[r++];
	}
	return true;
}

void DOS_Sort(std::vector<DOS_DTA::Result>& list, const ResultSorting sorting,
              const bool reverse_order, const ResultGrouping grouping)
{
	auto compare = [&](const DOS_DTA::Result& result1,
	                   const DOS_DTA::Result& result2) {
		if (grouping == ResultGrouping::FilesFirst ||
		    grouping == ResultGrouping::NonFilesFirst) {
			if (!result1.IsFile() && result2.IsFile()) {
				return (grouping == ResultGrouping::NonFilesFirst);
			}
			if (result1.IsFile() && !result2.IsFile()) {
				return (grouping == ResultGrouping::FilesFirst);
			}
		}

		auto& r1 = reverse_order ? result2 : result1;
		auto& r2 = reverse_order ? result1 : result2;

		switch (sorting) {
		case ResultSorting::ByName:
			return r1.name.compare(r2.name) < 0;
		case ResultSorting::ByExtension:
			return r1.GetExtension().compare(r2.GetExtension()) < 0;
		case ResultSorting::BySize:
			// Do not compare sizes of objects which are not files!
			if (!r1.IsFile()) {
				return true;
			} else if (!r2.IsFile()) {
				return false;
			}
			// Both are files - we can compare sizes
			return r1.size < r2.size;
		case ResultSorting::ByDateTime:
			return r1.date < r2.date ||
			       (r1.date == r2.date && r1.time < r2.time);
		case ResultSorting::None:
		default:
			return false;
		}
	};

	std::stable_sort(list.begin(), list.end(), compare);
}

bool DOS_GetCurrentDir(uint8_t drive, char* const buffer)
{
	if (drive==0) drive=DOS_GetDefaultDrive();
	else drive--;
	if ((drive>=DOS_DRIVES) || (!Drives[drive])) {
		DOS_SetError(DOSERR_INVALID_DRIVE);
		return false;
	}
	strcpy(buffer,Drives[drive]->curdir);
	return true;
}
static bool PathExists(const char *name);

bool DOS_ChangeDir(const char* const dir)
{
	uint8_t drive;
	char fulldir[DOS_PATHLENGTH];
	const auto exists_and_set = DOS_MakeName(dir, fulldir, &drive) &&
	                            Drives.at(drive)->TestDir(fulldir) &&
	                            safe_strcpy(Drives.at(drive)->curdir, fulldir);
	if (!exists_and_set) {
		DOS_SetError(DOSERR_PATH_NOT_FOUND);
	}
	return exists_and_set;
}

bool DOS_MakeDir(const char* const dir)
{
	uint8_t drive;char fulldir[DOS_PATHLENGTH];
	size_t len = strlen(dir);
	if(!len || dir[len-1] == '\\') {
		DOS_SetError(DOSERR_PATH_NOT_FOUND);
		return false;
	}
	if (!DOS_MakeName(dir,fulldir,&drive)) return false;

	const auto drive_ptr = Drives.at(drive);

	if (drive_ptr->IsReadOnly()) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	if (drive_ptr->MakeDir(fulldir)) {
		return true;
	}

	/* Determine reason for failing */
	if (drive_ptr->TestDir(fulldir)) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
	} else
		DOS_SetError(DOSERR_PATH_NOT_FOUND);
	return false;
}

bool DOS_RemoveDir(const char* const dir)
{
	/* We need to do the test before the removal as can not rely on
	 * the host to forbid removal of the current directory.
	 * We never change directory. Everything happens in the drives.
	 */
	uint8_t drive;char fulldir[DOS_PATHLENGTH];
	if (!DOS_MakeName(dir,fulldir,&drive)) return false;

	const auto drive_ptr = Drives.at(drive);

	/* Check if exists */
	if (!drive_ptr->TestDir(fulldir)) {
		DOS_SetError(DOSERR_PATH_NOT_FOUND);
		return false;
	}
	/* See if it's current directory */
	char currdir[DOS_PATHLENGTH]= { 0 };
	DOS_GetCurrentDir(drive + 1 ,currdir);
	if(strcmp(currdir,fulldir) == 0) {
		DOS_SetError(DOSERR_REMOVE_CURRENT_DIRECTORY);
		return false;
	}

	if (drive_ptr->IsReadOnly()) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	if (drive_ptr->RemoveDir(fulldir)) {
		return true;
	}

	/* Failed. We know it exists and it's not the current dir */
	/* Assume non empty */
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

static bool PathExists(const char* const name)
{
	const char* leading = strrchr(name,'\\');
	if(!leading) return true;
	char temp[CROSS_LEN];
	safe_strcpy(temp, name);
	char * lead = strrchr(temp,'\\');
	if (lead == temp) return true;
	*lead = 0;
	uint8_t drive;char fulldir[DOS_PATHLENGTH];
	if (!DOS_MakeName(temp,fulldir,&drive)) return false;
	if (!Drives.at(drive)->TestDir(fulldir)) {
		return false;
	}
	return true;
}

bool DOS_Rename(const char* const oldname, const char* const newname)
{
	uint8_t driveold;char fullold[DOS_PATHLENGTH];
	uint8_t drivenew;char fullnew[DOS_PATHLENGTH];
	if (!DOS_MakeName(oldname,fullold,&driveold)) return false;
	if (!DOS_MakeName(newname,fullnew,&drivenew)) return false;
	/* No tricks with devices */
	if ( (DOS_FindDevice(oldname) != DOS_DEVICES) ||
	     (DOS_FindDevice(newname) != DOS_DEVICES) ) {
		DOS_SetError(DOSERR_FILE_NOT_FOUND);
		return false;
	}
	/* Must be on the same drive */
	if(driveold != drivenew) {
		DOS_SetError(DOSERR_NOT_SAME_DEVICE);
		return false;
	}

	const auto new_ptr = Drives.at(drivenew);

	/*Test if target exists => no access */
	FatAttributeFlags attr = {};
	if (new_ptr->GetFileAttr(fullnew, &attr)) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	const auto old_ptr = Drives.at(driveold);

	/* Source must exist */
	if (!old_ptr->GetFileAttr(fullold, &attr)) {
		if (!PathExists(oldname)) DOS_SetError(DOSERR_PATH_NOT_FOUND);
		else DOS_SetError(DOSERR_FILE_NOT_FOUND);
		return false;
	}

	if (new_ptr->IsReadOnly()) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	if (new_ptr->Rename(fullold, fullnew)) {
		return true;
	}
	/* Rename failed despite checks => no access */
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool DOS_FindFirst(const char* search, FatAttributeFlags attr, bool fcb_findfirst)
{
	LOG(LOG_FILES, LOG_NORMAL)
	("file search attributes %X name %s", attr._data, search);
	DOS_DTA dta(dos.dta());
	uint8_t drive;char fullsearch[DOS_PATHLENGTH];
	char dir[DOS_PATHLENGTH];char pattern[DOS_PATHLENGTH];
	size_t len = strlen(search);

	const bool is_root = (len > 2) && (search[len - 2] == ':') &&
	                     (attr == FatAttributeFlags::Volume);
	const bool is_directory = len && search[len - 1] == '\\';
	if (!is_root && is_directory) {
		// Dark Forces installer, but c:\ is allright for volume
		// labels(exclusively set)
		DOS_SetError(DOSERR_NO_MORE_FILES);
		return false;
	}
	if (!DOS_MakeName(search,fullsearch,&drive)) return false;
	//Check for devices. FindDevice checks for leading subdir as well
	bool device = (DOS_FindDevice(search) != DOS_DEVICES);

	/* Split the search in dir and pattern */
	char * find_last;
	find_last=strrchr(fullsearch,'\\');
	if (!find_last) {	/*No dir */
		safe_strcpy(pattern, fullsearch);
		dir[0]=0;
	} else {
		*find_last=0;
		safe_strcpy(pattern, find_last + 1);
		safe_strcpy(dir, fullsearch);
	}

	dta.SetupSearch(drive, attr, pattern);

	if(device) {
		find_last = strrchr(pattern,'.');
		if(find_last) *find_last = 0;
		//TODO use current date and time
		dta.SetResult(pattern, 0, 0, 0, FatAttributeFlags::Device);
		LOG(LOG_DOSMISC, LOG_WARN)("finding device %s", pattern);
		return true;
	}

	if (Drives.at(drive)->FindFirst(dir, dta, fcb_findfirst)) {
		return true;
	}

	return false;
}

bool DOS_FindNext(void) {
	DOS_DTA dta(dos.dta());
	uint8_t i = dta.GetSearchDrive();
	if(i >= DOS_DRIVES || !Drives[i]) {
		/* Corrupt search. */
		LOG(LOG_FILES,LOG_ERROR)("Corrupt search!!!!");
		DOS_SetError(DOSERR_NO_MORE_FILES); 
		return false;
	} 
	if (Drives[i]->FindNext(dta)) return true;
	return false;
}


bool DOS_ReadFile(uint16_t entry,uint8_t * data,uint16_t * amount,bool fcb) {
	uint32_t handle = fcb?entry:RealHandle(entry);
	if (handle>=DOS_FILES) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	};
	if (!Files[handle]) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	};
/*
	if ((Files[handle]->flags & 0x0f) == OPEN_WRITE)) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	}
*/
	uint16_t toread=*amount;

	// Get current position
	uint32_t pos = 0;
	Files[handle]->Seek(&pos, SEEK_CUR);

	if (region_is_locked(handle, pos, toread)) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	bool ret=Files[handle]->Read(data,&toread);
	*amount=toread;
	return ret;
}

bool DOS_WriteFile(uint16_t entry,uint8_t * data,uint16_t * amount,bool fcb) {
	uint32_t handle = fcb?entry:RealHandle(entry);
	if (handle>=DOS_FILES) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	};
	if (!Files[handle]) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	};
/*
	if ((Files[handle]->flags & 0x0f) == OPEN_READ)) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	}
*/
	uint16_t towrite=*amount;

	// Get current position
	uint32_t pos = 0;
	Files[handle]->Seek(&pos, SEEK_CUR);

	if (region_is_locked(handle, pos, towrite)) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	bool ret=Files[handle]->Write(data,&towrite);
	*amount=towrite;
	if (ret) {
		// When a write happens, mark that the time should be flushed on close.
		// The updated time value is not the time of the write but the time of the close.
		// Writes also do not update the local date/time fields.
		// Local date/time fields (as returned by a call to DOS_GetFileDate())
		// are set on file open and only get changed by a call to DOS_SetFileDate()
		// This matches the behavior as tested on MS-DOS 6.22
		Files[handle]->flush_time_on_close = FlushTimeOnClose::CurrentTime;
	}
	return ret;
}

bool DOS_SeekFile(uint16_t entry,uint32_t * pos,uint32_t type,bool fcb) {
	if (type > DOS_SEEK_END) {
		DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
		return false;
	}
	uint32_t handle = fcb?entry:RealHandle(entry);
	if (handle>=DOS_FILES) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	};
	if (!Files[handle]) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	};
	return Files[handle]->Seek(pos,type);
}

bool DOS_CloseFile(uint16_t entry, bool fcb, uint8_t * refcnt) {
	uint32_t handle = fcb?entry:RealHandle(entry);
	if (handle>=DOS_FILES) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	};
	if (!Files[handle]) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	};
	Files[handle]->Close();

	DOS_PSP psp(dos.psp());
	if (!fcb) psp.SetFileHandle(entry,0xff);

	Bits refs=Files[handle]->RemoveRef();
	if (refs<=0) {
		Files[handle].reset();
		refs=0;
	}
	if (refcnt!=nullptr) *refcnt=static_cast<uint8_t>(refs+1);
	return true;
}

bool DOS_FlushFile(uint16_t entry) {
	uint32_t handle=RealHandle(entry);
	if (handle>=DOS_FILES) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	};
	if (!Files[handle]) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	};
	LOG(LOG_DOSMISC,LOG_NORMAL)("FFlush used.");
	return true;
}

bool DOS_CreateFile(const char* name, FatAttributeFlags attributes,
                    uint16_t* entry, bool fcb)
{
	// Creation of a device is the same as opening it
	// Tc201 installer
	if (DOS_FindDevice(name) != DOS_DEVICES)
		return DOS_OpenFile(name, OPEN_READ, entry, fcb);

	LOG(LOG_FILES, LOG_NORMAL)("file create attributes %X file %s", attributes._data, name);

	/* First check if the name is correct */
	char fullname[DOS_PATHLENGTH];
	uint8_t drive;
	if (!DOS_MakeName(name, fullname, &drive))
		return false;

	/* Check for a free file handle */
	uint8_t handle = DOS_FILES;
	for (uint8_t i = 0; i < DOS_FILES; ++i) {
		if (!Files[i]) {
			handle = i;
			break;
		}
	}
	if (handle == DOS_FILES) {
		DOS_SetError(DOSERR_TOO_MANY_OPEN_FILES);
		return false;
	}

	if (Drives.at(drive)->IsReadOnly() || file_is_locked(fullname, drive, OPEN_READWRITE)) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	/* We have a position in the main table now find one in the psp table */
	DOS_PSP psp(dos.psp());
	*entry = fcb?handle:psp.FindFreeFileEntry();
	if (*entry==0xff) {
		DOS_SetError(DOSERR_TOO_MANY_OPEN_FILES);
		return false;
	}
	/* Don't allow directories to be created */
	if (attributes.directory) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
	Files[handle] = Drives.at(drive)->FileCreate(fullname, attributes);
	if (Files[handle]) {
		Files[handle]->SetDrive(drive);
		Files[handle]->AddRef();
		if (!fcb) psp.SetFileHandle(*entry,handle);
		return true;
	} else {
		if (!PathExists(name)) DOS_SetError(DOSERR_PATH_NOT_FOUND);
		else DOS_SetError(DOSERR_ACCESS_DENIED); // Create failed but path exists => no access
		return false;
	}
}

bool DOS_OpenFile(const char* name, uint8_t flags, uint16_t* entry, bool fcb)
{
	/* First check for devices */
	if (flags>2) LOG(LOG_FILES,LOG_ERROR)("Special file open command %X file %s",flags,name);
	else LOG(LOG_FILES,LOG_NORMAL)("file open command %X file %s",flags,name);

	FatAttributeFlags attr = {};
	uint8_t devnum = DOS_FindDevice(name);
	bool device = (devnum != DOS_DEVICES);
	if (!device && DOS_GetFileAttr(name, &attr)) {
		// DON'T ALLOW directories to be openened.
		// (skip test if file is device).
		if (attr.directory || attr.volume) {
			DOS_SetError(DOSERR_ACCESS_DENIED);
			return false;
		}
	}

	/* First check if the name is correct */
	char fullname[DOS_PATHLENGTH];
	uint8_t drive;
	if (!DOS_MakeName(name, fullname, &drive))
		return false;

	/* Check for a free file handle */
	uint8_t handle = DOS_FILES;
	for (uint8_t i = 0; i < DOS_FILES; ++i) {
		if (!Files[i]) {
			handle = i;
			break;
		}
	}
	if (handle == DOS_FILES) {
		DOS_SetError(DOSERR_TOO_MANY_OPEN_FILES);
		return false;
	}

	/* We have a position in the main table now find one in the psp table */
	DOS_PSP psp(dos.psp());
	*entry = fcb?handle:psp.FindFreeFileEntry();

	if (*entry==0xff) {
		DOS_SetError(DOSERR_TOO_MANY_OPEN_FILES);
		return false;
	}
	if (device) {
		Files[handle] = std::make_unique<DOS_Device>(*Devices[devnum]);
	} else {
		if (file_is_locked(fullname, drive, flags)) {
			DOS_SetError(DOSERR_ACCESS_DENIED);
			return false;
		}
		const auto old_errorcode = dos.errorcode;
		dos.errorcode = 0;
		Files[handle] = Drives.at(drive)->FileOpen(fullname, flags);
		if (Files[handle]) {
			Files[handle]->SetDrive(drive);
		}
		if (dos.errorcode == DOSERR_ACCESS_CODE_INVALID)
			return false;
		dos.errorcode = old_errorcode;
	}
	if (Files[handle]) {
		Files[handle]->AddRef();
		if (!fcb) psp.SetFileHandle(*entry,handle);
		return true;
	} else {
		//Test if file exists, but opened in read-write mode (and writeprotected)
		if (((flags & 3) != OPEN_READ) &&
		    Drives.at(drive)->FileExists(fullname)) {
			DOS_SetError(DOSERR_ACCESS_DENIED);
		} else {
			if(!PathExists(name)) DOS_SetError(DOSERR_PATH_NOT_FOUND); 
			else DOS_SetError(DOSERR_FILE_NOT_FOUND);
		}
		return false;
	}
}

bool DOS_OpenFileExtended(const char* name, uint16_t flags,
                          FatAttributeFlags createAttr, uint16_t action,
                          uint16_t* entry, uint16_t* status)
{
	// FIXME: Not yet supported : Bit 13 of flags (int 0x24 on critical error)
	uint16_t result = 0;
	if (action==0) {
		// always fail setting
		DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
		return false;
	} else {
		if (((action & 0x0f)>2) || ((action & 0xf0)>0x10)) {
			// invalid action parameter
			DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
			return false;
		}
	}
	if (DOS_OpenFile(name, (uint8_t)(flags&0xff), entry)) {
		// File already exists
		switch (action & 0x0f) {
			case 0x00:		// failed
				DOS_SetError(DOSERR_FILE_ALREADY_EXISTS);
				return false;
			case 0x01:		// file open (already done)
				result = 1;
				break;
			case 0x02:		// replace
				DOS_CloseFile(*entry);
				if (!DOS_CreateFile(name, createAttr, entry)) return false;
				result = 3;
				break;
			default:
				DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
				E_Exit("DOS: OpenFileExtended: Unknown action.");
				break;
		}
	} else {
		// File doesn't exist
		if ((action & 0xf0)==0) {
			// uses error code from failed open
			return false;
		}
		// Create File
		if (!DOS_CreateFile(name, createAttr, entry)) {
			// uses error code from failed create
			return false;
		}
		result = 2;
	}
	// success
	*status = result;
	return true;
}

bool DOS_UnlinkFile(const char* const name)
{
	char fullname[DOS_PATHLENGTH];
	uint8_t drive;

	// An non-existing device returns an access denied error
	if (DOS_FindDevice(name) != DOS_DEVICES) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	// If a proper DOS path cannot be constructed ...
	if (!DOS_MakeName(name, fullname, &drive)) {
		DOS_SetError(DOSERR_PATH_NOT_FOUND);
		return false;
	}

	if (Drives.at(drive)->IsReadOnly()) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	return Drives.at(drive)->FileUnlink(fullname);
}

bool DOS_GetFileAttr(const char* const name, FatAttributeFlags* attr)
{
	char fullname[DOS_PATHLENGTH];
	uint8_t drive;
	if (!DOS_MakeName(name, fullname, &drive)) {
		return false;
	}

	if (Drives.at(drive)->GetFileAttr(fullname, attr)) {
		if (Drives.at(drive)->IsReadOnly()) {
			attr->read_only = true;
		}
		return true;
	} else {
		*attr = 0;
		DOS_SetError(DOSERR_FILE_NOT_FOUND);
		return false;
	}
}

bool DOS_SetFileAttr(const char* const name, FatAttributeFlags attr)
{
	char fullname[DOS_PATHLENGTH];
	uint8_t drive;
	if (!DOS_MakeName(name, fullname, &drive))
		return false;

	const auto drive_ptr = Drives.at(drive);

	if (drive_ptr->IsReadOnly()) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	FatAttributeFlags old_attr = {};
	if (!drive_ptr->GetFileAttr(fullname, &old_attr)) {
		DOS_SetError(DOSERR_FILE_NOT_FOUND);
		return false;
	}

	if (old_attr.volume != attr.volume) {
		// Change in volume label attribute
		LOG_WARNING("Attempted to change volume label attribute of '%s' with SetFileAttr",
		            name);
		return false;
	}

	// Preserve what cannot be changed
	attr.volume    = old_attr.volume;
	attr.directory = old_attr.directory;

	return drive_ptr->SetFileAttr(fullname, attr);
}

bool DOS_Canonicalize(const char* const name, char* const canonicalized)
{
	// TODO Add Better support for devices and shit but will it be needed i
	// doubt it :)

	// Initalize to invalid drive index so assert will fire if DOS_MakeName
	// does not set this
	uint8_t drive                 = 255;
	char fullname[DOS_PATHLENGTH] = {};
	if (!DOS_MakeName(name, fullname, &drive)) {
		return false;
	}
	canonicalized[0] = drive_letter(drive);
	canonicalized[1] = ':';
	canonicalized[2] = '\\';
	strcpy(&canonicalized[3], fullname);
	return true;
}

std::string DOS_Canonicalize(const char* const name)
{
	char canonicalized[DOS_PATHLENGTH] = {};
	if (!DOS_Canonicalize(name, canonicalized)) {
		return {};
	}
	return std::string(canonicalized);
}

bool DOS_GetFreeDiskSpace(uint8_t drive, uint16_t* bytes, uint8_t* sectors,
                          uint16_t* clusters, uint16_t* free)
{
	if (drive == 0) {
		drive = DOS_GetDefaultDrive();
	} else {
		--drive;
	}
	if ((drive >= DOS_DRIVES) || (!Drives[drive])) {
		DOS_SetError(DOSERR_INVALID_DRIVE);
		return false;
	}
	return Drives[drive]->AllocationInfo(bytes,sectors,clusters,free);
}

bool DOS_DuplicateEntry(uint16_t entry,uint16_t * newentry) {
	// Dont duplicate console handles
/*	if (entry<=STDPRN) {
		*newentry = entry;
		return true;
	};
*/
	const uint8_t handle = RealHandle(entry);
	if (handle >= DOS_FILES) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	};
	if (!Files[handle]) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	};
	DOS_PSP psp(dos.psp());
	*newentry = psp.FindFreeFileEntry();
	if (*newentry==0xff) {
		DOS_SetError(DOSERR_TOO_MANY_OPEN_FILES);
		return false;
	}
	Files[handle]->AddRef();	
	psp.SetFileHandle(*newentry,handle);
	return true;
}

bool DOS_ForceDuplicateEntry(uint16_t entry,uint16_t newentry) {
	if(entry == newentry) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	}
	uint8_t orig = RealHandle(entry);
	if (orig >= DOS_FILES) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	};
	if (!Files[orig]) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	};
	uint8_t newone = RealHandle(newentry);
	if (newone < DOS_FILES && Files[newone]) {
		DOS_CloseFile(newentry);
	}
	DOS_PSP psp(dos.psp());
	Files[orig]->AddRef();
	psp.SetFileHandle(newentry,orig);
	return true;
}


bool DOS_CreateTempFile(char * const name,uint16_t * entry) {
	size_t namelen=strlen(name);
	char * tempname=name+namelen;
	if (namelen==0) {
		// temp file created in root directory
		tempname[0]='\\';
		tempname++;
	} else {
		if ((name[namelen-1]!='\\') && (name[namelen-1]!='/')) {
			tempname[0]='\\';
			tempname++;
		}
	}
	const auto old_errorcode = dos.errorcode;
	dos.errorcode = 0;

	static const auto randomize_letter = create_randomizer<int16_t>('A', 'Z');
	do {
		uint32_t i;
		for (i=0;i<8;i++) {
			tempname[i] = check_cast<char>(randomize_letter());
		}
		tempname[8]=0;
	} while (DOS_FileExists(name));

	DOS_CreateFile(name,0,entry);
	if (dos.errorcode)
		return false;

	dos.errorcode = old_errorcode;
	return true;
}

char DOS_ToUpper(char c) {
	unsigned char uc = *reinterpret_cast<unsigned char*>(&c);
	if (uc > 0x60 && uc < 0x7B) uc -= 0x20;
	else if (uc > 0x7F && uc < 0xA5) {
		const unsigned char t[0x25] = { 
			0x00, 0x9a, 0x45, 0x41, 0x8E, 0x41, 0x8F, 0x80, 0x45, 0x45, 0x45, 0x49, 0x49, 0x49, 0x00, 0x00,
			0x00, 0x92, 0x00, 0x4F, 0x99, 0x4F, 0x55, 0x55, 0x59, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x41, 0x49, 0x4F, 0x55, 0xA5};
			if (t[uc - 0x80]) uc = t[uc-0x80];
	}
	char sc = *reinterpret_cast<char*>(&uc);
	return sc;
}

#define FCB_SEP ":;,=+"
#define ILLEGAL ":.;,=+ \t/\"[]<>|"

static bool isvalid(const char in){
	const char ill[]=ILLEGAL;    
	return (uint8_t(in)>0x1F) && (!strchr(ill,in));
}

#define PARSE_SEP_STOP          0x01
#define PARSE_DFLT_DRIVE        0x02
#define PARSE_BLNK_FNAME        0x04
#define PARSE_BLNK_FEXT         0x08

#define PARSE_RET_NOWILD        0
#define PARSE_RET_WILD          1
#define PARSE_RET_BADDRIVE      0xff

// TODO: Refactor and document this function until it's understandable
uint8_t FCB_Parsename(uint16_t seg, uint16_t offset, uint8_t parser,
                      const char* string, uint8_t* change)
{
	const char* string_begin = string;
	uint8_t ret=0;
	if (!(parser & PARSE_DFLT_DRIVE)) {
		// default drive forced, this intentionally invalidates an extended FCB
		mem_writeb(PhysicalMake(seg,offset),0);
	}
	DOS_FCB fcb(seg,offset,false);	// always a non-extended FCB
	bool hasdrive = false;
	bool hasname = false;
	bool hasext = false;
	Bitu index=0;
	uint8_t fill=' ';
/* First get the old data from the fcb */
#ifdef _MSC_VER
#pragma pack (1)
#endif
	union {
		struct {
			char drive[2];
			char name[9];
			char ext[4];
		} GCC_ATTRIBUTE (packed) part;
		char full[DOS_FCBNAME];
	} fcb_name;
#ifdef _MSC_VER
#pragma pack()
#endif
	/* Get the old information from the previous fcb */
	fcb.GetName(fcb_name.full);
	auto drive_idx = drive_index(fcb_name.part.drive[0]) + 1;
	fcb_name.part.drive[0] = int_to_char(drive_idx);
	fcb_name.part.drive[1] = 0;
	fcb_name.part.name[8] = 0;
	fcb_name.part.ext[3] = 0;
	/* strip leading spaces */
	while((*string==' ')||(*string=='\t')) string++;

	/* Strip of the leading separator */
	if((parser & PARSE_SEP_STOP) && *string) {
		char sep[] = FCB_SEP;char a[2];
		a[0] = *string;a[1] = '\0';
		if (strcspn(a,sep) == 0) string++;
	} 

	/* Skip following spaces as well */
	while((*string==' ')||(*string=='\t')) string++;

	/* Check for a drive */
	if (string[1]==':') {
		unsigned char d = *reinterpret_cast<const unsigned char*>(&string[0]);
		if (!isvalid(toupper(d))) {string += 2; goto savefcb;} //TODO check (for ret value)
		fcb_name.part.drive[0]=0;
		hasdrive=true;

		// Floppies under dos always exist, but don't bother with that at this level
		assert(d <= CHAR_MAX); // drive letters are strictly "low" ASCII a/A to z/Z
		if (!(isalpha(d) && Drives[drive_index(static_cast<char>(d))])) {
			ret = 0xff;
		}
		// Always do THIS* and continue parsing, just return the right code
		fcb_name.part.drive[0] = DOS_ToUpper(string[0]) - 'A' + 1;
		string += 2;
	}

	/* Check for extension only file names */
	if (string[0] == '.') {string++;goto checkext;}

	/* do nothing if not a valid name */
	if(!isvalid(string[0])) goto savefcb;

	hasname = true;
	fill = ' ';
	index = 0;
	/* Copy the name */	
	while (true) {
		unsigned char nc = *reinterpret_cast<const unsigned char*>(
		        &string[0]);
		char ncs = (char)toupper(nc); //Should use DOS_ToUpper, but then more calls need to be changed.
		if (ncs == '*') { //Handle *
			fill = '?';
			ncs = '?';
		}
		if (ncs == '?' && !ret && index < 8) ret = 1; //Don't override bad drive
		if (!isvalid(ncs)) { //Fill up the name.
			while(index < 8) 
				fcb_name.part.name[index++] = fill; 
			break;
		}
		if (index < 8) { 
			fcb_name.part.name[index++] = (fill == '?')?fill:ncs; 
		}
		string++;
	}
	if (!(string[0]=='.')) goto savefcb;
	string++;
checkext:
	/* Copy the extension */
	hasext = true;
	fill = ' ';
	index = 0;
	while (true) {
		unsigned char nc = *reinterpret_cast<const unsigned char*>(
		        &string[0]);
		char ncs = (char)toupper(nc);
		if (ncs == '*') { //Handle *
			fill = '?';
			ncs = '?';
		}
		if (ncs == '?' && !ret && index < 3) ret = 1;
		if (!isvalid(ncs)) { //Fill up the name.
			while(index < 3) 
				fcb_name.part.ext[index++] = fill; 
			break;
		}
		if (index < 3) { 
			fcb_name.part.ext[index++] = (fill=='?')?fill:ncs; 
		}
		string++;
	}
savefcb:
	if (!hasdrive & !(parser & PARSE_DFLT_DRIVE)) fcb_name.part.drive[0] = 0;
	if (!hasname & !(parser & PARSE_BLNK_FNAME))
		safe_strcpy(fcb_name.part.name, "        ");
	if (!hasext & !(parser & PARSE_BLNK_FEXT))
		safe_strcpy(fcb_name.part.ext, "   ");
	fcb.SetName(fcb_name.part.drive[0], fcb_name.part.name, fcb_name.part.ext);
	fcb.ClearBlockRecsize(); //Undocumented bonus work.
	*change=(uint8_t)(string-string_begin);
	return ret;
}

// DTAExtendName splits the fullname into name and extension portions space-pads
// them up to their full 8.3 lengths, if needed. The space-padding is required
// to fully populate DOS data memory structures. Also note that DOS filenames
// might not have a dot or extension.
static std::pair<std::string, std::string> DTAExtendName(const char *fullname)
{
	auto truncate_or_pad = [](std::string s, const size_t desired_length) {
		const auto l = s.length();
		if (l < desired_length)
			s.append(desired_length - l, ' ');
		else if (l > desired_length)
			s.resize(desired_length);
		assert(s.length() == desired_length);
		return s;
	};

	// Split the string on the dot (if it has one)
	auto v = split_with_empties(fullname, '.');

	// append placeholders until our vector has two chunks
	while (v.size() < 2)
		v.emplace_back("");

	const auto name = truncate_or_pad(std::move(v[0]), DOS_MFNLENGTH);
	const auto ext  = truncate_or_pad(std::move(v[1]), DOS_EXTLENGTH);
	return {name, ext};
}

static void SaveFindResult(DOS_FCB& find_fcb)
{
	DOS_DTA::Result search_result = {};
	DOS_DTA find_dta(dos.tables.tempdta);
	find_dta.GetResult(search_result);
	const uint8_t drive = find_fcb.GetDrive() + 1u;

	FatAttributeFlags find_attr = {};
	find_attr.archive           = true;
	find_fcb.GetAttr(find_attr); /* Gets search attributes if extended */
	/* Create a correct file and extention */
	const auto [file_name, ext] = DTAExtendName(search_result.name.c_str());
	DOS_FCB fcb(RealSegment(dos.dta()),RealOffset(dos.dta()));//TODO
	fcb.Create(find_fcb.Extended());
	fcb.SetName(drive, file_name.c_str(), ext.c_str());
	fcb.SetAttr(find_attr);      /* Only adds attribute if fcb is extended */
	fcb.SetResult(search_result.size,
	              search_result.date,
	              search_result.time,
	              search_result.attr);
}

bool DOS_FCBCreate(uint16_t seg, uint16_t offset)
{
	DOS_FCB fcb(seg, offset);
	char shortname[DOS_FCBNAME];
	uint16_t handle;
	fcb.GetName(shortname);
	FatAttributeFlags attr = {};
	attr.archive           = true;
	fcb.GetAttr(attr);
	if (!attr._data) {
		attr.archive = true; // Better safe than sorry
	}
	if (!DOS_CreateFile(shortname, attr, &handle, true)) {
		return false;
	}
	fcb.FileOpen((uint8_t)handle);
	return true;
}

bool DOS_FCBOpen(uint16_t seg, uint16_t offset)
{
	DOS_FCB fcb(seg, offset);
	char shortname[DOS_FCBNAME];
	uint16_t handle;
	fcb.GetName(shortname);

	/* Search for file if name has wildcards */
	if (strpbrk(shortname,"*?")) {
		LOG(LOG_FCB,LOG_WARN)("Wildcards in filename");
		if (!DOS_FCBFindFirst(seg,offset)) return false;
		DOS_DTA find_dta(dos.tables.tempdta);
		DOS_FCB find_fcb(RealSegment(dos.tables.tempdta),RealOffset(dos.tables.tempdta));

		DOS_DTA::Result search_result = {};
		find_dta.GetResult(search_result);

		const auto [file_name,
		            ext] = DTAExtendName(search_result.name.c_str());
		find_fcb.SetName(fcb.GetDrive() + 1, file_name.c_str(), ext.c_str());
		find_fcb.GetName(shortname);
	}

	/* First check if the name is correct */
	char fullname[DOS_PATHLENGTH];
	uint8_t drive;
	if (!DOS_MakeName(shortname, fullname, &drive))
		return false;

	if (!DOS_OpenFile(shortname,OPEN_READWRITE,&handle,true)) return false;
	fcb.FileOpen((uint8_t)handle);
	return true;
}

bool DOS_FCBClose(uint16_t seg, uint16_t offset)
{
	DOS_FCB fcb(seg, offset);
	if (!fcb.Valid()) {
		return false;
	}
	uint8_t fhandle;
	fcb.FileClose(fhandle);
	DOS_CloseFile(fhandle,true);
	return true;
}

bool DOS_FCBFindFirst(uint16_t seg, uint16_t offset)
{
	DOS_FCB fcb(seg, offset);
	RealPt old_dta = dos.dta();
	dos.dta(dos.tables.tempdta);
	char name[DOS_FCBNAME];fcb.GetName(name);
	FatAttributeFlags attr = {FatAttributeFlags::Archive};
	fcb.GetAttr(attr); /* Gets search attributes if extended */
	bool ret=DOS_FindFirst(name,attr,true);
	dos.dta(old_dta);
	if (ret) SaveFindResult(fcb);
	return ret;
}

bool DOS_FCBFindNext(uint16_t seg, uint16_t offset)
{
	DOS_FCB fcb(seg, offset);
	RealPt old_dta = dos.dta();
	dos.dta(dos.tables.tempdta);
	bool ret=DOS_FindNext();
	dos.dta(old_dta);
	if (ret) SaveFindResult(fcb);
	return ret;
}

uint8_t DOS_FCBRead(uint16_t seg, uint16_t offset, uint16_t recno)
{
	DOS_FCB fcb(seg, offset);
	uint8_t fhandle, cur_rec;
	uint16_t cur_block, rec_size;
	fcb.GetSeqData(fhandle,rec_size);
	if (fhandle==0xff && rec_size!=0) {
		if (!DOS_FCBOpen(seg,offset)) return FCB_READ_NODATA;
		LOG(LOG_FCB,LOG_WARN)("Reopened closed FCB");
		fcb.GetSeqData(fhandle,rec_size);
	}
	if (rec_size == 0) {
		rec_size = 128;
		fcb.SetSeqData(fhandle,rec_size);
	}
	fcb.GetRecord(cur_block,cur_rec);
	uint32_t pos=((cur_block*128)+cur_rec)*rec_size;
	if (!DOS_SeekFile(fhandle,&pos,DOS_SEEK_SET,true)) return FCB_READ_NODATA; 
	uint16_t toread=rec_size;
	if (!DOS_ReadFile(fhandle,dos_copybuf,&toread,true)) return FCB_READ_NODATA;
	if (toread == 0)
		return FCB_READ_NODATA;
	if (toread < rec_size) { //Zero pad copybuffer to rec_size
		Bitu i = toread;
		while(i < rec_size) dos_copybuf[i++] = 0;
	}
	MEM_BlockWrite(RealToPhysical(dos.dta())+recno*rec_size,dos_copybuf,rec_size);
	if (++cur_rec>127) { cur_block++;cur_rec=0; }
	fcb.SetRecord(cur_block,cur_rec);
	if (toread==rec_size) return FCB_SUCCESS;
	return FCB_READ_PARTIAL;
}

uint8_t DOS_FCBWrite(uint16_t seg, uint16_t offset, uint16_t recno)
{
	DOS_FCB fcb(seg, offset);
	uint8_t fhandle, cur_rec;
	uint16_t cur_block, rec_size;
	fcb.GetSeqData(fhandle,rec_size);
	if (fhandle==0xff && rec_size!=0) {
		if (!DOS_FCBOpen(seg,offset)) return FCB_READ_NODATA;
		LOG(LOG_FCB,LOG_WARN)("Reopened closed FCB");
		fcb.GetSeqData(fhandle,rec_size);
	}
	if (rec_size == 0) {
		rec_size = 128;
		fcb.SetSeqData(fhandle,rec_size);
	}
	fcb.GetRecord(cur_block,cur_rec);
	uint32_t pos=((cur_block*128)+cur_rec)*rec_size;
	if (!DOS_SeekFile(fhandle,&pos,DOS_SEEK_SET,true)) return FCB_ERR_WRITE; 
	MEM_BlockRead(RealToPhysical(dos.dta())+recno*rec_size,dos_copybuf,rec_size);
	uint16_t towrite=rec_size;
	if (!DOS_WriteFile(fhandle,dos_copybuf,&towrite,true)) return FCB_ERR_WRITE;
	uint32_t size;uint16_t date,time;
	fcb.GetSizeDateTime(size,date,time);
	if (pos+towrite>size) size=pos+towrite;
	//time doesn't keep track of endofday
	date = DOS_GetBiosDatePacked();
	time = DOS_GetBiosTimePacked();

	assert(fhandle < DOS_FILES);
	Files[fhandle]->time = time;
	Files[fhandle]->date = date;
	fcb.SetSizeDateTime(size,date,time);
	if (++cur_rec>127) { cur_block++;cur_rec=0; }	
	fcb.SetRecord(cur_block,cur_rec);
	return FCB_SUCCESS;
}

uint8_t DOS_FCBIncreaseSize(uint16_t seg, uint16_t offset)
{
	DOS_FCB fcb(seg, offset);
	uint8_t fhandle, cur_rec;
	uint16_t cur_block, rec_size;
	fcb.GetSeqData(fhandle,rec_size);
	fcb.GetRecord(cur_block,cur_rec);
	uint32_t pos=((cur_block*128)+cur_rec)*rec_size;
	if (!DOS_SeekFile(fhandle,&pos,DOS_SEEK_SET,true)) return FCB_ERR_WRITE; 
	uint16_t towrite=0;
	if (!DOS_WriteFile(fhandle,dos_copybuf,&towrite,true)) return FCB_ERR_WRITE;
	uint32_t size;uint16_t date,time;
	fcb.GetSizeDateTime(size,date,time);
	if (pos+towrite>size) size=pos+towrite;
	//time doesn't keep track of endofday
	date = DOS_PackDate(dos.date.year,dos.date.month,dos.date.day);
	uint32_t ticks = mem_readd(BIOS_TIMER);
	uint32_t seconds = (ticks*10)/182;
	uint16_t hour = (uint16_t)(seconds/3600);
	uint16_t min = (uint16_t)((seconds % 3600)/60);
	uint16_t sec = (uint16_t)(seconds % 60);
	time = DOS_PackTime(hour,min,sec);
	Files[fhandle]->time = time;
	Files[fhandle]->date = date;
	fcb.SetSizeDateTime(size,date,time);
	fcb.SetRecord(cur_block,cur_rec);
	return FCB_SUCCESS;
}

uint8_t DOS_FCBRandomRead(uint16_t seg, uint16_t offset, uint16_t* numRec, bool restore)
{
	/* if restore is true :random read else random blok read.
	 * random read updates old block and old record to reflect the random
	 * data before the read!!!!!!!!! and the random data is not updated!
	 * (user must do this) Random block read updates these fields to reflect
	 * the state after the read!
	 */
	DOS_FCB fcb(seg, offset);
	uint16_t old_block=0;
	uint8_t old_rec=0;
	uint8_t error=0;
	uint16_t count;

	/* Set the correct record from the random data */
	const uint32_t random = fcb.GetRandom();
	fcb.SetRecord((uint16_t)(random / 128),(uint8_t)(random & 127));
	if (restore) fcb.GetRecord(old_block,old_rec);//store this for after the read.
	// Read records
	for (count=0; count<*numRec; count++) {
		error = DOS_FCBRead(seg,offset,count);
		if (error!=FCB_SUCCESS) break;
	}
	if (error==FCB_READ_PARTIAL) count++;	//partial read counts
	*numRec=count;
	uint16_t new_block;uint8_t new_rec;
	fcb.GetRecord(new_block,new_rec);
	if (restore) fcb.SetRecord(old_block,old_rec);
	/* Update the random record pointer with new position only when restore is false*/
	if(!restore) fcb.SetRandom(new_block*128+new_rec); 
	return error;
}

uint8_t DOS_FCBRandomWrite(uint16_t seg, uint16_t offset, uint16_t* numRec, bool restore)
{
	/* see FCB_RandomRead */
	DOS_FCB fcb(seg, offset);
	uint16_t old_block=0;
	uint8_t old_rec=0;
	uint8_t error=0;
	uint16_t count;

	/* Set the correct record from the random data */
	const uint32_t random = fcb.GetRandom();
	fcb.SetRecord((uint16_t)(random / 128),(uint8_t)(random & 127));
	if (restore) fcb.GetRecord(old_block,old_rec);
	if (*numRec > 0) {
		/* Write records */
		for (count=0; count<*numRec; count++) {
			error = DOS_FCBWrite(seg,offset,count);// dos_fcbwrite return 0 false when true...
			if (error!=FCB_SUCCESS) break;
		}
		*numRec=count;
	} else {
		DOS_FCBIncreaseSize(seg,offset);
	}
	uint16_t new_block;uint8_t new_rec;
	fcb.GetRecord(new_block,new_rec);
	if (restore) fcb.SetRecord(old_block,old_rec);
	/* Update the random record pointer with new position only when restore is false */
	if (!restore) fcb.SetRandom(new_block*128+new_rec); 
	return error;
}

bool DOS_FCBGetFileSize(uint16_t seg, uint16_t offset)
{
	char shortname[DOS_PATHLENGTH];
	uint16_t entry;
	DOS_FCB fcb(seg, offset);
	fcb.GetName(shortname);
	if (!DOS_OpenFile(shortname,OPEN_READ,&entry,true)) return false;
	uint32_t size = 0;
	Files[entry]->Seek(&size,DOS_SEEK_END);
	DOS_CloseFile(entry,true);

	uint8_t handle; uint16_t rec_size;
	fcb.GetSeqData(handle,rec_size);
	if (rec_size == 0) rec_size = 128; //Use default if missing.
	
	uint32_t random=(size/rec_size);
	if (size % rec_size) random++;
	fcb.SetRandom(random);
	return true;
}

bool DOS_FCBDeleteFile(uint16_t seg, uint16_t offset)
{
	/* FCB DELETE honours wildcards. it will return true if one or more
	 * files get deleted.
	 * To get this: the dta is set to temporary dta in which found files are
	 * stored. This can not be the tempdta as that one is used by
	 * fcbfindfirst
	 */
	RealPt old_dta = dos.dta();
	dos.dta(dos.tables.tempdta_fcbdelete);
	RealPt new_dta=dos.dta();
	bool nextfile = false;
	bool return_value = false;
	nextfile = DOS_FCBFindFirst(seg,offset);
	DOS_FCB fcb(RealSegment(new_dta),RealOffset(new_dta));
	while(nextfile) {
		char shortname[DOS_FCBNAME] = { 0 };
		fcb.GetName(shortname);
		bool res=DOS_UnlinkFile(shortname);
		if(!return_value && res) return_value = true; //at least one file deleted
		nextfile = DOS_FCBFindNext(seg,offset);
	}
	dos.dta(old_dta);  /*Restore dta */
	return return_value;
}

bool DOS_FCBRenameFile(uint16_t seg, uint16_t offset)
{
	DOS_FCB fcbold(seg, offset);
	DOS_FCB fcbnew(seg, offset + 16);
	if(!fcbold.Valid()) return false;
	char oldname[DOS_FCBNAME];
	char newname[DOS_FCBNAME];
	fcbold.GetName(oldname);fcbnew.GetName(newname);

	/* Check, if sourcefile is still open. This was possible in DOS, but modern oses don't like this */
	char fullname[DOS_PATHLENGTH];
	uint8_t drive;
	if (!DOS_MakeName(oldname, fullname, &drive))
		return false;

	DOS_PSP psp(dos.psp());
	for (uint8_t i = 0; i < DOS_FILES; ++i) {
		if (Files[i] && Files[i]->IsName(fullname)) {
			uint16_t handle = psp.FindEntryByHandle(i);
			//(more than once maybe)
			if (handle == 0xFF) {
				DOS_CloseFile(i,true);
			} else {
				DOS_CloseFile(handle);
			}
		}
	}

	/* Rename the file */
	return DOS_Rename(oldname,newname);
}

void DOS_FCBSetRandomRecord(uint16_t seg, uint16_t offset)
{
	DOS_FCB fcb(seg, offset);
	uint16_t block;
	uint8_t rec;
	fcb.GetRecord(block,rec);
	fcb.SetRandom(block*128+rec);
}

bool DOS_FileExists(const char* const name)
{
	char fullname[DOS_PATHLENGTH];uint8_t drive;
	if (!DOS_MakeName(name,fullname,&drive)) return false;
	return Drives.at(drive)->FileExists(fullname);
}

bool DOS_GetAllocationInfo(uint8_t drive, uint16_t* _bytes_sector,
                           uint8_t* _sectors_cluster, uint16_t* _total_clusters)
{
	if (!drive) {
		drive = DOS_GetDefaultDrive();
	} else {
		drive--;
	}
	if (drive >= DOS_DRIVES || !Drives[drive]) {
		DOS_SetError(DOSERR_INVALID_DRIVE);
		return false;
	}
	uint16_t _free_clusters;
	Drives[drive]->AllocationInfo(_bytes_sector,_sectors_cluster,_total_clusters,&_free_clusters);
	SegSet16(ds,RealSegment(dos.tables.mediaid));
	reg_bx=RealOffset(dos.tables.mediaid+drive*9);
	return true;
}

bool DOS_SetDrive(uint8_t drive)
{
	if (Drives.at(drive)) {
		DOS_SetDefaultDrive(drive);
		return true;
	} else {
		return false;
	}
}

bool DOS_GetFileDate(uint16_t entry, uint16_t* otime, uint16_t* odate)
{
	uint32_t handle = RealHandle(entry);
	if (handle >= DOS_FILES) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	};
	if (!Files[handle]) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	};
	// Only return local date/time fields.
	// MS-DOS does not read from disk on this call.
	// These are updated by calls to DOS_SetFile()
	// But are not updated by file writes.
	*otime = Files[handle]->time;
	*odate = Files[handle]->date;
	return true;
}

bool DOS_SetFileDate(uint16_t entry, uint16_t ntime, uint16_t ndate)
{
	const uint32_t handle = RealHandle(entry);
	if (handle >= DOS_FILES) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	}
	if (!Files[handle]) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	}
	if (Files[handle]->IsOnReadOnlyMedium()) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
	// Local date/time fields get modified here.
	// They do not get flushed to the disk until the file is closed.
	Files[handle]->time = ntime;
	Files[handle]->date = ndate;
	Files[handle]->flush_time_on_close = FlushTimeOnClose::ManuallySet;
	return true;
}

void DOS_SetupFiles()
{
	DOS_ClearDrivesAndFiles();

	const auto z_drive_index = drive_index('Z');

	const auto z_drive_ptr = std::make_shared<Virtual_Drive>();
	DriveManager::RegisterFilesystemImage(z_drive_index, z_drive_ptr);
	Drives.at(z_drive_index) = z_drive_ptr;
}

bool DOS_LockFile(const uint16_t entry, const uint32_t pos, const uint32_t len)
{
	const auto handle = RealHandle(entry);
	if (handle >= DOS_FILES) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	}
	if (!Files[handle]) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	}
	if (region_is_locked(handle, pos, len)) {
		DOS_SetError(DOSERR_LOCK_VIOLATION);
		return false;
	}
	FileRegionLock lock = {};
	lock.pos = pos;
	lock.len = len;
	Files[handle]->region_locks.push_back(lock);
	return true;
}

bool DOS_UnlockFile(const uint16_t entry, const uint32_t pos, const uint32_t len)
{
	const auto handle = RealHandle(entry);
	if (handle >= DOS_FILES) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	}
	if (!Files[handle]) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	}
	for (auto it = Files[handle]->region_locks.begin(); it != Files[handle]->region_locks.end(); ++it) {
		if (it->pos == pos && it->len == len) {
			Files[handle]->region_locks.erase(it);
			return true;
		}
	}
	DOS_SetError(DOSERR_LOCK_VIOLATION);
	return false;
}

void DOS_ClearDrivesAndFiles()
{
	// Clear all the DOS files. This calls the files' derived destructors,
	// which might be LocalFiles, OverlayFile, etc.
	for (auto& f : Files) {
		f = nullptr;
	}

	// Clear the shared drive pointers. The actual objects are managed by
	// the drive manager class.
	Drives.fill(nullptr);
}
