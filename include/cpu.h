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

#ifndef DOSBOX_CPU_H
#define DOSBOX_CPU_H

#include "dosbox.h"

#include "support.h"

#ifndef DOSBOX_REGS_H
#include "regs.h"
#endif
#ifndef DOSBOX_MEM_H
#include "mem.h"
#endif

#define CPU_AUTODETERMINE_NONE		0x00
#define CPU_AUTODETERMINE_CORE		0x01
#define CPU_AUTODETERMINE_CYCLES	0x02

#define CPU_AUTODETERMINE_SHIFT		0x02
#define CPU_AUTODETERMINE_MASK		0x03

#define CPU_CYCLES_LOWER_LIMIT		200


#define CPU_ARCHTYPE_MIXED			0xff
#define CPU_ARCHTYPE_386SLOW		0x30
#define CPU_ARCHTYPE_386FAST		0x35
#define CPU_ARCHTYPE_486OLDSLOW		0x40
#define CPU_ARCHTYPE_486NEWSLOW		0x45
#define CPU_ARCHTYPE_PENTIUMSLOW	0x50

/* CPU Cycle Timing */
extern int32_t CPU_Cycles;
extern int32_t CPU_CycleLeft;
extern int32_t CPU_CycleMax;
extern int32_t CPU_OldCycleMax;
extern int32_t CPU_CyclePercUsed;
extern int32_t CPU_CycleLimit;
extern int64_t CPU_IODelayRemoved;
extern bool CPU_CycleAutoAdjust;
extern bool CPU_SkipCycleAutoAdjust;
extern Bitu CPU_AutoDetermineMode;

extern Bitu CPU_ArchitectureType;

extern Bitu CPU_PrefetchQueueSize;

/* Some common Defines */
/* A CPU Handler */
typedef Bits (CPU_Decoder)(void);
extern CPU_Decoder * cpudecoder;

constexpr bool CPU_ReuseCodepages = true;
#if defined(WIN32)
constexpr bool CPU_UseRwxMemProtect = true;
#endif

Bits CPU_Core_Normal_Run(void);
Bits CPU_Core_Normal_Trap_Run(void);
Bits CPU_Core_Simple_Run(void);
Bits CPU_Core_Simple_Trap_Run(void);
Bits CPU_Core_Full_Run(void);
Bits CPU_Core_Dyn_X86_Run(void);
Bits CPU_Core_Dyn_X86_Trap_Run(void);
Bits CPU_Core_Dynrec_Run(void);
Bits CPU_Core_Dynrec_Trap_Run(void);
Bits CPU_Core_Prefetch_Run(void);
Bits CPU_Core_Prefetch_Trap_Run(void);

void CPU_Enable_SkipAutoAdjust(void);
void CPU_Disable_SkipAutoAdjust(void);
void CPU_Reset_AutoAdjust(void);


//CPU Stuff

extern uint16_t parity_lookup[256];

bool CPU_LLDT(Bitu selector);
bool CPU_LTR(Bitu selector);
void CPU_LIDT(Bitu limit,Bitu base);
void CPU_LGDT(Bitu limit,Bitu base);

Bitu CPU_STR(void);
Bitu CPU_SLDT(void);
Bitu CPU_SIDT_base(void);
Bitu CPU_SIDT_limit(void);
Bitu CPU_SGDT_base(void);
Bitu CPU_SGDT_limit(void);

void CPU_ARPL(Bitu & dest_sel,Bitu src_sel);
void CPU_LAR(Bitu selector,Bitu & ar);
void CPU_LSL(Bitu selector,Bitu & limit);

void CPU_SET_CRX(Bitu cr,Bitu value);
bool CPU_WRITE_CRX(Bitu cr,Bitu value);
Bitu CPU_GET_CRX(Bitu cr);
bool CPU_READ_CRX(Bitu cr,uint32_t & retvalue);

bool CPU_WRITE_DRX(Bitu dr,Bitu value);
bool CPU_READ_DRX(Bitu dr,uint32_t & retvalue);

bool CPU_WRITE_TRX(Bitu dr,Bitu value);
bool CPU_READ_TRX(Bitu dr,uint32_t & retvalue);

Bitu CPU_SMSW(void);
bool CPU_LMSW(Bitu word);

void CPU_VERR(Bitu selector);
void CPU_VERW(Bitu selector);

void CPU_JMP(bool use32,Bitu selector,Bitu offset,Bitu oldeip);
void CPU_CALL(bool use32,Bitu selector,Bitu offset,Bitu oldeip);
void CPU_RET(bool use32,Bitu bytes,Bitu oldeip);
void CPU_IRET(bool use32,Bitu oldeip);
void CPU_HLT(Bitu oldeip);

