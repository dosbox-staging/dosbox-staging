/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2021-2023  The DOSBox Staging Team
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

#include "program_biostest.h"

#if C_DEBUG

#include <stdio.h>
#include <string.h>

#include <string>

#include "drives.h"
#include "regs.h"

void BIOSTEST::Run(void) {
	if (!(cmd->FindCommand(1, temp_line))) {
		WriteOut("Must specify BIOS file to load.\n");
		return;
	}

	uint8_t drive;
	char fullname[DOS_PATHLENGTH];
	localDrive *ldp = nullptr;
	if (!DOS_MakeName((char *)temp_line.c_str(), fullname, &drive))
		return;

	try {
		// try to read ROM file into buffer
		ldp = dynamic_cast<localDrive*>(Drives.at(drive));
		if (!ldp)
			return;

		FILE *tmpfile = ldp->GetSystemFilePtr(fullname, "rb");
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

#endif // C_DEBUG
