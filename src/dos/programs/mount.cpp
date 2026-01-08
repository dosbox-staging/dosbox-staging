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

	const int term_width   = real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS);
	const auto width_drive = static_cast<int>(header_drive.length());
	const auto width_label = std::max(minimum_column_length,
	                                  static_cast<int>(header_label.size()));
	const auto width_type  = term_width - 3 - width_drive - width_label;
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
	const std::string horizontal_divider(term_width, '-');
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
	constexpr auto only_expand_files        = true;
	constexpr auto skip_native_path         = true;
	std::vector<std::string> expanded_paths = {};
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

bool MOUNT::MountImage(char drive, const std::vector<std::string>& paths,
                       const std::string& type, const std::string& fstype,
                       uint16_t* sizes, bool roflag, bool wants_ide,
                       int8_t ide_index, bool is_second_cable_slot,
                       std::function<void(std::string)> logger)
{
	// Format strings like WriteOut does, but sending to the provided logger
	auto log_fmt = [&](const char* format, ...) {
		char buf[2048];
		va_list args;
		va_start(args, format);
		vsnprintf(buf, sizeof(buf), format, args);
		va_end(args);
		logger(std::string(buf));
	};

	const uint8_t mediaid = (type == "floppy") ? MediaId::Floppy1_44MB
	                                           : MediaId::HardDisk;

	auto write_out_mount_status = [&](const char* image_type,
	                                  const std::vector<std::string>& images,
	                                  const char drive_letter) {
		constexpr auto end_punctuation        = "";
		const auto images_str                 = join_with_commas(images,
                                                         MSG_Get("CONJUNCTION_AND"),
                                                         end_punctuation);
		const std::string type_and_images_str = image_type +
		                                        std::string(" ") + images_str;

		log_fmt(MSG_Get("PROGRAM_MOUNT_STATUS_2").c_str(),
		        type_and_images_str.c_str(),
		        drive_letter);
	};

	if (fstype == "fat") {
		bool imgsizedetect = (type == "hdd") &&
		                     (sizes[0] == 0 && sizes[1] == 0 &&
		                      sizes[2] == 0 && sizes[3] == 0);

		if (imgsizedetect) {
			FILE* diskfile = fopen_wrap_ro_fallback(paths[0], roflag);
			if (!diskfile) {
				log_fmt(MSG_Get("PROGRAM_IMGMOUNT_INVALID_IMAGE")
				                .c_str());
				return false;
			}
			const auto sz = stdio_num_sectors(diskfile);
			if (sz < 0) {
				fclose(diskfile);
				log_fmt(MSG_Get("PROGRAM_IMGMOUNT_INVALID_IMAGE")
				                .c_str());
				return false;
			}
			uint32_t fcsize = check_cast<uint32_t>(sz);
			uint8_t buf[512];
			if (cross_fseeko(diskfile, 0L, SEEK_SET) != 0 ||
			    fread(buf, sizeof(uint8_t), 512, diskfile) < 512) {
				fclose(diskfile);
				log_fmt(MSG_Get("PROGRAM_IMGMOUNT_INVALID_IMAGE")
				                .c_str());
				return false;
			}
			fclose(diskfile);
			if ((buf[510] != 0x55) || (buf[511] != 0xaa)) {
				log_fmt(MSG_Get("PROGRAM_IMGMOUNT_INVALID_GEOMETRY")
				                .c_str());
				return false;
			}
			Bitu sectors = (Bitu)(fcsize / (16 * 63));
			if (sectors * 16 * 63 != fcsize) {
				log_fmt(MSG_Get("PROGRAM_IMGMOUNT_INVALID_GEOMETRY")
				                .c_str());
				return false;
			}
			sizes[0] = 512;
			sizes[1] = 63;
			sizes[2] = 16;
			sizes[3] = static_cast<uint16_t>(sectors);

			LOG_MSG("autosized image file: %d:%d:%d:%d",
			        sizes[0],
			        sizes[1],
			        sizes[2],
			        sizes[3]);
		}

		if (Drives.at(drive_index(drive))) {
			log_fmt(MSG_Get("PROGRAM_IMGMOUNT_ALREADY_MOUNTED").c_str());
			return false;
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
				log_fmt(MSG_Get("PROGRAM_IMGMOUNT_CANT_CREATE").c_str());
				return false;
			}
		}
		if (fat_images.empty()) {
			log_fmt(MSG_Get("PROGRAM_IMGMOUNT_CANT_CREATE").c_str());
			return false;
		}

		// Update DriveManager
		DriveManager::AppendFilesystemImages(drive_index(drive), fat_images);
		DriveManager::InitializeDrive(drive_index(drive));

		// Set the correct media byte in the table
		mem_writeb(RealToPhysical(dos.tables.mediaid) + drive_index(drive) * 9,
		           mediaid);

		// Command uses dta so set it to our internal dta
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
			log_fmt(MSG_Get("PROGRAM_IMGMOUNT_ALREADY_MOUNTED").c_str());
			return false;
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
				log_fmt(MSG_Get("MSCDEX_ERROR_MULTIPLE_CDROMS").c_str());
				break;
			case 2:
				log_fmt(MSG_Get("MSCDEX_ERROR_NOT_SUPPORTED").c_str());
				break;
			case 3:
				log_fmt(MSG_Get("MSCDEX_ERROR_OPEN").c_str());
				break;
			case 4:
				log_fmt(MSG_Get("MSCDEX_TOO_MANY_DRIVES").c_str());
				break;
			case 5:
				log_fmt(MSG_Get("MSCDEX_LIMITED_SUPPORT").c_str());
				break;
			case 6:
				log_fmt(MSG_Get("MSCDEX_INVALID_FILEFORMAT").c_str());
				break;
			default:
				log_fmt(MSG_Get("MSCDEX_UNKNOWN_ERROR").c_str());
				break;
			}
			// error: clean up and leave
			if (error) { //-V547
				log_fmt(MSG_Get("PROGRAM_IMGMOUNT_CANT_CREATE").c_str());
				return false;
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
				log_fmt(MSG_Get("PROGRAM_IMGMOUNT_IDE_CONTROLLERS_UNAVAILABLE")
				                .c_str());
			}
		}

		// Print status message (success)
		log_fmt(MSG_Get("MSCDEX_SUCCESS").c_str());
		write_out_mount_status(MSG_Get("MOUNT_TYPE_ISO").c_str(), paths, drive);

	} else if (fstype == "none") {
		FILE* new_disk = fopen_wrap_ro_fallback(paths[0], roflag);
		if (!new_disk) {
			log_fmt(MSG_Get("PROGRAM_IMGMOUNT_INVALID_IMAGE").c_str());
			return false;
		}
		const auto sz = stdio_size_kb(new_disk);
		if (sz < 0) {
			fclose(new_disk);
			log_fmt(MSG_Get("PROGRAM_IMGMOUNT_INVALID_IMAGE").c_str());
			return false;
		}
		uint32_t imagesize = check_cast<uint32_t>(sz);
		const bool is_hdd  = (imagesize > 2880);
		// Seems to make sense to require a valid geometry..
		if (is_hdd && sizes[0] == 0 && sizes[1] == 0 && sizes[2] == 0 &&
		    sizes[3] == 0) {
			fclose(new_disk);
			log_fmt(MSG_Get("PROGRAM_IMGMOUNT_SPECIFY_GEOMETRY").c_str());
			return false;
		}

		const auto drv_idx = drive - '0';

		imageDiskList.at(drv_idx) = std::make_shared<imageDisk>(
		        new_disk, paths[0].c_str(), imagesize, is_hdd);

		if (is_hdd) {
			imageDiskList.at(drv_idx)->Set_Geometry(sizes[2],
			                                        sizes[3],
			                                        sizes[1],
			                                        sizes[0]);
		}

		if ((drive == '2' || drive == '3') && is_hdd) {
			updateDPT();
		}

		log_fmt(MSG_Get("PROGRAM_IMGMOUNT_MOUNT_NUMBER").c_str(),
		        drv_idx,
		        paths[0].c_str());
	}

	return true;
}

