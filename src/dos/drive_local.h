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

#ifndef DOSBOX_DRIVE_LOCAL_H
#define DOSBOX_DRIVE_LOCAL_H

#include "dos_system.h"
#include "drives.h"

class localFile : public DOS_File {
public:
	localFile(const char* name, const std_fs::path& path,
	          const NativeFileHandle handle, const char* basedir,
	          const bool _read_only_medium, const std::weak_ptr<localDrive> drive,
	          const DosDateTime dos_time, const uint8_t _flags);
	localFile(const localFile&)            = delete; // prevent copying
	localFile& operator=(const localFile&) = delete; // prevent assignment
	~localFile() override;
	bool Read(uint8_t* data, uint16_t* size) override;
	bool Write(uint8_t* data, uint16_t* size) override;
	bool Seek(uint32_t* pos, uint32_t type) override;
	void Close() override;
	uint16_t GetInformation() override;
	bool IsOnReadOnlyMedium() const override { return read_only_medium; }
	const char* GetBaseDir() const
	{
		return basedir;
	}
	std_fs::path GetPath() const
	{
		return path;
	}
	const std::weak_ptr<localDrive> local_drive = {};
	NativeFileHandle file_handle = InvalidNativeFileHandle;

private:
	void MaybeFlushTime();
	const std_fs::path path = {};
	const char* basedir     = nullptr;

	const bool read_only_medium = false;
	bool set_archive_on_close   = false;
};

#endif
