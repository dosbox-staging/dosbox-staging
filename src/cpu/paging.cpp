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

#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "dosbox.h"
#include "mem.h"
#include "paging.h"
#include "regs.h"

#define LINK_TOTAL		(64*1024)

PagingBlock paging;
static Bit32u mapfirstmb[LINK_START];

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


class InitPageHandler : public PageHandler {
public:
	InitPageHandler() {flags=0;}
	void AddPageLink(Bitu lin_page, Bitu phys_page) {
		assert(0);
	}
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
	void InitPage(Bitu addr) {
		Bitu lin_page=addr >> 12;
		Bitu phys_page;
		if (paging.enabled) {
			E_Exit("No paging support");
		} else {
			if (lin_page<LINK_START) phys_page=mapfirstmb[lin_page];
			else phys_page=lin_page;
		}
		PAGING_LinkPage(lin_page,phys_page);
	}
};


static InitPageHandler init_page_handler;


Bitu PAGING_GetDirBase(void) {
	return paging.cr3;
}

void PAGING_ClearTLB(void) {
	LOG(LOG_PAGING,LOG_NORMAL)("Clearing TLB");
	Bitu i;
	for (i=0;i<LINK_START;i++) {
		paging.tlb.read[i]=0;
		paging.tlb.write[i]=0;
		paging.tlb.handler[i]=&init_page_handler;
	}
	MEM_UnlinkPages();
}

void PAGING_InitTLB(void) {
	for (Bitu i=0;i<TLB_SIZE;i++) {
		paging.tlb.read[i]=0;
		paging.tlb.write[i]=0;
		paging.tlb.handler[i]=&init_page_handler;

	}
}

void PAGING_ClearTLBEntries(Bitu pages,Bit32u * entries) {
	for (;pages>0;pages--) {
		Bitu page=*entries++;
		paging.tlb.read[page]=0;
		paging.tlb.write[page]=0;
		paging.tlb.handler[page]=&init_page_handler;
	}
}

void PAGING_LinkPage(Bitu lin_page,Bitu phys_page) {

	PageHandler * handler=MEM_GetPageHandler(phys_page);
	Bitu lin_base=lin_page << 12;

	HostPt host_mem=handler->GetHostPt(phys_page);
	paging.tlb.phys_page[lin_page]=phys_page;
	if (handler->flags & PFLAG_READABLE) paging.tlb.read[lin_page]=host_mem-lin_base;
	else paging.tlb.read[lin_page]=0;
	if (handler->flags & PFLAG_WRITEABLE) paging.tlb.write[lin_page]=host_mem-lin_base;
	else paging.tlb.write[lin_page]=0;

	handler->AddPageLink(lin_page,phys_page);
	paging.tlb.handler[lin_page]=handler;
}

void PAGING_MapPage(Bitu lin_page,Bitu phys_page) {
	if (lin_page<LINK_START) {
		mapfirstmb[lin_page]=phys_page;
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
	LOG(LOG_PAGING,LOG_NORMAL)("CR3:%X Base %X",cr3,paging.base.page);
	if (paging.enabled) {
		PAGING_ClearTLB();
	}
}

void PAGING_Enable(bool enabled) {
	/* If paging is disable we work from a default paging table */
	if (paging.enabled==enabled) return;
	paging.enabled=enabled;
	if (!enabled) {
		LOG(LOG_PAGING,LOG_NORMAL)("Disabled");
	} else {
		LOG(LOG_PAGING,LOG_NORMAL)("Enabled");
#if !(C_DEBUG)
		E_Exit("CPU Paging features aren't supported");
#endif
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
		mapfirstmb[i]=i;
	}
}

