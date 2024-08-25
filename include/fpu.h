/*
 *  Copyright (C) 2024-2024  The DOSBox Staging Team
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

#ifndef DOSBOX_FPU_H
#define DOSBOX_FPU_H

#include <optional>

#ifndef DOSBOX_DOSBOX_H
	// So the right config.h gets included for C_DEBUG
	#include "dosbox.h"
#endif

#ifndef DOSBOX_MEM_H
#include "mem.h"
#endif

#include "mmx.h"

void FPU_ESC0_Normal(Bitu rm);
void FPU_ESC0_EA(Bitu func,PhysPt ea);
void FPU_ESC1_Normal(Bitu rm);
void FPU_ESC1_EA(Bitu func,PhysPt ea);
void FPU_ESC2_Normal(Bitu rm);
void FPU_ESC2_EA(Bitu func,PhysPt ea);
void FPU_ESC3_Normal(Bitu rm);
void FPU_ESC3_EA(Bitu func,PhysPt ea);
void FPU_ESC4_Normal(Bitu rm);
void FPU_ESC4_EA(Bitu func,PhysPt ea);
void FPU_ESC5_Normal(Bitu rm);
void FPU_ESC5_EA(Bitu func,PhysPt ea);
void FPU_ESC6_Normal(Bitu rm);
void FPU_ESC6_EA(Bitu func,PhysPt ea);
void FPU_ESC7_Normal(Bitu rm);
void FPU_ESC7_EA(Bitu func,PhysPt ea);

union FPU_Reg {
	double d = 0.0;
	struct {
#ifndef WORDS_BIGENDIAN
		uint32_t lower;
		int32_t upper;
#else
		int32_t upper;
		uint32_t lower;
#endif
	} l;
	int64_t ll;
};

struct FPU_P_Reg {
	uint32_t m1 = 0;
	uint32_t m2 = 0;

	uint16_t m3 = 0;

	uint16_t d1 = 0;
	uint32_t d2 = 0;
};

enum FPU_Tag : uint8_t {
	TAG_Valid = 0,
	TAG_Zero  = 1,
	TAG_Weird = 2,
	TAG_Empty = 3
};

enum FPU_Round : uint8_t {
	ROUND_Nearest = 0,
	ROUND_Down    = 1,
	ROUND_Up      = 2,
	ROUND_Chop    = 3
};

struct FPU_rec {
	FPU_Reg regs[9]         = {};
#if !C_FPU_X86
    // for FILD/FIST 64-bit memcpy fix
	std::optional<int64_t> regs_memcpy[9] = {};
#endif
	FPU_P_Reg p_regs[9]     = {};
	MMX_reg mmx_regs[8]     = {};
	FPU_Tag tags[9]         = {};
	uint16_t cw             = 0;
	uint16_t cw_mask_all    = 0;
	uint16_t sw             = 0;
	uint32_t top            = 0;
	FPU_Round round         = {};
};

extern FPU_rec fpu;

#define TOP fpu.top
#define STV(i)  ( (fpu.top+ (i) ) & 7 )

uint16_t FPU_GetTag();
void FPU_FLDCW(PhysPt addr);

static inline void FPU_SetTag(const uint16_t tag)
{
	for (uint8_t i = 0; i < 8; ++i) {
		fpu.tags[i] = static_cast<FPU_Tag>((tag >> (2 * i)) & 3);
	}
}

static inline uint16_t FPU_GetCW()
{
	return fpu.cw;
}

static inline void FPU_SetCW(const uint16_t word)
{
	fpu.cw          = word;
	fpu.cw_mask_all = word | 0x3f;
	fpu.round       = static_cast<FPU_Round>((word >> 10) & 3);
}

static inline uint16_t FPU_GetSW()
{
	return fpu.sw;
}

static inline void FPU_SetSW(const uint16_t word)
{
	fpu.sw = word;
}

constexpr uint16_t fpu_top_register_bits = 0x3800;

static inline uint8_t FPU_GET_TOP()
{
	return static_cast<uint8_t>((fpu.sw & fpu_top_register_bits) >> 11);
}

static inline void FPU_SET_TOP(const uint32_t val)
{
	fpu.sw &= ~fpu_top_register_bits;
	fpu.sw |= static_cast<uint16_t>((val & 7) << 11);
}

void FPU_SetPRegsFrom(const uint8_t dyn_regs[8][10]);
void FPU_GetPRegsTo(uint8_t dyn_regs[8][10]);

static inline void FPU_SET_C0(const bool C)
{
	fpu.sw &= ~0x0100;
	if(C) fpu.sw |=  0x0100;
}

static inline void FPU_SET_C1(const bool C)
{
	fpu.sw &= ~0x0200;
	if(C) fpu.sw |=  0x0200;
}

static inline void FPU_SET_C2(const bool C)
{
	fpu.sw &= ~0x0400;
	if(C) fpu.sw |=  0x0400;
}

static inline void FPU_SET_C3(const bool C)
{
	fpu.sw &= ~0x4000;
	if(C) fpu.sw |= 0x4000;
}

static inline void FPU_LOG_WARN(unsigned tree, bool ea, uintptr_t group, uintptr_t sub)
{
	LOG(LOG_FPU, LOG_WARN)("ESC %u%s: Unhandled group %" PRIuPTR " subfunction %" PRIuPTR,
	                       tree, ea ? " EA" : "", group, sub);
}

#define DB_FPU_STACK_CHECK_NONE 0
#define DB_FPU_STACK_CHECK_LOG  1
#define DB_FPU_STACK_CHECK_EXIT 2
//NONE is 0.74 behavior: not care about stack overflow/underflow
//Overflow is always logged/exited on.
//Underflow can be controlled with by this. 
//LOG is giving a message when encountered
//EXIT is to hard exit.
//Currently pop is ignored in release mode and overflow is exit.
//in debug mode: pop will log and overflow is exit. 
#if C_DEBUG
#define DB_FPU_STACK_CHECK_POP DB_FPU_STACK_CHECK_LOG
#define DB_FPU_STACK_CHECK_PUSH DB_FPU_STACK_CHECK_EXIT
#else
#define DB_FPU_STACK_CHECK_POP DB_FPU_STACK_CHECK_NONE
#define DB_FPU_STACK_CHECK_PUSH DB_FPU_STACK_CHECK_NONE
#endif

#endif
