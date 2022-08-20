/*
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


#include "dosbox.h"

#if (C_DYNAMIC_X86)

#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cstddef>
#include <cstdlib>

#if defined (WIN32)
#include <windows.h>
#include <winbase.h>
#endif

#if defined(HAVE_MPROTECT) || defined(HAVE_MMAP)
#include <sys/mman.h>

#include <limits.h>

#endif // HAVE_MPROTECT

#include "callback.h"
#include "cpu.h"
#include "debug.h"
#include "fpu.h"
#include "inout.h"
#include "mem.h"
#include "paging.h"
#include "regs.h"
#include "tracy.h"

#define CACHE_MAXSIZE	(4096*3)
#define CACHE_TOTAL		(1024*1024*8)
#define CACHE_PAGES		(512)
#define CACHE_BLOCKS	(64*1024)
#define CACHE_ALIGN		(16)
#define DYN_HASH_SHIFT	(4)
#define DYN_PAGE_HASH	(4096>>DYN_HASH_SHIFT)
#define DYN_LINKS		(16)

//#define DYN_LOG 1 //Turn logging on


#if C_FPU
#define CPU_FPU 1                                               //Enable FPU escape instructions
#define X86_DYNFPU_DH_ENABLED
#endif

enum {
	G_EAX,G_ECX,G_EDX,G_EBX,
	G_ESP,G_EBP,G_ESI,G_EDI,
	G_ES,G_CS,G_SS,G_DS,G_FS,G_GS,
	G_FLAGS,G_NEWESP,G_EIP,
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
	DOP_TEST,
	DOP_MOV,
	DOP_XCHG,
};

enum ShiftOps {
	SHIFT_ROL,SHIFT_ROR,
	SHIFT_RCL,SHIFT_RCR,
	SHIFT_SHL,SHIFT_SHR,
	SHIFT_SAL,SHIFT_SAR,
};

enum BranchTypes {
	BR_O,BR_NO,BR_B,BR_NB,
	BR_Z,BR_NZ,BR_BE,BR_NBE,
	BR_S,BR_NS,BR_P,BR_NP,
	BR_L,BR_NL,BR_LE,BR_NLE
};


enum BlockReturn {
	BR_Normal=0,
	BR_Cycles,
	BR_Link1,BR_Link2,
	BR_Opcode,
	BR_Iret,
	BR_CallBack,
	BR_SMCBlock
};

#define SMC_CURRENT_BLOCK	0xffff


#define DYNFLG_HAS16		0x1		//Would like 8-bit host reg support
#define DYNFLG_HAS8			0x2		//Would like 16-bit host reg support
#define DYNFLG_LOAD			0x4		//Load value when accessed
#define DYNFLG_SAVE			0x8		//Needs to be saved back at the end of block
#define DYNFLG_CHANGED		0x10	//Value is in a register and changed from load
#define DYNFLG_ACTIVE		0x20	//Register has an active value

class GenReg;

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
	uint32_t ea,tmpb,tmpd,stack,shift,newesp;
} extra_regs;

#define IllegalOption(msg) E_Exit("DYNX86: illegal option in " msg)

#define dyn_return(a,b) gen_return(a)
#include "dyn_cache.h"

static struct {
	Bitu callback;
	uint32_t readdata;
} core_dyn;

#if defined(X86_DYNFPU_DH_ENABLED)
static struct dyn_dh_fpu {
	uint16_t		cw,host_cw;
	bool		state_used;
	// some fields expanded here for alignment purposes
	struct {
		uint32_t cw;
		uint32_t sw;
		uint32_t tag;
		uint32_t ip;
		uint32_t cs;
		uint32_t ea;
		uint32_t ds;
		uint8_t st_reg[8][10];
	} state;
	FPU_P_Reg	temp,temp2;
	uint32_t		dh_fpu_enabled;
	uint8_t		temp_state[128];
} dyn_dh_fpu;
#endif

#define X86         0x01
#define X86_64      0x02

#if C_TARGETCPU == X86_64
#include "core_dyn_x86/risc_x64.h"
#elif C_TARGETCPU == X86
#include "core_dyn_x86/risc_x86.h"
#else
#error DYN_X86 core not supported for this CPU target.
#endif

struct DynState {
	DynReg regs[G_MAX];
};

static void dyn_flags_host_to_gen(void) {
	gen_dop_word(DOP_MOV,true,DREG(EXIT),DREG(FLAGS));
	gen_dop_word_imm(DOP_AND,true,DREG(EXIT),FMASK_TEST);
	gen_load_flags(DREG(EXIT));
	gen_releasereg(DREG(EXIT));
	gen_releasereg(DREG(FLAGS));
}

static void dyn_flags_gen_to_host(void) {
	gen_save_flags(DREG(EXIT));
	gen_dop_word_imm(DOP_AND,true,DREG(EXIT),FMASK_TEST);
	gen_dop_word_imm(DOP_AND,true,DREG(FLAGS),~FMASK_TEST);
	gen_dop_word(DOP_OR,true,DREG(FLAGS),DREG(EXIT)); //flags are marked for save
	gen_releasereg(DREG(EXIT));
	gen_releasereg(DREG(FLAGS));
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

static void dyn_saveregister(DynReg * src_reg, DynReg * dst_reg) {
	dst_reg->flags=src_reg->flags;
	dst_reg->genreg=src_reg->genreg;
}

static void dyn_restoreregister(DynReg * src_reg, DynReg * dst_reg) {
	dst_reg->flags=src_reg->flags;
	dst_reg->genreg=src_reg->genreg;
	dst_reg->genreg->dynreg=dst_reg;	// necessary when register has been released
}

#include "core_dyn_x86/decoder.h"

Bits CPU_Core_Dyn_X86_Run(void) {
	ZoneScoped
	// helper class to auto-save DH_FPU state on function exit
	class auto_dh_fpu {
	public:
		~auto_dh_fpu()
		{
#if defined(X86_DYNFPU_DH_ENABLED)
			if (dyn_dh_fpu.state_used)
				gen_dh_fpu_save();
#endif
		}
	};
	auto_dh_fpu fpu_saver;

	/* Determine the linear address of CS:EIP */
