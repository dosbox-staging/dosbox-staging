/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
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

// Uncomment to enable file-open diagnostic messages
// #define DEBUG 1

#include "drives.h"

#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <limits>
#include <sys/types.h>

#ifdef _MSC_VER
#include <sys/utime.h>
#else
#include <utime.h>
#endif

#include "dos_inc.h"
#include "dos_mscdex.h"
#include "fs_utils.h"
#include "string_utils.h"
#include "cross.h"
#include "inout.h"

bool localDrive::FileIsReadOnly(const char* name)
{
	if (IsReadOnly()) {
		return true;
	}
	FatAttributeFlags test_attr = {};
	if (GetFileAttr(name, &test_attr)) {
		return test_attr.read_only;
	}
	return false;
}

bool localDrive::FileCreate(DOS_File** file, const char* name, FatAttributeFlags attributes)
{
	assert(!IsReadOnly());

	// Don't allow overwriting read-only files.
	if (FileIsReadOnly(name)) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	char newname[CROSS_LEN];
	safe_strcpy(newname, basedir);
	safe_strcat(newname, name);
	CROSS_FILENAME(newname);

	// GetExpandNameAndNormaliseCase returns a pointer to a static local
	// string. Make a copy to ensure it doesn't get overwritten by future
	// calls.
	char expanded_name[CROSS_LEN];
	safe_strcpy(expanded_name, dirCache.GetExpandNameAndNormaliseCase(newname));

	const bool file_exists = FileExists(expanded_name);

	attributes.archive = true;
	FILE* file_pointer = local_drive_create_file(expanded_name, attributes);

	if (!file_pointer) {
		LOG_MSG("Warning: file creation failed: %s", expanded_name);
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	if (!file_exists) {
		dirCache.AddEntry(newname, true);
	}

	// Make the 16 bit device information
	*file = new localFile(name, expanded_name, file_pointer, basedir, IsReadOnly());

	(*file)->flags = OPEN_READWRITE;

	return true;
}

// Inform the user that the file is being protected against modification.
// If the DOS program /really/ needs to write to the file, it will
// crash/exit and this will be one of the last messages on the screen,
// so the user can decide to un-write-protect the file if they wish.
// We only print one message per file to eliminate redundant messaging.
void localDrive::MaybeLogFilesystemProtection(const std::string& filename)
{
	const auto ret = write_protected_files.insert(filename);

	const bool was_inserted = ret.second;
	if (was_inserted) {
		// For brevity and clarity to the user, we show just the
		// filename instead of the more cluttered absolute path.
		LOG_MSG("FILESYSTEM: protected from modification: %s",
				get_basename(filename).c_str());
	}
}

// Search the Files[] inventory for an open file matching the requested
// local drive and file name. Returns nullptr if not found.
DOS_File *FindOpenFile(const DOS_Drive *drive, const char *name)
{
	if (!drive || !name)
		return nullptr;

	uint8_t drive_num = DOS_DRIVES; // default out range
	// Look for a matching drive mount
	for (uint8_t i = 0; i < DOS_DRIVES; ++i) {
		if (Drives[i] && Drives[i] == drive) {
			drive_num = i;
			break;
		}
	}
	if (drive_num == DOS_DRIVES) // still out of range, no matching mount
		return nullptr;

	// Look for a matching open filename on the same mount
	for (auto *file : Files)
		if (file && file->IsOpen() && file->GetDrive() == drive_num && file->IsName(name))
			return file; // drive + file path is unique

	return nullptr;
}

bool localDrive::FileOpen(DOS_File **file, const char *name, uint8_t flags)
{
	bool write_access = false;
	switch (flags & 0xf) {
		case OPEN_READ:
		//No modification of dates. LORD4.07 uses this
		case OPEN_READ_NO_MOD:
			write_access = false;
			break;
		case OPEN_WRITE:
		case OPEN_READWRITE:
			write_access = true;
			break;
		default:
			DOS_SetError(DOSERR_ACCESS_CODE_INVALID);
			return false;
	}

	const std::string host_filename = MapDosToHostFilename(name);
	bool fallback_to_readonly = false;

	// Don't allow opening read-only files in write mode,
	// unless configured otherwise
	if (write_access && FileIsReadOnly(name)) {
		if (always_open_ro_files) {
			flags = OPEN_READ;
			write_access = false;
			fallback_to_readonly = true;
		} else {
			MaybeLogFilesystemProtection(host_filename);
			DOS_SetError(DOSERR_ACCESS_DENIED);
			return false;
		}
	}

	// If the file's already open then flush it before continuing
	// (Betrayal in Antara)
	auto open_file = dynamic_cast<localFile *>(FindOpenFile(this, name));
	if (open_file) {
		open_file->Flush();
	}

	FILE* fhandle = fopen(host_filename.c_str(), write_access ? "rb+" : "rb");

	// If we couldn't open the file, then it's possible that
	// the file is simply write-protected and the flags requested
	// RW access.  So check if this is the case:
	if (!fhandle && write_access && always_open_ro_files) {
		// If yes, check if the file can be opened with Read-only access:
		fhandle = fopen(host_filename.c_str(), "rb");
		if (fhandle) {
			flags = OPEN_READ;
			fallback_to_readonly = true;
		}
	}

	if (!fhandle) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return false;
	}

	if (fallback_to_readonly) {
		MaybeLogFilesystemProtection(host_filename);
	}

	*file = new localFile(name, host_filename, fhandle, basedir, IsReadOnly());
	(*file)->flags = flags;  // for the inheritance flag and maybe check for others.

	return true;
}

