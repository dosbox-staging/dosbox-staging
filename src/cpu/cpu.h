// SPDX-FileCopyrightText:  2021-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_CPU_H
#define DOSBOX_CPU_H

#include "dosbox.h"

#include <string>

#include "config/config.h"
#include "misc/support.h"

#ifndef DOSBOX_REGS_H
	#include "cpu/registers.h"
#endif
#ifndef DOSBOX_MEM_H
	#include "hardware/memory.h"
#endif

constexpr auto CpuCyclesMin = 50;
constexpr auto CpuCyclesMax = 2'000'000;

constexpr auto CpuCyclesRealModeDefault      = 3000;
constexpr auto CpuCyclesProtectedModeDefault = 60000;
constexpr auto CpuThrottleDefault            = false;

enum class ArchitectureType {
	Intel86         = 0x05,
	Intel186        = 0x15,
	Intel286        = 0x25,
	Intel386Slow    = 0x30,
	Intel386Fast    = 0x35,
	Intel486OldSlow = 0x40,
	Intel486NewSlow = 0x45,
	Pentium         = 0x50,
	PentiumMmx      = 0x55,
	Mixed           = 0xff,
};

// Current cycles values
extern int CPU_Cycles;
extern int CPU_CycleLeft;

// Cycles settings for both "legacy" and "modern" modes
extern bool CPU_CycleAutoAdjust;
extern int CPU_CycleMax;
extern int CPU_CyclePercUsed;
extern int CPU_CycleLimit;

extern int64_t CPU_IODelayRemoved;

struct CpuAutoDetermineMode {
	bool auto_core   = false;
	bool auto_cycles = false;
};

extern ArchitectureType CPU_ArchitectureType;
extern Bitu CPU_PrefetchQueueSize;

inline bool is_cpu_286_or_better()
{
	return CPU_ArchitectureType >= ArchitectureType::Intel286;
}

inline bool is_cpu_386_or_better()
{
	return CPU_ArchitectureType >= ArchitectureType::Intel386Slow;
}

inline bool is_cpu_486_or_better()
{
	return CPU_ArchitectureType >= ArchitectureType::Intel486OldSlow;
}

inline bool is_cpu_586_or_better()
{
	return CPU_ArchitectureType >= ArchitectureType::Pentium;
}

void CPU_AddConfigSection(const ConfigPtr& conf);

void CPU_Init();
void CPU_Destroy();

uint8_t CPU_GetLastInterrupt();

void CPU_RestoreRealModeCyclesConfig();

std::string CPU_GetCyclesConfigAsString();

// A CPU handler
typedef Bits(CPU_Decoder)();

extern CPU_Decoder* cpudecoder;

constexpr bool CPU_ReuseCodepages = true;
#if defined(WIN32)
constexpr bool CPU_UseRwxMemProtect = true;
#endif

Bits CPU_Core_Normal_Run() noexcept;
Bits CPU_Core_Normal_Trap_Run() noexcept;
Bits CPU_Core_Simple_Run() noexcept;
Bits CPU_Core_Simple_Trap_Run() noexcept;
Bits CPU_Core_Full_Run() noexcept;
Bits CPU_Core_Dyn_X86_Run() noexcept;
Bits CPU_Core_Dyn_X86_Trap_Run() noexcept;
Bits CPU_Core_Dynrec_Run() noexcept;
Bits CPU_Core_Dynrec_Trap_Run() noexcept;
Bits CPU_Core_Prefetch_Run() noexcept;
Bits CPU_Core_Prefetch_Trap_Run() noexcept;

void CPU_ResetAutoAdjust();

extern uint16_t parity_lookup[256];

bool CPU_LLDT(Bitu selector);
bool CPU_LTR(Bitu selector);
void CPU_LIDT(Bitu limit, Bitu base);
void CPU_LGDT(Bitu limit, Bitu base);

Bitu CPU_STR();
Bitu CPU_SLDT();
Bitu CPU_SIDT_base();
Bitu CPU_SIDT_limit();
Bitu CPU_SGDT_base();
Bitu CPU_SGDT_limit();

void CPU_ARPL(Bitu& dest_sel, Bitu src_sel);
void CPU_LAR(Bitu selector, Bitu& ar);
void CPU_LSL(Bitu selector, Bitu& limit);

void CPU_SET_CRX(Bitu cr, Bitu value);
bool CPU_WRITE_CRX(Bitu cr, Bitu value);
Bitu CPU_GET_CRX(Bitu cr);
bool CPU_READ_CRX(Bitu cr, uint32_t& retvalue);

