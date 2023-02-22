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

#ifndef DOSBOX_FPU_H
#define DOSBOX_FPU_H

#ifndef DOSBOX_DOSBOX_H
//So the right config.h gets included for C_DEBUG
#include "dosbox.h"
#endif

#ifndef DOSBOX_MEM_H
#include "mem.h"
#endif

#include "cpu.h"

void FPU_ESC0_Normal(Bitu rm);
void FPU_ESC0_EA(Bitu func,PhysPt ea);
void FPU_ESC1_Normal(Bitu rm);
void FPU_ESC1_EA(Bitu func,PhysPt ea, bool op16);
void FPU_ESC2_Normal(Bitu rm);
void FPU_ESC2_EA(Bitu func,PhysPt ea);
void FPU_ESC3_Normal(Bitu rm);
void FPU_ESC3_EA(Bitu func,PhysPt ea);
void FPU_ESC4_Normal(Bitu rm);
void FPU_ESC4_EA(Bitu func,PhysPt ea);
void FPU_ESC5_Normal(Bitu rm);
void FPU_ESC5_EA(Bitu func,PhysPt ea, bool op16);
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

template <class T, unsigned bitno, unsigned nbits = 1>
struct RegBit {
	enum : T { basemask = (1 << nbits) - 1 };
	enum : T { mask = basemask << bitno };
	T data;
	RegBit() = default;
	RegBit(const T& reg) : data(reg) {}

	template <class T2>
	RegBit& operator=(const T2 val)
	{
		data = static_cast<T>((data & ~mask) |
		                      ((nbits > 1 ? val & basemask : !!val) << bitno));
		return *this;
	}

	// Limit casting outward to our internal type, as it can't be smaller or
	// larger. Any type conversion warnings will occur externally (if misused).
	operator T() const
	{
		return static_cast<T>((data & mask) >> bitno);
	}
};

struct FPUControlWord {
	enum : uint16_t {
		mask8087     = 0x1fff,
		maskNon8087  = 0x1f7f,
		reservedMask = 0x40,
		initValue    = 0x37f
	};

	enum RoundMode : uint8_t {
		Nearest,
		Down,
		Up,
		Chop,
	};

	union {
		uint16_t reg = initValue;
		RegBit<decltype(reg), 0> IM;     // Invalid operation mask
		RegBit<decltype(reg), 1> DM;     // Denormalized operand mask
		RegBit<decltype(reg), 2> ZM;     // Zero divide mask
		RegBit<decltype(reg), 3> OM;     // Overflow mask
		RegBit<decltype(reg), 4> UM;     // Underflow mask
		RegBit<decltype(reg), 5> PM;     // Precision mask
		RegBit<decltype(reg), 7> M;      // Interrupt mask   (8087-only)
		RegBit<decltype(reg), 8, 2> PC;  // Precision control
		RegBit<decltype(reg), 10, 2> RC; // Rounding control
		RegBit<decltype(reg), 12> IC; // Infinity control (8087/80287-only)
	};

	FPUControlWord()                            = default;
	FPUControlWord(const FPUControlWord& other) = default;

	FPUControlWord& operator=(const FPUControlWord& other)
	{
		reg = other.reg;
		return *this;
	}

	template <class T>
	FPUControlWord& operator=(const T val)
	{
		const auto arch_mask = CPU_ArchitectureType == CPU_ARCHTYPE_8086
		                             ? mask8087
		                             : maskNon8087;

		reg = static_cast<uint16_t>(val & arch_mask) | reservedMask;
		return *this;
	}

	operator uint16_t() const
	{
		return reg;
	}

	template <class T>
	FPUControlWord& operator|=(const T val)
	{
		// use assignment operator
		*this = reg | val;
		return *this;
	}

	FPUControlWord allMasked() const
	{
		auto masked = *this;
		masked |= IM.mask | DM.mask | ZM.mask | OM.mask | UM.mask | PM.mask;
		return masked;
	}
};

