// SPDX-FileCopyrightText:  2021-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mount.h"
#include "mount_common.h"

#include "dosbox.h"

#include <algorithm>
#include <vector>

#include "config/config.h"
#include "dos/cdrom.h"
#include "dos/drives.h"
#include "gui/mapper.h"
#include "hardware/ide.h"
#include "ints/bios_disk.h"
#include "ints/int10.h"
#include "misc/cross.h"
#include "misc/notifications.h"
#include "more_output.h"
#include "shell/shell.h"
#include "utils/fs_utils.h"
#include "utils/string_utils.h"
#include <sys/stat.h>

#ifndef S_ISREG
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif

void MOUNT::ListMounts()
{
	const std::string header_drive = MSG_Get("PROGRAM_MOUNT_STATUS_DRIVE");
	const std::string header_type  = MSG_Get("PROGRAM_MOUNT_STATUS_TYPE");
	const std::string header_label = MSG_Get("PROGRAM_MOUNT_STATUS_LABEL");

	const int console_width = real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS);
	const auto width_drive  = static_cast<int>(header_drive.length());
	const auto width_label  = std::max(minimum_column_length,
                                          static_cast<int>(header_label.size()));
	const auto width_type   = console_width - 3 - width_drive - width_label;
	if (width_type < 0) {
		LOG_WARNING("Message is too long.");
		return;
	}

	auto print_row = [&](const std::string& txt_drive,
	                     const std::string& txt_type,
	                     const std::string& txt_label) {
		WriteOut("%-*s %-*s %-*s\n",
		         width_drive,
		         txt_drive.c_str(),
		         width_type,
		         txt_type.c_str(),
		         width_label,
		         txt_label.c_str());
	};

	WriteOut(MSG_Get("PROGRAM_MOUNT_STATUS_1"));
	print_row(header_drive, header_type, header_label);
	const std::string horizontal_divider(console_width, '-');
	WriteOut_NoParsing(horizontal_divider);

	bool found_drives = false;
	for (uint8_t d = 0; d < DOS_DRIVES; d++) {
		if (Drives[d]) {
			print_row(std::string{drive_letter(d)},
			          Drives[d]->GetInfoString(),
			          To_Label(Drives[d]->GetLabel()));
			found_drives = true;
		}
	}

	if (!found_drives) {
		WriteOut(MSG_Get("PROGRAM_IMGMOUNT_STATUS_NONE"));
	}
}

void MOUNT::ShowUsage()
{
	MoreOutputStrings output(*this);
	// Combined help
	output.AddString(MSG_Get("PROGRAM_MOUNT_HELP_LONG"), PRIMARY_MOD_NAME);
#ifdef WIN32
	output.AddString(MSG_Get("PROGRAM_MOUNT_HELP_LONG_WIN32"));
#elif defined(MACOSX)
	output.AddString(MSG_Get("PROGRAM_MOUNT_HELP_LONG_MACOSX"));
#else
	output.AddString(MSG_Get("PROGRAM_MOUNT_HELP_LONG_OTHER"));
#endif
	output.Display();
}

// A helper function that expands wildcard paths from the given argument and
// adds them to the given paths. Returns true if the expansion succeeded.
bool MOUNT::AddWildcardPaths(const std::string& path_arg,
                             std::vector<std::string>& paths)
{
	// Expand the given path argument
	constexpr auto OnlyExpandFiles          = true;
	constexpr auto SkipNativePath           = true;
	std::vector<std::string> expanded_paths = {};
	if (!get_expanded_files(path_arg, expanded_paths, OnlyExpandFiles, SkipNativePath)) {
		return false;
	}

	// Sort wildcards with natural ordering
	const auto has_wildcards = path_arg.find_first_of('*') != std::string::npos ||
	                           path_arg.find_first_of('?') != std::string::npos;
	if (has_wildcards) {
		std::sort(expanded_paths.begin(), expanded_paths.end(), natural_compare);
	}

	// Move the expanded paths out
	paths.insert(paths.end(), //-V823
	             std::make_move_iterator(expanded_paths.begin()),
	             std::make_move_iterator(expanded_paths.end()));

	return true;
}

void MOUNT::WriteMountStatus(const char* image_type,
                             const std::vector<std::string>& images, char drive_letter)
{
	constexpr auto EndPunctuation = "";

	const auto images_str = join_with_commas(images,
	                                         MSG_Get("CONJUNCTION_AND"),
	                                         EndPunctuation);

	const std::string type_and_images_str = image_type + std::string(" ") +
	                                        images_str;

	WriteOut(MSG_Get("PROGRAM_MOUNT_STATUS_2"),
	         type_and_images_str.c_str(),
	         drive_letter);
}

