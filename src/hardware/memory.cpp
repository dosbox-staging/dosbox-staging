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
#include <string.h>
#include <stdio.h>

#include "dosbox.h"
#include "mem.h"
#include "inout.h"
#include "setup.h"
#include "paging.h"
#include "vga.h"

#define PAGES_IN_BLOCK	((1024*1024)/MEM_PAGE_SIZE)
#define MAX_MEMORY	64
#define LFB_PAGES	512

static Bit8u controlport_data;
static bool	a20_enabled;

struct AllocBlock {
	Bit8u				data[PAGES_IN_BLOCK*4096];
	AllocBlock			* next;
};

static struct MemoryBlock {
	Bitu		pages;
	Bitu		free_pages;
	PageEntry *	entries;
	struct {
		Bitu			pages;
		HostPt			cur_page;
		AllocBlock		*cur_block;
	} block;
	struct	{
		Bitu		start_page;
		Bitu		end_page;
		Bitu		pages;
		HostPt		address;
		PageEntry	entries[LFB_PAGES];
	} lfb;
	struct {
		HostPt		ram_bases[128/4];
		HostPt		map_base;
		VGA_RANGES	range;
	} vga;
	PageDirectory	dir;
} memory;

Bit8u ENTRY_readb(PageEntry * pentry,PhysPt address) {
	switch(pentry->type) {
	case ENTRY_VGA:
		return (*vga.config.readhandler)(pentry->data.vga_base+(address & 4095));
	case ENTRY_NA:
		if (pentry->data.dir->InitPageLinear(address)) return mem_readb(address);
		break;
	case ENTRY_INIT:
		if (pentry->data.dir->InitPage(address)) return mem_readb(address);
		break;

	default:
		LOG(LOG_PAGING,LOG_ERROR)("Entry read from %X with illegal type %d",address,pentry->type);
	}
	return 0;
}

Bit16u ENTRY_readw(PageEntry * pentry,PhysPt address) {
	switch(pentry->type) {
	case ENTRY_VGA:
		{
			VGA_ReadHandler * handler=vga.config.readhandler;
			address=pentry->data.vga_base + (address & 4095);
			return (*handler)(address) | 
				   ((*handler)(address+1) << 8);
		}
	case ENTRY_NA:
		if (pentry->data.dir->InitPageLinear(address)) return mem_readw(address);
		break;
	case ENTRY_INIT:
		if (pentry->data.dir->InitPage(address)) return mem_readw(address);
		break;
	default:
		LOG(LOG_PAGING,LOG_ERROR)("Entry read from %X with illegal type %d",address,pentry->type);
	}
	return 0;
}

Bit32u ENTRY_readd(PageEntry * pentry,PhysPt address) {
	switch(pentry->type) {
	case ENTRY_VGA:
		{
			VGA_ReadHandler * handler=vga.config.readhandler;
			address=pentry->data.vga_base + (address & 4095);
			return (*handler)(address) |
				   ((*handler)(address+1) << 8)  |
				   ((*handler)(address+2) << 16) |
				   ((*handler)(address+3) << 24);
		}
	case ENTRY_NA:
		if (pentry->data.dir->InitPageLinear(address)) return mem_readd(address);
		break;
	case ENTRY_INIT:
		if (pentry->data.dir->InitPage(address)) return mem_readd(address);
		break;
	default:
		LOG(LOG_PAGING,LOG_ERROR)("Entry read from %X with illegal type %d",address,pentry->type);
	}
	return 0;
}

void ENTRY_writeb(PageEntry * pentry,PhysPt address,Bit8u  val) {
	switch(pentry->type) {
	case ENTRY_VGA:
		(*vga.config.writehandler)(pentry->data.vga_base+(address&4095),val);
		break;
	case ENTRY_NA:
		if (pentry->data.dir->InitPageLinear(address)) mem_writeb(address,val);
		break;
	case ENTRY_INIT:
		if (pentry->data.dir->InitPage(address)) mem_writeb(address,val);
		break;
	case ENTRY_ROM:
		LOG(LOG_PAGING,LOG_WARN)("Write %X to ROM at %X",val,address);
		break;
	case ENTRY_CHANGES:
		writeb(pentry->data.mem+(address&4095),val);
		{	
			Bitu start=address&4095;Bitu end=start;
			for (PageLink * link=pentry->links;link;link=link->next) 
				if (link->change) link->change->Changed(link,start,end);
		}
		break;
	default:
		LOG(LOG_PAGING,LOG_ERROR)("Entry write %X to %X with illegal type %d",val,address,pentry->type);
	}
}

