/*
 *  Copyright (C) 2002-2004  The DOSBox Team
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "dosbox.h"
#include "mem.h"
#include "paging.h"
#include "regs.h"
#include "lazyflags.h"
#include "cpu.h"
#include "debug.h"

#define LINK_TOTAL		(64*1024)

PagingBlock paging;



Bitu PageHandler::readb(PhysPt addr) {
	E_Exit("No byte handler for read from %d",addr);	
	return 0;
}
Bitu PageHandler::readw(PhysPt addr) {
	return 
		(readb(addr+0) << 0) |
		(readb(addr+1) << 8);
}
Bitu PageHandler::readd(PhysPt addr) {
	return 
		(readb(addr+0) << 0)  |
		(readb(addr+1) << 8)  |
		(readb(addr+2) << 16) |
		(readb(addr+3) << 24);
}

void PageHandler::writeb(PhysPt addr,Bitu val) {
	E_Exit("No byte handler for write to %d",addr);	
};

void PageHandler::writew(PhysPt addr,Bitu val) {
	writeb(addr+0,(Bit8u) (val >> 0));
	writeb(addr+1,(Bit8u) (val >> 8));
}
void PageHandler::writed(PhysPt addr,Bitu val) {
	writeb(addr+0,(Bit8u) (val >> 0));
	writeb(addr+1,(Bit8u) (val >> 8));
	writeb(addr+2,(Bit8u) (val >> 16));
	writeb(addr+3,(Bit8u) (val >> 24));
};

HostPt PageHandler::GetHostPt(Bitu phys_page) {
	return 0;
}


struct PF_Entry {
	Bitu cs;
	Bitu eip;
	Bitu page_addr;
};

#define PF_QUEUESIZE 16
struct {
	Bitu used;
	PF_Entry entries[PF_QUEUESIZE];
} pf_queue;

static Bits PageFaultCore(void) {
	CPU_CycleLeft+=CPU_Cycles;
	CPU_Cycles=1;
	Bitu ret=CPU_Core_Full_Run();
	CPU_CycleLeft+=CPU_Cycles;
	if (ret<0) E_Exit("Got a dosbox close machine in pagefault core?");
	if (ret) 
		return ret;
	if (!pf_queue.used) E_Exit("PF Core without PF");
	PF_Entry * entry=&pf_queue.entries[pf_queue.used-1];
	X86PageEntry pentry;
	pentry.load=phys_readd(entry->page_addr);
	if (pentry.block.p && entry->cs == SegValue(cs) && entry->eip==reg_eip)
		return -1;
	return 0;
}
#if C_DEBUG
Bitu DEBUG_EnableDebugger(void);
#endif

bool first=false;

void PAGING_PageFault(PhysPt lin_addr,Bitu page_addr,Bitu type) {
	/* Save the state of the cpu cores */
	LazyFlags old_lflags;
	memcpy(&old_lflags,&lflags,sizeof(LazyFlags));
	CPU_Decoder * old_cpudecoder;
	old_cpudecoder=cpudecoder;
	cpudecoder=&PageFaultCore;
	paging.cr2=lin_addr;
	PF_Entry * entry=&pf_queue.entries[pf_queue.used++];
	LOG(LOG_PAGING,LOG_NORMAL)("PageFault at %X type %d queue %d",lin_addr,type,pf_queue.used);
//	LOG_MSG("EAX:%04X ECX:%04X EDX:%04X EBX:%04X",reg_eax,reg_ecx,reg_edx,reg_ebx);
//	LOG_MSG("CS:%04X EIP:%08X SS:%04x SP:%08X",SegValue(cs),reg_eip,SegValue(ss),reg_esp);
	entry->cs=SegValue(cs);
	entry->eip=reg_eip;
	entry->page_addr=page_addr;
	//Caused by a write by default?
	CPU_Exception(14,0x2 );
#if C_DEBUG
//	DEBUG_EnableDebugger();
#endif
	DOSBOX_RunMachine();
	pf_queue.used--;
	LOG(LOG_PAGING,LOG_NORMAL)("Left PageFault for %x queue %d",lin_addr,pf_queue.used);
	memcpy(&lflags,&old_lflags,sizeof(LazyFlags));
	cpudecoder=old_cpudecoder;
//	LOG_MSG("SS:%04x SP:%08X",SegValue(ss),reg_esp);
}
 
class InitPageHandler : public PageHandler {
public:
	InitPageHandler() {flags=PFLAG_INIT|PFLAG_NOCODE;}
	Bitu readb(PhysPt addr) {
		InitPage(addr);
		return mem_readb(addr);
	}
	Bitu readw(PhysPt addr) {
		InitPage(addr);
		return mem_readw(addr);
	}
	Bitu readd(PhysPt addr) {
		InitPage(addr);
		return mem_readd(addr);
	}
	void writeb(PhysPt addr,Bitu val) {
		InitPage(addr);
		mem_writeb(addr,val);
	}
	void writew(PhysPt addr,Bitu val) {
		InitPage(addr);
		mem_writew(addr,val);
	}
	void writed(PhysPt addr,Bitu val) {
		InitPage(addr);
		mem_writed(addr,val);
	}
	void InitPage(Bitu lin_addr) {
		Bitu lin_page=lin_addr >> 12;
		Bitu phys_page;
		if (paging.enabled) {
			Bitu d_index=lin_page >> 10;
			Bitu t_index=lin_page & 0x3ff;
			Bitu table_addr=(paging.base.page<<12)+d_index*4;
			X86PageEntry table;
			table.load=phys_readd(table_addr);
			if (!table.block.p) {
				LOG(LOG_PAGING,LOG_ERROR)("NP Table");
				PAGING_PageFault(lin_addr,table_addr,0);
				table.load=phys_readd(table_addr);
				if (!table.block.p)
					E_Exit("Pagefault didn't correct table");
			}
			table.block.a=table.block.d=1;		//Set access/Dirty
			phys_writed(table_addr,table.load);
			X86PageEntry entry;
			Bitu entry_addr=(table.block.base<<12)+t_index*4;
			entry.load=phys_readd(entry_addr);
			if (!entry.block.p) {
				LOG(LOG_PAGING,LOG_ERROR)("NP Page");
				PAGING_PageFault(lin_addr,entry_addr,0);
				entry.load=phys_readd(entry_addr);
				if (!entry.block.p)
					E_Exit("Pagefault didn't correct page");
			}
			entry.block.a=entry.block.d=1;		//Set access/Dirty
			phys_writed(entry_addr,entry.load);
			phys_page=entry.block.base;
		} else {
			if (lin_page<LINK_START) phys_page=paging.firstmb[lin_page];
			else phys_page=lin_page;
		}
		PAGING_LinkPage(lin_page,phys_page);
	}
};

