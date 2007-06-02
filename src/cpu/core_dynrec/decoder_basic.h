/*
 *  Copyright (C) 2002-2006  The DOSBox Team
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


/*
	This file provides some definitions and basic level functions
	that use code generating functions from risc_?.h
	Important is the function call generation including parameter
	loading, the effective address calculation and the memory
	access functions.
*/


// instructions that use one operand
enum SingleOps {
	SOP_INC,SOP_DEC,
	SOP_NOT,SOP_NEG,
};

// instructions that use two operand
enum DualOps {
	DOP_ADD,DOP_ADC,
	DOP_SUB,DOP_SBB,
	DOP_CMP,DOP_XOR,
	DOP_AND,DOP_OR,
	DOP_TEST,
	DOP_MOV,
	DOP_XCHG,
};

// shift and rotate functions
enum ShiftOps {
	SHIFT_ROL,SHIFT_ROR,
	SHIFT_RCL,SHIFT_RCR,
	SHIFT_SHL,SHIFT_SHR,
	SHIFT_SAL,SHIFT_SAR,
};

// branch conditions
enum BranchTypes {
	BR_O,BR_NO,BR_B,BR_NB,
	BR_Z,BR_NZ,BR_BE,BR_NBE,
	BR_S,BR_NS,BR_P,BR_NP,
	BR_L,BR_NL,BR_LE,BR_NLE
};

// string instructions
enum StringOps {
	STR_OUTSB=0,STR_OUTSW,STR_OUTSD,
	STR_INSB=4,STR_INSW,STR_INSD,
	STR_MOVSB=8,STR_MOVSW,STR_MOVSD,
	STR_LODSB=12,STR_LODSW,STR_LODSD,
	STR_STOSB=16,STR_STOSW,STR_STOSD,
	STR_SCASB=20,STR_SCASW,STR_SCASD,
	STR_CMPSB=24,STR_CMPSW,STR_CMPSD,
};

// repeat prefix type (for string operations)
enum REP_Type {
	REP_NONE=0,REP_NZ,REP_Z
};

// loop type
enum LoopTypes {
	LOOP_NONE,LOOP_NE,LOOP_E,LOOP_JCXZ
};

// rotate operand type
enum grp2_types {
	grp2_1,grp2_imm,grp2_cl,
};

// opcode mapping for group1 instructions
static DualOps grp1_table[8]={
	DOP_ADD,DOP_OR,DOP_ADC,DOP_SBB,DOP_AND,DOP_SUB,DOP_XOR,DOP_CMP
};


// decoding information used during translation of a code block
static struct DynDecode {
	PhysPt code;			// pointer to next byte in the instruction stream
	PhysPt code_start;		// pointer to the start of the current code block
	PhysPt op_start;		// pointer to the start of the current instruction
	bool big_op;			// operand modifier
	bool big_addr;			// address modifier
	REP_Type rep;			// current repeat prefix
	Bitu cycles;			// number cycles used by currently translated code
	bool seg_prefix_used;	// segment overridden
	Bit8u seg_prefix;		// segment prefix (if seg_prefix_used==true)

	// block that contains the first instruction translated
	CacheBlockDynRec * block;
	// block that contains the current byte of the instruction stream
	CacheBlockDynRec * active_block;

	// the active page (containing the current byte of the instruction stream)
	struct {
		CodePageHandlerDynRec * code;
		Bitu index;		// index to the current byte of the instruction stream
		Bit8u * wmap;	// write map that indicates code presence for every byte of this page
		Bit8u * invmap;	// invalidation map
		Bitu first;		// page number 
	} page;

	// modrm state of the current instruction (if used)
	struct {
		Bitu val;
		Bitu mod;
		Bitu rm;
		Bitu reg;
	} modrm;
} decode;


static bool MakeCodePage(Bitu lin_addr,CodePageHandlerDynRec * &cph) {
	Bit8u rdval;
	//Ensure page contains memory:
	if (GCC_UNLIKELY(mem_readb_checked_x86(lin_addr,&rdval))) return true;

	Bitu lin_page=lin_addr >> 12;

	PageHandler * handler=paging.tlb.handler[lin_page];
	if (handler->flags & PFLAG_HASCODE) {
		// this is a codepage handler, and the one that we're looking for
		cph=(CodePageHandlerDynRec *)handler;
		return false;
	}
	if (handler->flags & PFLAG_NOCODE) {
		LOG_MSG("DYNREC:Can't run code in this page");
		cph=0;
		return false;
	} 
	Bitu phys_page=lin_page;
	// find the physical page that the linear page is mapped to
	if (!PAGING_MakePhysPage(phys_page)) {
		LOG_MSG("DYNREC:Can't find physpage");
		cph=0;
		return false;
	}
	// find a free CodePage
	if (!cache.free_pages) {
		if (cache.used_pages!=decode.page.code) cache.used_pages->ClearRelease();
		else {
			// try another page to avoid clearing our source-crosspage
			if ((cache.used_pages->next) && (cache.used_pages->next!=decode.page.code))
				cache.used_pages->next->ClearRelease();
			else {
				LOG_MSG("DYNREC:Invalid cache links");
				cache.used_pages->ClearRelease();
			}
		}
	}
	CodePageHandlerDynRec * cpagehandler=cache.free_pages;
	cache.free_pages=cache.free_pages->next;

	// adjust previous and next page pointer
	cpagehandler->prev=cache.last_page;
	cpagehandler->next=0;
	if (cache.last_page) cache.last_page->next=cpagehandler;
	cache.last_page=cpagehandler;
	if (!cache.used_pages) cache.used_pages=cpagehandler;

	// initialize the code page handler and add the handler to the memory page
	cpagehandler->SetupAt(phys_page,handler);
	MEM_SetPageHandler(phys_page,1,cpagehandler);
	PAGING_UnlinkPages(lin_page,1);
	cph=cpagehandler;
	return false;
}