void MOUNT::Run(void) {
	std::shared_ptr<DOS_Drive> newdrive = {};
	char drive                          = '\0';
	std::string label                   = {};
	std::string umount                  = {};

	// Hack To allow long commandlines
	ChangeToLongCmd();
	// Parse the command line
	// if the command line is empty show current mounts
	if (!cmd->GetCount()) {
		ListMounts();
		return;
	}
	// Print help if requested.
	if (HelpRequested()) {
		ShowUsage();
		return;
	}

	// In secure mode don't allow people to change mount points.
	// Neither mount nor unmount
	if (control->SecureMode()) {
		WriteOut(MSG_Get("PROGRAM_CONFIG_SECURE_DISALLOW"));
		return;
	}

	bool path_relative_to_last_config = false;
	if (cmd->FindExist("-pr", true)) {
		path_relative_to_last_config = true;
	}

	// Check for unmounting
	if (cmd->FindString("-u", umount, false)) {
		WriteOut(UnmountHelper(umount[0]), toupper(umount[0]));
		return;
	}

	const auto section = get_section("dosbox");
	assert(section);

	std::string type = "dir";
	cmd->FindString("-t", type, true);

	// Allow aliases or standard image types to pass through
	if (type == "cdrom") {
		type = "iso";
	}

	const bool readonly = cmd->FindExist("-ro", true);

	// Parse -fs (filesystem type)
	std::string fstype = "fat";
	if (type == "iso") {
		fstype = "iso";
	}

	// Check if user explicitly set -fs
	bool explicit_fs = cmd->FindString("-fs", fstype, true);

	// Parse -ide
	std::string ide_value     = {};
	int8_t ide_index          = -1;
	bool is_second_cable_slot = false;
	const bool wants_ide      = cmd->FindString("-ide", ide_value, true) ||
	                       cmd->FindExist("-ide", true);
	if (wants_ide && (type == "iso")) {
		IDE_Get_Next_Cable_Slot(ide_index, is_second_cable_slot);
	}

	// Prepare sizes/geometry
	uint16_t sizes[4]    = {0};
	uint8_t mediaid      = MediaId::HardDisk;
	std::string str_size = "";
	std::string str_chs  = "";

	// Default sizing logic based on type
	if (type == "floppy") {
		str_size = "512,1,2880,2880";
		mediaid  = MediaId::Floppy1_44MB;
	} else if (type == "dir" || type == "overlay") {
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
				mediaid = MediaId::Floppy1_44MB;
			}
		}
	} else if (type == "iso") {
		str_size = "2048,1,65535,0";
	} else if (type != "hdd") {
		// If it is 'hdd', we leave sizes 0 to trigger detection or
		// parsing below. If it is unknown, we error out later.
		WriteOut(MSG_Get("PROGRAM_MOUNT_ILL_TYPE"), type.c_str());
		return;
	}

	// Parse the free space in mb (kb for floppies)
	std::string mb_size;
	if (cmd->FindString("-freesize", mb_size, true)) {
		char teststr[1024];
		uint16_t freesize = static_cast<uint16_t>(atoi(mb_size.c_str()));
		if (type == "floppy") {
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
		Bitu index       = 0;
		Bitu count       = 0;
		// Parse the str_size string
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
			// always goes correct as index is max 20 at this point.
			number[index] = 0;
			sizes[count]  = atoi(number);
		}
	}

	// Parse -chs C,H,S
	if (cmd->FindString("-chs", str_chs, true)) {
		int cmd_cylinders = 0, cmd_heads = 0, cmd_sectors = 0;
		if (sscanf(str_chs.c_str(), "%d,%d,%d", &cmd_cylinders, &cmd_heads, &cmd_sectors) ==
		    3) {
			sizes[0] = 512;
			sizes[1] = static_cast<uint16_t>(cmd_sectors);
			sizes[2] = static_cast<uint16_t>(cmd_heads);
			sizes[3] = static_cast<uint16_t>(cmd_cylinders);
		} else {
			WriteOut("Invalid CHS format. Use -chs cylinders,heads,sectors\n");
			return;
		}
	}

	// get the drive letter or number
	cmd->FindCommand(1, temp_line);
	if ((temp_line.size() > 2) ||
	    ((temp_line.size() > 1) && (temp_line[1] != ':'))) {
		ShowUsage();
		return;
	}

	const char first_char = toupper(temp_line[0]);
	bool is_drive_number  = false;

	if (isdigit(first_char)) {
		// Handle Drive Numbers (0, 1, 2, 3) for bootable images
		if (first_char >= '0' && first_char <= '3') {
			drive           = first_char;
			is_drive_number = true;

			// If user did not explicit specify filesystem, assume
			// 'none' for raw bootable access
			if (!explicit_fs) {
				fstype = "none";
			}
		} else {
			WriteOut(MSG_Get("PROGRAM_IMGMOUNT_SPECIFY2"));
			return;
		}
	} else if (first_char >= 'A' && first_char <= 'Z') {
		drive = first_char;
	} else {
		ShowUsage();
		return;
	}

	// Check overlaps
	if (!is_drive_number) {
		if (type == "overlay") {
			// Ensure that the base drive exists:
			if (!Drives.at(drive_index(drive))) {
				WriteOut(MSG_Get("PROGRAM_MOUNT_OVERLAY_NO_BASE"));
				return;
			}
		} else if (Drives.at(drive_index(drive))) {
			WriteOut(MSG_Get("PROGRAM_MOUNT_ALREADY_MOUNTED"),
			         drive,
			         Drives.at(drive_index(drive))->GetInfoString().c_str());
			return;
		}
	}

	// Get the first path argument
	if (!cmd->FindCommand(2, temp_line)) {
		ShowUsage();
		return;
	}
	if (!temp_line.size()) {
		ShowUsage();
		return;
	}

	// Expand ~ to home directory
	temp_line = resolve_home(temp_line).string();

	// Resolve first path (needed for file/dir check)
	std::string path_arg_1 = temp_line;
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
	/* Removing trailing backslash if not root dir so stat will succeed */
	if (path_arg_1.size() > 3 && path_arg_1.back() == '\\') {
		path_arg_1.pop_back();
	}
