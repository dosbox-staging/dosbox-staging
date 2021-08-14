/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2021  The DOSBox Staging Team
 *  Copyright (C) 2002-2019  The DOSBox Team
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

/* PPC64LE/OpenPOWER (little endian) backend */

// debugging
#define __ASSERT(x,...) \
    { if(!(x)) { fprintf(stderr, "ASSERT:" __VA_ARGS__); asm("trap\n"); } }

// some configuring defines that specify the capabilities of this architecture
// or aspects of the recompiling

// protect FC_ADDR over function calls if necessaray
//#define DRC_PROTECT_ADDR_REG

// try to use non-flags generating functions if possible
#define DRC_FLAGS_INVALIDATION
// try to replace _simple functions by code
#define DRC_FLAGS_INVALIDATION_DCODE

// type with the same size as a pointer
#define DRC_PTR_SIZE_IM Bit64u

// calling convention modifier
#define DRC_FC /* nothing */
#define DRC_CALL_CONV /* nothing */

#define DRC_USE_REGS_ADDR
#define DRC_USE_SEGS_ADDR

// register mapping
enum HostReg {
	HOST_R0 = 0,
	HOST_R1,
	HOST_R2,
	HOST_R3,
	HOST_R4,
	HOST_R5,
	HOST_R6,
	HOST_R7,
	HOST_R8,
	HOST_R9,
	HOST_R10,
	HOST_R11,
	HOST_R12, // end of volatile registers. use for CTR calls
	HOST_R13,
	HOST_R14,
	HOST_R15,
	HOST_R16,
	HOST_R17,
	HOST_R18,
	HOST_R19,
	HOST_R20,
	HOST_R21,
	HOST_R22,
	HOST_R23,
	HOST_R24,
	HOST_R25,
	HOST_R26, // generic non-volatile (used for inline adc/sbb)
	HOST_R27, // points to current CacheBlock (decode.block)
	HOST_R28, // points to fpu
	HOST_R29, // FC_ADDR
	HOST_R30, // points to Segs
	HOST_R31, // points to cpu_regs

	HOST_NONE
};

static const HostReg RegParams[] = {
	HOST_R3, HOST_R4, HOST_R5, HOST_R6,
	HOST_R7, HOST_R8, HOST_R9, HOST_R10
};

#if C_FPU
#include "fpu.h"
extern FPU_rec fpu;
#endif

// register that holds function return values
#define FC_RETOP HOST_R3

// register used for address calculations, if the ABI does not
// state that this register is preserved across function calls
// then define DRC_PROTECT_ADDR_REG above
#define FC_ADDR HOST_R29

// register that points to Segs[]
#define FC_SEGS_ADDR HOST_R30
// register that points to cpu_regs[]
#define FC_REGS_ADDR HOST_R31

// register that holds the first parameter
#define FC_OP1 RegParams[0]

// register that holds the second parameter
#define FC_OP2 RegParams[1]

// special register that holds the third parameter for _R3 calls (byte accessible)
#define FC_OP3 RegParams[2]

// register that holds byte-accessible temporary values
#define FC_TMP_BA1 FC_OP2

// register that holds byte-accessible temporary values
#define FC_TMP_BA2 FC_OP1

// temporary register for LEA
#define TEMP_REG_DRC HOST_R10

// op comes right out of the PowerISA 3.0 documentation
#define IMM(op, regsd, rega, imm)            (Bit32u)(((op)<<26)|((regsd)<<21)|((rega)<<16)|             (((Bit64u)(imm))&0xFFFF))
#define DSF(op, regs, rega, ds, bb)          (Bit32u)(((op)<<26)|((regs) <<21)|((rega)<<16)|             (((Bit64u)(ds))&0xFFFC)|(bb))
#define EXT(regsd, rega, regb, op, rc)       (Bit32u)(  (31<<26)|((regsd)<<21)|((rega)<<16)| ((regb)<<11)|                       ((op)<<1)              |(rc))
#define RLW(op, regs, rega, sh, mb, me, rc)  (Bit32u)(((op)<<26)|((regs) <<21)|((rega)<<16)|   ((sh)<<11)|((mb   )<<6)|((me)<<1)                        |(rc))
#define RLD(op, regs, rega, sh, mx, opb, rc) (Bit32u)(((op)<<26)|((regs) <<21)|((rega)<<16)|((sh&31)<<11)|((mx&31)<<6)|(mx&32)  |((opb)<<2)|((sh&32)>>4)|(rc))

#define IMM_OP(op, regsd, rega, imm)                cache_addd(IMM(op, regsd, rega, imm))
#define DSF_OP(op, regs, rega, ds, bb)              cache_addd(DSF(op, regs, rega, ds, bb))
#define EXT_OP(regsd, rega, regb, op, rc)           cache_addd(EXT(regsd, rega, regb, op, rc))
#define RLW_OP(op, regs, rega, sh, mb, me, rc)      cache_addd(RLW(op, regs, rega, sh, mb, me, rc))
#define RLD_OP(op, regs, rega, sh, mx, opb, rc)     cache_addd(RLD(op, regs, rega, sh, mx, opb, rc))

#define NOP                                         IMM(24, 0, 0, 0) // or 0,0,0
#define NOP_OP()                                    cache_addd(NOP)
#define TRAP()                                      cache_addd(EXT(31, 0, 0, 4, 0)) // tw 31,0,0 