bool CPU_WRITE_DRX(Bitu dr, Bitu value);
bool CPU_READ_DRX(Bitu dr, uint32_t& retvalue);

bool CPU_WRITE_TRX(Bitu dr, Bitu value);
bool CPU_READ_TRX(Bitu dr, uint32_t& retvalue);

Bitu CPU_SMSW();
bool CPU_LMSW(Bitu word);

void CPU_VERR(Bitu selector);
void CPU_VERW(Bitu selector);

void CPU_JMP(bool use32, Bitu selector, Bitu offset, Bitu oldeip);
void CPU_CALL(bool use32, Bitu selector, Bitu offset, Bitu oldeip);
void CPU_RET(bool use32, Bitu bytes, Bitu oldeip);
void CPU_IRET(bool use32, Bitu oldeip);
void CPU_HLT(Bitu oldeip);

bool CPU_POPF(Bitu use32);
bool CPU_PUSHF(Bitu use32);
bool CPU_CLI();
bool CPU_STI();

bool CPU_IO_Exception(Bitu port, Bitu size);
void CPU_RunException();

void CPU_ENTER(bool use32, Bitu bytes, Bitu level);

#define CPU_INT_SOFTWARE    0x1
#define CPU_INT_EXCEPTION   0x2
#define CPU_INT_HAS_ERROR   0x4
#define CPU_INT_NOIOPLCHECK 0x8

void CPU_Interrupt(Bitu num, Bitu type, Bitu oldeip);
static inline void CPU_HW_Interrupt(Bitu num)
{
	CPU_Interrupt(num, 0, reg_eip);
}
static inline void CPU_SW_Interrupt(Bitu num, Bitu oldeip)
{
	CPU_Interrupt(num, CPU_INT_SOFTWARE, oldeip);
}
static inline void CPU_SW_Interrupt_NoIOPLCheck(Bitu num, Bitu oldeip)
{
	CPU_Interrupt(num, CPU_INT_SOFTWARE | CPU_INT_NOIOPLCHECK, oldeip);
}

bool CPU_PrepareException(Bitu which, Bitu error);
void CPU_Exception(Bitu which, Bitu error = 0);
void CPU_DebugException(uint32_t triggers, Bitu oldeip);

bool CPU_SetSegGeneral(SegNames seg, Bitu value);
bool CPU_PopSeg(SegNames seg, bool use32);

bool CPU_CPUID();

void CPU_ReadTSC();

uint16_t CPU_Pop16();
uint32_t CPU_Pop32();

void CPU_Push16(const uint16_t value);
void CPU_Push32(const uint32_t value);

#define EXCEPTION_DB 1
#define EXCEPTION_UD 6
#define EXCEPTION_TS 10
#define EXCEPTION_NP 11
#define EXCEPTION_SS 12
#define EXCEPTION_GP 13
#define EXCEPTION_PF 14

#define CR0_PROTECTION       0x00000001
#define CR0_MONITORPROCESSOR 0x00000002
#define CR0_FPUEMULATION     0x00000004
#define CR0_TASKSWITCH       0x00000008
#define CR0_FPUPRESENT       0x00000010
#define CR0_PAGING           0x80000000

// Reasons for triggering a debug exception
#define DBINT_BP0        0x00000001
#define DBINT_BP1        0x00000002
#define DBINT_BP2        0x00000004
#define DBINT_BP3        0x00000008
#define DBINT_GD         0x00002000
#define DBINT_STEP       0x00004000
#define DBINT_TASKSWITCH 0x00008000

// *********************************************************************
// Descriptor
// *********************************************************************

#define DESC_INVALID       0x00
#define DESC_286_TSS_A     0x01
#define DESC_LDT           0x02
#define DESC_286_TSS_B     0x03
#define DESC_286_CALL_GATE 0x04
#define DESC_TASK_GATE     0x05
#define DESC_286_INT_GATE  0x06
#define DESC_286_TRAP_GATE 0x07

#define DESC_386_TSS_A     0x09
#define DESC_386_TSS_B     0x0b
#define DESC_386_CALL_GATE 0x0c
#define DESC_386_INT_GATE  0x0e
#define DESC_386_TRAP_GATE 0x0f

// EU/ED Expand UP/DOWN RO/RW Read Only/Read Write NA/A Accessed
#define DESC_DATA_EU_RO_NA 0x10
#define DESC_DATA_EU_RO_A  0x11
#define DESC_DATA_EU_RW_NA 0x12
#define DESC_DATA_EU_RW_A  0x13
#define DESC_DATA_ED_RO_NA 0x14
#define DESC_DATA_ED_RO_A  0x15
#define DESC_DATA_ED_RW_NA 0x16
#define DESC_DATA_ED_RW_A  0x17

