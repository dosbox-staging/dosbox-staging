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
#define MAX_PAGE_ENTRIES (MAX_MEMORY*1024*1024/4096)
#define LFB_PAGES	512
#define MAX_LINKS	((MAX_MEMORY*1024/4)+4096)		//Hopefully enough

struct AllocBlock {
	Bit8u				data[PAGES_IN_BLOCK*4096];
	AllocBlock			* next;
};

struct LinkBlock {
	Bit8u used;
	Bit32u pages[MAX_LINKS];
};

static struct MemoryBlock {
	Bitu pages;
	PageHandler * * phandlers;
	HostPt * hostpts;
	MemHandle * mhandles;
	LinkBlock links;
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
	} lfb;
	struct {
		bool enabled;
		Bit8u controlport;
	} a20;
} memory;

class IllegalPageHandler : public PageHandler {
public:
	void AddPageLink(Bitu lin_page, Bitu phys_page) {
	}

	IllegalPageHandler() {
		flags=PFLAG_ILLEGAL|PFLAG_NOCODE;
	}
	Bitu readb(PhysPt addr) {
		LOG_MSG("Illegal read from %x",addr);
		return 0;
	} 
	void writeb(PhysPt addr,Bitu val) {
		LOG_MSG("Illegal write to %x",addr);
	}
	HostPt GetHostPt(Bitu phys_page) {
		return 0;
	}
};

class RAMPageHandler : public PageHandler {
public:
	void AddPageLink(Bitu lin_page, Bitu phys_page) {
		/* Always clear links in first MB on TLB change */
		if (lin_page<LINK_START) return;
		if (memory.links.used<MAX_LINKS) {
			memory.links.pages[memory.links.used++]=lin_page;
		} else E_Exit("MEM:Ran out of page links");
	}
	RAMPageHandler() {
		flags=PFLAG_READABLE|PFLAG_WRITEABLE;
	}
	HostPt GetHostPt(Bitu phys_page) {
		if (!memory.hostpts[phys_page]) {
			memory.hostpts[phys_page]=MEM_GetBlockPage();
		}
		return memory.hostpts[phys_page];
	}
};

class ROMPageHandler : public RAMPageHandler {
public:
	ROMPageHandler() {
		flags=PFLAG_READABLE|PFLAG_HASROM;
	}
};

class LFBPageHandler : public RAMPageHandler {
public:
	LFBPageHandler() {
		flags=PFLAG_READABLE|PFLAG_WRITEABLE|PFLAG_NOCODE;
	}
	HostPt GetHostPt(Bitu phys_page) {
		return memory.lfb.address+(phys_page-memory.lfb.start_page)*4096;
	}
};


static IllegalPageHandler illegal_page_handler;
static RAMPageHandler ram_page_handler;
static ROMPageHandler rom_page_handler;
static LFBPageHandler lfb_page_handler;

void MEM_SetLFB(Bitu page,Bitu pages,HostPt pt) {
	memory.lfb.address=pt;
	memory.lfb.start_page=page;
	memory.lfb.end_page=page+pages;
	memory.lfb.pages=pages;
	PAGING_ClearTLB();
}

PageHandler * MEM_GetPageHandler(Bitu phys_page) {
	if (phys_page<memory.pages) {
		return memory.phandlers[phys_page];
	} else if ((phys_page>=memory.lfb.start_page) && (phys_page<memory.lfb.end_page)) {
		return &lfb_page_handler;
	}
	return &illegal_page_handler;
}

void MEM_SetPageHandler(Bitu phys_page,Bitu pages,PageHandler * handler) {
	for (;pages>0;pages--) {
		memory.phandlers[phys_page]=handler;
		phys_page++;
	}
}

void MEM_UnlinkPages(void) {
	PAGING_ClearTLBEntries(memory.links.used,memory.links.pages);
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
		*write++=mem_readb_inline(pt++);
	}
}