// move a full register from reg_src to reg_dst
// truncate to 32-bits (matches x86_64, which uses 32-bit mov)
static void gen_mov_regs(HostReg reg_dst, HostReg reg_src)
{
// rld* etc. are backwards: rS is first in the encoding
// always move, even if reg_src == reg_dst, because we may need truncation
	RLD_OP(30, reg_src, reg_dst, 0, 32, 0, 0); // clrldi dst, src, 32
}

// move a 16bit constant value into dest_reg
// the upper 16bit of the destination register may be destroyed
static void gen_mov_word_to_reg_imm(HostReg dest_reg, Bit16u imm)
{
	IMM_OP(14, dest_reg, 0, imm); // li dest,imm
}

DRC_PTR_SIZE_IM block_ptr;

// Helper for loading addresses
// Emits relevant code to load the upper 48 bits if needed
static HostReg INLINE gen_addr(Bit64s &addr, HostReg dest)
{
	Bit64s off;

	if ((Bit16s)addr == addr)
		return HOST_R0; // lower to immediate

	off = addr - (Bit64s)&Segs;
	if ((Bit16s)off == off)
	{
		addr = off;
		return FC_SEGS_ADDR;
	}

	off = addr - (Bit64s)&cpu_regs;
	if ((Bit16s)off == off)
	{
		addr = off;
		return FC_REGS_ADDR;
	}

	off = addr - (Bit64s)block_ptr;
	if ((Bit16s)off == off)
	{
		addr = off;
		return HOST_R27;
	}

#if C_FPU
	off = addr - (Bit64s)&fpu;
	if ((Bit16s)off == off)
	{
		addr = off;
		return HOST_R28;
	}
#endif

	if (addr & 0xffffffff00000000) {
		IMM_OP(15, dest, 0, (addr & 0xffff000000000000) >> 48); // lis dest, upper
		if (addr & 0x0000ffff00000000)
			IMM_OP(24, dest, dest, (addr & 0x0000ffff00000000) >> 32); // ori dest, dest, ...
		RLD_OP(30, dest, dest, 32, 31, 1, 0); // rldicr dest, dest, 32, 31
		if (addr & 0x00000000ffff0000)
			IMM_OP(25, dest, dest, (addr & 0x00000000ffff0000) >> 16); // oris dest, dest, ...
	} else {
		IMM_OP(15, dest, 0, (addr & 0x00000000ffff0000) >> 16); // lis dest, lower
	}
	// watch unexpected sign extension with following instructions
	if (addr & 0x8000) {
		// make the displacement in the following instruction 0 for safety
		IMM_OP(24, dest, dest, (addr & 0x000000000000ffff));
		addr = 0;
	} else {
		addr = (Bit16s)addr;
	}
	return dest;
}

// move a 64bit constant value into dest_reg
static void gen_mov_qword_to_reg_imm(HostReg dest_reg, Bit64u imm)
{
	if (imm & 0xffffffff00000000) {
		IMM_OP(15, dest_reg, 0, (imm & 0xffff000000000000) >> 48); // lis dest, upper
		if (imm & 0x0000ffff00000000)
			IMM_OP(24, dest_reg, dest_reg, (imm & 0x0000ffff00000000) >> 32); // ori dest, dest, ...
		RLD_OP(30, dest_reg, dest_reg, 32, 31, 1, 0); // rldicr dest, dest, 32, 31
		if (imm & 0x00000000ffff0000)
			IMM_OP(25, dest_reg, dest_reg, (imm & 0x00000000ffff0000) >> 16); // oris dest, dest, ...
	} else {
		IMM_OP(15, dest_reg, 0, (imm & 0x00000000ffff0000) >> 16); // lis dest, lower
	}
	if (imm & 0xffff)
		IMM_OP(24, dest_reg, dest_reg, (imm & 0x000000000000ffff)); // ori dest, dest, ...
}

// move a 32bit constant value into dest_reg
static void gen_mov_dword_to_reg_imm(HostReg dest_reg, uint32_t imm)
{
	if (static_cast<int16_t>(imm) != static_cast<int32_t>(imm)) {
		IMM_OP(15, dest_reg, 0, (imm & 0xffff0000) >> 16); // lis
		if (imm & 0x0000ffff)
			IMM_OP(24, dest_reg, dest_reg, (imm & 0x0000ffff)); // ori
	} else {
		IMM_OP(14, dest_reg, 0, imm); // li
	}
}

// move a 32bit (dword==true) or 16bit (dword==false) value from memory into
// dest_reg 16bit moves may destroy the upper 16bit of the destination register
static void gen_mov_word_to_reg(HostReg dest_reg, void *data, bool dword)
{
	Bit64s addr = (Bit64s)data;
	HostReg ld = gen_addr(addr, dest_reg);
	IMM_OP(dword ? 32:40, dest_reg, ld, addr);  // lwz/lhz dest, addr@l(ld)
}

// move an 8bit constant value into dest_reg
// the upper 24bit of the destination register can be destroyed
// this function does not use FC_OP1/FC_OP2 as dest_reg as these
// registers might not be directly byte-accessible on some architectures
static void gen_mov_byte_to_reg_low_imm(HostReg dest_reg, Bit8u imm)
{
	gen_mov_word_to_reg_imm(dest_reg, imm);
}

// move an 8bit constant value into dest_reg
// the upper 24bit of the destination register can be destroyed
// this function can use FC_OP1/FC_OP2 as dest_reg which are
// not directly byte-accessible on some architectures
static void gen_mov_byte_to_reg_low_imm_canuseword(HostReg dest_reg, Bit8u imm)
{
	gen_mov_word_to_reg_imm(dest_reg, imm);
}

