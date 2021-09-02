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

#include "program_mem.h"

#include "callback.h"
#include "regs.h"

void MEM::Run(void) {
    /* Show conventional Memory */
    WriteOut("\n");

    Bit16u umb_start=dos_infoblock.GetStartOfUMBChain();
    Bit8u umb_flag=dos_infoblock.GetUMBChainState();
    Bit8u old_memstrat=DOS_GetMemAllocStrategy()&0xff;
    if (umb_start!=0xffff) {
        if ((umb_flag&1)==1) DOS_LinkUMBsToMemChain(0);
        DOS_SetMemAllocStrategy(0);
    }

    Bit16u seg,blocks;blocks=0xffff;
    DOS_AllocateMemory(&seg,&blocks);
    WriteOut(MSG_Get("PROGRAM_MEM_CONVEN"),blocks*16/1024);

    if (umb_start!=0xffff) {
        DOS_LinkUMBsToMemChain(1);
        DOS_SetMemAllocStrategy(0x40);	// search in UMBs only

        Bit16u largest_block=0,total_blocks=0,block_count=0;
        for (;; block_count++) {
            blocks=0xffff;
            DOS_AllocateMemory(&seg,&blocks);
            if (blocks==0) break;
            total_blocks+=blocks;
            if (blocks>largest_block) largest_block=blocks;
            DOS_AllocateMemory(&seg,&blocks);
        }

        Bit8u current_umb_flag=dos_infoblock.GetUMBChainState();
        if ((current_umb_flag&1)!=(umb_flag&1)) DOS_LinkUMBsToMemChain(umb_flag);
        DOS_SetMemAllocStrategy(old_memstrat);	// restore strategy

        if (block_count>0) WriteOut(MSG_Get("PROGRAM_MEM_UPPER"),total_blocks*16/1024,block_count,largest_block*16/1024);
    }

    /* Test for and show free XMS */
    reg_ax=0x4300;CALLBACK_RunRealInt(0x2f);
    if (reg_al==0x80) {
        reg_ax=0x4310;CALLBACK_RunRealInt(0x2f);
        Bit16u xms_seg=SegValue(es);Bit16u xms_off=reg_bx;
        reg_ah=8;
        CALLBACK_RunRealFar(xms_seg,xms_off);
        if (!reg_bl) {
            WriteOut(MSG_Get("PROGRAM_MEM_EXTEND"),reg_dx);
        }
    }
    /* Test for and show free EMS */
    Bit16u handle;
    char emm[9] = { 'E','M','M','X','X','X','X','0',0 };
    if (DOS_OpenFile(emm,0,&handle)) {
        DOS_CloseFile(handle);
        reg_ah=0x42;
        CALLBACK_RunRealInt(0x67);
        WriteOut(MSG_Get("PROGRAM_MEM_EXPAND"),reg_bx*16);
    }
}

void MEM_ProgramStart(Program * * make) {
	*make=new MEM;
}
