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
#include "../hardware/vga.h"


#define LINK_TOTAL		(64*1024)

static PageLink			link_list[LINK_TOTAL];
struct PagingBlock		paging;

class PageDirChange : public PageChange {
public:
	PageDirChange(PageDirectory * mydir) { dir=mydir;}
	void Changed(PageLink * link,Bitu start,Bitu end) {
		start>>=2;end>>=2;
		for (;start<=end;start++) {
			dir->InvalidateTable(start);
		}
	}
private:
	PageDirectory * dir;
};

class PageTableChange : public PageChange {
public:
	PageTableChange(PageDirectory * mydir) { dir=mydir;}
	void Changed(PageLink * link,Bitu start,Bitu end) {
		start>>=2;end>>=2;
		for (;start<=end;start++) {
			dir->InvalidateLink(link->data.table,start);
		}
	}
private:
	PageDirectory * dir;
};



PageDirectory::PageDirectory()	{
	entry_init.data.dir=this;
	entry_init.type=ENTRY_INIT;
	link_init.read=0;
	link_init.write=0;
	link_init.entry=&entry_init;
	table_change = new PageTableChange(this);
	dir_change = new PageDirChange(this);
}
PageDirectory::~PageDirectory() {
	delete table_change;
	delete dir_change;
}
void PageDirectory::ClearDirectory(void) {
	Bitu i;
	for (i=0;i<1024*1024;i++) links[i]=&link_init;
	for (i=0;i<1024;i++) {
		tables[i]=0;
	}
}
void PageDirectory::SetBase(PhysPt page) {
	base_page=page;
	ClearDirectory();
	/* Setup handler for PageDirectory changes */
	link_dir=MEM_LinkPage(base_page,0);
	if (!link_dir) E_Exit("PageDirectory setup on illegal address");
	link_dir->data.dir=this;
	link_dir->change=dir_change;
	MEM_CheckLinks(link_dir->entry);
}
void PageDirectory::LinkPage(Bitu lin_page,Bitu phys_page) {
	if (links[lin_page] != &link_init) MEM_UnlinkPage(links[lin_page]);
	PageLink * link=MEM_LinkPage(phys_page,lin_page*4096);
	if (link) links[lin_page]=link;
	else links[lin_page]=&link_init;
}

bool PageDirectory::InitPage(Bitu lin_address) {
	Bitu lin_page=lin_address >> 12;
	Bitu table=lin_page >> 10;
	Bitu index=lin_page & 0x3ff;
	/* Check if there already is table linked */
	if (!tables[table]) {
		X86PageEntry table_entry;
		table_entry.load=phys_page_readd(base_page,0);
		if (!table_entry.block.p) {
			LOG(LOG_PAGING,LOG_ERROR)("NP TABLE");
			return false;
		}
		PageLink * link=MEM_LinkPage(table_entry.block.base,table_entry.block.base);
		if (!link) return false;
		link->data.table=table;
		link->change=table_change;
		MEM_CheckLinks(link->entry);
		tables[table]=link;
	}
	X86PageEntry entry;
	entry.load=phys_page_readd(tables[table]->lin_base,index);
	if (!entry.block.p) {
		LOG(LOG_PAGING,LOG_ERROR)("NP PAGE");
		return false;
	}
	PageLink * link=MEM_LinkPage(entry.block.base,lin_page*4096);
	if (!link) return false;
	links[lin_page]=link;
	return true;
}


bool PageDirectory::InitPageLinear(Bitu lin_address) {
	Bitu phys_page=lin_address >> 12;
	PageLink * link=MEM_LinkPage(phys_page,phys_page*4096);
	if (link) {
		/* Set the page entry in our table */
		links[phys_page]=link;
		return true;
	}
	return false;
}


void PageDirectory::InvalidateTable(Bitu table) {
	if (tables[table]) {
		MEM_UnlinkPage(tables[table]);
		tables[table]=0;
		for (Bitu i=(table*1024);i<(table+1)*1024;i++) {
			if (links[i]!=&link_init) {
				MEM_UnlinkPage(links[i]);
				links[i]=&link_init;
			}
		}
	}
}

void PageDirectory::InvalidateLink(Bitu table,Bitu index) {
	Bitu i=(table*1024)+index;
	if (links[i]!=&link_init) {
		MEM_UnlinkPage(links[i]);
		links[i]=&link_init;
	}
}


Bitu PAGING_GetDirBase(void) {
	return paging.cr3;
}

void PAGING_SetDirBase(Bitu cr3) {
	paging.cr3=cr3;
	Bitu base_page=cr3 >> 12;
	LOG_MSG("CR3:%X Base %X",cr3,base_page);
	if (paging.enabled) {
		/* Check if we already have this one cached */
		PageDirectory * dir=paging.cache;
		while (dir) {
			if (dir->base_page==base_page) {
				paging.dir=dir;
				return;
			}
			dir=dir->next;
		}
		/* Couldn't find cached directory, make a new one */
		dir=new PageDirectory();
		dir->next=paging.cache;
		paging.cache=dir;
		dir->SetBase(base_page);
		paging.dir=dir;
	}
}

void PAGING_Enable(bool enabled) {
	/* If paging is disable we work from a default paging table */
	if (paging.enabled==enabled) return;
	paging.enabled=enabled;
	if (!enabled) {
		LOG_MSG("Paging disabled");
		paging.dir=MEM_DefaultDirectory();
	} else {
		LOG_MSG("Paging enabled");
		PAGING_SetDirBase(paging.cr3);
	}
}

bool PAGING_Enabled(void) {
	return paging.enabled;
}


void PAGING_FreePageLink(PageLink * link) {
	MEM_UnlinkPage(link);
	PAGING_AddFreePageLink(link);
}

void PAGING_LinkPage(PageDirectory * dir,Bitu lin_page,Bitu phys_page) {
	PageLink * link=MEM_LinkPage(phys_page,lin_page*4096);
	/* Only replace if we can */
	if (link) {
		PAGING_FreePageLink(dir->links[lin_page]);
		dir->links[lin_page]=link;
	}
}

void PAGING_AddFreePageLink(PageLink * link) {
	link->read=0;
	link->write=0;
	link->change=0;
	link->next=paging.free_link;
	link->entry=0;
	paging.free_link=link;
}

PageLink * PAGING_GetFreePageLink(void) {
	PageLink * ret;
	if (paging.free_link) ret=paging.free_link;
	else E_Exit("PAGING:Ran out of PageEntries");
	paging.free_link=ret->next;
	ret->next=0;
	return ret;
}

void PAGING_Init(Section * sec) {
	Bitu i;
	/* Setup the free pages tables for fast page allocation */
	paging.cache=0;
	paging.free_link=0;
	for (i=0;i<LINK_TOTAL;i++) PAGING_AddFreePageLink(&link_list[i]);
	/* Setup default Page Directory, force it to update */
	paging.enabled=true;PAGING_Enable(false);
}