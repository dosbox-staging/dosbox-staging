/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2021-2025  The DOSBox Staging Team
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

#include "program_mount_common.h"

#include "bios_disk.h"
#include "drives.h"
#include "support.h"

Bitu ZDRIVE_NUM = 25;

const char* UnmountHelper(char umount)
{
	const char drive_id           = toupper(umount);
	const bool using_drive_number = (drive_id >= '0' && drive_id <= '3');
	const bool using_drive_letter = (drive_id >= 'A' && drive_id <= 'Z');

	if (!using_drive_number && !using_drive_letter) {
		return MSG_Get("PROGRAM_MOUNT_DRIVEID_ERROR");
	}

	const uint8_t i_drive = using_drive_number ? (drive_id - '0')
	                                           : drive_index(drive_id);
	assert(i_drive < DOS_DRIVES);

	if (i_drive < MAX_DISK_IMAGES && !Drives[i_drive] && !imageDiskList[i_drive]) {
		return MSG_Get("PROGRAM_MOUNT_UMOUNT_NOT_MOUNTED");
	}

	if (i_drive >= MAX_DISK_IMAGES && !Drives[i_drive]) {
		return MSG_Get("PROGRAM_MOUNT_UMOUNT_NOT_MOUNTED");
	}

	if (Drives[i_drive]) {
		switch (DriveManager::UnmountDrive(i_drive)) {
		case 1: return MSG_Get("PROGRAM_MOUNT_UMOUNT_NO_VIRTUAL");
		case 2: return MSG_Get("MSCDEX_ERROR_MULTIPLE_CDROMS");
		}
		Drives[i_drive] = nullptr;
		mem_writeb(RealToPhysical(dos.tables.mediaid) + i_drive * 9, 0);
		if (i_drive == DOS_GetDefaultDrive()) {
			DOS_SetDrive(ZDRIVE_NUM);
		}
	}

	if (i_drive < MAX_DISK_IMAGES && imageDiskList[i_drive]) {
		imageDiskList[i_drive] = nullptr;
	}

	return MSG_Get("PROGRAM_MOUNT_UMOUNT_SUCCESS");
}

void AddCommonMountMessages()
{
	if (MSG_Exists("MSCDEX_SUCCESS")) {
		// Avoid adding the same messages twice.
		return;
	}
	MSG_Add("MSCDEX_SUCCESS", "MSCDEX installed.\n\n");

	MSG_Add("MSCDEX_ERROR_MULTIPLE_CDROMS",
	        "MSCDEX: Failure: Drive-letters of multiple CD-ROM drives have to be continuous.\n\n");

	MSG_Add("MSCDEX_ERROR_NOT_SUPPORTED", "MSCDEX: Failure: Not yet supported.\n\n");

	MSG_Add("MSCDEX_ERROR_PATH",
	        "MSCDEX: Specified location is not a CD-ROM drive.\n\n");

	MSG_Add("MSCDEX_ERROR_OPEN",
	        "MSCDEX: Failure: Invalid file or unable to open.\n\n");

	MSG_Add("MSCDEX_TOO_MANY_DRIVES",
	        "MSCDEX: Failure: Too many CD-ROM drives (max: 5). MSCDEX Installation failed.\n\n");

	MSG_Add("MSCDEX_LIMITED_SUPPORT",
	        "MSCDEX: Mounted subdirectory: limited support.\n\n");

	MSG_Add("MSCDEX_INVALID_FILEFORMAT",
	        "MSCDEX: Failure: File is either no ISO/CUE image or contains errors.\n\n");

	MSG_Add("MSCDEX_UNKNOWN_ERROR", "MSCDEX: Failure: Unknown error.\n\n");

	MSG_Add("MSCDEX_WARNING_NO_OPTION",
	        "MSCDEX: Warning: Ignoring unsupported option '%s'.\n\n");

	MSG_Add("PROGRAM_MOUNT_STATUS_DRIVE", "Drive");
	MSG_Add("PROGRAM_MOUNT_STATUS_TYPE", "Type");
	MSG_Add("PROGRAM_MOUNT_STATUS_LABEL", "Label");
	MSG_Add("PROGRAM_MOUNT_STATUS_NAME", "Image name");
	MSG_Add("PROGRAM_MOUNT_STATUS_SLOT", "Swap slot");

	MSG_Add("PROGRAM_MOUNT_STATUS_1", "The currently mounted drives are:\n");
	MSG_Add("PROGRAM_MOUNT_STATUS_2", "%s mounted as %c drive\n");
	MSG_Add("PROGRAM_MOUNT_READONLY", "Mounted read-only\n");
}

void AddMountTypeMessages()
{
	MSG_Add("MOUNT_TYPE_LOCAL_DIRECTORY", "Local directory");
	MSG_Add("MOUNT_TYPE_CDROM", "CD-ROM drive");
	MSG_Add("MOUNT_TYPE_FAT", "FAT image");
	MSG_Add("MOUNT_TYPE_ISO", "ISO image");
	MSG_Add("MOUNT_TYPE_VIRTUAL", "Internal virtual drive");
	MSG_Add("MOUNT_TYPE_UNKNOWN", "unknown drive");
}