void ENTRY_writew(PageEntry * pentry,PhysPt address,Bit16u val) {
	switch(pentry->type) {
	case ENTRY_VGA:
		{
			VGA_WriteHandler * handler=vga.config.writehandler;
			address=pentry->data.vga_base+(address&4095);
			(*handler)(address,(Bit8u)val);
			(*handler)(address+1,(Bit8u)(val>>8));
		}
		break;
	case ENTRY_NA:
		if (pentry->data.dir->InitPageLinear(address)) mem_writew(address,val);
		break;
	case ENTRY_INIT:
		if (pentry->data.dir->InitPage(address)) mem_writew(address,val);
		break;
	case ENTRY_ROM:
		LOG(LOG_PAGING,LOG_WARN)("Write %X to ROM at %X",val,address);
		break;
	case ENTRY_CHANGES:
		writew(pentry->data.mem+(address&4095),val);
		{	
			Bitu start=address&4095;Bitu end=start+1;
			for (PageLink * link=pentry->links;link;link=link->next) 
				if (link->change) link->change->Changed(link,start,end);
		}
		break;
	default:
		LOG(LOG_PAGING,LOG_ERROR)("Entry write %X to %X with illegal type %d",val,address,pentry->type);
	}
}


void ENTRY_writed(PageEntry * pentry,PhysPt address,Bit32u val) {
	switch(pentry->type) {
	case ENTRY_VGA:
		{
			VGA_WriteHandler * handler=vga.config.writehandler;
			address=pentry->data.vga_base+(address&4095);
			(*handler)(address,(Bit8u)val);
			(*handler)(address+1,(Bit8u)(val>>8));
			(*handler)(address+2,(Bit8u)(val>>16));
			(*handler)(address+3,(Bit8u)(val>>24));
		}
		break;
	case ENTRY_NA:
		if (pentry->data.dir->InitPageLinear(address)) mem_writed(address,val);
		break;
	case ENTRY_INIT:
		if (pentry->data.dir->InitPage(address)) mem_writed(address,val);
		break;
	case ENTRY_ROM:
		LOG(LOG_PAGING,LOG_WARN)("Write %X to ROM at %X",val,address);
		break;
	case ENTRY_CHANGES:
		writed(pentry->data.mem+(address&4095),val);
		{	
			Bitu start=address&4095;Bitu end=start+3;
			for (PageLink * link=pentry->links;link;link=link->next) 
				if (link->change) link->change->Changed(link,start,end);
		}
		break;
	default:
		LOG(LOG_PAGING,LOG_ERROR)("Entry write %X to %X with illegal type %d",val,address,pentry->type);
	}
}


Bitu mem_strlen(PhysPt pt) {
	Bitu x=0;
	while (x<1024) {
		if (!mem_readb_inline(pt+x)) return x;
		x++;
	}
	return 0;		//Hope this doesn't happend
}

void mem_strcpy(PhysPt dest,PhysPt src) {
	Bit8u r;
	while (r=mem_readb(src++)) mem_writeb_inline(dest++,r);
	mem_writeb_inline(dest,0);
}

void mem_memcpy(PhysPt dest,PhysPt src,Bitu size) {
	while (size--) mem_writeb_inline(dest++,mem_readb_inline(src++));
}

void MEM_BlockRead(PhysPt pt,void * data,Bitu size) {
	Bit8u * write=(Bit8u *) data;
	while (size--) {
//		*write++=mem_readb_inline(pt++);
		*write = mem_readb(pt);
		write++;
		pt++;
	}
}

