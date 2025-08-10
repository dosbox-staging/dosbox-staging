// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dosbox.h"

#include "cpu/callback.h"
#include "cpu/cpu.h"
#include "debug.h"
#include "fpu.h"
#include "hardware/port.h"
#include "lazyflags.h"
#include "cpu/paging.h"
#include "hardware/pic.h"
#include "cpu/registers.h"
#include "tracy.h"

typedef PhysPt EAPoint;
#define SegBase(c)	SegPhys(c)

#define LoadMb(off) mem_readb_inline(off)
#define LoadMw(off) mem_readw_inline(off)
#define LoadMd(off) mem_readd_inline(off)

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

Bits CPU_Core_Full_Run() noexcept
{
	ZoneScoped;
	FullData inst{};
	while (CPU_Cycles-->0) {
#if C_DEBUG
		cycle_count++;
#if C_HEAVY_DEBUG
		if (DEBUG_HeavyIsBreakpoint()) {
			FillFlags();
			return debugCallback;
		};
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

void CPU_Core_Full_Init(void) {

}
