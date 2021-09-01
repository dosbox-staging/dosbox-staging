#include "bios_disk.h"
#include "drives.h"
#include "program_mount_common.h"
#include "support.h"

Bitu ZDRIVE_NUM = 25;

const char *UnmountHelper(char umount)
{
	const char drive_id = toupper(umount);
	const bool using_drive_number = (drive_id >= '0' && drive_id <= '3');
	const bool using_drive_letter = (drive_id >= 'A' && drive_id <= 'Z');

	if (!using_drive_number && !using_drive_letter)
		return MSG_Get("PROGRAM_MOUNT_DRIVEID_ERROR");

	const uint8_t i_drive = using_drive_number ? (drive_id - '0')
	                                           : drive_index(drive_id);
	assert(i_drive < DOS_DRIVES);

	if (i_drive < MAX_DISK_IMAGES && Drives[i_drive] == NULL && !imageDiskList[i_drive])
		return MSG_Get("PROGRAM_MOUNT_UMOUNT_NOT_MOUNTED");

	if (i_drive >= MAX_DISK_IMAGES && Drives[i_drive] == NULL)
		return MSG_Get("PROGRAM_MOUNT_UMOUNT_NOT_MOUNTED");

	if (Drives[i_drive]) {
		switch (DriveManager::UnmountDrive(i_drive)) {
			case 1: return MSG_Get("PROGRAM_MOUNT_UMOUNT_NO_VIRTUAL");
			case 2: return MSG_Get("MSCDEX_ERROR_MULTIPLE_CDROMS");
		}
		Drives[i_drive] = 0;
		mem_writeb(Real2Phys(dos.tables.mediaid)+i_drive*9,0);
		if (i_drive == DOS_GetDefaultDrive()) {
			DOS_SetDrive(ZDRIVE_NUM);
		}

	}

	if (i_drive < MAX_DISK_IMAGES && imageDiskList[i_drive]) {
		imageDiskList[i_drive].reset();
	}

	return MSG_Get("PROGRAM_MOUNT_UMOUNT_SUCCESS");
}
