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

void Set_Label(const char* const input, char* const output, bool cdrom)
{
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
uint8_t DriveManager::currentDrive                       = 0;
DriveManager::drive_infos_t DriveManager::drive_infos    = {};
DriveManager::raw_images_t DriveManager::indexed_images  = {};
DriveManager::raw_images_t DriveManager::raw_floppy_images  = {};

DOS_Drive* DriveManager::RegisterFilesystemImage(const int drive,
                                                 std::unique_ptr<DOS_Drive>&& image)
{
	auto& disks = drive_infos.at(drive).disks;
	disks.clear();
	return disks.emplace_back(std::move(image)).get();
}

std::vector<DOS_Drive*> DriveManager::AppendFilesystemImages(const int drive,
                                                             filesystem_images_t& images)
{
	std::vector<DOS_Drive*> pointers = {};

	auto& disks = drive_infos.at(drive).disks;

	// Move all the source images into the drive's disk vector, and add the
	// resulting pointers back to our return vector.
	for (auto& image : images) {
		pointers.emplace_back(disks.emplace_back(std::move(image)).get());
	}

	images.clear();
	return pointers;
}

imageDisk* DriveManager::RegisterNumberedImage(FILE* img_file,
                                               const std::string& img_name,
                                               const uint32_t img_size_kb,
                                               const bool is_hdd)
{
	auto image = std::make_unique<imageDisk>(img_file,
	                                         img_name.c_str(),
	                                         img_size_kb,
	                                         is_hdd);

	return indexed_images.emplace_back(std::move(image)).get();
}

void DriveManager::CloseNumberedImage(const imageDisk* image_ptr)
{
	auto matcher = [image_ptr](const std::unique_ptr<imageDisk>& image) {
		return image.get() == image_ptr;
	};

	const auto matching_images = std::remove_if(indexed_images.begin(),
	                                            indexed_images.end(),
	                                            matcher);

	indexed_images.erase(matching_images, indexed_images.end());
}

imageDisk* DriveManager::RegisterRawFloppyImage(FILE* img_file,
                                                const std::string& img_name,
                                                const uint32_t img_size_kb)
{
	constexpr bool is_hdd = false;

	auto image = std::make_unique<imageDisk>(img_file,
	                                         img_name.c_str(),
	                                         img_size_kb,
	                                         is_hdd);

	return raw_floppy_images.emplace_back(std::move(image)).get();
}

void DriveManager::CloseRawFddImages()
{
	raw_floppy_images.clear();
}

void DriveManager::InitializeDrive(int drive) {
	currentDrive    = check_cast<uint8_t>(drive);
	auto& drive_info = drive_infos.at(currentDrive);
	if (!drive_info.disks.empty()) {
		drive_info.current_disk = 0;
		const auto disk_pointer =
		        drive_info.disks[drive_info.current_disk].get();
		Drives.at(currentDrive) = disk_pointer;
		if (disk_pointer && drive_info.disks.size() > 1) {
			disk_pointer->Activate();
		}
	}
}

void DriveManager::CycleDisks(int requested_drive, bool notify)
{
	const auto drive = check_cast<int8_t>(requested_drive);

	// short-hand reference
	auto& drive_info = drive_infos.at(drive);

	auto num_disks = check_cast<int>(drive_info.disks.size());
	if (num_disks > 1) {
		// cycle disk
		int current_disk = drive_info.current_disk;

		// dettach CDROM from controller, if attached
		const auto is_cdrom = Drives.at(drive) &&
		                      (Drives.at(drive)->GetType() ==
		                       DosDriveType::Iso);
		int8_t index = -1;
		bool slave = false;
		if (is_cdrom) {
			IDE_CDROM_Detach_Ret(index, slave, drive);
		}

		const auto old_disk     = drive_info.disks[current_disk].get();
		current_disk            = (current_disk + 1) % num_disks;
		const auto new_disk     = drive_info.disks[current_disk].get();
		drive_info.current_disk = current_disk;
		if (drive < MAX_DISK_IMAGES && imageDiskList.at(drive)) {
			if (new_disk && new_disk->GetType() == DosDriveType::Fat) {
				const auto fat_drive = dynamic_cast<fatDrive*>(new_disk);
				imageDiskList[drive] = fat_drive->loadedDisk.get();
			} else {
				imageDiskList[drive] = dynamic_cast<imageDisk*>(
				        new_disk);
			}
			if ((drive == 2 || drive == 3) && imageDiskList[drive] && imageDiskList[drive]->hardDrive) {
				updateDPT();
			}
		}

		// copy working directory, acquire system resources and finally switch to next drive
		if (new_disk && old_disk) {
			strcpy(new_disk->curdir, old_disk->curdir);
			new_disk->Activate();
		}
		Drives.at(drive) = new_disk;

		// Re-attach the new drive to the controller
		if (is_cdrom && index > -1) {
			IDE_CDROM_Attach(index, slave, drive);
		}

		if (notify)
			LOG_MSG("Drive %c: disk %d of %d now active",
			        'A' + drive,
			        current_disk + 1,
			        num_disks);
	}
}

void DriveManager::CycleAllDisks(void) {
	for (int idrive=0; idrive<DOS_DRIVES; idrive++) CycleDisks(idrive, true);
}

int DriveManager::UnmountDrive(int drive) {
	int result = 0;

	// dettach CDROM from controller, if attached
	const auto is_cdrom = Drives.at(drive) &&
	                      (Drives[drive]->GetType() == DosDriveType::Iso);
	if (is_cdrom) {
		IDE_CDROM_Detach(drive);
	}

	// short-hand reference
	auto& drive_info = drive_infos.at(drive);

	// unmanaged drive
	if (drive_info.disks.size() == 0) {
		result = Drives.at(drive)->UnMount();
	} else {
		// managed drive
		int current_disk = drive_info.current_disk;
		result           = drive_info.disks[current_disk]->UnMount();
	}
	// only clear on success
	if (result == 0) {
		drive_infos.at(drive) = {};
		Drives.at(drive)      = nullptr;
	}
	return result;
}

char *DriveManager::GetDrivePosition(int drive)
{
	static char swap_position[10];
	const auto& drive_info = drive_infos.at(drive);
	safe_sprintf(swap_position,
	             "%d / %d",
	             drive_info.current_disk + 1,
	             (int)drive_info.disks.size());
	return swap_position;
}

void DriveManager::Init(Section* /* sec */) {
	// setup drive_infos structure
	currentDrive = 0;
	for(int i = 0; i < DOS_DRIVES; i++) {
		drive_infos.at(i).current_disk = 0;
	}

	// MAPPER_AddHandler(&CycleDisk, SDL_SCANCODE_F3, MMOD1,
	//                   "cycledisk", "Cycle Disk");
	// MAPPER_AddHandler(&CycleDrive, SDL_SCANCODE_F3, MMOD2,
	//                   "cycledrive", "Cycle Drv");
}

void DRIVES_Init(Section* sec) {
	DriveManager::Init(sec);
}