// move 32bit (dword==true) or 16bit (dword==false) of a register into memory
static void gen_mov_word_from_reg(HostReg src_reg, void *dest, bool dword)
{
	Bit64s addr = (Bit64s)dest;
	HostReg ld = gen_addr(addr, HOST_R8);
	IMM_OP(dword ? 36 : 44, src_reg, ld, addr);  // stw/sth src,addr@l(ld)
}

// move an 8bit value from memory into dest_reg
// the upper 24bit of the destination register can be destroyed
// this function does not use FC_OP1/FC_OP2 as dest_reg as these
// registers might not be directly byte-accessible on some architectures
static void gen_mov_byte_to_reg_low(HostReg dest_reg, void *data)
{
	Bit64s addr = (Bit64s)data;
	HostReg ld = gen_addr(addr, dest_reg);
	IMM_OP(34, dest_reg, ld, addr);  // lbz dest,addr@l(ld)
}

// move an 8bit value from memory into dest_reg
// the upper 24bit of the destination register can be destroyed
// this function can use FC_OP1/FC_OP2 as dest_reg which are
// not directly byte-accessible on some architectures
static void gen_mov_byte_to_reg_low_canuseword(HostReg dest_reg, void *data)
{
	gen_mov_byte_to_reg_low(dest_reg, data);
}

// move the lowest 8bit of a register into memory
static void gen_mov_byte_from_reg_low(HostReg src_reg, void *dest)
{
	Bit64s addr = (Bit64s)dest;
	HostReg ld = gen_addr(addr, HOST_R8);
	IMM_OP(38, src_reg, ld, addr);  // stb src_reg,addr@l(ld)
}

// convert an 8bit word to a 32bit dword
// the register is zero-extended (sign==false) or sign-extended (sign==true)
static void gen_extend_byte(bool sign, HostReg reg)
{
	if (sign)
		EXT_OP(reg, reg, 0, 954, 0); // extsb reg, src
	else
		RLW_OP(21, reg, reg, 0, 24, 31, 0); // rlwinm reg, src, 0, 24, 31
}

// convert a 16bit word to a 32bit dword
// the register is zero-extended (sign==false) or sign-extended (sign==true)
static void gen_extend_word(bool sign, HostReg reg)
{
	if (sign)
		EXT_OP(reg, reg, 0, 922, 0); // extsh reg, reg
	else
		RLW_OP(21, reg, reg, 0, 16, 31, 0); // rlwinm reg, reg, 0, 16, 31
}

// add a 32bit value from memory to a full register
static void gen_add(HostReg reg, void *op)
{
	gen_mov_word_to_reg(HOST_R8, op, true); // r8 = *(Bit32u*)op
	EXT_OP(reg,reg,HOST_R8,266,0);          // add reg,reg,r8
}

// add a 32bit constant value to a full register
static void gen_add_imm(HostReg reg, Bit32u imm)
{
    if (!imm) return;
	if ((Bit16s)imm != (Bit32s)imm)
		IMM_OP(15, reg, reg, (imm+0x8000)>>16); // addis reg,reg,imm@ha
	if ((Bit16s)imm)
		IMM_OP(14, reg, reg, imm);              // addi reg, reg, imm@l
}

// and a 32bit constant value with a full register
static void gen_and_imm(HostReg reg, Bit32u imm)
{
	Bits sbit,ebit,tbit,bbit,abit,i;

	// sbit = number of leading 0 bits
	// ebit = number of trailing 0 bits
	// tbit = number of total 0 bits
	// bbit = number of leading 1 bits
	// abit = number of trailing 1 bits

	if (imm == 0xFFFFFFFF)
		return;

	if (!imm)
		return gen_mov_word_to_reg_imm(reg, 0);

	sbit = ebit = tbit = bbit = abit = 0;
	for (i=0; i < 32; i++)
	{
		if (!(imm & (1<<(31-i))))
		{
			abit = 0;
			tbit++;
			if (sbit == i)
				sbit++;
			ebit++;
		}
		else
		{
			ebit = 0;
			if (bbit == i)
				bbit++;
			abit++;
		}
	}

	if (sbit + ebit == tbit)
	{
		RLW_OP(21,reg,reg,0,sbit,31-ebit,0); // rlwinm reg,reg,0,sbit,31-ebit
		return;
	}

	if (sbit >= 16)
	{
		IMM_OP(28,reg,reg,imm); // andi. reg,reg,imm
		return;
	}
	if (ebit >= 16)
	{
		IMM_OP(29,reg,reg,imm>>16); // andis. reg,reg,(imm>>16)
		return;
	}

	if (bbit + abit == (32 - tbit))
	{
		RLW_OP(21,reg,reg,0,32-abit,bbit-1,0); // rlwinm reg,reg,0,32-abit,bbit-1
		return;
	}

	IMM_OP(28, reg, HOST_R0, imm); // andi. r0, reg, imm@l
	IMM_OP(29, reg, reg, imm>16);  // andis. reg, reg, imm@h
	EXT_OP(reg, reg, HOST_R0, 444, 0); // or reg, reg, r0
}

// move a 32bit constant value into memory
static void gen_mov_direct_dword(void *dest, Bit32u imm)
{
	gen_mov_dword_to_reg_imm(HOST_R9, imm);
	gen_mov_word_from_reg(HOST_R9, dest, 1);
}

// move an address into memory (assumes address != NULL)
static void INLINE gen_mov_direct_ptr(void *dest, DRC_PTR_SIZE_IM imm)
{
	block_ptr = 0;
	gen_mov_qword_to_reg_imm(HOST_R27, imm);
	// this will be used to look-up the linked blocks
	block_ptr = imm;
	// "gen_mov_qword_from_reg(HOST_R27, dest, 1);"
	Bit64s addr = (Bit64s)dest;
	HostReg ld = gen_addr(addr, HOST_R8);
	DSF_OP(62, HOST_R27, ld, addr, 0); // std r27, addr@l(ld)
}

