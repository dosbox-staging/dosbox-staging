// SPDX-FileCopyrightText:  2021-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "loadrom.h"

#include "dosbox.h"

#include <cstdio>

#include "cpu/callback.h"
#include "dos/dos_windows.h"
#include "dos/drives.h"
#include "more_output.h"
#include "cpu/registers.h"

void LOADROM::Run(void)
{
	if (!(cmd->FindCommand(1, temp_line))) {
		WriteOut(MSG_Get("PROGRAM_LOADROM_SPECIFY_FILE"));
		return;
	}
	if (HelpRequested()) {
		MoreOutputStrings output(*this);
		output.AddString(MSG_Get("PROGRAM_LOADROM_HELP_LONG"));
		output.Display();
		return;
	}

	// Loading the ROM when Windows is running is asking for trouble
	if (WINDOWS_IsStarted()) {
		WriteOut(MSG_Get("SHELL_CANT_RUN_UNDER_WINDOWS"));
		return;
	}

	uint8_t drive;
	char fullname[DOS_PATHLENGTH];
	if (!DOS_MakeName(temp_line.c_str(), fullname, &drive)) {
		return;
	}

	try {
		/* try to read ROM file into buffer */
		if (!Drives.at(drive)) {
			return;
		}

		FILE* tmpfile = Drives.at(drive)->GetHostFilePtr(fullname, "rb");
		if (tmpfile == nullptr) {
			WriteOut(MSG_Get("PROGRAM_LOADROM_CANT_OPEN"));
			return;
		}
		fseek(tmpfile, 0L, SEEK_END);
		if (ftell(tmpfile) > 0x8000) {
			WriteOut(MSG_Get("PROGRAM_LOADROM_TOO_LARGE"));
			fclose(tmpfile);
			return;
		}
		fseek(tmpfile, 0L, SEEK_SET);
		uint8_t rom_buffer[0x8000];
		Bitu data_read = fread(rom_buffer, 1, 0x8000, tmpfile);
		fclose(tmpfile);

		/* try to identify ROM type */
		PhysPt rom_base = 0;
		if (data_read >= 0x4000 && rom_buffer[0] == 0x55 &&
		    rom_buffer[1] == 0xaa && (rom_buffer[3] & 0xfc) == 0xe8 &&
		    strncmp((char*)(&rom_buffer[0x1e]), "IBM", 3) == 0) {
			if (!is_machine_ega_or_better()) {
				WriteOut(MSG_Get("PROGRAM_LOADROM_INCOMPATIBLE"));
				return;
			}
			rom_base = PhysicalMake(0xc000, 0); // video BIOS
		} else if (data_read == 0x8000 && rom_buffer[0] == 0xe9 &&
		           rom_buffer[1] == 0x8f && rom_buffer[2] == 0x7e &&
		           strncmp((char*)(&rom_buffer[0x4cd4]), "IBM", 3) == 0) {
			rom_base = PhysicalMake(0xf600, 0); // BASIC
		}

		if (rom_base) {
			/* write buffer into ROM */
			for (Bitu i = 0; i < data_read; i++) {
				phys_writeb(rom_base + i, rom_buffer[i]);
			}

			if (rom_base == 0xc0000) {
				/* initialize video BIOS */
				phys_writeb(PhysicalMake(0xf000, 0xf065), 0xcf);
				reg_flags &= ~FLAG_IF;
				CALLBACK_RunRealFar(0xc000, 0x0003);
				LOG_MSG("Video BIOS ROM loaded and initialized.");
			} else {
				WriteOut(MSG_Get("PROGRAM_LOADROM_BASIC_LOADED"));
			}
		} else {
			WriteOut(MSG_Get("PROGRAM_LOADROM_UNRECOGNIZED"));
		}
	} catch (...) {
		return;
	}
}

void LOADROM::AddMessages()
{
	MSG_Add("PROGRAM_LOADROM_HELP_LONG",
	        "Load a ROM image of the video BIOS or IBM BASIC.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=light-green]loadrom [color=light-cyan]IMAGEFILE[reset]\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=light-cyan]IMAGEFILE[reset]  video BIOS or IBM BASIC ROM image\n"
	        "\n"
	        "Notes:\n"
	        "  After loading an IBM BASIC ROM image into the emulated ROM with the command,\n"
	        "  you can run the original IBM BASIC interpreter program in DOSBox Staging.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]loadrom[reset] [color=light-cyan]bios.rom[reset]\n");
	MSG_Add("PROGRAM_LOADROM_SPECIFY_FILE", "Must specify ROM file to load.\n");
	MSG_Add("PROGRAM_LOADROM_CANT_OPEN", "ROM file not accessible.\n");
	MSG_Add("PROGRAM_LOADROM_TOO_LARGE", "ROM file too large.\n");
	MSG_Add("PROGRAM_LOADROM_INCOMPATIBLE",
	        "Video BIOS not supported by machine type.\n");
	MSG_Add("PROGRAM_LOADROM_UNRECOGNIZED", "ROM file not recognized.\n");
	MSG_Add("PROGRAM_LOADROM_BASIC_LOADED", "BASIC ROM loaded.\n");
}
