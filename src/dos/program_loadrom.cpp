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

#include "program_loadrom.h"

#include "dosbox.h"

#include <stdio.h>

#include "drives.h"
#include "callback.h"
#include "regs.h"

void LOADROM::Run(void) {
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

void LOADROM_ProgramStart(Program **make) {
	*make=new LOADROM;
}