#endif

	// Check first path
	struct stat test;
	bool stat_ok = (stat(path_arg_1.c_str(), &test) == 0);
	bool target_is_dir       = stat_ok && S_ISDIR(test.st_mode);
	bool explicit_image_type = (type == "hdd" || type == "iso" ||
	                            type == "floppy");

	bool is_image_mode = false;
	// Explicit triggers
	if (explicit_image_type || is_drive_number) {
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
		std::vector<std::string> image_paths;

		// Loop through ALL remaining arguments (indexes 2, 3, ...)
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

			// Try to find the path on native filesystem first
			auto real_path         = to_native_path(cur_arg);
			std::string final_path = real_path.empty() ? cur_arg
			                                           : real_path;

			if (real_path.empty() || !path_exists(real_path)) {

				// Try Virtual DOS Drive mapping (IMPORTANT for
				// 'imgmount d c:\iso.cue')
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
								final_path = host_name;
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
					                            image_paths)) {
						continue;
					}
				}
			}

			// Auto-detect type from FIRST valid file if generic "dir"
			if (image_paths.empty() && type == "dir") {
				// Check if actually file
				struct stat t2;
				if (stat(final_path.c_str(), &t2) == 0 &&
				    S_ISREG(t2.st_mode)) {
					auto ext = final_path.substr(
					        final_path.find_last_of(".") + 1);
					std::transform(ext.begin(),
					               ext.end(),
					               ext.begin(),
					               ::tolower);
					if (ext == "iso" || ext == "cue" ||
					    ext == "bin" || ext == "mds" ||
					    ext == "ccd") {
						type   = "iso";
						fstype = "iso";
					} else if (ext == "img" ||
					           ext == "ima" || ext == "vhd") {
						type = "hdd";
					}
				}
			}

			// Resolves to absolute canonical path
			final_path = simplify_path(final_path).string();
			image_paths.push_back(final_path);
		}

		if (image_paths.empty()) {
			WriteOut(MSG_Get("PROGRAM_IMGMOUNT_FILE_NOT_FOUND"));
			return;
		}

		// Ensure consistency between type and fstype if user didn't
		// override -fs
		if (type == "floppy" && fstype == "fat") {
			mediaid = MediaId::Floppy1_44MB;
		}

		bool success = this->MountImage(drive,
		                                image_paths,
		                                type,
		                                fstype,
		                                sizes,
		                                readonly,
		                                wants_ide,
		                                ide_index,
		                                is_second_cable_slot,
		                                [this](std::string msg) {
			                                this->WriteOut_NoParsing(msg);
		                                });

		if (success && type == "floppy") {
			incrementFDD();
		}
		return;
	}

	// Standard directory or overlay mount

	if (!S_ISDIR(test.st_mode)) {
		WriteOut(MSG_Get("PROGRAM_MOUNT_ERROR_2"), temp_line.c_str());
		return;
	}

	if (temp_line.back() != CROSS_FILESPLIT) {
		temp_line += CROSS_FILESPLIT;
	}
	uint8_t int8_tize = (uint8_t)sizes[1];

	// Give a warning when mount c:\ or the / root directory
