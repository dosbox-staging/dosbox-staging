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

#include "program_boot.h"

#include <limits>
#include <stdio.h>

#include "bios_disk.h"
#include "callback.h"
#include "control.h"
#include "cross.h"
#include "dma.h"
#include "drives.h"
#include "mapper.h"
#include "regs.h"
#include "string_utils.h"

FILE *BOOT::getFSFile_mounted(char const *filename, uint32_t *ksize, uint32_t *bsize, uint8_t *error)
{
	// if return NULL then put in error the errormessage code if an error
	// was requested
	bool tryload = (*error) ? true : false;
	*error = 0;
	uint8_t drive;
	FILE *tmpfile;
	char fullname[DOS_PATHLENGTH];

	localDrive *ldp = 0;
	if (!DOS_MakeName(const_cast<char *>(filename), fullname, &drive))
		return NULL;

	try {
		ldp = dynamic_cast<localDrive *>(Drives[drive]);
		if (!ldp)
			return NULL;

		tmpfile = ldp->GetSystemFilePtr(fullname, "rb");
		if (tmpfile == NULL) {
			if (!tryload)
				*error = 1;
			return NULL;
		}

		// get file size
		fseek(tmpfile, 0L, SEEK_END);
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
				if (!tryload)
					*error = 1;
				return NULL;
			}
		}

		return tmpfile;
	} catch (...) {
		return NULL;
	}
}

FILE *BOOT::getFSFile(char const *filename, uint32_t *ksize, uint32_t *bsize, bool tryload)
{
	uint8_t error = tryload ? 1 : 0;
	FILE *tmpfile = getFSFile_mounted(filename, ksize, bsize, &error);
	if (tmpfile)
		return tmpfile;
	// File not found on mounted filesystem. Try regular filesystem
	std::string filename_s(filename);
	Cross::ResolveHomedir(filename_s);
	tmpfile = fopen_wrap(filename_s.c_str(), "rb+");
	if (!tmpfile) {
		if ((tmpfile = fopen_wrap(filename_s.c_str(), "rb"))) {
			// File exists; So can't be opened in correct mode =>
			// error 2
			//				fclose(tmpfile);
			//				if (tryload) error = 2;
			WriteOut(MSG_Get("PROGRAM_BOOT_WRITE_PROTECTED"));
			fseek(tmpfile, 0L, SEEK_END);
			*ksize = (ftell(tmpfile) / 1024);
			*bsize = ftell(tmpfile);
			return tmpfile;
		}
		// Give the delayed errormessages from the mounted variant (or
		// from above)
		if (error == 1)
			WriteOut(MSG_Get("PROGRAM_BOOT_NOT_EXIST"));
		if (error == 2)
			WriteOut(MSG_Get("PROGRAM_BOOT_NOT_OPEN"));
		return NULL;
	}
	fseek(tmpfile, 0L, SEEK_END);
	*ksize = (ftell(tmpfile) / 1024);
	*bsize = ftell(tmpfile);
	return tmpfile;
}

void BOOT::printError(void)
{
	WriteOut(MSG_Get("PROGRAM_BOOT_PRINT_ERROR"), PRIMARY_MOD_NAME);
}