bool CPU_POPF(Bitu use32);
bool CPU_PUSHF(Bitu use32);
bool CPU_CLI(void);
bool CPU_STI(void);

bool CPU_IO_Exception(Bitu port,Bitu size);
void CPU_RunException(void);

void CPU_ENTER(bool use32,Bitu bytes,Bitu level);

#define CPU_INT_SOFTWARE		0x1
#define CPU_INT_EXCEPTION		0x2
#define CPU_INT_HAS_ERROR		0x4
#define CPU_INT_NOIOPLCHECK		0x8

void CPU_Interrupt(Bitu num,Bitu type,Bitu oldeip);
static inline void CPU_HW_Interrupt(Bitu num) {
	CPU_Interrupt(num,0,reg_eip);
}
static inline void CPU_SW_Interrupt(Bitu num,Bitu oldeip) {
	CPU_Interrupt(num,CPU_INT_SOFTWARE,oldeip);
}
static inline void CPU_SW_Interrupt_NoIOPLCheck(Bitu num,Bitu oldeip) {
	CPU_Interrupt(num,CPU_INT_SOFTWARE|CPU_INT_NOIOPLCHECK,oldeip);
}

bool CPU_PrepareException(Bitu which,Bitu error);
void CPU_Exception(Bitu which,Bitu error=0);
void CPU_DebugException(uint32_t triggers,Bitu oldeip);

bool CPU_SetSegGeneral(SegNames seg,Bitu value);
bool CPU_PopSeg(SegNames seg,bool use32);

bool CPU_CPUID(void);
Bitu CPU_Pop16(void);
Bitu CPU_Pop32(void);
void CPU_Push16(Bitu value);
void CPU_Push32(Bitu value);

#define EXCEPTION_DB			1
#define EXCEPTION_UD			6
#define EXCEPTION_TS			10
#define EXCEPTION_NP			11
#define EXCEPTION_SS			12
#define EXCEPTION_GP			13
#define EXCEPTION_PF			14

#define CR0_PROTECTION			0x00000001
#define CR0_MONITORPROCESSOR	0x00000002
#define CR0_FPUEMULATION		0x00000004
#define CR0_TASKSWITCH			0x00000008
#define CR0_FPUPRESENT			0x00000010
#define CR0_PAGING				0x80000000

// reasons for triggering a debug exception
#define DBINT_BP0               0x00000001
#define DBINT_BP1               0x00000002
#define DBINT_BP2               0x00000004
#define DBINT_BP3               0x00000008
#define DBINT_GD                0x00002000
#define DBINT_STEP              0x00004000
#define DBINT_TASKSWITCH        0x00008000

// *********************************************************************
// Descriptor
// *********************************************************************

#define DESC_INVALID				0x00
#define DESC_286_TSS_A				0x01
#define DESC_LDT					0x02
#define DESC_286_TSS_B				0x03
#define DESC_286_CALL_GATE			0x04
#define DESC_TASK_GATE				0x05
#define DESC_286_INT_GATE			0x06
#define DESC_286_TRAP_GATE			0x07

#define DESC_386_TSS_A				0x09
#define DESC_386_TSS_B				0x0b
#define DESC_386_CALL_GATE			0x0c
#define DESC_386_INT_GATE			0x0e
#define DESC_386_TRAP_GATE			0x0f

/* EU/ED Expand UP/DOWN RO/RW Read Only/Read Write NA/A Accessed */
#define DESC_DATA_EU_RO_NA			0x10
#define DESC_DATA_EU_RO_A			0x11
#define DESC_DATA_EU_RW_NA			0x12
#define DESC_DATA_EU_RW_A			0x13
#define DESC_DATA_ED_RO_NA			0x14
#define DESC_DATA_ED_RO_A			0x15
#define DESC_DATA_ED_RW_NA			0x16
#define DESC_DATA_ED_RW_A			0x17

/* N/R Readable  NC/C Confirming A/NA Accessed */
#define DESC_CODE_N_NC_A			0x18
#define DESC_CODE_N_NC_NA			0x19
#define DESC_CODE_R_NC_A			0x1a
#define DESC_CODE_R_NC_NA			0x1b
#define DESC_CODE_N_C_A				0x1c
#define DESC_CODE_N_C_NA			0x1d
#define DESC_CODE_R_C_A				0x1e
#define DESC_CODE_R_C_NA			0x1f

#ifdef _MSC_VER
#pragma pack (1)
#endif