// add a 32bit (dword==true) or 16bit (dword==false) constant value
// to a 32bit memory value
static void gen_add_direct_word(void *dest, Bit32u imm, bool dword)
{
	HostReg ld;
	Bit64s addr = (Bit64s)dest;

	if (!dword)
	{
		imm &= 0xFFFF;
		//addr += 2; // ENDIAN!!!
	}

	if (!imm)
		return;

	ld = gen_addr(addr, HOST_R8);
	IMM_OP(dword ? 32 : 40, HOST_R9, ld, addr); // lwz/lhz r9, addr@l(ld)
	if (dword && (Bit16s)imm != (Bit32s)imm)
		IMM_OP(15, HOST_R9, HOST_R9, (imm+0x8000)>>16); // addis r9,r9,imm@ha
	if (!dword || (Bit16s)imm)
		IMM_OP(14, HOST_R9, HOST_R9, imm);      // addi r9,r9,imm@l
	IMM_OP(dword ? 36 : 44, HOST_R9, ld, addr); // stw/sth r9, addr@l(ld)
}

// subtract a 32bit (dword==true) or 16bit (dword==false) constant value
// from a 32-bit memory value
static void gen_sub_direct_word(void *dest, Bit32u imm, bool dword)
{
	gen_add_direct_word(dest, -(Bit32s)imm, dword);
}

// effective address calculation, destination is dest_reg
// scale_reg is scaled by scale (scale_reg*(2^scale)) and
// added to dest_reg, then the immediate value is added
static INLINE void gen_lea(HostReg dest_reg, HostReg scale_reg, Bitu scale, Bits imm)
{
	if (scale)
	{
		RLW_OP(21, scale_reg, HOST_R8, scale, 0, 31-scale, 0); // slwi scale_reg,r8,scale
		scale_reg = HOST_R8;
	}

	gen_add_imm(dest_reg, imm);
	EXT_OP(dest_reg, dest_reg, scale_reg, 266, 0); // add dest,dest,scaled
}

// effective address calculation, destination is dest_reg
// dest_reg is scaled by scale (dest_reg*(2^scale)),
// then the immediate value is added
static INLINE void gen_lea(HostReg dest_reg, Bitu scale, Bits imm)
{
	if (scale)
	{
		RLW_OP(21, dest_reg, dest_reg, scale, 0, 31-scale, 0); // slwi dest,dest,scale
	}

	gen_add_imm(dest_reg, imm);
}

// helper function to choose direct or indirect call
static int INLINE do_gen_call(void *func, Bit64u *npos, bool pad)
{
	Bit64s f = (Bit64s)func;
	Bit64s off = f - (Bit64s)npos;
	Bit32u *pos = (Bit32u *)npos;

	// the length of this branch stanza must match the assumptions in
	// gen_fill_function_ptr

	// relative branches are limited to +/- ~32MB
	if (off < 0x02000000 && off >= -0x02000000)
	{
		pos[0] = 0x48000001 | (off & 0x03FFFFFC); // bl func // "lis"
		if (pad)
		{
			// keep this patchable
			pos[1] = NOP; // nop "ori"
			pos[2] = NOP; // nop "rldicr"
			pos[3] = NOP; // nop "oris"
			pos[4] = NOP; // nop "ori"
			pos[5] = NOP; // nop "mtctr"
			pos[6] = NOP; // nop "bctrl"
			return 28;
		}
		return 4;
	}

	// for ppc64le ELF ABI, use r12 to branch
	pos[0] = IMM(15, HOST_R12, 0,        (f & 0xffff000000000000)>>48); // lis
	pos[1] = IMM(24, HOST_R12, HOST_R12, (f & 0x0000ffff00000000)>>32); // ori
	pos[2] = RLD(30, HOST_R12, HOST_R12, 32, 31, 1, 0); // rldicr
	pos[3] = IMM(25, HOST_R12, HOST_R12, (f & 0x00000000ffff0000)>>16); // oris
	pos[4] = IMM(24, HOST_R12, HOST_R12, (f & 0x000000000000ffff)    ); // ori
	pos[5] = EXT(HOST_R12, 9, 0, 467, 0);      // mtctr r12
	pos[6] = IMM(19, 0x14, 0, (528<<1)|1); // bctrl
	return 28;
}

// generate a call to a parameterless function
static void INLINE gen_call_function_raw(void *func, bool fastcall = true)
{
	cache.pos += do_gen_call(func, (Bit64u*)cache.pos, fastcall);
}

// generate a call to a function with paramcount parameters
// note: the parameters are loaded in the architecture specific way
// using the gen_load_param_ functions below
static Bit64u INLINE gen_call_function_setup(void *func,
                                             Bitu paramcount,
                                             bool fastcall = false)
{
	Bit64u proc_addr=(Bit64u)cache.pos;
	gen_call_function_raw(func,fastcall);
	return proc_addr;
}

// load an immediate value as param'th function parameter
// these are 32-bit (see risc_x64.h)
static void INLINE gen_load_param_imm(Bitu imm, Bitu param)
{
	gen_mov_dword_to_reg_imm(RegParams[param], imm);
}

// load an address as param'th function parameter
// 32-bit
static void INLINE gen_load_param_addr(Bitu addr, Bitu param)
{
	gen_load_param_imm(addr, param);
}

