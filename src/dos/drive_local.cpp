// SPDX-FileCopyrightText:  2020-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

// Uncomment to enable file-open diagnostic messages
// #define DEBUG 1

#include "dos/drives.h"
#include "drive_local.h"

#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <limits>
#include <sys/types.h>

#include "audio/disk_noise.h"
#include "dos.h"
#include "dos_mscdex.h"
#include "hardware/port.h"
#include "misc/cross.h"
#include "utils/fs_utils.h"
#include "utils/string_utils.h"

bool localDrive::FileIsReadOnly(const char* name)
{
	FatAttributeFlags test_attr = {};
	if (GetFileAttr(name, &test_attr)) {
		return test_attr.read_only;
	}
	return false;
}

bool localDrive::FileOrDriveIsReadOnly(const char* name)
{
	return IsReadOnly() || FileIsReadOnly(name);
}

std::unique_ptr<DOS_File> localDrive::FileCreate(const char* name,
                                                 FatAttributeFlags attributes)
{
	assert(!IsReadOnly());

	// Don't allow overwriting read-only files.
	if (FileOrDriveIsReadOnly(name)) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return nullptr;
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
	NativeFileHandle file_handle = create_native_file(expanded_name, attributes);

	if (file_handle == InvalidNativeFileHandle) {
		LOG_MSG("Warning: file creation failed: %s", expanded_name);
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return nullptr;
	}

	if (!file_exists) {
		dirCache.AddEntry(newname, true);
	}

	const DosDateTime dos_time = {
		.date = DOS_GetBiosDatePacked(),
		.time = DOS_GetBiosTimePacked()
	};

	// The timestamp cache is for emulating DOS behavior, not performance.
	// On some hosts, the timestamp will be updated with current host time
	// on write. DOS instead writes the timestamp on close. It also uses
	// internal BIOS time which can differ from host time. Ex. The "date"
	// command can set it to any arbitary date.
	timestamp_cache[expanded_name] = dos_time;

	// Make the 16 bit device information
	auto file = std::make_unique<localFile>(name,
	                                        expanded_name,
	                                        file_handle,
	                                        basedir,
	                                        IsReadOnly(),
	                                        weak_from_this(),
	                                        dos_time,
	                                        OPEN_READWRITE);

	return file;
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

std::unique_ptr<DOS_File> localDrive::FileOpen(const char* name, uint8_t flags)
{
	bool dos_write_access = false;
	switch (flags & 0xf) {
		case OPEN_READ:
		//No modification of dates. LORD4.07 uses this
		case OPEN_READ_NO_MOD:
			dos_write_access = false;
			break;
		case OPEN_WRITE:
		case OPEN_READWRITE:
			dos_write_access = true;
			break;
		default:
			DOS_SetError(DOSERR_ACCESS_CODE_INVALID);
		        return nullptr;
	}

	const std::string host_filename = MapDosToHostFilename(name);
	bool fallback_to_readonly = false;

	// Unless blocked, always try to open files in r/w mode on the host end
	// since setting modify date doesn't work in r/o mode on Windows but it
	// does on DOS. We already emulate read/write restrictions so it should
	// be fine.
	bool host_write_access = true;

	// Don't allow opening read-only files in write mode,
	// unless configured otherwise
	if (FileIsReadOnly(name)) {
		host_write_access = false;
		if (dos_write_access) {
			if (always_open_ro_files) {
				flags                = OPEN_READ;
				dos_write_access     = false;
				fallback_to_readonly = true;
			} else {
				MaybeLogFilesystemProtection(host_filename);
				DOS_SetError(DOSERR_ACCESS_DENIED);
				return nullptr;
			}
		}
	}

	// DOS allows opening files in r/w mode on a r/o medium just fine, there
	// won't be an error unless the app actually tries to write something.
	if (IsReadOnly()) {
		host_write_access = false;
		if (dos_write_access) {
			flags                = OPEN_READ;
			dos_write_access     = false;
			fallback_to_readonly = true;
		}
	}

	NativeFileHandle file_handle = open_native_file(host_filename.c_str(),
	                                                host_write_access);

	// If we couldn't open the file, then it's possible that
	// the underlying host file is simply write-protected and the flags
	// requested RW access. So check if this is the case:
	if (file_handle == InvalidNativeFileHandle && host_write_access) {
		// If yes, check if the file can be opened with Read-only access:
		file_handle = open_native_file(host_filename.c_str(), false);
		if (file_handle != InvalidNativeFileHandle && dos_write_access) {
			flags                = OPEN_READ;
			dos_write_access     = false;
			fallback_to_readonly = true;
		}
	}

	if (file_handle == InvalidNativeFileHandle) {
		DOS_SetError(DOSERR_INVALID_HANDLE);
		return nullptr;
	}

	if (fallback_to_readonly) {
		MaybeLogFilesystemProtection(host_filename);
	}

	DosDateTime dos_time   = {};
	const auto cache_entry = timestamp_cache.find(host_filename);
	if (cache_entry == timestamp_cache.end()) {
		// The first time a file gets opened, read the timestamp from
		// the host and add it to the cache. This gets "locked in" until
		// the file is closed. This handles the case where a single file
		// gets opened with multiple handles. The 2nd handle should not
		// see updated timestamps from writes to the 1st handle.
		dos_time                       = get_dos_file_time(file_handle);
		timestamp_cache[host_filename] = dos_time;
	} else {
		dos_time = cache_entry->second;
	}

	auto file = std::make_unique<localFile>(name,
	                                        host_filename.c_str(),
	                                        file_handle,
	                                        basedir,
	                                        IsReadOnly(),
	                                        weak_from_this(),
	                                        dos_time,
	                                        flags);

	return file;
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
	if (FileOrDriveIsReadOnly(name)) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	char newname[CROSS_LEN];
	safe_strcpy(newname, basedir);
	safe_strcat(newname, name);
	CROSS_FILENAME(newname);
	const char* fullname = dirCache.GetExpandNameAndNormaliseCase(newname);

	// Can we remove the file without issue?
	if (delete_native_file(fullname)) {
		timestamp_cache.erase(fullname);
		dirCache.DeleteEntry(newname);
		return true;
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
			if (wild_file_cmp(dirCache.GetLabel(), tempDir)) {
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
		if (!wild_file_cmp(dir_ent, search_pattern)) {
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
	if (local_drive_get_attributes(MapDosToHostFilename(name).c_str(), *attr) != DOSERR_NONE) {
		// The caller is responsible to act accordingly, possibly
		// it should set DOS error code (setting it here is not allowed)
		*attr = 0;
		return false;
	}

	return true;
}

bool localDrive::SetFileAttr(const char* name, const FatAttributeFlags attr)
{
	assert(!IsReadOnly());
	const std::string host_filename = MapDosToHostFilename(name);

	const auto result = local_drive_set_attributes(host_filename.c_str(), attr);
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

	const auto success = local_drive_remove_dir(dirCache.GetExpandNameAndNormaliseCase(newdir));
	if (success) {
		dirCache.DeleteEntry(newdir, true);
	}
	return success;
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
	return local_drive_path_exists(host_dir.c_str());
}

bool localDrive::Rename(const char* oldname, const char* newname)
{
	assert(!IsReadOnly());
	const std::string old_host_filename = MapDosToHostFilename(oldname);

	char newnew[CROSS_LEN];
	safe_strcpy(newnew, basedir);
	safe_strcat(newnew, newname);
	CROSS_FILENAME(newnew);
	const bool success = local_drive_rename_file_or_directory(old_host_filename.c_str(), dirCache.GetExpandNameAndNormaliseCase(newnew));
	if (success) {
		timestamp_cache.erase(old_host_filename);
		dirCache.CacheOut(newnew);
	}
	return success;
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

bool localFile::Read(uint8_t* data, uint16_t* num_bytes)
{
	assert(file_handle != InvalidNativeFileHandle);
	// check if the file is opened in write-only mode
	if ((this->flags & 0xf) == OPEN_WRITE) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	// Store last path to enable disk noise to choose sequential vs. random
	// access noises
	DiskNoises* disk_noises = DiskNoises::GetInstance();
	if (disk_noises != nullptr) {
		disk_noises->SetLastIoPath(path,
		                           DiskNoiseIoType::Read,
		                           DOS_GetDiskTypeFromMediaByte(
		                                   local_drive.lock()->GetMediaByte()));
	}

	const auto ret = read_native_file(file_handle, data, *num_bytes);
	*num_bytes     = check_cast<uint16_t>(ret.num_bytes);
	if (ret.error) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
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

bool localFile::Write(uint8_t* data, uint16_t* num_bytes)
{
	assert(file_handle != InvalidNativeFileHandle);
	uint8_t lastflags = this->flags & 0xf;
	if (lastflags == OPEN_READ || lastflags == OPEN_READ_NO_MOD) {	// check if file opened in read-only mode
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	// File should always be opened in read-only mode if on read-only drive
	assert(!IsOnReadOnlyMedium());

	set_archive_on_close = true;

	// Truncate the file
	if (*num_bytes == 0) {
		if (!truncate_native_file(file_handle)) {
			LOG_DEBUG("FS: Failed truncating file '%s'", name.c_str());
			return false;
		}
		// Truncation succeeded if we made it here
		return true;
	}

	// Store last path to enable disk noise to choose sequential vs. random
	// access noises
	DiskNoises* disk_noises = DiskNoises::GetInstance();
	if (disk_noises != nullptr) {
		disk_noises->SetLastIoPath(path,
		                           DiskNoiseIoType::Write,
		                           DOS_GetDiskTypeFromMediaByte(
		                                   local_drive.lock()->GetMediaByte()));
	}

	// Otherwise we have some data to write
	const auto ret = write_native_file(file_handle, data, *num_bytes);
	*num_bytes     = check_cast<uint16_t>(ret.num_bytes);
	if (ret.error) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	return true;
}

bool localFile::Seek(uint32_t *pos_addr, uint32_t type)
{
	assert(file_handle != InvalidNativeFileHandle);

	// Tested this interrupt on MS-DOS 6.22
	// The values for SEEK_CUR and SEEK_END can be negative
	// But some games/programs depend on the wrapping behavior of a 32-bit integer
	// Example: WinG installer for Windows 3.1
	// Wrapping a 32-bit signed is technically undefined in C
	// So just leave it unsigned and the math works out to be the same
	uint32_t seek_to = 0;
	switch (type) {
		case DOS_SEEK_SET: {
			seek_to = *pos_addr;
			break;
		}
		case DOS_SEEK_CUR: {
			const auto current_pos = get_native_file_position(file_handle);
			if (current_pos == NativeSeekFailed) {
				LOG_WARNING("FS: File seek failed for '%s'", path.c_str());
				DOS_SetError(DOSERR_ACCESS_DENIED);
				return false;
			}
			seek_to = check_cast<uint32_t>(current_pos) + *pos_addr;
			break;
		}
		case DOS_SEEK_END: {
			const auto end_pos = seek_native_file(file_handle, 0, NativeSeek::End);
			if (end_pos == NativeSeekFailed) {
				LOG_WARNING("FS: File seek failed for '%s'", path.c_str());
				DOS_SetError(DOSERR_ACCESS_DENIED);
				return false;
			}
			seek_to = check_cast<uint32_t>(end_pos) + *pos_addr;
			break;
		}
		default: {
			// DOS_SeekFile() should have already thrown this error
			assertm(false, "Invalid seek type");
			DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
			return false;
		}
	}

	// Always use NativeSeek::Set set since we calculate the absolute value above
	auto returned_pos = seek_native_file(file_handle, seek_to, NativeSeek::Set);

	if (returned_pos == NativeSeekFailed) {
		LOG_WARNING("FS: File seek failed for '%s'", path.c_str());
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return false;
	}

	// The returned value is always positive.
	// It can exceed 32-bit signed max (ex. Blackthorne)
	*pos_addr = check_cast<uint32_t>(returned_pos);

	return true;
}

void localFile::MaybeFlushTime()
{
	assert(file_handle != InvalidNativeFileHandle);
	DosDateTime dos_time = {};

	switch (flush_time_on_close) {
	case FlushTimeOnClose::NoUpdate: return;
	case FlushTimeOnClose::ManuallySet:
		dos_time.date = date;
		dos_time.time = time;
		break;
	case FlushTimeOnClose::CurrentTime:
		dos_time.date = DOS_GetBiosDatePacked();
		dos_time.time = DOS_GetBiosTimePacked();
		break;
	}

	assert(!IsOnReadOnlyMedium());
	set_dos_file_time(file_handle, dos_time.date, dos_time.time);

	// Using a weak_ptr here as we don't need to extend localDrive's
	// lifetime. All we're doing is updating the cache which has no effect
	// if there are no other references to the drive.
	const auto drive_ptr = local_drive.lock();
	if (drive_ptr) {
		auto& cache = drive_ptr->timestamp_cache;
		auto entry  = cache.find(path);
		if (entry != cache.end()) {
			// Only update the cache if it already contains
			// the entry. This handles an edge case where
			// the file has been unlinked while it is open.
			entry->second = dos_time;
		}
	}
}

void localFile::Close()
{
	assert(file_handle != InvalidNativeFileHandle);

	// only close if one reference left
	if (refCtr == 1) {
		if (set_archive_on_close) {
			assert(!IsOnReadOnlyMedium());
			FatAttributeFlags attributes = {};
			if (DOSERR_NONE ==
			    local_drive_get_attributes(path.c_str(), attributes)
				&& !attributes.archive) {
				attributes.archive = true;
				local_drive_set_attributes(path.c_str(), attributes);
			}
			set_archive_on_close = false;
		}

		// Not sure if setting extended attributes modifies the time.
		// Do it here to be safe even though it means typing this block twice.
		MaybeFlushTime();

		close_native_file(file_handle);
		file_handle = InvalidNativeFileHandle;
	} else {
		MaybeFlushTime();
	}
}

uint16_t localFile::GetInformation(void)
{
	return read_only_medium ? 0x40 : 0;
}

localFile::localFile(const char* _name, const char* path,
                     const NativeFileHandle handle, const char* _basedir,
                     const bool _read_only_medium,
                     const std::weak_ptr<localDrive> drive,
                     const DosDateTime dos_time, const uint8_t _flags)
        : local_drive(drive),
          file_handle(handle),
          path(path),
          basedir(_basedir),
          read_only_medium(_read_only_medium)
{
	assert(file_handle != InvalidNativeFileHandle);

	time = dos_time.time;
	date = dos_time.date;
	flags = _flags;

	attr = FatAttributeFlags::Archive;

	SetName(_name);
}

localFile::~localFile()
{
	// Make sure we close the host file handle and flush the timestamps.
	// This can happen if the user closes DOSBox while a game is running.
	// It can also happen if a game leaks file handles. Ex: Crystal Caves
	if (file_handle != InvalidNativeFileHandle) {
		// Release all references so the close function will close the host file
		refCtr = 1;

		// Make sure to avoid virtual dispatch inside a destructor
		localFile::Close();
	}
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

std::unique_ptr<DOS_File> cdromDrive::FileOpen(const char* name, uint8_t flags)
{
	if ((flags & 0xf) == OPEN_READWRITE) {
		flags &= ~static_cast<unsigned>(OPEN_READWRITE);
	} else if ((flags & 0xf) == OPEN_WRITE) {
		DOS_SetError(DOSERR_ACCESS_DENIED);
		return nullptr;
	}
	return localDrive::FileOpen(name, flags);
}

std::unique_ptr<DOS_File> cdromDrive::FileCreate([[maybe_unused]]const char* name,
                                                 [[maybe_unused]]FatAttributeFlags attributes)
{
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return nullptr;
}

bool cdromDrive::FileUnlink([[maybe_unused]]const char* name)
{
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool cdromDrive::RemoveDir([[maybe_unused]]const char* dir)
{
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool cdromDrive::MakeDir([[maybe_unused]]const char* dir)
{
	DOS_SetError(DOSERR_ACCESS_DENIED);
	return false;
}

bool cdromDrive::Rename([[maybe_unused]]const char* oldname, [[maybe_unused]]const char* newname)
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

bool cdromDrive::FindFirst(const char* _dir, DOS_DTA& dta, [[maybe_unused]]bool fcb_findfirst)
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
