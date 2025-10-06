// SPDX-FileCopyrightText:  2024-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_FPU_H
#define DOSBOX_FPU_H

#include <optional>

#ifndef DOSBOX_DOSBOX_H
	// So the right config.h gets included for C_DEBUGGER
	#include "dosbox.h"
#endif

#ifndef DOSBOX_MEM_H
#include "hardware/memory.h"
#endif

#include "cpu/mmx.h"

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

#include <bit>
#include <optional>

struct FPU_Reg {
	double d;

	constexpr FPU_Reg() noexcept : d(0.0) {}
	constexpr FPU_Reg(double v) noexcept : d(v) {}
	constexpr explicit FPU_Reg(uint64_t bits) noexcept
	        : d(std::bit_cast<double>(bits))
	{}

	constexpr uint64_t bits(void) const noexcept
	{
		return std::bit_cast<uint64_t>(d);
	}
	constexpr void set_bits(uint64_t val) noexcept
	{
		d = std::bit_cast<double>(val);
	}

	constexpr FPU_Reg& operator=(uint64_t val) noexcept
	{
		d = std::bit_cast<double>(val);
		return *this;
	}
	constexpr FPU_Reg& operator=(double val) noexcept
	{
		d = val;
		return *this;
	}
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
	int64_t regs_memcpy[9] = {};
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

static inline void FPU_SET_C0(Bitu C){
	fpu.sw &= ~0x0100;
	if(C) fpu.sw |=  0x0100;
}

static inline void FPU_SET_C1(Bitu C){
	fpu.sw &= ~0x0200;
	if(C) fpu.sw |=  0x0200;
}

static inline void FPU_SET_C2(Bitu C){
	fpu.sw &= ~0x0400;
	if(C) fpu.sw |=  0x0400;
}

static inline void FPU_SET_C3(Bitu C){
	fpu.sw &= ~0x4000;
	if(C) fpu.sw |= 0x4000;
}

static inline void FPU_LOG_WARN(unsigned tree, bool ea, uintptr_t group, uintptr_t sub)
{
	LOG(LOG_FPU, LOG_WARN)("ESC %u%s: Unhandled group %" PRIuPTR " subfunction %" PRIuPTR,
	                       tree, ea ? " EA" : "", group, sub);
}

void FPU_Init();

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
#if C_DEBUGGER
#define DB_FPU_STACK_CHECK_POP DB_FPU_STACK_CHECK_LOG
#define DB_FPU_STACK_CHECK_PUSH DB_FPU_STACK_CHECK_EXIT
#else
#define DB_FPU_STACK_CHECK_POP DB_FPU_STACK_CHECK_NONE
#define DB_FPU_STACK_CHECK_PUSH DB_FPU_STACK_CHECK_NONE
#endif

#endif
