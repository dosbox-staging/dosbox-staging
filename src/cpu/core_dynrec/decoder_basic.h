// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "../string_ops.h"

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
	SOP_NOT,SOP_NEG
};

// instructions that use two operand
enum DualOps {
	DOP_ADD,DOP_ADC,
	DOP_SUB,DOP_SBB,
	DOP_CMP,DOP_XOR,
	DOP_AND,DOP_OR,
	DOP_TEST,
	DOP_MOV,
	DOP_XCHG
};

// shift and rotate functions
enum ShiftOps {
	SHIFT_ROL,SHIFT_ROR,
	SHIFT_RCL,SHIFT_RCR,
	SHIFT_SHL,SHIFT_SHR,
	SHIFT_SAL,SHIFT_SAR
};

// branch conditions
enum BranchTypes {
	BR_O,BR_NO,BR_B,BR_NB,
	BR_Z,BR_NZ,BR_BE,BR_NBE,
	BR_S,BR_NS,BR_P,BR_NP,
	BR_L,BR_NL,BR_LE,BR_NLE
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
	grp2_1,grp2_imm,grp2_cl
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
	uint8_t seg_prefix;		// segment prefix (if seg_prefix_used==true)

	// block that contains the first instruction translated
	CacheBlock *block;
	// block that contains the current byte of the instruction stream
	CacheBlock *active_block;

	// the active page (containing the current byte of the instruction stream)
	struct {
		CodePageHandler *code;
		Bitu index;		// index to the current byte of the instruction stream
		uint8_t * wmap;	// write map that indicates code presence for every byte of this page
		uint8_t * invmap;	// invalidation map
		Bitu first;		// page number 
	} page;

	// modrm state of the current instruction (if used)
	struct {
		uint_fast8_t val;
		uint_fast8_t mod;
		uint_fast8_t rm;
		uint_fast8_t reg;
	} modrm;
} decode;