FILE* localDrive::GetHostFilePtr(const char* const name, const char* const type)
{
	return fopen(MapDosToHostFilename(name).c_str(), type);
}

std::string localDrive::MapDosToHostFilename(const char* const dos_name)
{
	char newname[CROSS_LEN];
	safe_strcpy(newname, basedir);
	safe_strcat(newname, dos_name);
	CROSS_FILENAME(newname);
	return dirCache.GetExpandNameAndNormaliseCase(newname);
}

// Attempt to delete the file name from our local drive mount
bool localDrive::FileUnlink(const char* name)
{
	assert(!IsReadOnly());

	if (!FileExists(name)) {
		LOG_DEBUG("FS: Skipping removal of '%s' because it doesn't exist",
		          name);
		DOS_SetError(DOSERR_FILE_NOT_FOUND);
		return false;
	}

	// Don't allow deleting read-only files.
	if (FileIsReadOnly(name)) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	char newname[CROSS_LEN];
	safe_strcpy(newname, basedir);
	safe_strcat(newname, name);
	CROSS_FILENAME(newname);
	const char* fullname = dirCache.GetExpandNameAndNormaliseCase(newname);

	// Can we remove the file without issue?
	if (remove(fullname) == 0) {
		dirCache.DeleteEntry(newname);
		return true;
	}

	// Otherwise maybe the file's opened within our mount ...
	DOS_File *open_file = FindOpenFile(this, name);
	if (open_file) {
		size_t max = DOS_FILES;
		// then close and remove references (as many times as needed),
		while (open_file->IsOpen() && --max) {
			open_file->Close();
			if (open_file->RemoveRef() <= 0) {
				break;
			}
		}
		// and try removing it again.
		if (remove(fullname) == 0) {
			dirCache.DeleteEntry(newname);
			return true;
		}
	}
	LOG_DEBUG("FS: Unable to remove file '%s'", fullname);
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool localDrive::FindFirst(const char* _dir, DOS_DTA& dta, bool fcb_findfirst)
{
	char tempDir[CROSS_LEN];
	safe_strcpy(tempDir, basedir);
	safe_strcat(tempDir, _dir);
	CROSS_FILENAME(tempDir);

	if (allocation.mediaid == 0xF0) {
		EmptyCache(); //rescan floppie-content on each findfirst
	}

	// End the temp directory with a slash
	const auto temp_dir_len = strlen(tempDir);
	if (temp_dir_len < 1 || tempDir[temp_dir_len - 1] != CROSS_FILESPLIT) {
		constexpr char end[] = {CROSS_FILESPLIT, '\0'};
		safe_strcat(tempDir, end);
	}

	uint16_t id;
	if (!dirCache.FindFirst(tempDir,id)) {
		DOS_SetError(DOSERR_PATH_NOT_FOUND);
		return false;
	}
	safe_strcpy(srchInfo[id].srch_dir, tempDir);
	dta.SetDirID(id);

	FatAttributeFlags search_attr = {};
	dta.GetSearchParams(search_attr, tempDir);

	if (this->IsRemote() && this->IsRemovable()) {
		// cdroms behave a bit different than regular drives
		if (search_attr == FatAttributeFlags::Volume) {
			dta.SetResult(dirCache.GetLabel(), 0, 0, 0, FatAttributeFlags::Volume);
			return true;
		}
	} else {
		if (search_attr == FatAttributeFlags::Volume) {
			if (is_empty(dirCache.GetLabel())) {
				// LOG(LOG_DOSMISC,LOG_ERROR)("DRIVELABEL REQUESTED: none present, returned  NOLABEL");
				// dta.SetResult("NO_LABEL",0,0,0,FatAttributeFlags::Volume);
				// return true;
				DOS_SetError(DOSERR_NO_MORE_FILES);
				return false;
			}
			dta.SetResult(dirCache.GetLabel(), 0, 0, 0, FatAttributeFlags::Volume);
			return true;
		} else if (search_attr.volume && (*_dir == 0) && !fcb_findfirst) {
			// should check for a valid leading directory instead of
			// 0 exists==true if the volume label matches the
			// searchmask and the path is valid
			if (WildFileCmp(dirCache.GetLabel(), tempDir)) {
				dta.SetResult(dirCache.GetLabel(),
				              0,
				              0,
				              0,
				              FatAttributeFlags::Volume);
				return true;
			}
		}
	}
	return FindNext(dta);
}

bool localDrive::FindNext(DOS_DTA& dta)
{
	char* dir_ent;
	struct stat stat_block;
	char full_name[CROSS_LEN];
	char dir_entcopy[CROSS_LEN];

	FatAttributeFlags search_attr = {};
	char search_pattern[DOS_NAMELENGTH_ASCII];

	dta.GetSearchParams(search_attr, search_pattern);
	uint16_t id = dta.GetDirID();

	while (true) {
		if (!dirCache.FindNext(id, dir_ent)) {
			DOS_SetError(DOSERR_NO_MORE_FILES);
			return false;
		}
		if (!WildFileCmp(dir_ent, search_pattern)) {
			continue;
		}

		safe_strcpy(full_name, srchInfo[id].srch_dir);
		safe_strcat(full_name, dir_ent);

		// GetExpandNameAndNormaliseCase might indirectly destroy
		// dir_ent (by caching in a new directory and due to its design
		// dir_ent might be lost.) Copying dir_ent first
		safe_strcpy(dir_entcopy, dir_ent);
		const char* temp_name = dirCache.GetExpandNameAndNormaliseCase(
		        full_name);
		if (stat(temp_name, &stat_block) != 0) {
			continue; // No symlinks and such
		}

		if (is_hidden_by_host(temp_name)) {
			continue; // No host-only hidden files
		}

		FatAttributeFlags find_attr = {};
		if (DOSERR_NONE != local_drive_get_attributes(temp_name, find_attr)) {
			continue;
		}

		if ((find_attr.directory && !search_attr.directory) ||
		    (find_attr.hidden && !search_attr.hidden) ||
		    (find_attr.system && !search_attr.system)) {
			continue;
		}

		/*file is okay, setup everything to be copied in DTA Block */
		char find_name[DOS_NAMELENGTH_ASCII] = "";
		uint16_t find_date;
		uint16_t find_time;
		uint32_t find_size;

		if (safe_strlen(dir_entcopy) < DOS_NAMELENGTH_ASCII) {
			safe_strcpy(find_name, dir_entcopy);
			upcase(find_name);
		}

		find_size = (uint32_t)stat_block.st_size;
		struct tm datetime;
		if (cross::localtime_r(&stat_block.st_mtime, &datetime)) {
			find_date = DOS_PackDate(datetime);
			find_time = DOS_PackTime(datetime);
		} else {
			find_time = 6;
			find_date = 4;
		}
		dta.SetResult(find_name,
		              find_size,
		              find_date,
		              find_time,
		              find_attr._data);
		return true;
	}
	return false;
}

bool localDrive::GetFileAttr(const char* name, FatAttributeFlags* attr)
{
	if (local_drive_get_attributes(MapDosToHostFilename(name), *attr) != DOSERR_NONE) {
		// The caller is responsible to act accordingly, possibly
		// it should set DOS error code (setting it here is not allowed)
		*attr = 0;
		return false;
	}

	if (IsReadOnly()) {
		attr->read_only = true;
	}

	return true;
}

bool localDrive::SetFileAttr(const char* name, const FatAttributeFlags attr)
{
	assert(!IsReadOnly());
	const std::string host_filename = MapDosToHostFilename(name);

	const auto result = local_drive_set_attributes(host_filename, attr);
	dirCache.CacheOut(host_filename.c_str());

	if (result != DOSERR_NONE) {
		DOS_SetError(result);
		return false;
	}

	return true;
}

bool localDrive::MakeDir(const char* dir)
{
	assert(!IsReadOnly());

	char newdir[CROSS_LEN];
	safe_strcpy(newdir, basedir);
	safe_strcat(newdir, dir);
	CROSS_FILENAME(newdir);

	const auto result = local_drive_create_dir(
	        dirCache.GetExpandNameAndNormaliseCase(newdir));
	if (result == DOSERR_NONE) {
		dirCache.CacheOut(newdir, true);
	}
	return (result == DOSERR_NONE);
}

bool localDrive::RemoveDir(const char* dir)
{
	assert(!IsReadOnly());

	char newdir[CROSS_LEN];
	safe_strcpy(newdir, basedir);
	safe_strcat(newdir, dir);
	CROSS_FILENAME(newdir);
	int temp = rmdir(dirCache.GetExpandNameAndNormaliseCase(newdir));
	if (temp==0) dirCache.DeleteEntry(newdir,true);
	return (temp==0);
}

bool localDrive::TestDir(const char* dir)
{
	const std::string host_dir = MapDosToHostFilename(dir);
	// Skip directory test, if "\"
	if (!host_dir.empty() && host_dir.back() != '\\') {
		// It has to be a directory !
		struct stat test;
		if (stat(host_dir.c_str(), &test)) {
			return false;
		}
		if ((test.st_mode & S_IFDIR) == 0) {
			return false;
		}
	}
	return path_exists(host_dir);
}

bool localDrive::Rename(const char* oldname, const char* newname)
{
	assert(!IsReadOnly());
	const std::string old_host_filename = MapDosToHostFilename(oldname);

	char newnew[CROSS_LEN];
	safe_strcpy(newnew, basedir);
	safe_strcat(newnew, newname);
	CROSS_FILENAME(newnew);
	int temp = rename(old_host_filename.c_str(), dirCache.GetExpandNameAndNormaliseCase(newnew));
	if (temp == 0) {
		dirCache.CacheOut(newnew);
	}
	return temp == 0;
}

bool localDrive::AllocationInfo(uint16_t* _bytes_sector, uint8_t* _sectors_cluster,
                                uint16_t* _total_clusters, uint16_t* _free_clusters)
{
	*_bytes_sector    = allocation.bytes_sector;
	*_sectors_cluster = allocation.sectors_cluster;
	*_total_clusters  = allocation.total_clusters;
	*_free_clusters   = allocation.free_clusters;
	return true;
}

bool localDrive::FileExists(const char* name)
{
	const std::string host_filename = MapDosToHostFilename(name);
	struct stat temp_stat;
	if (stat(host_filename.c_str(), &temp_stat) != 0) {
		return false;
	}
	if (temp_stat.st_mode & S_IFDIR) {
		return false;
	}
	return true;
}

uint8_t localDrive::GetMediaByte(void)
{
	return allocation.mediaid;
}

bool localDrive::IsRemote(void)
{
	return false;
}

bool localDrive::IsRemovable(void)
{
	return false;
}

Bits localDrive::UnMount()
{
	return 0;
}

localDrive::localDrive(const char* startdir, uint16_t _bytes_sector,
                       uint8_t _sectors_cluster, uint16_t _total_clusters,
                       uint16_t _free_clusters, uint8_t _mediaid,
                       bool _readonly, bool _always_open_ro_files)
        : readonly(_readonly),
          always_open_ro_files(_always_open_ro_files),
          write_protected_files{},
          allocation{_bytes_sector, _sectors_cluster, _total_clusters, _free_clusters, _mediaid}
{
	type = DosDriveType::Local;
	safe_strcpy(basedir, startdir);
	safe_strcpy(info, startdir);
	dirCache.SetBaseDir(basedir);
}

// Updates the internal file's current position
bool localFile::ftell_and_check()
{
	if (!fhandle)
		return false;

	stream_pos = ftell(fhandle);
	if (stream_pos >= 0)
		return true;

	LOG_DEBUG("FS: Failed obtaining position in file '%s'", name.c_str());
	return false;
}

// Seeks the internal file to the specified position relative to whence
bool localFile::fseek_to_and_check(long pos, int whence)
{
	if (!fhandle)
		return false;

	if (fseek(fhandle, pos, whence) == 0) {
		stream_pos = pos;
		return true;
	}
	LOG_DEBUG("FS: Failed seeking to byte %ld in file '%s'",
	          stream_pos,
	          name.c_str());
	return false;
}

// Seeks the internal file to the internal position relative to whence
void localFile::fseek_and_check(int whence)
{
	static_cast<void>(fseek_to_and_check(stream_pos, whence));
}

//TODO Maybe use fflush, but that seemed to fuck up in visual c
bool localFile::Read(uint8_t *data, uint16_t *size)
{
	// check if the file is opened in write-only mode
	if ((this->flags & 0xf) == OPEN_WRITE) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	// Seek if we last wrote
	if (last_action == LastAction::Write)
		if (ftell_and_check())
			fseek_and_check(SEEK_SET);

	last_action = LastAction::Read;
	const auto requested = *size;
	const auto actual = static_cast<uint16_t>(fread(data, 1, requested, fhandle));
	*size = actual; // always save the actual

	if (actual != requested) {
		// LOG_DEBUG("FS: Only read %u of %u requested bytes from file '%s'",
		//           actual,
		//           requested,
		//           name.c_str());

		// Check for host read error
		if (ferror(fhandle)) {
			clearerr(fhandle);
			DOS_SetError(DOSERR_ACCESS_DENIED);
			return false;
		}
	}

	/* Fake harddrive motion. Inspector Gadget with Sound Blaster compatible */
	/* Same for Igor */
	/* hardrive motion => unmask irq 2. Only do it when it's masked as
	 * unmasking is realitively heavy to emulate */
	uint8_t mask = IO_Read(0x21);
	if (mask & 0x4)
		IO_Write(0x21, mask & 0xfb);
	return true;
}

bool localFile::Write(uint8_t *data, uint16_t *size)
{
	uint8_t lastflags = this->flags & 0xf;
	if (lastflags == OPEN_READ || lastflags == OPEN_READ_NO_MOD) {	// check if file opened in read-only mode
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	// File should always be opened in read-only mode if on read-only drive
	assert(!IsOnReadOnlyMedium());

	// Seek if we last read
	if (last_action == LastAction::Read)
		if (ftell_and_check())
			fseek_and_check(SEEK_SET);

	last_action = LastAction::Write;
	set_archive_on_close = true;

	// Truncate the file
	if (*size == 0) {
		const auto file = cross_fileno(fhandle);
		if (file == -1) {
			LOG_DEBUG("FS: Could not resolve file number for '%s'",
			          name.c_str());
			return false;
		}
		if (!ftell_and_check()) {
			return false;
		}
		if (ftruncate(file, stream_pos) != 0) {
			LOG_DEBUG("FS: Failed truncating file '%s'", name.c_str());
			return false;
		}
		// Truncation succeeded if we made it here
		return true;
	}

	// Otherwise we have some data to write
	const auto requested = *size;
	const auto actual = static_cast<uint16_t>(fwrite(data, 1, requested, fhandle));
	if (actual != requested) {
		LOG_DEBUG("FS: Only wrote %u of %u requested bytes to file '%s'",
		          actual,
		          requested,
		          name.c_str());

		// Check for host write error
		if (ferror(fhandle)) {
			clearerr(fhandle);
			DOS_SetError(DOSERR_ACCESS_DENIED);
			return false;
		}
	}
	*size = actual; // always save the actual
	return true;    // always return true, even if partially written
}

bool localFile::Seek(uint32_t *pos_addr, uint32_t type)
{
	int seektype;
	switch (type) {
	case DOS_SEEK_SET:seektype=SEEK_SET;break;
	case DOS_SEEK_CUR:seektype=SEEK_CUR;break;
	case DOS_SEEK_END:seektype=SEEK_END;break;
	default:
	//TODO Give some doserrorcode;
		return false;//ERROR
	}

	// The inbound position is actually an int32_t being passed through a
	// uint32_t* pointer (pos_addr), so reinterpret the underlying memory as
	// such to prevent rollover into the unsigned range.
	const auto pos = *reinterpret_cast<int32_t *>(pos_addr);
	if (!fseek_to_and_check(pos, seektype)) {
		// Failed to seek, but try again this time seeking to
		// the end of file, which satisfies Black Thorne.
		stream_pos = 0;
		fseek_and_check(SEEK_END);
	}
#if 0
	fpos_t temppos;
	fgetpos(fhandle,&temppos);
	uint32_t * fake_pos=(uint32_t*)&temppos;
	*pos_addr = *fake_pos;
#endif
	static_cast<void>(ftell_and_check());

	// The inbound position is actually an int32_t being passed through a
	// uint32_t* pointer (pos_addr), so before we save the seeked position
	// back into it we first ensure the current long stream_pos (which is a
	// signed 64-bit on some platforms + OSes) can fit within the int32_t
	// range before assigning it.
	assert(stream_pos >= std::numeric_limits<int32_t>::min() &&
	       stream_pos <= std::numeric_limits<int32_t>::max());
	*reinterpret_cast<int32_t *>(pos_addr) = static_cast<int32_t>(stream_pos);

	last_action = LastAction::None;
	return true;
}

bool localFile::Close()
{
	bool result = true;

	// only close if one reference left
	if (refCtr == 1) {
		if (set_archive_on_close) {
			assert(!IsOnReadOnlyMedium());
			FatAttributeFlags attributes = {};
			if (DOSERR_NONE !=
			    local_drive_get_attributes(path, attributes)) {
				result = false;
			} else if (!attributes.archive) {
				attributes.archive = true;
				if (DOSERR_NONE !=
				    local_drive_set_attributes(path, attributes)) {
					result = false;
				}
			}
			set_archive_on_close = false;
		}

		if (fhandle) {
			fclose(fhandle);
		}
		fhandle = nullptr;
		open = false;
	};

	if (newtime) {
		assert(!IsOnReadOnlyMedium());
		// backport from DOS_PackDate() and DOS_PackTime()
		struct tm tim = {};
		tim.tm_sec = (time & 0x1f) * 2;
		tim.tm_min = (time >> 5) & 0x3f;
		tim.tm_hour = (time >> 11) & 0x1f;
		tim.tm_mday = date & 0x1f;
		tim.tm_mon = ((date >> 5) & 0x0f) - 1;
		tim.tm_year = (date >> 9) + 1980 - 1900;
		//  have the C run-time library code compute whether standard
		//  time or daylight saving time is in effect.
		tim.tm_isdst = -1;
		// serialize time
		mktime(&tim);

		utimbuf ftim;
		ftim.actime = ftim.modtime = mktime(&tim);

		char fullname[CROSS_LEN];
		safe_sprintf(fullname, "%s%s", basedir, name.c_str());
		CROSS_FILENAME(fullname);

		// FIXME: utime is deprecated, need a modern cross-platform
		// implementation.
		if (utime(fullname, &ftim)) {
			result = false;
		}
	}

	return result;
}

uint16_t localFile::GetInformation(void)
{
	return read_only_medium ? 0x40 : 0;
}

localFile::localFile(const char* _name, const std_fs::path& path, FILE* handle,
                     const char* _basedir, const bool _read_only_medium)
        : fhandle(handle),
          path(path),
          basedir(_basedir),
          read_only_medium(_read_only_medium)
{
	open = true;
	localFile::UpdateDateTimeFromHost();
	attr = FatAttributeFlags::Archive;

	SetName(_name);
}

bool localFile::UpdateDateTimeFromHost()
{
	if (!open)
		return false;

	// Legal defaults if we're unable to populate them
	time = 1;
	date = 1;

	const auto file = cross_fileno(fhandle);
	if (file == -1)
		return true; // use defaults

	struct stat temp_stat;
	if (fstat(file, &temp_stat) == -1)
		return true; // use defaults

	struct tm datetime;
	if (!cross::localtime_r(&temp_stat.st_mtime, &datetime))
		return true; // use defaults

	time = DOS_PackTime(datetime);
	date = DOS_PackDate(datetime);
	return true;
}

void localFile::Flush()
{
	if (last_action != LastAction::Write)
		return;

	if (ftell_and_check())
		fseek_and_check(SEEK_SET);

	// Always reset the state even if the file is broken
	last_action = LastAction::None;
}

// ********************************************
// CDROM DRIVE
// ********************************************

cdromDrive::cdromDrive(const char _driveLetter,
                       const char * startdir,
                       uint16_t _bytes_sector,
                       uint8_t _sectors_cluster,
                       uint16_t _total_clusters,
                       uint16_t _free_clusters,
                       uint8_t _mediaid,
                       int& error)
	: localDrive(startdir,
	             _bytes_sector,
	             _sectors_cluster,
	             _total_clusters,
	             _free_clusters,
	             _mediaid,
	             true),
	  subUnit(0),
	  driveLetter(_driveLetter)
{
	// Init mscdex
	error = MSCDEX_AddDrive(driveLetter,startdir,subUnit);
	type  = DosDriveType::Cdrom;
	safe_strcpy(info, startdir);
	// Get Volume Label
	char name[32];
	if (MSCDEX_GetVolumeName(subUnit,name)) dirCache.SetLabel(name,true,true);
}

bool cdromDrive::FileOpen(DOS_File** file, const char* name, uint8_t flags)
{
	if ((flags & 0xf) == OPEN_READWRITE) {
		flags &= ~static_cast<unsigned>(OPEN_READWRITE);
	} else if ((flags & 0xf) == OPEN_WRITE) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}
	bool success = localDrive::FileOpen(file, name, flags);
	return success;
}

bool cdromDrive::FileCreate(DOS_File** /*file*/, const char* /*name*/,
                            FatAttributeFlags /*attributes*/)
{
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool cdromDrive::FileUnlink(const char* /*name*/)
{
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool cdromDrive::RemoveDir(const char* /*dir*/)
{
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool cdromDrive::MakeDir(const char* /*dir*/)
{
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool cdromDrive::Rename(const char* /*oldname*/, const char* /*newname*/)
{
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool cdromDrive::GetFileAttr(const char* name, FatAttributeFlags* attr)
{
	const bool result = localDrive::GetFileAttr(name, attr);
	if (result) {
		attr->archive   = false;
		attr->system    = false;
		attr->read_only = true;
	}
	return result;
}

bool cdromDrive::FindFirst(const char* _dir, DOS_DTA& dta, bool /*fcb_findfirst*/)
{
	// If media has changed, reInit drivecache.
	if (MSCDEX_HasMediaChanged(subUnit)) {
		dirCache.EmptyCache();
		// Get Volume Label
		char name[32];
		if (MSCDEX_GetVolumeName(subUnit,name)) dirCache.SetLabel(name,true,true);
	}
	return localDrive::FindFirst(_dir,dta);
}

void cdromDrive::SetDir(const char* path)
{
	// If media has changed, reInit drivecache.
	if (MSCDEX_HasMediaChanged(subUnit)) {
		dirCache.EmptyCache();
		// Get Volume Label
		char name[32];
		if (MSCDEX_GetVolumeName(subUnit,name)) dirCache.SetLabel(name,true,true);
	}
	localDrive::SetDir(path);
}

bool cdromDrive::IsRemote()
{
	return true;
}

bool cdromDrive::IsRemovable()
{
	return true;
}

Bits cdromDrive::UnMount()
{
	if (MSCDEX_RemoveDrive(driveLetter)) {
		return 0;
	}
	return 2;
}
