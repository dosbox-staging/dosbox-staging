// SPDX-FileCopyrightText:  2021-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "boot.h"

#include <cstdio>
#include <limits>

#include "config/config.h"
#include "cpu/callback.h"
#include "cpu/registers.h"
#include "dos/dos_windows.h"
#include "dos/drives.h"
#include "gui/mapper.h"
#include "gui/titlebar.h"
#include "hardware/dma.h"
#include "hardware/input/mouse.h"
#include "hardware/virtualbox.h"
#include "hardware/vmware.h"
#include "ints/bios_disk.h"
#include "misc/video.h"
#include "more_output.h"
#include "utils/fs_utils.h"
#include "utils/string_utils.h"

FILE* BOOT::getFSFile_mounted(const char* filename, uint32_t* ksize,
                              uint32_t* bsize, uint8_t* error)
{
	// if return NULL then put in error the errormessage code if an error
	// was requested
	bool tryload = (*error) ? true : false;
	*error = 0;
	uint8_t drive;
	FILE *tmpfile;
	char fullname[DOS_PATHLENGTH];

	if (!DOS_MakeName(filename, fullname, &drive))
		return nullptr;

	try {
		const auto ldp = std::dynamic_pointer_cast<localDrive>(
		        Drives.at(drive));
		if (!ldp) {
			return nullptr;
		}

		tmpfile = ldp->GetHostFilePtr(fullname, "rb");
		if (tmpfile == nullptr) {
			if (!tryload)
				*error = 1;
			return nullptr;
		}

		// get file size
		if (!check_fseek("BOOT", "image", filename, tmpfile, 0L, SEEK_END)) {
			return nullptr;
		}

		*ksize = (ftell(tmpfile) / 1024);
		*bsize = ftell(tmpfile);
		fclose(tmpfile);

		tmpfile = ldp->GetHostFilePtr(fullname, "rb+");
		if (tmpfile == nullptr) {
			//				if (!tryload) *error=2;
			//				return NULL;
			WriteOut(MSG_Get("PROGRAM_BOOT_WRITE_PROTECTED"));
			tmpfile = ldp->GetHostFilePtr(fullname, "rb");
			if (tmpfile == nullptr) {
				if (!tryload)
					*error = 1;
				return nullptr;
			}
		}

		return tmpfile;
	} catch (...) {
		return nullptr;
	}
}