static bool MakeCodePage(Bitu lin_addr, CodePageHandler *&cph)
{
	uint8_t rdval;
	const Bitu cflag = cpu.code.big ? PFLAG_HASCODE32:PFLAG_HASCODE16;
	//Ensure page contains memory:
	if (mem_readb_checked(lin_addr, &rdval)) {
		return true;
	}

	PageHandler * handler=get_tlb_readhandler(lin_addr);
	if (handler->flags & PFLAG_HASCODE) {
		// this is a codepage handler, make sure it matches current code size
		cph = static_cast<CodePageHandler *>(handler);
		if (CPU_ReuseCodepages || (handler->flags & cflag)) {
			return false;
		}
		// wrong code size/stale dynamic code, drop it
		cph->ClearRelease();
		cph = nullptr;
		// handler was changed, refresh
		handler = get_tlb_readhandler(lin_addr);
	}
	if (handler->flags & PFLAG_NOCODE) {
		if (PAGING_ForcePageInit(lin_addr)) {
			handler=get_tlb_readhandler(lin_addr);
			if (handler->flags & PFLAG_HASCODE) {
				cph = (CodePageHandler *)handler;
				if (handler->flags & cflag) return false;
				cph->ClearRelease();
				cph = nullptr;
				handler=get_tlb_readhandler(static_cast<PhysPt>(lin_addr));
			}
		}
		if (handler->flags & PFLAG_NOCODE) {
			LOG_MSG("DYNREC:Can't run code in this page");
			cph = nullptr;
			return false;
		}
	} 
	Bitu lin_page=lin_addr>>12;
	Bitu phys_page=lin_page;
	// find the physical page that the linear page is mapped to
	if (!PAGING_MakePhysPage(phys_page)) {
		LOG_MSG("DYNREC:Can't find physpage");
		cph = nullptr;
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
	CodePageHandler *cpagehandler = cache.free_pages;
	cache.free_pages=cache.free_pages->next;

	// adjust previous and next page pointer
	cpagehandler->prev=cache.last_page;
	cpagehandler->next=nullptr;
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
	++decode.page.first;
	Bitu faddr=decode.page.first << 12;
	mem_readb(faddr);
	MakeCodePage(faddr,decode.page.code);
	CacheBlock *newblock = cache_getblock();
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
static uint8_t decode_fetchb(void) {
	if (decode.page.index >= 4096) {
		decode_advancepage();
	}
	decode.page.wmap[decode.page.index]+=0x01;
	++decode.page.index;
	++decode.code;
	return mem_readb(decode.code-1);
}
// fetch the next word of the instruction stream
static uint16_t decode_fetchw(void) {
	if (decode.page.index >= 4095) {
		uint16_t val = decode_fetchb();
		val |= decode_fetchb() << 8;
		return val;
	}
	add_to_unaligned_uint16(&decode.page.wmap[decode.page.index], 0x0101);
	decode.code+=2;decode.page.index+=2;
	return mem_readw(decode.code-2);
}
// fetch the next dword of the instruction stream
static uint32_t decode_fetchd(void) {
	if (decode.page.index >= 4093) {
		uint32_t val = decode_fetchb();
		val |= decode_fetchb() << 8;
		val |= decode_fetchb() << 16;
		val|=decode_fetchb() << 24;
		return val;
        /* Advance to the next page */
	}
	add_to_unaligned_uint32(&decode.page.wmap[decode.page.index], 0x01010101);
	decode.code+=4;decode.page.index+=4;
	return mem_readd(decode.code-4);
}

// fetch a byte, val points to the code location if possible,
// otherwise val contains the current value read from the position
static bool decode_fetchb_imm(Bitu & val) {
	if (decode.page.index >= 4096) {
		decode_advancepage();
	}
	// see if position is directly accessible
	if (decode.page.invmap != nullptr) {
		if (decode.page.invmap[decode.page.index] == 0) {
			// position not yet modified
			val=(uint32_t)decode_fetchb();
			return false;
		}

		HostPt tlb_addr=get_tlb_read(decode.code);
		if (tlb_addr) {
			val=(Bitu)(tlb_addr+decode.code);
			decode.active_block->cache.AddByteToWriteMaskAt(decode.page.index);
			++decode.code;
			++decode.page.index;
			return true;
		}
	}
	// first time decoding or not directly accessible, just fetch the value
	val=(uint32_t)decode_fetchb();
	return false;
}

// fetch a word, val points to the code location if possible,
// otherwise val contains the current value read from the position
static bool decode_fetchw_imm(Bitu & val) {
	if (decode.page.index<4095) {
		if (decode.page.invmap != nullptr) {
			if ((decode.page.invmap[decode.page.index] == 0) &&
				(decode.page.invmap[decode.page.index + 1] == 0)) {
				// position not yet modified
				val=decode_fetchw();
				return false;
			}

			HostPt tlb_addr=get_tlb_read(decode.code);
			// see if position is directly accessible
			if (tlb_addr) {
				val=(Bitu)(tlb_addr+decode.code);
				decode.active_block->cache.AddWordToWriteMaskAt(decode.page.index);
				decode.code += 2;
				decode.page.index += 2;
				return true;
			}
		}
	}
	// first time decoding or not directly accessible, just fetch the value
	val=decode_fetchw();
	return false;
}

// fetch a dword, val points to the code location if possible,
// otherwise val contains the current value read from the position
static bool decode_fetchd_imm(Bitu & val) {
	if (decode.page.index<4093) {
		if (decode.page.invmap != nullptr) {
			if ((decode.page.invmap[decode.page.index] == 0) &&
				(decode.page.invmap[decode.page.index + 1] == 0) &&
				(decode.page.invmap[decode.page.index + 2] == 0) &&
				(decode.page.invmap[decode.page.index + 3] == 0)) {
				// position not yet modified
				val=decode_fetchd();
				return false;
			}

			HostPt tlb_addr=get_tlb_read(decode.code);
			// see if position is directly accessible
			if (tlb_addr) {
				val=(Bitu)(tlb_addr+decode.code);
				decode.active_block->cache.AddDwordToWriteMaskAt(decode.page.index);
				decode.code += 4;
				decode.page.index += 4;
				return true;
			}
		}
	}
	// first time decoding or not directly accessible, just fetch the value
	val=decode_fetchd();
	return false;
}

// modrm decoding helper
static inline void dyn_get_modrm()
{
	decode.modrm.val = decode_fetchb();
	decode.modrm.mod = (decode.modrm.val >> 6) & 3;
	decode.modrm.reg = (decode.modrm.val >> 3) & 7;
	decode.modrm.rm  = (decode.modrm.val & 7);
}

#ifdef DRC_USE_SEGS_ADDR

#define MOV_SEG_VAL_TO_HOST_REG(host_reg, seg_index) gen_mov_seg16_to_reg(host_reg,(Bitu)(DRCD_SEG_VAL(seg_index)) - (Bitu)(&Segs))

#define MOV_SEG_PHYS_TO_HOST_REG(host_reg, seg_index) gen_mov_seg32_to_reg(host_reg,(Bitu)(DRCD_SEG_PHYS(seg_index)) - (Bitu)(&Segs))
#define ADD_SEG_PHYS_TO_HOST_REG(host_reg, seg_index) gen_add_seg32_to_reg(host_reg,(Bitu)(DRCD_SEG_PHYS(seg_index)) - (Bitu)(&Segs))

#else

#define MOV_SEG_VAL_TO_HOST_REG(host_reg, seg_index) gen_mov_word_to_reg(host_reg,DRCD_SEG_VAL(seg_index),false)

#define MOV_SEG_PHYS_TO_HOST_REG(host_reg, seg_index) gen_mov_word_to_reg(host_reg,DRCD_SEG_PHYS(seg_index),true)
#define ADD_SEG_PHYS_TO_HOST_REG(host_reg, seg_index) gen_add(host_reg,DRCD_SEG_PHYS(seg_index))

#endif


#ifdef DRC_USE_REGS_ADDR

#define MOV_REG_VAL_TO_HOST_REG(host_reg, reg_index) gen_mov_regval32_to_reg(host_reg,(Bitu)(DRCD_REG_VAL(reg_index)) - (Bitu)(&cpu_regs))
#define ADD_REG_VAL_TO_HOST_REG(host_reg, reg_index) gen_add_regval32_to_reg(host_reg,(Bitu)(DRCD_REG_VAL(reg_index)) - (Bitu)(&cpu_regs))

#define MOV_REG_WORD16_TO_HOST_REG(host_reg, reg_index) gen_mov_regval16_to_reg(host_reg,(Bitu)(DRCD_REG_WORD(reg_index,false)) - (Bitu)(&cpu_regs))
#define MOV_REG_WORD32_TO_HOST_REG(host_reg, reg_index) gen_mov_regval32_to_reg(host_reg,(Bitu)(DRCD_REG_WORD(reg_index,true)) - (Bitu)(&cpu_regs))
#define MOV_REG_WORD_TO_HOST_REG(host_reg, reg_index, dword) gen_mov_regword_to_reg(host_reg,(Bitu)(DRCD_REG_WORD(reg_index,dword)) - (Bitu)(&cpu_regs), dword)

#define MOV_REG_WORD16_FROM_HOST_REG(host_reg, reg_index) gen_mov_regval16_from_reg(host_reg,(Bitu)(DRCD_REG_WORD(reg_index,false)) - (Bitu)(&cpu_regs))
#define MOV_REG_WORD32_FROM_HOST_REG(host_reg, reg_index) gen_mov_regval32_from_reg(host_reg,(Bitu)(DRCD_REG_WORD(reg_index,true)) - (Bitu)(&cpu_regs))
#define MOV_REG_WORD_FROM_HOST_REG(host_reg, reg_index, dword) gen_mov_regword_from_reg(host_reg,(Bitu)(DRCD_REG_WORD(reg_index,dword)) - (Bitu)(&cpu_regs), dword)

#define MOV_REG_BYTE_TO_HOST_REG_LOW(host_reg, reg_index, high_byte) gen_mov_regbyte_to_reg_low(host_reg,(Bitu)(DRCD_REG_BYTE(reg_index,high_byte)) - (Bitu)(&cpu_regs))
#define MOV_REG_BYTE_TO_HOST_REG_LOW_CANUSEWORD(host_reg, reg_index, high_byte) gen_mov_regbyte_to_reg_low_canuseword(host_reg,(Bitu)(DRCD_REG_BYTE(reg_index,high_byte)) - (Bitu)(&cpu_regs))
#define MOV_REG_BYTE_FROM_HOST_REG_LOW(host_reg, reg_index, high_byte) gen_mov_regbyte_from_reg_low(host_reg,(Bitu)(DRCD_REG_BYTE(reg_index,high_byte)) - (Bitu)(&cpu_regs))

#else

#define MOV_REG_VAL_TO_HOST_REG(host_reg, reg_index) gen_mov_word_to_reg(host_reg,DRCD_REG_VAL(reg_index),true)
#define ADD_REG_VAL_TO_HOST_REG(host_reg, reg_index) gen_add(host_reg,DRCD_REG_VAL(reg_index))

#define MOV_REG_WORD16_TO_HOST_REG(host_reg, reg_index) gen_mov_word_to_reg(host_reg,DRCD_REG_WORD(reg_index,false),false)
#define MOV_REG_WORD32_TO_HOST_REG(host_reg, reg_index) gen_mov_word_to_reg(host_reg,DRCD_REG_WORD(reg_index,true),true)
#define MOV_REG_WORD_TO_HOST_REG(host_reg, reg_index, dword) gen_mov_word_to_reg(host_reg,DRCD_REG_WORD(reg_index,dword),dword)

#define MOV_REG_WORD16_FROM_HOST_REG(host_reg, reg_index) gen_mov_word_from_reg(host_reg,DRCD_REG_WORD(reg_index,false),false)
#define MOV_REG_WORD32_FROM_HOST_REG(host_reg, reg_index) gen_mov_word_from_reg(host_reg,DRCD_REG_WORD(reg_index,true),true)
#define MOV_REG_WORD_FROM_HOST_REG(host_reg, reg_index, dword) gen_mov_word_from_reg(host_reg,DRCD_REG_WORD(reg_index,dword),dword)

#define MOV_REG_BYTE_TO_HOST_REG_LOW(host_reg, reg_index, high_byte) gen_mov_byte_to_reg_low(host_reg,DRCD_REG_BYTE(reg_index,high_byte))
#define MOV_REG_BYTE_TO_HOST_REG_LOW_CANUSEWORD(host_reg, reg_index, high_byte) gen_mov_byte_to_reg_low_canuseword(host_reg,DRCD_REG_BYTE(reg_index,high_byte))
#define MOV_REG_BYTE_FROM_HOST_REG_LOW(host_reg, reg_index, high_byte) gen_mov_byte_from_reg_low(host_reg,DRCD_REG_BYTE(reg_index,high_byte))

#endif


#define DYN_LEA_MEM_MEM(ea_reg, op1, op2, scale, imm) dyn_lea_mem_mem(ea_reg,op1,op2,scale,imm)

#if defined(DRC_USE_REGS_ADDR) && defined(DRC_USE_SEGS_ADDR)

#define DYN_LEA_SEG_PHYS_REG_VAL(ea_reg, op1_index, op2_index, scale, imm) dyn_lea_segphys_regval(ea_reg,op1_index,op2_index,scale,imm)
#define DYN_LEA_REG_VAL_REG_VAL(ea_reg, op1_index, op2_index, scale, imm) dyn_lea_regval_regval(ea_reg,op1_index,op2_index,scale,imm)
#define DYN_LEA_MEM_REG_VAL(ea_reg, op1, op2_index, scale, imm) dyn_lea_mem_regval(ea_reg,op1,op2_index,scale,imm)

#elif defined(DRC_USE_REGS_ADDR)

#define DYN_LEA_SEG_PHYS_REG_VAL(ea_reg, op1_index, op2_index, scale, imm) dyn_lea_mem_regval(ea_reg,DRCD_SEG_PHYS(op1_index),op2_index,scale,imm)
#define DYN_LEA_REG_VAL_REG_VAL(ea_reg, op1_index, op2_index, scale, imm) dyn_lea_regval_regval(ea_reg,op1_index,op2_index,scale,imm)
#define DYN_LEA_MEM_REG_VAL(ea_reg, op1, op2_index, scale, imm) dyn_lea_mem_regval(ea_reg,op1,op2_index,scale,imm)

#elif defined(DRC_USE_SEGS_ADDR)

#define DYN_LEA_SEG_PHYS_REG_VAL(ea_reg, op1_index, op2_index, scale, imm) dyn_lea_segphys_mem(ea_reg,op1_index,DRCD_REG_VAL(op2_index),scale,imm)
#define DYN_LEA_REG_VAL_REG_VAL(ea_reg, op1_index, op2_index, scale, imm) dyn_lea_mem_mem(ea_reg,DRCD_REG_VAL(op1_index),DRCD_REG_VAL(op2_index),scale,imm)
#define DYN_LEA_MEM_REG_VAL(ea_reg, op1, op2_index, scale, imm) dyn_lea_mem_mem(ea_reg,op1,DRCD_REG_VAL(op2_index),scale,imm)

#else

#define DYN_LEA_SEG_PHYS_REG_VAL(ea_reg, op1_index, op2_index, scale, imm) dyn_lea_mem_mem(ea_reg,DRCD_SEG_PHYS(op1_index),DRCD_REG_VAL(op2_index),scale,imm)
#define DYN_LEA_REG_VAL_REG_VAL(ea_reg, op1_index, op2_index, scale, imm) dyn_lea_mem_mem(ea_reg,DRCD_REG_VAL(op1_index),DRCD_REG_VAL(op2_index),scale,imm)
#define DYN_LEA_MEM_REG_VAL(ea_reg, op1, op2_index, scale, imm) dyn_lea_mem_mem(ea_reg,op1,DRCD_REG_VAL(op2_index),scale,imm)

#endif



// adjust CPU_Cycles value
static void dyn_reduce_cycles(void) {
	if (!decode.cycles) {
		++decode.cycles;
	}
	gen_sub_direct_word(&CPU_Cycles,decode.cycles,true);
}


// set reg to the start of the next instruction
// set reg_eip to the start of the current instruction
static inline void dyn_set_eip_last_end(HostReg reg) {
	gen_mov_word_to_reg(reg,&reg_eip,true);
	gen_add_imm(reg,(uint32_t)(decode.code-decode.code_start));
	gen_add_direct_word(&reg_eip,decode.op_start-decode.code_start,decode.big_op);
}

// set reg_eip to the start of the current instruction
static inline void dyn_set_eip_last(void) {
	gen_add_direct_word(&reg_eip,decode.op_start-decode.code_start,cpu.code.big);
}

// set reg_eip to the start of the next instruction
static inline void dyn_set_eip_end(void) {
	gen_add_direct_word(&reg_eip,decode.code-decode.code_start,cpu.code.big);
}

// set reg_eip to the start of the next instruction plus an offset (imm)
static inline void dyn_set_eip_end(HostReg reg,uint32_t imm=0) {
	gen_mov_word_to_reg(reg,&reg_eip,true); //get_extend_word will mask off the upper bits
	//gen_mov_word_to_reg(reg,&reg_eip,decode.big_op);
	gen_add_imm(reg,(uint32_t)(decode.code-decode.code_start+imm));
}



// the following functions generate function calls
// parameters are loaded by generating code using gen_load_param_ which
// is architecture dependent
// R=host register; I=32bit immediate value; A=address value; m=memory

static inline const uint8_t* gen_call_function_R(void * func,Bitu op) {
	gen_load_param_reg(op,0);
	return gen_call_function_setup(func, 1);
}

static inline const uint8_t* gen_call_function_R3(void * func,Bitu op) {
	gen_load_param_reg(op,2);
	return gen_call_function_setup(func, 3, true);
}

static inline const uint8_t* gen_call_function_RI(void * func,Bitu op1,Bitu op2) {
	gen_load_param_imm(op2,1);
	gen_load_param_reg(op1,0);
	return gen_call_function_setup(func, 2);
}

static inline const uint8_t* gen_call_function_RA(void * func,Bitu op1,Bitu op2) {
	gen_load_param_addr(op2,1);
	gen_load_param_reg(op1,0);
	return gen_call_function_setup(func, 2);
}

static inline const uint8_t* gen_call_function_RR(void * func,Bitu op1,Bitu op2) {
	gen_load_param_reg(op2,1);
	gen_load_param_reg(op1,0);
	return gen_call_function_setup(func, 2);
}

static inline const uint8_t* gen_call_function_IR(void * func,Bitu op1,Bitu op2) {
	gen_load_param_reg(op2,1);
	gen_load_param_imm(op1,0);
	return gen_call_function_setup(func, 2);
}

static inline const uint8_t* gen_call_function_I(void * func,Bitu op) {
	gen_load_param_imm(op,0);
	return gen_call_function_setup(func, 1);
}

static inline const uint8_t* gen_call_function_II(void * func,Bitu op1,Bitu op2) {
	gen_load_param_imm(op2,1);
	gen_load_param_imm(op1,0);
	return gen_call_function_setup(func, 2);
}

static inline const uint8_t* gen_call_function_III(void * func,Bitu op1,Bitu op2,Bitu op3) {
	gen_load_param_imm(op3,2);
	gen_load_param_imm(op2,1);
	gen_load_param_imm(op1,0);
	return gen_call_function_setup(func, 3);
}

static inline const uint8_t* gen_call_function_IA(void * func,Bitu op1,Bitu op2) {
	gen_load_param_addr(op2,1);
	gen_load_param_imm(op1,0);
	return gen_call_function_setup(func, 2);
}

static inline const uint8_t* gen_call_function_IIR(void * func,Bitu op1,Bitu op2,Bitu op3) {
	gen_load_param_reg(op3,2);
	gen_load_param_imm(op2,1);
	gen_load_param_imm(op1,0);
	return gen_call_function_setup(func, 3);
}

static inline const uint8_t* gen_call_function_IIIR(void * func,Bitu op1,Bitu op2,Bitu op3,Bitu op4) {
	gen_load_param_reg(op4,3);
	gen_load_param_imm(op3,2);
	gen_load_param_imm(op2,1);
	gen_load_param_imm(op1,0);
	return gen_call_function_setup(func, 4);
}

static inline const uint8_t* gen_call_function_IRRR(void * func,Bitu op1,Bitu op2,Bitu op3,Bitu op4) {
	gen_load_param_reg(op4,3);
	gen_load_param_reg(op3,2);
	gen_load_param_reg(op2,1);
	gen_load_param_imm(op1,0);
	return gen_call_function_setup(func, 4);
}

static inline const uint8_t* gen_call_function_m(void * func,Bitu op) {
	gen_load_param_mem(op,2);
	return gen_call_function_setup(func, 3, true);
}

static inline const uint8_t* gen_call_function_mm(void * func,Bitu op1,Bitu op2) {
	gen_load_param_mem(op2,3);
	gen_load_param_mem(op1,2);
	return gen_call_function_setup(func, 4, true);
}



enum save_info_type {db_exception, cycle_check, string_break};


// function that is called on exceptions
static BlockReturn DynRunException(uint32_t eip_add,uint32_t cycle_sub) {
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
	const uint8_t* branch_pos;
	uint32_t eip_change;
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
			case db_exception:
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
	cache_block_before_close();
	cache_closeblock();
	cache_block_closing(decode.block->cache.start,decode.block->cache.size);
}


// add a check that can branch to the exception handling
static void dyn_check_exception(HostReg reg) {
	save_info_dynrec[used_save_info_dynrec].branch_pos=gen_create_branch_long_nonzero(reg,false);
	if (!decode.cycles) {
		++decode.cycles;
	}
	save_info_dynrec[used_save_info_dynrec].cycles=decode.cycles;
	// in case of an exception eip will point to the start of the current instruction
	save_info_dynrec[used_save_info_dynrec].eip_change=decode.op_start-decode.code_start;
	if (!cpu.code.big) save_info_dynrec[used_save_info_dynrec].eip_change&=0xffff;
	save_info_dynrec[used_save_info_dynrec].type=db_exception;
	++used_save_info_dynrec;
}

bool DRC_CALL_CONV mem_readb_checked_drc(PhysPt address) DRC_FC;
bool DRC_CALL_CONV mem_readb_checked_drc(PhysPt address) {
	HostPt tlb_addr=get_tlb_read(address);
	if (tlb_addr) {
		const uint8_t byte = host_readb(tlb_addr + address);
		memcpy(&core_dynrec.readdata, &byte, sizeof(byte));
		return false;
	} else {
		return get_tlb_readhandler(address)->readb_checked(address, (uint8_t*)(&core_dynrec.readdata));
	}
}

bool DRC_CALL_CONV mem_readw_checked_drc(PhysPt address) DRC_FC;
bool DRC_CALL_CONV mem_readw_checked_drc(PhysPt address) {
	if ((address & 0xfff)<0xfff) {
		HostPt tlb_addr=get_tlb_read(address);
		if (tlb_addr) {
			const uint16_t word = host_readw(tlb_addr + address);
			memcpy(&core_dynrec.readdata, &word, sizeof(word));
			return false;
		} else return get_tlb_readhandler(address)->readw_checked(address, (uint16_t*)(&core_dynrec.readdata));
	} else return mem_unalignedreadw_checked(address, ((uint16_t*)(&core_dynrec.readdata)));
}

bool DRC_CALL_CONV mem_readd_checked_drc(PhysPt address) DRC_FC;
bool DRC_CALL_CONV mem_readd_checked_drc(PhysPt address) {
	if ((address & 0xfff)<0xffd) {
		HostPt tlb_addr=get_tlb_read(address);
		if (tlb_addr) {
			const uint32_t dword = host_readd(tlb_addr + address);
			memcpy(&core_dynrec.readdata, &dword, sizeof(dword));
			return false;
		} else return get_tlb_readhandler(address)->readd_checked(address, (uint32_t*)(&core_dynrec.readdata));
	} else return mem_unalignedreadd_checked(address, ((uint32_t*)(&core_dynrec.readdata)));
}

bool DRC_CALL_CONV mem_writeb_checked_drc(PhysPt address, uint8_t val) DRC_FC;
bool DRC_CALL_CONV mem_writeb_checked_drc(PhysPt address, uint8_t val)
{
	HostPt tlb_addr = get_tlb_write(address);
	if (tlb_addr) {
		host_writeb(tlb_addr + address, val);
		return false;
	} else {
		return get_tlb_writehandler(address)->writeb_checked(address, val);
	}
}

bool DRC_CALL_CONV mem_writew_checked_drc(PhysPt address,uint16_t val) DRC_FC;
bool DRC_CALL_CONV mem_writew_checked_drc(PhysPt address,uint16_t val) {
	if ((address & 0xfff)<0xfff) {
		HostPt tlb_addr=get_tlb_write(address);
		if (tlb_addr) {
			host_writew(tlb_addr+address,val);
			return false;
		} else return get_tlb_writehandler(address)->writew_checked(address,val);
	} else return mem_unalignedwritew_checked(address,val);
}

bool DRC_CALL_CONV mem_writed_checked_drc(PhysPt address,uint32_t val) DRC_FC;
bool DRC_CALL_CONV mem_writed_checked_drc(PhysPt address,uint32_t val) {
	if ((address & 0xfff)<0xffd) {
		HostPt tlb_addr=get_tlb_write(address);
		if (tlb_addr) {
			host_writed(tlb_addr+address,val);
			return false;
		} else return get_tlb_writehandler(address)->writed_checked(address,val);
	} else return mem_unalignedwrited_checked(address,val);
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
[[maybe_unused]] static void dyn_lea_mem_mem(HostReg ea_reg,
                                         void *op1,
                                         void *op2,
                                         Bitu scale,
                                         Bits imm)
{
	if (scale || imm) {
		if (op1!=nullptr) {
			gen_mov_word_to_reg(ea_reg,op1,true);
			gen_mov_word_to_reg(TEMP_REG_DRC,op2,true);

			gen_lea(ea_reg,TEMP_REG_DRC,scale,imm);
		} else {
			gen_mov_word_to_reg(ea_reg,op2,true);
			gen_lea(ea_reg,scale,imm);
		}
	} else {
		gen_mov_word_to_reg(ea_reg,op2,true);
		if (op1!=nullptr) gen_add(ea_reg,op1);
	}
}

#ifdef DRC_USE_REGS_ADDR
// effective address calculation helper
// loads op1 into ea_reg and adds the scaled op2 and the immediate to it
// op1 is cpu_regs[op1_index], op2 is cpu_regs[op2_index] 
static void dyn_lea_regval_regval(HostReg ea_reg,Bitu op1_index,Bitu op2_index,Bitu scale,Bits imm) {
	if (scale || imm) {
		MOV_REG_VAL_TO_HOST_REG(ea_reg,op1_index);
		MOV_REG_VAL_TO_HOST_REG(TEMP_REG_DRC,op2_index);

		gen_lea(ea_reg,TEMP_REG_DRC,scale,imm);
	} else {
		MOV_REG_VAL_TO_HOST_REG(ea_reg,op2_index);
		ADD_REG_VAL_TO_HOST_REG(ea_reg,op1_index);
	}
}

// effective address calculation helper
// loads op1 into ea_reg and adds the scaled op2 and the immediate to it
// op2 is cpu_regs[op2_index] 
static void dyn_lea_mem_regval(HostReg ea_reg,void* op1,Bitu op2_index,Bitu scale,Bits imm) {
	if (scale || imm) {
		if (op1!=nullptr) {
			gen_mov_word_to_reg(ea_reg,op1,true);
			MOV_REG_VAL_TO_HOST_REG(TEMP_REG_DRC,op2_index);

			gen_lea(ea_reg,TEMP_REG_DRC,scale,imm);
		} else {
			MOV_REG_VAL_TO_HOST_REG(ea_reg,op2_index);
			gen_lea(ea_reg,scale,imm);
		}
	} else {
		MOV_REG_VAL_TO_HOST_REG(ea_reg,op2_index);
		if (op1!=nullptr) gen_add(ea_reg,op1);
	}
}
#endif

#ifdef DRC_USE_SEGS_ADDR
#ifdef DRC_USE_REGS_ADDR
// effective address calculation helper
// loads op1 into ea_reg and adds the scaled op2 and the immediate to it
// op1 is Segs[op1_index], op2 is cpu_regs[op2_index] 
static void dyn_lea_segphys_regval(HostReg ea_reg,Bitu op1_index,Bitu op2_index,Bitu scale,Bits imm) {
	if (scale || imm) {
		MOV_SEG_PHYS_TO_HOST_REG(ea_reg,op1_index);
		MOV_REG_VAL_TO_HOST_REG(TEMP_REG_DRC,op2_index);

		gen_lea(ea_reg,TEMP_REG_DRC,scale,imm);
	} else {
		MOV_REG_VAL_TO_HOST_REG(ea_reg,op2_index);
		ADD_SEG_PHYS_TO_HOST_REG(ea_reg,op1_index);
	}
}

#else

// effective address calculation helper, op2 has to be present!
// loads op1 into ea_reg and adds the scaled op2 and the immediate to it
// op1 is Segs[op1_index] 
static void dyn_lea_segphys_mem(HostReg ea_reg,Bitu op1_index,void* op2,Bitu scale,Bits imm) {
	if (scale || imm) {
		MOV_SEG_PHYS_TO_HOST_REG(ea_reg,op1_index);
		gen_mov_word_to_reg(TEMP_REG_DRC,op2,true);

		gen_lea(ea_reg,TEMP_REG_DRC,scale,imm);
	} else {
		gen_mov_word_to_reg(ea_reg,op2,true);
		ADD_SEG_PHYS_TO_HOST_REG(ea_reg,op1_index);
	}
}
#endif
#endif

// calculate the effective address and store it in ea_reg
static void dyn_fill_ea(HostReg ea_reg,bool addseg=true) {
	uint8_t seg_base=DRC_SEG_DS;
	if (!decode.big_addr) {
		Bits imm = 0;
		switch (decode.modrm.mod) {
		case 0:imm=0;break;
		case 1:imm=(int8_t)decode_fetchb();break;
		case 2:imm=(int16_t)decode_fetchw();break;
		}
		switch (decode.modrm.rm) {
		case 0:// BX+SI
			DYN_LEA_REG_VAL_REG_VAL(ea_reg,DRC_REG_EBX,DRC_REG_ESI,0,imm);
			break;
		case 1:// BX+DI
			DYN_LEA_REG_VAL_REG_VAL(ea_reg,DRC_REG_EBX,DRC_REG_EDI,0,imm);
			break;
		case 2:// BP+SI
			DYN_LEA_REG_VAL_REG_VAL(ea_reg,DRC_REG_EBP,DRC_REG_ESI,0,imm);
			seg_base=DRC_SEG_SS;
			break;
		case 3:// BP+DI
			DYN_LEA_REG_VAL_REG_VAL(ea_reg,DRC_REG_EBP,DRC_REG_EDI,0,imm);
			seg_base=DRC_SEG_SS;
			break;
		case 4:// SI
			MOV_REG_VAL_TO_HOST_REG(ea_reg,DRC_REG_ESI);
			if (imm) gen_add_imm(ea_reg,(uint32_t)imm);
			break;
		case 5:// DI
			MOV_REG_VAL_TO_HOST_REG(ea_reg,DRC_REG_EDI);
			if (imm) gen_add_imm(ea_reg,(uint32_t)imm);
			break;
		case 6:// imm/BP
			if (!decode.modrm.mod) {
				imm=decode_fetchw();
				gen_mov_dword_to_reg_imm(ea_reg,(uint32_t)imm);
				goto skip_extend_word;
			} else {
				MOV_REG_VAL_TO_HOST_REG(ea_reg,DRC_REG_EBP);
				gen_add_imm(ea_reg,(uint32_t)imm);
				seg_base=DRC_SEG_SS;
			}
			break;
		case 7: // BX
			MOV_REG_VAL_TO_HOST_REG(ea_reg,DRC_REG_EBX);
			if (imm) gen_add_imm(ea_reg,(uint32_t)imm);
			break;
		}
		// zero out the high 16bit so ea_reg can be used as full register
		gen_extend_word(false,ea_reg);
skip_extend_word:
		if (addseg) {
			// add the physical segment value if requested
			ADD_SEG_PHYS_TO_HOST_REG(ea_reg,(decode.seg_prefix_used ? decode.seg_prefix : seg_base));
		}
	} else {
		Bits imm = 0;
		uint8_t base_reg = 0;
		uint8_t scaled_reg = 0;
		Bitu scale = 0;
		switch (decode.modrm.rm) {
		case 0:base_reg=DRC_REG_EAX;break;
		case 1:base_reg=DRC_REG_ECX;break;
		case 2:base_reg=DRC_REG_EDX;break;
		case 3:base_reg=DRC_REG_EBX;break;
		case 4:	// SIB
			{
				Bitu sib=decode_fetchb();
				bool scaled_reg_used=false;
				static uint8_t scaledtable[8]={
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
									gen_mov_LE_word_to_reg(ea_reg,(void*)val,true);
								} else {
									DYN_LEA_MEM_REG_VAL(ea_reg,nullptr,scaled_reg,scale,0);
									gen_add_LE(ea_reg,(void*)val);
								}
							} else {
								if (!scaled_reg_used) {
									MOV_SEG_PHYS_TO_HOST_REG(ea_reg,(decode.seg_prefix_used ? decode.seg_prefix : seg_base));
								} else {
									DYN_LEA_SEG_PHYS_REG_VAL(ea_reg,(decode.seg_prefix_used ? decode.seg_prefix : seg_base),scaled_reg,scale,0);
								}
								gen_add_LE(ea_reg,(void*)val);
							}
							return;
						}
						// couldn't get a pointer, use the current value
						imm=(int32_t)val;

						if (!addseg) {
							if (!scaled_reg_used) {
								gen_mov_dword_to_reg_imm(ea_reg,(uint32_t)imm);
							} else {
								DYN_LEA_MEM_REG_VAL(ea_reg,nullptr,scaled_reg,scale,imm);
							}
						} else {
							if (!scaled_reg_used) {
								MOV_SEG_PHYS_TO_HOST_REG(ea_reg,(decode.seg_prefix_used ? decode.seg_prefix : seg_base));
								if (imm) gen_add_imm(ea_reg,(uint32_t)imm);
							} else {
								DYN_LEA_SEG_PHYS_REG_VAL(ea_reg,(decode.seg_prefix_used ? decode.seg_prefix : seg_base),scaled_reg,scale,imm);
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
					imm=(int8_t)decode_fetchb();
					break;
				case 2: {
					Bitu val;
					// try to get a pointer to the next dword code position
					if (decode_fetchd_imm(val)) {
						// succeeded, use the pointer to avoid code invalidation
						if (!addseg) {
							if (!scaled_reg_used) {
								MOV_REG_VAL_TO_HOST_REG(ea_reg,base_reg);
								gen_add_LE(ea_reg,(void*)val);
							} else {
								DYN_LEA_REG_VAL_REG_VAL(ea_reg,base_reg,scaled_reg,scale,0);
								gen_add_LE(ea_reg,(void*)val);
							}
						} else {
							if (!scaled_reg_used) {
								MOV_SEG_PHYS_TO_HOST_REG(ea_reg,(decode.seg_prefix_used ? decode.seg_prefix : seg_base));
							} else {
								DYN_LEA_SEG_PHYS_REG_VAL(ea_reg,(decode.seg_prefix_used ? decode.seg_prefix : seg_base),scaled_reg,scale,0);
							}
							ADD_REG_VAL_TO_HOST_REG(ea_reg,base_reg);
							gen_add_LE(ea_reg,(void*)val);
						}
						return;
					}
					// couldn't get a pointer, use the current value
					imm=(int32_t)val;
					break;
					}
				}

				if (!addseg) {
					if (!scaled_reg_used) {
						MOV_REG_VAL_TO_HOST_REG(ea_reg,base_reg);
						gen_add_imm(ea_reg,(uint32_t)imm);
					} else {
						DYN_LEA_REG_VAL_REG_VAL(ea_reg,base_reg,scaled_reg,scale,imm);
					}
				} else {
					if (!scaled_reg_used) {
						MOV_SEG_PHYS_TO_HOST_REG(ea_reg,(decode.seg_prefix_used ? decode.seg_prefix : seg_base));
						ADD_REG_VAL_TO_HOST_REG(ea_reg,base_reg);
						if (imm) gen_add_imm(ea_reg,(uint32_t)imm);
					} else {
						DYN_LEA_SEG_PHYS_REG_VAL(ea_reg,(decode.seg_prefix_used ? decode.seg_prefix : seg_base),scaled_reg,scale,imm);
						ADD_REG_VAL_TO_HOST_REG(ea_reg,base_reg);
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

				imm=(int32_t)decode_fetchd();
				if (!addseg) {
					gen_mov_dword_to_reg_imm(ea_reg,(uint32_t)imm);
				} else {
					MOV_SEG_PHYS_TO_HOST_REG(ea_reg,(decode.seg_prefix_used ? decode.seg_prefix : seg_base));
					if (imm) gen_add_imm(ea_reg,(uint32_t)imm);
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
			imm=(int8_t)decode_fetchb();
			break;
		case 2: {
			Bitu val;
			// try to get a pointer to the next dword code position
			if (decode_fetchd_imm(val)) {
				// succeeded, use the pointer to avoid code invalidation
				if (!addseg) {
					MOV_REG_VAL_TO_HOST_REG(ea_reg,base_reg);
					gen_add_LE(ea_reg,(void*)val);
				} else {
					MOV_SEG_PHYS_TO_HOST_REG(ea_reg,(decode.seg_prefix_used ? decode.seg_prefix : seg_base));
					ADD_REG_VAL_TO_HOST_REG(ea_reg,base_reg);
					gen_add_LE(ea_reg,(void*)val);
				}
				return;
			}
			// couldn't get a pointer, use the current value
			imm=(int32_t)val;
			break;
			}
		}

		if (!addseg) {
			MOV_REG_VAL_TO_HOST_REG(ea_reg,base_reg);
			if (imm) gen_add_imm(ea_reg,(uint32_t)imm);
		} else {
			MOV_SEG_PHYS_TO_HOST_REG(ea_reg,(decode.seg_prefix_used ? decode.seg_prefix : seg_base));
			ADD_REG_VAL_TO_HOST_REG(ea_reg,base_reg);
			if (imm) gen_add_imm(ea_reg,(uint32_t)imm);
		}
	}
}

// save back the address register
static void gen_protect_addr_reg(void) {
#ifdef DRC_PROTECT_ADDR_REG
	gen_mov_word_from_reg(FC_ADDR,&core_dynrec.protected_regs[FC_ADDR],true);
#endif
}

// restore the address register
static void gen_restore_addr_reg(void) {
#ifdef DRC_PROTECT_ADDR_REG
	gen_mov_word_to_reg(FC_ADDR,&core_dynrec.protected_regs[FC_ADDR],true);
#endif
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



// flags optimization functions
// they try to find out if a function can be replaced by another
// one that does not generate any flags at all

static Bitu mf_functions_num=0;
static struct {
	const uint8_t* pos;
	void* fct_ptr;
	Bitu ftype;
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
		gen_fill_function_ptr(mf_functions[ct].pos,mf_functions[ct].fct_ptr,mf_functions[ct].ftype);
	}
	mf_functions_num=0;
#endif
}

// replace all queued functions with their simpler variants
// because the current instruction destroys all condition flags and
// the flags are not required before
static void InvalidateFlags(void* current_simple_function,Bitu flags_type) {
#ifdef DRC_FLAGS_INVALIDATION
	for (Bitu ct=0; ct<mf_functions_num; ct++) {
		gen_fill_function_ptr(mf_functions[ct].pos,mf_functions[ct].fct_ptr,mf_functions[ct].ftype);
	}
	mf_functions_num=1;
	mf_functions[0].pos=cache.pos;
	mf_functions[0].fct_ptr=current_simple_function;
	mf_functions[0].ftype=flags_type;
#endif
}

// enqueue this instruction, if later an instruction is encountered that
// destroys all condition flags and the flags weren't needed in-between
// this function can be replaced by a simpler one as well
static void InvalidateFlagsPartially(void* current_simple_function,Bitu flags_type) {
#ifdef DRC_FLAGS_INVALIDATION
	mf_functions[mf_functions_num].pos=cache.pos;
	mf_functions[mf_functions_num].fct_ptr=current_simple_function;
	mf_functions[mf_functions_num].ftype=flags_type;
	++mf_functions_num;
#endif
}

// enqueue this instruction, if later an instruction is encountered that
// destroys all condition flags and the flags weren't needed in-between
// this function can be replaced by a simpler one as well
static void InvalidateFlagsPartially(void* current_simple_function,const uint8_t* cpos,Bitu flags_type) {
#ifdef DRC_FLAGS_INVALIDATION
	mf_functions[mf_functions_num].pos=cpos;
	mf_functions[mf_functions_num].fct_ptr=current_simple_function;
	mf_functions[mf_functions_num].ftype=flags_type;
	++mf_functions_num;
#endif
}

// the current function needs the condition flags thus reset the queue
static void AcquireFlags([[maybe_unused]] Bitu flags_mask) {
#ifdef DRC_FLAGS_INVALIDATION
	mf_functions_num=0;
#endif
}
