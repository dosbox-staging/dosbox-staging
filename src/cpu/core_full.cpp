/*
 *  Copyright (C) 2002-2004  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "dosbox.h"

#include "pic.h"
#include "regs.h"
#include "cpu.h"
#include "lazyflags.h"
#include "paging.h"
#include "fpu.h"
#include "debug.h"
#include "inout.h"
#include "callback.h"


typedef PhysPt EAPoint;
#define SegBase(c)	SegPhys(c)

#define LoadMb(off) mem_readb_inline(off)
#define LoadMw(off) mem_readw_inline(off)
#define LoadMd(off) mem_readd_inline(off)

#define LoadMbs(off) (Bit8s)(LoadMb(off))
#define LoadMws(off) (Bit16s)(LoadMw(off))
#define LoadMds(off) (Bit32s)(LoadMd(off))

#define SaveMb(off,val)	mem_writeb_inline(off,val)
#define SaveMw(off,val)	mem_writew_inline(off,val)
#define SaveMd(off,val)	mem_writed_inline(off,val)

#define LoadD(reg) reg
#define SaveD(reg,val)	reg=val



#include "core_full/loadwrite.h"
#include "core_full/support.h"
#include "core_full/optable.h"
#include "instructions.h"

#define LEAVECORE											\
		SaveIP();											\
		FillFlags();

#define EXCEPTION(blah)										\
	{														\
		Bit8u new_num=blah;									\
		IPPoint=inst.opcode_start;							\
		LEAVECORE;											\
		CPU_Exception(new_num,0);							\
		LoadIP();											\
		goto nextopcode;									\
	}

Bits CPU_Core_Full_Run(void) {
	FullData inst;	
restart_core:
	if (CPU_Cycles<=0) return CBRET_NONE;
	if (!cpu.code.big) {
		inst.start_prefix=0x0;;
		inst.start_entry=0x0;
	} else {
		inst.start_prefix=PREFIX_ADDR;
		inst.start_entry=0x200;
	}
	EAPoint IPPoint;
	LoadIP();
	lflags.type=t_UNKNOWN;
	while (CPU_Cycles-->0) {
#if C_DEBUG
		cycle_count++;
#if C_HEAVY_DEBUG
		SaveIP();
		if (DEBUG_HeavyIsBreakpoint()) {
			LEAVECORE;
			return debugCallback;
		};
#endif
#endif
		inst.opcode_start=IPPoint;
		inst.entry=inst.start_entry;
		inst.prefix=inst.start_prefix;
restartopcode:
		inst.entry=(inst.entry & 0xffffff00) | Fetchb();
		inst.code=OpCodeTable[inst.entry];
		#include "core_full/load.h"
		#include "core_full/op.h"
		#include "core_full/save.h"
nextopcode:;
	}
exit_core:
	LEAVECORE;
	return CBRET_NONE;
}


void CPU_Core_Full_Init(void) {

}