static void decode_advancepage(void) {
	// Advance to the next page
	decode.active_block->page.end=4095;
	// trigger possible page fault here
	decode.page.first++;
	Bitu faddr=decode.page.first << 12;
	mem_readb(faddr);
	MakeCodePage(faddr,decode.page.code);
	CacheBlockDynRec * newblock=cache_getblock();
	decode.active_block->crossblock=newblock;
	newblock->crossblock=decode.active_block;
	decode.active_block=newblock;
	decode.active_block->page.start=0;
	decode.page.code->AddCrossBlock(decode.active_block);
	decode.page.wmap=decode.page.code->write_map;
	decode.page.invmap=decode.page.code->invalidation_map;
	decode.page.index=0;
}

// fetch the next byte of the instruction stream
static Bit8u decode_fetchb(void) {
	if (GCC_UNLIKELY(decode.page.index>=4096)) {
		decode_advancepage();
	}
	decode.page.wmap[decode.page.index]+=0x01;
	decode.page.index++;
	decode.code+=1;
	return mem_readb(decode.code-1);
}
// fetch the next word of the instruction stream
static Bit16u decode_fetchw(void) {
	if (GCC_UNLIKELY(decode.page.index>=4095)) {
   		Bit16u val=decode_fetchb();
		val|=decode_fetchb() << 8;
		return val;
	}
	*(Bit16u *)&decode.page.wmap[decode.page.index]+=0x0101;
	decode.code+=2;decode.page.index+=2;
	return mem_readw(decode.code-2);
}
// fetch the next dword of the instruction stream
static Bit32u decode_fetchd(void) {
	if (GCC_UNLIKELY(decode.page.index>=4093)) {
   		Bit32u val=decode_fetchb();
		val|=decode_fetchb() << 8;
		val|=decode_fetchb() << 16;
		val|=decode_fetchb() << 24;
		return val;
        /* Advance to the next page */
	}
	*(Bit32u *)&decode.page.wmap[decode.page.index]+=0x01010101;
	decode.code+=4;decode.page.index+=4;
	return mem_readd(decode.code-4);
}

#define START_WMMEM 64

// adjust writemap mask to care for map holes due to special
// codefetch functions
static void INLINE decode_increase_wmapmask(Bitu size) {
	Bitu mapidx;
	CacheBlockDynRec* activecb=decode.active_block; 
	if (GCC_UNLIKELY(!activecb->cache.wmapmask)) {
		// no mask memory yet allocated, start with a small buffer
		activecb->cache.wmapmask=(Bit8u*)malloc(START_WMMEM);
		memset(activecb->cache.wmapmask,0,START_WMMEM);
		activecb->cache.maskstart=decode.page.index;	// start of buffer is current code position
		activecb->cache.masklen=START_WMMEM;
		mapidx=0;
	} else {
		mapidx=decode.page.index-activecb->cache.maskstart;
		if (GCC_UNLIKELY(mapidx+size>=activecb->cache.masklen)) {
			// mask buffer too small, increase
			Bitu newmasklen=activecb->cache.masklen*4;
			if (newmasklen<mapidx+size) newmasklen=((mapidx+size)&~3)*2;
			Bit8u* tempmem=(Bit8u*)malloc(newmasklen);
			memset(tempmem,0,newmasklen);
			memcpy(tempmem,activecb->cache.wmapmask,activecb->cache.masklen);
			free(activecb->cache.wmapmask);
			activecb->cache.wmapmask=tempmem;
			activecb->cache.masklen=newmasklen;
		}
	}
	// update mask entries
	switch (size) {
		case 1 : activecb->cache.wmapmask[mapidx]+=0x01; break;
		case 2 : (*(Bit16u*)&activecb->cache.wmapmask[mapidx])+=0x0101; break;
		case 4 : (*(Bit32u*)&activecb->cache.wmapmask[mapidx])+=0x01010101; break;
	}
}

// fetch a byte, val points to the code location if possible,
// otherwise val contains the current value read from the position
static bool decode_fetchb_imm(Bitu & val) {
	if (GCC_UNLIKELY(decode.page.index>=4096)) {
		decode_advancepage();
	}
	Bitu index=(decode.code>>12);
	// see if position is directly accessible
	if (paging.tlb.read[index]) {
		val=(Bitu)(paging.tlb.read[index]+decode.code);
		decode_increase_wmapmask(1);
		decode.code++;
		decode.page.index++;
		return true;
	}
	// not directly accessible, just fetch the value
	val=(Bit32u)decode_fetchb();
	return false;
}

// fetch a word, val points to the code location if possible,
// otherwise val contains the current value read from the position
static bool decode_fetchw_imm(Bitu & val) {
	if (decode.page.index<4095) {
		Bitu index=(decode.code>>12);
		// see if position is directly accessible
		if (paging.tlb.read[index]) {
			val=(Bitu)(paging.tlb.read[index]+decode.code);
			decode_increase_wmapmask(2);
			decode.code+=2;
			decode.page.index+=2;
			return true;
		}
	}
	// not directly accessible, just fetch the value
	val=decode_fetchw();
	return false;
}

