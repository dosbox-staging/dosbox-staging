/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
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

#include "bios_disk.h"
#include "control.h"
#include "drives.h"
#include "fs_utils.h"
#include "shell.h"
#include "cdrom.h"
#include "string_utils.h"
#include "../ints/int10.h"

void MOUNT::Move_Z(char new_z)
{
	const char new_drive_z = toupper(new_z);

	if (new_drive_z < 'A' || new_drive_z > 'Z') {
		WriteOut(MSG_Get("PROGRAM_MOUNT_DRIVEID_ERROR"), new_drive_z);
		return;
	}

	const uint8_t new_idx = drive_index(new_drive_z);

	if (Drives[new_idx]) {
		WriteOut(MSG_Get("PROGRAM_MOUNT_MOVE_Z_ERROR_1"), new_drive_z);
		return;
	}

	if (new_idx < DOS_DRIVES - 1) {
		ZDRIVE_NUM = new_idx;
		/* remap drives */
		Drives[new_idx] = Drives[25];
		Drives[25] = 0;
		if (!first_shell) return; //Should not be possible
		/* Update environment */
		std::string line = "";
		std::string tempenv = {new_drive_z, ':', '\\'};
		if (first_shell->GetEnvStr("PATH",line)) {
			std::string::size_type idx = line.find('=');
			std::string value = line.substr(idx +1 , std::string::npos);
			while ( (idx = value.find("Z:\\")) != std::string::npos ||
				(idx = value.find("z:\\")) != std::string::npos  )
				value.replace(idx,3,tempenv);
			line = std::move(value);
		}
		if (!line.size()) line = tempenv;
		first_shell->SetEnv("PATH",line.c_str());
		tempenv += "COMMAND.COM";
		first_shell->SetEnv("COMSPEC",tempenv.c_str());

		/* Update batch file if running from Z: (very likely: autoexec) */
		if (first_shell->bf) {
			std::string &name = first_shell->bf->filename;
			if (starts_with("Z:", name))
				name[0] = new_drive_z;
		}
		/* Change the active drive */
		if (DOS_GetDefaultDrive() == 25)
			DOS_SetDrive(new_idx);
	}
}

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

