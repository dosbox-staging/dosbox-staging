/*
 *  Copyright (C) 2002-2003  The DOSBox Team
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

static EAPoint IPPoint;

#include "core_full/loadwrite.h"
#include "core_full/support.h"
#include "core_full/optable.h"
#include "core_full/ea_lookup.h"
#include "instructions.h"

static INLINE void DecodeModRM(void) {
	inst.rm=Fetchb();
	inst.rm_index=(inst.rm >> 3) & 7;
	inst.rm_eai=inst.rm&07;
	inst.rm_mod=inst.rm>>6;
	/* Decode address of mod/rm if needed */
	if (inst.rm<0xc0) inst.rm_eaa=(inst.prefix & PREFIX_ADDR) ? RMAddress_32() : RMAddress_16();
}

#define LEAVECORE											\
		SaveIP();											\
		FILLFLAGS;

#define EXCEPTION(blah)										\
	{														\
		Bit8u new_num=blah;									\
		IPPoint=inst.start_entry;							\
		LEAVECORE;											\
		Interrupt(new_num);									\
		LoadIP();											\
		goto nextopcode;									\
	}

Bits Full_DeCode(void) {

	LoadIP();
	flags.type=t_UNKNOWN;
	while (CPU_Cycles>0) {
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
		inst.start=IPPoint;
		inst.entry=inst.start_entry;
		inst.prefix=inst.start_prefix;
restartopcode:
		inst.entry=(inst.entry & 0xffffff00) | Fetchb();

		inst.code=OpCodeTable[inst.entry];
		#include "core_full/load.h"
		#include "core_full/op.h"
		#include "core_full/save.h"
nextopcode:;
		CPU_Cycles--;
	}
	LEAVECORE;
	return CBRET_NONE;
}


void CPU_Core_Full_Start(bool big) {
	if (!big) {
		inst.start_prefix=0x0;;
		inst.start_entry=0x0;
	} else {
		inst.start_prefix=PREFIX_ADDR;
		inst.start_entry=0x200;
	}
	cpudecoder=&Full_DeCode;
}