struct S_Descriptor {
#ifdef WORDS_BIGENDIAN
	uint32_t base_0_15	:16;
	uint32_t limit_0_15	:16;
	uint32_t base_24_31	:8;
	uint32_t g			:1;
	uint32_t big			:1;
	uint32_t r			:1;
	uint32_t avl			:1;
	uint32_t limit_16_19	:4;
	uint32_t p			:1;
	uint32_t dpl			:2;
	uint32_t type			:5;
	uint32_t base_16_23	:8;
#else
	uint32_t limit_0_15	:16;
	uint32_t base_0_15	:16;
	uint32_t base_16_23	:8;
	uint32_t type			:5;
	uint32_t dpl			:2;
	uint32_t p			:1;
	uint32_t limit_16_19	:4;
	uint32_t avl			:1;
	uint32_t r			:1;
	uint32_t big			:1;
	uint32_t g			:1;
	uint32_t base_24_31	:8;
#endif
}GCC_ATTRIBUTE(packed);

struct G_Descriptor {
#ifdef WORDS_BIGENDIAN
	uint32_t selector:	16;
	uint32_t offset_0_15	:16;
	uint32_t offset_16_31	:16;
	uint32_t p			:1;
	uint32_t dpl			:2;
	uint32_t type			:5;
	uint32_t reserved		:3;
	uint32_t paramcount	:5;
#else
	uint32_t offset_0_15	:16;
	uint32_t selector		:16;
	uint32_t paramcount	:5;
	uint32_t reserved		:3;
	uint32_t type			:5;
	uint32_t dpl			:2;
	uint32_t p			:1;
	uint32_t offset_16_31	:16;
#endif
} GCC_ATTRIBUTE(packed);

struct TSS_16 {	
    uint16_t back;                 /* Back link to other task */
    uint16_t sp0;				     /* The CK stack pointer */
    uint16_t ss0;					 /* The CK stack selector */
	uint16_t sp1;                  /* The parent KL stack pointer */
    uint16_t ss1;                  /* The parent KL stack selector */
	uint16_t sp2;                  /* Unused */
    uint16_t ss2;                  /* Unused */
    uint16_t ip;                   /* The instruction pointer */
    uint16_t flags;                /* The flags */
    uint16_t ax, cx, dx, bx;       /* The general purpose registers */
    uint16_t sp, bp, si, di;       /* The special purpose registers */
    uint16_t es;                   /* The extra selector */
    uint16_t cs;                   /* The code selector */
    uint16_t ss;                   /* The application stack selector */
    uint16_t ds;                   /* The data selector */
    uint16_t ldt;                  /* The local descriptor table */
} GCC_ATTRIBUTE(packed);

struct TSS_32 {	
    uint32_t back;                /* Back link to other task */
	uint32_t esp0;		         /* The CK stack pointer */
    uint32_t ss0;					 /* The CK stack selector */
	uint32_t esp1;                 /* The parent KL stack pointer */
    uint32_t ss1;                  /* The parent KL stack selector */
	uint32_t esp2;                 /* Unused */
    uint32_t ss2;                  /* Unused */
	uint32_t cr3;                  /* The page directory pointer */
    uint32_t eip;                  /* The instruction pointer */
    uint32_t eflags;               /* The flags */
    uint32_t eax, ecx, edx, ebx;   /* The general purpose registers */
    uint32_t esp, ebp, esi, edi;   /* The special purpose registers */
    uint32_t es;                   /* The extra selector */
    uint32_t cs;                   /* The code selector */
    uint32_t ss;                   /* The application stack selector */
    uint32_t ds;                   /* The data selector */
    uint32_t fs;                   /* And another extra selector */
    uint32_t gs;                   /* ... and another one */
    uint32_t ldt;                  /* The local descriptor table */
} GCC_ATTRIBUTE(packed);

#ifdef _MSC_VER
#pragma pack()
#endif
class Descriptor
{
public:
	Descriptor() : saved{{0, 0}} {}

	void Load(PhysPt address);
	void Save(PhysPt address);

	PhysPt GetBase()
	{
		const auto base = (saved.seg.base_24_31  << 24) |
		                  (saved.seg.base_16_23 << 16) |
		                   saved.seg.base_0_15;
		return static_cast<PhysPt>(base);
	}

	uint32_t GetLimit()
	{
		const auto limit_16_19 = check_cast<uint8_t>(saved.seg.limit_16_19); // 4 bits
		const auto limit_0_15 = check_cast<uint16_t>(saved.seg.limit_0_15); // 16 bits
		const auto limit = (limit_16_19 << 16) | limit_0_15;
		if (saved.seg.g)
			return static_cast<uint32_t>((limit << 12) | 0xFFF);
		return static_cast<uint32_t>(limit);
	}

