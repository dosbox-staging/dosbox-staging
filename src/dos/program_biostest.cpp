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

void BIOSTEST_ProgramStart(Program **make) {
	*make = new BIOSTEST;
}

#endif // C_DEBUG