struct FPUStatusWord {
	enum : uint16_t {
		conditionMask             = 0x4700,
		conditionUnmask           = static_cast<uint16_t>(~conditionMask),
		conditionAndExceptionMask = 0x47bf,
		initValue                 = 0,
	};
	union {
		uint16_t reg = initValue;
		RegBit<decltype(reg), 0> IE;  // Invalid operation
		RegBit<decltype(reg), 1> DE;  // Denormalized operand
		RegBit<decltype(reg), 2> ZE;  // Divide-by-zero
		RegBit<decltype(reg), 3> OE;  // Overflow
		RegBit<decltype(reg), 4> UE;  // Underflow
		RegBit<decltype(reg), 5> PE;  // Precision
		RegBit<decltype(reg), 6> SF;  // Stack Flag (non-8087/802087)
		RegBit<decltype(reg), 7> IR;  // Interrupt request (8087-only)
		RegBit<decltype(reg), 7> ES;  // Error summary     (non-8087)
		RegBit<decltype(reg), 8> C0;  // Condition flag
		RegBit<decltype(reg), 9> C1;  // Condition flag
		RegBit<decltype(reg), 10> C2; // Condition flag
		RegBit<decltype(reg), 11, 3> top; // Top of stack pointer
		RegBit<decltype(reg), 14> C3;     // Condition flag
		RegBit<decltype(reg), 15> B;      // Busy flag
	};

	FPUStatusWord()                           = default;
	FPUStatusWord(const FPUStatusWord& other) = default;

	FPUStatusWord& operator=(const FPUStatusWord& other)
	{
		reg = other.reg;
		return *this;
	}

	template <class T>
	FPUStatusWord& operator=(const T val)
	{
		if constexpr (std::is_signed_v<T>) {
			assert(val >= 0);
		}
		if constexpr (sizeof(T) > sizeof(reg)) {
			assert(val <= UINT16_MAX);
		}
		reg = static_cast<uint16_t>(val);
		return *this;
	}

	operator uint16_t() const
	{
		return reg;
	}

	template <class T>
	FPUStatusWord& operator|=(const T val)
	{
		// use assignment operator
		*this = reg | val;
		return *this;
	}

	void clearExceptions()
	{
		IE = false;
		DE = false;
		ZE = false;
		OE = false;
		UE = false;
		PE = false;
		ES = false;
	}
	std::string to_string() const;
};

struct FPU_rec {
	FPU_Reg regs[9]     = {};
	FPU_P_Reg p_regs[9] = {};
	FPU_Tag tags[9]     = {};
	FPUControlWord cw   = {};
	FPUStatusWord sw    = {};
};

extern FPU_rec fpu;

#define TOP fpu.sw.top
#define STV(i)  ( (fpu.sw.top + (i) ) & 7 )

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
	fpu.cw = word;
}

static inline uint16_t FPU_GetSW()
{
	return fpu.sw;
}

static inline void FPU_SetMaskedSW(const uint16_t word, 
                                   const uint16_t mask = FPUStatusWord::conditionAndExceptionMask)
{
	fpu.sw = (word & mask) | (fpu.sw & FPUStatusWord::conditionUnmask);
}

static inline void FPU_SetSW(const uint16_t word)
{
	fpu.sw = word;
}

void FPU_SetPRegsFrom(const uint8_t dyn_regs[8][10]);
void FPU_GetPRegsTo(uint8_t dyn_regs[8][10]);

static inline void FPU_SET_C0(const uint16_t value)
{
	fpu.sw.C0 = (value > 0) ? 0b1 : 0b0;
}

static inline void FPU_SET_C1(const uint16_t value)
{
	fpu.sw.C1 = (value > 0) ? 0b1 : 0b0;
}

static inline void FPU_SET_C2(const uint16_t value)
{
	fpu.sw.C2 = (value > 0) ? 0b1 : 0b0;
}

static inline void FPU_SET_C3(const uint16_t value)
{
	fpu.sw.C3 = (value > 0) ? 0b1 : 0b0;
}

static inline void FPU_SET_D(const uint16_t value)
{
	fpu.sw.DE = (value > 0) ? 0b1 : 0b0;
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
#define DB_FPU_STACK_CHECK_PUSH DB_FPU_STACK_CHECK_EXIT
#endif

#endif