void MEM_BlockWrite(PhysPt pt,void * data,Bitu size) {
	Bit8u * read=(Bit8u *) data;
	while (size--) {
//		mem_writeb_inline(pt++,*read++);
		mem_writeb(pt,*read);
		pt++;
		read++;
	}
}

void MEM_BlockCopy(PhysPt dest,PhysPt src,Bitu size) {
	mem_memcpy(dest,src,size);
}

void MEM_StrCopy(PhysPt pt,char * data,Bitu size) {
	while (size--) {
		Bit8u r=mem_readb_inline(pt++);
		if (!r) break;
		*data++=r;
	}
	*data=0;
}

Bitu MEM_TotalPages(void) {
	return memory.pages;
}

void MEM_UnlinkPage(PageLink * plink) {
	PageLink * checker=plink->entry->links;
	PageLink * * last=&plink->entry->links;
	while (checker) {
		if (checker == plink) {
			*last=plink->next;
			PAGING_AddFreePageLink(plink);
			return;
		}
		last=&checker->next;
		checker=checker->next;
	}
	E_Exit("Unlinking unlinked link");
	
}

PageLink * MEM_LinkPage(Bitu phys_page,PhysPt lin_base) {
	PageEntry * entry;
	/* Check if it's in a valid memory range */
	if (phys_page<memory.pages) {
		entry=&memory.entries[phys_page];
	/* Check if it's in the lfb range */
	} else if (phys_page>=memory.lfb.start_page && phys_page<memory.lfb.end_page) {
		entry=&memory.lfb.entries[phys_page-memory.lfb.start_page];
	} else {
		/* Invalid memory */
		LOG(LOG_PAGING,LOG_NORMAL)("Trying to link invalid page %X",phys_page);
		return 0;
	}
	PageLink * link=PAGING_GetFreePageLink();
	link->lin_base=lin_base;
	link->change=0;
	/* Check what kind of handler we need to give the page */
	switch (entry->type) {
	case ENTRY_RAM:
		link->read=entry->data.mem - lin_base;
		link->write=link->read;
		break;
	case ENTRY_ROM:
		link->read=entry->data.mem - lin_base;
		link->write=0;
		break;
	case ENTRY_VGA:
		link->read=0;
		link->write=0;
		break;
	case ENTRY_LFB:
		link->read=entry->data.mem - lin_base;
		link->write=link->read;
		break;
	case ENTRY_ALLOC:
		entry->type=ENTRY_RAM;
		entry->data.mem=MEM_GetBlockPage();		
		link->read=entry->data.mem - lin_base;
		link->write=link->read;
		break;
	case ENTRY_CHANGES:
		link->read=entry->data.mem - lin_base;
		link->write=0;
		break;
	default:
		E_Exit("LinkPage:Illegal type %d",entry->type);
	}
	/* Place the entry in the link */
	link->entry=entry;
	link->next=entry->links;
	entry->links=link;
	return link;
}


void MEM_CheckLinks(PageEntry * theentry) {
	if (theentry->type!=ENTRY_RAM && theentry->type!=ENTRY_CHANGES) {
		LOG(LOG_PAGING,LOG_NORMAL)("Checking links on type %d",theentry->type);
		return;
	}
	bool haschange=false;PageLink * link;
	for (link=theentry->links;link;link=link->next) {
		if (link->change) {
			haschange=true;break;
		}
	}
	if (haschange) {
		theentry->type=ENTRY_CHANGES;
		for (link=theentry->links;link;link=link->next) {
			link->read=theentry->data.mem - link->lin_base;
			link->write=0;
		}
	} else {
		theentry->type=ENTRY_RAM;
		for (link=theentry->links;link;link=link->next) {
			link->read=theentry->data.mem - link->lin_base;
			link->write=link->read;
		}
	}
}

void MEM_AllocLinkMemory(PageEntry * theentry) {
	//TODO Maybe check if this is a LINK_ALLOC type
	HostPt themem=MEM_GetBlockPage();
	theentry->data.mem=themem;
	theentry->type=ENTRY_RAM;
	theentry->links=0;
}

