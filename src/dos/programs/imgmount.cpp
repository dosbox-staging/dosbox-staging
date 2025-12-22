// SPDX-FileCopyrightText:  2021-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "imgmount.h"

#include "dosbox.h"

#include <vector>

#include "ints/bios_disk.h"
#include "config/config.h"
#include "misc/cross.h"
#include "dos/cdrom.h"
#include "dos/drives.h"
#include "utils/fs_utils.h"
#include "hardware/ide.h"
#include "ints/int10.h"
#include "gui/mapper.h"
#include "more_output.h"
#include "mount_common.h"
#include "shell/shell.h"
#include "utils/string_utils.h"

void IMGMOUNT::ListImgMounts(void)
{
	const std::string header_drive = MSG_Get("PROGRAM_MOUNT_STATUS_DRIVE");
	const std::string header_name  = MSG_Get("PROGRAM_MOUNT_STATUS_NAME");
	const std::string header_label = MSG_Get("PROGRAM_MOUNT_STATUS_LABEL");
	const std::string header_slot  = MSG_Get("PROGRAM_MOUNT_STATUS_SLOT");

	const int term_width   = real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS);
	const auto width_drive = static_cast<int>(header_drive.length());
	const auto width_label = std::max(minimum_column_length,
	                                  static_cast<int>(header_label.length()));
	const auto width_slot  = std::max(minimum_column_length,
                                         static_cast<int>(header_slot.length()));
	const auto width_name = term_width - 4 - width_drive - width_label - width_slot;
	if (width_name < 0) {
		LOG_WARNING("Message is too long.");
		return;
	}

	auto print_row = [&](const std::string& txt_drive,
	                     const std::string& txt_name,
	                     const std::string& txt_label,
	                     const std::string& txt_slot) {
		WriteOut("%-*s %-*s %-*s %-*s\n",
		         width_drive,
		         txt_drive.c_str(),
		         width_name,
		         txt_name.c_str(),
		         width_label,
		         txt_label.c_str(),
		         width_slot,
		         txt_slot.c_str());
	};

	WriteOut(MSG_Get("PROGRAM_MOUNT_STATUS_1"));
	print_row(header_drive, header_name, header_label, header_slot);
	const std::string horizontal_divider(term_width, '-');
	WriteOut_NoParsing(horizontal_divider);

	bool found_drives = false;
	for (uint8_t d = 0; d < DOS_DRIVES; d++) {
		if (!Drives[d]) {
			continue;
		}
		if (Drives[d]->GetType() == DosDriveType::Fat ||
		    Drives[d]->GetType() == DosDriveType::Iso) {
			print_row(std::string{drive_letter(d)},
			          Drives[d]->GetInfo(),
			          To_Label(Drives[d]->GetLabel()),
			          DriveManager::GetDrivePosition(d));
			found_drives = true;
		}
	}
	if (!found_drives) {
		WriteOut(MSG_Get("PROGRAM_IMGMOUNT_STATUS_NONE"));
	}
}

// A helper function that expands wildcard paths from the given argument and
// adds them to the given paths. Returns true if the expansion succeeded.
static bool add_wildcard_paths(const std::string& path_arg,
                               std::vector<std::string>& paths)
{
	// Expand the given path argument
	constexpr auto only_expand_files = true;
	constexpr auto skip_native_path  = true;
	std::vector<std::string> expanded_paths = {}; // filled by get_expanded_files
	if (!get_expanded_files(path_arg, expanded_paths, only_expand_files, skip_native_path)) {
		return false;
	}

	// Sort wildcards with natural ordering
	constexpr auto npos      = std::string::npos;
	const auto has_wildcards = path_arg.find_first_of('*') != npos ||
	                           path_arg.find_first_of('?') != npos;
	if (has_wildcards) {
		std::sort(expanded_paths.begin(), expanded_paths.end(), natural_compare);
	}

	// Move the expanded paths out
	paths.insert(paths.end(), //-V823
	             std::make_move_iterator(expanded_paths.begin()),
	             std::make_move_iterator(expanded_paths.end()));

	return true;
}

