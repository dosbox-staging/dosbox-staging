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

#include "programs.h"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "bios_disk.h"
#include "bios.h"
#include "callback.h"
#include "cdrom.h"
#include "control.h"
#include "cross.h"
#include "dma.h"
#include "dos_system.h"
#include "drives.h"
#include "fs_utils.h"
#include "inout.h"
#include "mapper.h"
#include "ide.h"
#include "mem.h"
#include "program_autotype.h"
#include "program_choice.h"
#include "program_help.h"
#include "program_ls.h"
#include "regs.h"
#include "setup.h"
#include "shell.h"
#include "support.h"
#include "string_utils.h"
#include "../ints/int10.h"

#if defined(WIN32)
#ifndef S_ISDIR
#define S_ISDIR(m) (((m)&S_IFMT)==S_IFDIR)
#endif
#endif

#if C_DEBUG
Bitu DEBUG_EnableDebugger(void);
#endif

static Bitu ZDRIVE_NUM = 25;

static const char *UnmountHelper(char umount)
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

class MOUNT final : public Program {
public:
	void Move_Z(char new_z)
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

	void ListMounts()
	{
		const std::string header_drive = MSG_Get("PROGRAM_MOUNT_STATUS_DRIVE");
		const std::string header_type = MSG_Get("PROGRAM_MOUNT_STATUS_TYPE");
		const std::string header_label = MSG_Get("PROGRAM_MOUNT_STATUS_LABEL");

		const int term_width = real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS);
		const auto width_1 = static_cast<int>(header_drive.size());
		const auto width_3 = std::max(11, static_cast<int>(header_label.size()));
		const auto width_2 = term_width - 3 - width_1 - width_3;

		auto print_row = [&](const std::string &txt_1,
		                     const std::string &txt_2,
		                     const std::string &txt_3) {
			WriteOut("%-*s %-*s %-*s\n",
			         width_1, txt_1.c_str(),
			         width_2, txt_2.c_str(),
			         width_3, txt_3.c_str());
		};

		WriteOut(MSG_Get("PROGRAM_MOUNT_STATUS_1"));
		print_row(header_drive, header_type, header_label);
		for (int i = 0; i < term_width; i++)
			WriteOut_NoParsing("-");

