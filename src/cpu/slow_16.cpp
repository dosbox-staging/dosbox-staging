/*
 *  Copyright (C) 2002-2003  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#include "dosbox.h"
#include "mem.h"
#include "cpu.h"
#include "lazyflags.h"
#include "inout.h"
#include "callback.h"
#include "pic.h"
#include "fpu.h"

#if C_DEBUG
#include "debug.h"
#endif

typedef PhysPt EAPoint;
#define SegBase(c)	SegPhys(c)
#define LoadMb(off) mem_readb(off)
#define LoadMw(off) mem_readw(off)
#define LoadMd(off) mem_readd(off)

#define LoadMbs(off) (Bit8s)(LoadMb(off))
#define LoadMws(off) (Bit16s)(LoadMw(off))
#define LoadMds(off) (Bit32s)(LoadMd(off))

#define SaveMb(off,val)	mem_writeb(off,val)
#define SaveMw(off,val)	mem_writew(off,val)
#define SaveMd(off,val)	mem_writed(off,val)

#define LoadRb(reg) reg
#define LoadRw(reg) reg
#define LoadRd(reg) reg

#define SaveRb(reg,val)	reg=val
#define SaveRw(reg,val)	reg=val
#define SaveRd(reg,val)	reg=val

extern Bitu cycle_count;

/* Enable parts of the cpu emulation */
#define CPU_386							//Enable 386 instructions
#define CPU_PREFIX_67					//Enable the 0x67 prefix
#define CPU_PIC_CHECK					//Check for IRQ's on critical moment

#if C_FPU
#define CPU_FPU							//Enable FPU escape instructions
#endif

static struct {
	Bitu prefixes;
	PhysPt segbase;
	PhysPt ip_lookup;
	PhysPt ip_start;
}core_16 ;


#include "instructions.h"
#include "core_16/support.h"
static Bits CPU_Real_16_Slow_Decode_Trap(void);

static Bits CPU_Real_16_Slow_Decode(void) {
decode_start:
	LOADIP;
	flags.type=t_UNKNOWN;
	while (CPU_Cycles>0) {
#if C_DEBUG
		cycle_count++;		
#if C_HEAVY_DEBUG
		LEAVECORE;
		if (DEBUG_HeavyIsBreakpoint()) return 1;
#endif
#endif
		core_16.ip_start=core_16.ip_lookup;
		core_16.prefixes=0;
		lookupEATable=EAPrefixTable[0];
		#include "core_16/main.h"
		CPU_Cycles--;
	}
decode_end:
	LEAVECORE;
	return CBRET_NONE;
}

static Bits CPU_Real_16_Slow_Decode_Trap(void) {

	Bits oldCycles = CPU_Cycles;
	CPU_Cycles = 1;
	Bits ret=CPU_Real_16_Slow_Decode();
	
//	LOG_DEBUG("TRAP: Trap Flag executed");
	Interrupt(1);
	
	CPU_Cycles = oldCycles-1;
	cpudecoder = &CPU_Real_16_Slow_Decode;

	return ret;
}


void CPU_Real_16_Slow_Start(bool big) {
	if (big) E_Exit("Core 16 only runs 16-bit code");
	cpudecoder=&CPU_Real_16_Slow_Decode;
	EAPrefixTable[2]=&GetEA_32_n;
	EAPrefixTable[3]=&GetEA_32_s;
	EAPrefixTable[0]=&GetEA_16_n;
	EAPrefixTable[1]=&GetEA_16_s;

};