void MEM_SetLFB(Bitu page,Bitu pages,HostPt pt) {
	if (pages>LFB_PAGES) E_Exit("MEM:LFB to large");
	LOG_MSG("LFB Base at address %X,page %X",page*4096,page);
	memory.lfb.pages=pages;
	memory.lfb.start_page=page;
	memory.lfb.end_page=page+pages;
	memory.lfb.address=pt;
	for (Bitu i=0;i<pages;i++) {
		memory.lfb.entries[i].data.mem=pt+(i*4096);
		memory.lfb.entries[i].type=ENTRY_LFB;
	}
	//TODO Maybe free the linked pages, but doubht that will be necessary
}

void MEM_SetupVGA(VGA_RANGES range,HostPt base) {
	Bitu i;
	memory.vga.map_base=base;
	/* If it's another range, first clear the old one */
	if (memory.vga.range!=range) {
		Bitu start,end;
		switch (memory.vga.range) {
		case VGA_RANGE_A000:start=0;end=16;break;
		case VGA_RANGE_B000:start=16;end=24;break;
		case VGA_RANGE_B800:start=24;end=32;break;
		}
		for (i=start;i<end;i++) {
			PageEntry * theentry=&memory.entries[i+0xa0];
			HostPt themem=memory.vga.ram_bases[i];
			theentry->data.mem=themem;
			theentry->type=ENTRY_LFB;
			PageLink * link=theentry->links;
			while (link) {
				link->read=themem - link->lin_base;
				link->write=link->read;
				link=link->next;
			}
		}
		memory.vga.range=range;
	}
	/* Setup the new range, check if it's gonna handler based */
	Bitu start,end;
	switch (range) {
	case VGA_RANGE_A000:start=0;end=16;break;
	case VGA_RANGE_B000:start=16;end=24;break;
	case VGA_RANGE_B800:start=24;end=32;break;
	}
	if (base) {
		/* If it has an address it's  a mapping */
		for (i=start;i<end;i++) {
			PageEntry * theentry=&memory.entries[i+0xa0];
			HostPt themem=base+(i-start)*4096;
			theentry->type=ENTRY_LFB;
			theentry->data.mem=themem;
			PageLink * link=theentry->links;
			while (link) {
				link->read=themem - link->lin_base;
				link->write=link->read;
				link=link->next;
			}
		}
	} else {
	/* No address, so it'll be a handler */
		for (i=start;i<end;i++) {
			PageEntry * theentry=&memory.entries[i+0xa0];
			theentry->type=ENTRY_VGA;
			PhysPt thebase=(i-start)*4096;
			PageLink * link=theentry->links;
			theentry->data.vga_base=thebase;
			while (link) {
				link->read=0;
				link->write=0;
				link=link->next;
			}
		}
	}
}

Bitu MEM_FreeLargest(void) {
	Bitu size=0;Bitu largest=0;
	Bitu index=XMS_START;	
	while (index<memory.pages) {
		if (!memory.entries[index].next_handle) {
			size++;
		} else {
			if (size>largest) largest=size;
			size=0;
		}
		index++;
	}
	if (size>largest) largest=size;
	return largest;
}

Bitu MEM_FreeTotal(void) {
	Bitu free=0;
	Bitu index=XMS_START;	
	while (index<memory.pages) {
		if (!memory.entries[index].next_handle) free++;
		index++;
	}
	return free;
}

Bitu MEM_AllocatedPages(MemHandle handle) 
{
	Bitu pages = 0;
	while (handle>0) {
		pages++;
		handle=memory.entries[handle].next_handle;
	}
	return pages;
}

//TODO Maybe some protection for this whole allocation scheme

INLINE Bitu BestMatch(Bitu size) {
	Bitu index=XMS_START;	
	Bitu first=0;
	Bitu best=0xfffffff;
	Bitu best_first=0;
	while (index<memory.pages) {
		/* Check if we are searching for first free page */
		if (!first) {
			/* Check if this is a free page */
			if (!memory.entries[index].next_handle) {
				first=index;	
			}
		} else {
			/* Check if this still is used page */
			if (memory.entries[index].next_handle) {
				Bitu pages=index-first;
				if (pages==size) {
					return first;
				} else if (pages>size) {
					if (pages<best) {
						best=pages;
						best_first=first;
					}
				}
				first=0;			//Always reset for new search
			}
		}
		index++;
	}
	/* Check for the final block if we can */
	if (first && (index-first>=size) && (index-first<best)) {
		return first;
	}
	return best_first;
}