FILE* BOOT::getFSFile(const char* filename, uint32_t* ksize, uint32_t* bsize,
                      bool tryload)
{
	uint8_t error = tryload ? 1 : 0;
	FILE *tmpfile = getFSFile_mounted(filename, ksize, bsize, &error);
	if (tmpfile)
		return tmpfile;
	// File not found on mounted filesystem. Try regular filesystem
	const auto filename_s = resolve_home(filename).string();
	tmpfile = fopen(filename_s.c_str(), "rb+");

	// Used for logging in check_fseek()
	constexpr auto ModuleName = "BOOT";
	constexpr auto FileDescription = "image";

	if (!tmpfile) {
		if ((tmpfile = fopen(filename_s.c_str(), "rb"))) {
			// File exists; So can't be opened in correct mode =>
			// error 2
			//				fclose(tmpfile);
			//				if (tryload) error = 2;
			WriteOut(MSG_Get("PROGRAM_BOOT_WRITE_PROTECTED"));
			if (!check_fseek(ModuleName, FileDescription, filename, tmpfile, 0L, SEEK_END)) {
				return nullptr;
			}
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
		return nullptr;
	}
	if (!check_fseek(ModuleName, FileDescription, filename, tmpfile, 0L, SEEK_END)) {
		return nullptr;
	}
	*ksize = (ftell(tmpfile) / 1024);
	*bsize = ftell(tmpfile);
	return tmpfile;
}

void BOOT::printError(void)
{
	WriteOut(MSG_Get("PROGRAM_BOOT_PRINT_ERROR"), PRIMARY_MOD_NAME);
}

void BOOT::DisableUmbXmsEms(void)
{
	set_section_property_value("dos", "umb", "false");
	set_section_property_value("dos", "xms", "false");
	set_section_property_value("dos", "ems", "false");

	DOS_NotifySettingUpdated("umb");
	DOS_NotifySettingUpdated("xms");
	DOS_NotifySettingUpdated("ems");
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

	FILE *usefile_1 = nullptr;
	FILE *usefile_2 = nullptr;
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
		MoreOutputStrings output(*this);
		output.AddString(MSG_Get("PROGRAM_BOOT_HELP_LONG"));
		output.Display();
		return;
	}

	// Booting would have terminated the running Windows, don't do it
	if (WINDOWS_IsStarted()) {
		WriteOut(MSG_Get("SHELL_CANT_RUN_UNDER_WINDOWS"));
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

			if (imageDiskList[0] || imageDiskList[1]) {
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
			if (usefile != nullptr) {
				diskSwap[i] = std::make_shared<imageDisk>(
				        usefile, temp_line.c_str(), floppysize, false);
				if (usefile_1 == nullptr) {
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

	if (!imageDiskList.at(drive_index(drive))) {
		WriteOut(MSG_Get("PROGRAM_BOOT_UNABLE"), drive);
		return;
	}

	bootSector bootarea;
	imageDiskList.at(drive_index(drive))
	        ->Read_Sector(0, 0, 1, reinterpret_cast<uint8_t*>(&bootarea));
	if ((bootarea.rawdata[0] == 0x50) && (bootarea.rawdata[1] == 0x43) &&
	    (bootarea.rawdata[2] == 0x6a) && (bootarea.rawdata[3] == 0x72)) {
		if (!is_machine_pcjr()) {
			WriteOut(MSG_Get("PROGRAM_BOOT_CART_WO_PCJR"));
		} else {
			uint8_t rombuf[65536];
			Bits cfound_at = -1;

			// Used for logging in check_fseek()
			constexpr auto ModuleName = "BOOT";
			constexpr auto FileDescription = "cartridge";

			if (!cart_cmd.empty()) {
				if (!usefile_1) {
					WriteOut(MSG_Get("PROGRAM_BOOT_IMAGE_NOT_OPEN"), temp_line.c_str());
					return;
				}
				/* read cartridge data into buffer */
				constexpr auto seek_pos = 0x200;
				if (!check_fseek(ModuleName, FileDescription, temp_line.c_str(), usefile_1, seek_pos, SEEK_SET)) {
					return;
				}
				const auto rom_bytes_expected = rombytesize_1 - 0x200;
				const auto rom_bytes_read = fread(
				        rombuf, 1, rom_bytes_expected, usefile_1);
				if (rom_bytes_read < rom_bytes_expected) {
					LOG_ERR("BOOT: Failed to read sufficient cartridge data");
					fclose(usefile_1);
					return;
				}
				// Null-terminate the buffer
				rombuf[rom_bytes_read] = '\0';

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
					diskSwap.fill(nullptr);

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
						diskSwap.fill(nullptr);
						return;
					}
				}
			}

			DisableUmbXmsEms();
			MEM_PreparePCJRCartRom();

			if (usefile_1 == nullptr)
				return;

			uint32_t sz1, sz2;
			constexpr auto rom_filename = "system.rom";
			FILE* tfile = getFSFile(rom_filename, &sz1, &sz2, true);
			if (tfile != nullptr) {
				if (!check_fseek("BOOT", "system ROM", rom_filename, tfile, 0x3000L, SEEK_SET)) {
					return;
				}
				auto drd = (uint32_t)fread(rombuf, 1, 0xb000, tfile);
				if (drd == 0xb000) {
					for (i = 0; i < 0xb000; i++)
						phys_writeb(0xf3000 + i, rombuf[i]);
				}
				fclose(tfile);
			}

			if (usefile_2 != nullptr) {
				if (!check_fseek(ModuleName, FileDescription, temp_line.c_str(), usefile_2, 0x0L, SEEK_SET)) {
					return;
				}
				if (fread(rombuf, 1, 0x200, usefile_2) < 0x200) {
					LOG_MSG("Failed to read sufficient ROM data");
					fclose(usefile_2);
					return;
				}

				PhysPt romseg_pt = host_readw(&rombuf[0x1ce]) << 4;

				/* read cartridge data into buffer */
				if (!check_fseek(ModuleName, FileDescription, temp_line.c_str(), usefile_2, 0x200L, SEEK_SET)) {
					return;
				}
				if (fread(rombuf, 1, rombytesize_2 - 0x200, usefile_2) <
				    rombytesize_2 - 0x200) {
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

			if (!check_fseek(ModuleName, FileDescription, temp_line.c_str(), usefile_1, 0x0L, SEEK_SET)) {
				return;
			}
			if (fread(rombuf, 1, 0x200, usefile_1) < 0x200) {
				LOG_MSG("Failed to read sufficient cartridge data");
				fclose(usefile_1);
				return;
			}

			uint16_t romseg = host_readw(&rombuf[0x1ce]);

			/* read cartridge data into buffer */
			if (!check_fseek(ModuleName, FileDescription, temp_line.c_str(), usefile_1, 0x200L, SEEK_SET)) {
				return;
			}
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
			diskSwap.fill(nullptr);

			NotifyBooting();

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
					SegSet16(cs, RealSegment(new_int18));
					reg_ip = RealOffset(new_int18);
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
		DisableUmbXmsEms();
		MEM_RemoveEMSPageFrame();

		NotifyBooting();
		WriteOut(MSG_Get("PROGRAM_BOOT_BOOT"), drive);

		for (i = 0; i < 512; i++)
			real_writeb(0, 0x7c00 + i, bootarea.rawdata[i]);

		/* create appearance of floppy drive DMA usage (Demon's Forge) */
		if (!is_machine_pcjr_or_tandy() && floppysize != 0)
			DMA_GetChannel(2)->has_reached_terminal_count = true;

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

void BOOT::NotifyBooting()
{
	DOS_NotifyBooting();
	TITLEBAR_NotifyBooting();
	MOUSE_NotifyBooting();
	VIRTUALBOX_NotifyBooting();
	VMWARE_NotifyBooting();
	WINDOWS_NotifyBooting();
}

void BOOT::AddMessages()
{
	MSG_Add("PROGRAM_BOOT_HELP_LONG",
	        "Boot DOSBox Staging from a DOS drive or disk image.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=light-green]boot[reset] [color=white]DRIVE[reset]\n"
	        "  [color=light-green]boot[reset] [color=light-cyan]IMAGEFILE[reset]\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=white]DRIVE[reset]      drive to boot from, must be [color=white]A:[reset], [color=white]C:[reset], or [color=white]D:[reset]\n"
	        "  [color=light-cyan]IMAGEFILE[reset]  one or more floppy images, separated by spaces\n"
	        "\n"
	        "Notes:\n"
	        "  A DOS drive letter must have been mounted previously with [color=light-green]imgmount[reset] command.\n"
	        "  The DOS drive or disk image must be bootable, containing DOS system files.\n"
	        "  If more than one disk images are specified, you can swap them with a hotkey.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]boot[reset] [color=white]c:[reset]\n"
	        "  [color=light-green]boot[reset] [color=light-cyan]disk1.ima disk2.ima[reset]\n");
	MSG_Add("PROGRAM_BOOT_NOT_EXIST","Bootdisk file does not exist. Failing.\n");
	MSG_Add("PROGRAM_BOOT_NOT_OPEN","Cannot open bootdisk file. Failing.\n");
	MSG_Add("PROGRAM_BOOT_WRITE_PROTECTED","Image file is read-only! Might create problems.\n");
	MSG_Add("PROGRAM_BOOT_PRINT_ERROR",
	        "This command boots DOSBox Staging from either a floppy or hard disk image.\n\n"
	        "For this command, one can specify a succession of floppy disks swappable by\n"
	        "pressing [color=yellow]%s+F4[reset], and -l specifies the mounted drive to boot from. If no drive\n"
	        "letter is specified, this defaults to booting from the A drive. The only\n"
	        "bootable drive letters are A, C, and D. For booting from a hard drive (C or D),\n"
	        "the image should have already been mounted using the [color=light-blue]IMGMOUNT[reset] command.\n\n"
	        "Type [color=light-blue]BOOT /?[reset] for the syntax of this command.\n");
	MSG_Add("PROGRAM_BOOT_UNABLE","Unable to boot off of drive %c.\n");
	MSG_Add("PROGRAM_BOOT_IMAGE_OPEN","Opening image file: %s\n");
	MSG_Add("PROGRAM_BOOT_IMAGE_MOUNTED","Floppy image(s) already mounted.\n");
	MSG_Add("PROGRAM_BOOT_IMAGE_NOT_OPEN","Cannot open %s\n");
	MSG_Add("PROGRAM_BOOT_BOOT","Booting from drive %c...\n");
	MSG_Add("PROGRAM_BOOT_CART_WO_PCJR","PCjr cartridge found, but machine is not PCjr.\n");
	MSG_Add("PROGRAM_BOOT_CART_LIST_CMDS", "Available PCjr cartridge commands: %s\n");
	MSG_Add("PROGRAM_BOOT_CART_NO_CMDS", "No PCjr cartridge commands found.\n");
}