// load a host-register as param'th function parameter
// 32-bit
static void INLINE gen_load_param_reg(Bitu reg, Bitu param)
{
	gen_mov_regs(RegParams[param], (HostReg)reg);
}

// load a value from memory as param'th function parameter
// 32-bit
static void INLINE gen_load_param_mem(Bitu mem, Bitu param)
{
	gen_mov_word_to_reg(RegParams[param], (void*)mem, true);
}

// jump to an address pointed at by ptr, offset is in imm
// use r12 for ppc64le ABI compatibility
static void gen_jmp_ptr(void *ptr, Bits imm = 0)
{
	// "gen_mov_qword_to_reg"
	gen_mov_qword_to_reg_imm(HOST_R12,(Bit64u)ptr);         // r12 = *(Bit64u*)ptr
	DSF_OP(58, HOST_R12, HOST_R12, 0, 0);

	if ((Bit16s)imm != (Bit32s)imm) {
		// FIXME: this is not tested. I've left it as a quasi-assertion.
		fprintf(stderr, "large gen_jmp_ptr offset\n");
		__asm__("trap\n");
		IMM_OP(15, HOST_R12, HOST_R12, (imm + 0x8000)>>16); // addis r12, r12, imm@ha
	}
	DSF_OP(58, HOST_R12, HOST_R12, (Bit16s)imm, 0);                 // ld r12, imm@l(r12)
	EXT_OP(HOST_R12, 9, 0, 467, 0);                         // mtctr r12
	IMM_OP(19, 0x14, 0, 528<<1);                            // bctr
}

// short conditional jump (+-127 bytes) if register is zero
// the destination is set by gen_fill_branch() later
static Bit64u gen_create_branch_on_zero(HostReg reg, bool dword)
{
	if (!dword)
		IMM_OP(28,reg,HOST_R0,0xFFFF); // andi. r0,reg,0xFFFF
	else
		IMM_OP(11, 0, reg, 0);         // cmpwi cr0, reg, 0

	IMM_OP(16, 0x0C, 2, 0); // bc 12,CR0[Z] (beq)
	return ((Bit64u)cache.pos-4);
}

// short conditional jump (+-127 bytes) if register is nonzero
// the destination is set by gen_fill_branch() later
static Bit64u gen_create_branch_on_nonzero(HostReg reg, bool dword)
{
	if (!dword)
		IMM_OP(28,reg,HOST_R0,0xFFFF); // andi. r0,reg,0xFFFF
	else
		IMM_OP(11, 0, reg, 0);         // cmpwi cr0, reg, 0

	IMM_OP(16, 0x04, 2, 0); // bc 4,CR0[Z] (bne)
	return ((Bit64u)cache.pos-4);
}

// calculate relative offset and fill it into the location pointed to by data
static void gen_fill_branch(DRC_PTR_SIZE_IM data)
{
#if C_DEBUG
	Bits len=(Bit64u)cache.pos-data;
	if (len<0) len=-len;
	if (len >= 0x8000) LOG_MSG("Big jump %d",len);
#endif

	// FIXME: assert???
	((Bit16u*)data)[0] =((Bit64u)cache.pos-data) & 0xFFFC; // ENDIAN!!!
}


// conditional jump if register is nonzero
// for isdword==true the 32bit of the register are tested
// for isdword==false the lowest 8bit of the register are tested
static Bit64u gen_create_branch_long_nonzero(HostReg reg, bool dword)
{
	if (!dword)
		IMM_OP(28,reg,HOST_R0,0xFF); // andi. r0,reg,0xFF
	else
		IMM_OP(11, 0, reg, 0);       // cmpwi cr0, reg, 0

	IMM_OP(16, 0x04, 2, 0); // bne
	return ((Bit64u)cache.pos-4);
}

// compare 32bit-register against zero and jump if value less/equal than zero
static Bit64u gen_create_branch_long_leqzero(HostReg reg)
{
	IMM_OP(11, 0, reg, 0); // cmpwi cr0, reg, 0

	IMM_OP(16, 0x04, 1, 0); // ble
	return ((Bit64u)cache.pos-4);
}

// calculate long relative offset and fill it into the location pointed to by data
static void gen_fill_branch_long(Bit64u data)
{
	return gen_fill_branch((DRC_PTR_SIZE_IM)data);
}

static void cache_block_closing(const uint8_t *block_start, Bitu block_size)
{
	// in the Linux kernel i-cache and d-cache are flushed separately
	// there's probably a good reason for this ...
	Bit8u* dstart = (Bit8u*)((Bit64u)block_start & -128);
	Bit8u* istart = dstart;

	while (dstart < block_start + block_size)
	{
		asm volatile("dcbf %y0" :: "Z"(*dstart));
		// cache line size for POWER8 and POWER9 is 128 bytes
		dstart += 128;
	}
	asm volatile("sync");

	while (istart < block_start + block_size)
	{
		asm volatile("icbi %y0" :: "Z"(*istart));
		istart += 128;
	}
	asm volatile("isync");
}

static void cache_block_before_close(void) {}

static void gen_function(void *func)
{
	Bit64s off = (Bit64s)func - (Bit64s)cache.pos;

	// relative branches are limited to +/- 32MB
	if (off < 0x02000000 && off >= -0x02000000) {
		cache_addd(0x48000000 | (off & 0x03FFFFFC)); // b func
		return;
	}

	gen_mov_qword_to_reg_imm(HOST_R12, (Bit64u)func); // r12 = func
	EXT_OP(HOST_R12, 9, 0, 467, 0);  // mtctr r12
	IMM_OP(19, 0x14, 0, 528<<1); // bctr
}