// fetch a dword, val points to the code location if possible,
// otherwise val contains the current value read from the position
static bool decode_fetchd_imm(Bitu & val) {
	if (decode.page.index<4093) {
		Bitu index=(decode.code>>12);
		// see if position is directly accessible
		if (paging.tlb.read[index]) {
			val=(Bitu)(paging.tlb.read[index]+decode.code);
			decode_increase_wmapmask(4);
			decode.code+=4;
			decode.page.index+=4;
			return true;
		}
	}
	// not directly accessible, just fetch the value
	val=decode_fetchd();
	return false;
}


// modrm decoding helper
static void INLINE dyn_get_modrm(void) {
	decode.modrm.val=decode_fetchb();
	decode.modrm.mod=(decode.modrm.val >> 6) & 3;
	decode.modrm.reg=(decode.modrm.val >> 3) & 7;
	decode.modrm.rm=(decode.modrm.val & 7);
}



// adjust CPU_Cycles value
static void dyn_reduce_cycles(void) {
	if (!decode.cycles) decode.cycles++;
	gen_sub_direct_word(&CPU_Cycles,decode.cycles,true);
}


// set reg to the start of the next instruction
// set reg_eip to the start of the current instruction
static INLINE void dyn_set_eip_last_end(HostReg reg) {
	gen_mov_word_to_reg(reg,&reg_eip,true);
	gen_add_imm(reg,(Bit32u)(decode.code-decode.code_start));
	gen_add_direct_word(&reg_eip,decode.op_start-decode.code_start,decode.big_op);
}

// set reg_eip to the start of the current instruction
static INLINE void dyn_set_eip_last(void) {
	gen_add_direct_word(&reg_eip,decode.op_start-decode.code_start,cpu.code.big);
}

// set reg_eip to the start of the next instruction
static INLINE void dyn_set_eip_end(void) {
	gen_add_direct_word(&reg_eip,decode.code-decode.code_start,cpu.code.big);
}

// set reg_eip to the start of the next instruction plus an offset (imm)
static INLINE void dyn_set_eip_end(HostReg reg,Bit32u imm=0) {
	gen_mov_word_to_reg(reg,&reg_eip,decode.big_op);
	gen_add_imm(reg,(Bit32u)(decode.code-decode.code_start+imm));
	if (!decode.big_op) gen_extend_word(false,reg);
}



// the following functions generate function calls
// parameters are loaded by generating code using gen_load_param_ which
// is architecture dependent
// R=host register; I=32bit immediate value; A=address value; m=memory

static DRC_PTR_SIZE_IM INLINE gen_call_function_R(void * func,Bitu op) {
	gen_load_param_reg(op,0);
	return gen_call_function_setup(func, 1);
}

static DRC_PTR_SIZE_IM INLINE gen_call_function_R3(void * func,Bitu op) {
	gen_load_param_reg(op,2);
	return gen_call_function_setup(func, 3, true);
}

static DRC_PTR_SIZE_IM INLINE gen_call_function_RI(void * func,Bitu op1,Bitu op2) {
	gen_load_param_imm(op2,1);
	gen_load_param_reg(op1,0);
	return gen_call_function_setup(func, 2);
}

static DRC_PTR_SIZE_IM INLINE gen_call_function_RA(void * func,Bitu op1,DRC_PTR_SIZE_IM op2) {
	gen_load_param_addr(op2,1);
	gen_load_param_reg(op1,0);
	return gen_call_function_setup(func, 2);
}

static DRC_PTR_SIZE_IM INLINE gen_call_function_RR(void * func,Bitu op1,Bitu op2) {
	gen_load_param_reg(op2,1);
	gen_load_param_reg(op1,0);
	return gen_call_function_setup(func, 2);
}

static DRC_PTR_SIZE_IM INLINE gen_call_function_IR(void * func,Bitu op1,Bitu op2) {
	gen_load_param_reg(op2,1);
	gen_load_param_imm(op1,0);
	return gen_call_function_setup(func, 2);
}

static DRC_PTR_SIZE_IM INLINE gen_call_function_I(void * func,Bitu op) {
	gen_load_param_imm(op,0);
	return gen_call_function_setup(func, 1);
}

static DRC_PTR_SIZE_IM INLINE gen_call_function_II(void * func,Bitu op1,Bitu op2) {
	gen_load_param_imm(op2,1);
	gen_load_param_imm(op1,0);
	return gen_call_function_setup(func, 2);
}

static DRC_PTR_SIZE_IM INLINE gen_call_function_III(void * func,Bitu op1,Bitu op2,Bitu op3) {
	gen_load_param_imm(op3,2);
	gen_load_param_imm(op2,1);
	gen_load_param_imm(op1,0);
	return gen_call_function_setup(func, 3);
}

static DRC_PTR_SIZE_IM INLINE gen_call_function_IA(void * func,Bitu op1,DRC_PTR_SIZE_IM op2) {
	gen_load_param_addr(op2,1);
	gen_load_param_imm(op1,0);
	return gen_call_function_setup(func, 2);
}