bool MOUNT::MountImageFat(MountParameters& params)
{
	// Autosize detection
	bool imgsizedetect = (params.type == "hdd") &&
	                     (params.sizes[0] == 0 && params.sizes[1] == 0 &&
	                      params.sizes[2] == 0 && params.sizes[3] == 0);

	if (imgsizedetect) {
		FILE* diskfile = fopen_wrap_ro_fallback(params.paths[0],
		                                        params.roflag);
		if (!diskfile) {
			NOTIFY_DisplayWarning(Notification::Source::Console,
			                      "MOUNT",
			                      "PROGRAM_IMGMOUNT_INVALID_IMAGE");
			return false;
		}
		const auto sz = stdio_num_sectors(diskfile);
		if (sz < 0) {
			fclose(diskfile);
			NOTIFY_DisplayWarning(Notification::Source::Console,
			                      "MOUNT",
			                      "PROGRAM_IMGMOUNT_INVALID_IMAGE");
			return false;
		}
		uint32_t fcsize = check_cast<uint32_t>(sz);
		uint8_t buf[512];
		if (cross_fseeko(diskfile, 0L, SEEK_SET) != 0 ||
		    fread(buf, sizeof(uint8_t), 512, diskfile) < 512) {
			fclose(diskfile);
			NOTIFY_DisplayWarning(Notification::Source::Console,
			                      "MOUNT",
			                      "PROGRAM_IMGMOUNT_INVALID_IMAGE");
			return false;
		}
		fclose(diskfile);
		if ((buf[510] != 0x55) || (buf[511] != 0xaa)) {
			NOTIFY_DisplayWarning(Notification::Source::Console,
			                      "MOUNT",
			                      "PROGRAM_IMGMOUNT_INVALID_GEOMETRY");
			return false;
		}
		Bitu sectors = (Bitu)(fcsize / (16 * 63));
		if (sectors * 16 * 63 != fcsize) {
			NOTIFY_DisplayWarning(Notification::Source::Console,
			                      "MOUNT",
			                      "PROGRAM_IMGMOUNT_INVALID_GEOMETRY");
			return false;
		}
		params.sizes[0] = 512;
		params.sizes[1] = 63;
		params.sizes[2] = 16;
		params.sizes[3] = static_cast<uint16_t>(sectors);

		LOG_MSG("autosized image file: %d:%d:%d:%d",
		        params.sizes[0],
		        params.sizes[1],
		        params.sizes[2],
		        params.sizes[3]);
	}

	if (Drives.at(drive_index(params.drive))) {
		NOTIFY_DisplayWarning(Notification::Source::Console,
		                      "MOUNT",
		                      "PROGRAM_IMGMOUNT_ALREADY_MOUNTED");
		return false;
	}

	DriveManager::filesystem_images_t fat_images = {};

	for (const auto& fat_path : params.paths) {
		auto fat_image = std::make_shared<fatDrive>(fat_path.c_str(),
		                                            params.sizes[0],
		                                            params.sizes[1],
		                                            params.sizes[2],
		                                            params.sizes[3],
		                                            params.mediaid,
		                                            params.roflag);
		if (fat_image->created_successfully) {
			fat_images.push_back(fat_image);
		} else {
			NOTIFY_DisplayWarning(Notification::Source::Console,
			                      "MOUNT",
			                      "PROGRAM_IMGMOUNT_CANT_CREATE");
			return false;
		}
	}
	if (fat_images.empty()) {
		NOTIFY_DisplayWarning(Notification::Source::Console,
		                      "MOUNT",
		                      "PROGRAM_IMGMOUNT_CANT_CREATE");
		return false;
	}

	// Update DriveManager
	DriveManager::AppendFilesystemImages(drive_index(params.drive), fat_images);
	DriveManager::InitializeDrive(drive_index(params.drive));

	// Set the correct media byte in the table
	// Each entry is 9 bytes, with the media byte at offset 0x00
	constexpr auto DptEntrySize = 9;
	mem_writeb(RealToPhysical(dos.tables.mediaid) +
	                   drive_index(params.drive) * DptEntrySize,
	           params.mediaid);

	// Command uses dta so set it to our internal dta
	RealPt save_dta = dos.dta();
	dos.dta(dos.tables.tempdta);

	for (auto it = fat_images.begin(); it != fat_images.end(); ++it) {
		const bool should_notify = std::next(it) == fat_images.end();
		DriveManager::CycleDisks(drive_index(params.drive), should_notify);
		char root[7] = {params.drive, ':', '\\', '*', '.', '*', 0};

		// Obtain the drive label, saving it in the dirCache
		if (!DOS_FindFirst(root, FatAttributeFlags::Volume)) {
			LOG_WARNING("DRIVE: Unable to find %c drive's volume label",
			            params.drive);
		}
	}
	dos.dta(save_dta);

	WriteMountStatus(MSG_Get("MOUNT_TYPE_FAT").c_str(), params.paths, params.drive);

	const auto fat_image = std::dynamic_pointer_cast<fatDrive>(
	        fat_images.front());
	assert(fat_image);
	const auto has_hdd = fat_image->loadedDisk && fat_image->loadedDisk->hardDrive;

	const auto is_floppy = (params.drive == 'A' || params.drive == 'B') &&
	                       !has_hdd;
	const auto is_hdd = (params.drive == 'C' || params.drive == 'D') && has_hdd;
	if (is_floppy || is_hdd) {
		imageDiskList.at(drive_index(params.drive)) = fat_image->loadedDisk;
		updateDPT();
	}
	return true;
}

bool MOUNT::MountImageIso(MountParameters& params)
{
	if (Drives.at(drive_index(params.drive))) {
		NOTIFY_DisplayWarning(Notification::Source::Console,
		                      "MOUNT",
		                      "PROGRAM_IMGMOUNT_ALREADY_MOUNTED");
		return false;
	}

	// create new drives for all images
	DriveManager::filesystem_images_t iso_images = {};
	for (const auto& iso_path : params.paths) {
		int error = -1;
		iso_images.push_back(std::make_shared<isoDrive>(
		        params.drive, iso_path.c_str(), params.mediaid, error));

		// Error handling switch
		const char* msg_id = nullptr;
		switch (error) { //-V785
		case 0: break;
		case 1: msg_id = "MSCDEX_ERROR_MULTIPLE_CDROMS"; break;
		case 2: msg_id = "MSCDEX_ERROR_NOT_SUPPORTED"; break;
		case 3: msg_id = "MSCDEX_ERROR_OPEN"; break;
		case 4: msg_id = "MSCDEX_TOO_MANY_DRIVES"; break;
		case 5: msg_id = "MSCDEX_LIMITED_SUPPORT"; break;
		case 6: msg_id = "MSCDEX_INVALID_FILEFORMAT"; break;
		default: msg_id = "MSCDEX_UNKNOWN_ERROR"; break;
		}

		if (msg_id) { //-V547
			NOTIFY_DisplayWarning(Notification::Source::Console,
			                      "MOUNT",
			                      msg_id);
		}

		// error: clean up and leave
		if (error) { //-V547
			NOTIFY_DisplayWarning(Notification::Source::Console,
			                      "MOUNT",
			                      "PROGRAM_IMGMOUNT_CANT_CREATE");
			return false;
		}
	}

	// Update DriveManager
	DriveManager::AppendFilesystemImages(drive_index(params.drive), iso_images);
	DriveManager::InitializeDrive(drive_index(params.drive));

	// Set the correct media byte in the table
	mem_writeb(RealToPhysical(dos.tables.mediaid) + drive_index(params.drive) * 9,
	           params.mediaid);

	// If instructed, attach to IDE controller as ATAPI CD-ROM device
	if (params.is_ide) {
		if (params.ide_index >= 0) {
			IDE_CDROM_Attach(params.ide_index,
			                 params.is_second_cable_slot,
			                 drive_index(params.drive));
		} else {
			NOTIFY_DisplayWarning(Notification::Source::Console,
			                      "MOUNT",
			                      "PROGRAM_IMGMOUNT_IDE_CONTROLLERS_UNAVAILABLE");
		}
	}

	// Print status message (success)
	WriteOut(MSG_Get("MSCDEX_SUCCESS"));
	WriteMountStatus(MSG_Get("MOUNT_TYPE_ISO").c_str(), params.paths, params.drive);
	return true;
}