// N/R Readable  NC/C Confirming A/NA Accessed
#define DESC_CODE_N_NC_A  0x18
#define DESC_CODE_N_NC_NA 0x19
#define DESC_CODE_R_NC_A  0x1a
#define DESC_CODE_R_NC_NA 0x1b
#define DESC_CODE_N_C_A   0x1c
#define DESC_CODE_N_C_NA  0x1d
#define DESC_CODE_R_C_A   0x1e
#define DESC_CODE_R_C_NA  0x1f

#ifdef _MSC_VER
	#pragma pack(1)
#endif

struct S_Descriptor {
#ifdef WORDS_BIGENDIAN
	uint32_t base_0_15 : 16;
	uint32_t limit_0_15 : 16;
	uint32_t base_24_31 : 8;
	uint32_t g : 1;
	uint32_t big : 1;
	uint32_t r : 1;
	uint32_t avl : 1;
	uint32_t limit_16_19 : 4;
	uint32_t p : 1;
	uint32_t dpl : 2;
	uint32_t type : 5;
	uint32_t base_16_23 : 8;
#else
	uint32_t limit_0_15 : 16;
	uint32_t base_0_15 : 16;
	uint32_t base_16_23 : 8;
	uint32_t type : 5;
	uint32_t dpl : 2;
	uint32_t p : 1;
	uint32_t limit_16_19 : 4;
	uint32_t avl : 1;
	uint32_t r : 1;
	uint32_t big : 1;
	uint32_t g : 1;
	uint32_t base_24_31 : 8;
#endif
} GCC_ATTRIBUTE(packed);

struct G_Descriptor {
#ifdef WORDS_BIGENDIAN
	uint32_t selector : 16;
	uint32_t offset_0_15 : 16;
	uint32_t offset_16_31 : 16;
	uint32_t p : 1;
	uint32_t dpl : 2;
	uint32_t type : 5;
	uint32_t reserved : 3;
	uint32_t paramcount : 5;
#else
	uint32_t offset_0_15 : 16;
	uint32_t selector : 16;
	uint32_t paramcount : 5;
	uint32_t reserved : 3;
	uint32_t type : 5;
	uint32_t dpl : 2;
	uint32_t p : 1;
	uint32_t offset_16_31 : 16;
#endif
} GCC_ATTRIBUTE(packed);

struct TSS_16 {
	uint16_t back;           // Back link to other task
	uint16_t sp0;            // The CK stack pointer
	uint16_t ss0;            // The CK stack selector
	uint16_t sp1;            // The parent KL stack pointer
	uint16_t ss1;            // The parent KL stack selector
	uint16_t sp2;            // Unused
	uint16_t ss2;            // Unused
	uint16_t ip;             // The instruction pointer
	uint16_t flags;          // The flags
	uint16_t ax, cx, dx, bx; // The general purpose registers
	uint16_t sp, bp, si, di; // The special purpose registers
	uint16_t es;             // The extra selector
	uint16_t cs;             // The code selector
	uint16_t ss;             // The application stack selector
	uint16_t ds;             // The data selector
	uint16_t ldt;            // The local descriptor table
} GCC_ATTRIBUTE(packed);

struct TSS_32 {
	uint32_t back;               // Back link to other task
	uint32_t esp0;               // The CK stack pointer
	uint32_t ss0;                // The CK stack selector
	uint32_t esp1;               // The parent KL stack pointer
	uint32_t ss1;                // The parent KL stack selector
	uint32_t esp2;               // Unused
	uint32_t ss2;                // Unused
	uint32_t cr3;                // The page directory pointer
	uint32_t eip;                // The instruction pointer
	uint32_t eflags;             // The flags
	uint32_t eax, ecx, edx, ebx; // The general purpose registers
	uint32_t esp, ebp, esi, edi; // The special purpose registers
	uint32_t es;                 // The extra selector
	uint32_t cs;                 // The code selector
	uint32_t ss;                 // The application stack selector
	uint32_t ds;                 // The data selector
	uint32_t fs;                 // And another extra selector
	uint32_t gs;                 // ... and another one
	uint32_t ldt;                // The local descriptor table
} GCC_ATTRIBUTE(packed);

#ifdef _MSC_VER
	#pragma pack()
#endif
class Descriptor {
public:
	Descriptor()
	        : saved{{0, 0}}
	{}

	void Load(PhysPt address);
	void Save(PhysPt address);

