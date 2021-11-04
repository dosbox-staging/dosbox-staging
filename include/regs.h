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

#ifndef DOSBOX_REGS_H
#define DOSBOX_REGS_H

#ifndef DOSBOX_MEM_H
#include "mem.h"
#endif

#define FLAG_CF		0x00000001
#define FLAG_PF		0x00000004
#define FLAG_AF		0x00000010
#define FLAG_ZF		0x00000040
#define FLAG_SF		0x00000080
#define FLAG_OF		0x00000800

#define FLAG_TF		0x00000100
#define FLAG_IF		0x00000200
#define FLAG_DF		0x00000400

#define FLAG_IOPL	0x00003000
#define FLAG_NT		0x00004000
#define FLAG_VM		0x00020000
#define FLAG_AC		0x00040000
#define FLAG_ID		0x00200000

#define FMASK_TEST		(FLAG_CF | FLAG_PF | FLAG_AF | FLAG_ZF | FLAG_SF | FLAG_OF)
#define FMASK_NORMAL	(FMASK_TEST | FLAG_DF | FLAG_TF | FLAG_IF )	
#define FMASK_ALL		(FMASK_NORMAL | FLAG_IOPL | FLAG_NT)

#define SETFLAGBIT(TYPE,TEST) if (TEST) reg_flags|=FLAG_ ## TYPE; else reg_flags&=~FLAG_ ## TYPE

#define GETFLAG(TYPE) (reg_flags & FLAG_ ## TYPE)
#define GETFLAGBOOL(TYPE) ((reg_flags & FLAG_ ## TYPE) ? true : false )

#define GETFLAG_IOPL ((reg_flags & FLAG_IOPL) >> 12)

struct Segment {
	uint16_t val = 0;
	PhysPt phys = 0; /* The physical address start in emulated machine */
};

enum SegNames {
	es = 0,
	cs,
	ss,
	ds,
	fs,
	gs,
};

struct Segments {
	uint16_t val[8] = {};
	PhysPt phys[8] = {};
};

union GenReg32 {
	uint32_t dword[1] = {};
	uint16_t word[2];
	uint8_t byte[4];
};

#ifdef WORDS_BIGENDIAN

constexpr auto DW_INDEX = 0;
constexpr auto W_INDEX = 1;
constexpr auto BH_INDEX = 2;
constexpr auto BL_INDEX = 3;

#else

constexpr auto DW_INDEX = 0;
constexpr auto W_INDEX = 0;
constexpr auto BH_INDEX = 1;
constexpr auto BL_INDEX = 0;

#endif

struct CPU_Regs {
	GenReg32 regs[8] = {};
	GenReg32 ip = {};
	uint32_t flags = 0;
};

extern Segments Segs;
extern CPU_Regs cpu_regs;

static inline PhysPt SegPhys(SegNames index) {
	return Segs.phys[index];
}

static inline uint16_t SegValue(SegNames index)
{
	return Segs.val[index];
}

static inline RealPt RealMakeSeg(SegNames index, uint16_t off)
{
	return RealMake(SegValue(index), off);
}

static inline void SegSet16(Bitu index, uint16_t val)
{
	Segs.val[index] = val;
	Segs.phys[index] = static_cast<uint32_t>(val << 4);
}

enum {
	REGI_AX = 0,
	REGI_CX,
	REGI_DX,
	REGI_BX,
	REGI_SP,
	REGI_BP,
	REGI_SI,
	REGI_DI,
};

enum {
	REGI_AL = 0,
	REGI_CL,
	REGI_DL,
	REGI_BL,
	REGI_AH,
	REGI_CH,
	REGI_DH,
	REGI_BH,
};

//macros to convert a 3-bit register index to the correct register
constexpr uint8_t &reg_8l(const uint8_t reg)
{
	return cpu_regs.regs[reg].byte[BL_INDEX];
}
constexpr uint8_t &reg_8h(const uint8_t reg)
{
	return cpu_regs.regs[reg].byte[BH_INDEX];
}
constexpr uint8_t &reg_8(const uint8_t reg)
{
	return (reg & 4) ? reg_8h(reg & 3) : reg_8l(reg & 3);
}
constexpr uint16_t &reg_16(const uint8_t reg)
{
	return cpu_regs.regs[reg].word[W_INDEX];
}
constexpr uint32_t &reg_32(const uint8_t reg)
{
	return cpu_regs.regs[reg].dword[DW_INDEX];
}

inline constexpr uint8_t &reg_al = cpu_regs.regs[REGI_AX].byte[BL_INDEX];
inline constexpr uint8_t &reg_ah = cpu_regs.regs[REGI_AX].byte[BH_INDEX];
inline constexpr uint16_t &reg_ax = cpu_regs.regs[REGI_AX].word[W_INDEX];
inline constexpr uint32_t &reg_eax = cpu_regs.regs[REGI_AX].dword[DW_INDEX];

inline constexpr uint8_t &reg_bl = cpu_regs.regs[REGI_BX].byte[BL_INDEX];
inline constexpr uint8_t &reg_bh = cpu_regs.regs[REGI_BX].byte[BH_INDEX];
inline constexpr uint16_t &reg_bx = cpu_regs.regs[REGI_BX].word[W_INDEX];
inline constexpr uint32_t &reg_ebx = cpu_regs.regs[REGI_BX].dword[DW_INDEX];

inline constexpr uint8_t &reg_cl = cpu_regs.regs[REGI_CX].byte[BL_INDEX];
inline constexpr uint8_t &reg_ch = cpu_regs.regs[REGI_CX].byte[BH_INDEX];
inline constexpr uint16_t &reg_cx = cpu_regs.regs[REGI_CX].word[W_INDEX];
inline constexpr uint32_t &reg_ecx = cpu_regs.regs[REGI_CX].dword[DW_INDEX];

inline constexpr uint8_t &reg_dl = cpu_regs.regs[REGI_DX].byte[BL_INDEX];
inline constexpr uint8_t &reg_dh = cpu_regs.regs[REGI_DX].byte[BH_INDEX];
inline constexpr uint16_t &reg_dx = cpu_regs.regs[REGI_DX].word[W_INDEX];
inline constexpr uint32_t &reg_edx = cpu_regs.regs[REGI_DX].dword[DW_INDEX];

inline constexpr uint16_t &reg_si = cpu_regs.regs[REGI_SI].word[W_INDEX];
inline constexpr uint32_t &reg_esi = cpu_regs.regs[REGI_SI].dword[DW_INDEX];

inline constexpr uint16_t &reg_di = cpu_regs.regs[REGI_DI].word[W_INDEX];
inline constexpr uint32_t &reg_edi = cpu_regs.regs[REGI_DI].dword[DW_INDEX];

inline constexpr uint16_t &reg_sp = cpu_regs.regs[REGI_SP].word[W_INDEX];
inline constexpr uint32_t &reg_esp = cpu_regs.regs[REGI_SP].dword[DW_INDEX];

inline constexpr uint16_t &reg_bp = cpu_regs.regs[REGI_BP].word[W_INDEX];
inline constexpr uint32_t &reg_ebp = cpu_regs.regs[REGI_BP].dword[DW_INDEX];

inline constexpr uint16_t &reg_ip = cpu_regs.ip.word[W_INDEX];
inline constexpr uint32_t &reg_eip = cpu_regs.ip.dword[DW_INDEX];

#define reg_flags cpu_regs.flags

#endif