bool MOUNT::MountImageRaw(MountParameters& params)
{
	auto new_disk = fopen_wrap_ro_fallback(params.paths[0], params.roflag);
	if (!new_disk) {
		NOTIFY_DisplayWarning(Notification::Source::Console,
		                      "MOUNT",
		                      "PROGRAM_IMGMOUNT_INVALID_IMAGE");
		return false;
	}
	const auto sz = stdio_size_kb(new_disk);
	if (sz < 0) {
		fclose(new_disk);
		NOTIFY_DisplayWarning(Notification::Source::Console,
		                      "MOUNT",
		                      "PROGRAM_IMGMOUNT_INVALID_IMAGE");
		return false;
	}
	auto imagesize = check_cast<uint32_t>(sz);
	// 0=A:, 1=B:, 2=C:, 3=D:
	const auto is_hdd = (params.drive >= '2');
	// Seems to make sense to require a valid geometry..
	if (is_hdd && params.sizes[0] == 0 && params.sizes[1] == 0 &&
	    params.sizes[2] == 0 && params.sizes[3] == 0) {
		fclose(new_disk);
		NOTIFY_DisplayWarning(Notification::Source::Console,
		                      "MOUNT",
		                      "PROGRAM_IMGMOUNT_SPECIFY_GEOMETRY");
		return false;
	}

	const auto drv_idx = params.drive - '0';

	imageDiskList.at(drv_idx) = std::make_shared<imageDisk>(
	        new_disk, params.paths[0].c_str(), imagesize, is_hdd);

	if (is_hdd) {
		imageDiskList.at(drv_idx)->Set_Geometry(params.sizes[2],
		                                        params.sizes[3],
		                                        params.sizes[1],
		                                        params.sizes[0]);
	}

	if ((params.drive == '2' || params.drive == '3') && is_hdd) {
		updateDPT();
	}

	WriteOut(MSG_Get("PROGRAM_IMGMOUNT_MOUNT_NUMBER"),
	         drv_idx,
	         params.paths[0].c_str());
	return true;
}

bool MOUNT::MountImage(MountParameters& params)
{
	// Determine Media ID if not already set strictly
	params.mediaid = (params.type == "floppy") ? MediaId::Floppy1_44MB
	                                           : MediaId::HardDisk;

	if (params.fstype == "fat") {
		return MountImageFat(params);
	} else if (params.fstype == "iso") {
		return MountImageIso(params);
	} else if (params.fstype == "none") {
		return MountImageRaw(params);
	}
	return true;
}

bool MOUNT::HandleUnmount()
{
	std::string umount = {};
	if (cmd->FindString("-u", umount, false)) {
		WriteOut(UnmountHelper(umount[0]), toupper(umount[0]));
		return true;
	}
	return false;
}

void MOUNT::ParseArguments(MountParameters& params, bool& explicit_fs,
                           bool& path_relative_to_last_config)
{
	if (cmd->FindExist("-pr", true)) {
		path_relative_to_last_config = true;
	}

	cmd->FindString("-t", params.type, true);
	// Allow aliases or standard image types to pass through
	if (params.type == "cdrom") {
		params.type = "iso";
	}

	params.roflag = cmd->FindExist("-ro", true);

	// Parse -fs (filesystem type)
	if (params.type == "iso") {
		params.fstype = "iso";
	}
	explicit_fs = cmd->FindString("-fs", params.fstype, true);

	// Parse -ide
	std::string ide_value = {};

	params.is_ide = cmd->FindString("-ide", ide_value, true) ||
	                cmd->FindExist("-ide", true);
	if (params.is_ide && (params.type == "iso")) {
		IDE_Get_Next_Cable_Slot(params.ide_index, params.is_second_cable_slot);
	}

	// Label
	cmd->FindString("-label", params.label, true);
}

void MOUNT::ParseGeometry(MountParameters& params)
{
	std::string str_size = "";
	std::string str_chs  = "";

	// Default sizing logic based on type
	if (params.type == "floppy") {
		str_size       = "512,1,2880,2880";
		params.mediaid = MediaId::Floppy1_44MB;

	} else if (params.type == "dir" || params.type == "overlay") {
		// 512*32*32765==~500MB total size
		// 512*32*16000==~250MB total free size
		str_size = "512,32,32765,16000";

		// If drive mounted to A or B, set mediaid to floppy
		// This is preferable to using type because floppies can
		// be auto-mounted as type "dir"
		std::string command_arg;
		cmd->FindCommand(1, command_arg);
		if (!command_arg.empty()) {
			const int i_drive = std::toupper(command_arg[0]);
			if (i_drive == 'A' || i_drive == 'B') {
				params.mediaid = MediaId::Floppy1_44MB;
			}
		}
	} else if (params.type == "iso") {
		str_size = "2048,1,65535,0";
	} else if (params.type != "hdd") {
		// If it is 'hdd', we leave sizes 0 to trigger detection or
		// parsing below. If it is unknown, we error out later.
		NOTIFY_DisplayWarning(Notification::Source::Console,
		                      "MOUNT",
		                      "PROGRAM_MOUNT_ILL_TYPE");
		return;
	}

	// Parse the free space in mb (kb for floppies)
	std::string mb_size;
	if (cmd->FindString("-freesize", mb_size, true)) {
		char teststr[1024];
		uint16_t freesize = static_cast<uint16_t>(atoi(mb_size.c_str()));
		if (params.type == "floppy") {
			// freesize in kb
			safe_sprintf(teststr,
			             "512,1,2880,%d",
			             freesize * 1024 / (512 * 1));
		} else {
			uint32_t total_size_cyl = 32765;
			uint32_t free_size_cyl  = (uint32_t)freesize * 1024 *
			                         1024 / (512 * 32);
			if (free_size_cyl > 65534) {
				free_size_cyl = 65534;
			}
			if (total_size_cyl < free_size_cyl) {
				total_size_cyl = free_size_cyl + 10;
			}
			if (total_size_cyl > 65534) {
				total_size_cyl = 65534;
			}
			safe_sprintf(teststr, "512,32,%d,%d", total_size_cyl, free_size_cyl);
		}
		str_size = teststr;
	}

	// Parse -size
	cmd->FindString("-size", str_size, true);

	// Apply str_size string to sizes array
	if (!str_size.empty()) {
		char number[21]  = {0};
		const char* scan = str_size.c_str();
		int index        = 0;
		int count        = 0;
		// Parse the str_size string
		while (*scan && index < 20 && count < 4) {
			if (*scan == ',') {
				number[index]         = 0;
				params.sizes[count++] = atoi(number);
				index                 = 0;
			} else {
				number[index++] = *scan;
			}
			scan++;
		}
		if (count < 4) {
			// always goes correct as index is max 20 at this point.
			number[index]       = 0;
			params.sizes[count] = atoi(number);
		}
	}

	// Parse -chs C,H,S
	if (cmd->FindString("-chs", str_chs, true)) {
		int cmd_cylinders = 0, cmd_heads = 0, cmd_sectors = 0;
		if (sscanf(str_chs.c_str(), "%d,%d,%d", &cmd_cylinders, &cmd_heads, &cmd_sectors) ==
		    3) {
			params.sizes[0] = 512;
			params.sizes[1] = static_cast<uint16_t>(cmd_sectors);
			params.sizes[2] = static_cast<uint16_t>(cmd_heads);
			params.sizes[3] = static_cast<uint16_t>(cmd_cylinders);
		} else {
			NOTIFY_DisplayWarning(Notification::Source::Console,
			                      "MOUNT",
			                      "PROGRAM_MOUNT_INVALID_CHS");
		}
	}
}