void MEM_BlockWrite(PhysPt pt,void * data,Bitu size) {
	Bit8u * read=(Bit8u *) data;
	while (size--) {
		mem_writeb_inline(pt++,*read++);
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

Bitu MEM_FreeLargest(void) {
	Bitu size=0;Bitu largest=0;
	Bitu index=XMS_START;	
	while (index<memory.pages) {
		if (!memory.mhandles[index]) {
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
		if (!memory.mhandles[index]) free++;
		index++;
	}
	return free;
}

Bitu MEM_AllocatedPages(MemHandle handle) 
{
	Bitu pages = 0;
	while (handle>0) {
		pages++;
		handle=memory.mhandles[handle];
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
			if (!memory.mhandles[index]) {
				first=index;	
			}
		} else {
			/* Check if this still is used page */
			if (memory.mhandles[index]) {
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
			next=&memory.mhandles[index];
			index++;pages--;
		}
		*next=-1;
	} else {
		if (MEM_FreeTotal()<pages) return 0;
		MemHandle * next=&ret;
		while (pages) {
			Bitu index=BestMatch(1);
			if (!index) E_Exit("MEM:corruption during allocate");
			while (pages && (!memory.mhandles[index])) {
				*next=index;
				next=&memory.mhandles[index];
				index++;pages--;
			}
			*next=-1;		//Invalidate it in case we need another match
		}
	}
	return ret;
}

void MEM_ReleasePages(MemHandle handle) {
	while (handle>0) {
		MemHandle next=memory.mhandles[handle];
		memory.mhandles[handle]=0;
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
		index=memory.mhandles[index];
	}
	if (old_pages == pages) return true;
	if (old_pages > pages) {
		/* Decrease size */
		pages--;index=handle;old_pages--;
		while (pages) {
			index=memory.mhandles[index];
			pages--;old_pages--;
		}
		MemHandle next=memory.mhandles[index];
		memory.mhandles[index]=-1;
		index=next;
		while (old_pages) {
			next=memory.mhandles[index];
			memory.mhandles[index]=0;
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
			while ((index<(MemHandle)memory.pages) && !memory.mhandles[index]) {
				index++;free++;
			}
			if (free>=need) {
				/* Enough space allocate more pages */
				index=last;
				while (need) {
					memory.mhandles[index]=index+1;
					need--;index++;
				}
				memory.mhandles[index]=-1;
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
			memory.mhandles[last]=rem;
			return true;
		}
	}
	return 0;
}

MemHandle MEM_NextHandle(MemHandle handle) {
	return memory.mhandles[handle];
}

MemHandle MEM_NextHandleAt(MemHandle handle,Bitu where) {
	while (where) {
		where--;	
		handle=memory.mhandles[handle];
	}
	return handle;
}


/* 
	A20 line handling, 
	Basically maps the 4 pages at the 1mb to 0mb in the default page directory
*/
bool MEM_A20_Enabled(void) {
	return memory.a20.enabled;
}

void MEM_A20_Enable(bool enabled) {
	Bitu phys_base=enabled ? (1024/4) : 0;
	for (Bitu i=0;i<16;i++) PAGING_MapPage((1024/4)+i,phys_base+i);
	memory.a20.enabled=enabled;
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
	HostPt block=memory.hostpts[addr >> 12];
	if (!block) {
		block=memory.hostpts[addr >> 12]=MEM_GetBlockPage();
	}
	host_writeb(block+(addr & 4095),val);
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

Bit32u MEM_PhysReadD(Bitu addr) {
	Bitu page=addr >> 12;
	Bitu index=(addr & 4095);
	HostPt block=memory.hostpts[page];
	if (!block) {
		E_Exit("Reading from empty page");
	}
	return host_readd(block+index);
}


static void write_p92(Bit32u port,Bit8u val) {	
	// Bit 0 = system reset (switch back to real mode)
	if (val&1) E_Exit("XMS: CPU reset via port 0x92 not supported.");
	memory.a20.controlport = val & ~2;
	MEM_A20_Enable((val & 2)>0);
}

static Bit8u read_p92(Bit32u port) {
	return memory.a20.controlport | (memory.a20.enabled ? 0x02 : 0);
}


HostPt MEM_GetBlockPage(void) {
	HostPt ret;
	if (memory.block.pages) {
		ret=memory.block.cur_page;
		memory.block.pages--;
		memory.block.cur_page+=4096;
	} else {
		AllocBlock * newblock=new AllocBlock;
		memset(newblock,0,sizeof(AllocBlock));	//zero new allocated memory
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
	/* Allocate the data for the different page information blocks */
	memory.hostpts=new HostPt[memory.pages];
	memory.phandlers=new  PageHandler * [memory.pages];
	memory.mhandles=new MemHandle [memory.pages];
	for (i=0;i<memory.pages;i++) {
		memory.hostpts[i]=0;				//0 handler is allocate memory
		memory.phandlers[i]=&ram_page_handler;
		memory.mhandles[i]=0;				//Set to 0 for memory allocation
	}
	/* Reset some links */
	memory.links.used=0;
	// A20 Line - PS/2 system control port A
	IO_RegisterWriteHandler(0x92,write_p92,"Control Port");
	IO_RegisterReadHandler(0x92,read_p92,"Control Port");
	MEM_A20_Enable(false);
	/* shutdown function */
	sec->AddDestroyFunction(&MEM_ShutDown);
}