restart_core:
	PhysPt ip_point=SegPhys(cs)+reg_eip;
#if C_DEBUG
#if C_HEAVY_DEBUG
		if (DEBUG_HeavyIsBreakpoint()) return debugCallback;
#endif
#endif
	CodePageHandler * chandler=0;
	if (GCC_UNLIKELY(MakeCodePage(ip_point,chandler))) {
		CPU_Exception(cpu.exception.which,cpu.exception.error);
		goto restart_core;
	}
	if (!chandler) {
		return CPU_Core_Normal_Run();
	}
	/* Find correct Dynamic Block to run */
	CacheBlock * block=chandler->FindCacheBlock(ip_point&4095);
	if (!block) {
		if (!chandler->invalidation_map || (chandler->invalidation_map[ip_point&4095]<4)) {
			block=CreateCacheBlock(chandler,ip_point,32);
		} else {
			int32_t old_cycles=CPU_Cycles;
			CPU_Cycles=1;
			// manually save
			fpu_saver = auto_dh_fpu();
			Bits nc_retcode=CPU_Core_Normal_Run();
			if (!nc_retcode) {
				CPU_Cycles=old_cycles-1;
				goto restart_core;
			}
			CPU_CycleLeft+=old_cycles;
			return nc_retcode; 
		}
	}
run_block:
	cache.block.running=0;
	BlockReturn ret=gen_runcode(block->cache.start);
#if C_DEBUG
	cycle_count += 32;
