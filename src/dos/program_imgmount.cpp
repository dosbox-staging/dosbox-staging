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

#include "program_imgmount.h"

#include "dosbox.h"

#include <vector>

#include "bios_disk.h"
#include "control.h"
#include "cross.h"
#include "drives.h"
#include "fs_utils.h"
#include "ide.h"
#include "mapper.h"
#include "program_mount_common.h"
#include "shell.h"
#include "cdrom.h"
#include "string_utils.h"
#include "../ints/int10.h"

void IMGMOUNT::ListImgMounts(void)
{
	const std::string header_drive = MSG_Get("PROGRAM_MOUNT_STATUS_DRIVE");
	const std::string header_name = MSG_Get("PROGRAM_MOUNT_STATUS_NAME");
	const std::string header_label = MSG_Get("PROGRAM_MOUNT_STATUS_LABEL");
	const std::string header_slot = MSG_Get("PROGRAM_MOUNT_STATUS_SLOT");

	const int term_width = real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS);
	const auto width_drive = static_cast<int>(header_drive.length());
	const auto width_label = std::max(minimum_column_length,
	                                  static_cast<int>(header_label.length()));
	const auto width_slot = std::max(minimum_column_length,
	                                 static_cast<int>(header_slot.length()));
	const auto width_name = term_width - 4 - width_drive - width_label - width_slot;
	if (width_name < 0) {
		LOG_WARNING("Message is too long.");
		return;
	}

	auto print_row = [&](const std::string &txt_drive,
	                     const std::string &txt_name, const std::string &txt_label,
	                     const std::string &txt_slot) {
		WriteOut("%-*s %-*s %-*s %-*s\n", width_drive, txt_drive.c_str(),
		         width_name, txt_name.c_str(), width_label,
		         txt_label.c_str(), width_slot, txt_slot.c_str());
	};

	WriteOut(MSG_Get("PROGRAM_MOUNT_STATUS_1"));
	print_row(header_drive, header_name, header_label, header_slot);
	const std::string horizontal_divider(term_width, '-');
	WriteOut_NoParsing(horizontal_divider.c_str());

	bool found_drives = false;
	for (uint8_t d = 0; d < DOS_DRIVES; d++) {
		std::string disk_info = Drives[d] ? Drives[d]->GetInfo() : "";
		if (disk_info.substr(0, 9) == "fatDrive " ||
		    disk_info.substr(0, 9) == "isoDrive ") {
			print_row(std::string{drive_letter(d)}, disk_info.substr(9),
			          To_Label(Drives[d]->GetLabel()),
			          DriveManager::GetDrivePosition(d));
			found_drives = true;
		}
	}
	if (!found_drives)
		WriteOut(MSG_Get("PROGRAM_IMGMOUNT_STATUS_NONE"));
}