MemHandle MEM_AllocatePages(Bitu pages,bool sequence) {
	MemHandle ret;
	if (!pages) return 0;
	if (sequence) {
		Bitu index=BestMatch(pages);
		if (!index) return 0;
		MemHandle * next=&ret;
		while (pages) {
			*next=index;
			next=&memory.entries[index].next_handle;
			index++;pages--;
		}
		*next=-1;
	} else {
		if (MEM_FreeTotal()<pages) return 0;
		MemHandle * next=&ret;
		while (pages) {
			Bitu index=BestMatch(1);
			if (!index) E_Exit("MEM:corruption during allocate");
			while (pages && (!memory.entries[index].next_handle)) {
				*next=index;
				next=&memory.entries[index].next_handle;
				index++;pages--;
			}
			*next=-1;		//Invalidate it in case we need another match
		}
	}
	return ret;
}

void MEM_ReleasePages(MemHandle handle) {
	while (handle>0) {
		MemHandle next=memory.entries[handle].next_handle;
		memory.entries[handle].next_handle=0;
		handle=next;
	}
}

bool MEM_ReAllocatePages(MemHandle & handle,Bitu pages,bool sequence) {
	if (handle<0) {
		if (!pages) return true;
		handle=MEM_AllocatePages(pages,sequence);
		return (handle>0);
	}
	if (!pages) {
		MEM_ReleasePages(handle);
		handle=-1;
		return true;
	}
	MemHandle index=handle;
	MemHandle last;Bitu old_pages=0;
	while (index>0) {
		old_pages++;
		last=index;
		index=memory.entries[index].next_handle;
	}
	if (old_pages == pages) return true;
	if (old_pages > pages) {
		/* Decrease size */
		pages--;index=handle;old_pages--;
		while (pages) {
			index=memory.entries[index].next_handle;
			pages--;old_pages--;
		}
		MemHandle next=memory.entries[index].next_handle;
		memory.entries[index].next_handle=-1;
		index=next;
		while (old_pages) {
			next=memory.entries[index].next_handle;
			memory.entries[index].next_handle=0;
			index=next;
			old_pages--;
		}
		return true;
	} else {
		/* Increase size, check for enough free space */
		Bitu need=pages-old_pages;
		if (sequence) {
			index=last+1;
			Bitu free=0;
			while ((index<memory.pages) && !memory.entries[index].next_handle) {
				index++;free++;
			}
			if (free>=need) {
				/* Enough space allocate more pages */
				index=last;
				while (need) {
					memory.entries[index].next_handle=index+1;
					need--;index++;
				}
				memory.entries[index].next_handle=-1;
				return true;
			} else {
				/* Not Enough space allocate new block and copy */
				MemHandle newhandle=MEM_AllocatePages(pages,true);
				if (!newhandle) return false;
				MEM_BlockCopy(newhandle*4096,handle*4096,old_pages*4096);
				MEM_ReleasePages(handle);
				handle=newhandle;
				return true;
			}
		} else {
			MemHandle rem=MEM_AllocatePages(need,false);
			if (!rem) return false;
			memory.entries[last].next_handle=rem;
			return true;
		}
	}
	return 0;
}


void MEM_UnmapPages(Bitu phys_page,Bitu pages) {
	for (;pages;pages--) {
		memory.dir.LinkPage(phys_page,phys_page);
		phys_page++;
	}
}

void MEM_MapPagesHandle(Bitu lin_page,MemHandle mem,Bitu mem_page,Bitu pages) {
	for (;mem_page;mem_page--) {
		if (mem<=0) E_Exit("MEM:MapPages:Fault in memory tables");
		mem=memory.entries[mem].next_handle;
	}
	for (;pages;pages--) {
		if (mem<=0) E_Exit("MEM:MapPages:Fault in memory tables");
		memory.dir.LinkPage(lin_page++,mem);
		mem=memory.entries[mem].next_handle;
	}
}