bool PAGING_MakePhysPage(Bitu & page) {
	if (paging.enabled) {
		Bitu d_index=page >> 10;
		Bitu t_index=page & 0x3ff;
		X86PageEntry table;
		table.load=phys_readd((paging.base.page<<12)+d_index*4);
		if (!table.block.p) return false;
		X86PageEntry entry;
		entry.load=phys_readd((table.block.base<<12)+t_index*4);
		if (!entry.block.p) return false;
		page=entry.block.base;
		} else {
			if (page<LINK_START) page=paging.firstmb[page];
			//Else keep it the same
		}
		return true;
}

static InitPageHandler init_page_handler;


Bitu PAGING_GetDirBase(void) {
	return paging.cr3;
}

void PAGING_InitTLB(void) {
	for (Bitu i=0;i<TLB_SIZE;i++) {
		paging.tlb.read[i]=0;
		paging.tlb.write[i]=0;
		paging.tlb.handler[i]=&init_page_handler;
	}
	paging.links.used=0;
}

void PAGING_ClearTLB(void) {
	Bit32u * entries=&paging.links.entries[0];
	for (;paging.links.used>0;paging.links.used--) {
		Bitu page=*entries++;
		paging.tlb.read[page]=0;
		paging.tlb.write[page]=0;
		paging.tlb.handler[page]=&init_page_handler;
	}
	paging.links.used=0;
}

void PAGING_UnlinkPages(Bitu lin_page,Bitu pages) {
	for (;pages>0;pages--) {
		paging.tlb.read[lin_page]=0;
		paging.tlb.write[lin_page]=0;
		paging.tlb.handler[lin_page]=&init_page_handler;
	}
}

void PAGING_LinkPage(Bitu lin_page,Bitu phys_page) {
	PageHandler * handler=MEM_GetPageHandler(phys_page);
	Bitu lin_base=lin_page << 12;
	if (lin_page>=TLB_SIZE || phys_page>=TLB_SIZE) 
		E_Exit("Illegal page");

	if (paging.links.used>=PAGING_LINKS) {
		LOG(LOG_PAGING,LOG_NORMAL)("Not enough paging links, resetting cache");
		PAGING_ClearTLB();
	}

	HostPt host_mem=handler->GetHostPt(phys_page);
	paging.tlb.phys_page[lin_page]=phys_page;
	if (handler->flags & PFLAG_READABLE) paging.tlb.read[lin_page]=host_mem-lin_base;
	else paging.tlb.read[lin_page]=0;
	if (handler->flags & PFLAG_WRITEABLE) paging.tlb.write[lin_page]=host_mem-lin_base;
	else paging.tlb.write[lin_page]=0;

	paging.links.entries[paging.links.used++]=lin_page;
	paging.tlb.handler[lin_page]=handler;
}

void PAGING_MapPage(Bitu lin_page,Bitu phys_page) {
	if (lin_page<LINK_START) {
		paging.firstmb[lin_page]=phys_page;
		paging.tlb.read[lin_page]=0;
		paging.tlb.write[lin_page]=0;
		paging.tlb.handler[lin_page]=&init_page_handler;
	} else {
		PAGING_LinkPage(lin_page,phys_page);
	}
}

void PAGING_SetDirBase(Bitu cr3) {
	paging.cr3=cr3;
	
	paging.base.page=cr3 >> 12;
	paging.base.addr=cr3 & ~4095;
//	LOG(LOG_PAGING,LOG_NORMAL)("CR3:%X Base %X",cr3,paging.base.page);
	if (paging.enabled) {
		PAGING_ClearTLB();
	}
}

void PAGING_Enable(bool enabled) {
	/* If paging is disable we work from a default paging table */
	if (paging.enabled==enabled) return;
	paging.enabled=enabled;
	if (!enabled) {
//		LOG(LOG_PAGING,LOG_NORMAL)("Disabled");
	} else {
		if (cpudecoder==CPU_Core_Simple_Run) {
			LOG_MSG("CPU core simple won't run this game,switching to normal");
			cpudecoder=CPU_Core_Normal_Run;
			CPU_CycleLeft+=CPU_Cycles;
			CPU_Cycles=0;
		}
//		LOG(LOG_PAGING,LOG_NORMAL)("Enabled");
		PAGING_SetDirBase(paging.cr3);
	}
	PAGING_ClearTLB();
}

bool PAGING_Enabled(void) {
	return paging.enabled;
}

void PAGING_Init(Section * sec) {
	/* Setup default Page Directory, force it to update */
	paging.enabled=false;
	PAGING_InitTLB();
	Bitu i;
	for (i=0;i<LINK_START;i++) {
		paging.firstmb[i]=i;
	}
	pf_queue.used=0;
}

