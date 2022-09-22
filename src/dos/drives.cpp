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

#include <string_view>

#include "bios_disk.h"
#include "ide.h"
#include "string_utils.h"

extern char sfn[DOS_NAMELENGTH_ASCII];

// TODO Right now label formatting seems to be a bit of mess, with various
// places in code setting/expecting different format, so simple GetLabel() on
// a drive object might not yield an expected result. Not sure how to sort it
// out, but it will require some attention to detail.
// Also: this function is too strict - it removes all punctuation when *some*
// punctuation is acceptable in drive labels (e.g. '_' or '-').
//
std::string To_Label(const char* name) {
	// Reformat the name per the DOS label specification:
	// - Upper-case, up to 11 ASCII characters
	// - Internal spaces allowed but no: tabs ? / \ | . , ; : + = [ ] < > " '
	std::string label(name);
	trim(label); // strip front-and-back white-space
	strip_punctuation(label); // strip all punctuation
	label.resize(11); // collapse remainder to (at-most) 11 chars
	upcase(label);
	return label;
}

void Set_Label(char const * const input, char * const output, bool cdrom) {
	Bitu togo     = 8;
	Bitu vnamePos = 0;
	Bitu labelPos = 0;
	bool point    = false;

	//spacepadding the filenamepart to include spaces after the terminating zero is more closely to the specs. (not doing this now)
	// HELLO\0' '' '

	while (togo > 0) {
		if (input[vnamePos]==0) break;
		if (!point && (input[vnamePos]=='.')) {	togo=4; point=true; }

		//another mscdex quirk. Label is not always uppercase. (Daggerfall)
		output[labelPos] = (cdrom?input[vnamePos]:toupper(input[vnamePos]));

		labelPos++; vnamePos++;
		togo--;
		if ((togo==0) && !point) {
			if (input[vnamePos]=='.') vnamePos++;
			output[labelPos]='.'; labelPos++; point=true; togo=3;
		}
	};
	output[labelPos]=0;

	//Remove trailing dot. except when on cdrom and filename is exactly 8 (9 including the dot) letters. MSCDEX feature/bug (fifa96 cdrom detection)
	if((labelPos > 0) && (output[labelPos-1] == '.') && !(cdrom && labelPos ==9))
		output[labelPos-1] = 0;
}

constexpr bool is_special_character(const char c)
{
    constexpr auto special_characters = std::string_view("\"+=,;:<>[]|?*");
    return special_characters.find(c) != std::string_view::npos;
}

/* Generate 8.3 names from LFNs, with tilde usage (from ~1 to ~9999). */
std::string generate_8x3(const char *lfn, const unsigned int num, const bool start)
{
	unsigned int tilde_limit = 1000000;
	if (num >= tilde_limit)
		return "";
	static std::string result = "";
	std::string input = lfn;
	while (input.size() && (input[0] == '.' || input[0] == ' '))
		input.erase(input.begin());
	while (input.size() && (input.back() == '.' || input.back() == ' '))
		input.pop_back();
	size_t len = 0;
	auto found = input.rfind('.');
	unsigned int tilde_pos = 6 - (unsigned int)floor(log10(num));
	if (num == 1 || start) {
		result.clear();
		len = found != std::string::npos ? found : input.size();
		for (size_t i = 0; i < len; i++) {
			if (input[i] != ' ') {
				result += is_special_character(input[i])
				                  ? "_"
				                  : std::string(1, toupper(input[i]));
				if (result.size() >= tilde_pos)
					break;
			}
		}
	}
	result.erase(tilde_pos);
	result += '~' + std::to_string(num);
	if (found != std::string::npos) {
		input.erase(0, found + 1);
		size_t len_ext = 0;
		len = input.size();
		for (size_t i = 0; i < len; i++) {
			if (input[i] != ' ') {
				if (!len_ext)
					result += ".";
				result += is_special_character(input[i])
				                  ? "_"
				                  : std::string(1, toupper(input[i]));
				if (++len_ext >= 3)
					break;
			}
		}
	}
	return result;
}

bool filename_not_8x3(const char *n)
{
	unsigned int i;

	i = 0;
	while (*n != 0) {
		if (*n == '.')
			break;
		if ((*n & 0xFF) <= 32 || *n == 127 || is_special_character(*n))
			return true;
		i++;
		n++;
	}
	if (i > 8)
		return true;
	if (*n == 0)
		return false; /* made it past 8 or less normal chars and end of
		                 string: normal */

	/* skip dot */
	assert(*n == '.');
	n++;

	i = 0;
	while (*n != 0) {
		if (*n == '.')
			return true; /* another '.' means LFN */
		if ((*n & 0xFF) <= 32 || *n == 127 || is_special_character(*n))
			return true;
		i++;
		n++;
	}
	if (i > 3)
		return true;

	return false; /* it is 8.3 case */
}

/* Assuming an LFN call, if the name is not strict 8.3 uppercase, return true.
 * If the name is strict 8.3 uppercase like "FILENAME.TXT" there is no point
 * making an LFN because it is a waste of space */
bool filename_not_strict_8x3(const char *n)
{
	if (filename_not_8x3(n))
		return true;
	const auto len = strlen(n);
	for (unsigned int i = 0; i < len; i++)
		if (n[i] >= 'a' && n[i] <= 'z')
			return true;
	return false; /* it is strict 8.3 upper case */
}

DOS_Drive::DOS_Drive()
	: dirCache()
{
	curdir[0] = '\0';
	info[0] = '\0';
}

void DOS_Drive::SetDir(const char *path)
{
	safe_strcpy(curdir, path);
}

// static members variables
int DriveManager::currentDrive;
DriveManager::DriveInfo DriveManager::driveInfos[26];