void BOOT::disable_umb_ems_xms(void)
{
	Section *dos_sec = control->GetSection("dos");
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

void BOOT::Run(void)
{
	// Hack To allow long commandlines
	ChangeToLongCmd();
	/* In secure mode don't allow people to boot stuff.
	 * They might try to corrupt the data on it */
	if (control->SecureMode()) {
		WriteOut(MSG_Get("PROGRAM_CONFIG_SECURE_DISALLOW"));
		return;
	}

	FILE *usefile_1 = NULL;
	FILE *usefile_2 = NULL;
	Bitu i = 0;
	uint32_t floppysize = 0;
	uint32_t rombytesize_1 = 0;
	uint32_t rombytesize_2 = 0;
	char drive = 'A';
	std::string cart_cmd = "";

	if (!cmd->GetCount()) {
		printError();
		return;
	}

	if (HelpRequested()) {
		WriteOut(MSG_Get("SHELL_CMD_BOOT_HELP_LONG"));
		return;
	}
	if (cmd->GetCount() == 1) {
		if (cmd->FindCommand(1, temp_line)) {
			if (temp_line.length() == 2 && toupper(temp_line[0]) >= 'A' &&
			    toupper(temp_line[0]) <= 'Z' && temp_line[1] == ':') {
				drive = toupper(temp_line[0]);
				if ((drive != 'A') && (drive != 'C') &&
				    (drive != 'D')) {
					printError();
					return;
				}
				i++;
			}
		}
	}
	while (i < cmd->GetCount()) {
		if (cmd->FindCommand(i + 1, temp_line)) {
			if ((temp_line == "-l") || (temp_line == "-L")) {
				/* Specifying drive... next argument then is the
				 * drive */
				i++;
				if (cmd->FindCommand(i + 1, temp_line)) {
					drive = toupper(temp_line[0]);
					if ((drive != 'A') && (drive != 'C') &&
					    (drive != 'D')) {
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
					for (size_t ct = 0;
					     ct < temp_line.size(); ct++)
						temp_line[ct] = toupper(
						        temp_line[ct]);
					cart_cmd = temp_line;
				} else {
					printError();
					return;
				}
				i++;
				continue;
			}


			if (imageDiskList[0] != nullptr || imageDiskList[1] != nullptr) {
				WriteOut(MSG_Get("PROGRAM_BOOT_IMAGE_MOUNTED"));
				return;
			}

			if (i >= MAX_SWAPPABLE_DISKS) {
				return; // TODO give a warning.
			}
			WriteOut(MSG_Get("PROGRAM_BOOT_IMAGE_OPEN"),
			         temp_line.c_str());
			uint32_t rombytesize;
			FILE *usefile = getFSFile(temp_line.c_str(),
			                          &floppysize, &rombytesize);
			if (usefile != NULL) {
				diskSwap[i].reset(
				        new imageDisk(usefile, temp_line.c_str(),
				                      floppysize, false));
				if (usefile_1 == NULL) {
					usefile_1 = usefile;
					rombytesize_1 = rombytesize;
				} else {
					usefile_2 = usefile;
					rombytesize_2 = rombytesize;
				}
			} else {
				WriteOut(MSG_Get("PROGRAM_BOOT_IMAGE_NOT_OPEN"),
				         temp_line.c_str());
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
	imageDiskList[drive_index(drive)]->Read_Sector(
	        0, 0, 1, reinterpret_cast<uint8_t *>(&bootarea));
	if ((bootarea.rawdata[0] == 0x50) && (bootarea.rawdata[1] == 0x43) &&
	    (bootarea.rawdata[2] == 0x6a) && (bootarea.rawdata[3] == 0x72)) {
		if (machine != MCH_PCJR) {
			WriteOut(MSG_Get("PROGRAM_BOOT_CART_WO_PCJR"));
		} else {
			uint8_t rombuf[65536];
			Bits cfound_at = -1;
			if (!cart_cmd.empty()) {
				if (!usefile_1) {
					WriteOut(MSG_Get("PROGRAM_BOOT_IMAGE_NOT_OPEN"), temp_line.c_str());
					return;
				}
				/* read cartridge data into buffer */
				constexpr auto seek_pos = 0x200;
				if (fseek(usefile_1, seek_pos, SEEK_SET) != 0) {
					LOG_ERR("BOOT: Failed seeking to %d in cartridge data file '%s': %s",
					        seek_pos, temp_line.c_str(), strerror(errno));
					return;
				}
				if (fread(rombuf, 1, rombytesize_1 - 0x200,
				          usefile_1) < rombytesize_1 - 0x200) {
					LOG_ERR("BOOT: Failed to read sufficient cartridge data");
					fclose(usefile_1);
					return;
				}
				char cmdlist[1024];
				cmdlist[0] = 0;
				Bitu ct = 6;

				auto clen = rombuf[ct];
				static constexpr auto max_clen =
				        std::numeric_limits<decltype(clen)>::max();
				std::array<char, max_clen + 1> buf = {0};

				if (cart_cmd == "?") {
					while (clen != 0) {
						strncpy(buf.data(),
						        (char *)&rombuf[ct + 1],
						        clen);
						buf[clen] = 0;
						upcase(buf.data());
						safe_strcat(cmdlist, " ");
						safe_strcat(cmdlist, buf.data());
						ct += 1 + clen + 3;
						if (ct > sizeof(cmdlist))
							break;
						clen = rombuf[ct];
					}
					if (ct > 6) {
						WriteOut(MSG_Get("PROGRAM_BOOT_CART_LIST_CMDS"),
						         cmdlist);
					} else {
						WriteOut(MSG_Get(
						        "PROGRAM_BOOT_CART_NO_CMDS"));
					}
					for (auto &disk : diskSwap)
						disk.reset();
					// fclose(usefile_1); //delete diskSwap
					// closes the file
					return;
				} else {
					while (clen != 0) {
						strncpy(buf.data(),
						        (char *)&rombuf[ct + 1],
						        clen);
						buf[clen] = 0;
						upcase(buf.data());
						safe_strcat(cmdlist, " ");
						safe_strcat(cmdlist, buf.data());
						ct += 1 + clen;

						if (cart_cmd == buf.data()) {
							cfound_at = ct;
							break;
						}

						ct += 3;
						if (ct > sizeof(cmdlist))
							break;
						clen = rombuf[ct];
					}
					if (cfound_at <= 0) {
						if (ct > 6) {
							WriteOut(MSG_Get("PROGRAM_BOOT_CART_LIST_CMDS"),
							         cmdlist);
						} else {
							WriteOut(MSG_Get(
							        "PROGRAM_BOOT_CART_NO_CMDS"));
						}
						for (auto &disk : diskSwap)
							disk.reset();
						// fclose(usefile_1); //Delete
						// diskSwap closes the file
						return;
					}
				}
			}

			disable_umb_ems_xms();
			MEM_PreparePCJRCartRom();

			if (usefile_1 == NULL)
				return;

			uint32_t sz1, sz2;
			FILE *tfile = getFSFile("system.rom", &sz1, &sz2, true);
			if (tfile != NULL) {
				fseek(tfile, 0x3000L, SEEK_SET);
				uint32_t drd = (uint32_t)fread(rombuf, 1, 0xb000, tfile);
				if (drd == 0xb000) {
					for (i = 0; i < 0xb000; i++)
						phys_writeb(0xf3000 + i, rombuf[i]);
				}
				fclose(tfile);
			}

			if (usefile_2 != NULL) {
				fseek(usefile_2, 0x0L, SEEK_SET);
				if (fread(rombuf, 1, 0x200, usefile_2) < 0x200) {
					LOG_MSG("Failed to read sufficient ROM data");
					fclose(usefile_2);
					return;
				}

				PhysPt romseg_pt = host_readw(&rombuf[0x1ce]) << 4;

				/* read cartridge data into buffer */
				fseek(usefile_2, 0x200L, SEEK_SET);
				if (fread(rombuf, 1, rombytesize_2 - 0x200,
				          usefile_2) < rombytesize_2 - 0x200) {
					LOG_MSG("Failed to read sufficient ROM data");
					fclose(usefile_2);
					return;
				}

				// fclose(usefile_2); //usefile_2 is in diskSwap
				// structure which should be deleted to close
				// the file

				/* write cartridge data into ROM */
				for (i = 0; i < rombytesize_2 - 0x200; i++)
					phys_writeb(romseg_pt + i, rombuf[i]);
			}

			fseek(usefile_1, 0x0L, SEEK_SET);
			if (fread(rombuf, 1, 0x200, usefile_1) < 0x200) {
				LOG_MSG("Failed to read sufficient cartridge data");
				fclose(usefile_1);
				return;
			}

			uint16_t romseg = host_readw(&rombuf[0x1ce]);

			/* read cartridge data into buffer */
			fseek(usefile_1, 0x200L, SEEK_SET);
			if (fread(rombuf, 1, rombytesize_1 - 0x200, usefile_1) <
			    rombytesize_1 - 0x200) {
				LOG_MSG("Failed to read sufficient cartridge data");
				fclose(usefile_1);
				return;
			}
			// fclose(usefile_1); //usefile_1 is in diskSwap
			// structure which should be deleted to close the file

			/* write cartridge data into ROM */
			for (i = 0; i < rombytesize_1 - 0x200; i++)
				phys_writeb((romseg << 4) + i, rombuf[i]);

			// Close cardridges
			for (auto &disk : diskSwap)
				disk.reset();

			if (cart_cmd.empty()) {
				uint32_t old_int18 = mem_readd(0x60);
				/* run cartridge setup */
				SegSet16(ds, romseg);
				SegSet16(es, romseg);
				SegSet16(ss, 0x8000);
				reg_esp = 0xfffe;
				CALLBACK_RunRealFar(romseg, 0x0003);

				uint32_t new_int18 = mem_readd(0x60);
				if (old_int18 != new_int18) {
					/* boot cartridge (int18) */
					SegSet16(cs, RealSeg(new_int18));
					reg_ip = RealOff(new_int18);
				}
			} else {
				if (cfound_at > 0) {
					/* run cartridge setup */
					SegSet16(ds, dos.psp());
					SegSet16(es, dos.psp());
					CALLBACK_RunRealFar(romseg, cfound_at);
				}
			}
		}
	} else {
		disable_umb_ems_xms();
		MEM_RemoveEMSPageFrame();
		WriteOut(MSG_Get("PROGRAM_BOOT_BOOT"), drive);
		for (i = 0; i < 512; i++)
			real_writeb(0, 0x7c00 + i, bootarea.rawdata[i]);

		/* create appearance of floppy drive DMA usage (Demon's Forge) */
		if (!IS_TANDY_ARCH && floppysize != 0)
			GetDMAChannel(2)->tcount = true;

		/* revector some dos-allocated interrupts */
		real_writed(0, 0x01 * 4, 0xf000ff53);
		real_writed(0, 0x03 * 4, 0xf000ff53);

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
		reg_edx = 0;      // Head 0 drive 0
		reg_ebx = 0x7c00; // Real code probably uses bx to load the image
	}
}

void BOOT::AddMessages() {
	MSG_Add("SHELL_CMD_BOOT_HELP_LONG",
	        "Boots DOSBox Staging from a DOS drive or disk image.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=green]boot[reset] [color=white]DRIVE[reset]\n"
	        "  [color=green]boot[reset] [color=cyan]IMAGEFILE[reset]\n"
	        "\n"
	        "Where:\n"
	        "  [color=white]DRIVE[reset] is a drive to boot from, must be [color=white]A:[reset], [color=white]C:[reset], or [color=white]D:[reset].\n"
	        "  [color=cyan]IMAGEFILE[reset] is one or more floppy images, separated by spaces.\n"
	        "\n"
	        "Notes:\n"
	        "  A DOS drive letter must have been mounted previously with [color=green]imgmount[reset] command.\n"
	        "  The DOS drive or disk image must be bootable, containing DOS system files.\n"
	        "  If more than one disk images are specified, you can swap them with a hotkey.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=green]boot[reset] [color=white]c:[reset]\n"
	        "  [color=green]boot[reset] [color=cyan]disk1.ima disk2.ima[reset]\n");
	MSG_Add("PROGRAM_BOOT_NOT_EXIST","Bootdisk file does not exist.  Failing.\n");
	MSG_Add("PROGRAM_BOOT_NOT_OPEN","Cannot open bootdisk file.  Failing.\n");
	MSG_Add("PROGRAM_BOOT_WRITE_PROTECTED","Image file is read-only! Might create problems.\n");
	MSG_Add("PROGRAM_BOOT_PRINT_ERROR",
	        "This command boots DOSBox Staging from either a floppy or hard disk image.\n\n"
	        "For this command, one can specify a succession of floppy disks swappable\n"
	        "by pressing %s+F4, and -l specifies the mounted drive to boot from.  If\n"
	        "no drive letter is specified, this defaults to booting from the A drive.\n"
	        "The only bootable drive letters are A, C, and D.  For booting from a hard\n"
	        "drive (C or D), the image should have already been mounted using the\n"
	        "\033[34;1mIMGMOUNT\033[0m command.\n\n"
	        "Type \033[34;1mBOOT /?\033[0m for the syntax of this command.\033[0m\n");
	MSG_Add("PROGRAM_BOOT_UNABLE","Unable to boot off of drive %c");
	MSG_Add("PROGRAM_BOOT_IMAGE_OPEN","Opening image file: %s\n");
	MSG_Add("PROGRAM_BOOT_IMAGE_MOUNTED","Floppy image(s) already mounted.\n");
	MSG_Add("PROGRAM_BOOT_IMAGE_NOT_OPEN","Cannot open %s");
	MSG_Add("PROGRAM_BOOT_BOOT","Booting from drive %c...\n");
	MSG_Add("PROGRAM_BOOT_CART_WO_PCJR","PCjr cartridge found, but machine is not PCjr");
	MSG_Add("PROGRAM_BOOT_CART_LIST_CMDS", "Available PCjr cartridge commands: %s");
	MSG_Add("PROGRAM_BOOT_CART_NO_CMDS", "No PCjr cartridge commands found");
}
