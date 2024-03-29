/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2021-2024  The DOSBox Staging Team
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
#include "program_mount.h"

#include "dosbox.h"

#include "../ints/int10.h"
#include "bios_disk.h"
#include "cdrom.h"
#include "control.h"
#include "drives.h"
#include "fs_utils.h"
#include "program_more_output.h"
#include "shell.h"
#include "string_utils.h"


void MOUNT::ListMounts()
{
	const std::string header_drive = MSG_Get("PROGRAM_MOUNT_STATUS_DRIVE");
	const std::string header_type = MSG_Get("PROGRAM_MOUNT_STATUS_TYPE");
	const std::string header_label = MSG_Get("PROGRAM_MOUNT_STATUS_LABEL");

	const int term_width = real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS);
	const auto width_drive = static_cast<int>(header_drive.length());
	const auto width_label = std::max(minimum_column_length,
	                                  static_cast<int>(header_label.size()));
	const auto width_type = term_width - 3 - width_drive - width_label;
	if (width_type < 0) {
		LOG_WARNING("Message is too long.");
		return;
	}

	auto print_row = [&](const std::string &txt_drive, const std::string &txt_type,
	                     const std::string &txt_label) {
		WriteOut("%-*s %-*s %-*s\n", width_drive, txt_drive.c_str(),
		         width_type, txt_type.c_str(), width_label,
		         txt_label.c_str());
	};

	WriteOut(MSG_Get("PROGRAM_MOUNT_STATUS_1"));
	print_row(header_drive, header_type, header_label);
	const std::string horizontal_divider(term_width, '-');
	WriteOut_NoParsing(horizontal_divider.c_str());

	for (uint8_t d = 0; d < DOS_DRIVES; d++) {
		if (Drives[d]) {
			print_row(std::string{drive_letter(d)},
			          Drives[d]->GetInfoString().c_str(),
			          To_Label(Drives[d]->GetLabel()));
		}
	}
}

void MOUNT::ShowUsage()
{
	MoreOutputStrings output(*this);
	output.AddString(MSG_Get("PROGRAM_MOUNT_HELP_LONG"));
#ifdef WIN32
	output.AddString(MSG_Get("PROGRAM_MOUNT_HELP_LONG_WIN32"));
#elif MACOSX
	output.AddString(MSG_Get("PROGRAM_MOUNT_HELP_LONG_MACOSX"));
#else
	output.AddString(MSG_Get("PROGRAM_MOUNT_HELP_LONG_OTHER"));
#endif
	output.Display();
}