bool MOUNT::ParseDrive(MountParameters& params, bool explicit_fs)
{
	// get the drive letter or number
	std::string temp_line;
	cmd->FindCommand(1, temp_line);
	if ((temp_line.size() > 2) ||
	    ((temp_line.size() > 1) && (temp_line[1] != ':'))) {
		ShowUsage();
		return false;
	}

	const char first_char  = toupper(temp_line[0]);
	params.is_drive_number = false;

	if (isdigit(first_char)) {
		// Handle Drive Numbers (0, 1, 2, 3) for bootable images
		if (first_char >= '0' && first_char <= '3') {
			params.drive           = first_char;
			params.is_drive_number = true;

			// If user did not explicit specify filesystem, assume
			// 'none' for raw bootable access
			if (!explicit_fs) {
				params.fstype = "none";
			}
		} else {
			NOTIFY_DisplayWarning(Notification::Source::Console,
			                      "MOUNT",
			                      "PROGRAM_IMGMOUNT_SPECIFY2");
			return false;
		}
	} else if (first_char >= 'A' && first_char <= 'Z') {
		params.drive = first_char;
		// Allow A:, B:, C: and D: to be mounted as raw bootable images
		if (explicit_fs && params.fstype == "none") {
			switch (params.drive) {
			case 'A':
				params.drive           = '0';
				params.is_drive_number = true;
				break;
			case 'B':
				params.drive           = '1';
				params.is_drive_number = true;
				break;
			case 'C':
				params.drive           = '2';
				params.is_drive_number = true;
				break;
			case 'D':
				params.drive           = '3';
				params.is_drive_number = true;
				break;
			default:
				// Don't allow booting from E:, F:, etc.
				NOTIFY_DisplayWarning(Notification::Source::Console,
				                      "MOUNT",
				                      "PROGRAM_IMGMOUNT_SPECIFY2");
				return false;
			}
		}
	} else {
		ShowUsage();
		return false;
	}

	// Check overlaps
	if (!params.is_drive_number) {
		if (params.type == "overlay") {
			// Ensure that the base drive exists:
			if (!Drives.at(drive_index(params.drive))) {
				NOTIFY_DisplayWarning(Notification::Source::Console,
				                      "MOUNT",
				                      "PROGRAM_MOUNT_OVERLAY_NO_BASE");
				return false;
			}
		} else if (Drives.at(drive_index(params.drive))) {
			NOTIFY_DisplayWarning(Notification::Source::Console,
			                      "MOUNT",
			                      "PROGRAM_MOUNT_ALREADY_MOUNTED",
			                      params.drive,
			                      Drives.at(drive_index(params.drive))
			                              ->GetInfoString()
			                              .c_str());
			return false;
		}
	}
	return true;
}