void DriveManager::AppendDisk(int drive, DOS_Drive* disk) {
	driveInfos[drive].disks.push_back(disk);
}

void DriveManager::InitializeDrive(int drive) {
	currentDrive = drive;
	DriveInfo& driveInfo = driveInfos[currentDrive];
	if (driveInfo.disks.size() > 0) {
		driveInfo.currentDisk = 0;
		DOS_Drive* disk = driveInfo.disks[driveInfo.currentDisk];
		Drives[currentDrive] = disk;
		if (driveInfo.disks.size() > 1) disk->Activate();
	}
}

/*
void DriveManager::CycleDrive(bool pressed) {
	if (!pressed) return;
		
	// do one round through all drives or stop at the next drive with multiple disks
	int oldDrive = currentDrive;
	do {
		currentDrive = (currentDrive + 1) % DOS_DRIVES;
		int numDisks = driveInfos[currentDrive].disks.size();
		if (numDisks > 1) break;
	} while (currentDrive != oldDrive);
}

void DriveManager::CycleDisk(bool pressed) {
	if (!pressed) return;
	
	int numDisks = driveInfos[currentDrive].disks.size();
	if (numDisks > 1) {
		// cycle disk
		int currentDisk = driveInfos[currentDrive].currentDisk;
		DOS_Drive* oldDisk = driveInfos[currentDrive].disks[currentDisk];
		currentDisk = (currentDisk + 1) % numDisks;		
		DOS_Drive* newDisk = driveInfos[currentDrive].disks[currentDisk];
		driveInfos[currentDrive].currentDisk = currentDisk;
		
		// copy working directory, acquire system resources and finally switch to next drive		
		strcpy(newDisk->curdir, oldDisk->curdir);
		newDisk->Activate();
		Drives[currentDrive] = newDisk;
	}
}
*/

void DriveManager::CycleDisks(int requested_drive, bool notify)
{
	const auto drive = check_cast<int8_t>(requested_drive);

	int numDisks = (int)driveInfos[drive].disks.size();
	if (numDisks > 1) {
		// cycle disk
		int currentDisk = driveInfos[drive].currentDisk;

		// dettach CDROM from controller, if attached
		isoDrive *cdrom = dynamic_cast<isoDrive *>(Drives[drive]);
		int8_t index = -1;
		bool slave = false;
		if (cdrom)
			IDE_CDROM_Detach_Ret(index, slave, drive);

		DOS_Drive *oldDisk = driveInfos[drive].disks[currentDisk];
		currentDisk = (currentDisk + 1) % numDisks;
		DOS_Drive* newDisk = driveInfos[drive].disks[currentDisk];
		driveInfos[drive].currentDisk = currentDisk;
		if (drive < MAX_DISK_IMAGES && imageDiskList[drive] != nullptr) {
			std::string disk_info = newDisk->GetInfo();
			std::string type_fat = MSG_Get("MOUNT_TYPE_FAT");
			if (disk_info.substr(0, type_fat.size()) == type_fat) {
				imageDiskList[drive] = reinterpret_cast<fatDrive *>(newDisk)->loadedDisk;
			} else {
				imageDiskList[drive].reset(reinterpret_cast<imageDisk *>(newDisk));
			}
			if ((drive == 2 || drive == 3) && imageDiskList[drive]->hardDrive) {
				updateDPT();
			}
		}

		// copy working directory, acquire system resources and finally switch to next drive		
		strcpy(newDisk->curdir, oldDisk->curdir);
		newDisk->Activate();
		Drives[drive] = newDisk;

		// Re-attach the new drive to the controller
		if (cdrom && index > -1)
			IDE_CDROM_Attach(index, slave, drive);

		if (notify)
			LOG_MSG("Drive %c: disk %d of %d now active",
			        'A' + drive, currentDisk + 1, numDisks);
	}
}

void DriveManager::CycleAllDisks(void) {
	for (int idrive=0; idrive<DOS_DRIVES; idrive++) CycleDisks(idrive, true);
}

int DriveManager::UnmountDrive(int drive) {
	int result = 0;

	// dettach CDROM from controller, if attached
	isoDrive *cdrom = dynamic_cast<isoDrive *>(Drives[drive]);
	if (cdrom)
		IDE_CDROM_Detach(drive);

	// unmanaged drive
	if (driveInfos[drive].disks.size() == 0) {
		result = Drives[drive]->UnMount();
	} else {
		// managed drive
		int currentDisk = driveInfos[drive].currentDisk;
		result = driveInfos[drive].disks[currentDisk]->UnMount();
		// only delete on success, current disk set to NULL because of UnMount
		if (result == 0) {
			driveInfos[drive].disks[currentDisk] = NULL;
			for (int i = 0; i < (int)driveInfos[drive].disks.size(); i++) {
				delete driveInfos[drive].disks[i];
			}
			driveInfos[drive].disks.clear();
		}
	}
	return result;
}

char *DriveManager::GetDrivePosition(int drive)
{
	static char swap_position[10];
	safe_sprintf(swap_position, "%d / %d", driveInfos[drive].currentDisk + 1,
	             (int)driveInfos[drive].disks.size());
	return swap_position;
}

void DriveManager::Init(Section* /* sec */) {
	
	// setup driveInfos structure
	currentDrive = 0;
	for(int i = 0; i < DOS_DRIVES; i++) {
		driveInfos[i].currentDisk = 0;
	}

	// MAPPER_AddHandler(&CycleDisk, SDL_SCANCODE_F3, MMOD1,
	//                   "cycledisk", "Cycle Disk");
	// MAPPER_AddHandler(&CycleDrive, SDL_SCANCODE_F3, MMOD2,
	//                   "cycledrive", "Cycle Drv");
}

void DRIVES_Init(Section* sec) {
	DriveManager::Init(sec);
}