	uint32_t GetOffset()
	{
		const auto offset_16_31 = check_cast<uint16_t>(saved.gate.offset_16_31); // 16 bits
		const auto offset_0_15 = check_cast<uint16_t>(saved.gate.offset_0_15); // 16 bits
		const auto offset = (offset_16_31 << 16) | offset_0_15;
		return static_cast<uint32_t>(offset);
	}

	uint16_t GetSelector()
	{
		return check_cast<uint16_t>(saved.gate.selector); // 16 bit
	}

	uint8_t Type()
	{
		return check_cast<uint8_t>(saved.seg.type); // 5 bits
	}

	uint8_t Conforming()
	{
		return check_cast<uint8_t>(saved.seg.type & 8); // 5 bit
	}

	uint8_t DPL()
	{
		return check_cast<uint8_t>(saved.seg.dpl); // 2 bit
	}

	uint8_t Big()
	{
		return check_cast<uint8_t>(saved.seg.big); // 1 bit
	}

public:
	union {
		uint32_t fill[2];
		S_Descriptor seg;
		G_Descriptor gate;
	} saved;
};

class DescriptorTable {
public:
	PhysPt	GetBase			(void)			{ return table_base;	}
	Bitu	GetLimit		(void)			{ return table_limit;	}
	void	SetBase			(PhysPt _base)	{ table_base = _base;	}
	void	SetLimit		(Bitu _limit)	{ table_limit= _limit;	}

	bool GetDescriptor	(Bitu selector, Descriptor& desc) {
		selector &= ~7ul;
		const auto nonbitu_selector = check_cast<PhysPt>(selector);
		if (selector>=table_limit) return false;
		desc.Load(table_base + nonbitu_selector);
		return true;
	}
protected:
	PhysPt table_base;
	Bitu table_limit;
};

class GDTDescriptorTable final : public DescriptorTable {
public:
	bool GetDescriptor(Bitu selector, Descriptor& desc) {
		Bitu address = selector & ~7ul;
		const auto nonbitu_address = check_cast<PhysPt>(address);
		if (selector & 4) {
			if (address>=ldt_limit) return false;
			desc.Load(ldt_base + nonbitu_address);
			return true;
		} else {
			if (address>=table_limit) return false;
			desc.Load(table_base + nonbitu_address);
			return true;
		}
	}
	bool SetDescriptor(Bitu selector, Descriptor& desc) {
		Bitu address = selector & ~7ul;
		const auto nonbitu_address = check_cast<PhysPt>(address);
		if (selector & 4) {
			if (address>=ldt_limit) return false;
			desc.Save(ldt_base + nonbitu_address);
			return true;
		} else {
			if (address>=table_limit) return false;
			desc.Save(table_base + nonbitu_address);
			return true;
		}
	} 
	Bitu SLDT(void)	{
		return ldt_value;
	}
	bool LLDT(Bitu value)	{
		if ((value&0xfffc)==0) {
			ldt_value=0;
			ldt_base=0;
			ldt_limit=0;
			return true;
		}
		Descriptor desc;
		if (!GetDescriptor(value,desc)) return !CPU_PrepareException(EXCEPTION_GP,value);
		if (desc.Type()!=DESC_LDT) return !CPU_PrepareException(EXCEPTION_GP,value);
		if (!desc.saved.seg.p) return !CPU_PrepareException(EXCEPTION_NP,value);
		ldt_base=desc.GetBase();
		ldt_limit=desc.GetLimit();
		ldt_value=value;
		return true;
	}
private:
	PhysPt ldt_base;
	Bitu ldt_limit;
	Bitu ldt_value;
};

class TSS_Descriptor final : public Descriptor {
public:
	Bitu IsBusy(void) {
		return saved.seg.type & 2;
	}
	Bitu Is386(void) {
		return saved.seg.type & 8;
	}
	void SetBusy(bool busy) {
		if (busy) saved.seg.type|=2;
		else saved.seg.type&=~2;
	}
};


struct CPUBlock {
	Bitu cpl;							/* Current Privilege */
	Bitu mpl;
	Bitu cr0;
	bool pmode;							/* Is Protected mode enabled */
	GDTDescriptorTable gdt;
	DescriptorTable idt;
	struct {
		Bitu mask,notmask;
		bool big;
	} stack;
	struct {
		bool big;
	} code;
	struct {
		Bitu cs,eip;
		CPU_Decoder * old_decoder;
	} hlt;
	struct {
		Bitu which,error;
	} exception;
	Bits direction;
	bool trap_skip;
	uint32_t drx[8];
	uint32_t trx[8];
};

extern CPUBlock cpu;

void CPU_SetFlags(const uint32_t word, uint32_t mask);
void CPU_SetFlagsd(const uint32_t word);
void CPU_SetFlagsw(const uint32_t word);

#endif