// gen_run_code is assumed to be called exactly once, gen_return_function() jumps back to it
static void* epilog_addr;
static Bit8u *getCF_glue;
static void gen_run_code(void)
{
	// prolog
	DSF_OP(62, HOST_R1, HOST_R1, -256, 1); // stdu sp,-256(sp)
	EXT_OP(FC_OP1, 9, 0, 467, 0); // mtctr FC_OP1
	EXT_OP(HOST_R0, 8, 0, 339, 0); // mflr r0
	// we don't clobber any CR fields we need to restore, so no need to save

	// put at the very end of the stack frame, since we have no floats
	// to save
	DSF_OP(62, HOST_R26, HOST_R1, 208+0 , 0); // std r26, 208(sp)
	DSF_OP(62, HOST_R27, HOST_R1, 208+8 , 0); // std r27, 216(sp)
	DSF_OP(62, HOST_R28, HOST_R1, 208+16, 0); // :
	DSF_OP(62, HOST_R29, HOST_R1, 208+24, 0); // :
	DSF_OP(62, HOST_R30, HOST_R1, 208+32, 0); // :
	DSF_OP(62, HOST_R31, HOST_R1, 208+40, 0); // std r31, 248(sp)

#if C_FPU
	gen_mov_qword_to_reg_imm(HOST_R28, ((Bit64u)&fpu));
#endif
	gen_mov_qword_to_reg_imm(FC_SEGS_ADDR, ((Bit64u)&Segs));
	gen_mov_qword_to_reg_imm(FC_REGS_ADDR, ((Bit64u)&cpu_regs));
	DSF_OP(62, HOST_R0,  HOST_R1, 256+16, 0); // std r0,256+16(sp)
	//TRAP();
	IMM_OP(19, 0x14, 0, 528<<1);     // bctr

	// epilog
	epilog_addr = cache.pos;
	//TRAP();
	DSF_OP(58, HOST_R0, HOST_R1, 256+16, 0);  // ld r0,256+16(sp)
	EXT_OP(HOST_R0, 8, 0, 467, 0);            // mtlr r0
	DSF_OP(58, HOST_R31, HOST_R1, 208+40, 0); // ld r31, 248(sp)
	DSF_OP(58, HOST_R30, HOST_R1, 208+32, 0); // etc.
	DSF_OP(58, HOST_R29, HOST_R1, 208+24, 0);
	DSF_OP(58, HOST_R28, HOST_R1, 208+16, 0);
	DSF_OP(58, HOST_R27, HOST_R1, 208+8 , 0);
	DSF_OP(58, HOST_R26, HOST_R1, 208+0 , 0);

	IMM_OP(14, HOST_R1, HOST_R1, 256);   // addi sp, sp, 256
	IMM_OP(19, 0x14, 0, 16<<1);          // blr

	// trampoline to call get_CF()
	getCF_glue = cache.pos;
	gen_function((void*)get_CF);
}

// return from a function
static void gen_return_function(void)
{
	gen_function(epilog_addr);
}

