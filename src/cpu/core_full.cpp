#include "dosbox.h"

#include "pic.h"
#include "regs.h"
#include "cpu.h"
#include "lazyflags.h"
#include "fpu.h"
#include "debug.h"
#include "inout.h"
#include "callback.h"


Bit8u PAGE_Readb(PhysPt address);
Bit16u PAGE_Readw(PhysPt address);
Bit32u PAGE_Readd(PhysPt address);

void PAGE_Writeb(PhysPt address,Bit8u val);
void PAGE_Writew(PhysPt address,Bit16u val);
void PAGE_Writed(PhysPt address,Bit32u val);

typedef PhysPt EAPoint;
#define SegBase(c)	SegPhys(c)
#if 1
#define LoadMb(off) mem_readb(off)
#define LoadMw(off) mem_readw(off)
#define LoadMd(off) mem_readd(off)

#define LoadMbs(off) (Bit8s)(LoadMb(off))
#define LoadMws(off) (Bit16s)(LoadMw(off))
#define LoadMds(off) (Bit32s)(LoadMd(off))

#define SaveMb(off,val)	mem_writeb(off,val)
#define SaveMw(off,val)	mem_writew(off,val)
#define SaveMd(off,val)	mem_writed(off,val)

#else

#define LoadMb(off) PAGE_Readb(off)
#define LoadMw(off) PAGE_Readw(off)
#define LoadMd(off) PAGE_Readd(off)

#define LoadMbs(off) (Bit8s)(LoadMb(off))
#define LoadMws(off) (Bit16s)(LoadMw(off))
#define LoadMds(off) (Bit32s)(LoadMd(off))

#define SaveMb(off,val)	PAGE_Writeb(off,val)
#define SaveMw(off,val)	PAGE_Writew(off,val)
#define SaveMd(off,val)	PAGE_Writed(off,val)

#endif

#define LoadD(reg) reg
#define SaveD(reg,val)	reg=val

static EAPoint IPPoint;

#include "core_full/loadwrite.h"
#include "core_full/support.h"
#include "core_full/optable.h"
#include "core_full/ea_lookup.h"
#include "instructions.h"


static INLINE void DecodeModRM(void) {
	inst.rm=Fetchb();
	inst.rm_index=(inst.rm >> 3) & 7;
	inst.rm_eai=inst.rm&07;
	inst.rm_mod=inst.rm>>6;
	/* Decode address of mod/rm if needed */
	if (inst.rm<0xc0) inst.rm_eaa=(inst.prefix & PREFIX_ADDR) ? RMAddress_32() : RMAddress_16();
}

#define LEAVECORE											\
		SaveIP();											\
		FILLFLAGS;

#define EXCEPTION(blah)										\
	{														\
		Bit8u new_num=blah;									\
		LEAVECORE;											\
		Interrupt(new_num);									\
		LoadIP();											\
		goto nextopcode;									\
	}

Bitu Full_DeCode(void) {

	LoadIP();
	flags.type=t_UNKNOWN;
	while (CPU_Cycles>0) {
#if C_DEBUG
		cycle_count++;
#if C_HEAVY_DEBUG
		if (DEBUG_HeavyIsBreakpoint()) {
			LEAVECORE;
			return 1;
		};
#endif
#endif
		inst.start=IPPoint;
		inst.entry=cpu.full.entry;
		inst.prefix=cpu.full.prefix;
restartopcode:
		inst.entry=(inst.entry & 0xffffff00) | Fetchb();

		inst.code=OpCodeTable[inst.entry];
		#include "core_full/load.h"
		#include "core_full/op.h"
		#include "core_full/save.h"
nextopcode:;
		CPU_Cycles--;
	}	
	LEAVECORE;
	return CBRET_NONE;
}


void CPU_Core_Full_Start(void) {
	cpudecoder=&Full_DeCode;
}
