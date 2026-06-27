// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dosbox.h"

#include "cpu/callback.h"
#include "cpu/cpu.h"
#include "cpu/paging.h"
#include "cpu/registers.h"
#include "debugger/debugger.h"
#include "fpu/fpu.h"
#include "hardware/pic.h"
#include "hardware/port.h"
#include "lazyflags.h"

typedef PhysPt EAPoint;
#define SegBase(c)	SegPhys(c)

#define LoadMb(off) mem_readb_inline(off)
#define LoadMw(off) mem_readw_inline(off)
#define LoadMd(off) mem_readd_inline(off)

// Instruction fetches are not data reads, so they must never trigger memory
// read breakpoints (FetchMb), unlike operand data accesses (LoadM*).
#define FetchMb(off) mem_readb_inline<MemOpMode::SkipBreakpoints>(off)
#define FetchMw(off) mem_readw_inline<MemOpMode::SkipBreakpoints>(off)
#define FetchMd(off) mem_readd_inline<MemOpMode::SkipBreakpoints>(off)

#define LoadMbs(off) (int8_t)(LoadMb(off))
#define LoadMws(off) (int16_t)(LoadMw(off))
#define LoadMds(off) (int32_t)(LoadMd(off))

#define SaveMb(off,val)	mem_writeb_inline(off,val)
#define SaveMw(off,val)	mem_writew_inline(off,val)
#define SaveMd(off,val)	mem_writed_inline(off,val)

#define LoadD(reg) reg
#define SaveD(reg,val)	reg=val



#include "core_full/loadwrite.h"
#include "core_full/support.h"
#include "core_full/optable.h"
#include "instructions.h"

#define EXCEPTION(blah)										\
	{														\
		uint8_t new_num=blah;									\
		CPU_Exception(new_num,0);							\
		continue;											\
	}

// The shared helper code above always uses the breakpoint-aware reads. The
// decode loop below is compiled twice via CPU_Core_Full_Run_T (see
// CPU_Core_Full_Run): once honouring memory read breakpoints and once skipping
// them entirely. Re-point the data-read macros at the core's compile-time
// op_mode so the no-breakpoint variant carries zero per-read cost.
#undef LoadMb
#undef LoadMw
#undef LoadMd
#define LoadMb(off) mem_readb_inline<op_mode>(off)
#define LoadMw(off) mem_readw_inline<op_mode>(off)
#define LoadMd(off) mem_readd_inline<op_mode>(off)

template <bool debug_active>
static inline Bits CPU_Core_Full_Run_T() noexcept
{
	[[maybe_unused]] constexpr auto op_mode = debug_active
	                                                ? MemOpMode::WithBreakpoints
	                                                : MemOpMode::SkipBreakpoints;
	FullData inst{};
	while (CPU_Cycles-->0) {
#if C_DEBUGGER
		cycle_count++;
#if C_HEAVY_DEBUGGER
		if constexpr (debug_active) {
			if (DEBUG_HeavyIsBreakpoint()) {
				FillFlags();
				return debugCallback;
			}
		}
#endif
#endif
		LoadIP();
		inst.entry=cpu.code.big*0x200;
		inst.prefix=cpu.code.big;
restartopcode:
		inst.entry=(inst.entry & 0xffffff00) | Fetchb();
		inst.code=OpCodeTable[inst.entry];
		#include "core_full/load.h"
		#include "core_full/op.h"
		#include "core_full/save.h"
nextopcode:;
		SaveIP();
		continue;
illegalopcode:
		LOG(LOG_CPU,LOG_NORMAL)("Illegal opcode");
		CPU_Exception(0x6,0);
	}
	FillFlags();
	return CBRET_NONE;
}

Bits CPU_Core_Full_Run() noexcept
{
#if C_DEBUGGER && C_HEAVY_DEBUGGER
	if (DEBUG_heavy_active) {
		return CPU_Core_Full_Run_T<true>();
	}
#endif
	return CPU_Core_Full_Run_T<false>();
}

void CPU_Core_Full_Init(void) {

}