static DRC_PTR_SIZE_IM INLINE gen_call_function_IIR(void * func,Bitu op1,Bitu op2,Bitu op3) {
	gen_load_param_reg(op3,2);
	gen_load_param_imm(op2,1);
	gen_load_param_imm(op1,0);
	return gen_call_function_setup(func, 3);
}

static DRC_PTR_SIZE_IM INLINE gen_call_function_IIIR(void * func,Bitu op1,Bitu op2,Bitu op3,Bitu op4) {
	gen_load_param_reg(op4,3);
	gen_load_param_imm(op3,2);
	gen_load_param_imm(op2,1);
	gen_load_param_imm(op1,0);
	return gen_call_function_setup(func, 4);
}

static DRC_PTR_SIZE_IM INLINE gen_call_function_IRRR(void * func,Bitu op1,Bitu op2,Bitu op3,Bitu op4) {
	gen_load_param_reg(op4,3);
	gen_load_param_reg(op3,2);
	gen_load_param_reg(op2,1);
	gen_load_param_imm(op1,0);
	return gen_call_function_setup(func, 4);
}

static DRC_PTR_SIZE_IM INLINE gen_call_function_m(void * func,Bitu op) {
	gen_load_param_mem(op,2);
	return gen_call_function_setup(func, 3, true);
}

static DRC_PTR_SIZE_IM INLINE gen_call_function_mm(void * func,Bitu op1,Bitu op2) {
	gen_load_param_mem(op2,3);
	gen_load_param_mem(op1,2);
	return gen_call_function_setup(func, 4, true);
}



enum save_info_type {exception, cycle_check, string_break};


// function that is called on exceptions
static BlockReturn DynRunException(Bit32u eip_add,Bit32u cycle_sub) {
	reg_eip+=eip_add;
	CPU_Cycles-=cycle_sub;
	if (cpu.exception.which==SMC_CURRENT_BLOCK) return BR_SMCBlock;
	CPU_Exception(cpu.exception.which,cpu.exception.error);
	return BR_Normal;
}


// array with information about code that is generated at the
// end of a cache block because it is rarely reached (like exceptions)
static struct {
	save_info_type type;
	DRC_PTR_SIZE_IM branch_pos;
	Bit32u eip_change;
	Bitu cycles;
} save_info_dynrec[512];

Bitu used_save_info_dynrec=0;


// return from current block, with returncode
static void dyn_return(BlockReturn retcode,bool ret_exception=false) {
	if (!ret_exception) {
		gen_mov_dword_to_reg_imm(FC_RETOP,retcode);
	}
	gen_return_function();
}

static void dyn_run_code(void) {
	gen_run_code();
	gen_return_function();
}

// fill in code at the end of the block that contains rarely-executed code
// which is executed conditionally (like exceptions)
static void dyn_fill_blocks(void) {
	for (Bitu sct=0; sct<used_save_info_dynrec; sct++) {
		gen_fill_branch_long(save_info_dynrec[sct].branch_pos);
		switch (save_info_dynrec[sct].type) {
			case exception:
				// code for exception handling, load cycles and call DynRunException
				decode.cycles=save_info_dynrec[sct].cycles;
				if (cpu.code.big) gen_call_function_II((void *)&DynRunException,save_info_dynrec[sct].eip_change,save_info_dynrec[sct].cycles);
				else gen_call_function_II((void *)&DynRunException,save_info_dynrec[sct].eip_change&0xffff,save_info_dynrec[sct].cycles);
				dyn_return(BR_Normal,true);
				break;
			case cycle_check:
				// cycles are <=0 so exit the core
				dyn_return(BR_Cycles);
				break;
			case string_break:
				// interrupt looped string instruction, can be continued later
				gen_add_direct_word(&reg_eip,save_info_dynrec[sct].eip_change,decode.big_op);
				dyn_return(BR_Cycles);
				break;
		}
	}
	used_save_info_dynrec=0;
}


static void dyn_closeblock(void) {
	//Shouldn't create empty block normally but let's do it like this
	dyn_fill_blocks();
	cache_closeblock();
}


// add a check that can branch to the exception handling
static void dyn_check_exception(HostReg reg) {
	save_info_dynrec[used_save_info_dynrec].branch_pos=gen_create_branch_long_nonzero(reg,false);
	if (!decode.cycles) decode.cycles++;
	save_info_dynrec[used_save_info_dynrec].cycles=decode.cycles;
	// in case of an exception eip will point to the start of the current instruction
	save_info_dynrec[used_save_info_dynrec].eip_change=decode.op_start-decode.code_start;
	if (!cpu.code.big) save_info_dynrec[used_save_info_dynrec].eip_change&=0xffff;
	save_info_dynrec[used_save_info_dynrec].type=exception;
	used_save_info_dynrec++;
}



bool DRC_CALL_CONV mem_readb_checked_drc(PhysPt address) {
	Bitu index=(address>>12);
	if (paging.tlb.read[index]) {
		*((Bit8u*)(&core_dynrec.readdata))=host_readb(paging.tlb.read[index]+address);
		return false;
	} else {
		Bitu uval;
		bool retval;
		retval=paging.tlb.handler[index]->readb_checked(address, &uval);
		*((Bit8u*)(&core_dynrec.readdata))=(Bit8u)uval;
		return retval;
	}
}

bool DRC_CALL_CONV mem_writeb_checked_drc(PhysPt address,Bit8u val) {
	Bitu index=(address>>12);
	if (paging.tlb.write[index]) {
		host_writeb(paging.tlb.write[index]+address,val);
		return false;
	} else return paging.tlb.handler[index]->writeb_checked(address,val);
}

