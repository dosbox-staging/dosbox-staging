// SPDX-FileCopyrightText:  2021-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "biostest.h"

#if C_DEBUGGER

#include <cstdio>
#include <cstring>

#include <string>

#include "dos/drives.h"
#include "cpu/registers.h"

void BIOSTEST::Run(void) {
	if (!(cmd->FindCommand(1, temp_line))) {
		WriteOut("Must specify BIOS file to load.\n");
		return;
	}

	uint8_t drive;
	char fullname[DOS_PATHLENGTH];
	if (!DOS_MakeName((char *)temp_line.c_str(), fullname, &drive))
		return;

	try {
		// try to read ROM file into buffer
		if (!Drives.at(drive)) {
			return;
		}

		FILE* tmpfile = Drives.at(drive)->GetHostFilePtr(fullname, "rb");

		if (tmpfile == nullptr) {
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
		uint8_t buffer[64 * 1024];
		const auto bytes_read = fread(buffer, 1, sizeof(buffer), tmpfile);
		assert(bytes_read <= sizeof(buffer));

		fclose(tmpfile);

		// Override regular DOSBox BIOS
		const auto rom_base = PhysicalMake(0xf000, 0);

		// write buffer into ROM
		for (PhysPt i = 0; i < check_cast<PhysPt>(bytes_read); ++i)
			phys_writeb(rom_base + i, buffer[i]);

		// Reset the CPU registers and memory segments
		cpu_regs = {};
		Segs = {};

		// Start executing this bios
		SegSet16(cs, 0xf000);
		reg_eip = 0xfff0;
	} catch (...) {
		return;
	}
}

#endif // C_DEBUGGER
