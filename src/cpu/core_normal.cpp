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
#include "mem.h"
#include "cpu.h"
#include "lazyflags.h"
#include "inout.h"
#include "callback.h"
#include "pic.h"
#include "fpu.h"

#if C_DEBUG
#include "debug.h"
#endif


#define SegBase(c)	SegPhys(c)
#if (!C_CORE_INLINE)

#define LoadMb(off) mem_readb(off)
#define LoadMw(off) mem_readw(off)
#define LoadMd(off) mem_readd(off)

#define SaveMb(off,val)	mem_writeb(off,val)
#define SaveMw(off,val)	mem_writew(off,val)
#define SaveMd(off,val)	mem_writed(off,val)

#else 

#include "paging.h"
#define LoadMb(off) mem_readb_inline(off)
#define LoadMw(off) mem_readw_inline(off)
#define LoadMd(off) mem_readd_inline(off)

#define SaveMb(off,val)	mem_writeb_inline(off,val)
#define SaveMw(off,val)	mem_writew_inline(off,val)
#define SaveMd(off,val)	mem_writed_inline(off,val)

#endif

#define LoadMbs(off) (Bit8s)(LoadMb(off))
#define LoadMws(off) (Bit16s)(LoadMw(off))
#define LoadMds(off) (Bit32s)(LoadMd(off))

#define LoadRb(reg) reg
#define LoadRw(reg) reg
#define LoadRd(reg) reg

#define SaveRb(reg,val)	reg=val
#define SaveRw(reg,val)	reg=val
#define SaveRd(reg,val)	reg=val

extern Bitu cycle_count;

#if C_FPU
#define CPU_FPU	1						//Enable FPU escape instructions
#endif

#define CPU_PIC_CHECK 1
#define CPU_TRAP_CHECK 1

#define OPCODE_NONE			0x000
#define OPCODE_0F			0x100
#define OPCODE_SIZE			0x200

#define PREFIX_SEG			0x1
#define PREFIX_ADDR			0x2
#define PREFIX_SEG_ADDR		(PREFIX_SEG|PREFIX_ADDR)
#define PREFIX_REP			0x4

#define TEST_PREFIX_SEG		(core.prefixes & PREFIX_SEG)
#define TEST_PREFIX_ADDR	(core.prefixes & PREFIX_ADDR)
#define TEST_PREFIX_REP		(core.prefixes & PREFIX_REP)

#define DO_PREFIX_SEG(_SEG)					\
	core.prefixes|=PREFIX_SEG;				\
	core.seg_prefix_base=SegBase(_SEG);		\
	goto restart_prefix;

#define DO_PREFIX_ADDR()									\
	core.prefixes|=(core.prefix_default ^ PREFIX_ADDR) & PREFIX_ADDR;		\
	goto restart_prefix;

#define DO_PREFIX_REP(_ZERO)				\
	core.prefixes|=PREFIX_REP;				\
	core.rep_zero=_ZERO;					\
	goto restart_prefix;

typedef PhysPt (*GetEATable[256])(void);

static struct {
	Bitu opcode_index;
	Bitu prefixes;
	Bitu index_default;
	Bitu prefix_default;
	PhysPt op_start;
	PhysPt ip_lookup;
	PhysPt seg_prefix_base;
	bool rep_zero;
	GetEATable * ea_table;
	struct {
		bool skip;
	} trap;
} core;

#include "instructions.h"
#include "core_normal/support.h"
#include "core_normal/string.h"

static GetEATable * EAPrefixTable[8] = {
	&GetEA_NONE,&GetEA_SEG,&GetEA_ADDR,&GetEA_SEG_ADDR,
	&GetEA_NONE,&GetEA_SEG,&GetEA_ADDR,&GetEA_SEG_ADDR,
};

#define CASE_W(_WHICH)							\
	case (OPCODE_NONE+_WHICH):

#define CASE_D(_WHICH)							\
	case (OPCODE_SIZE+_WHICH):

#define CASE_B(_WHICH)							\
	CASE_W(_WHICH)								\
	CASE_D(_WHICH)

#define CASE_0F_W(_WHICH)						\
	case ((OPCODE_0F|OPCODE_NONE)+_WHICH):

#define CASE_0F_D(_WHICH)						\
	case ((OPCODE_0F|OPCODE_SIZE)+_WHICH):

#define CASE_0F_B(_WHICH)						\
	CASE_0F_W(_WHICH)							\
	CASE_0F_D(_WHICH)

#define EALookupTable (*(core.ea_table))

Bits CPU_Core_Normal_Run(void) {
decode_start:
	if (cpu.code.big) {
		core.index_default=0x200;
		core.prefix_default=PREFIX_ADDR;
	} else {
		core.index_default=0;
		core.prefix_default=0;
	}
	LOADIP;
	lflags.type=t_UNKNOWN;
	while (CPU_Cycles-->0) {
		core.op_start=core.ip_lookup;
		core.opcode_index=core.index_default;
		core.prefixes=core.prefix_default;
#if C_DEBUG
		cycle_count++;
#if C_HEAVY_DEBUG
		SAVEIP;
		if (DEBUG_HeavyIsBreakpoint()) {
			LEAVECORE;
			return debugCallback;
		};
#endif
#endif
restart_prefix:
		core.ea_table=EAPrefixTable[core.prefixes];
restart_opcode:
		switch (core.opcode_index+Fetchb()) {

		#include "core_normal/prefix_none.h"
		#include "core_normal/prefix_0f.h"
		#include "core_normal/prefix_66.h"
		#include "core_normal/prefix_66_0f.h"
		default:
			ADDIPFAST(-1);
#if C_DEBUG
			LOG_MSG("Unhandled code %X",core.opcode_index+Fetchb());
#else
			E_Exit("Unhandled CPU opcode");
#endif
		}
	}
decode_end:
	LEAVECORE;
	return CBRET_NONE;

illegal_opcode:
	LEAVECORE;
	reg_eip-=core.ip_lookup-core.op_start;
	CPU_Exception(6,0);
	goto decode_start;
}

Bits CPU_Core_Normal_Trap_Run(void) {

	Bits oldCycles = CPU_Cycles;
	CPU_Cycles = 1;
	core.trap.skip=false;

	Bits ret=CPU_Core_Normal_Run();
	if (!core.trap.skip) CPU_SW_Interrupt(1,0);
	CPU_Cycles = oldCycles-1;
	cpudecoder = &CPU_Core_Normal_Run;

	return ret;
}



void CPU_Core_Normal_Init(void) {

}