void MEM_MapPagesDirect(Bitu lin_page,Bitu phys_page,Bitu pages) {
	for (;pages;pages--) {
		memory.dir.LinkPage(lin_page++,phys_page++);
	}
}

MemHandle MEM_NextHandle(MemHandle handle) {
	return memory.entries[handle].next_handle;
}

/* 
	A20 line handling, 
	Basically maps the 4 pages at the 1mb to 0mb in the default page directory
*/
bool MEM_A20_Enabled(void) {
	return a20_enabled;
}

void MEM_A20_Enable(bool enabled) {
	a20_enabled=enabled;
	Bitu i;
	if (!enabled) {
		for (i=0x0;i<0x10;i++) {
			memory.dir.LinkPage(0x100+i,i);
		}
	} else {
		for (i=0x0;i<0x10;i++) {
			memory.dir.LinkPage(0x100+i,0x100+i);
		}
	}
}


/* Memory access functions */
Bit16u mem_unalignedreadw(PhysPt address) {
	return mem_readb_inline(address) |
		mem_readb_inline(address+1) << 8;
}

Bit32u mem_unalignedreadd(PhysPt address) {
	return mem_readb_inline(address) |
		(mem_readb_inline(address+1) << 8) |
		(mem_readb_inline(address+2) << 16) |
		(mem_readb_inline(address+3) << 24);
}


void mem_unalignedwritew(PhysPt address,Bit16u val) {
	mem_writeb_inline(address,(Bit8u)val);val>>=8;
	mem_writeb_inline(address+1,(Bit8u)val);
}

void mem_unalignedwrited(PhysPt address,Bit32u val) {
	mem_writeb_inline(address,(Bit8u)val);val>>=8;
	mem_writeb_inline(address+1,(Bit8u)val);val>>=8;
	mem_writeb_inline(address+2,(Bit8u)val);val>>=8;
	mem_writeb_inline(address+3,(Bit8u)val);
}


Bit8u mem_readb(PhysPt address) {
	return mem_readb_inline(address);
}

Bit16u mem_readw(PhysPt address) {
	return mem_readw_inline(address);
}

Bit32u mem_readd(PhysPt address) {
	return mem_readd_inline(address);
}

void mem_writeb(PhysPt address,Bit8u val) {
	mem_writeb_inline(address,val);
}

void mem_writew(PhysPt address,Bit16u val) {
	mem_writew_inline(address,val);
}

void mem_writed(PhysPt address,Bit32u val) {
	mem_writed_inline(address,val);
}

void phys_writeb(PhysPt addr,Bit8u val) {
	Bitu page=addr>>12;
	if (page>=memory.pages) E_Exit("physwrite:outside of physical range");
	PageEntry * theentry=&memory.entries[page];
	switch (theentry->type) {
	case ENTRY_ALLOC:
		MEM_AllocLinkMemory(theentry);
		break;
	case ENTRY_RAM:
	case ENTRY_ROM:
		break;
	default:
		E_Exit("physwrite:illegal type %d",theentry->type);
	}
	writeb(theentry->data.mem+(addr & 4095),val);
}

void phys_writew(PhysPt addr,Bit16u val) {
	phys_writeb(addr,(Bit8u)val);
	phys_writeb(addr+1,(Bit8u)(val >> 8));	
}

void phys_writed(PhysPt addr,Bit32u val) {
	phys_writeb(addr,(Bit8u)val);
	phys_writeb(addr+1,(Bit8u)(val >> 8));
	phys_writeb(addr+2,(Bit8u)(val >> 16));
	phys_writeb(addr+3,(Bit8u)(val >> 24));
}

Bit32u phys_page_readd(Bitu page,Bitu index) {
	if (page>=memory.pages) E_Exit("physwrite:outside of physical range");
	PageEntry * theentry=&memory.entries[page];
	switch (theentry->type) {
	case ENTRY_CHANGES:
	case ENTRY_RAM:
	case ENTRY_ROM:
		return readd(memory.entries[page].data.mem+index*4);	
		break;
	default:
		E_Exit("pageread:illegal type %d",theentry->type);
	}
	return 0;
}


