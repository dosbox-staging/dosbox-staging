/*
 *  Copyright (C) 2002-2005  The DOSBox Team
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

/* $Id: paging.h,v 1.14 2005-02-10 10:20:47 qbix79 Exp $ */

#ifndef _PAGING_H_
#define _PAGING_H_

#include "dosbox.h"
#include "mem.h"

class PageDirectory;

#define MEM_PAGE_SIZE	(4096)
#define XMS_START		(0x110)
#define TLB_SIZE		(1024*1024)

#define PFLAG_READABLE		0x1
#define PFLAG_WRITEABLE		0x2
#define PFLAG_HASROM		0x4
#define PFLAG_HASCODE		0x8				//Page contains dynamic code
#define PFLAG_NOCODE		0x10			//No dynamic code can be generated here
#define PFLAG_INIT			0x20			//No dynamic code can be generated here

#define LINK_START	((1024+64)/4)			//Start right after the HMA

//Allow 128 mb of memory to be linked
#define PAGING_LINKS (128*1024/4)

class PageHandler {
public:
	virtual Bitu readb(PhysPt addr);
	virtual Bitu readw(PhysPt addr);
	virtual Bitu readd(PhysPt addr);
	virtual void writeb(PhysPt addr,Bitu val);
	virtual void writew(PhysPt addr,Bitu val);
	virtual void writed(PhysPt addr,Bitu val);
	virtual HostPt GetHostPt(Bitu phys_page);
	Bitu flags;
};

/* Some other functions */
void PAGING_Enable(bool enabled);
bool PAGING_Enabled(void);

Bitu PAGING_GetDirBase(void);
void PAGING_SetDirBase(Bitu cr3);
void PAGING_InitTLB(void);
void PAGING_ClearTLB(void);

void PAGING_LinkPage(Bitu lin_page,Bitu phys_page);
void PAGING_UnlinkPages(Bitu lin_page,Bitu pages);
/* This maps the page directly, only use when paging is disabled */
void PAGING_MapPage(Bitu lin_page,Bitu phys_page);
bool PAGING_MakePhysPage(Bitu & page);

void MEM_SetLFB(Bitu _page,Bitu _pages,HostPt _pt);
void MEM_SetPageHandler(Bitu phys_page,Bitu pages,PageHandler * handler);
void MEM_ResetPageHandler(Bitu phys_page, Bitu pages);


#ifdef _MSC_VER
#pragma pack (1)
#endif
struct X86_PageEntryBlock{
	Bit32u		p:1;
	Bit32u		wr:1;
	Bit32u		us:1;
	Bit32u		pwt:1;
	Bit32u		pcd:1;
	Bit32u		a:1;
	Bit32u		d:1;
	Bit32u		pat:1;
	Bit32u		g:1;
	Bit32u		avl:3;
	Bit32u		base:20;
} GCC_ATTRIBUTE(packed);
#ifdef _MSC_VER
#pragma pack ()
#endif


union X86PageEntry {
	Bit32u load;
	X86_PageEntryBlock block;
};

struct PagingBlock {
	Bitu			cr3;
	Bitu			cr2;
	struct {
		Bitu page;
		PhysPt addr;
	} base;
	struct {
		HostPt read[TLB_SIZE];
		HostPt write[TLB_SIZE];
		PageHandler * handler[TLB_SIZE];
		Bit32u	phys_page[TLB_SIZE];
	} tlb;
	struct {
		Bitu used;
		Bit32u entries[PAGING_LINKS];
	} links;
	Bit32u		firstmb[LINK_START];
	bool		enabled;
};

extern PagingBlock paging; 

/* Some support functions */

PageHandler * MEM_GetPageHandler(Bitu phys_page);

/* Unaligned address handlers */
Bit16u mem_unalignedreadw(PhysPt address);
Bit32u mem_unalignedreadd(PhysPt address);
void mem_unalignedwritew(PhysPt address,Bit16u val);
void mem_unalignedwrited(PhysPt address,Bit32u val);

/* Special inlined memory reading/writing */

INLINE Bit8u mem_readb_inline(PhysPt address) {
	Bitu index=(address>>12);
	if (paging.tlb.read[index]) return host_readb(paging.tlb.read[index]+address);
	else return paging.tlb.handler[index]->readb(address);
}

INLINE Bit16u mem_readw_inline(PhysPt address) {
	if (address & 1) return mem_unalignedreadw(address);

	Bitu index=(address>>12);
	if (paging.tlb.read[index]) return host_readw(paging.tlb.read[index]+address);
	else return paging.tlb.handler[index]->readw(address);
}


INLINE Bit32u mem_readd_inline(PhysPt address) {
	if (address & 3) return mem_unalignedreadd(address);

	Bitu index=(address>>12);
	if (paging.tlb.read[index]) return host_readd(paging.tlb.read[index]+address);
	else return paging.tlb.handler[index]->readd(address);
}

INLINE void mem_writeb_inline(PhysPt address,Bit8u val) {
	Bitu index=(address>>12);

	if (paging.tlb.write[index]) host_writeb(paging.tlb.write[index]+address,val);
	else paging.tlb.handler[index]->writeb(address,val);
}

INLINE void mem_writew_inline(PhysPt address,Bit16u val) {
	if (address & 1) {mem_unalignedwritew(address,val);return;}

	Bitu index=(address>>12);

	if (paging.tlb.write[index]) host_writew(paging.tlb.write[index]+address,val);
	else paging.tlb.handler[index]->writew(address,val);
}

INLINE void mem_writed_inline(PhysPt address,Bit32u val) {
	if (address & 3) {mem_unalignedwrited(address,val);return;}

	Bitu index=(address>>12);
	if (paging.tlb.write[index]) host_writed(paging.tlb.write[index]+address,val);
	else paging.tlb.handler[index]->writed(address,val);

}

#endif
