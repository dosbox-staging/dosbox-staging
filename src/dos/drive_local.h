// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_DRIVE_LOCAL_H
#define DOSBOX_DRIVE_LOCAL_H

#include "dos/dos_system.h"
#include "dos/drives.h"

class localFile : public DOS_File {
public:
	localFile(const char* name, const char* path,
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
	std::string GetPath() const
	{
		return path;
	}
	const std::weak_ptr<localDrive> local_drive = {};
	NativeFileHandle file_handle = InvalidNativeFileHandle;

private:
	void MaybeFlushTime();
	const std::string path = {};
	const char* basedir     = nullptr;

	const bool read_only_medium = false;
	bool set_archive_on_close   = false;
};

#endif // DOSBOX_DRIVE_LOCAL_H
