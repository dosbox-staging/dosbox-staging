/*
 *  Copyright (C) 2002  The DOSBox Team
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

#ifndef _PAGING_H_
#define _PAGING_H_

#include "mem.h"

class PageDirectory;
struct PageEntry;
struct PageLink;

#define MEM_PAGE_SIZE	(4096)
#define XMS_START		(0x110)

enum EntryTypes {		//The type of memory contained in this link
	ENTRY_VGA,
	ENTRY_CHANGES,
	ENTRY_INIT,
	ENTRY_NA,
	ENTRY_ROM,
	ENTRY_LFB,
	ENTRY_RAM,
	ENTRY_ALLOC,
};

enum VGA_RANGES {
	VGA_RANGE_A000,
	VGA_RANGE_B000,
	VGA_RANGE_B800,
};


class PageChange {
public:
	virtual void Changed(PageLink * link,Bitu start,Bitu end)=0;
};

/* Some other functions */
void PAGING_Enable(bool enabled);
bool PAGING_Enabled(void);

void MEM_CheckLinks(PageEntry * theentry);

PageDirectory * MEM_DefaultDirectory(void);
Bitu PAGING_GetDirBase(void);
void PAGING_SetDirBase(Bitu cr3);

PageLink * MEM_LinkPage(Bitu phys_page,PhysPt lin_base);

void MEM_UnlinkPage(PageLink * link);
void MEM_SetLFB(Bitu _page,Bitu _pages,HostPt _pt);

#pragma pack(1)
typedef struct {
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
} X86_PageEntryBlock GCC_ATTRIBUTE(packed);
#pragma pack()

union X86PageEntry {
	Bit32u load;
	X86_PageEntryBlock block;
};

struct PageLink {
	HostPt read;
	HostPt write;
	PageChange * change;
	PhysPt lin_base;
	PageEntry * entry;
	union {
		PageDirectory * dir;
		Bitu table;
	} data;
	PageLink * next;
};

struct PageEntry {
	PageLink * links;
	EntryTypes type;
	union {
		HostPt mem;
		PhysPt vga_base;
		PageDirectory * dir;
	} data;
	MemHandle next_handle;
};

class PageDirectory {
public:
	PageDirectory();
	~PageDirectory();
	void ClearDirectory(void);
	void SetBase(PhysPt page);
	void LinkPage(Bitu lin_page,Bitu phys_page);
	bool InitPage(Bitu lin_address);
	bool InitPageLinear(Bitu lin_address);
	void InvalidateTable(Bitu table);
	void InvalidateLink(Bitu table,Bitu index);
	PageDirectory * next;
	PageLink	*links[1024*1024];
	PageLink	*tables[1024];
	PageLink	*link_dir;						//Handler for main directory table
	PageEntry	entry_init;						//Handler for pages that need init
	PageLink	link_init;						//Handler for pages that need init
	Bit32u		base_page;						//Base got from CR3
	PageChange * table_change;
	PageChange * dir_change;
};

struct PagingBlock {
	PageDirectory   * cache;
	PageDirectory	* dir;
	PageLink		* free_link;
	Bitu			cr3;
	bool			enabled;
};

extern PagingBlock paging; 

/* Some support functions */

static INLINE PageLink * GetPageLink(PhysPt address) {
	Bitu index=(address>>12);
	return paging.dir->links[index];
}

void PAGING_AddFreePageLink(PageLink * link);

PageLink * PAGING_GetFreePageLink(void);
void MEM_SetupVGA(VGA_RANGES range,HostPt base);

/* Page Handler functions */

Bit8u  ENTRY_readb(PageEntry * pentry,PhysPt address);
Bit16u ENTRY_readw(PageEntry * pentry,PhysPt address);
Bit32u ENTRY_readd(PageEntry * pentry,PhysPt address);
void ENTRY_writeb(PageEntry * pentry,PhysPt address,Bit8u  val);
void ENTRY_writew(PageEntry * pentry,PhysPt address,Bit16u val);
void ENTRY_writed(PageEntry * pentry,PhysPt address,Bit32u val);

/* Unaligned address handlers */
Bit16u mem_unalignedreadw(PhysPt address);
Bit32u mem_unalignedreadd(PhysPt address);
void mem_unalignedwritew(PhysPt address,Bit16u val);
void mem_unalignedwrited(PhysPt address,Bit32u val);

/* Special inlined memory reading/writing */

INLINE Bit8u mem_readb_inline(PhysPt address) {
	PageLink * plink=GetPageLink(address);

	if (plink->read) return readb(plink->read+address);
	else return ENTRY_readb(plink->entry,address);
}

INLINE Bit16u mem_readw_inline(PhysPt address) {
	if (address & 1) return mem_unalignedreadw(address);
	PageLink * plink=GetPageLink(address);

	if (plink->read) return readw(plink->read+address);
	else return ENTRY_readw(plink->entry,address);
}


INLINE Bit32u mem_readd_inline(PhysPt address) {
	if (address & 3) return mem_unalignedreadd(address);
	PageLink * plink=GetPageLink(address);

	if (plink->read) return readd(plink->read+address);
	else return ENTRY_readd(plink->entry,address);
}

INLINE void mem_writeb_inline(PhysPt address,Bit8u val) {
	PageLink * plink=GetPageLink(address);

	if (plink->write) writeb(plink->write+address,val);
	else ENTRY_writeb(plink->entry,address,val);
}

INLINE void mem_writew_inline(PhysPt address,Bit16u val) {
	if (address & 1) {mem_unalignedwritew(address,val);return;}

	PageLink * plink=GetPageLink(address);

	if (plink->write) writew(plink->write+address,val);
	else ENTRY_writew(plink->entry,address,val);
}

INLINE void mem_writed_inline(PhysPt address,Bit32u val) {
	if (address & 3) {mem_unalignedwrited(address,val);return;}

	PageLink * plink=GetPageLink(address);

	if (plink->write) writed(plink->write+address,val);
	else ENTRY_writed(plink->entry,address,val);
}


#endif