// called when a call to a function can be replaced by a
// call to a simpler function
// these must equal the length of a branch stanza (see
// do_gen_call)
static void gen_fill_function_ptr(Bit8u *pos, void *fct_ptr, Bitu flags_type)
{
	Bit32u *op = (Bit32u*)pos;

	// blank the entire old stanza
	op[1] = NOP;
	op[2] = NOP;
	op[3] = NOP;
	op[4] = NOP;
	op[5] = NOP;
	op[6] = NOP;

	switch (flags_type) {
#if defined(DRC_FLAGS_INVALIDATION_DCODE)
		// try to avoid function calls but rather directly fill in code
		case t_ADDb:
		case t_ADDw:
		case t_ADDd:
			*op++ = EXT(FC_RETOP, FC_OP1, FC_OP2, 266, 0); // add FC_RETOP, FC_OP1, FC_OP2
			break;
		case t_ORb:
		case t_ORw:
		case t_ORd:
			*op++ = EXT(FC_OP1, FC_RETOP, FC_OP2, 444, 0); // or FC_RETOP, FC_OP1, FC_OP2
			break;
		case t_ADCb:
		case t_ADCw:
		case t_ADCd:
			op[0] = EXT(HOST_R26, FC_OP1, FC_OP2, 266, 0); // r26 = FC_OP1 + FC_OP2
			op[1] = 0x48000001 | ((getCF_glue-(pos+4)) & 0x03FFFFFC); // bl get_CF
			op[2] = IMM(12, HOST_R0, FC_RETOP, -1);        // addic r0, FC_RETOP, 0xFFFFFFFF (XER[CA] = !!CF)
			op[3] = EXT(FC_RETOP, HOST_R26, 0, 202, 0);    // addze; FC_RETOP = r26 + !!CF
			return;
		case t_SBBb:
		case t_SBBw:
		case t_SBBd:
			op[0] = EXT(HOST_R26, FC_OP2, FC_OP1, 40, 0);  // r26 = FC_OP1 - FC_OP2
			op[1] = 0x48000001 | ((getCF_glue-(pos+4)) & 0x03FFFFFC); // bl get_CF
			op[2] = IMM(8, HOST_R0, FC_RETOP, 0);          // subfic r0, FC_RETOP, 0 (XER[CA] = !CF)
			op[3] = EXT(FC_RETOP, HOST_R26, 0, 234, 0);    // addme; FC_RETOP = r26 - 1 + !CF
			return;
		case t_ANDb:
		case t_ANDw:
		case t_ANDd:
			*op++ = EXT(FC_OP1, FC_RETOP, FC_OP2, 28, 0); // and FC_RETOP, FC_OP1, FC_OP2
			break;
		case t_SUBb:
		case t_SUBw:
		case t_SUBd:
			*op++ = EXT(FC_RETOP, FC_OP2, FC_OP1, 40, 0); // subf FC_RETOP, FC_OP2, FC_OP1
			break;
		case t_XORb:
		case t_XORw:
		case t_XORd:
			*op++ = EXT(FC_OP1, FC_RETOP, FC_OP2, 316, 0); // xor FC_RETOP, FC_OP1, FC_OP2
			break;
		case t_CMPb:
		case t_CMPw:
		case t_CMPd:
		case t_TESTb:
		case t_TESTw:
		case t_TESTd:
			break;
		case t_INCb:
		case t_INCw:
		case t_INCd:
			*op++ = IMM(14, FC_RETOP, FC_OP1, 1); // addi FC_RETOP, FC_OP1, #1
			break;
		case t_DECb:
		case t_DECw:
		case t_DECd:
			*op++ = IMM(14, FC_RETOP, FC_OP1, -1); // addi FC_RETOP, FC_OP1, #-1
			break;
		case t_NEGb:
		case t_NEGw:
		case t_NEGd:
			*op++ = EXT(FC_RETOP, FC_OP1, 0, 104, 0); // neg FC_RETOP, FC_OP1
			break;
		case t_SHLb:
		case t_SHLw:
		case t_SHLd:
			*op++ = EXT(FC_OP1, FC_RETOP, FC_OP2, 24, 0); // slw FC_RETOP, FC_OP1, FC_OP2
			break;
		case t_SHRb:
		case t_SHRw:
		case t_SHRd:
			*op++ = EXT(FC_OP1, FC_RETOP, FC_OP2, 536, 0); // srw FC_RETOP, FC_OP1, FC_OP2
			break;
		case t_SARb:
			*op++ = EXT(FC_OP1, FC_RETOP, 0, 954, 0); // extsb FC_RETOP, FC_OP1
		case t_SARw:
			if (flags_type == t_SARw)
				*op++ = EXT(FC_OP1, FC_RETOP, 0, 922, 0); // extsh FC_RETOP, FC_OP1
		case t_SARd:
			*op++ = EXT(FC_OP1, FC_RETOP, FC_OP2, 792, 0); // sraw FC_RETOP, FC_OP1, FC_OP2
			break;

		case t_ROLb:
			*op++ = RLW(20, FC_OP1, FC_OP1, 24, 0, 7, 0); // rlwimi FC_OP1, FC_OP1, 24, 0, 7
		case t_ROLw:
			if (flags_type == t_ROLw)
				*op++ = RLW(20, FC_OP1, FC_OP1, 16, 0, 15, 0); // rlwimi FC_OP1, FC_OP1, 16, 0, 15
		case t_ROLd:
			*op++ = RLW(23, FC_OP1, FC_RETOP, FC_OP2, 0, 31, 0); // rotlw FC_RETOP, FC_OP1, FC_OP2
			break;

		case t_RORb:
			*op++ = RLW(20, FC_OP1, FC_OP1, 8, 16, 23, 0); // rlwimi FC_OP1, FC_OP1, 8, 16, 23
		case t_RORw:
			if (flags_type == t_RORw)
				*op++ = RLW(20, FC_OP1, FC_OP1, 16, 0, 15, 0); // rlwimi FC_OP1, FC_OP1, 16, 0, 15
		case t_RORd:
			*op++ = IMM(8, FC_OP2, FC_OP2, 32); // subfic FC_OP2, FC_OP2, 32 (FC_OP2 = 32 - FC_OP2)
			*op++ = RLW(23, FC_OP1, FC_RETOP, FC_OP2, 0, 31, 0); // rotlw FC_RETOP, FC_OP1, FC_OP2
			break;

		case t_DSHLw: // technically not correct for FC_OP3 > 16
			*op++ = RLW(20, FC_OP2, FC_RETOP, 16, 0, 15, 0); // rlwimi FC_RETOP, FC_OP2, 16, 0, 5
			*op++ = RLW(23, FC_RETOP, FC_RETOP, FC_OP3, 0, 31, 0); // rotlw FC_RETOP, FC_RETOP, FC_OP3
			break;
		case t_DSHLd:
			op[0] = EXT(FC_OP1, FC_RETOP, FC_OP3, 24, 0); // slw FC_RETOP, FC_OP1, FC_OP3
			op[1] = IMM(8, FC_OP3, FC_OP3, 32); // subfic FC_OP3, FC_OP3, 32 (FC_OP3 = 32 - FC_OP3)
			op[2] = EXT(FC_OP2, FC_OP2, FC_OP3, 536, 0); // srw FC_OP2, FC_OP2, FC_OP3
			op[3] = EXT(FC_RETOP, FC_RETOP, FC_OP2, 444, 0); // or FC_RETOP, FC_RETOP, FC_OP2
			return;
		case t_DSHRw: // technically not correct for FC_OP3 > 16
			*op++ = RLW(20, FC_OP2, FC_RETOP, 16, 0, 15, 0); // rlwimi FC_RETOP, FC_OP2, 16, 0, 5
			*op++ = EXT(FC_RETOP, FC_RETOP, FC_OP3, 536, 0); // srw FC_RETOP, FC_RETOP, FC_OP3
			break;
		case t_DSHRd:
			op[0] = EXT(FC_OP1, FC_RETOP, FC_OP3, 536, 0); // srw FC_RETOP, FC_OP1, FC_OP3
			op[1] = IMM(8, FC_OP3, FC_OP3, 32); // subfic FC_OP3, FC_OP3, 32 (FC_OP32 = 32 - FC_OP3)
			op[2] = EXT(FC_OP2, FC_OP2, FC_OP3, 24, 0); // slw FC_OP2, FC_OP2, FC_OP3
			op[3] = EXT(FC_RETOP, FC_RETOP, FC_OP2, 444, 0); // or FC_RETOP, FC_RETOP, FC_OP2
			return;
#endif
		default:
			do_gen_call(fct_ptr, (Bit64u*)op, true);
			return;
	}
}