void MOUNT::Run(void) {
	std::unique_ptr<DOS_Drive> newdrive = {};
	DOS_Drive* drive_pointer            = nullptr;
	char drive                          = '\0';
	std::string label                   = {};
	std::string umount                  = {};

	//Hack To allow long commandlines
	ChangeToLongCmd();
	/* Parse the command line */
	/* if the command line is empty show current mounts */
	if (!cmd->GetCount()) {
		ListMounts();
		return;
	}
	// Print help if requested. Previously, help was shown as 
	// a side effect of not being able to parse the correct 
	// command line options.
	if (HelpRequested()) {
		ShowUsage();
		return;
	}

	/* In secure mode don't allow people to change mount points.
		* Neither mount nor unmount */
	if (control->SecureMode()) {
		WriteOut(MSG_Get("PROGRAM_CONFIG_SECURE_DISALLOW"));
		return;
	}
	bool path_relative_to_last_config = false;
	if (cmd->FindExist("-pr",true)) path_relative_to_last_config = true;

	/* Check for unmounting */
	if (cmd->FindString("-u",umount,false)) {
		WriteOut(UnmountHelper(umount[0]), toupper(umount[0]));
		return;
	}

	const Section_prop* section = static_cast<Section_prop*>(
	        control->GetSection("dosbox"));
	assert(section);

	std::string type="dir";
	cmd->FindString("-t",type,true);
	bool iscdrom = (type =="cdrom"); //Used for mscdex bug cdrom label name emulation
	if (type=="floppy" || type=="dir" || type=="cdrom" || type =="overlay") {
		uint16_t sizes[4] ={0};
		uint8_t mediaid;
		std::string str_size = "";
		if (type=="floppy") {
			str_size="512,1,2880,2880";/* All space free */
			mediaid=0xF0;		/* Floppy 1.44 media */
		} else if (type=="dir" || type == "overlay") {
			// 512*32*32765==~500MB total size
			// 512*32*16000==~250MB total free size
			str_size="512,32,32765,16000";
			mediaid=0xF8;		/* Hard Disk */
		} else if (type=="cdrom") {
			str_size="2048,1,65535,0";
			mediaid=0xF8;		/* Hard Disk */
		} else {
			WriteOut(MSG_Get("PROGAM_MOUNT_ILL_TYPE"),type.c_str());
			return;
		}
		/* Parse the free space in mb's (kb's for floppies) */
		std::string mb_size;
		if (cmd->FindString("-freesize",mb_size,true)) {
			char teststr[1024];
			uint16_t freesize = static_cast<uint16_t>(atoi(mb_size.c_str()));
			if (type=="floppy") {
				// freesize in kb
				sprintf(teststr,"512,1,2880,%d",freesize*1024/(512*1));
			} else {
				uint32_t total_size_cyl=32765;
				uint32_t free_size_cyl=(uint32_t)freesize*1024*1024/(512*32);
				if (free_size_cyl>65534) free_size_cyl=65534;
				if (total_size_cyl<free_size_cyl) total_size_cyl=free_size_cyl+10;
				if (total_size_cyl>65534) total_size_cyl=65534;
				sprintf(teststr,"512,32,%d,%d",total_size_cyl,free_size_cyl);
			}
			str_size=teststr;
		}

		cmd->FindString("-size",str_size,true);
		char number[21] = { 0 };const char * scan = str_size.c_str();
		Bitu index = 0;Bitu count = 0;
		/* Parse the str_size string */
		while (*scan && index < 20 && count < 4) {
			if (*scan==',') {
				number[index] = 0;
				sizes[count++] = atoi(number);
				index = 0;
			} else number[index++] = *scan;
			scan++;
		}
		if (count < 4) {
			number[index] = 0; //always goes correct as index is max 20 at this point.
			sizes[count] = atoi(number);
		}

		// get the drive letter
		cmd->FindCommand(1,temp_line);
		if ((temp_line.size() > 2) ||
		    ((temp_line.size() > 1) && (temp_line[1] != ':'))) {
			ShowUsage();
			return;
		}
		const int i_drive = toupper(temp_line[0]);

		if (i_drive < 'A' || i_drive > 'Z') {
			ShowUsage();
			return;
		}
		drive = int_to_char(i_drive);
		if (type == "overlay") {
			//Ensure that the base drive exists:
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

		if (!cmd->FindCommand(2,temp_line)) {
			ShowUsage();
			return;
		}
		if (!temp_line.size()) {
			ShowUsage();
			return;
		}

		if (path_relative_to_last_config && control->configfiles.size() &&
		    !std_fs::path(temp_line).is_absolute()) {
			std::string lastconfigdir =
			        control->configfiles[control->configfiles.size() - 1];

			std::string::size_type pos = lastconfigdir.rfind(
			        CROSS_FILESPLIT);

			if (pos == std::string::npos) {
				pos = 0; // No directory then erase string
			}
			lastconfigdir.erase(pos);
			if (lastconfigdir.length()) {
				temp_line = lastconfigdir + CROSS_FILESPLIT + temp_line;
			}
		}

#if defined(WIN32)
		/* Removing trailing backslash if not root dir so stat
			* will succeed */
		if (temp_line.size() > 3 && temp_line.back() == '\\')
			temp_line.pop_back();
#endif

		const std::string real_path = to_native_path(temp_line);
		if (real_path.empty()) {
			LOG_MSG("MOUNT: Path '%s' not found", temp_line.c_str());
		} else {
			const auto home_resolve = resolve_home(temp_line).string();
			if (home_resolve == real_path) {
				LOG_MSG("MOUNT: Path '%s' found",
						temp_line.c_str());
			} else {
				LOG_MSG("MOUNT: Path '%s' found, while looking for '%s'",
						real_path.c_str(),
						temp_line.c_str());
			}
			temp_line = real_path;
		}

		struct stat test;
		if (stat(temp_line.c_str(),&test)) {
			WriteOut(MSG_Get("PROGRAM_MOUNT_ERROR_1"),temp_line.c_str());
			return;
		}
		/* Not a switch so a normal directory/file */
		if (!S_ISDIR(test.st_mode)) {
			WriteOut(MSG_Get("PROGRAM_MOUNT_ERROR_2"),temp_line.c_str());
			return;
		}

		if (temp_line[temp_line.size() - 1] != CROSS_FILESPLIT) temp_line += CROSS_FILESPLIT;
		uint8_t int8_tize = (uint8_t)sizes[1];

		if (type == "cdrom") {
			// Following options were relevant only for physical CD-ROM support:
			for (auto opt : {"-noioctl", "-ioctl", "-ioctl_dx", "-ioctl_mci", "-ioctl_dio"}) {
				if (cmd->FindExist(opt, false))
					WriteOut(MSG_Get("MSCDEX_WARNING_NO_OPTION"), opt);
			}

			int error = 0;
			newdrive  = std::make_unique<cdromDrive>(
			        drive,
			        temp_line.c_str(),
			        sizes[0],
			        int8_tize,
			        sizes[2],
			        0,
			        mediaid,
			        error);
			// Check Mscdex, if it worked out...
			switch (error) {
				case 0  :	WriteOut(MSG_Get("MSCDEX_SUCCESS"));				break;
				case 1  :	WriteOut(MSG_Get("MSCDEX_ERROR_MULTIPLE_CDROMS"));	break;
				case 2  :	WriteOut(MSG_Get("MSCDEX_ERROR_NOT_SUPPORTED"));	break;
				case 3  :	WriteOut(MSG_Get("MSCDEX_ERROR_PATH"));				break;
				case 4  :	WriteOut(MSG_Get("MSCDEX_TOO_MANY_DRIVES"));		break;
				case 5  :	WriteOut(MSG_Get("MSCDEX_LIMITED_SUPPORT"));		break;
				default :	WriteOut(MSG_Get("MSCDEX_UNKNOWN_ERROR"));			break;
			};
			if (error && error != 5) {
				return;
			}
		} else {
			/* Give a warning when mount c:\ or the / */
#if defined (WIN32)
			if ( (temp_line == "c:\\") || (temp_line == "C:\\") ||
				(temp_line == "c:/") || (temp_line == "C:/")    )
				WriteOut(MSG_Get("PROGRAM_MOUNT_WARNING_WIN"));
#else
			if (temp_line == "/") WriteOut(MSG_Get("PROGRAM_MOUNT_WARNING_OTHER"));
#endif
			if (type == "overlay") {
				const auto ldp = dynamic_cast<localDrive*>(Drives[drive_index(drive)]);
				const auto cdp = dynamic_cast<cdromDrive*>(Drives[drive_index(drive)]);
				if (!ldp || cdp) {
					WriteOut(MSG_Get("PROGRAM_MOUNT_OVERLAY_INCOMPAT_BASE"));
					return;
				}
				std::string base = ldp->GetBasedir();
				uint8_t o_error = 0;
				newdrive = std::make_unique<Overlay_Drive>(
				        base.c_str(),
				        temp_line.c_str(),
				        sizes[0],
				        int8_tize,
				        sizes[2],
				        sizes[3],
				        mediaid,
				        o_error);
				// Erase old drive on success
				if (o_error) {
					if (o_error == 1) WriteOut("No mixing of relative and absolute paths. Overlay failed.");
					else if (o_error == 2) WriteOut("overlay directory can not be the same as underlying file system.");
					else {
						WriteOut("Something went wrong");
					}
					return;
				}
				//Copy current directory if not marked as deleted.
				if (newdrive->TestDir(ldp->curdir)) {
					safe_strcpy(newdrive->curdir,
								ldp->curdir);
				}
				Drives.at(drive_index(drive)) = nullptr;
			} else {
				newdrive = std::make_unique<localDrive>(
				        temp_line.c_str(),
				        sizes[0],
				        int8_tize,
				        sizes[2],
				        sizes[3],
				        mediaid,
				        section->Get_bool(
				                "allow_write_protected_files"));
			}
		}
	} else {
		WriteOut(MSG_Get("PROGRAM_MOUNT_ILL_TYPE"),type.c_str());
		return;
	}

	drive_pointer = DriveManager::RegisterFilesystemImage(drive_index(drive),
	                                                      std::move(newdrive));
	Drives.at(drive_index(drive)) = drive_pointer;

	/* Set the correct media byte in the table */
	mem_writeb(RealToPhysical(dos.tables.mediaid) + (drive_index(drive)) * 9,
	           drive_pointer->GetMediaByte());
	if (type != "overlay")
		WriteOut(MSG_Get("PROGRAM_MOUNT_STATUS_2"),
		         drive_pointer->GetInfoString().c_str(),
		         drive);
	else
		WriteOut(MSG_Get("PROGRAM_MOUNT_OVERLAY_STATUS"),
		         temp_line.c_str(),
		         drive);
	/* check if volume label is given and don't allow it to updated in the future */
	if (cmd->FindString("-label", label, true)) {
		drive_pointer->dirCache.SetLabel(label.c_str(), iscdrom, false);
	}
	/* For hard drives set the label to DRIVELETTER_Drive.
	 * For floppy drives set the label to DRIVELETTER_Floppy.
	 * This way every drive except cdroms should get a label.*/
	else if (type == "dir" || type == "overlay") {
		label = drive; label += "_DRIVE";
		drive_pointer->dirCache.SetLabel(label.c_str(), iscdrom, false);
	} else if (type == "floppy") {
		label = drive; label += "_FLOPPY";
		drive_pointer->dirCache.SetLabel(label.c_str(), iscdrom, true);
	}
	if (type == "floppy") incrementFDD();
	return;
}

void MOUNT::AddMessages() {
	AddCommonMountMessages();
	if (MSG_Exists("PROGRAM_MOUNT_HELP")) {
		return;
	}
	MSG_Add("PROGRAM_MOUNT_HELP",
	        "Map physical folders or drives to a virtual drive letter.\n");

	MSG_Add("PROGRAM_MOUNT_HELP_LONG",
	        "Mount a directory from the host OS to a drive letter.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=light-green]mount[reset] [color=white]DRIVE[reset] [color=light-cyan]DIRECTORY[reset] [-t TYPE] [-freesize SIZE] [-label LABEL]\n"
	        "  [color=light-green]mount[reset] -u [color=white]DRIVE[reset]  (unmounts the DRIVE's directory)\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=white]DRIVE[reset]      drive letter where the directory will be mounted: A, C, D, ...\n"
	        "  [color=light-cyan]DIRECTORY[reset]  directory on the host OS to mount\n"
	        "  TYPE       type of the directory to mount: dir, floppy, cdrom, or overlay\n"
	        "  SIZE       free space for the virtual drive (KB for floppies, MB otherwise)\n"
	        "  LABEL      drive label name to use\n"
	        "\n"
	        "Notes:\n"
	        "  - '-t overlay' redirects writes for mounted drive to another directory.\n"
	        "  - Additional options are described in the manual (README file, chapter 4).\n"
	        "\n"
	        "Examples:\n");
	MSG_Add("PROGRAM_MOUNT_HELP_LONG_WIN32",
	        "  [color=light-green]mount[reset] [color=white]C[reset] [color=light-cyan]C:\\dosgames[reset]\n"
	        "  [color=light-green]mount[reset] [color=white]D[reset] [color=light-cyan]D:\\ [reset]-t cdrom\n"
	        "  [color=light-green]mount[reset] [color=white]C[reset] [color=light-cyan]my_savegame_files[reset] -t overlay\n");
	MSG_Add("PROGRAM_MOUNT_HELP_LONG_MACOSX",
	        "  [color=light-green]mount[reset] [color=white]C[reset] [color=light-cyan]~/dosgames[reset]\n"
	        "  [color=light-green]mount[reset] [color=white]D[reset] [color=light-cyan]\"/Volumes/Game CD\"[reset] -t cdrom\n"
	        "  [color=light-green]mount[reset] [color=white]C[reset] [color=light-cyan]my_savegame_files[reset] -t overlay\n");
	MSG_Add("PROGRAM_MOUNT_HELP_LONG_OTHER",
	        "  [color=light-green]mount[reset] [color=white]C[reset] [color=light-cyan]~/dosgames[reset]\n"
	        "  [color=light-green]mount[reset] [color=white]D[reset] [color=light-cyan]\"/media/USERNAME/Game CD\"[reset] -t cdrom\n"
	        "  [color=light-green]mount[reset] [color=white]C[reset] [color=light-cyan]my_savegame_files[reset] -t overlay\n");

	MSG_Add("PROGRAM_MOUNT_CDROMS_FOUND","CD-ROMs found: %d\n");
	MSG_Add("PROGRAM_MOUNT_ERROR_1","Directory %s doesn't exist.\n");
	MSG_Add("PROGRAM_MOUNT_ERROR_2","%s isn't a directory.\n");
	MSG_Add("PROGRAM_MOUNT_ILL_TYPE","Illegal type %s\n");
	MSG_Add("PROGRAM_MOUNT_ALREADY_MOUNTED","Drive %c already mounted with %s\n");
	MSG_Add("PROGRAM_MOUNT_UMOUNT_NOT_MOUNTED","Drive %c isn't mounted.\n");
	MSG_Add("PROGRAM_MOUNT_UMOUNT_SUCCESS","Drive %c has successfully been removed.\n");
	MSG_Add("PROGRAM_MOUNT_UMOUNT_NO_VIRTUAL","Virtual Drives can not be unMOUNTed.\n");
	MSG_Add("PROGRAM_MOUNT_DRIVEID_ERROR", "'%c' is not a valid drive identifier.\n");
	MSG_Add("PROGRAM_MOUNT_WARNING_WIN","[color=light-red]Mounting c:\\ is NOT recommended. Please mount a (sub)directory next time.[reset]\n");
	MSG_Add("PROGRAM_MOUNT_WARNING_OTHER","[color=light-red]Mounting / is NOT recommended. Please mount a (sub)directory next time.[reset]\n");
	MSG_Add("PROGRAM_MOUNT_NO_OPTION", "Warning: Ignoring unsupported option '%s'.\n");
	MSG_Add("PROGRAM_MOUNT_OVERLAY_NO_BASE",
	        "A normal directory needs to be MOUNTed first before an overlay can be added on\n"
	        "top.\n");
	MSG_Add("PROGRAM_MOUNT_OVERLAY_INCOMPAT_BASE","The overlay is NOT compatible with the drive that is specified.\n");
	MSG_Add("PROGRAM_MOUNT_OVERLAY_MIXED_BASE",
	        "The overlay needs to be specified using the same addressing as the underlying\n"
	        "drive. No mixing of relative and absolute paths.\n");
	MSG_Add("PROGRAM_MOUNT_OVERLAY_SAME_AS_BASE","The overlay directory can not be the same as underlying drive.\n");
	MSG_Add("PROGRAM_MOUNT_OVERLAY_GENERIC_ERROR","Something went wrong.\n");
	MSG_Add("PROGRAM_MOUNT_OVERLAY_STATUS","Overlay %s on drive %c mounted.\n");
}