// This function desparately needs to be refactored into type-specific mounters
void IMGMOUNT::Run(void)
{
	// Hack To allow long commandlines
	ChangeToLongCmd();

	if (!cmd->GetCount()) {
		ListImgMounts();
		return;
	}
	// Usage
	if (HelpRequested()) {
		MoreOutputStrings output(*this);
		output.AddString(MSG_Get("PROGRAM_IMGMOUNT_HELP_LONG"),
		                 PRIMARY_MOD_NAME);
#if defined(WIN32)
		output.AddString(MSG_Get("PROGRAM_IMGMOUNT_HELP_LONG_WIN32"));
#elif defined(MACOSX)
		output.AddString(MSG_Get("PROGRAM_IMGMOUNT_HELP_LONG_MACOSX"));
#else
		output.AddString(MSG_Get("PROGRAM_IMGMOUNT_HELP_LONG_OTHER"));
#endif
		output.AddString(MSG_Get("PROGRAM_IMGMOUNT_HELP_LONG_GENERIC"));

		output.Display();
		return;
	}

	/* In secure mode don't allow people to change imgmount points.
	 * Neither mount nor unmount */
	if (control->SecureMode()) {
		WriteOut(MSG_Get("PROGRAM_CONFIG_SECURE_DISALLOW"));
		return;
	}

	char drive;
	std::vector<std::string> paths;
	std::string umount;
	/* Check for unmounting */
	if (cmd->FindString("-u", umount, false)) {
		WriteOut(UnmountHelper(umount[0]), toupper(umount[0]));
		return;
	}

	std::string type   = "hdd";
	std::string fstype = "fat";
	cmd->FindString("-t", type, true);
	cmd->FindString("-fs", fstype, true);

	bool roflag = false;
	if (cmd->FindExist("-ro", true)) {
		roflag = true;
	}

	// Types 'cdrom' and 'iso' are synonyms. Name 'cdrom' is easier
	// to remember and makes more sense, while name 'iso' is
	// required for backwards compatibility and for users conflating
	// -fs and -t parameters.
	if (type == "cdrom") {
		type = "iso";
	}

	if (type != "floppy" && type != "hdd" && type != "iso") {
		WriteOut(MSG_Get("PROGRAM_IMGMOUNT_TYPE_UNSUPPORTED"), type.c_str());
		return;
	}

	uint16_t sizes[4]  = {0};
	bool imgsizedetect = false;
	const uint8_t mediaid = (type == "floppy") ? 0xF0 : 0xF8; // all others

	std::string str_size = "";

	// Possibly used to hold the IDE channel and drive slot for CDROM types
	std::string ide_value     = {};
	int8_t ide_index          = -1;
	bool is_second_cable_slot = false;
	const bool wants_ide      = cmd->FindString("-ide", ide_value, true) ||
	                       cmd->FindExist("-ide", true);

	if (type == "iso") {
		// str_size="2048,1,65535,0";	// ignored, see drive_iso.cpp
		// (AllocationInfo)
		fstype = "iso";
		if (wants_ide) {
			IDE_Get_Next_Cable_Slot(ide_index, is_second_cable_slot);
		}
	}

	cmd->FindString("-size", str_size, true);
	if ((type == "hdd") && (str_size.size() == 0)) {
		imgsizedetect = true;
	} else {
		char number[21]  = {0};
		const char* scan = str_size.c_str();
		Bitu index       = 0;
		Bitu count       = 0;
		/* Parse the str_size string */
		while (*scan && index < 20 && count < 4) {
			if (*scan == ',') {
				number[index]  = 0;
				sizes[count++] = atoi(number);
				index          = 0;
			} else {
				number[index++] = *scan;
			}
			scan++;
		}
		if (count < 4) {
			number[index] = 0; // always goes correct as index is
			                   // max 20 at this point.
			sizes[count] = atoi(number);
		}
	}

	if (fstype == "fat" || fstype == "iso") {
		// get the drive letter
		if (!cmd->FindCommand(1, temp_line) || (temp_line.size() > 2) ||
		    ((temp_line.size() > 1) && (temp_line[1] != ':'))) {
			WriteOut_NoParsing(MSG_Get("PROGRAM_IMGMOUNT_SPECIFY_DRIVE"));
			return;
		}
		const int i_drive = toupper(temp_line[0]);
		if (i_drive < 'A' || i_drive > 'Z') {
			WriteOut_NoParsing(MSG_Get("PROGRAM_IMGMOUNT_SPECIFY_DRIVE"));
			return;
		}
		drive = int_to_char(i_drive);
	} else if (fstype == "none") {
		if (!cmd->FindCommand(1, temp_line) || temp_line.size() > 1 ||
		    !isdigit(temp_line[0])) {
			WriteOut_NoParsing(MSG_Get("PROGRAM_IMGMOUNT_SPECIFY2"));
			return;
		}
		drive = temp_line[0];
		if ((drive < '0') || (drive >= (MAX_DISK_IMAGES + '0'))) {
			WriteOut_NoParsing(MSG_Get("PROGRAM_IMGMOUNT_SPECIFY2"));
			return;
		}
	} else {
		WriteOut(MSG_Get("PROGRAM_IMGMOUNT_FORMAT_UNSUPPORTED"),
		         fstype.c_str());
		return;
	}
	cmd->Shift(); // consume the drive letter portion

	// find all file parameters, assuming that all option parameters have
	// been removed
	uint16_t arg_pos = 1;
	while (cmd->FindCommand(arg_pos++, temp_line)) {
		// Try to find the path on native filesystem first
		assert(!temp_line.empty());
		const auto real_path = to_native_path(temp_line);
		if (real_path.empty()) {
			if (add_wildcard_paths(temp_line, paths)) {
				continue;
			} else {
				LOG_MSG("IMGMOUNT: Path '%s' not found, maybe it's a DOS path",
				        temp_line.c_str());
			}
		} else {
			const auto home_dir = resolve_home(temp_line).string();
			if (home_dir == real_path) {
				LOG_MSG("IMGMOUNT: Path '%s' found",
				        temp_line.c_str());
			} else {
				LOG_MSG("IMGMOUNT: Path '%s' found, while looking for '%s'",
				        real_path.c_str(),
				        temp_line.c_str());
			}
			temp_line = real_path;
		}

		// Test if input is file on virtual DOS drive.
		if (!path_exists(temp_line)) {
			// See if it works if the ~ are written out
			const auto home_dir = resolve_home(temp_line).string();
			if (path_exists(home_dir)) {
				temp_line = home_dir;
			} else {
				// convert dosbox filename to system filename
				char fullname[CROSS_LEN];
				char tmp[CROSS_LEN];
				safe_strcpy(tmp, temp_line.c_str());

				uint8_t dummy;
				if (!DOS_MakeName(tmp, fullname, &dummy)) {
					WriteOut(MSG_Get(
					        "PROGRAM_IMGMOUNT_NON_LOCAL_DRIVE"));
					return;
				}
				if (Drives.at(dummy)->GetType() !=
				    DosDriveType::Local) {
					WriteOut(MSG_Get(
					        "PROGRAM_IMGMOUNT_NON_LOCAL_DRIVE"));
					return;
				}

				const auto ldp = std::dynamic_pointer_cast<localDrive>(
				        Drives.at(dummy));
				if (ldp == nullptr) {
					WriteOut(MSG_Get(
					        "PROGRAM_IMGMOUNT_FILE_NOT_FOUND"));
					return;
				}
				temp_line = ldp->MapDosToHostFilename(fullname);

				if (!path_exists(temp_line)) {
					if (add_wildcard_paths(temp_line, paths)) {
						continue;
					} else {
						WriteOut(MSG_Get(
						        "PROGRAM_IMGMOUNT_FILE_NOT_FOUND"));
						return;
					}
				}

				LOG_MSG("IMGMOUNT: Path '%s' found on virtual drive %c:",
				        fullname,
				        drive_letter(dummy));
			}
		}
		if (is_directory(temp_line)) {
			WriteOut(MSG_Get("PROGRAM_IMGMOUNT_MOUNT"));
			return;
		}
		paths.push_back(temp_line);
	}
	if (paths.empty()) {
		WriteOut(MSG_Get("PROGRAM_IMGMOUNT_SPECIFY_FILE"));
		return;
	}
	// Tidy up the paths
	for (auto& p : paths) {
		p = simplify_path(p).string();
	}

	if (paths.size() == 1) {
		temp_line = paths[0];
	}

	auto write_out_mount_status = [this](const char* image_type,
	                                     const std::vector<std::string>& images,
	                                     const char drive_letter) {
		constexpr auto end_punctuation = "";

		const auto images_str = join_with_commas(images,
		                                         MSG_Get("CONJUNCTION_AND"),
		                                         end_punctuation);

		const std::string type_and_images_str = image_type +
		                                        std::string(" ") + images_str;

		WriteOut(MSG_Get("PROGRAM_MOUNT_STATUS_2"),
		         type_and_images_str.c_str(),
		         drive_letter);
	};

	if (fstype == "fat") {
		if (imgsizedetect) {
			FILE* diskfile = fopen_wrap_ro_fallback(temp_line, roflag);
			if (!diskfile) {
				WriteOut(MSG_Get("PROGRAM_IMGMOUNT_INVALID_IMAGE"));
				return;
			}
			const auto sz = stdio_num_sectors(diskfile);
			if (sz < 0) {
				fclose(diskfile);
				WriteOut(MSG_Get("PROGRAM_IMGMOUNT_INVALID_IMAGE"));
				return;
			}
			uint32_t fcsize = check_cast<uint32_t>(sz);
			uint8_t buf[512];
			if (cross_fseeko(diskfile, 0L, SEEK_SET) != 0 || 
				fread(buf, sizeof(uint8_t), 512, diskfile) < 512) {
				fclose(diskfile);
				WriteOut(MSG_Get("PROGRAM_IMGMOUNT_INVALID_IMAGE"));
				return;
			}
			fclose(diskfile);
			if ((buf[510] != 0x55) || (buf[511] != 0xaa)) {
				WriteOut(MSG_Get("PROGRAM_IMGMOUNT_INVALID_GEOMETRY"));
				return;
			}
			Bitu sectors = (Bitu)(fcsize / (16 * 63));
			if (sectors * 16 * 63 != fcsize) {
				WriteOut(MSG_Get("PROGRAM_IMGMOUNT_INVALID_GEOMETRY"));
				return;
			}
			sizes[0] = 512;
			sizes[1] = 63;
			sizes[2] = 16;
			sizes[3] = sectors;

			LOG_MSG("autosized image file: %d:%d:%d:%d",
			        sizes[0],
			        sizes[1],
			        sizes[2],
			        sizes[3]);
		}

		if (Drives.at(drive_index(drive))) {
			WriteOut(MSG_Get("PROGRAM_IMGMOUNT_ALREADY_MOUNTED"));
			return;
		}

		DriveManager::filesystem_images_t fat_images = {};

		for (const auto& fat_path : paths) {
			auto fat_image = std::make_shared<fatDrive>(fat_path.c_str(),
			                                            sizes[0],
			                                            sizes[1],
			                                            sizes[2],
			                                            sizes[3],
														mediaid,
			                                            roflag);
			if (fat_image->created_successfully) {
				fat_images.push_back(fat_image);
			} else {
				WriteOut(MSG_Get("PROGRAM_IMGMOUNT_CANT_CREATE"));
				return;
			}
		}
		if (fat_images.empty()) {
			WriteOut(MSG_Get("PROGRAM_IMGMOUNT_CANT_CREATE"));
			return;
		}

		// Update DriveManager
		DriveManager::AppendFilesystemImages(drive_index(drive), fat_images);
		DriveManager::InitializeDrive(drive_index(drive));

		// Set the correct media byte in the table
		mem_writeb(RealToPhysical(dos.tables.mediaid) + drive_index(drive) * 9,
		           mediaid);

		/* Command uses dta so set it to our internal dta */
		RealPt save_dta = dos.dta();
		dos.dta(dos.tables.tempdta);

		for (auto it = fat_images.begin(); it != fat_images.end(); ++it) {
			const bool should_notify = std::next(it) == fat_images.end();
			DriveManager::CycleDisks(drive_index(drive), should_notify);
			char root[7] = {drive, ':', '\\', '*', '.', '*', 0};

			// Obtain the drive label, saving it in the dirCache
			if (!DOS_FindFirst(root, FatAttributeFlags::Volume)) {
				LOG_WARNING("DRIVE: Unable to find %c drive's volume label",
				            drive);
			}
		}
		dos.dta(save_dta);

		write_out_mount_status(MSG_Get("MOUNT_TYPE_FAT").c_str(), paths, drive);

		const auto fat_image = std::dynamic_pointer_cast<fatDrive>(
		        fat_images.front());
		assert(fat_image);
		const auto has_hdd = fat_image->loadedDisk &&
		                     fat_image->loadedDisk->hardDrive;

		const auto is_floppy = (drive == 'A' || drive == 'B') && !has_hdd;
		const auto is_hdd = (drive == 'C' || drive == 'D') && has_hdd;
		if (is_floppy || is_hdd) {
			imageDiskList.at(drive_index(drive)) = fat_image->loadedDisk;
			updateDPT();
		}
	} else if (fstype == "iso") {
		if (Drives.at(drive_index(drive))) {
			WriteOut(MSG_Get("PROGRAM_IMGMOUNT_ALREADY_MOUNTED"));
			return;
		}

		// create new drives for all images
		DriveManager::filesystem_images_t iso_images = {};
		for (const auto& iso_path : paths) {
			int error = -1;

			iso_images.push_back(std::make_shared<isoDrive>(
			        drive, iso_path.c_str(), mediaid, error));
			switch (error) { //-V785
			case 0: break;
			case 1:
				WriteOut(MSG_Get("MSCDEX_ERROR_MULTIPLE_CDROMS"));
				break;
			case 2:
				WriteOut(MSG_Get("MSCDEX_ERROR_NOT_SUPPORTED"));
				break;
			case 3: WriteOut(MSG_Get("MSCDEX_ERROR_OPEN")); break;
			case 4:
				WriteOut(MSG_Get("MSCDEX_TOO_MANY_DRIVES"));
				break;
			case 5:
				WriteOut(MSG_Get("MSCDEX_LIMITED_SUPPORT"));
				break;
			case 6:
				WriteOut(MSG_Get("MSCDEX_INVALID_FILEFORMAT"));
				break;
			default:
				WriteOut(MSG_Get("MSCDEX_UNKNOWN_ERROR"));
				break;
			}
			// error: clean up and leave
			if (error) { //-V547
				WriteOut(MSG_Get("PROGRAM_IMGMOUNT_CANT_CREATE"));
				return;
			}
		}
		// Update DriveManager
		DriveManager::AppendFilesystemImages(drive_index(drive), iso_images);
		DriveManager::InitializeDrive(drive_index(drive));

		// Set the correct media byte in the table
		mem_writeb(RealToPhysical(dos.tables.mediaid) + drive_index(drive) * 9,
		           mediaid);

		// If instructed, attach to IDE controller as ATAPI CD-ROM device
		if (wants_ide) {
			if (ide_index >= 0) {
				IDE_CDROM_Attach(ide_index,
				                 is_second_cable_slot,
				                 drive_index(drive));
			} else {
				WriteOut(MSG_Get(
				        "PROGRAM_IMGMOUNT_IDE_CONTROLLERS_UNAVAILABLE"));
			}
		}

		// Print status message (success)
		WriteOut(MSG_Get("MSCDEX_SUCCESS"));

		write_out_mount_status(MSG_Get("MOUNT_TYPE_ISO").c_str(), paths, drive);

	} else if (fstype == "none") {
		FILE* new_disk = fopen_wrap_ro_fallback(temp_line, roflag);
		if (!new_disk) {
			WriteOut(MSG_Get("PROGRAM_IMGMOUNT_INVALID_IMAGE"));
			return;
		}
		const auto sz = stdio_size_kb(new_disk);
		if (sz < 0) {
			fclose(new_disk);
			WriteOut(MSG_Get("PROGRAM_IMGMOUNT_INVALID_IMAGE"));
			return;
		}
		uint32_t imagesize = check_cast<uint32_t>(sz);
		const bool is_hdd  = (imagesize > 2880);
		// Seems to make sense to require a valid geometry..
		if (is_hdd && sizes[0] == 0 && sizes[1] == 0 && sizes[2] == 0 &&
		    sizes[3] == 0) {
			fclose(new_disk);
			WriteOut(MSG_Get("PROGRAM_IMGMOUNT_SPECIFY_GEOMETRY"));
			return;
		}

		const auto drive_index = drive - '0';

		imageDiskList.at(drive_index) = std::make_shared<imageDisk>(
		        new_disk, temp_line.c_str(), imagesize, is_hdd);

		if (is_hdd) {
			imageDiskList.at(drive_index)
			        ->Set_Geometry(sizes[2], sizes[3], sizes[1], sizes[0]);
		}

		if ((drive == '2' || drive == '3') && is_hdd) {
			updateDPT();
		}

		WriteOut(MSG_Get("PROGRAM_IMGMOUNT_MOUNT_NUMBER"),
		         drive_index,
		         temp_line.c_str());
	}

	// check if volume label is given. be careful for cdrom
	// if (cmd->FindString("-label",label,true))
	// newdrive->dirCache.SetLabel(label.c_str());
	return;
}