// mov 16bit value from Segs[index] into dest_reg using FC_SEGS_ADDR (index
// modulo 2 must be zero) 16bit moves may destroy the upper 16bit of the
// destination register
static void gen_mov_seg16_to_reg(HostReg dest_reg, Bitu index)
{
	gen_mov_word_to_reg(dest_reg, (Bit8u*)&Segs + index, false);
}

// mov 32bit value from Segs[index] into dest_reg using FC_SEGS_ADDR (index
// modulo 4 must be zero)
static void gen_mov_seg32_to_reg(HostReg dest_reg, Bitu index)
{
	gen_mov_word_to_reg(dest_reg, (Bit8u*)&Segs + index, true);
}

// add a 32bit value from Segs[index] to a full register using FC_SEGS_ADDR
// (index modulo 4 must be zero)
static void gen_add_seg32_to_reg(HostReg reg, Bitu index)
{
	gen_add(reg, (Bit8u*)&Segs + index);
}

// mov 16bit value from cpu_regs[index] into dest_reg using FC_REGS_ADDR (index
// modulo 2 must be zero) 16bit moves may destroy the upper 16bit of the
// destination register
static void gen_mov_regval16_to_reg(HostReg dest_reg, Bitu index)
{
	gen_mov_word_to_reg(dest_reg, (Bit8u*)&cpu_regs + index, false);
}

// mov 32bit value from cpu_regs[index] into dest_reg using FC_REGS_ADDR (index
// modulo 4 must be zero)
static void gen_mov_regval32_to_reg(HostReg dest_reg, Bitu index)
{
	gen_mov_word_to_reg(dest_reg, (Bit8u*)&cpu_regs + index, true);
}

// move an 8bit value from cpu_regs[index]  into dest_reg using FC_REGS_ADDR
// the upper 24bit of the destination register can be destroyed
// this function does not use FC_OP1/FC_OP2 as dest_reg as these
// registers might not be directly byte-accessible on some architectures
static void gen_mov_regbyte_to_reg_low(HostReg dest_reg, Bitu index)
{
	gen_mov_byte_to_reg_low(dest_reg, (Bit8u*)&cpu_regs + index);
}

// move an 8bit value from cpu_regs[index]  into dest_reg using FC_REGS_ADDR
// the upper 24bit of the destination register can be destroyed
// this function can use FC_OP1/FC_OP2 as dest_reg which are
// not directly byte-accessible on some architectures
static void INLINE gen_mov_regbyte_to_reg_low_canuseword(HostReg dest_reg, Bitu index)
{
	gen_mov_byte_to_reg_low_canuseword(dest_reg, (Bit8u*)&cpu_regs + index);
}

// move 16bit of register into cpu_regs[index] using FC_REGS_ADDR
// (index modulo 2 must be zero)
static void gen_mov_regval16_from_reg(HostReg src_reg, Bitu index)
{
	gen_mov_word_from_reg(src_reg, (Bit8u*)&cpu_regs + index, false);
}

// move 32bit of register into cpu_regs[index] using FC_REGS_ADDR
// (index modulo 4 must be zero)
static void gen_mov_regval32_from_reg(HostReg src_reg, Bitu index)
{
	gen_mov_word_from_reg(src_reg, (Bit8u*)&cpu_regs + index, true);
}

// move the lowest 8bit of a register into cpu_regs[index] using FC_REGS_ADDR
static void gen_mov_regbyte_from_reg_low(HostReg src_reg, Bitu index)
{
	gen_mov_byte_from_reg_low(src_reg, (Bit8u*)&cpu_regs + index);
}

// add a 32bit value from cpu_regs[index] to a full register using FC_REGS_ADDR
// (index modulo 4 must be zero)
static void gen_add_regval32_to_reg(HostReg reg, Bitu index)
{
	gen_add(reg, (Bit8u*)&cpu_regs + index);
}

// move 32bit (dword==true) or 16bit (dword==false) of a register into
// cpu_regs[index] using FC_REGS_ADDR (if dword==true index modulo 4 must be
// zero) (if dword==false index modulo 2 must be zero)
static void gen_mov_regword_from_reg(HostReg src_reg, Bitu index, bool dword)
{
	if (dword)
		gen_mov_regval32_from_reg(src_reg, index);
	else
		gen_mov_regval16_from_reg(src_reg, index);
}

// move a 32bit (dword==true) or 16bit (dword==false) value from cpu_regs[index]
// into dest_reg using FC_REGS_ADDR (if dword==true index modulo 4 must be zero)
// (if dword==false index modulo 2 must be zero) 16bit moves may destroy the
// upper 16bit of the destination register
static void gen_mov_regword_to_reg(HostReg dest_reg, Bitu index, bool dword)
{
	if (dword)
		gen_mov_regval32_to_reg(dest_reg, index);
	else
		gen_mov_regval16_to_reg(dest_reg, index);
}