#if defined(WIN32)
	if ((temp_line == "c:\\") || (temp_line == "C:\\") ||
	    (temp_line == "c:/") || (temp_line == "C:/")) {
		WriteOut(MSG_Get("PROGRAM_MOUNT_WARNING_WIN"));
	}
#else
	if (temp_line == "/") {
		WriteOut(MSG_Get("PROGRAM_MOUNT_WARNING_OTHER"));
	}
#endif

	if (type == "overlay") {
		const auto ldp = std::dynamic_pointer_cast<localDrive>(
		        Drives[drive_index(drive)]);
		const auto cdp = std::dynamic_pointer_cast<cdromDrive>(
		        Drives[drive_index(drive)]);
		if (!ldp || cdp) {
			WriteOut(MSG_Get("PROGRAM_MOUNT_OVERLAY_INCOMPAT_BASE"));
			return;
		}
		std::string base = ldp->GetBasedir();
		uint8_t o_error  = 0;
		newdrive         = std::make_shared<Overlay_Drive>(base.c_str(),
                                                           temp_line.c_str(),
                                                           sizes[0],
                                                           int8_tize,
                                                           sizes[2],
                                                           sizes[3],
                                                           mediaid,
                                                           o_error);
		// Erase old drive on success
		if (o_error) { //-V547
			if (o_error == 1) {
				WriteOut("No mixing of relative and absolute paths. Overlay failed.");
			} else if (o_error == 2) {
				WriteOut("overlay directory can not be the same as underlying file system.");
			} else {
				WriteOut("Something went wrong");
			}
			return;
		}
		// Copy current directory if not marked as deleted.
		if (newdrive->TestDir(ldp->curdir)) {
			safe_strcpy(newdrive->curdir, ldp->curdir);
		}
		Drives.at(drive_index(drive)) = nullptr;
	} else {
		// Standard directory mount
		newdrive = std::make_shared<localDrive>(
		        temp_line.c_str(),
		        sizes[0],
		        int8_tize,
		        sizes[2],
		        sizes[3],
		        mediaid,
		        readonly,
		        section->GetBool("allow_write_protected_files"));
	}

	DriveManager::RegisterFilesystemImage(drive_index(drive), newdrive);
	Drives.at(drive_index(drive)) = newdrive;

	// Set the correct media byte in the table
	mem_writeb(RealToPhysical(dos.tables.mediaid) + (drive_index(drive)) * 9,
	           newdrive->GetMediaByte());

	if (type != "overlay") {
		WriteOut(MSG_Get("PROGRAM_MOUNT_STATUS_2"),
		         newdrive->GetInfoString().c_str(),
		         drive);
		if (readonly) {
			WriteOut(MSG_Get("PROGRAM_MOUNT_READONLY"));
		}
	} else {
		WriteOut(MSG_Get("PROGRAM_MOUNT_OVERLAY_STATUS"),
		         temp_line.c_str(),
		         drive);
	}
	// check if volume label is given and don't allow it to updated in the
	// future
	if (cmd->FindString("-label", label, true)) {
		newdrive->dirCache.SetLabel(label.c_str(), false, false);
	}

	// For hard drives set the label to DRIVELETTER_Drive.
	// For floppy drives set the label to DRIVELETTER_Floppy.
	// This way every drive except cdroms should get a label.
	else if (type == "dir" || type == "overlay") {
		label = drive;
		label += "_DRIVE";
		newdrive->dirCache.SetLabel(label.c_str(), false, false);
	}
	// Floppy labels handled in IMGMOUNT logic usually, but if mounted as dir:
	else if (type == "floppy") {
		label = drive;
		label += "_FLOPPY";
		newdrive->dirCache.SetLabel(label.c_str(), false, true);
	}

	// We incrementFDD here only if it was a directory mount pretending to
	// be floppy Image mounts return early.
	if (type == "floppy") {
		incrementFDD();
	}
	return;
}

