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
#include "string_utils.h"
#include "../ints/int10.h"

void IMGMOUNT::Run(void) {
    //Hack To allow long commandlines
    ChangeToLongCmd();

    // Usage
    if (!cmd->GetCount() || cmd->FindExist("/?", false) ||
        cmd->FindExist("-h", false) || cmd->FindExist("--help", false)) {
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
    std::string ideattach = "auto";

    std::string str_size = "";
    Bit8u mediaid = 0xF8;

    /* DOSBox-X: we allow "-ide" to allow controlling which IDE controller and
     * slot to attach the hard disk/CD-ROM to */
    cmd->FindString("-ide", ideattach, true);

    if (ideattach == "auto") {
	    if (type != "floppy")
		    IDE_Auto(ide_index, ide_slave);
    } else if (ideattach != "none" && isdigit(ideattach[0]) &&
	       ideattach[0] > '0') { /* takes the form [controller]<m/s> such
		                        as: 1m for primary master */
	    ide_index = ideattach[0] - '1';
	    if (ideattach.length() >= 1)
		    ide_slave = (ideattach[1] == 's');
	    LOG_MSG("IDE: index %d slave=%d", ide_index, ide_slave ? 1 : 0);
    }

    if (type == "floppy") {
	    mediaid = 0xF0;
    } else if (type == "iso") {
	    // str_size="2048,1,65535,0";	// ignored, see drive_iso.cpp
	    // (AllocationInfo)
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
	if (ide_index >= 0)
		IDE_CDROM_Attach(ide_index, ide_slave, drive - 'A');

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

void IMGMOUNT_ProgramStart(Program **make) {
	*make=new IMGMOUNT;
}