void MOUNT::Run(void) {
	DOS_Drive * newdrive;char drive;
	std::string label;
	std::string umount;
	std::string newz;

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
		WriteOut(MSG_Get("SHELL_CMD_MOUNT_HELP_LONG"));
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

	/* Check for moving Z: */
	/* Only allowing moving it once. It is merely a convenience added for the wine team */
	if (ZDRIVE_NUM == 25 && cmd->FindString("-z", newz,false)) {
		Move_Z(newz[0]);
		return;
	}

	if (cmd->FindExist("-cd", false) || cmd->FindExist("-listcd", false)) {
		int num = SDL_CDNumDrives();
		WriteOut(MSG_Get("PROGRAM_MOUNT_CDROMS_FOUND"), num);
		for (int i = 0; i < num; i++)
			WriteOut("%2d. %s\n", i, SDL_CDName(i));
		return;
	}

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
		if ((temp_line.size() > 2) || ((temp_line.size() > 1) && (temp_line[1]!=':'))) goto showusage;
		const int i_drive = toupper(temp_line[0]);

		if (i_drive < 'A' || i_drive > 'Z') {
			goto showusage;
		}
		drive = int_to_char(i_drive);
		if (type == "overlay") {
			//Ensure that the base drive exists:
			if (!Drives[drive_index(drive)]) {
				WriteOut(MSG_Get("PROGRAM_MOUNT_OVERLAY_NO_BASE"));
				return;
			}
		} else if (Drives[drive_index(drive)]) {
			WriteOut(MSG_Get("PROGRAM_MOUNT_ALREADY_MOUNTED"),
			         drive,
			         Drives[drive_index(drive)]->GetInfoString().c_str());
			return;
		}

		if (!cmd->FindCommand(2,temp_line)) {
			goto showusage;
		}
		if (!temp_line.size()) {
			goto showusage;
		}
		if (path_relative_to_last_config && control->configfiles.size() && !Cross::IsPathAbsolute(temp_line)) {
			std::string lastconfigdir(control->configfiles[control->configfiles.size() - 1]);
			std::string::size_type pos = lastconfigdir.rfind(CROSS_FILESPLIT);
			if (pos == std::string::npos) {
				pos = 0; //No directory then erase string
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
			std::string home_resolve = temp_line;
			Cross::ResolveHomedir(home_resolve);
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
			int num = -1;
			cmd->FindInt("-usecd", num, true);
			MSCDEX_SetCDInterface(CDROM_USE_SDL, num);

			int error = 0;
			newdrive  = new cdromDrive(drive,temp_line.c_str(),sizes[0],int8_tize,sizes[2],0,mediaid,error);
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
			if (error && error!=5) {
				delete newdrive;
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
				localDrive *ldp = dynamic_cast<localDrive *>(
						Drives[drive_index(drive)]);
				cdromDrive *cdp = dynamic_cast<cdromDrive *>(
						Drives[drive_index(drive)]);
				if (!ldp || cdp) {
					WriteOut(MSG_Get("PROGRAM_MOUNT_OVERLAY_INCOMPAT_BASE"));
					return;
				}
				std::string base = ldp->GetBasedir();
				uint8_t o_error = 0;
				newdrive = new Overlay_Drive(base.c_str(),temp_line.c_str(),sizes[0],int8_tize,sizes[2],sizes[3],mediaid,o_error);
				//Erase old drive on success
				if (o_error) {
					if (o_error == 1) WriteOut("No mixing of relative and absolute paths. Overlay failed.");
					else if (o_error == 2) WriteOut("overlay directory can not be the same as underlying file system.");
					else WriteOut("Something went wrong");
					delete newdrive;
					return;
				}
				//Copy current directory if not marked as deleted.
				if (newdrive->TestDir(ldp->curdir)) {
					safe_strcpy(newdrive->curdir,
								ldp->curdir);
				}

				delete Drives[drive_index(drive)];
				Drives[drive_index(drive)] = nullptr;
			} else {
				newdrive = new localDrive(temp_line.c_str(),sizes[0],int8_tize,sizes[2],sizes[3],mediaid);
			}
		}
	} else {
		WriteOut(MSG_Get("PROGRAM_MOUNT_ILL_TYPE"),type.c_str());
		return;
	}
	Drives[drive_index(drive)] = newdrive;
	/* Set the correct media byte in the table */
	mem_writeb(Real2Phys(dos.tables.mediaid) + (drive_index(drive)) * 9,
				newdrive->GetMediaByte());
	if (type != "overlay")
		WriteOut(MSG_Get("PROGRAM_MOUNT_STATUS_2"),
		         drive,
		         newdrive->GetInfoString().c_str());
	else
		WriteOut(MSG_Get("PROGRAM_MOUNT_OVERLAY_STATUS"),
		         temp_line.c_str(),
		         drive);
	/* check if volume label is given and don't allow it to updated in the future */
	if (cmd->FindString("-label",label,true)) newdrive->dirCache.SetLabel(label.c_str(),iscdrom,false);
	/* For hard drives set the label to DRIVELETTER_Drive.
		* For floppy drives set the label to DRIVELETTER_Floppy.
		* This way every drive except cdroms should get a label.*/
	else if (type == "dir" || type == "overlay") {
		label = drive; label += "_DRIVE";
		newdrive->dirCache.SetLabel(label.c_str(),iscdrom,false);
	} else if (type == "floppy") {
		label = drive; label += "_FLOPPY";
		newdrive->dirCache.SetLabel(label.c_str(),iscdrom,true);
	}
	if (type == "floppy") incrementFDD();
	return;
showusage:
	WriteOut(MSG_Get("SHELL_CMD_MOUNT_HELP_LONG"));
	return;
}