// Returns true if processed successfully (even if it means it found an image
// and decided to mount it) Returns false on failure.
bool MOUNT::ProcessPaths(MountParameters& params, bool path_relative_to_last_config)
{
	std::string final_path;
	// Get the first path argument
	if (!cmd->FindCommand(2, final_path) || final_path.empty()) {
		ShowUsage();
		return false;
	}

	// Expand ~ to home directory
	final_path = resolve_home(final_path).string();

	// Resolve first path
	std::string path_arg_1 = final_path;
	if (path_relative_to_last_config && control->config_files.size() &&
	    !std_fs::path(path_arg_1).is_absolute()) {
		std::string lastconfigdir =
		        control->config_files[control->config_files.size() - 1];
		std::string::size_type pos = lastconfigdir.rfind(CROSS_FILESPLIT);
		if (pos == std::string::npos) {
			pos = 0;
		}
		lastconfigdir.erase(pos);
		if (lastconfigdir.length()) {
			path_arg_1 = lastconfigdir + CROSS_FILESPLIT + path_arg_1;
		}
	}

#if defined(WIN32)
	// Removing trailing backslash if not root dir so stat will succeed
	if (path_arg_1.size() > 3 && path_arg_1.back() == '\\') {
		path_arg_1.pop_back();
	}
#endif

	// Check first path
	struct stat test;
	auto stat_ok       = (stat(path_arg_1.c_str(), &test) == 0);
	auto target_is_dir = stat_ok && S_ISDIR(test.st_mode);
	auto explicit_image_type = (params.type == "hdd" || params.type == "iso" ||
	                            params.type == "floppy");

	const auto has_wildcards = path_arg_1.find_first_of("*?") !=
	                           std::string::npos;
	auto is_image_mode = false;

	// Explicit triggers
	if (explicit_image_type || params.is_drive_number || has_wildcards) {
		is_image_mode = true;
	}

	// If the target is a directory, it is a directory mount,
	// even if -t floppy was specified (legacy MOUNT behavior).
	if (target_is_dir) {
		is_image_mode = false;
	}

	// Implicit trigger: First argument exists and is a regular file
	if (!is_image_mode && stat_ok && S_ISREG(test.st_mode)) {
		is_image_mode = true;
	}

	if (is_image_mode) {
		// Loop through all remaining arguments
		uint16_t arg_idx = 2;
		std::string cur_arg;

		while (cmd->FindCommand(arg_idx++, cur_arg)) {
			// Expand ~ to home directory
			cur_arg = resolve_home(cur_arg).string();
			// Apply relative path logic to current argument
			if (path_relative_to_last_config &&
			    control->config_files.size() &&
			    !std_fs::path(cur_arg).is_absolute()) {
				std::string lastconfigdir =
				        control->config_files.back();
				auto pos = lastconfigdir.rfind(CROSS_FILESPLIT);
				if (pos == std::string::npos) {
					pos = 0;
				}
				lastconfigdir.erase(pos);
				if (!lastconfigdir.empty()) {
					cur_arg = lastconfigdir +
					          CROSS_FILESPLIT + cur_arg;
				}
			}

			// Resolve virtual drive letters to host paths first
			char fullname[CROSS_LEN];
			char tmp[CROSS_LEN];
			safe_strcpy(tmp, cur_arg.c_str());
			uint8_t drive_idx_found;
			std::string path_to_expand = cur_arg;

			if (DOS_MakeName(tmp, fullname, &drive_idx_found)) {
				if (Drives.at(drive_idx_found) &&
				    Drives.at(drive_idx_found)->GetType() ==
				            DosDriveType::Local) {
					const auto ldp = std::dynamic_pointer_cast<localDrive>(
					        Drives.at(drive_idx_found));
					if (ldp) {
						// This turns "C:\*.CUE" into
						// "/home/user/dosbox/c_drive/*.CUE"
						path_to_expand = ldp->MapDosToHostFilename(
						        fullname);
					}
				}
			}

			// Now try wildcard expansion on the translated host path
			if (path_to_expand.find_first_of("*?") != std::string::npos) {
				if (MOUNT::AddWildcardPaths(path_to_expand,
				                            params.paths)) {
					continue;
				}
			}

			// Fallback for literal files
			auto real_path = to_native_path(path_to_expand);
			std::string final_path = real_path.empty() ? path_to_expand
			                                           : real_path;

			if (real_path.empty() || !path_exists(real_path)) {
				// Try Virtual DOS Drive mapping
				bool found_on_virtual = false;

				// convert dosbox filename to system filename
				char fullname[CROSS_LEN];
				char tmp[CROSS_LEN];
				safe_strcpy(tmp, cur_arg.c_str());
				uint8_t dummy;
				if (DOS_MakeName(tmp, fullname, &dummy)) {
					if (Drives.at(dummy) &&
					    Drives.at(dummy)->GetType() ==
					            DosDriveType::Local) {
						const auto ldp = std::dynamic_pointer_cast<localDrive>(
						        Drives.at(dummy));
						if (ldp) {
							std::string host_name = ldp->MapDosToHostFilename(
							        fullname);
							if (path_exists(host_name)) {
								final_path = std::move(
								        host_name);
								found_on_virtual = true;
								LOG_MSG("IMGMOUNT: Path '%s' found on virtual drive %c:",
								        fullname,
								        drive_letter(dummy));
							}
						}
					}
				}
				if (!found_on_virtual) {
					// Try wildcards if strictly not found
					if (MOUNT::AddWildcardPaths(cur_arg,
					                            params.paths)) {
						continue;
					}
				}
			}

			// Auto-detect type from FIRST valid file if generic "dir"
			if (params.paths.empty() && params.type == "dir") {
				// Check if actually file
				struct stat t2;
				if (stat(final_path.c_str(), &t2) == 0 &&
				    S_ISREG(t2.st_mode)) {
					auto ext = final_path.substr(
					        final_path.find_last_of('.') + 1);
					std::transform(ext.begin(),
					               ext.end(),
					               ext.begin(),
					               ::tolower);
					if (ext == "iso" || ext == "cue" ||
					    ext == "bin" || ext == "mds" ||
					    ext == "ccd") {
						params.type   = "iso";
						params.fstype = "iso";
					} else if (ext == "img" ||
					           ext == "ima" || ext == "vhd") {
						params.type = "hdd";
					}
				}
			}

			// Resolves to absolute canonical path
			final_path = simplify_path(final_path).string();
			params.paths.push_back(final_path);
		}

		if (params.paths.empty()) {
			NOTIFY_DisplayWarning(Notification::Source::Console,
			                      "MOUNT",
			                      "PROGRAM_IMGMOUNT_FILE_NOT_FOUND");
			return false;
		}

		// Ensure consistency between type and fstype if user didn't
		// override -fs
		if (params.type == "floppy" && params.fstype == "fat") {
			params.mediaid = MediaId::Floppy1_44MB;
		}

		bool success = MountImage(params);
		if (success && params.type == "floppy") {
			incrementFDD();
		}
		return true;
	}

	// Standard directory or overlay mount
	if (!S_ISDIR(test.st_mode)) {
		NOTIFY_DisplayWarning(Notification::Source::Console,
		                      "MOUNT",
		                      "PROGRAM_MOUNT_ERROR_2",
		                      final_path.c_str());
		return false;
	}

	MountLocal(params, final_path);
	return true;
}

