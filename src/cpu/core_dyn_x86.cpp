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

#if (C_DYNAMIC_X86)

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "callback.h"
#include "regs.h"
#include "mem.h"
#include "cpu.h"
#include "debug.h"
#include "paging.h"
#include "inout.h"

#define CACHE_TOTAL		(1024*1024*2)
#define CACHE_MAXSIZE	(4096)
#define CACHE_BLOCKS	(50*1024)
#define CACHE_ALIGN		(16)
#define DYN_HASH_SHIFT	(4)
#define DYN_PAGE_HASH	(4096>>DYN_HASH_SHIFT)
#define DYN_LINKS		(16)

#if 1
#define DYN_LOG	LOG_MSG
#else 
#define DYN_LOG
#endif

enum {
	G_EAX,G_ECX,G_EDX,G_EBX,
	G_ESP,G_EBP,G_ESI,G_EDI,
	G_ES,G_CS,G_SS,G_DS,G_FS,G_GS,
	G_FLAGS,G_SMASK,G_EIP,
	G_EA,G_STACK,G_CYCLES,
	G_TMPB,G_TMPW,G_SHIFT,
	G_EXIT,
	G_MAX,
};

enum SingleOps {
	SOP_INC,SOP_DEC,
	SOP_NOT,SOP_NEG,
};

enum DualOps {
	DOP_ADD,DOP_ADC,
	DOP_SUB,DOP_SBB,
	DOP_CMP,DOP_XOR,
	DOP_AND,DOP_OR,
	DOP_MOV,
	DOP_TEST,
	DOP_IMUL,
	DOP_XCHG,
};

enum ShiftOps {
	SHIFT_ROL,SHIFT_ROR,
	SHIFT_RCL,SHIFT_RCR,
	SHIFT_SHL,SHIFT_SHR,
	SHIFT_SAR,
};

enum BranchTypes {
	BR_O,BR_NO,BR_B,BR_NB,
	BR_Z,BR_NZ,BR_BE,BR_NBE,
	BR_S,BR_NS,BR_P,BR_NP,
	BR_L,BR_NL,BR_LE,BR_NLE
};

enum BlockType {
	BT_Free=0,
	BT_Normal,
	BT_SingleLink,
	BT_DualLink,
	BT_CheckFlags,
};

enum BlockReturn {
	BR_Normal=0,
	BR_Cycles,
	BR_Link1,BR_Link2,
	BR_Opcode,
	BR_CallBack,
};

#define DYNFLG_HAS16		0x1		//Would like 8-bit host reg support
#define DYNFLG_HAS8			0x2		//Would like 16-bit host reg support
#define DYNFLG_LOAD			0x4		//Load value when accessed
#define DYNFLG_SAVE			0x8		//Needs to be saved back at the end of block
#define DYNFLG_CHANGED		0x10	//Load value only once because of save
#define DYNFLG_LOADONCE		0x20	//Load value only once because of save

class GenReg;
class CodePageHandler;

struct DynReg {
	Bitu flags;
	GenReg * genreg;
	void * data;
}; 

enum DynAccess {
	DA_d,DA_w,
	DA_bh,DA_bl
};

enum ByteCombo {
	BC_ll,BC_lh,
	BC_hl,BC_hh,
};

static DynReg DynRegs[G_MAX];
#define DREG(_WHICH_) &DynRegs[G_ ## _WHICH_ ]

static struct {
	Bitu ea,tmpb,tmpd,stack,shift;
} extra_regs;

static void IllegalOption(void) {
	E_Exit("Illegal option");
}

#include "core_dyn_x86/cache.h" 

static struct {
	Bitu callback;
} core_dyn;


#include "core_dyn_x86/risc_x86.h"

struct DynState {
	DynReg regs[G_MAX];
};

static void dyn_releaseregs(void) {
	for (Bitu i=0;i<G_MAX;i++) gen_releasereg(&DynRegs[i]);
}
static void dyn_load_flags(void) {
	/* Load the host flags with emulated flags */
	gen_dop_word(DOP_MOV,true,DREG(TMPW),DREG(FLAGS));
	gen_dop_word_imm(DOP_AND,true,DREG(TMPW),FMASK_TEST);
	gen_load_flags(DREG(TMPW));
	gen_releasereg(DREG(TMPW));
	gen_releasereg(DREG(FLAGS));
}