void IMGMOUNT::AddMessages()
{
	AddCommonMountMessages();
	MSG_Add("PROGRAM_IMGMOUNT_HELP_LONG",
	        "Mount a CD-ROM, floppy, or disk image to a drive letter.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=light-green]imgmount[reset] [color=white]DRIVE[reset] [color=light-cyan]CDROM-SET[reset] [-fs iso] [-ide] -t cdrom|iso\n"
	        "  [color=light-green]imgmount[reset] [color=white]DRIVE[reset] [color=light-cyan]IMAGEFILE[reset] [IMAGEFILE2 [..]] [-fs fat] -t hdd|floppy -ro\n"
	        "  [color=light-green]imgmount[reset] [color=white]DRIVE[reset] [color=light-cyan]BOOTIMAGE[reset] [-fs fat|none] -t hdd -size GEOMETRY -ro\n"
	        "  [color=light-green]imgmount[reset] -u [color=white]DRIVE[reset]  (unmounts the [color=white]DRIVE[reset]'s image)\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=white]DRIVE[reset]      drive letter where the image will be mounted: A, C, D, ...\n"
	        "  [color=light-cyan]CDROM-SET[reset]  ISO, CUE+BIN, CUE+ISO, or CUE+ISO+FLAC/OPUS/OGG/MP3/WAV\n"
	        "  [color=light-cyan]IMAGEFILE[reset]  hard drive or floppy image in FAT16 or FAT12 format\n"
	        "  [color=light-cyan]BOOTIMAGE[reset]  bootable disk image with specified -size GEOMETRY:\n"
	        "             bytes-per-sector,sectors-per-head,heads,cylinders\n"
	        "\n"
	        "Notes:\n"
			"  - You can use wildcards to mount multiple images, e.g.:\n"
			"      [color=light-green]imgmount[reset] [color=white]A[reset] [color=light-cyan]floppy*.img[reset] -t floppy\n"
	        "  - [color=yellow]%s+F4[reset] swaps & mounts the next [color=light-cyan]CDROM-SET[reset] or [color=light-cyan]BOOTIMAGE[reset], if provided.\n"
	        "  - The -ro flag mounts the disk image in read-only (write-protected) mode.\n"
	        "  - The -ide flag emulates an IDE controller with attached IDE CD drive, useful\n"
	        "    for CD-based games that need a real DOS environment via bootable HDD image.\n"
	        "\n"
	        "Examples:\n");
	MSG_Add("PROGRAM_IMGMOUNT_HELP_LONG_WIN32",
	        "  [color=light-green]imgmount[reset] [color=white]D[reset] [color=light-cyan]C:\\Games\\doom.iso[reset] -t cdrom\n");
	MSG_Add("PROGRAM_IMGMOUNT_HELP_LONG_MACOSX",
	        "  [color=light-green]imgmount[reset] [color=white]D[reset] [color=light-cyan]/Users/USERNAME/Games/doom.iso[reset] -t cdrom\n");
	MSG_Add("PROGRAM_IMGMOUNT_HELP_LONG_OTHER",
	        "  [color=light-green]imgmount[reset] [color=white]D[reset] [color=light-cyan]/home/USERNAME/games/doom.iso[reset] -t cdrom\n");
	MSG_Add("PROGRAM_IMGMOUNT_HELP_LONG_GENERIC",
	        "  [color=light-green]imgmount[reset] [color=white]D[reset] [color=light-cyan]cd/quake1.cue[reset] -t cdrom\n"
	        "  [color=light-green]imgmount[reset] [color=white]A[reset] [color=light-cyan]floppy1.img floppy2.img floppy3.img[reset] -t floppy -ro\n"
	        "  [color=light-green]imgmount[reset] [color=white]A[reset] [color=light-cyan]floppy*.img[reset] -t floppy -ro\n"
	        "  [color=light-green]imgmount[reset] [color=white]C[reset] [color=light-cyan]bootable.img[reset] -t hdd -fs none -size 512,63,32,1023\n");

	MSG_Add("PROGRAM_IMGMOUNT_SPECIFY_DRIVE",
	        "Must specify drive letter to mount image at.\n");

	MSG_Add("PROGRAM_IMGMOUNT_SPECIFY2",
	        "Must specify drive number (0 or 3) to mount image at (0,1=fda,fdb; 2,3=hda,hdb).\n");

	MSG_Add("PROGRAM_IMGMOUNT_SPECIFY_GEOMETRY",
	        "For CD-ROM images:\n"
	        "  [color=light-green]imgmount[reset] [color=white]DRIVE[reset] [color=light-cyan]IMAGEFILE[reset] -t iso\n"
	        "For hard drive images, must specify drive geometry:\n"
	        "  bytes-per-sector,sectors-per-head,heads,cylinders\n"
	        "  [color=light-green]imgmount[reset] [color=white]DRIVE[reset] [color=light-cyan]IMAGEFILE[reset] -size bps,spc,hpc,cyl\n");

	MSG_Add("PROGRAM_IMGMOUNT_STATUS_NONE", "No drive available.\n");

	MSG_Add("PROGRAM_IMGMOUNT_IDE_CONTROLLERS_UNAVAILABLE",
	        "No available IDE controllers. Drive will not have IDE emulation.\n");

	MSG_Add("PROGRAM_IMGMOUNT_INVALID_IMAGE",
	        "Could not load image file.\n"
	        "Check that the path is correct and the image is accessible.\n");

	MSG_Add("PROGRAM_IMGMOUNT_INVALID_GEOMETRY",
	        "Could not extract drive geometry from image.\n"
	        "Use parameter -size bps,spc,hpc,cyl to specify the geometry.\n");

	MSG_Add("PROGRAM_IMGMOUNT_TYPE_UNSUPPORTED",
	        "Type '%s' is unsupported. Specify 'floppy', 'hdd', 'cdrom', or 'iso'.\n");

	MSG_Add("PROGRAM_IMGMOUNT_FORMAT_UNSUPPORTED",
	        "Format '%s' is unsupported. Specify 'fat', 'iso', or 'none'.\n");

	MSG_Add("PROGRAM_IMGMOUNT_SPECIFY_FILE",
	        "Must specify file-image to mount.\n");

	MSG_Add("PROGRAM_IMGMOUNT_FILE_NOT_FOUND", "Image file not found.\n");

	MSG_Add("PROGRAM_IMGMOUNT_MOUNT",
	        "To mount directories, use the [color=light-green]MOUNT[reset] command, not the [color=light-green]IMGMOUNT[reset] command.\n");

	MSG_Add("PROGRAM_IMGMOUNT_ALREADY_MOUNTED",
	        "Drive already mounted at that letter.\n");

	MSG_Add("PROGRAM_IMGMOUNT_CANT_CREATE", "Can't create drive from file.\n");
	MSG_Add("PROGRAM_IMGMOUNT_MOUNT_NUMBER", "Drive number %d mounted as %s.\n");

	MSG_Add("PROGRAM_IMGMOUNT_NON_LOCAL_DRIVE",
	        "The image must be on a host or local drive.\n");
}