void IMGMOUNT::Run(void) {
    //Hack To allow long commandlines
    ChangeToLongCmd();

    if (!cmd->GetCount()) {
        ListImgMounts();
        return;
    }
    // Usage
    if (HelpRequested()) {
        WriteOut(MSG_Get("SHELL_CMD_IMGMOUNT_HELP_LONG"), PRIMARY_MOD_NAME);
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

    bool roflag = false;
    if (cmd->FindExist("-ro", true))
	    roflag = true;

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

    uint16_t sizes[4] = {0};
    bool imgsizedetect = false;

    std::string str_size = "";
    uint8_t mediaid = 0xF8;

    // Possibly used to hold the IDE channel and drive slot for CDROM types
    std::string ide_value = {};
    int8_t ide_index = -1;
    bool is_second_cable_slot = false;
    const bool wants_ide = cmd->FindString("-ide", ide_value, true) || cmd->FindExist("-ide", true);


    if (type == "floppy") {
	    mediaid = 0xF0;
    } else if (type == "iso") {
	    // str_size="2048,1,65535,0";	// ignored, see drive_iso.cpp
	    // (AllocationInfo)
	    mediaid = 0xF8;
	    fstype = "iso";

        if (wants_ide) {
            IDE_Get_Next_Cable_Slot(ide_index, is_second_cable_slot);
        }
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
        const auto real_path = to_native_path(temp_line);
        constexpr auto only_expand_files = true;
        constexpr auto skip_native_path = true;
        if (real_path.empty()) {
            if (get_expanded_files(temp_line, paths, only_expand_files, skip_native_path)) {
                temp_line = paths[0];
                continue;
            } else {
                LOG_MSG("IMGMOUNT: Path '%s' not found, maybe it's a DOS path",
                        temp_line.c_str());
            }
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

                uint8_t dummy;
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

                if (stat(temp_line.c_str(), &test)) {
                    if (get_expanded_files(temp_line, paths, only_expand_files, skip_native_path)) {
                        temp_line = paths[0];
                        continue;
                    } else {
                        WriteOut(MSG_Get("PROGRAM_IMGMOUNT_FILE_NOT_FOUND"));
                        return;
                    }
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
            FILE * diskfile = fopen_wrap_ro_fallback(temp_line, roflag);
            if (!diskfile) {
                WriteOut(MSG_Get("PROGRAM_IMGMOUNT_INVALID_IMAGE"));
                return;
            }
            fseek(diskfile, 0L, SEEK_END);
            uint32_t fcsize = (uint32_t)(ftell(diskfile) / 512L);
            uint8_t buf[512];
            fseek(diskfile, 0L, SEEK_SET);
            if (fread(buf,sizeof(uint8_t),512,diskfile)<512) {
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
                new fatDrive(paths[i].c_str(), sizes[0], sizes[1],
                             sizes[2], sizes[3], 0, roflag));

            if (newDrive->created_successfully) {
                imgDisks.push_back(
                    static_cast<DOS_Drive *>(newDrive.release()));
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

        auto newdrive = static_cast<fatDrive*>(imgDisks[0]);
        assert(newdrive);
        if (('A' <= drive && drive <= 'B' && !(newdrive->loadedDisk->hardDrive)) ||
            ('C' <= drive && drive <= 'D' && newdrive->loadedDisk->hardDrive)) {
            const size_t idx = drive_index(drive);
            imageDiskList[idx] = newdrive->loadedDisk;
            updateDPT();
        }
    } else if (fstype=="iso") {
        if (Drives[drive_index(drive)]) {
            WriteOut(MSG_Get("PROGRAM_IMGMOUNT_ALREADY_MOUNTED"));
            return;
        }

        MSCDEX_SetCDInterface(CDROM_USE_SDL, -1);
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
        if (wants_ide) {
            if (ide_index >= 0) {
                IDE_CDROM_Attach(ide_index, is_second_cable_slot, drive_index(drive));
            } else {
                WriteOut(MSG_Get("PROGRAM_IMGMOUNT_IDE_CONTROLLERS_UNAVAILABLE"));
            }
        }

        // Print status message (success)
        WriteOut(MSG_Get("MSCDEX_SUCCESS"));
        std::string tmp(paths[0]);
        for (i = 1; i < paths.size(); i++) {
            tmp += "; " + paths[i];
        }
        WriteOut(MSG_Get("PROGRAM_MOUNT_STATUS_2"), drive, tmp.c_str());

    } else if (fstype == "none") {
        FILE *newDisk = fopen_wrap_ro_fallback(temp_line, roflag);
        if (!newDisk) {
            WriteOut(MSG_Get("PROGRAM_IMGMOUNT_INVALID_IMAGE"));
            return;
        }
        fseek(newDisk,0L, SEEK_END);
        uint32_t imagesize = (ftell(newDisk) / 1024);
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

    	if ((drive == '2' || drive == '3') && hdd)
		    updateDPT();

        WriteOut(MSG_Get("PROGRAM_IMGMOUNT_MOUNT_NUMBER"),drive - '0',temp_line.c_str());
    }

    // check if volume label is given. be careful for cdrom
    //if (cmd->FindString("-label",label,true)) newdrive->dirCache.SetLabel(label.c_str());
    return;
}

void IMGMOUNT::AddMessages() {
    AddCommonMountMessages();
	MSG_Add("SHELL_CMD_IMGMOUNT_HELP_LONG",
	        "Mount a CD-ROM, floppy, or disk image to a drive letter.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=green]imgmount[reset] [color=white]DRIVE[reset] [color=cyan]CDROM-SET[reset] [-fs iso] [-ide] -t cdrom|iso\n"
	        "  [color=green]imgmount[reset] [color=white]DRIVE[reset] [color=cyan]IMAGEFILE[reset] [IMAGEFILE2 [..]] [-fs fat] -t hdd|floppy -ro\n"
	        "  [color=green]imgmount[reset] [color=white]DRIVE[reset] [color=cyan]BOOTIMAGE[reset] [-fs fat|none] -t hdd -size GEOMETRY -ro\n"
	        "  [color=green]imgmount[reset] -u [color=white]DRIVE[reset]  (unmounts the [color=white]DRIVE[reset]'s image)\n"
	        "\n"
	        "Where:\n"
	        "  [color=white]DRIVE[reset]     is the drive letter where the image will be mounted: a, c, d, ...\n"
	        "  [color=cyan]CDROM-SET[reset] is an ISO, CUE+BIN, CUE+ISO, or CUE+ISO+FLAC/OPUS/OGG/MP3/WAV\n"
	        "  [color=cyan]IMAGEFILE[reset] is a hard drive or floppy image in FAT16 or FAT12 format\n"
	        "  [color=cyan]BOOTIMAGE[reset] is a bootable disk image with specified -size GEOMETRY:\n"
	        "            bytes-per-sector,sectors-per-head,heads,cylinders\n"
	        "Notes:\n"
	        "  - %s+F4 swaps & mounts the next [color=cyan]CDROM-SET[reset] or [color=cyan]BOOTIMAGE[reset], if provided.\n"
	        "  - The -ro flag mounts the disk image in read-only (write-protected) mode.\n"
	        "  - The -ide flag emulates an IDE controller with attached IDE CD drive, useful\n"
	        "    for CD-based games that need a real DOS environment via bootable HDD image.\n"
	        "Examples:\n"
#if defined(WIN32)
	        "  [color=green]imgmount[reset] [color=white]D[reset] [color=cyan]C:\\games\\doom.iso[reset] -t cdrom\n"
	        "  [color=green]imgmount[reset] [color=white]D[reset] [color=cyan]cd/quake1.cue[reset] -t cdrom\n"
	        "  [color=green]imgmount[reset] [color=white]A[reset] [color=cyan]floppy1.img floppy2.img floppy3.img[reset] -t floppy -ro\n"
	        "  [color=green]imgmount[reset] [color=white]C[reset] [color=cyan]bootable.img[reset] -t hdd -fs none -size 512,63,32,1023\n"
#elif defined(MACOSX)
	        "  [color=green]imgmount[reset] [color=white]D[reset] [color=cyan]/Users/USERNAME/games/doom.iso[reset] -t cdrom\n"
	        "  [color=green]imgmount[reset] [color=white]D[reset] [color=cyan]cd/quake1.cue[reset] -t cdrom\n"
	        "  [color=green]imgmount[reset] [color=white]A[reset] [color=cyan]floppy1.img floppy2.img floppy3.img[reset] -t floppy -ro\n"
	        "  [color=green]imgmount[reset] [color=white]C[reset] [color=cyan]bootable.img[reset] -t hdd -fs none -size 512,63,32,1023\n"
#else
	        "  [color=green]imgmount[reset] [color=white]D[reset] [color=cyan]/home/USERNAME/games/doom.iso[reset] -t cdrom\n"
	        "  [color=green]imgmount[reset] [color=white]D[reset] [color=cyan]cd/quake1.cue[reset] -t cdrom\n"
	        "  [color=green]imgmount[reset] [color=white]A[reset] [color=cyan]floppy1.img floppy2.img floppy3.img[reset] -t floppy -ro\n"
	        "  [color=green]imgmount[reset] [color=white]C[reset] [color=cyan]bootable.img[reset] -t hdd -fs none -size 512,63,32,1023\n"
#endif
	);
    
    MSG_Add("PROGRAM_IMGMOUNT_STATUS_1",
	        "The currently mounted disk and CD image drives are:\n");
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
	MSG_Add("PROGRAM_IMGMOUNT_STATUS_NONE",
		"No drive available\n");
	MSG_Add("PROGRAM_IMGMOUNT_IDE_CONTROLLERS_UNAVAILABLE",
		"No available IDE controllers. Drive will not have IDE emulation.\n");
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
}
