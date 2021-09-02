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

#include "program_intro.h"

#include "mapper.h"

void INTRO::WriteOutProgramIntroSpecial()
{
    WriteOut(MSG_Get("PROGRAM_INTRO_SPECIAL"), MMOD2_NAME,
                MMOD2_NAME, PRIMARY_MOD_NAME, PRIMARY_MOD_PAD,
                PRIMARY_MOD_NAME, PRIMARY_MOD_PAD, PRIMARY_MOD_NAME,
                PRIMARY_MOD_PAD, PRIMARY_MOD_NAME, PRIMARY_MOD_PAD,
                PRIMARY_MOD_NAME, PRIMARY_MOD_PAD, PRIMARY_MOD_NAME,
                PRIMARY_MOD_PAD, PRIMARY_MOD_NAME, PRIMARY_MOD_PAD,
                PRIMARY_MOD_NAME, PRIMARY_MOD_PAD, PRIMARY_MOD_NAME,
                PRIMARY_MOD_PAD, MMOD2_NAME);
}

void INTRO::DisplayMount(void) {
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

void INTRO::Run(void) {
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
        WriteOutProgramIntroSpecial();
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
    WriteOutProgramIntroSpecial();
}

void INTRO_ProgramStart(Program **make) {
	*make=new INTRO;
}