void MOUNT::AddMessages() {
	AddCommonMountMessages();
	MSG_Add("SHELL_CMD_MOUNT_HELP",
	        "maps physical folders or drives to a virtual drive letter.\n");

	MSG_Add("SHELL_CMD_MOUNT_HELP_LONG",
	        "Mount a directory from the host OS to a drive letter.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=green]mount[reset] [color=white]DRIVE[reset] [color=cyan]DIRECTORY[reset] [-t TYPE] [-usecd #] [-freesize SIZE] [-label LABEL]\n"
	        "  [color=green]mount[reset] -listcd / -cd (lists all detected CD-ROM drives and their numbers)\n"
	        "  [color=green]mount[reset] -u [color=white]DRIVE[reset]  (unmounts the DRIVE's directory)\n"
	        "\n"
	        "Where:\n"
	        "  [color=white]DRIVE[reset]     the drive letter where the directory will be mounted: A, C, D, ...\n"
	        "  [color=cyan]DIRECTORY[reset] is the directory on the host OS to be mounted\n"
	        "  TYPE      type of the directory to mount: dir, floppy, cdrom, or overlay\n"
	        "  SIZE      free space for the virtual drive (KiB for floppies, MiB otherwise)\n"
	        "  LABEL     drive label name to be used\n"
	        "\n"
	        "Notes:\n"
	        "  - '-t overlay' redirects writes for mounted drive to another directory.\n"
	        "  - Additional options are described in the manual (README file, chapter 4).\n"
	        "\n"
	        "Examples:\n"
#if defined(WIN32)
	        "  [color=green]mount[reset] [color=white]C[reset] [color=cyan]C:\\dosgames[reset]\n"
	        "  [color=green]mount[reset] [color=white]D[reset] [color=cyan]D:\\[reset] -t cdrom\n"
	        "  [color=green]mount[reset] [color=white]C[reset] [color=cyan]my_savegame_files[reset] -t overlay\n"
#elif defined(MACOSX)
	        "  [color=green]mount[reset] [color=white]C[reset] [color=cyan]~/dosgames[reset]\n"
	        "  [color=green]mount[reset] [color=white]D[reset] [color=cyan]\"/Volumes/Game CD\"[reset] -t cdrom\n"
	        "  [color=green]mount[reset] [color=white]C[reset] [color=cyan]my_savegame_files[reset] -t overlay\n"
#else
	        "  [color=green]mount[reset] [color=white]C[reset] [color=cyan]~/dosgames[reset]\n"
	        "  [color=green]mount[reset] [color=white]D[reset] [color=cyan]\"/media/USERNAME/Game CD\"[reset] -t cdrom\n"
	        "  [color=green]mount[reset] [color=white]C[reset] [color=cyan]my_savegame_files[reset] -t overlay\n"
#endif
	);

	MSG_Add("PROGRAM_MOUNT_CDROMS_FOUND","CDROMs found: %d\n");
	MSG_Add("PROGRAM_MOUNT_ERROR_1","Directory %s doesn't exist.\n");
	MSG_Add("PROGRAM_MOUNT_ERROR_2","%s isn't a directory\n");
	MSG_Add("PROGRAM_MOUNT_ILL_TYPE","Illegal type %s\n");
	MSG_Add("PROGRAM_MOUNT_ALREADY_MOUNTED","Drive %c already mounted with %s\n");
	MSG_Add("PROGRAM_MOUNT_UMOUNT_NOT_MOUNTED","Drive %c isn't mounted.\n");
	MSG_Add("PROGRAM_MOUNT_UMOUNT_SUCCESS","Drive %c has successfully been removed.\n");
	MSG_Add("PROGRAM_MOUNT_UMOUNT_NO_VIRTUAL","Virtual Drives can not be unMOUNTed.\n");
	MSG_Add("PROGRAM_MOUNT_DRIVEID_ERROR", "'%c' is not a valid drive identifier.\n");
	MSG_Add("PROGRAM_MOUNT_WARNING_WIN","\033[31;1mMounting c:\\ is NOT recommended. Please mount a (sub)directory next time.\033[0m\n");
	MSG_Add("PROGRAM_MOUNT_WARNING_OTHER","\033[31;1mMounting / is NOT recommended. Please mount a (sub)directory next time.\033[0m\n");
	MSG_Add("PROGRAM_MOUNT_NO_OPTION", "Warning: Ignoring unsupported option '%s'.\n");
	MSG_Add("PROGRAM_MOUNT_OVERLAY_NO_BASE","A normal directory needs to be MOUNTed first before an overlay can be added on top.\n");
	MSG_Add("PROGRAM_MOUNT_OVERLAY_INCOMPAT_BASE","The overlay is NOT compatible with the drive that is specified.\n");
	MSG_Add("PROGRAM_MOUNT_OVERLAY_MIXED_BASE","The overlay needs to be specified using the same addressing as the underlying drive. No mixing of relative and absolute paths.");
	MSG_Add("PROGRAM_MOUNT_OVERLAY_SAME_AS_BASE","The overlay directory can not be the same as underlying drive.\n");
	MSG_Add("PROGRAM_MOUNT_OVERLAY_GENERIC_ERROR","Something went wrong.\n");
	MSG_Add("PROGRAM_MOUNT_OVERLAY_STATUS","Overlay %s on drive %c mounted.\n");
	MSG_Add("PROGRAM_MOUNT_MOVE_Z_ERROR_1", "Can't move drive Z. Drive %c is mounted already.\n");
}
