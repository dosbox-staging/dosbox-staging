#include "dosbox.h"

#include "pic.h"
#include "regs.h"
#include "cpu.h"
#include "debug.h"
#include "inout.h"
#include "callback.h"

typedef PhysPt EAPoint;
#define SegBase(c)	SegPhys(c)
#define LoadMb(off) mem_readb_inline(off)
#define LoadMw(off) mem_readw_inline(off)
#define LoadMd(off) mem_readd_inline(off)

#define LoadMbs(off) (Bit8s)(LoadMb(off))
#define LoadMws(off) (Bit16s)(LoadMw(off))
#define LoadMds(off) (Bit32s)(LoadMd(off))

#define SaveMb(off,val)	mem_writeb_inline(off,val)
#define SaveMw(off,val)	mem_writew_inline(off,val)
#define SaveMd(off,val)	mem_writed_inline(off,val)

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


Bitu Full_DeCode(void) {
	LoadIP();
	while (CPU_Cycles>0) {
#if C_DEBUG
		cycle_count++;
#endif
		CPU_Cycles--;
restartopcode:
		inst.entry=(inst.entry & 0xffffff00) | Fetchb();

		inst.code=OpCodeTable[inst.entry];
		#include "core_full/load.h"
		#include "core_full/op.h"
		#include "core_full/save.h"
nextopcode:		
		inst.prefix=0;
		inst.entry=0;
	}	
	SaveIP();
	return 0;
}


void CPU_Core_Full_Start(void) {
	cpudecoder=&Full_DeCode;
}