#endif
	switch (ret) {
	case BR_Iret:
#if C_DEBUG
#if C_HEAVY_DEBUG
		if (DEBUG_HeavyIsBreakpoint()) {
			return debugCallback;
		}
#endif
#endif
		if (!GETFLAG(TF)) {
			if (GETFLAG(IF) && PIC_IRQCheck) {
				return CBRET_NONE;
			}
			goto restart_core;
		}
		cpudecoder=CPU_Core_Dyn_X86_Trap_Run;
		return CBRET_NONE;
	case BR_Normal:
		/* Maybe check if we staying in the same page? */
#if C_DEBUG
#if C_HEAVY_DEBUG
		if (DEBUG_HeavyIsBreakpoint()) return debugCallback;
#endif
#endif
		goto restart_core;
	case BR_Cycles:
#if C_DEBUG
#if C_HEAVY_DEBUG			
		if (DEBUG_HeavyIsBreakpoint()) return debugCallback;
#endif
#endif
		return CBRET_NONE;
	case BR_CallBack:
		return core_dyn.callback;
	case BR_SMCBlock:
		// LOG_MSG("selfmodification of running block at %x:%x",
		//         SegValue(cs), reg_eip);
		cpu.exception.which=0;
		[[fallthrough]]; // let the normal core handle the block-modifying
		             // instruction
	case BR_Opcode:
		CPU_CycleLeft+=CPU_Cycles;
		CPU_Cycles=1;
		return CPU_Core_Normal_Run();
	case BR_Link1:
	case BR_Link2:
		{
			uint32_t temp_ip=SegPhys(cs)+reg_eip;
			CodePageHandler* temp_handler = reinterpret_cast<CodePageHandler *>(get_tlb_readhandler(temp_ip));
			if (temp_handler->flags & (cpu.code.big ? PFLAG_HASCODE32:PFLAG_HASCODE16)) {
				block=temp_handler->FindCacheBlock(temp_ip & 4095);
				if (!block || !cache.block.running) goto restart_core;
				cache.block.running->LinkTo(ret==BR_Link2,block);
				goto run_block;
			}
		}
		goto restart_core;
	}
	return CBRET_NONE;
}

Bits CPU_Core_Dyn_X86_Trap_Run(void) {
	int32_t oldCycles = CPU_Cycles;
	CPU_Cycles = 1;
	cpu.trap_skip = false;

	Bits ret=CPU_Core_Normal_Run();
	if (!cpu.trap_skip) CPU_DebugException(DBINT_STEP,reg_eip);
	CPU_Cycles = oldCycles-1;
	cpudecoder = &CPU_Core_Dyn_X86_Run;

	return ret;
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

	DynRegs[G_NEWESP].data=&extra_regs.newesp;
	DynRegs[G_NEWESP].flags=0;

	DynRegs[G_EIP].data=&reg_eip;
	DynRegs[G_EIP].flags=DYNFLG_LOAD|DYNFLG_SAVE;

	DynRegs[G_EA].data=&extra_regs.ea;
	DynRegs[G_EA].flags=0;
	DynRegs[G_STACK].data=&extra_regs.stack;
	DynRegs[G_STACK].flags=0;
	DynRegs[G_CYCLES].data=&CPU_Cycles;
	DynRegs[G_CYCLES].flags=DYNFLG_LOAD|DYNFLG_SAVE;
	DynRegs[G_TMPB].data=&extra_regs.tmpb;
	DynRegs[G_TMPB].flags=DYNFLG_HAS8|DYNFLG_HAS16;
	DynRegs[G_TMPW].data=&extra_regs.tmpd;
	DynRegs[G_TMPW].flags=DYNFLG_HAS16;
	DynRegs[G_SHIFT].data=&extra_regs.shift;
	DynRegs[G_SHIFT].flags=DYNFLG_HAS8|DYNFLG_HAS16;
	DynRegs[G_EXIT].data=0;
	DynRegs[G_EXIT].flags=DYNFLG_HAS16;
	/* Init the generator */
	gen_init();

#if defined(X86_DYNFPU_DH_ENABLED)
	/* Init the fpu state */
	dyn_dh_fpu.dh_fpu_enabled=true;
	dyn_dh_fpu.state_used=false;
	dyn_dh_fpu.cw=0x37f;
	// FINIT
	memset(&dyn_dh_fpu.state, 0, sizeof(dyn_dh_fpu.state));
	dyn_dh_fpu.state.cw = 0x37F;
	dyn_dh_fpu.state.tag = 0xFFFF;
#endif

	return;
}

void CPU_Core_Dyn_X86_Cache_Init(bool enable_cache) {
	/* Initialize code cache and dynamic blocks */
	cache_init(enable_cache);
}

void CPU_Core_Dyn_X86_Cache_Close(void) {
	cache_close();
}

void CPU_Core_Dyn_X86_SetFPUMode(bool dh_fpu) {
#if defined(X86_DYNFPU_DH_ENABLED)
	dyn_dh_fpu.dh_fpu_enabled=dh_fpu;
#endif
}

#endif