void MOUNT::AddMessages() {
	AddCommonMountMessages();
	if (MSG_Exists("PROGRAM_MOUNT_HELP")) {
		return;
	}
	MSG_Add("PROGRAM_MOUNT_HELP",
	        "Map physical folders, drives, or images to a virtual drive letter.\n");

	MSG_Add("PROGRAM_MOUNT_HELP_LONG",
	        "Mount a directory or an image file from the host OS to a drive letter.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=light-green]mount[reset] [color=white]DRIVE[reset] [color=light-cyan]PATH[reset] [-t TYPE] [-fs FS] [-ide] [-chs C,H,S] [-pr] [-ro]\n"
	        "  [color=light-green]mount[reset] -u [color=white]DRIVE[reset]  (unmounts the DRIVE)\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=white]DRIVE[reset]      drive letter (A-Z) or number (0-3) for booting\n"
	        "  [color=light-cyan]PATH[reset]       directory OR image file(s) on the host OS\n"
	        "             (Wildcards supported for images, e.g. *.iso)\n"
	        "  TYPE       type of mount: dir, overlay, floppy, hdd, iso (or cdrom)\n"
	        "  FS         filesystem: fat, iso, or none (for bootable images)\n"
	        "  -ide       attach as IDE device (for CD-ROM/HDD images)\n"
	        "  -chs       specify geometry (Cylinders,Heads,Sectors) for HDD images\n"
	        "  -size      specify geometry (BytesPerSector,SectorsPerHead,Heads,Cylinders)\n"
	        "             Alternative to -chs for HDD images\n"
	        "  -ro        mount as read-only\n"
	        "  -pr        path is relative to the configuration file location\n"
	        "  -t overlay mounts the directory as an overlay on top of the existing drive.\n"
	        "             All writes are redirected to this directory, leaving the original\n"
	        "             files untouched.\n"
	        "\n"
	        "Notes:\n"
	        "  - You can use wildcards to mount multiple images, e.g.:\n"
	        "      [color=light-green]mount[reset] [color=white]A[reset] [color=light-cyan]floppy*.img[reset] -t floppy\n"
	        "  - [color=yellow]%s+F4[reset] swaps & mounts the next disk image, if provided via wildcards.\n"
	        "  - The -ide flag emulates an IDE controller with attached IDE CD drive, useful\n"
	        "    for CD-based games that need a real DOS environment via bootable HDD image.\n"
	        "  - Type [color=light-cyan]overlay[reset] requires [color=white]DRIVE[reset] to be already mounted. It mounts\n"
	        "    [color=light-cyan]PATH[reset] as a write-layer over the drive. Modified files are stored\n"
	        "    in [color=light-cyan]PATH[reset], leaving the original drive data unchanged.\n"
	        "\n"
	        "Examples:\n");
	MSG_Add("PROGRAM_MOUNT_HELP_LONG_WIN32",
	        "  [color=light-green]mount[reset] [color=white]C[reset] [color=light-cyan]C:\\dosgames[reset]\n"
	        "  [color=light-green]mount[reset] [color=white]C[reset] [color=light-cyan]C:\\dosgamesoverlay[reset] -t overlay\n"
	        "  [color=light-green]mount[reset] [color=white]D[reset] [color=light-cyan]D:\\Games\\doom.iso[reset] -t iso\n"
	        "  [color=light-green]mount[reset] [color=white]2[reset] [color=light-cyan]Win95.img[reset] -t hdd -fs none -chs 304,64,63\n"
	        "  [color=light-green]mount[reset] [color=white]0[reset] [color=light-cyan]floppy.img[reset] -t floppy -fs none\n"
	        "  [color=light-green]mount[reset] [color=white]A[reset] [color=light-cyan]floppy*.img[reset] -t floppy\n");
	MSG_Add("PROGRAM_MOUNT_HELP_LONG_MACOSX",
	        "  [color=light-green]mount[reset] [color=white]C[reset] [color=light-cyan]~/dosgames[reset]\n"
	        "  [color=light-green]mount[reset] [color=white]C[reset] [color=light-cyan]~/dosgamesoverlay[reset] -t overlay\n"
	        "  [color=light-green]mount[reset] [color=white]D[reset] [color=light-cyan]~/Games/doom.iso[reset] -t iso\n"
	        "  [color=light-green]mount[reset] [color=white]2[reset] [color=light-cyan]Win95.img[reset] -t hdd -fs none -chs 304,64,63\n"
	        "  [color=light-green]mount[reset] [color=white]0[reset] [color=light-cyan]floppy.img[reset] -t floppy -fs none\n"
	        "  [color=light-green]mount[reset] [color=white]A[reset] [color=light-cyan]floppy*.img[reset] -t floppy -ro\n");
	MSG_Add("PROGRAM_MOUNT_HELP_LONG_OTHER",
	        "  [color=light-green]mount[reset] [color=white]C[reset] [color=light-cyan]~/dosgames[reset]\n"
	        "  [color=light-green]mount[reset] [color=white]C[reset] [color=light-cyan]~/dosgamesoverlay[reset] -t overlay\n"
	        "  [color=light-green]mount[reset] [color=white]D[reset] [color=light-cyan]~/Games/doom.iso[reset] -t iso\n"
	        "  [color=light-green]mount[reset] [color=white]2[reset] [color=light-cyan]Win95.img[reset] -t hdd -fs none -chs 304,64,63\n"
	        "  [color=light-green]mount[reset] [color=white]0[reset] [color=light-cyan]floppy.img[reset] -t floppy -fs none\n"
	        "  [color=light-green]mount[reset] [color=white]A[reset] [color=light-cyan]floppy*.img[reset] -t floppy -ro\n");

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

	// IMGMOUNT merged messages
	MSG_Add("PROGRAM_IMGMOUNT_SPECIFY2",
	        "Must specify drive number (0 or 3) to mount image at (0,1=fda,fdb; 2,3=hda,hdb).\n");
	MSG_Add("PROGRAM_IMGMOUNT_SPECIFY_GEOMETRY",
	        "For hard drive images, drive geometry must be specified:\n"
	        "  [color=light-green]imgmount[reset] [color=white]DRIVE[reset] [color=light-cyan]IMAGEFILE[reset] -chs Cylinders,Heads,Sectors\n"
	        "Alternatively:\n"
	        "  [color=light-green]imgmount[reset] [color=white]DRIVE[reset] [color=light-cyan]IMAGEFILE[reset] -size BytesPerSector,SectorsPerHead,Heads,Cylinders\n"
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
	        "Use parameter -chs cylinders,heads,sectors to specify the geometry.\n"
	        "Alternatively: -size bps,spc,hpc,cyl\n");
	MSG_Add("PROGRAM_IMGMOUNT_FILE_NOT_FOUND", "Image file not found.\n");
	MSG_Add("PROGRAM_IMGMOUNT_ALREADY_MOUNTED",
	        "Drive already mounted at that letter.\n");
	MSG_Add("PROGRAM_IMGMOUNT_CANT_CREATE", "Can't create drive from file.\n");
	MSG_Add("PROGRAM_IMGMOUNT_MOUNT_NUMBER", "Drive number %d mounted as %s.\n");
}