static void write_p92(Bit32u port,Bit8u val) {	
	// Bit 0 = system reset (switch back to real mode)
	if (val&1) E_Exit("XMS: CPU reset via port 0x92 not supported.");
	controlport_data = val & ~2;
	MEM_A20_Enable((val & 2)>0);
}

static Bit8u read_p92(Bit32u port) {
	return controlport_data | (a20_enabled ? 0x02 : 0);
}


PageDirectory * MEM_DefaultDirectory(void) {
	return &memory.dir;
}

HostPt MEM_GetBlockPage(void) {
	HostPt ret;
	if (memory.block.pages) {
		ret=memory.block.cur_page;
		memory.block.pages--;
		memory.block.cur_page+=4096;
	} else {
		AllocBlock * newblock=new AllocBlock;
		memset(newblock,0xcd,sizeof(AllocBlock));	//zero new allocated memory
		newblock->next=memory.block.cur_block;
		memory.block.cur_block=newblock;
		
		memory.block.pages=PAGES_IN_BLOCK-1;
		ret=&newblock->data[0];
		memory.block.cur_page=&newblock->data[4096];
	}
	return ret;
}

static void MEM_ShutDown(Section * sec) {
	AllocBlock * theblock=memory.block.cur_block;
	while (theblock) {
		AllocBlock * next=theblock->next;
		delete theblock;
		theblock=next;
	}
}

void MEM_SetRemainingMem(Bitu remaining)
{

};

void MEM_Init(Section * sec) {
	Bitu i;
	Section_prop * section=static_cast<Section_prop *>(sec);

	/* Setup Memory Block */
	memory.block.pages=0;
	memory.block.cur_block=0;	

	/* Setup the Physical Page Links */
	Bitu memsize=section->Get_int("memsize");

	if (memsize<1) memsize=1;
	if (memsize>MAX_MEMORY) {
		LOG_MSG("Maximum memory size is %d MB",MAX_MEMORY);
		memsize=MAX_MEMORY;
	}
	memory.pages=(memsize*1024*1024)/4096;
	if (memory.pages>0x110) memory.free_pages=memory.pages-0x110;
	else memory.free_pages=0;

	memory.entries= new PageEntry[memory.pages];
	for (i=0;i<memory.pages;i++) {
		memory.entries[i].type=ENTRY_ALLOC;
		memory.entries[i].data.mem=0;
		memory.entries[i].links=0;
		memory.entries[i].next_handle=0;			//Set to 0 for memory allocation
	}
	/* Setup 128kb of memory for the VGA segments */
	for (i=0;i<128/4;i++) {
		memory.vga.ram_bases[i]=MEM_GetBlockPage();
		memory.entries[0xa0+i].type=ENTRY_LFB;
		memory.entries[0xa0+i].data.mem=memory.vga.ram_bases[i];
	}
	memory.vga.range=VGA_RANGE_B800;
	/* Setup the default page mapping */
	memory.dir.entry_init.type=ENTRY_NA;		//Setup to use NA by default
	memory.dir.ClearDirectory();
	/* All pages now pointing to NA handler that will check on first access */
	/* Setup the ROM Areas */
	for (i=0xc0;i<0xd0;i++) {
		MEM_AllocLinkMemory(&memory.entries[i]);
		memory.entries[i].type=ENTRY_ROM;
	}
	for (i=0xf0;i<0x100;i++) {
		MEM_AllocLinkMemory(&memory.entries[i]);
		memory.entries[i].type=ENTRY_ROM;
	}
	// A20 Line - PS/2 system control port A
	IO_RegisterWriteHandler(0x92,write_p92,"Control Port");
	IO_RegisterReadHandler(0x92,read_p92,"Control Port");
	MEM_A20_Enable(false);
	/* shutdown function */
	sec->AddDestroyFunction(&MEM_ShutDown);
}


