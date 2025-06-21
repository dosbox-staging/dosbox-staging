// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_REGS_H
#define DOSBOX_REGS_H

#ifndef DOSBOX_MEM_H
#include "mem.h"
#endif

#include <cassert>

// x86 CPU FLAGS register bit positions
// - Intel iAXP 286 Programmer's Reference Manual
// - Intel 64 and IA-32 Architectures Software Developer's Manual. Vol.1

// Carry Flag (bit 0), 1 indicates an arithmetic carry or borrow has been
// generated out of the most significant arithmetic logic unit (ALU) bit
// position.
constexpr uint32_t FLAG_CF = 1 << 0;

// Parity Flag (bit 2) indicates whether the modulo 2 sum of the low-order eight
// bits of the result is even (PF=O) or odd (PF=1).
constexpr uint32_t FLAG_PF = 1 << 2;

// Auxiliary Carry Flag (bit 4), 1 indicates a carry from the lower nibble or a
// for the lower nibble in BCD (Binary-coded Decimal) operations.
constexpr uint32_t FLAG_AF = 1 << 4;

// Zero Flag (bit 6), 1 indicates the result is zero.
constexpr uint32_t FLAG_ZF = 1 << 6;

// Sign Flag (bit 7), 1 indicates the result is negative.
constexpr uint32_t FLAG_SF = 1 << 7;

// Overflow Flag (bit 11), 1 indicates that the operation has overflowed; the
// complete result was too large to be stored in the resulting register.
constexpr uint32_t FLAG_OF = 1 << 11;

// Trap flag (bit 8) 1 indicates that the processor is in single-step mode
// (debugging).
constexpr uint32_t FLAG_TF = 1 << 8;

// I/O level flag (bit 9), 1 indicates that interrupts are enabled.
constexpr uint32_t FLAG_IF = 1 << 9;

// direction flag (bit 10), 1 indicates the direction is down. The meaning of
// 'down' is in context to the instruction.
constexpr uint32_t FLAG_DF = 1 << 10;

// I/O privilege level flags (bits 12 and 13). 286+ only. This is all ones on
// 8086 and 186.
constexpr uint32_t FLAG_IOPL = (1 << 12) | (1 << 13);

// Nested task flag (bit 14), 286+ only. This is always 1 on 8086 and 186.
constexpr uint32_t FLAG_NT = 1 << 14;

// Virtual 8086 mode flag (bit 17), 386+ only.
constexpr uint32_t FLAG_VM = 1 << 17;

// Alignment Check (bit 18), 486+-only.
constexpr uint32_t FLAG_AC = 1 << 18;

// CPUID instruction availability (bit 21), Pentium+-only.
constexpr uint32_t FLAG_ID = 1 << 21;

// Flag groups:
constexpr uint32_t FMASK_TEST = FLAG_CF | FLAG_PF | FLAG_AF | FLAG_ZF |
                                FLAG_SF | FLAG_OF;

constexpr uint32_t FMASK_NORMAL = FMASK_TEST | FLAG_DF | FLAG_TF | FLAG_IF;

constexpr uint32_t FMASK_ALL = FMASK_NORMAL | FLAG_IOPL | FLAG_NT;

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

// The CpuTestFlags struct allows CPU test flags to be applied en masse. First
// construct it with the set of flags you will be adjusting followed by
// assigning the actual flag state to the verbosely-named members. For example:
//
// CpuTestFlags my_flags(FLAG_CF | FLAG_OF);
//   my_flags.has_carry = (did the operation need to carry results?);
//   my_flags.has_overflow = (did the operation overflow?);
//
// Then apply the flags to the CPU registers:
//   cpu_regs.ApplyTestFlags(my_flags);
//
struct CpuTestFlags {
	constexpr CpuTestFlags() = delete;
	constexpr CpuTestFlags(const uint32_t clear_mask);
	constexpr uint32_t GetClearMask() const;
	constexpr uint32_t GetSetMask() const;

	bool has_carry        = false;
	bool has_odd_parity   = false;
	bool has_auxiliary    = false;
	bool is_zero          = false;
	bool is_sign_negative = false;
	bool has_overflow     = false;

private:
	const uint32_t clear_mask = 0;
};

constexpr CpuTestFlags::CpuTestFlags(const uint32_t _clear_mask)
        : clear_mask(_clear_mask)
{
	assert((clear_mask & ~FMASK_TEST) == 0 &&
	       "Attempting to clear more than the test flags");
}

constexpr uint32_t CpuTestFlags::GetClearMask() const
{
	return clear_mask;
}

constexpr uint32_t CpuTestFlags::GetSetMask() const
{
	// clang-format off
	const uint32_t set_mask = 
		(has_carry        ? FLAG_CF : 0) |
		(has_odd_parity   ? FLAG_PF : 0) |
		(has_auxiliary    ? FLAG_AF : 0) |
		(is_zero          ? FLAG_ZF : 0) |
		(is_sign_negative ? FLAG_SF : 0) |
		(has_overflow     ? FLAG_OF : 0);
	// clang-format on

	assert((set_mask & ~clear_mask) == 0 &&
	       "Attempting to set flags that aren't cleared");
	return set_mask;
}

struct CPU_Regs {
	GenReg32 regs[8] = {};
	GenReg32 ip      = {};
	uint32_t flags   = 0;
	constexpr void ApplyTestFlags(const CpuTestFlags& requested_flags);
};

constexpr void CPU_Regs::ApplyTestFlags(const CpuTestFlags& requested_flags)
{
	// First clear then set the requested flags
	flags &= ~requested_flags.GetClearMask();
	flags |= requested_flags.GetSetMask();
}

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