	PhysPt GetBase()
	{
		const auto base = (saved.seg.base_24_31 << 24) |
		                  (saved.seg.base_16_23 << 16) |
		                  saved.seg.base_0_15;
		return static_cast<PhysPt>(base);
	}

	uint32_t GetLimit()
	{
		const auto limit_16_19 = check_cast<uint8_t>(saved.seg.limit_16_19); // 4 bits
		const auto limit_0_15 = check_cast<uint16_t>(saved.seg.limit_0_15); // 16 bits
		const auto limit = (limit_16_19 << 16) | limit_0_15;
		if (saved.seg.g) {
			return static_cast<uint32_t>((limit << 12) | 0xFFF);
		}
		return static_cast<uint32_t>(limit);
	}

	uint32_t GetOffset()
	{
		const auto offset_16_31 = check_cast<uint16_t>(
		        saved.gate.offset_16_31); // 16 bits
		const auto offset_0_15 = check_cast<uint16_t>(
		        saved.gate.offset_0_15); // 16 bits
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
	PhysPt GetBase()
	{
		return table_base;
	}
	Bitu GetLimit()
	{
		return table_limit;
	}
	void SetBase(PhysPt _base)
	{
		table_base = _base;
	}
	void SetLimit(Bitu _limit)
	{
		table_limit = _limit;
	}

	bool GetDescriptor(Bitu selector, Descriptor& desc)
	{
		selector &= ~7ul;
		const auto nonbitu_selector = check_cast<PhysPt>(selector);
		if (selector >= table_limit) {
			return false;
		}
		desc.Load(table_base + nonbitu_selector);
		return true;
	}

protected:
	PhysPt table_base;
	Bitu table_limit;
};

class GDTDescriptorTable final : public DescriptorTable {
public:
	bool GetDescriptor(Bitu selector, Descriptor& desc)
	{
		Bitu address               = selector & ~7ul;
		const auto nonbitu_address = check_cast<PhysPt>(address);
		if (selector & 4) {
			if (address >= ldt_limit) {
				return false;
			}
			desc.Load(ldt_base + nonbitu_address);
			return true;
		} else {
			if (address >= table_limit) {
				return false;
			}
			desc.Load(table_base + nonbitu_address);
			return true;
		}
	}
	bool SetDescriptor(Bitu selector, Descriptor& desc)
	{
		Bitu address               = selector & ~7ul;
		const auto nonbitu_address = check_cast<PhysPt>(address);
		if (selector & 4) {
			if (address >= ldt_limit) {
				return false;
			}
			desc.Save(ldt_base + nonbitu_address);
			return true;
		} else {
			if (address >= table_limit) {
				return false;
			}
			desc.Save(table_base + nonbitu_address);
			return true;
		}
	}
	Bitu SLDT()
	{
		return ldt_value;
	}
	bool LLDT(Bitu value)
	{
		if ((value & 0xfffc) == 0) {
			ldt_value = 0;
			ldt_base  = 0;
			ldt_limit = 0;
			return true;
		}
		Descriptor desc;
		if (!GetDescriptor(value, desc)) {
			return !CPU_PrepareException(EXCEPTION_GP, value);
		}
		if (desc.Type() != DESC_LDT) {
			return !CPU_PrepareException(EXCEPTION_GP, value);
		}
		if (!desc.saved.seg.p) {
			return !CPU_PrepareException(EXCEPTION_NP, value);
		}
		ldt_base  = desc.GetBase();
		ldt_limit = desc.GetLimit();
		ldt_value = value;
		return true;
	}

private:
	PhysPt ldt_base;
	Bitu ldt_limit;
	Bitu ldt_value;
};

class TSS_Descriptor final : public Descriptor {
public:
	Bitu IsBusy()
	{
		return saved.seg.type & 2;
	}
	Bitu Is386()
	{
		return saved.seg.type & 8;
	}
	void SetBusy(bool busy)
	{
		if (busy) {
			saved.seg.type |= 2;
		} else {
			saved.seg.type &= ~2;
		}
	}
};

struct CPUBlock {
	// Current privilege
	Bitu cpl; 

	Bitu mpl;
	Bitu cr0;

	// Is protected mode enabled
	bool pmode; 

	GDTDescriptorTable gdt;
	DescriptorTable idt;

	struct {
		Bitu mask, notmask;
		bool big;
	} stack;

	struct {
		bool big;
	} code;

	struct {
		Bitu cs, eip;
		CPU_Decoder* old_decoder;
	} hlt;

	struct {
		Bitu which, error;
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

// Whether the emulator should execute HLT instruction upon detecting the guest
// program is idle
bool CPU_ShouldHltOnIdle();

#endif