void MOUNT::MountLocal(MountParameters& params, const std::string& local_path)
{
	std::string final_path = local_path;
	if (!final_path.empty() && final_path.back() != CROSS_FILESPLIT) {
		final_path += CROSS_FILESPLIT;
	}

	// Give a warning when mount c:\ or the / root directory
#if defined(WIN32)
	if ((final_path == "c:\\") || (final_path == "C:\\") ||
	    (final_path == "c:/") || (final_path == "C:/")) {
		NOTIFY_DisplayWarning(Notification::Source::Console,
		                      "MOUNT",
		                      "PROGRAM_MOUNT_WARNING_WIN");
	}
#else
	if (final_path == "/") {
		NOTIFY_DisplayWarning(Notification::Source::Console,
		                      "MOUNT",
		                      "PROGRAM_MOUNT_WARNING_OTHER");
	}
#endif

	std::shared_ptr<DOS_Drive> newdrive = {};
	const uint8_t int8_tize             = (uint8_t)params.sizes[1];

	if (params.type == "overlay") {
		const auto ldp = std::dynamic_pointer_cast<localDrive>(
		        Drives[drive_index(params.drive)]);
		const auto cdp = std::dynamic_pointer_cast<cdromDrive>(
		        Drives[drive_index(params.drive)]);
		if (!ldp || cdp) {
			NOTIFY_DisplayWarning(Notification::Source::Console,
			                      "MOUNT",
			                      "PROGRAM_MOUNT_OVERLAY_INCOMPAT_BASE");
			return;
		}
		std::string base = ldp->GetBasedir();
		uint8_t o_error  = 0;
		newdrive         = std::make_shared<Overlay_Drive>(base.c_str(),
                                                           final_path.c_str(),
                                                           params.sizes[0],
                                                           int8_tize,
                                                           params.sizes[2],
                                                           params.sizes[3],
                                                           params.mediaid,
                                                           o_error);
		// Erase old drive on success
		if (o_error) { //-V547
			if (o_error == 1) {
				NOTIFY_DisplayWarning(Notification::Source::Console,
				                      "MOUNT",
				                      "PROGRAM_MOUNT_OVERLAY_REL_ABS");
			} else if (o_error == 2) {
				NOTIFY_DisplayWarning(Notification::Source::Console,
				                      "MOUNT",
				                      "PROGRAM_MOUNT_OVERLAY_SAME_FS");
			} else {
				NOTIFY_DisplayWarning(Notification::Source::Console,
				                      "MOUNT",
				                      "PROGRAM_MOUNT_OVERLAY_UNKNOWN_ERROR");
			}
			return;
		}
		// Copy current directory if not marked as deleted.
		if (newdrive->TestDir(ldp->curdir)) {
			safe_strcpy(newdrive->curdir, ldp->curdir);
		}
		Drives.at(drive_index(params.drive)) = nullptr;
	} else {
		const auto section = get_section("dosbox");
		// Standard directory mount
		newdrive = std::make_shared<localDrive>(
		        final_path.c_str(),
		        params.sizes[0],
		        int8_tize,
		        params.sizes[2],
		        params.sizes[3],
		        params.mediaid,
		        params.roflag,
		        section->GetBool("allow_write_protected_files"));
	}

	DriveManager::RegisterFilesystemImage(drive_index(params.drive), newdrive);
	Drives.at(drive_index(params.drive)) = newdrive;

	// Set the correct media byte in the table
	mem_writeb(RealToPhysical(dos.tables.mediaid) +
	                   (drive_index(params.drive)) * 9,
	           newdrive->GetMediaByte());

	if (params.type != "overlay") {
		WriteOut(MSG_Get("PROGRAM_MOUNT_STATUS_2"),
		         newdrive->GetInfoString().c_str(),
		         params.drive);
		if (params.roflag) {
			WriteOut(MSG_Get("PROGRAM_MOUNT_READONLY"));
		}
	} else {
		WriteOut(MSG_Get("PROGRAM_MOUNT_OVERLAY_STATUS"),
		         final_path.c_str(),
		         params.drive);
	}

	// check if volume label is given and don't allow it to updated in the
	// future
	if (!params.label.empty()) {
		newdrive->dirCache.SetLabel(params.label.c_str(), false, false);

	} else if (params.type == "dir" || params.type == "overlay") {
		// For hard drives set the label to DRIVELETTER_Drive.
		// For floppy drives set the label to DRIVELETTER_Floppy.
		// This way every drive except cdroms should get a label.
		params.label = params.drive;
		params.label += "_DRIVE";
		newdrive->dirCache.SetLabel(params.label.c_str(), false, false);

	} else if (params.type == "floppy") {
		// Floppy labels handled in IMGMOUNT logic usually, but if
		// mounted as dir:
		params.label = params.drive;
		params.label += "_FLOPPY";
		newdrive->dirCache.SetLabel(params.label.c_str(), false, true);
	}

	// We incrementFDD here only if it was a directory mount pretending to
	// be floppy Image mounts return early.
	if (params.type == "floppy") {
		incrementFDD();
	}
}

void MOUNT::Run(void)
{
	MountParameters params;

	// Hack to allow long commandlines
	ChangeToLongCmd();

	if (!cmd->GetCount()) {
		ListMounts();
		return;
	}
	if (HelpRequested()) {
		ShowUsage();
		return;
	}
	if (control->SecureMode()) {
		NOTIFY_DisplayWarning(Notification::Source::Console,
		                      "MOUNT",
		                      "PROGRAM_CONFIG_SECURE_DISALLOW");
		return;
	}

	// Check for unmounting
	if (HandleUnmount()) {
		return;
	}

	bool explicit_fs                  = false;
	bool path_relative_to_last_config = false;

	// Parse command line arguments
	ParseArguments(params, explicit_fs, path_relative_to_last_config);
	ParseGeometry(params);

	if (!ParseDrive(params, explicit_fs)) {
		return;
	}

	// Parse paths and execute (MountImage or MountLocal)
	ProcessPaths(params, path_relative_to_last_config);
}