bool DRC_CALL_CONV mem_readw_checked_drc(PhysPt address) {
#if defined(WORDS_BIGENDIAN) || !defined(C_UNALIGNED_MEMORY)
	if (!(address & 1)) {
#else
	if ((address & 0xfff)<0xfff) {
#endif
			Bitu index=(address>>12);
		if (paging.tlb.read[index]) {
			*((Bit16u*)(&core_dynrec.readdata))=host_readw(paging.tlb.read[index]+address);
			return false;
		} else {
			Bitu uval;
			bool retval;
			retval=paging.tlb.handler[index]->readw_checked(address, &uval);
			*((Bit16u*)(&core_dynrec.readdata))=(Bit16u)uval;
			return retval;
		}
	} else return mem_unalignedreadw_checked_x86(address, ((Bit16u*)(&core_dynrec.readdata)));
}

bool DRC_CALL_CONV mem_readd_checked_drc(PhysPt address) {
#if defined(WORDS_BIGENDIAN) || !defined(C_UNALIGNED_MEMORY)
	if (!(address & 3)) {
#else
	if ((address & 0xfff)<0xffd) {
#endif
		Bitu index=(address>>12);
		if (paging.tlb.read[index]) {
			*((Bit32u*)(&core_dynrec.readdata))=host_readd(paging.tlb.read[index]+address);
			return false;
		} else {
			Bitu uval;
			bool retval;
			retval=paging.tlb.handler[index]->readd_checked(address, &uval);
			*((Bit32u*)(&core_dynrec.readdata))=(Bit32u)uval;
			return retval;
		}
	} else return mem_unalignedreadd_checked_x86(address, ((Bit32u*)(&core_dynrec.readdata)));
}

bool DRC_CALL_CONV mem_writew_checked_drc(PhysPt address,Bit16u val) {
#if defined(WORDS_BIGENDIAN) || !defined(C_UNALIGNED_MEMORY)
	if (!(address & 1)) {
#else
	if ((address & 0xfff)<0xfff) {
#endif
		Bitu index=(address>>12);
		if (paging.tlb.write[index]) {
			host_writew(paging.tlb.write[index]+address,val);
			return false;
		} else return paging.tlb.handler[index]->writew_checked(address,val);
	} else return mem_unalignedwritew_checked_x86(address,val);
}

bool DRC_CALL_CONV mem_writed_checked_drc(PhysPt address,Bit32u val) {
#if defined(WORDS_BIGENDIAN) || !defined(C_UNALIGNED_MEMORY)
	if (!(address & 3)) {
#else
	if ((address & 0xfff)<0xffd) {
#endif
		Bitu index=(address>>12);
		if (paging.tlb.write[index]) {
			host_writed(paging.tlb.write[index]+address,val);
			return false;
		} else return paging.tlb.handler[index]->writed_checked(address,val);
	} else return mem_unalignedwrited_checked_x86(address,val);
}


// functions that enable access to the memory

// read a byte from a given address and store it in reg_dst
static void dyn_read_byte(HostReg reg_addr,HostReg reg_dst) {
	gen_mov_regs(FC_OP1,reg_addr);
	gen_call_function_raw((void *)&mem_readb_checked_drc);
	dyn_check_exception(FC_RETOP);
	gen_mov_byte_to_reg_low(reg_dst,&core_dynrec.readdata);
}
static void dyn_read_byte_canuseword(HostReg reg_addr,HostReg reg_dst) {
	gen_mov_regs(FC_OP1,reg_addr);
	gen_call_function_raw((void *)&mem_readb_checked_drc);
	dyn_check_exception(FC_RETOP);
	gen_mov_byte_to_reg_low_canuseword(reg_dst,&core_dynrec.readdata);
}

// write a byte from reg_val into the memory given by the address
static void dyn_write_byte(HostReg reg_addr,HostReg reg_val) {
	gen_mov_regs(FC_OP2,reg_val);
	gen_mov_regs(FC_OP1,reg_addr);
	gen_call_function_raw((void *)&mem_writeb_checked_drc);
	dyn_check_exception(FC_RETOP);
}

// read a 32bit (dword=true) or 16bit (dword=false) value
// from a given address and store it in reg_dst
static void dyn_read_word(HostReg reg_addr,HostReg reg_dst,bool dword) {
	gen_mov_regs(FC_OP1,reg_addr);
	if (dword) gen_call_function_raw((void *)&mem_readd_checked_drc);
	else gen_call_function_raw((void *)&mem_readw_checked_drc);
	dyn_check_exception(FC_RETOP);
	gen_mov_word_to_reg(reg_dst,&core_dynrec.readdata,dword);
}

// write a 32bit (dword=true) or 16bit (dword=false) value
// from reg_val into the memory given by the address
static void dyn_write_word(HostReg reg_addr,HostReg reg_val,bool dword) {
//	if (!dword) gen_extend_word(false,reg_val);
	gen_mov_regs(FC_OP2,reg_val);
	gen_mov_regs(FC_OP1,reg_addr);
	if (dword) gen_call_function_raw((void *)&mem_writed_checked_drc);
	else gen_call_function_raw((void *)&mem_writew_checked_drc);
	dyn_check_exception(FC_RETOP);
}



// effective address calculation helper, op2 has to be present!
// loads op1 into ea_reg and adds the scaled op2 and the immediate to it
static void dyn_lea(HostReg ea_reg,void* op1,void* op2,Bitu scale,Bits imm) {
	if (scale || imm) {
		if (op1!=NULL) {
			gen_mov_word_to_reg(ea_reg,op1,true);
			gen_mov_word_to_reg(TEMP_REG_DRC,op2,true);

			gen_lea(ea_reg,TEMP_REG_DRC,scale,imm);
		} else {
			gen_mov_word_to_reg(ea_reg,op2,true);
			gen_lea(ea_reg,scale,imm);
		}
	} else {
		gen_mov_word_to_reg(ea_reg,op2,true);
		if (op1!=NULL) gen_add(ea_reg,op1);
	}
}

// calculate the effective address and store it in ea_reg
static void dyn_fill_ea(HostReg ea_reg,bool addseg=true) {
	Bit8u seg_base=DRC_SEG_DS;
	if (!decode.big_addr) {
		Bits imm;
		switch (decode.modrm.mod) {
		case 0:imm=0;break;
		case 1:imm=(Bit8s)decode_fetchb();break;
		case 2:imm=(Bit16s)decode_fetchw();break;
		}
		switch (decode.modrm.rm) {
		case 0:// BX+SI
			dyn_lea(ea_reg,DRCD_REG(DRC_REG_EBX),DRCD_REG(DRC_REG_ESI),0,imm);
			break;
		case 1:// BX+DI
			dyn_lea(ea_reg,DRCD_REG(DRC_REG_EBX),DRCD_REG(DRC_REG_EDI),0,imm);
			break;
		case 2:// BP+SI
			dyn_lea(ea_reg,DRCD_REG(DRC_REG_EBP),DRCD_REG(DRC_REG_ESI),0,imm);
			seg_base=DRC_SEG_SS;
			break;
		case 3:// BP+DI
			dyn_lea(ea_reg,DRCD_REG(DRC_REG_EBP),DRCD_REG(DRC_REG_EDI),0,imm);
			seg_base=DRC_SEG_SS;
			break;
		case 4:// SI
			gen_mov_word_to_reg(ea_reg,DRCD_REG(DRC_REG_ESI),true);
			if (imm) gen_add_imm(ea_reg,(Bit32u)imm);
			break;
		case 5:// DI
			gen_mov_word_to_reg(ea_reg,DRCD_REG(DRC_REG_EDI),true);
			if (imm) gen_add_imm(ea_reg,(Bit32u)imm);
			break;
		case 6:// imm/BP
			if (!decode.modrm.mod) {
				imm=decode_fetchw();
				gen_mov_dword_to_reg_imm(ea_reg,(Bit32u)imm);
				goto skip_extend_word;
			} else {
				gen_mov_word_to_reg(ea_reg,DRCD_REG(DRC_REG_EBP),true);
				gen_add_imm(ea_reg,(Bit32u)imm);
				seg_base=DRC_SEG_SS;
			}
			break;
		case 7: // BX
			gen_mov_word_to_reg(ea_reg,DRCD_REG(DRC_REG_EBX),true);
			if (imm) gen_add_imm(ea_reg,(Bit32u)imm);
			break;
		}
		// zero out the high 16bit so ea_reg can be used as full register
		gen_extend_word(false,ea_reg);
skip_extend_word:
		if (addseg) {
			// add the physical segment value if requested
			gen_add(ea_reg,DRCD_SEG_PHYS(decode.seg_prefix_used ? decode.seg_prefix : seg_base));
		}
	} else {
		Bits imm=0;
		Bit8u base_reg;
		Bit8u scaled_reg;
		Bitu scale=0;
		switch (decode.modrm.rm) {
		case 0:base_reg=DRC_REG_EAX;break;
		case 1:base_reg=DRC_REG_ECX;break;
		case 2:base_reg=DRC_REG_EDX;break;
		case 3:base_reg=DRC_REG_EBX;break;
		case 4:	// SIB
			{
				Bitu sib=decode_fetchb();
				bool scaled_reg_used=false;
				static Bit8u scaledtable[8]={
					DRC_REG_EAX,DRC_REG_ECX,DRC_REG_EDX,DRC_REG_EBX,
							0,DRC_REG_EBP,DRC_REG_ESI,DRC_REG_EDI
				};
				// see if scaling should be used and which register is to be scaled in this case
				if (((sib >> 3) &7)!=4) scaled_reg_used=true;
				scaled_reg=scaledtable[(sib >> 3) &7];
				scale=(sib >> 6);

				switch (sib & 7) {
				case 0:base_reg=DRC_REG_EAX;break;
				case 1:base_reg=DRC_REG_ECX;break;
				case 2:base_reg=DRC_REG_EDX;break;
				case 3:base_reg=DRC_REG_EBX;break;
				case 4:base_reg=DRC_REG_ESP;seg_base=DRC_SEG_SS;break;
				case 5:
					if (decode.modrm.mod) {
						base_reg=DRC_REG_EBP;seg_base=DRC_SEG_SS;
					} else {
						// no basereg, maybe scalereg
						Bitu val;
						// try to get a pointer to the next dword code position
						if (decode_fetchd_imm(val)) {
							// succeeded, use the pointer to avoid code invalidation
							if (!addseg) {
								if (!scaled_reg_used) {
									gen_mov_word_to_reg(ea_reg,(void*)val,true);
								} else {
									dyn_lea(ea_reg,NULL,DRCD_REG(scaled_reg),scale,0);
									gen_add(ea_reg,(void*)val);
								}
							} else {
								if (!scaled_reg_used) {
									gen_mov_word_to_reg(ea_reg,DRCD_SEG_PHYS(decode.seg_prefix_used ? decode.seg_prefix : seg_base),true);
								} else {
									dyn_lea(ea_reg,DRCD_SEG_PHYS(decode.seg_prefix_used ? decode.seg_prefix : seg_base),DRCD_REG(scaled_reg),scale,0);
								}
								gen_add(ea_reg,(void*)val);
							}
							return;
						}
						// couldn't get a pointer, use the current value
						imm=(Bit32s)val;

						if (!addseg) {
							if (!scaled_reg_used) {
								gen_mov_dword_to_reg_imm(ea_reg,(Bit32u)imm);
							} else {
								dyn_lea(ea_reg,NULL,DRCD_REG(scaled_reg),scale,imm);
							}
						} else {
							if (!scaled_reg_used) {
								gen_mov_word_to_reg(ea_reg,DRCD_SEG_PHYS(decode.seg_prefix_used ? decode.seg_prefix : seg_base),true);
								if (imm) gen_add_imm(ea_reg,(Bit32u)imm);
							} else {
								dyn_lea(ea_reg,DRCD_SEG_PHYS(decode.seg_prefix_used ? decode.seg_prefix : seg_base),DRCD_REG(scaled_reg),scale,imm);
							}
						}

						return;
					}
					break;
				case 6:base_reg=DRC_REG_ESI;break;
				case 7:base_reg=DRC_REG_EDI;break;
				}
				// basereg, maybe scalereg
				switch (decode.modrm.mod) {
				case 1:
					imm=(Bit8s)decode_fetchb();
					break;
				case 2: {
					Bitu val;
					// try to get a pointer to the next dword code position
					if (decode_fetchd_imm(val)) {
						// succeeded, use the pointer to avoid code invalidation
						if (!addseg) {
							if (!scaled_reg_used) {
								gen_mov_word_to_reg(ea_reg,DRCD_REG(base_reg),true);
								gen_add(ea_reg,(void*)val);
							} else {
								dyn_lea(ea_reg,DRCD_REG(base_reg),DRCD_REG(scaled_reg),scale,0);
								gen_add(ea_reg,(void*)val);
							}
						} else {
							if (!scaled_reg_used) {
								gen_mov_word_to_reg(ea_reg,DRCD_SEG_PHYS(decode.seg_prefix_used ? decode.seg_prefix : seg_base),true);
							} else {
								dyn_lea(ea_reg,DRCD_SEG_PHYS(decode.seg_prefix_used ? decode.seg_prefix : seg_base),DRCD_REG(scaled_reg),scale,0);
							}
							gen_add(ea_reg,DRCD_REG(base_reg));
							gen_add(ea_reg,(void*)val);
						}
						return;
					}
					// couldn't get a pointer, use the current value
					imm=(Bit32s)val;
					break;
					}
				}

				if (!addseg) {
					if (!scaled_reg_used) {
						gen_mov_word_to_reg(ea_reg,DRCD_REG(base_reg),true);
						gen_add_imm(ea_reg,(Bit32u)imm);
					} else {
						dyn_lea(ea_reg,DRCD_REG(base_reg),DRCD_REG(scaled_reg),scale,imm);
					}
				} else {
					if (!scaled_reg_used) {
						gen_mov_word_to_reg(ea_reg,DRCD_SEG_PHYS(decode.seg_prefix_used ? decode.seg_prefix : seg_base),true);
						gen_add(ea_reg,DRCD_REG(base_reg));
						if (imm) gen_add_imm(ea_reg,(Bit32u)imm);
					} else {
						dyn_lea(ea_reg,DRCD_SEG_PHYS(decode.seg_prefix_used ? decode.seg_prefix : seg_base),DRCD_REG(scaled_reg),scale,imm);
						gen_add(ea_reg,DRCD_REG(base_reg));
					}
				}

				return;
			}	
			break;	// SIB Break
		case 5:
			if (decode.modrm.mod) {
				base_reg=DRC_REG_EBP;seg_base=DRC_SEG_SS;
			} else {
				// no base, no scalereg

				imm=(Bit32s)decode_fetchd();
				if (!addseg) {
					gen_mov_dword_to_reg_imm(ea_reg,(Bit32u)imm);
				} else {
					gen_mov_word_to_reg(ea_reg,DRCD_SEG_PHYS(decode.seg_prefix_used ? decode.seg_prefix : seg_base),true);
					if (imm) gen_add_imm(ea_reg,(Bit32u)imm);
				}

				return;
			}
			break;
		case 6:base_reg=DRC_REG_ESI;break;
		case 7:base_reg=DRC_REG_EDI;break;
		}

		// no scalereg, but basereg

		switch (decode.modrm.mod) {
		case 1:
			imm=(Bit8s)decode_fetchb();
			break;
		case 2: {
			Bitu val;
			// try to get a pointer to the next dword code position
			if (decode_fetchd_imm(val)) {
				// succeeded, use the pointer to avoid code invalidation
				if (!addseg) {
					gen_mov_word_to_reg(ea_reg,DRCD_REG(base_reg),true);
					gen_add(ea_reg,(void*)val);
				} else {
					gen_mov_word_to_reg(ea_reg,DRCD_SEG_PHYS(decode.seg_prefix_used ? decode.seg_prefix : seg_base),true);
					gen_add(ea_reg,DRCD_REG(base_reg));
					gen_add(ea_reg,(void*)val);
				}
				return;
			}
			// couldn't get a pointer, use the current value
			imm=(Bit32s)val;
			break;
			}
		}

		if (!addseg) {
			gen_mov_word_to_reg(ea_reg,DRCD_REG(base_reg),true);
			if (imm) gen_add_imm(ea_reg,(Bit32u)imm);
		} else {
			gen_mov_word_to_reg(ea_reg,DRCD_SEG_PHYS(decode.seg_prefix_used ? decode.seg_prefix : seg_base),true);
			gen_add(ea_reg,DRCD_REG(base_reg));
			if (imm) gen_add_imm(ea_reg,(Bit32u)imm);
		}
	}
}



// add code that checks if port access is allowed
// the port is given in a register
static void dyn_add_iocheck(HostReg reg_port,Bitu access_size) {
	if (cpu.pmode) {
		gen_call_function_RI((void *)&CPU_IO_Exception,reg_port,access_size);
		dyn_check_exception(FC_RETOP);
	}
}

// add code that checks if port access is allowed
// the port is a constant
static void dyn_add_iocheck_var(Bit8u accessed_port,Bitu access_size) {
	if (cpu.pmode) {
		gen_call_function_II((void *)&CPU_IO_Exception,accessed_port,access_size);
		dyn_check_exception(FC_RETOP);
	}
}



// save back the address register
static void gen_protect_addr_reg(void) {
	gen_mov_word_from_reg(FC_ADDR,&core_dynrec.protected_regs[FC_ADDR],true);
}

// restore the address register
static void gen_restore_addr_reg(void) {
	gen_mov_word_to_reg(FC_ADDR,&core_dynrec.protected_regs[FC_ADDR],true);
}

// save back an arbitrary register
static void gen_protect_reg(HostReg reg) {
	gen_mov_word_from_reg(reg,&core_dynrec.protected_regs[reg],true);
}

// restore an arbitrary register
static void gen_restore_reg(HostReg reg) {
	gen_mov_word_to_reg(reg,&core_dynrec.protected_regs[reg],true);
}

// restore an arbitrary register into a different register
static void gen_restore_reg(HostReg reg,HostReg dest_reg) {
	gen_mov_word_to_reg(dest_reg,&core_dynrec.protected_regs[reg],true);
}



#include "lazyflags.h"

// flags optimization functions
// they try to find out if a function can be replaced by another
// one that does not generate any flags at all

static Bitu mf_functions_num=0;
static struct {
	DRC_PTR_SIZE_IM pos;
	Bit32u fct_ptr;
} mf_functions[64];

static void InitFlagsOptimization(void) {
	mf_functions_num=0;
}

// replace all queued functions with their simpler variants
// because the current instruction destroys all condition flags and
// the flags are not required before
static void InvalidateFlags(void) {
#ifdef DRC_FLAGS_INVALIDATION
	for (Bitu ct=0; ct<mf_functions_num; ct++) {
		*(Bit32u*)(mf_functions[ct].pos)=(Bit32u)(mf_functions[ct].fct_ptr);
	}
	mf_functions_num=0;
#endif
}

// replace all queued functions with their simpler variants
// because the current instruction destroys all condition flags and
// the flags are not required before
static void InvalidateFlags(void* current_simple_function) {
#ifdef DRC_FLAGS_INVALIDATION
	for (Bitu ct=0; ct<mf_functions_num; ct++) {
		*(Bit32u*)(mf_functions[ct].pos)=(Bit32u)(mf_functions[ct].fct_ptr);
	}
	mf_functions_num=1;
	mf_functions[0].pos=(DRC_PTR_SIZE_IM)cache.pos+1;
	mf_functions[0].fct_ptr=(Bit32u)current_simple_function - (Bit32u)mf_functions[0].pos-4;
#endif
}

// enqueue this instruction, if later an instruction is encountered that
// destroys all condition flags and the flags weren't needed in-between
// this function can be replaced by a simpler one as well
static void InvalidateFlagsPartially(void* current_simple_function) {
#ifdef DRC_FLAGS_INVALIDATION
	mf_functions[mf_functions_num].pos=(DRC_PTR_SIZE_IM)cache.pos+1;
	mf_functions[mf_functions_num].fct_ptr=(Bit32u)current_simple_function - (Bit32u)mf_functions[mf_functions_num].pos-4;
	mf_functions_num++;
#endif
}

// enqueue this instruction, if later an instruction is encountered that
// destroys all condition flags and the flags weren't needed in-between
// this function can be replaced by a simpler one as well
static void InvalidateFlagsPartially(void* current_simple_function,DRC_PTR_SIZE_IM cpos) {
#ifdef DRC_FLAGS_INVALIDATION
	mf_functions[mf_functions_num].pos=cpos;
	mf_functions[mf_functions_num].fct_ptr=(Bit32u)current_simple_function - (Bit32u)mf_functions[mf_functions_num].pos-4;
	mf_functions_num++;
#endif
}

// the current function needs the condition flags thus reset the queue
static void AcquireFlags(Bitu flags_mask) {
#ifdef DRC_FLAGS_INVALIDATION
	mf_functions_num=0;
#endif
}