static void dyn_save_flags(bool stored=false) {
	/* Store the host flags back in emulated ones */
	gen_save_flags(DREG(EXIT),stored);
	gen_dop_word_imm(DOP_AND,true,DREG(EXIT),FMASK_TEST);
	gen_dop_word_imm(DOP_AND,true,DREG(FLAGS),~FMASK_TEST);
	gen_dop_word(DOP_OR,true,DREG(FLAGS),DREG(EXIT)); //flags are marked for save
	gen_releasereg(DREG(EXIT));
}


static void dyn_savestate(DynState * state) {
	for (Bitu i=0;i<G_MAX;i++) {
		state->regs[i].flags=DynRegs[i].flags;
		state->regs[i].genreg=DynRegs[i].genreg;
	}
}

static void dyn_loadstate(DynState * state) {
	for (Bitu i=0;i<G_MAX;i++) {
		gen_setupreg(&DynRegs[i],&state->regs[i]);
	}
}


static void dyn_synchstate(DynState * state) {
	for (Bitu i=0;i<G_MAX;i++) {
		gen_synchreg(&DynRegs[i],&state->regs[i]);
	}
}
#include "core_dyn_x86/decoder.h"

Bits CPU_Core_Dyn_X86_Run(void) {
	/* Determine the linear address of CS:EIP */
restart_core:
	PhysPt ip_point=SegPhys(cs)+reg_eip;
	Bitu ip_page=ip_point>>12;
	mem_readb(ip_point);		//Init whatever page we are in
	PageHandler * handler=paging.tlb.handler[ip_page];
	CodePageHandler * chandler=0;
	#if C_HEAVY_DEBUG
			if (DEBUG_HeavyIsBreakpoint()) return debugCallback;
	#endif
	if (handler->flags & PFLAG_HASCODE) {
		/* Find correct Dynamic Block to run */
		chandler=(CodePageHandler *)handler;
findblock:;
		CacheBlock * block=chandler->FindCacheBlock(ip_point&4095);
		if (!block) {
			cache.block.running=0;
			block=CreateCacheBlock(ip_point,cpu.code.big,128);
			DYN_LOG("Created block size %x type %d",block->cache.size,block->type);
			chandler->AddCacheBlock(block);
			if (block->page.end>=4096) {
				DYN_LOG("block crosses page boundary");
			}
		}
run_block:
		BlockReturn ret=gen_runcode(block->cache.start);
		switch (ret) {
		case BR_Normal:
			/* Maybe check if we staying in the same page? */
#if C_HEAVY_DEBUG
			if (DEBUG_HeavyIsBreakpoint()) return debugCallback;
#endif
			goto restart_core;
		case BR_Cycles:
#if C_HEAVY_DEBUG			
			if (DEBUG_HeavyIsBreakpoint()) return debugCallback;
#endif
			return CBRET_NONE;
		case BR_CallBack:
			return core_dyn.callback;
		case BR_Opcode:
			CPU_CycleLeft+=CPU_Cycles;
			CPU_Cycles=1;
			return CPU_Core_Normal_Run();
		case BR_Link1:
		case BR_Link2:
			{
				Bitu temp_ip=SegPhys(cs)+reg_eip;
				Bitu temp_page=temp_ip >> 12;
				CodePageHandler * temp_handler=(CodePageHandler *)paging.tlb.handler[temp_page];
				if (temp_handler->flags & PFLAG_HASCODE) {
					block=temp_handler->FindCacheBlock(temp_ip & 4095);
					if (!block) goto restart_core;
					cache_linkblocks(cache.block.running,block,ret==BR_Link2);
					goto run_block;
				}
			}
			goto restart_core;
		}
	} else {
		if (handler->flags & PFLAG_NOCODE) {
			LOG_MSG("can't run code in this page");
			return CPU_Core_Normal_Run();
		} 
		Bitu phys_page=ip_page;
		if (!PAGING_MakePhysPage(phys_page)) {
			LOG_MSG("Can't find physpage");
			return CPU_Core_Normal_Run();
		}
		chandler=new CodePageHandler(handler);
		MEM_SetPageHandler(phys_page,1,chandler);		//Setup the handler
		PAGING_UnlinkPages(ip_page,1);
		goto findblock;
	}
	return 0;
}