void MOUNT::AddMessages()
{
	AddCommonMountMessages();
	if (MSG_Exists("PROGRAM_MOUNT_HELP")) {
		return;
	}
	MSG_Add("PROGRAM_MOUNT_HELP",
	        "Mount a directory or an image file to a drive letter.\n");

	MSG_Add("PROGRAM_MOUNT_HELP_LONG",
	        "Mount a directory or an image file to a drive letter.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=light-green]mount[reset] [color=white]DRIVE[reset] [color=light-cyan]PATH[reset] [PARAMETERS]\n"
	        "  [color=light-green]mount[reset] [color=white]DRIVE[reset] [color=light-cyan]IMAGEFILE[reset] [IMAGEFILE2...] [PARAMETERS]\n"
	        "  [color=light-green]mount[reset] [color=white]DRIVE[reset] [color=light-cyan]IMAGE-SET[reset] [PARAMETERS]\n"
	        "  [color=light-green]mount[reset] -u [color=white]DRIVE[reset]  (unmounts [color=white]DRIVE[reset])\n"
	        "\n"
	        "Common parameters:\n"
	        "  [color=white]DRIVE[reset]           drive letter (A-Z) to mount to\n"
	        "  [color=light-cyan]PATH[reset]            directory on the host OS (absolute or relative path)\n"
	        "  [color=light-cyan]IMAGEFILE[reset]       image file on the host OS (absolute or relative path) or on a\n"
	        "                  mounted DOS drive (e.g. C:\\GAME.ISO)\n"
	        "  [color=light-cyan]IMAGE-SET[reset]       ISO, CUE+BIN, CUE+ISO, or CUE+ISO+FLAC/OPUS/OGG/MP3/WAV\n"
	        "\n"
	        "  -t [color=white]TYPE[reset]         type of mount: [color=light-cyan]dir[reset], [color=light-cyan]overlay[reset], [color=light-cyan]floppy[reset], [color=light-cyan]hdd[reset], [color=light-cyan]iso[reset] (or [color=light-cyan]cdrom[reset])\n"
	        "  -fs [color=white]FS[reset]          filesystem: [color=light-cyan]fat[reset], [color=light-cyan]iso[reset], or [color=light-cyan]none[reset] (for bootable images)\n"
	        "  -label [color=white]LABEL[reset]    volume label to assign to the mounted drive\n"
	        "  -ro             mount as read-only\n"
	        "\n"
	        "Directory mount parameters:\n"
	        "  -freesize [color=white]SIZE[reset]  size (in KB for floppies, in MB for hard disks); sets the\n"
	        "                  amount of free space available on the drive (~250 MB by\n"
	        "                  default for HDD directory mounts)\n"
	        "  -t [color=light-cyan]overlay[reset]      mounts the directory as an overlay on top of an existing drive\n"
	        "\n"
	        "Image parameters:\n"
	        "  -chs [color=white]C,H,S[reset]      specify geometry ([color=white]C[reset]ylinders,[color=white]H[reset]eads,[color=white]S[reset]ectors) for HDD images\n"
	        "  -size [color=white]B,S,H,C[reset]   specify geometry ([color=white]B[reset]ytesPerSector,[color=white]S[reset]ectors,[color=white]H[reset]eads,[color=white]C[reset]ylinders);\n"
	        "                  alternative to -chs for HDD images\n"
	        "  -ide            attach as IDE device (for CD-ROM and HDD images)\n"
	        "  -pr             path is relative to the configuration file location\n"
	        "\n"
	        "Notes:\n"
	        "  - Use wildcards or multiple image files to mount them at the same drive\n"
	        "    letter, then press [color=yellow]%s+F4[reset] to cycle between them. This is useful for\n"
	        "    programs that require swapping disks while running.\n"
	        "\n"
	        "  - The -ide flag emulates an IDE controller for an attached HDD or CD drive\n"
	        "    for CD-based games that need a real DOS environment via a bootable HDD\n"
	        "    image.\n"
	        "\n"
	        "  - Type [color=light-cyan]overlay[reset] requires [color=white]DRIVE[reset] to be already mounted. It mounts [color=light-cyan]PATH[reset] on the\n"
	        "    host OS as a write-layer over the drive. Modified files are stored in [color=light-cyan]PATH[reset],\n"
	        "    leaving the original drive data unchanged.\n"
	        "\n"
	        "Examples:\n");

	MSG_Add("PROGRAM_MOUNT_HELP_LONG_WIN32",
	        "  [color=light-green]mount[reset] [color=white]C[reset] [color=light-cyan]C:\\dosgames[reset]\n"
	        "  [color=light-green]mount[reset] [color=white]C[reset] [color=light-cyan]C:\\dosgamesoverlay[reset] -t overlay\n"
	        "  [color=light-green]mount[reset] [color=white]D[reset] [color=light-cyan]D:\\Games\\doom.iso[reset] -t iso\n"
	        "  [color=light-green]mount[reset] [color=white]2[reset] [color=light-cyan]Win95.img[reset] -t hdd -fs none -chs 304,64,63\n"
	        "  [color=light-green]mount[reset] [color=white]0[reset] [color=light-cyan]floppy.img[reset] -t floppy -fs none\n"
	        "  [color=light-green]mount[reset] [color=white]A[reset] [color=light-cyan]floppy*.img[reset] -t floppy\n"
	        "  [color=light-green]mount[reset] [color=white]A[reset] [color=light-cyan]disk01.img disk02.img[reset] -t floppy\n");

	MSG_Add("PROGRAM_MOUNT_HELP_LONG_MACOSX",
	        "  (~ is expanded to your home directory)\n"
	        "  [color=light-green]mount[reset] [color=white]C[reset] [color=light-cyan]~/dosgames[reset]\n"
	        "  [color=light-green]mount[reset] [color=white]C[reset] [color=light-cyan]~/dosgamesoverlay[reset] -t overlay\n"
	        "  [color=light-green]mount[reset] [color=white]D[reset] [color=light-cyan]~/Games/doom.iso[reset] -t iso\n"
	        "  [color=light-green]mount[reset] [color=white]2[reset] [color=light-cyan]Win95.img[reset] -t hdd -fs none -chs 304,64,63\n"
	        "  [color=light-green]mount[reset] [color=white]0[reset] [color=light-cyan]floppy.img[reset] -t floppy -fs none\n"
	        "  [color=light-green]mount[reset] [color=white]A[reset] [color=light-cyan]floppy*.img[reset] -t floppy -ro\n"
	        "  [color=light-green]mount[reset] [color=white]A[reset] [color=light-cyan]disk01.img disk02.img[reset] -t floppy\n");

	MSG_Add("PROGRAM_MOUNT_HELP_LONG_OTHER",
	        "  (~ is expanded to your home directory)\n"
	        "  [color=light-green]mount[reset] [color=white]C[reset] [color=light-cyan]~/dosgames[reset]\n"
	        "  [color=light-green]mount[reset] [color=white]C[reset] [color=light-cyan]~/dosgamesoverlay[reset] -t overlay\n"
	        "  [color=light-green]mount[reset] [color=white]D[reset] [color=light-cyan]~/Games/doom.iso[reset] -t iso\n"
	        "  [color=light-green]mount[reset] [color=white]2[reset] [color=light-cyan]Win95.img[reset] -t hdd -fs none -chs 304,64,63\n"
	        "  [color=light-green]mount[reset] [color=white]0[reset] [color=light-cyan]floppy.img[reset] -t floppy -fs none\n"
	        "  [color=light-green]mount[reset] [color=white]A[reset] [color=light-cyan]floppy*.img[reset] -t floppy -ro\n"
	        "  [color=light-green]mount[reset] [color=white]A[reset] [color=light-cyan]disk01.img disk02.img[reset] -t floppy\n");

	MSG_Add("PROGRAM_MOUNT_CDROMS_FOUND", "CD-ROMs found: %d\n");
	MSG_Add("PROGRAM_MOUNT_ERROR_1", "Directory or file %s doesn't exist.\n");

	MSG_Add("PROGRAM_MOUNT_ERROR_2",
	        "%s isn't a directory or valid image file.\n");

	MSG_Add("PROGRAM_MOUNT_ILL_TYPE", "Illegal type %s\n");
	MSG_Add("PROGRAM_MOUNT_ALREADY_MOUNTED", "Drive %c already mounted with %s\n");
	MSG_Add("PROGRAM_MOUNT_UMOUNT_NOT_MOUNTED", "Drive %c isn't mounted.\n");

	MSG_Add("PROGRAM_MOUNT_UMOUNT_SUCCESS",
	        "Drive %c has successfully been removed.\n");

	MSG_Add("PROGRAM_MOUNT_UMOUNT_NO_VIRTUAL",
	        "Virtual Drives can not be unMOUNTed.\n");

	MSG_Add("PROGRAM_MOUNT_DRIVEID_ERROR",
	        "'%c' is not a valid drive identifier.\n");

	MSG_Add("PROGRAM_MOUNT_WARNING_WIN",
	        "[color=light-red]Mounting c:\\ is NOT recommended. Please mount a (sub)directory next time.[reset]\n");

	MSG_Add("PROGRAM_MOUNT_WARNING_OTHER",
	        "[color=light-red]Mounting / is NOT recommended. Please mount a (sub)directory next time.[reset]\n");

	MSG_Add("PROGRAM_MOUNT_NO_OPTION",
	        "Warning: Ignoring unsupported option '%s'.\n");

	MSG_Add("PROGRAM_MOUNT_OVERLAY_NO_BASE",
	        "A normal directory needs to be MOUNTed first before an overlay can be added on\n"
	        "top.\n");

	MSG_Add("PROGRAM_MOUNT_OVERLAY_INCOMPAT_BASE",
	        "The overlay is NOT compatible with the drive that is specified.\n");

	MSG_Add("PROGRAM_MOUNT_OVERLAY_MIXED_BASE",
	        "The overlay needs to be specified using the same addressing as the underlying\n"
	        "drive. No mixing of relative and absolute paths.\n");

	MSG_Add("PROGRAM_MOUNT_OVERLAY_SAME_AS_BASE",
	        "The overlay directory can not be the same as underlying drive.\n");

	MSG_Add("PROGRAM_MOUNT_OVERLAY_GENERIC_ERROR", "Something went wrong.\n");
	MSG_Add("PROGRAM_MOUNT_OVERLAY_STATUS", "Overlay %s on drive %c mounted.\n");

	MSG_Add("PROGRAM_MOUNT_INVALID_CHS",
	        "Invalid CHS format. Use -chs cylinders,heads,sectors\n");

	MSG_Add("PROGRAM_MOUNT_OVERLAY_REL_ABS",
	        "The overlay needs to be specified using the same addressing as the underlying\n"
	        "drive. No mixing of relative and absolute paths.\n");

	MSG_Add("PROGRAM_MOUNT_OVERLAY_SAME_FS",
	        "The overlay needs to be on the same filesystem as the underlying drive.\n");

	MSG_Add("PROGRAM_MOUNT_OVERLAY_UNKNOWN_ERROR", "Something went wrong.\n");

	// IMGMOUNT merged messages
	MSG_Add("PROGRAM_IMGMOUNT_SPECIFY2",
	        "Must specify a drive letter A/B/C/D or drive number 0/1/2/3 to mount image at.\n");

	MSG_Add("PROGRAM_IMGMOUNT_SPECIFY_GEOMETRY",
	        "For hard drive images, drive geometry must be specified:\n"
	        "  [color=light-green]imgmount[reset] [color=white]DRIVE[reset] [color=light-cyan]IMAGEFILE[reset] -chs Cylinders,Heads,Sectors\n"
	        "Alternatively:\n"
	        "  [color=light-green]imgmount[reset] [color=white]DRIVE[reset] [color=light-cyan]IMAGEFILE[reset] -size BytesPerSector,Sectors,Heads,Cylinders\n"
	        "For CD-ROM images:\n"
	        "  [color=light-green]imgmount[reset] [color=white]DRIVE[reset] [color=light-cyan]IMAGEFILE[reset] -t iso\n");

	MSG_Add("PROGRAM_IMGMOUNT_STATUS_NONE", "No drive available.\n");

	MSG_Add("PROGRAM_IMGMOUNT_IDE_CONTROLLERS_UNAVAILABLE",
	        "No available IDE controllers. Drive will not have IDE emulation.\n");

	MSG_Add("PROGRAM_IMGMOUNT_INVALID_IMAGE",
	        "Could not load image file.\n"
	        "Check that the path is correct and the image is accessible.\n");

	MSG_Add("PROGRAM_IMGMOUNT_INVALID_GEOMETRY",
	        "Could not extract drive geometry from image.\n"
	        "Use parameter -chs Cylinders,Heads,Sectors to specify the geometry.\n"
	        "Alternatively: -size BytesPerSector,Sectors,Heads,Cylinders\n");

	MSG_Add("PROGRAM_IMGMOUNT_FILE_NOT_FOUND", "Image file not found.\n");

	MSG_Add("PROGRAM_IMGMOUNT_ALREADY_MOUNTED",
	        "Drive already mounted at that letter.\n");

	MSG_Add("PROGRAM_IMGMOUNT_CANT_CREATE", "Can't create drive from file.\n");
	MSG_Add("PROGRAM_IMGMOUNT_MOUNT_NUMBER", "Drive number %d mounted as %s.\n");

	MSG_Add("PROGRAM_IMGMOUNT_DEPRECATED",
	        "[color=yellow]Note: 'imgmount' is deprecated.[reset]\n"
	        "Use 'mount' for both directories and disk images.");
}