		for (uint8_t d = 0; d < DOS_DRIVES; d++) {
			if (Drives[d]) {
				print_row(std::string{drive_letter(d)},
				          Drives[d]->GetInfo(),
				          To_Label(Drives[d]->GetLabel()));
			}
		}
	}

	void Run(void) {
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

		if (cmd->FindExist("-cd", false)) {
			WriteOut(MSG_Get("PROGRAM_MOUNT_NO_OPTION"), "-cd");
			return;
		}

		std::string type="dir";
		cmd->FindString("-t",type,true);
		bool iscdrom = (type =="cdrom"); //Used for mscdex bug cdrom label name emulation
		if (type=="floppy" || type=="dir" || type=="cdrom" || type =="overlay") {
			Bit16u sizes[4] ={0};
			Bit8u mediaid;
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
				Bit16u freesize = static_cast<Bit16u>(atoi(mb_size.c_str()));
				if (type=="floppy") {
					// freesize in kb
					sprintf(teststr,"512,1,2880,%d",freesize*1024/(512*1));
				} else {
					Bit32u total_size_cyl=32765;
					Bit32u free_size_cyl=(Bit32u)freesize*1024*1024/(512*32);
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
				         Drives[drive_index(drive)]->GetInfo());
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
			Bit8u bit8size=(Bit8u) sizes[1];

			if (type == "cdrom") {
				// Following options were relevant only for physical CD-ROM support:
				for (auto opt : {"-usecd", "-noioctl", "-ioctl", "-ioctl_dx", "-ioctl_mci", "-ioctl_dio"}) {
					if (cmd->FindExist(opt, false))
						WriteOut(MSG_Get("MSCDEX_WARNING_NO_OPTION"), opt);
				}

				int error = 0;
				newdrive  = new cdromDrive(drive,temp_line.c_str(),sizes[0],bit8size,sizes[2],0,mediaid,error);
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
					Bit8u o_error = 0;
					newdrive = new Overlay_Drive(base.c_str(),temp_line.c_str(),sizes[0],bit8size,sizes[2],sizes[3],mediaid,o_error);
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
					newdrive = new localDrive(temp_line.c_str(),sizes[0],bit8size,sizes[2],sizes[3],mediaid);
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
			WriteOut(MSG_Get("PROGRAM_MOUNT_STATUS_2"), drive,
			         newdrive->GetInfo());
		else
			WriteOut(MSG_Get("PROGRAM_MOUNT_OVERLAY_STATUS"),
			         temp_line.c_str(), drive);
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
};

static void MOUNT_ProgramStart(Program * * make) {
	*make=new MOUNT;
}

class MEM final : public Program {
public:
	void Run(void) {
		/* Show conventional Memory */
		WriteOut("\n");

		Bit16u umb_start=dos_infoblock.GetStartOfUMBChain();
		Bit8u umb_flag=dos_infoblock.GetUMBChainState();
		Bit8u old_memstrat=DOS_GetMemAllocStrategy()&0xff;
		if (umb_start!=0xffff) {
			if ((umb_flag&1)==1) DOS_LinkUMBsToMemChain(0);
			DOS_SetMemAllocStrategy(0);
		}

		Bit16u seg,blocks;blocks=0xffff;
		DOS_AllocateMemory(&seg,&blocks);
		WriteOut(MSG_Get("PROGRAM_MEM_CONVEN"),blocks*16/1024);

		if (umb_start!=0xffff) {
			DOS_LinkUMBsToMemChain(1);
			DOS_SetMemAllocStrategy(0x40);	// search in UMBs only

			Bit16u largest_block=0,total_blocks=0,block_count=0;
			for (;; block_count++) {
				blocks=0xffff;
				DOS_AllocateMemory(&seg,&blocks);
				if (blocks==0) break;
				total_blocks+=blocks;
				if (blocks>largest_block) largest_block=blocks;
				DOS_AllocateMemory(&seg,&blocks);
			}

			Bit8u current_umb_flag=dos_infoblock.GetUMBChainState();
			if ((current_umb_flag&1)!=(umb_flag&1)) DOS_LinkUMBsToMemChain(umb_flag);
			DOS_SetMemAllocStrategy(old_memstrat);	// restore strategy

			if (block_count>0) WriteOut(MSG_Get("PROGRAM_MEM_UPPER"),total_blocks*16/1024,block_count,largest_block*16/1024);
		}

		/* Test for and show free XMS */
		reg_ax=0x4300;CALLBACK_RunRealInt(0x2f);
		if (reg_al==0x80) {
			reg_ax=0x4310;CALLBACK_RunRealInt(0x2f);
			Bit16u xms_seg=SegValue(es);Bit16u xms_off=reg_bx;
			reg_ah=8;
			CALLBACK_RunRealFar(xms_seg,xms_off);
			if (!reg_bl) {
				WriteOut(MSG_Get("PROGRAM_MEM_EXTEND"),reg_dx);
			}
		}
		/* Test for and show free EMS */
		Bit16u handle;
		char emm[9] = { 'E','M','M','X','X','X','X','0',0 };
		if (DOS_OpenFile(emm,0,&handle)) {
			DOS_CloseFile(handle);
			reg_ah=0x42;
			CALLBACK_RunRealInt(0x67);
			WriteOut(MSG_Get("PROGRAM_MEM_EXPAND"),reg_bx*16);
		}
	}
};


static void MEM_ProgramStart(Program * * make) {
	*make=new MEM;
}

extern Bit32u floppytype;


class BOOT final : public Program {
private:

	FILE *getFSFile_mounted(char const* filename, Bit32u *ksize, Bit32u *bsize, Bit8u *error) {
		//if return NULL then put in error the errormessage code if an error was requested
		bool tryload = (*error)?true:false;
		*error = 0;
		Bit8u drive;
		FILE *tmpfile;
		char fullname[DOS_PATHLENGTH];

		localDrive* ldp=0;
		if (!DOS_MakeName(const_cast<char*>(filename),fullname,&drive)) return NULL;

		try {
			ldp=dynamic_cast<localDrive*>(Drives[drive]);
			if (!ldp) return NULL;

			tmpfile = ldp->GetSystemFilePtr(fullname, "rb");
			if (tmpfile == NULL) {
				if (!tryload) *error=1;
				return NULL;
			}

			// get file size
			fseek(tmpfile,0L, SEEK_END);
			*ksize = (ftell(tmpfile) / 1024);
			*bsize = ftell(tmpfile);
			fclose(tmpfile);

			tmpfile = ldp->GetSystemFilePtr(fullname, "rb+");
			if (tmpfile == NULL) {
//				if (!tryload) *error=2;
//				return NULL;
				WriteOut(MSG_Get("PROGRAM_BOOT_WRITE_PROTECTED"));
				tmpfile = ldp->GetSystemFilePtr(fullname, "rb");
				if (tmpfile == NULL) {
					if (!tryload) *error=1;
					return NULL;
				}
			}

			return tmpfile;
		}
		catch(...) {
			return NULL;
		}
	}

	FILE *getFSFile(char const * filename, Bit32u *ksize, Bit32u *bsize,bool tryload=false) {
		Bit8u error = tryload?1:0;
		FILE* tmpfile = getFSFile_mounted(filename,ksize,bsize,&error);
		if (tmpfile) return tmpfile;
		//File not found on mounted filesystem. Try regular filesystem
		std::string filename_s(filename);
		Cross::ResolveHomedir(filename_s);
		tmpfile = fopen_wrap(filename_s.c_str(),"rb+");
		if (!tmpfile) {
			if ( (tmpfile = fopen_wrap(filename_s.c_str(),"rb")) ) {
				//File exists; So can't be opened in correct mode => error 2
//				fclose(tmpfile);
//				if (tryload) error = 2;
				WriteOut(MSG_Get("PROGRAM_BOOT_WRITE_PROTECTED"));
				fseek(tmpfile,0L, SEEK_END);
				*ksize = (ftell(tmpfile) / 1024);
				*bsize = ftell(tmpfile);
				return tmpfile;
			}
			// Give the delayed errormessages from the mounted variant (or from above)
			if (error == 1) WriteOut(MSG_Get("PROGRAM_BOOT_NOT_EXIST"));
			if (error == 2) WriteOut(MSG_Get("PROGRAM_BOOT_NOT_OPEN"));
			return NULL;
		}
		fseek(tmpfile,0L, SEEK_END);
		*ksize = (ftell(tmpfile) / 1024);
		*bsize = ftell(tmpfile);
		return tmpfile;
	}

	void printError(void) {
		WriteOut(MSG_Get("PROGRAM_BOOT_PRINT_ERROR"));
	}

	void disable_umb_ems_xms(void) {
		Section* dos_sec = control->GetSection("dos");
		dos_sec->ExecuteDestroy(false);
		char test[20];
		safe_strcpy(test, "umb=false");
		dos_sec->HandleInputline(test);
		safe_strcpy(test, "xms=false");
		dos_sec->HandleInputline(test);
		safe_strcpy(test, "ems=false");
		dos_sec->HandleInputline(test);
		dos_sec->ExecuteInit(false);
     }

public:

	void Run(void) {
		//Hack To allow long commandlines
		ChangeToLongCmd();
		/* In secure mode don't allow people to boot stuff.
		 * They might try to corrupt the data on it */
		if (control->SecureMode()) {
			WriteOut(MSG_Get("PROGRAM_CONFIG_SECURE_DISALLOW"));
			return;
		}

		FILE *usefile_1=NULL;
		FILE *usefile_2=NULL;
		Bitu i=0;
		Bit32u floppysize=0;
		Bit32u rombytesize_1=0;
		Bit32u rombytesize_2=0;
		char drive = 'A';
		std::string cart_cmd="";

		if (!cmd->GetCount()) {
			printError();
			return;
		}
		while (i<cmd->GetCount()) {
			if (cmd->FindCommand(i+1, temp_line)) {
				if ((temp_line == "-l") || (temp_line == "-L")) {
					/* Specifying drive... next argument then is the drive */
					i++;
					if (cmd->FindCommand(i+1, temp_line)) {
						drive=toupper(temp_line[0]);
						if ((drive != 'A') && (drive != 'C') && (drive != 'D')) {
							printError();
							return;
						}

					} else {
						printError();
						return;
					}
					i++;
					continue;
				}

				if ((temp_line == "-e") || (temp_line == "-E")) {
					/* Command mode for PCJr cartridges */
					i++;
					if (cmd->FindCommand(i + 1, temp_line)) {
						for (size_t ct = 0;ct < temp_line.size();ct++) temp_line[ct] = toupper(temp_line[ct]);
						cart_cmd = temp_line;
					} else {
						printError();
						return;
					}
					i++;
					continue;
				}

				if ( i >= MAX_SWAPPABLE_DISKS ) {
					return; //TODO give a warning.
				}
				WriteOut(MSG_Get("PROGRAM_BOOT_IMAGE_OPEN"), temp_line.c_str());
				Bit32u rombytesize;
				FILE *usefile = getFSFile(temp_line.c_str(), &floppysize, &rombytesize);
				if (usefile != NULL) {
					diskSwap[i].reset(new imageDisk(usefile, temp_line.c_str(), floppysize, false));
					if (usefile_1==NULL) {
						usefile_1=usefile;
						rombytesize_1=rombytesize;
					} else {
						usefile_2=usefile;
						rombytesize_2=rombytesize;
					}
				} else {
					WriteOut(MSG_Get("PROGRAM_BOOT_IMAGE_NOT_OPEN"), temp_line.c_str());
					return;
				}

			}
			i++;
		}

		swapInDisks(0);

		if (!imageDiskList[drive_index(drive)]) {
			WriteOut(MSG_Get("PROGRAM_BOOT_UNABLE"), drive);
			return;
		}

		bootSector bootarea;
		imageDiskList[drive_index(drive)]->Read_Sector(0, 0, 1,
		                                               reinterpret_cast<uint8_t *>(&bootarea));
		if ((bootarea.rawdata[0] == 0x50) && (bootarea.rawdata[1] == 0x43) &&
		    (bootarea.rawdata[2] == 0x6a) && (bootarea.rawdata[3] == 0x72)) {
			if (machine != MCH_PCJR) {
				WriteOut(MSG_Get("PROGRAM_BOOT_CART_WO_PCJR"));
			}
			else {
				Bit8u rombuf[65536];
				Bits cfound_at=-1;
				if (!cart_cmd.empty()) {
					/* read cartridge data into buffer */
					fseek(usefile_1,0x200L, SEEK_SET);
					if (fread(rombuf, 1, rombytesize_1-0x200, usefile_1) < rombytesize_1 - 0x200) {
						LOG_MSG("Failed to read sufficient cartridge data");
						fclose(usefile_1);
						return;
					}
					char cmdlist[1024];
					cmdlist[0]=0;
					Bitu ct=6;
					Bits clen=rombuf[ct];
					char buf[257];
					if (cart_cmd=="?") {
						while (clen!=0) {
							strncpy(buf,(char*)&rombuf[ct+1],clen);
							buf[clen]=0;
							upcase(buf);
							safe_strcat(cmdlist, " ");
							safe_strcat(cmdlist, buf);
							ct += 1 + clen + 3;
							if (ct>sizeof(cmdlist)) break;
							clen=rombuf[ct];
						}
						if (ct>6) {
							WriteOut(MSG_Get("PROGRAM_BOOT_CART_LIST_CMDS"),cmdlist);
						} else {
							WriteOut(MSG_Get("PROGRAM_BOOT_CART_NO_CMDS"));
						}
						for (auto &disk : diskSwap)
							disk.reset();
						//fclose(usefile_1); //delete diskSwap closes the file
						return;
					} else {
						while (clen!=0) {
							strncpy(buf,(char*)&rombuf[ct+1],clen);
							buf[clen]=0;
							upcase(buf);
							safe_strcat(cmdlist, " ");
							safe_strcat(cmdlist, buf);
							ct += 1 + clen;

							if (cart_cmd==buf) {
								cfound_at=ct;
								break;
							}

							ct+=3;
							if (ct>sizeof(cmdlist)) break;
							clen=rombuf[ct];
						}
						if (cfound_at<=0) {
							if (ct>6) {
								WriteOut(MSG_Get("PROGRAM_BOOT_CART_LIST_CMDS"),cmdlist);
							} else {
								WriteOut(MSG_Get("PROGRAM_BOOT_CART_NO_CMDS"));
							}
							for (auto &disk : diskSwap)
								disk.reset();
							//fclose(usefile_1); //Delete diskSwap closes the file
							return;
						}
					}
				}

				disable_umb_ems_xms();
				MEM_PreparePCJRCartRom();

				if (usefile_1==NULL) return;

				Bit32u sz1,sz2;
				FILE *tfile = getFSFile("system.rom", &sz1, &sz2, true);
				if (tfile!=NULL) {
					fseek(tfile, 0x3000L, SEEK_SET);
					Bit32u drd=(Bit32u) fread(rombuf, 1, 0xb000, tfile);
					if (drd==0xb000) {
						for ( i = 0; i < 0xb000; i++) phys_writeb(0xf3000 + i, rombuf[i]);
					}
					fclose(tfile);
				}

				if (usefile_2!=NULL) {
					fseek(usefile_2, 0x0L, SEEK_SET);
					if (fread(rombuf, 1, 0x200, usefile_2) < 0x200) {
						LOG_MSG("Failed to read sufficient ROM data");
						fclose(usefile_2);
						return;
					}

					PhysPt romseg_pt=host_readw(&rombuf[0x1ce])<<4;

					/* read cartridge data into buffer */
					fseek(usefile_2, 0x200L, SEEK_SET);
					if (fread(rombuf, 1, rombytesize_2-0x200, usefile_2) < rombytesize_2 - 0x200) {
						LOG_MSG("Failed to read sufficient ROM data");
						fclose(usefile_2);
						return;
					}

					//fclose(usefile_2); //usefile_2 is in diskSwap structure which should be deleted to close the file

					/* write cartridge data into ROM */
					for (i = 0; i < rombytesize_2 - 0x200; i++) phys_writeb(romseg_pt + i, rombuf[i]);
				}

				fseek(usefile_1, 0x0L, SEEK_SET);
				if (fread(rombuf, 1, 0x200, usefile_1) < 0x200) {
					LOG_MSG("Failed to read sufficient cartridge data");
					fclose(usefile_1);
					return;
				}

				Bit16u romseg=host_readw(&rombuf[0x1ce]);

				/* read cartridge data into buffer */
				fseek(usefile_1,0x200L, SEEK_SET);
				if (fread(rombuf, 1, rombytesize_1-0x200, usefile_1) < rombytesize_1 - 0x200) {
					LOG_MSG("Failed to read sufficient cartridge data");
					fclose(usefile_1);
					return;
				}
				//fclose(usefile_1); //usefile_1 is in diskSwap structure which should be deleted to close the file

				/* write cartridge data into ROM */
				for (i = 0; i < rombytesize_1 - 0x200; i++) phys_writeb((romseg << 4) + i, rombuf[i]);

				//Close cardridges
				for (auto &disk : diskSwap)
					disk.reset();

				if (cart_cmd.empty()) {
					Bit32u old_int18=mem_readd(0x60);
					/* run cartridge setup */
					SegSet16(ds,romseg);
					SegSet16(es,romseg);
					SegSet16(ss,0x8000);
					reg_esp=0xfffe;
					CALLBACK_RunRealFar(romseg,0x0003);

					Bit32u new_int18=mem_readd(0x60);
					if (old_int18!=new_int18) {
						/* boot cartridge (int18) */
						SegSet16(cs,RealSeg(new_int18));
						reg_ip = RealOff(new_int18);
					}
				} else {
					if (cfound_at>0) {
						/* run cartridge setup */
						SegSet16(ds,dos.psp());
						SegSet16(es,dos.psp());
						CALLBACK_RunRealFar(romseg,cfound_at);
					}
				}
			}
		} else {
			disable_umb_ems_xms();
			MEM_RemoveEMSPageFrame();
			WriteOut(MSG_Get("PROGRAM_BOOT_BOOT"), drive);
			for (i = 0; i < 512; i++) real_writeb(0, 0x7c00 + i, bootarea.rawdata[i]);

			/* create appearance of floppy drive DMA usage (Demon's Forge) */
			if (!IS_TANDY_ARCH && floppysize!=0) GetDMAChannel(2)->tcount=true;

			/* revector some dos-allocated interrupts */
			real_writed(0,0x01*4,0xf000ff53);
			real_writed(0,0x03*4,0xf000ff53);

			SegSet16(cs, 0);
			reg_ip = 0x7c00;
			SegSet16(ds, 0);
			SegSet16(es, 0);
			/* set up stack at a safe place */
			SegSet16(ss, 0x7000);
			reg_esp = 0x100;
			reg_esi = 0;
			reg_ecx = 1;
			reg_ebp = 0;
			reg_eax = 0;
			reg_edx = 0; //Head 0 drive 0
			reg_ebx= 0x7c00; //Real code probably uses bx to load the image
		}
	}
};

static void BOOT_ProgramStart(Program * * make) {
	*make=new BOOT;
}


class LOADROM final : public Program {
public:
	void Run(void) {
		if (!(cmd->FindCommand(1, temp_line))) {
			WriteOut(MSG_Get("PROGRAM_LOADROM_SPECIFY_FILE"));
			return;
		}

		Bit8u drive;
		char fullname[DOS_PATHLENGTH];
		localDrive* ldp=0;
		if (!DOS_MakeName((char *)temp_line.c_str(),fullname,&drive)) return;

		try {
			/* try to read ROM file into buffer */
			ldp=dynamic_cast<localDrive*>(Drives[drive]);
			if (!ldp) return;

			FILE *tmpfile = ldp->GetSystemFilePtr(fullname, "rb");
			if (tmpfile == NULL) {
				WriteOut(MSG_Get("PROGRAM_LOADROM_CANT_OPEN"));
				return;
			}
			fseek(tmpfile, 0L, SEEK_END);
			if (ftell(tmpfile)>0x8000) {
				WriteOut(MSG_Get("PROGRAM_LOADROM_TOO_LARGE"));
				fclose(tmpfile);
				return;
			}
			fseek(tmpfile, 0L, SEEK_SET);
			Bit8u rom_buffer[0x8000];
			Bitu data_read = fread(rom_buffer, 1, 0x8000, tmpfile);
			fclose(tmpfile);

			/* try to identify ROM type */
			PhysPt rom_base = 0;
			if (data_read >= 0x4000 && rom_buffer[0] == 0x55 && rom_buffer[1] == 0xaa &&
				(rom_buffer[3] & 0xfc) == 0xe8 && strncmp((char*)(&rom_buffer[0x1e]), "IBM", 3) == 0) {

				if (!IS_EGAVGA_ARCH) {
					WriteOut(MSG_Get("PROGRAM_LOADROM_INCOMPATIBLE"));
					return;
				}
				rom_base = PhysMake(0xc000, 0); // video BIOS
			}
			else if (data_read == 0x8000 && rom_buffer[0] == 0xe9 && rom_buffer[1] == 0x8f &&
				rom_buffer[2] == 0x7e && strncmp((char*)(&rom_buffer[0x4cd4]), "IBM", 3) == 0) {

				rom_base = PhysMake(0xf600, 0); // BASIC
			}

			if (rom_base) {
				/* write buffer into ROM */
				for (Bitu i=0; i<data_read; i++) phys_writeb(rom_base + i, rom_buffer[i]);

				if (rom_base == 0xc0000) {
					/* initialize video BIOS */
					phys_writeb(PhysMake(0xf000, 0xf065), 0xcf);
					reg_flags &= ~FLAG_IF;
					CALLBACK_RunRealFar(0xc000, 0x0003);
					LOG_MSG("Video BIOS ROM loaded and initialized.");
				}
				else WriteOut(MSG_Get("PROGRAM_LOADROM_BASIC_LOADED"));
			}
			else WriteOut(MSG_Get("PROGRAM_LOADROM_UNRECOGNIZED"));
		}
		catch(...) {
			return;
		}
	}
};

static void LOADROM_ProgramStart(Program * * make) {
	*make=new LOADROM;
}

#if C_DEBUG
class BIOSTEST final : public Program {
public:
	void Run(void) {
		if (!(cmd->FindCommand(1, temp_line))) {
			WriteOut("Must specify BIOS file to load.\n");
			return;
		}

		Bit8u drive;
		char fullname[DOS_PATHLENGTH];
		localDrive* ldp = 0;
		if (!DOS_MakeName((char *)temp_line.c_str(), fullname, &drive)) return;

		try {
			/* try to read ROM file into buffer */
			ldp = dynamic_cast<localDrive*>(Drives[drive]);
			if (!ldp) return;

			FILE *tmpfile = ldp->GetSystemFilePtr(fullname, "rb");
			if (tmpfile == NULL) {
				WriteOut("Can't open a file");
				return;
			}
			fseek(tmpfile, 0L, SEEK_END);
			if (ftell(tmpfile) > 64 * 1024) {
				WriteOut("BIOS File too large");
				fclose(tmpfile);
				return;
			}
			fseek(tmpfile, 0L, SEEK_SET);
			Bit8u buffer[64*1024];
			Bitu data_read = fread(buffer, 1, sizeof( buffer), tmpfile);
			fclose(tmpfile);

			Bit32u rom_base = PhysMake(0xf000, 0); // override regular dosbox bios
			/* write buffer into ROM */
			for (Bitu i = 0; i < data_read; i++) phys_writeb(rom_base + i, buffer[i]);

			//Start executing this bios
			memset(&cpu_regs, 0, sizeof(cpu_regs));
			memset(&Segs, 0, sizeof(Segs));


			SegSet16(cs, 0xf000);
			reg_eip = 0xfff0;
		}
		catch (...) {
			return;
		}
	}
};

static void BIOSTEST_ProgramStart(Program * * make) {
	*make = new BIOSTEST;
}

#endif

// LOADFIX

class LOADFIX final : public Program {
public:
	void Run(void);
};

void LOADFIX::Run(void)
{
	Bit16u commandNr = 1;
	Bit16u kb = 64;
	if (cmd->FindCommand(commandNr, temp_line)) {
		if (temp_line[0] == '-') {
			const auto ch = std::toupper(temp_line[1]);
			if ((ch == 'D') || (ch == 'F')) {
				// Deallocate all
				DOS_FreeProcessMemory(0x40);
				WriteOut(MSG_Get("PROGRAM_LOADFIX_DEALLOCALL"),kb);
				return;
			} else {
				// Set mem amount to allocate
				kb = atoi(temp_line.c_str()+1);
				if (kb==0) kb=64;
				commandNr++;
			}
		}
	}
	// Allocate Memory
	Bit16u segment;
	Bit16u blocks = kb*1024/16;
	if (DOS_AllocateMemory(&segment,&blocks)) {
		DOS_MCB mcb((Bit16u)(segment-1));
		mcb.SetPSPSeg(0x40);			// use fake segment
		WriteOut(MSG_Get("PROGRAM_LOADFIX_ALLOC"),kb);
		// Prepare commandline...
		if (cmd->FindCommand(commandNr++,temp_line)) {
			// get Filename
			char filename[128];
			safe_strcpy(filename, temp_line.c_str());
			// Setup commandline
			char args[256+1];
			args[0] = 0;
			bool found = cmd->FindCommand(commandNr++,temp_line);
			while (found) {
				if (strlen(args)+temp_line.length()+1>256) break;
				safe_strcat(args, temp_line.c_str());
				found = cmd->FindCommand(commandNr++,temp_line);
				if (found)
					safe_strcat(args, " ");
			}
			// Use shell to start program
			DOS_Shell shell;
			shell.Execute(filename,args);
			DOS_FreeMemory(segment);
			WriteOut(MSG_Get("PROGRAM_LOADFIX_DEALLOC"),kb);
		}
	} else {
		WriteOut(MSG_Get("PROGRAM_LOADFIX_ERROR"),kb);
	}
}

static void LOADFIX_ProgramStart(Program * * make) {
	*make=new LOADFIX;
}

// RESCAN

class RESCAN final : public Program {
public:
	void Run(void);
};

void RESCAN::Run(void)
{
	bool all = false;

	Bit8u drive = DOS_GetDefaultDrive();

	if (cmd->FindCommand(1,temp_line)) {
		//-A -All /A /All
		if (temp_line.size() >= 2 &&
		    (temp_line[0] == '-' || temp_line[0] == '/') &&
		    (temp_line[1] == 'a' || temp_line[1] == 'A')) {
			all = true;
		} else if (temp_line.size() == 2 && temp_line[1] == ':') {
			lowcase(temp_line);
			drive  = temp_line[0] - 'a';
		}
	}
	// Get current drive
	if (all) {
		for (Bitu i =0; i<DOS_DRIVES; i++) {
			if (Drives[i]) Drives[i]->EmptyCache();
		}
		WriteOut(MSG_Get("PROGRAM_RESCAN_SUCCESS"));
	} else {
		if (drive < DOS_DRIVES && Drives[drive]) {
			Drives[drive]->EmptyCache();
			WriteOut(MSG_Get("PROGRAM_RESCAN_SUCCESS"));
		}
	}
}

static void RESCAN_ProgramStart(Program * * make) {
	*make=new RESCAN;
}

class INTRO final : public Program {
public:
	void DisplayMount(void) {
		/* Basic mounting has a version for each operating system.
		 * This is done this way so both messages appear in the language file*/
		WriteOut(MSG_Get("PROGRAM_INTRO_MOUNT_START"));
#if (WIN32)
		WriteOut(MSG_Get("PROGRAM_INTRO_MOUNT_WINDOWS"));
#else
		WriteOut(MSG_Get("PROGRAM_INTRO_MOUNT_OTHER"));
#endif
		WriteOut(MSG_Get("PROGRAM_INTRO_MOUNT_END"));
	}

	void Run(void) {
		/* Only run if called from the first shell (Xcom TFTD runs any intro file in the path) */
		if (DOS_PSP(dos.psp()).GetParent() != DOS_PSP(DOS_PSP(dos.psp()).GetParent()).GetParent()) return;
		if (cmd->FindExist("cdrom",false)) {
#ifdef WIN32
			WriteOut(MSG_Get("PROGRAM_INTRO_CDROM_WINDOWS"));
#else
			WriteOut(MSG_Get("PROGRAM_INTRO_CDROM_OTHER"));
#endif
			return;
		}
		if (cmd->FindExist("mount",false)) {
			WriteOut("\033[2J");//Clear screen before printing
			DisplayMount();
			return;
		}
		if (cmd->FindExist("special",false)) {
			WriteOut(MSG_Get("PROGRAM_INTRO_SPECIAL"));
			return;
		}
		/* Default action is to show all pages */
		WriteOut(MSG_Get("PROGRAM_INTRO"));
		Bit8u c;Bit16u n=1;
		DOS_ReadFile (STDIN,&c,&n);
		DisplayMount();
		DOS_ReadFile (STDIN,&c,&n);
#ifdef WIN32
		WriteOut(MSG_Get("PROGRAM_INTRO_CDROM_WINDOWS"));
#else
		WriteOut(MSG_Get("PROGRAM_INTRO_CDROM_OTHER"));
#endif
		DOS_ReadFile(STDIN, &c, &n);
		WriteOut(MSG_Get("PROGRAM_INTRO_SPECIAL"));
	}
};

static void INTRO_ProgramStart(Program * * make) {
	*make=new INTRO;
}

class IMGMOUNT final : public Program {
public:
	void Run(void) {
		//Hack To allow long commandlines
		ChangeToLongCmd();

		// Usage
		if (!cmd->GetCount() || cmd->FindExist("/?", false) ||
		    cmd->FindExist("-h", false) || cmd->FindExist("--help", false)) {
			WriteOut(MSG_Get("SHELL_CMD_IMGMOUNT_HELP_LONG"));
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
		if (cmd->FindString("-u",umount,false)) {
			WriteOut(UnmountHelper(umount[0]), toupper(umount[0]));
			return;
		}


		std::string type   = "hdd";
		std::string fstype = "fat";
		cmd->FindString("-t",type,true);
		cmd->FindString("-fs",fstype,true);

		// Types 'cdrom' and 'iso' are synonyms. Name 'cdrom' is easier
		// to remember and makes more sense, while name 'iso' is
		// required for backwards compatibility and for users conflating
		// -fs and -t parameters.
		if (type == "cdrom")
			type = "iso";

		if (type != "floppy" && type != "hdd" && type != "iso") {
			WriteOut(MSG_Get("PROGRAM_IMGMOUNT_TYPE_UNSUPPORTED"),
			         type.c_str());
			return;
		}

		Bit16u sizes[4] = {0};
		bool imgsizedetect = false;

        signed char ide_index = -1;
        bool ide_slave = false;
        std::string ideattach="auto";
		std::string str_size = "";
		Bit8u mediaid = 0xF8;

        /* DOSBox-X: we allow "-ide" to allow controlling which IDE controller and slot to attach the hard disk/CD-ROM to */
        cmd->FindString("-ide",ideattach,true);

        if (ideattach == "auto") {
            if (type != "floppy") IDE_Auto(ide_index,ide_slave);
        }
        else if (ideattach != "none" && isdigit(ideattach[0]) && ideattach[0] > '0') { /* takes the form [controller]<m/s> such as: 1m for primary master */
            ide_index = ideattach[0] - '1';
            if (ideattach.length() >= 1) ide_slave = (ideattach[1] == 's');
            LOG_MSG("IDE: index %d slave=%d",ide_index,ide_slave?1:0);
        }

        if (type == "floppy") {
			mediaid = 0xF0;
		} else if (type == "iso") {
			//str_size="2048,1,65535,0";	// ignored, see drive_iso.cpp (AllocationInfo)
			mediaid = 0xF8;
			fstype = "iso";
		}

		cmd->FindString("-size",str_size,true);
		if ((type=="hdd") && (str_size.size()==0)) {
			imgsizedetect = true;
		} else {
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
		}

		if (fstype=="fat" || fstype=="iso") {
			// get the drive letter
			if (!cmd->FindCommand(1,temp_line) || (temp_line.size() > 2) || ((temp_line.size() > 1) && (temp_line[1]!=':'))) {
				WriteOut_NoParsing(MSG_Get("PROGRAM_IMGMOUNT_SPECIFY_DRIVE"));
				return;
			}
			const int i_drive = toupper(temp_line[0]);
			if (i_drive < 'A' || i_drive > 'Z') {
				WriteOut_NoParsing(MSG_Get("PROGRAM_IMGMOUNT_SPECIFY_DRIVE"));
				return;
			}
			drive = int_to_char(i_drive);
		} else if (fstype=="none") {
			cmd->FindCommand(1,temp_line);
			if ((temp_line.size() > 1) || (!isdigit(temp_line[0]))) {
				WriteOut_NoParsing(MSG_Get("PROGRAM_IMGMOUNT_SPECIFY2"));
				return;
			}
			drive = temp_line[0];
			if ((drive<'0') || (drive>=(MAX_DISK_IMAGES+'0'))) {
				WriteOut_NoParsing(MSG_Get("PROGRAM_IMGMOUNT_SPECIFY2"));
				return;
			}
		} else {
			WriteOut(MSG_Get("PROGRAM_IMGMOUNT_FORMAT_UNSUPPORTED"),fstype.c_str());
			return;
		}

		// find all file parameters, assuming that all option parameters have been removed
		while (cmd->FindCommand((unsigned int)(paths.size() + 2), temp_line) && temp_line.size()) {
			// Try to find the path on native filesystem first
			const std::string real_path = to_native_path(temp_line);
			if (real_path.empty()) {
				LOG_MSG("IMGMOUNT: Path '%s' not found, maybe it's a DOS path",
				        temp_line.c_str());
			} else {
				std::string home_resolve = temp_line;
				Cross::ResolveHomedir(home_resolve);
				if (home_resolve == real_path) {
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
			struct stat test;
			if (stat(temp_line.c_str(),&test)) {
				//See if it works if the ~ are written out
				std::string homedir(temp_line);
				Cross::ResolveHomedir(homedir);
				if (!stat(homedir.c_str(),&test)) {
					temp_line = std::move(homedir);
				} else {
					// convert dosbox filename to system filename
					char fullname[CROSS_LEN];
					char tmp[CROSS_LEN];
					safe_strcpy(tmp, temp_line.c_str());

					Bit8u dummy;
					if (!DOS_MakeName(tmp, fullname, &dummy) || strncmp(Drives[dummy]->GetInfo(),"local directory",15)) {
						WriteOut(MSG_Get("PROGRAM_IMGMOUNT_NON_LOCAL_DRIVE"));
						return;
					}

					localDrive *ldp = dynamic_cast<localDrive*>(Drives[dummy]);
					if (ldp==NULL) {
						WriteOut(MSG_Get("PROGRAM_IMGMOUNT_FILE_NOT_FOUND"));
						return;
					}
					ldp->GetSystemFilename(tmp, fullname);
					temp_line = tmp;

					if (stat(temp_line.c_str(),&test)) {
						WriteOut(MSG_Get("PROGRAM_IMGMOUNT_FILE_NOT_FOUND"));
						return;
					}

					LOG_MSG("IMGMOUNT: Path '%s' found on virtual drive %c:",
					        fullname, drive_letter(dummy));
				}
			}
			if (S_ISDIR(test.st_mode)) {
				WriteOut(MSG_Get("PROGRAM_IMGMOUNT_MOUNT"));
				return;
			}
			paths.push_back(temp_line);
		}
		if (paths.size() == 0) {
			WriteOut(MSG_Get("PROGRAM_IMGMOUNT_SPECIFY_FILE"));
			return;
		}
		if (paths.size() == 1)
			temp_line = paths[0];

		if (fstype=="fat") {
			if (imgsizedetect) {
				FILE * diskfile = fopen_wrap(temp_line.c_str(), "rb+");
				if (!diskfile) {
					WriteOut(MSG_Get("PROGRAM_IMGMOUNT_INVALID_IMAGE"));
					return;
				}
				fseek(diskfile, 0L, SEEK_END);
				Bit32u fcsize = (Bit32u)(ftell(diskfile) / 512L);
				Bit8u buf[512];
				fseek(diskfile, 0L, SEEK_SET);
				if (fread(buf,sizeof(Bit8u),512,diskfile)<512) {
					fclose(diskfile);
					WriteOut(MSG_Get("PROGRAM_IMGMOUNT_INVALID_IMAGE"));
					return;
				}
				fclose(diskfile);
				if ((buf[510]!=0x55) || (buf[511]!=0xaa)) {
					WriteOut(MSG_Get("PROGRAM_IMGMOUNT_INVALID_GEOMETRY"));
					return;
				}
				Bitu sectors=(Bitu)(fcsize/(16*63));
				if (sectors*16*63!=fcsize) {
					WriteOut(MSG_Get("PROGRAM_IMGMOUNT_INVALID_GEOMETRY"));
					return;
				}
				sizes[0]=512;	sizes[1]=63;	sizes[2]=16;	sizes[3]=sectors;

				LOG_MSG("autosized image file: %d:%d:%d:%d",sizes[0],sizes[1],sizes[2],sizes[3]);
			}

			if (Drives[drive_index(drive)]) {
				WriteOut(MSG_Get("PROGRAM_IMGMOUNT_ALREADY_MOUNTED"));
				return;
			}

			std::vector<DOS_Drive*> imgDisks;
			std::vector<std::string>::size_type i;
			std::vector<DOS_Drive*>::size_type ct;

			for (i = 0; i < paths.size(); i++) {
				std::unique_ptr<fatDrive> newDrive(
					new fatDrive(paths[i].c_str(),sizes[0],sizes[1],sizes[2],sizes[3],0));

				if (newDrive->created_successfully) {
					imgDisks.push_back(static_cast<DOS_Drive*>(newDrive.release()));
				} else {
					// Tear-down all prior drives when we hit a problem
					WriteOut(MSG_Get("PROGRAM_IMGMOUNT_CANT_CREATE"));
					for (auto pImgDisk : imgDisks) {
						delete pImgDisk;
					}
					return;
				}
			}

			// Update DriveManager
			for (ct = 0; ct < imgDisks.size(); ct++) {
				DriveManager::AppendDisk(drive_index(drive),
				                         imgDisks[ct]);
			}
			DriveManager::InitializeDrive(drive_index(drive));

			// Set the correct media byte in the table
			mem_writeb(Real2Phys(dos.tables.mediaid) +
			                   drive_index(drive) * 9,
			           mediaid);

			/* Command uses dta so set it to our internal dta */
			RealPt save_dta = dos.dta();
			dos.dta(dos.tables.tempdta);

			for (ct = 0; ct < imgDisks.size(); ct++) {
				const bool notify = (ct == (imgDisks.size() - 1));
				DriveManager::CycleDisks(drive_index(drive), notify);
				char root[7] = {drive,':','\\','*','.','*',0};
				DOS_FindFirst(root, DOS_ATTR_VOLUME); // force obtaining the label and saving it in dirCache
			}
			dos.dta(save_dta);

			std::string tmp(paths[0]);
			for (i = 1; i < paths.size(); i++) {
				tmp += "; " + paths[i];
			}
			WriteOut(MSG_Get("PROGRAM_MOUNT_STATUS_2"), drive, tmp.c_str());

			if (paths.size() == 1) {
				auto *newdrive = static_cast<fatDrive*>(imgDisks[0]);
				if (('A' <= drive && drive <= 'B' && !(newdrive->loadedDisk->hardDrive)) ||
				    ('C' <= drive && drive <= 'D' && newdrive->loadedDisk->hardDrive)) {
					const size_t idx = drive_index(drive);
					imageDiskList[idx] = newdrive->loadedDisk;
					updateDPT();
				}
			}
		} else if (fstype=="iso") {
			if (Drives[drive_index(drive)]) {
				WriteOut(MSG_Get("PROGRAM_IMGMOUNT_ALREADY_MOUNTED"));
				return;
			}

			// create new drives for all images
			std::vector<DOS_Drive*> isoDisks;
			std::vector<std::string>::size_type i;
			std::vector<DOS_Drive*>::size_type ct;
			for (i = 0; i < paths.size(); i++) {
				int error = -1;
				DOS_Drive* newDrive = new isoDrive(drive, paths[i].c_str(), mediaid, error);
				isoDisks.push_back(newDrive);
				switch (error) {
					case 0  :	break;
					case 1  :	WriteOut(MSG_Get("MSCDEX_ERROR_MULTIPLE_CDROMS"));	break;
					case 2  :	WriteOut(MSG_Get("MSCDEX_ERROR_NOT_SUPPORTED"));	break;
					case 3  :	WriteOut(MSG_Get("MSCDEX_ERROR_OPEN"));				break;
					case 4  :	WriteOut(MSG_Get("MSCDEX_TOO_MANY_DRIVES"));		break;
					case 5  :	WriteOut(MSG_Get("MSCDEX_LIMITED_SUPPORT"));		break;
					case 6  :	WriteOut(MSG_Get("MSCDEX_INVALID_FILEFORMAT"));		break;
					default :	WriteOut(MSG_Get("MSCDEX_UNKNOWN_ERROR"));			break;
				}
				// error: clean up and leave
				if (error) {
					for (ct = 0; ct < isoDisks.size(); ct++) {
						delete isoDisks[ct];
					}
					return;
				}
			}
			// Update DriveManager
			for (ct = 0; ct < isoDisks.size(); ct++) {
				DriveManager::AppendDisk(drive_index(drive),
				                         isoDisks[ct]);
			}
			DriveManager::InitializeDrive(drive_index(drive));

			// Set the correct media byte in the table
			mem_writeb(Real2Phys(dos.tables.mediaid) +
			                   drive_index(drive) * 9,
			           mediaid);

            // If instructed, attach to IDE controller as ATAPI CD-ROM device
            if (ide_index >= 0) IDE_CDROM_Attach(ide_index, ide_slave, drive - 'A');

			// Print status message (success)
			WriteOut(MSG_Get("MSCDEX_SUCCESS"));
			std::string tmp(paths[0]);
			for (i = 1; i < paths.size(); i++) {
				tmp += "; " + paths[i];
			}
			WriteOut(MSG_Get("PROGRAM_MOUNT_STATUS_2"), drive, tmp.c_str());

		} else if (fstype == "none") {
			FILE *newDisk = fopen_wrap(temp_line.c_str(), "rb+");
			if (!newDisk) {
				WriteOut(MSG_Get("PROGRAM_IMGMOUNT_INVALID_IMAGE"));
				return;
			}
			fseek(newDisk,0L, SEEK_END);
			Bit32u imagesize = (ftell(newDisk) / 1024);
			const bool hdd = (imagesize > 2880);
			//Seems to make sense to require a valid geometry..
			if (hdd && sizes[0] == 0 && sizes[1] == 0 && sizes[2] == 0 && sizes[3] == 0) {
				fclose(newDisk);
				WriteOut(MSG_Get("PROGRAM_IMGMOUNT_SPECIFY_GEOMETRY"));
				return;
			}

			imageDisk * newImage = new imageDisk(newDisk, temp_line.c_str(), imagesize, hdd);

			if (hdd) newImage->Set_Geometry(sizes[2],sizes[3],sizes[1],sizes[0]);
			imageDiskList[drive - '0'].reset(newImage);
			updateDPT();
			WriteOut(MSG_Get("PROGRAM_IMGMOUNT_MOUNT_NUMBER"),drive - '0',temp_line.c_str());
		}

		// check if volume label is given. be careful for cdrom
		//if (cmd->FindString("-label",label,true)) newdrive->dirCache.SetLabel(label.c_str());
		return;
	}
};

void IMGMOUNT_ProgramStart(Program * * make) {
	*make=new IMGMOUNT;
}


Bitu DOS_SwitchKeyboardLayout(const char* new_layout, Bit32s& tried_cp);
Bitu DOS_LoadKeyboardLayout(const char * layoutname, Bit32s codepage, const char * codepagefile);
const char* DOS_GetLoadedLayout(void);

class KEYB final : public Program {
public:
	void Run(void);
};

void KEYB::Run(void) {
	if (cmd->FindCommand(1,temp_line)) {
		if (cmd->FindString("?",temp_line,false)) {
			WriteOut(MSG_Get("PROGRAM_KEYB_SHOWHELP"));
		} else {
			/* first parameter is layout ID */
			Bitu keyb_error=0;
			std::string cp_string;
			Bit32s tried_cp = -1;
			if (cmd->FindCommand(2,cp_string)) {
				/* second parameter is codepage number */
				tried_cp=atoi(cp_string.c_str());
				char cp_file_name[256];
				if (cmd->FindCommand(3,cp_string)) {
					/* third parameter is codepage file */
					safe_strcpy(cp_file_name, cp_string.c_str());
				} else {
					/* no codepage file specified, use automatic selection */
					safe_strcpy(cp_file_name, "auto");
				}

				keyb_error=DOS_LoadKeyboardLayout(temp_line.c_str(), tried_cp, cp_file_name);
			} else {
				keyb_error=DOS_SwitchKeyboardLayout(temp_line.c_str(), tried_cp);
			}
			switch (keyb_error) {
				case KEYB_NOERROR:
					WriteOut(MSG_Get("PROGRAM_KEYB_NOERROR"),temp_line.c_str(),dos.loaded_codepage);
					break;
				case KEYB_FILENOTFOUND:
					WriteOut(MSG_Get("PROGRAM_KEYB_FILENOTFOUND"),temp_line.c_str());
					WriteOut(MSG_Get("PROGRAM_KEYB_SHOWHELP"));
					break;
				case KEYB_INVALIDFILE:
					WriteOut(MSG_Get("PROGRAM_KEYB_INVALIDFILE"),temp_line.c_str());
					break;
				case KEYB_LAYOUTNOTFOUND:
					WriteOut(MSG_Get("PROGRAM_KEYB_LAYOUTNOTFOUND"),temp_line.c_str(),tried_cp);
					break;
				case KEYB_INVALIDCPFILE:
					WriteOut(MSG_Get("PROGRAM_KEYB_INVCPFILE"),temp_line.c_str());
					WriteOut(MSG_Get("PROGRAM_KEYB_SHOWHELP"));
					break;
				default:
					LOG(LOG_DOSMISC,LOG_ERROR)("KEYB:Invalid returncode %x",keyb_error);
					break;
			}
		}
	} else {
		/* no parameter in the command line, just output codepage info and possibly loaded layout ID */
		const char* layout_name = DOS_GetLoadedLayout();
		if (layout_name==NULL) {
			WriteOut(MSG_Get("PROGRAM_KEYB_INFO"),dos.loaded_codepage);
		} else {
			WriteOut(MSG_Get("PROGRAM_KEYB_INFO_LAYOUT"),dos.loaded_codepage,layout_name);
		}
	}
}

static void KEYB_ProgramStart(Program * * make) {
	*make=new KEYB;
}

void DOS_SetupPrograms(void) {
	/*Add Messages */

	MSG_Add("PROGRAM_MOUNT_CDROMS_FOUND","CDROMs found: %d\n");
	MSG_Add("PROGRAM_MOUNT_STATUS_DRIVE", "Drive");
	MSG_Add("PROGRAM_MOUNT_STATUS_TYPE", "Type");
	MSG_Add("PROGRAM_MOUNT_STATUS_LABEL", "Label");
	MSG_Add("PROGRAM_MOUNT_STATUS_2","Drive %c is mounted as %s\n");
	MSG_Add("PROGRAM_MOUNT_STATUS_1", "The currently mounted drives are:\n");
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

	MSG_Add("PROGRAM_MEM_CONVEN", "%10d kB free conventional memory\n");
	MSG_Add("PROGRAM_MEM_EXTEND", "%10d kB free extended memory\n");
	MSG_Add("PROGRAM_MEM_EXPAND", "%10d kB free expanded memory\n");
	MSG_Add("PROGRAM_MEM_UPPER",
	        "%10d kB free upper memory in %d blocks (largest UMB %d kB)\n");

	MSG_Add("PROGRAM_LOADFIX_ALLOC", "%d kB allocated.\n");
	MSG_Add("PROGRAM_LOADFIX_DEALLOC", "%d kB freed.\n");
	MSG_Add("PROGRAM_LOADFIX_DEALLOCALL","Used memory freed.\n");
	MSG_Add("PROGRAM_LOADFIX_ERROR","Memory allocation error.\n");

	MSG_Add("MSCDEX_SUCCESS","MSCDEX installed.\n");
	MSG_Add("MSCDEX_ERROR_MULTIPLE_CDROMS","MSCDEX: Failure: Drive-letters of multiple CD-ROM drives have to be continuous.\n");
	MSG_Add("MSCDEX_ERROR_NOT_SUPPORTED","MSCDEX: Failure: Not yet supported.\n");
	MSG_Add("MSCDEX_ERROR_PATH","MSCDEX: Specified location is not a CD-ROM drive.\n");
	MSG_Add("MSCDEX_ERROR_OPEN","MSCDEX: Failure: Invalid file or unable to open.\n");
	MSG_Add("MSCDEX_TOO_MANY_DRIVES","MSCDEX: Failure: Too many CD-ROM drives (max: 5). MSCDEX Installation failed.\n");
	MSG_Add("MSCDEX_LIMITED_SUPPORT","MSCDEX: Mounted subdirectory: limited support.\n");
	MSG_Add("MSCDEX_INVALID_FILEFORMAT","MSCDEX: Failure: File is either no ISO/CUE image or contains errors.\n");
	MSG_Add("MSCDEX_UNKNOWN_ERROR","MSCDEX: Failure: Unknown error.\n");
	MSG_Add("MSCDEX_WARNING_NO_OPTION", "MSCDEX: Warning: Ignoring unsupported option '%s'.\n");

	MSG_Add("PROGRAM_RESCAN_SUCCESS","Drive cache cleared.\n");

	MSG_Add("PROGRAM_INTRO",
		"\033[2J\033[32;1mWelcome to DOSBox Staging\033[0m, an x86 emulator with sound and graphics.\n"
		"DOSBox creates a shell for you which looks like old plain DOS.\n"
		"\n"
		"For information about basic mount type \033[34;1mintro mount\033[0m\n"
		"For information about CD-ROM support type \033[34;1mintro cdrom\033[0m\n"
		"For information about special keys type \033[34;1mintro special\033[0m\n"
		"For more imformation, visit DOSBox Staging wiki:\033[34;1m\n"
		"https://github.com/dosbox-staging/dosbox-staging/wiki\033[0m\n"
		"\n"
		"\033[31;1mDOSBox will stop/exit without a warning if an error occurred!\033[0m\n"
		);
	MSG_Add("PROGRAM_INTRO_MOUNT_START",
		"\033[2J\033[32;1mHere are some commands to get you started:\033[0m\n"
		"Before you can use the files located on your own filesystem,\n"
		"You have to mount the directory containing the files.\n"
		"\n"
		);
	MSG_Add("PROGRAM_INTRO_MOUNT_WINDOWS",
		"\033[44;1m\xC9\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
		"\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
		"\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xBB\n"
		"\xBA \033[32mmount c c:\\dosgames\\\033[37m will create a C drive with c:\\dosgames as contents.\xBA\n"
		"\xBA                                                                         \xBA\n"
		"\xBA \033[32mc:\\dosgames\\\033[37m is an example. Replace it with your own games directory.  \033[37m \xBA\n"
		"\xC8\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
		"\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
		"\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xBC\033[0m\n"
		);
	MSG_Add("PROGRAM_INTRO_MOUNT_OTHER",
		"\033[44;1m\xC9\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
		"\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
		"\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xBB\n"
		"\xBA \033[32mmount c ~/dosgames\033[37m will create a C drive with ~/dosgames as contents.\xBA\n"
		"\xBA                                                                      \xBA\n"
		"\xBA \033[32m~/dosgames\033[37m is an example. Replace it with your own games directory.\033[37m  \xBA\n"
		"\xC8\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
		"\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
		"\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xBC\033[0m\n"
		);
	MSG_Add("PROGRAM_INTRO_MOUNT_END",
		"After successfully mounting the disk you can type \033[34;1mc:\033[0m to go to your freshly\n"
		"mounted C-drive. Typing \033[34;1mdir\033[0m there will show its contents."
		" \033[34;1mcd\033[0m will allow you to\n"
		"enter a directory (recognised by the \033[33;1m[]\033[0m in a directory listing).\n"
		"You can run programs/files with extensions \033[31m.exe .bat\033[0m and \033[31m.com\033[0m.\n"
		);
	MSG_Add("PROGRAM_INTRO_CDROM_WINDOWS",
	        "\033[2J\033[32;1mHow to mount a virtual CD-ROM Drive in DOSBox:\033[0m\n"
	        "DOSBox provides CD-ROM emulation on several levels.\n"
	        "\n"
	        "This works on all normal directories, installs MSCDEX and marks the files\n"
	        "read-only. Usually this is enough for most games:\n"
	        "\n"
	        "\033[34;1mmount D C:\\example -t cdrom\033[0m\n"
	        "\n"
	        "If it doesn't work you might have to tell DOSBox the label of the CD-ROM:\n"
	        "\n"
	        "\033[34;1mmount D C:\\example -t cdrom -label CDLABEL\033[0m\n"
	        "\n"
	        "Additionally, you can use imgmount to mount iso or cue/bin images:\n"
	        "\n"
	        "\033[34;1mimgmount D C:\\cd.iso -t cdrom\033[0m\n"
	        "\n"
	        "\033[34;1mimgmount D C:\\cd.cue -t cdrom\033[0m\n");
	MSG_Add("PROGRAM_INTRO_CDROM_OTHER",
	        "\033[2J\033[32;1mHow to mount a virtual CD-ROM Drive in DOSBox:\033[0m\n"
	        "DOSBox provides CD-ROM emulation on several levels.\n"
	        "\n"
	        "This works on all normal directories, installs MSCDEX and marks the files\n"
	        "read-only. Usually this is enough for most games:\n"
	        "\n"
	        "\033[34;1mmount D ~/example -t cdrom\033[0m\n"
	        "\n"
	        "If it doesn't work you might have to tell DOSBox the label of the CD-ROM:\n"
	        "\n"
	        "\033[34;1mmount D ~/example -t cdrom -label CDLABEL\033[0m\n"
	        "\n"
	        "Additionally, you can use imgmount to mount iso or cue/bin images:\n"
	        "\n"
	        "\033[34;1mimgmount D ~/cd.iso -t cdrom\033[0m\n"
	        "\n"
	        "\033[34;1mimgmount D ~/cd.cue -t cdrom\033[0m\n");
	MSG_Add("PROGRAM_INTRO_SPECIAL",
	        "\033[2J\033[32;1mSpecial keys:\033[0m\n"
	        "These are the default keybindings.\n"
	        "They can be changed in the \033[33mkeymapper\033[0m.\n"
	        "\n"
	        "\033[33;1m" MMOD2_NAME "+Enter\033[0m  Switch between fullscreen and window mode.\n"
	        "\033[33;1m" MMOD2_NAME "+Pause\033[0m  Pause/Unpause emulator.\n"
	        "\033[33;1m" PRIMARY_MOD_NAME "+F1\033[0m   " PRIMARY_MOD_PAD " Start the \033[33mkeymapper\033[0m.\n"
	        "\033[33;1m" PRIMARY_MOD_NAME "+F4\033[0m   " PRIMARY_MOD_PAD " Swap mounted disk image, update directory cache for all drives.\n"
	        "\033[33;1m" PRIMARY_MOD_NAME "+F5\033[0m   " PRIMARY_MOD_PAD " Save a screenshot.\n"
	        "\033[33;1m" PRIMARY_MOD_NAME "+F6\033[0m   " PRIMARY_MOD_PAD " Start/Stop recording sound output to a wave file.\n"
	        "\033[33;1m" PRIMARY_MOD_NAME "+F7\033[0m   " PRIMARY_MOD_PAD " Start/Stop recording video output to a zmbv file.\n"
	        "\033[33;1m" PRIMARY_MOD_NAME "+F9\033[0m   " PRIMARY_MOD_PAD " Shutdown emulator.\n"
	        "\033[33;1m" PRIMARY_MOD_NAME "+F10\033[0m  " PRIMARY_MOD_PAD " Capture/Release the mouse.\n"
	        "\033[33;1m" PRIMARY_MOD_NAME "+F11\033[0m  " PRIMARY_MOD_PAD " Slow down emulation.\n"
	        "\033[33;1m" PRIMARY_MOD_NAME "+F12\033[0m  " PRIMARY_MOD_PAD " Speed up emulation.\n"
	        "\033[33;1m" MMOD2_NAME "+F12\033[0m    Unlock speed (turbo button/fast forward).\n");

	MSG_Add("PROGRAM_BOOT_NOT_EXIST","Bootdisk file does not exist.  Failing.\n");
	MSG_Add("PROGRAM_BOOT_NOT_OPEN","Cannot open bootdisk file.  Failing.\n");
	MSG_Add("PROGRAM_BOOT_WRITE_PROTECTED","Image file is read-only! Might create problems.\n");
	MSG_Add("PROGRAM_BOOT_PRINT_ERROR",
	        "This command boots DOSBox from either a floppy or hard disk image.\n\n"
	        "For this command, one can specify a succession of floppy disks swappable\n"
	        "by pressing " PRIMARY_MOD_NAME "+F4, and -l specifies the mounted drive to boot from.  If\n"
	        "no drive letter is specified, this defaults to booting from the A drive.\n"
	        "The only bootable drive letters are A, C, and D.  For booting from a hard\n"
	        "drive (C or D), the image should have already been mounted using the\n"
	        "\033[34;1mIMGMOUNT\033[0m command.\n\n"
	        "The syntax of this command is:\n\n"
	        "\033[34;1mBOOT [diskimg1.img diskimg2.img] [-l driveletter]\033[0m\n");
	MSG_Add("PROGRAM_BOOT_UNABLE","Unable to boot off of drive %c");
	MSG_Add("PROGRAM_BOOT_IMAGE_OPEN","Opening image file: %s\n");
	MSG_Add("PROGRAM_BOOT_IMAGE_NOT_OPEN","Cannot open %s");
	MSG_Add("PROGRAM_BOOT_BOOT","Booting from drive %c...\n");
	MSG_Add("PROGRAM_BOOT_CART_WO_PCJR","PCjr cartridge found, but machine is not PCjr");
	MSG_Add("PROGRAM_BOOT_CART_LIST_CMDS", "Available PCjr cartridge commands: %s");
	MSG_Add("PROGRAM_BOOT_CART_NO_CMDS", "No PCjr cartridge commands found");

	MSG_Add("PROGRAM_LOADROM_SPECIFY_FILE","Must specify ROM file to load.\n");
	MSG_Add("PROGRAM_LOADROM_CANT_OPEN","ROM file not accessible.\n");
	MSG_Add("PROGRAM_LOADROM_TOO_LARGE","ROM file too large.\n");
	MSG_Add("PROGRAM_LOADROM_INCOMPATIBLE","Video BIOS not supported by machine type.\n");
	MSG_Add("PROGRAM_LOADROM_UNRECOGNIZED","ROM file not recognized.\n");
	MSG_Add("PROGRAM_LOADROM_BASIC_LOADED","BASIC ROM loaded.\n");

	MSG_Add("SHELL_CMD_IMGMOUNT_HELP",
	        "mounts compact disc image(s) or floppy disk image(s) to a given drive letter.\n");

	MSG_Add("SHELL_CMD_IMGMOUNT_HELP_LONG",
	        "Mount a CD-ROM, floppy, or disk image to a drive letter.\n"
	        "\n"
	        "Usage:\n"
	        "  \033[32;1mimgmount\033[0m \033[37;1mDRIVE\033[0m \033[36;1mCDROM-SET\033[0m [CDROM-SET2 [..]] [-fs iso] -t cdrom|iso\n"
	        "  \033[32;1mimgmount\033[0m \033[37;1mDRIVE\033[0m \033[36;1mIMAGEFILE\033[0m [IMAGEFILE2 [..]] [-fs fat] -t hdd|floppy\n"
	        "  \033[32;1mimgmount\033[0m \033[37;1mDRIVE\033[0m \033[36;1mBOOTIMAGE\033[0m [-fs fat|none] -t hdd -size GEOMETRY\n"
	        "  \033[32;1mimgmount\033[0m -u \033[37;1mDRIVE\033[0m  (unmounts the DRIVE's image)\n"
	        "\n"
	        "Where:\n"
	        "  \033[37;1mDRIVE\033[0m     is the drive letter where the image will be mounted: a, c, d, ...\n"
	        "  \033[36;1mCDROM-SET\033[0m is an ISO, CUE+BIN, CUE+ISO, or CUE+ISO+FLAC/OPUS/OGG/MP3/WAV\n"
	        "  \033[36;1mIMAGEFILE\033[0m is a hard drive or floppy image in FAT16 or FAT12 format\n"
	        "  \033[36;1mBOOTIMAGE\033[0m is a bootable disk image with specified -size GEOMETRY:\n"
	        "            bytes-per-sector,sectors-per-head,heads,cylinders\n"
	        "Notes:\n"
	        "  - " PRIMARY_MOD_NAME "+F4 swaps & mounts the next CDROM-SET or IMAGEFILE, if provided.\n"
	        "\n"
	        "Examples:\n"
#if defined(WIN32)
	        "  \033[32;1mimgmount\033[0m \033[37;1mD\033[0m \033[36;1mC:\\games\\doom.iso\033[0m -t cdrom\n"
	        "  \033[32;1mimgmount\033[0m \033[37;1mD\033[0m \033[36;1mcd/quake1.cue\033[0m -t cdrom\n"
	        "  \033[32;1mimgmount\033[0m \033[37;1mA\033[0m \033[36;1mfloppy1.img floppy2.img floppy3.img\033[0m -t floppy\n"
	        "  \033[32;1mimgmount\033[0m \033[37;1mC\033[0m \033[36;1mC:\\dos\\c_drive.img\033[0m -t hdd\n"
	        "  \033[32;1mimgmount\033[0m \033[37;1mC\033[0m \033[36;1mbootable.img\033[0m -t hdd -fs none -size 512,63,32,1023\n"
#elif defined(MACOSX)
	        "  \033[32;1mimgmount\033[0m \033[37;1mD\033[0m \033[36;1m/Users/USERNAME/games/doom.iso\033[0m -t cdrom\n"
	        "  \033[32;1mimgmount\033[0m \033[37;1mD\033[0m \033[36;1mcd/quake1.cue\033[0m -t cdrom\n"
	        "  \033[32;1mimgmount\033[0m \033[37;1mA\033[0m \033[36;1mfloppy1.img floppy2.img floppy3.img\033[0m -t floppy\n"
	        "  \033[32;1mimgmount\033[0m \033[37;1mC\033[0m \033[36;1m~/dos/c_drive.img\033[0m -t hdd\n"
	        "  \033[32;1mimgmount\033[0m \033[37;1mC\033[0m \033[36;1mbootable.img\033[0m -t hdd -fs none -size 512,63,32,1023\n"
#else
	        "  \033[32;1mimgmount\033[0m \033[37;1mD\033[0m \033[36;1m/home/USERNAME/games/doom.iso\033[0m -t cdrom\n"
	        "  \033[32;1mimgmount\033[0m \033[37;1mD\033[0m \033[36;1mcd/quake1.cue\033[0m -t cdrom\n"
	        "  \033[32;1mimgmount\033[0m \033[37;1mA\033[0m \033[36;1mfloppy1.img floppy2.img floppy3.img\033[0m -t floppy\n"
	        "  \033[32;1mimgmount\033[0m \033[37;1mC\033[0m \033[36;1m~/dos/c_drive.img\033[0m -t hdd\n"
	        "  \033[32;1mimgmount\033[0m \033[37;1mC\033[0m \033[36;1mbootable.img\033[0m -t hdd -fs none -size 512,63,32,1023\n"
#endif
	);

	MSG_Add("SHELL_CMD_MOUNT_HELP",
	        "maps physical folders or drives to a virtual drive letter.\n");

	MSG_Add("SHELL_CMD_MOUNT_HELP_LONG",
	        "Mount a directory from the host OS to a drive letter.\n"
	        "\n"
	        "Usage:\n"
	        "  \033[32;1mmount\033[0m \033[37;1mDRIVE\033[0m \033[36;1mDIRECTORY\033[0m [-t TYPE] [-freesize SIZE] [-label LABEL]\n"
	        "  \033[32;1mmount\033[0m -u \033[37;1mDRIVE\033[0m  (unmounts the DRIVE's directory)\n"
	        "\n"
	        "Where:\n"
	        "  \033[37;1mDRIVE\033[0m     the drive letter where the directory will be mounted: A, C, D, ...\n"
	        "  \033[36;1mDIRECTORY\033[0m is the directory on the host OS to be mounted\n"
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
	        "  \033[32;1mmount\033[0m \033[37;1mC\033[0m \033[36;1mC:\\dosgames\033[0m\n"
	        "  \033[32;1mmount\033[0m \033[37;1mD\033[0m \033[36;1mD:\\\033[0m -t cdrom\n"
	        "  \033[32;1mmount\033[0m \033[37;1mC\033[0m \033[36;1mmy_savegame_files\033[0m -t overlay\n"
#elif defined(MACOSX)
	        "  \033[32;1mmount\033[0m \033[37;1mC\033[0m \033[36;1m~/dosgames\033[0m\n"
	        "  \033[32;1mmount\033[0m \033[37;1mD\033[0m \033[36;1m\"/Volumes/Game CD\"\033[0m -t cdrom\n"
	        "  \033[32;1mmount\033[0m \033[37;1mC\033[0m \033[36;1mmy_savegame_files\033[0m -t overlay\n"
#else
	        "  \033[32;1mmount\033[0m \033[37;1mC\033[0m \033[36;1m~/dosgames\033[0m\n"
	        "  \033[32;1mmount\033[0m \033[37;1mD\033[0m \033[36;1m\"/media/USERNAME/Game CD\"\033[0m -t cdrom\n"
	        "  \033[32;1mmount\033[0m \033[37;1mC\033[0m \033[36;1mmy_savegame_files\033[0m -t overlay\n"
#endif
	);

	MSG_Add("PROGRAM_IMGMOUNT_SPECIFY_DRIVE",
	        "Must specify drive letter to mount image at.\n");
	MSG_Add("PROGRAM_IMGMOUNT_SPECIFY2",
	        "Must specify drive number (0 or 3) to mount image at (0,1=fda,fdb;2,3=hda,hdb).\n");
	MSG_Add("PROGRAM_IMGMOUNT_SPECIFY_GEOMETRY",
		"For \033[33mCD-ROM\033[0m images:   \033[34;1mIMGMOUNT drive-letter location-of-image -t iso\033[0m\n"
		"\n"
		"For \033[33mhardrive\033[0m images: Must specify drive geometry for hard drives:\n"
		"bytes_per_sector, sectors_per_cylinder, heads_per_cylinder, cylinder_count.\n"
		"\033[34;1mIMGMOUNT drive-letter location-of-image -size bps,spc,hpc,cyl\033[0m\n");
	MSG_Add("PROGRAM_IMGMOUNT_INVALID_IMAGE","Could not load image file.\n"
		"Check that the path is correct and the image is accessible.\n");
	MSG_Add("PROGRAM_IMGMOUNT_INVALID_GEOMETRY","Could not extract drive geometry from image.\n"
		"Use parameter -size bps,spc,hpc,cyl to specify the geometry.\n");
	MSG_Add("PROGRAM_IMGMOUNT_TYPE_UNSUPPORTED",
	        "Type '%s' is unsupported. Specify 'floppy', 'hdd', 'cdrom', or 'iso'.\n");
	MSG_Add("PROGRAM_IMGMOUNT_FORMAT_UNSUPPORTED","Format \"%s\" is unsupported. Specify \"fat\" or \"iso\" or \"none\".\n");
	MSG_Add("PROGRAM_IMGMOUNT_SPECIFY_FILE","Must specify file-image to mount.\n");
	MSG_Add("PROGRAM_IMGMOUNT_FILE_NOT_FOUND","Image file not found.\n");
	MSG_Add("PROGRAM_IMGMOUNT_MOUNT","To mount directories, use the \033[34;1mMOUNT\033[0m command, not the \033[34;1mIMGMOUNT\033[0m command.\n");
	MSG_Add("PROGRAM_IMGMOUNT_ALREADY_MOUNTED","Drive already mounted at that letter.\n");
	MSG_Add("PROGRAM_IMGMOUNT_CANT_CREATE","Can't create drive from file.\n");
	MSG_Add("PROGRAM_IMGMOUNT_MOUNT_NUMBER","Drive number %d mounted as %s\n");
	MSG_Add("PROGRAM_IMGMOUNT_NON_LOCAL_DRIVE", "The image must be on a host or local drive.\n");
	MSG_Add("PROGRAM_IMGMOUNT_MULTIPLE_NON_CUEISO_FILES", "Using multiple files is only supported for cue/iso images.\n");

	MSG_Add("PROGRAM_KEYB_INFO","Codepage %i has been loaded\n");
	MSG_Add("PROGRAM_KEYB_INFO_LAYOUT","Codepage %i has been loaded for layout %s\n");
	MSG_Add("PROGRAM_KEYB_SHOWHELP",
		"\033[32;1mKEYB\033[0m [keyboard layout ID[ codepage number[ codepage file]]]\n\n"
		"Some examples:\n"
		"  \033[32;1mKEYB\033[0m: Display currently loaded codepage.\n"
		"  \033[32;1mKEYB\033[0m sp: Load the spanish (SP) layout, use an appropriate codepage.\n"
		"  \033[32;1mKEYB\033[0m sp 850: Load the spanish (SP) layout, use codepage 850.\n"
		"  \033[32;1mKEYB\033[0m sp 850 mycp.cpi: Same as above, but use file mycp.cpi.\n");
	MSG_Add("PROGRAM_KEYB_NOERROR","Keyboard layout %s loaded for codepage %i\n");
	MSG_Add("PROGRAM_KEYB_FILENOTFOUND","Keyboard file %s not found\n\n");
	MSG_Add("PROGRAM_KEYB_INVALIDFILE","Keyboard file %s invalid\n");
	MSG_Add("PROGRAM_KEYB_LAYOUTNOTFOUND","No layout in %s for codepage %i\n");
	MSG_Add("PROGRAM_KEYB_INVCPFILE","None or invalid codepage file for layout %s\n\n");

	PROGRAMS_MakeFile("AUTOTYPE.COM", AUTOTYPE_ProgramStart);
#if C_DEBUG
	PROGRAMS_MakeFile("BIOSTEST.COM", BIOSTEST_ProgramStart);
#endif
	PROGRAMS_MakeFile("BOOT.COM", BOOT_ProgramStart);
	PROGRAMS_MakeFile("CHOICE.COM", CHOICE_ProgramStart);
	PROGRAMS_MakeFile("HELP.COM", HELP_ProgramStart);
	PROGRAMS_MakeFile("IMGMOUNT.COM", IMGMOUNT_ProgramStart);
	PROGRAMS_MakeFile("INTRO.COM", INTRO_ProgramStart);
	PROGRAMS_MakeFile("KEYB.COM", KEYB_ProgramStart);
	PROGRAMS_MakeFile("LOADFIX.COM", LOADFIX_ProgramStart);
	PROGRAMS_MakeFile("LOADROM.COM", LOADROM_ProgramStart);
	PROGRAMS_MakeFile("LS.COM", LS_ProgramStart);
	PROGRAMS_MakeFile("MEM.COM", MEM_ProgramStart);
	PROGRAMS_MakeFile("MOUNT.COM", MOUNT_ProgramStart);
	PROGRAMS_MakeFile("RESCAN.COM", RESCAN_ProgramStart);
}