void CPU_Core_Dyn_X86_Init(void) {
	Bits i;
	/* Setup the global registers and their flags */
	for (i=0;i<G_MAX;i++) DynRegs[i].genreg=0;
	DynRegs[G_EAX].data=&reg_eax;
	DynRegs[G_EAX].flags=DYNFLG_HAS8|DYNFLG_HAS16|DYNFLG_LOAD|DYNFLG_SAVE;
	DynRegs[G_ECX].data=&reg_ecx;
	DynRegs[G_ECX].flags=DYNFLG_HAS8|DYNFLG_HAS16|DYNFLG_LOAD|DYNFLG_SAVE;
	DynRegs[G_EDX].data=&reg_edx;
	DynRegs[G_EDX].flags=DYNFLG_HAS8|DYNFLG_HAS16|DYNFLG_LOAD|DYNFLG_SAVE;
	DynRegs[G_EBX].data=&reg_ebx;
	DynRegs[G_EBX].flags=DYNFLG_HAS8|DYNFLG_HAS16|DYNFLG_LOAD|DYNFLG_SAVE;

	DynRegs[G_EBP].data=&reg_ebp;
	DynRegs[G_EBP].flags=DYNFLG_HAS16|DYNFLG_LOAD|DYNFLG_SAVE;
	DynRegs[G_ESP].data=&reg_esp;
	DynRegs[G_ESP].flags=DYNFLG_HAS16|DYNFLG_LOAD|DYNFLG_SAVE;
	DynRegs[G_EDI].data=&reg_edi;
	DynRegs[G_EDI].flags=DYNFLG_HAS16|DYNFLG_LOAD|DYNFLG_SAVE;
	DynRegs[G_ESI].data=&reg_esi;
	DynRegs[G_ESI].flags=DYNFLG_HAS16|DYNFLG_LOAD|DYNFLG_SAVE;

	DynRegs[G_ES].data=&Segs.phys[es];
	DynRegs[G_ES].flags=DYNFLG_LOAD|DYNFLG_SAVE;
	DynRegs[G_CS].data=&Segs.phys[cs];
	DynRegs[G_CS].flags=DYNFLG_LOAD|DYNFLG_SAVE;
	DynRegs[G_SS].data=&Segs.phys[ss];
	DynRegs[G_SS].flags=DYNFLG_LOAD|DYNFLG_SAVE;
	DynRegs[G_DS].data=&Segs.phys[ds];
	DynRegs[G_DS].flags=DYNFLG_LOAD|DYNFLG_SAVE;
	DynRegs[G_FS].data=&Segs.phys[fs];
	DynRegs[G_FS].flags=DYNFLG_LOAD|DYNFLG_SAVE;
	DynRegs[G_GS].data=&Segs.phys[gs];
	DynRegs[G_GS].flags=DYNFLG_LOAD|DYNFLG_SAVE;

	DynRegs[G_FLAGS].data=&reg_flags;
	DynRegs[G_FLAGS].flags=DYNFLG_LOAD|DYNFLG_SAVE;

	DynRegs[G_SMASK].data=&cpu.stack.mask;
	DynRegs[G_SMASK].flags=DYNFLG_LOAD|DYNFLG_SAVE;

	DynRegs[G_EIP].data=&reg_eip;
	DynRegs[G_EIP].flags=DYNFLG_LOAD|DYNFLG_SAVE;

	DynRegs[G_EA].data=&extra_regs.ea;
	DynRegs[G_EA].flags=0;
	DynRegs[G_STACK].data=&extra_regs.stack;
	DynRegs[G_STACK].flags=0;
	DynRegs[G_CYCLES].data=&CPU_Cycles;
	DynRegs[G_CYCLES].flags=DYNFLG_LOAD|DYNFLG_SAVE;;
	DynRegs[G_TMPB].data=&extra_regs.tmpb;
	DynRegs[G_TMPB].flags=DYNFLG_HAS8|DYNFLG_HAS16;
	DynRegs[G_TMPW].data=&extra_regs.tmpd;
	DynRegs[G_TMPW].flags=DYNFLG_HAS16;
	DynRegs[G_SHIFT].data=&extra_regs.shift;
	DynRegs[G_SHIFT].flags=DYNFLG_HAS8|DYNFLG_HAS16;
	DynRegs[G_EXIT].data=0;
	DynRegs[G_EXIT].flags=DYNFLG_HAS16;
	/* Initialize code cache and dynamic blocks */
	cache_init();
	/* Init the generator */
	gen_init();
	return;
}

#endif
