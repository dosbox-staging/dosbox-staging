/*
 *  Copyright (C) 2002-2021  The DOSBox Team
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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#include "mem.h"

#include <string.h>

#include "inout.h"
#include "setup.h"
#include "paging.h"
#include "regs.h"
#include "support.h"

#define PAGES_IN_BLOCK	((1024*1024)/MEM_PAGE_SIZE)
#define SAFE_MEMORY	32
#define MAX_MEMORY	64
#define MAX_PAGE_ENTRIES (MAX_MEMORY*1024*1024/4096)
#define LFB_PAGES	512
#define MAX_LINKS	((MAX_MEMORY*1024/4)+4096)		//Hopefully enough

struct LinkBlock {
	Bitu used;
	uint32_t pages[MAX_LINKS];
};

static struct MemoryBlock {
	Bitu pages;
	PageHandler * phandlers[MAX_PAGE_ENTRIES];
	MemHandle mhandles[MAX_PAGE_ENTRIES];
	LinkBlock links;
	struct	{
		Bitu		start_page;
		Bitu		end_page;
		Bitu		pages;
		PageHandler *handler;
		PageHandler *mmiohandler;
	} lfb;
	struct {
		bool enabled;
		uint8_t controlport;
	} a20;
} memory;

alignas(host_pagesize) uint8_t MemBase[MAX_MEMORY*1024*1024];

class IllegalPageHandler final : public PageHandler {
public:
	IllegalPageHandler() {
		flags=PFLAG_INIT|PFLAG_NOCODE;
	}
	uint8_t readb(PhysPt addr)
	{
#if C_DEBUG
		LOG_MSG("Illegal read from %x, CS:IP %8x:%8x",addr,SegValue(cs),reg_eip);
#else
		static Bits lcount=0;
		if (lcount<1000) {
			lcount++;
			LOG_MSG("Illegal read from %x, CS:IP %8x:%8x",addr,SegValue(cs),reg_eip);
		}
#endif
		return 0xff;
	}
	void writeb(PhysPt addr, [[maybe_unused]] uint8_t val)
	{
#if C_DEBUG
		LOG_MSG("Illegal write to %x, CS:IP %8x:%8x",addr,SegValue(cs),reg_eip);
#else
		static Bits lcount=0;
		if (lcount<1000) {
			lcount++;
			LOG_MSG("Illegal write to %x, CS:IP %8x:%8x",addr,SegValue(cs),reg_eip);
		}
#endif
	}
};

class RAMPageHandler : public PageHandler {
public:
	RAMPageHandler() {
		flags=PFLAG_READABLE|PFLAG_WRITEABLE;
	}
	HostPt GetHostReadPt(Bitu phys_page) {
		return MemBase+phys_page*dos_pagesize;
	}
	HostPt GetHostWritePt(Bitu phys_page) {
		return MemBase+phys_page*dos_pagesize;
	}
};

class ROMPageHandler final : public RAMPageHandler {
public:
	ROMPageHandler() {
		flags=PFLAG_READABLE|PFLAG_HASROM;
	}
	void writeb(PhysPt addr,uint8_t val){
		LOG(LOG_CPU, LOG_ERROR)("Write 0x%x to rom at %x", val, addr);
	}
	void writew(PhysPt addr,uint16_t val){
		LOG(LOG_CPU, LOG_ERROR)("Write 0x%x to rom at %x", val, addr);
	}
	void writed(PhysPt addr,uint32_t val){
		LOG(LOG_CPU, LOG_ERROR)("Write 0x%x to rom at %x", val, addr);
	}
};



static IllegalPageHandler illegal_page_handler;
static RAMPageHandler ram_page_handler;
static ROMPageHandler rom_page_handler;

void MEM_SetLFB(Bitu page, Bitu pages, PageHandler *handler, PageHandler *mmiohandler) {
	memory.lfb.handler=handler;
	memory.lfb.mmiohandler=mmiohandler;
	memory.lfb.start_page=page;
	memory.lfb.end_page=page+pages;
	memory.lfb.pages=pages;
	PAGING_ClearTLB();
}

PageHandler * MEM_GetPageHandler(Bitu phys_page) {
	if (phys_page<memory.pages) {
		return memory.phandlers[phys_page];
	} else if ((phys_page>=memory.lfb.start_page) && (phys_page<memory.lfb.end_page)) {
		return memory.lfb.handler;
	} else if ((phys_page>=memory.lfb.start_page+0x01000000/4096) &&
				(phys_page<memory.lfb.start_page+0x01000000/4096+16)) {
		return memory.lfb.mmiohandler;
	}
	return &illegal_page_handler;
}

void MEM_SetPageHandler(Bitu phys_page,Bitu pages,PageHandler * handler) {
	for (;pages>0;pages--) {
		memory.phandlers[phys_page]=handler;
		phys_page++;
	}
}

void MEM_ResetPageHandler(Bitu phys_page, Bitu pages) {
	for (;pages>0;pages--) {
		memory.phandlers[phys_page]=&ram_page_handler;
		phys_page++;
	}
}

Bitu mem_strlen(PhysPt pt) {
	Bitu x=0;
	while (x<1024) {
		if (!mem_readb_inline(pt+x)) return x;
		x++;
	}
	return 0;		//Hope this doesn't happen
}

void mem_strcpy(PhysPt dest,PhysPt src) {
	uint8_t r;
	while ( (r = mem_readb(src++)) ) mem_writeb_inline(dest++,r);
	mem_writeb_inline(dest,0);
}

void mem_memcpy(PhysPt dest,PhysPt src,Bitu size) {
	while (size--) mem_writeb_inline(dest++,mem_readb_inline(src++));
}

void MEM_BlockRead(PhysPt pt,void * data,Bitu size) {
	uint8_t * write=reinterpret_cast<uint8_t *>(data);
	while (size--) {
		*write++=mem_readb_inline(pt++);
	}
}

void MEM_BlockWrite(PhysPt pt, const void *data, size_t size)
{
	const uint8_t *read = static_cast<const uint8_t *>(data);
	while (size--) {
		mem_writeb_inline(pt++,*read++);
	}
}

void MEM_BlockCopy(PhysPt dest,PhysPt src,Bitu size) {
	mem_memcpy(dest,src,size);
}

void MEM_StrCopy(PhysPt pt,char * data,Bitu size) {
	while (size--) {
		uint8_t r=mem_readb_inline(pt++);
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

inline Bitu BestMatch(Bitu size) {
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

MemHandle MEM_GetNextFreePage(void) {
	return (MemHandle)BestMatch(1);
}

void MEM_ReleasePages(MemHandle handle) {
	while (handle>0) {
		MemHandle next=memory.mhandles[handle];
		memory.mhandles[handle]=0;
		handle=next;
	}
}

bool MEM_ReAllocatePages(MemHandle & handle,Bitu pages,bool sequence) {
	if (handle<=0) {
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

// Use this function for initialization since the public 
// function is optimized to return on the same state, and
// we need the page mappings initialized for disabled.
static void InitA20() {
	memory.a20.enabled = true;
	MEM_A20_Enable(false);
}

void MEM_A20_Enable(bool enabled) {
	if (enabled == memory.a20.enabled) return;
	const uint32_t phys_base=enabled ? (1024/4) : 0;
	for (int i=0;i<16;i++) PAGING_MapPage((1024/4)+i,phys_base+i);
	memory.a20.enabled=enabled;
}


/* Memory access functions */
uint16_t mem_unalignedreadw(PhysPt address) {
	uint16_t ret = mem_readb_inline(address);
	ret       |= mem_readb_inline(address+1) << 8;
	return ret;
}

uint32_t mem_unalignedreadd(PhysPt address) {
	uint32_t ret = mem_readb_inline(address);
	ret       |= mem_readb_inline(address+1) << 8;
	ret       |= mem_readb_inline(address+2) << 16;
	ret       |= mem_readb_inline(address+3) << 24;
	return ret;
}


void mem_unalignedwritew(PhysPt address,uint16_t val) {
	mem_writeb_inline(address,(uint8_t)val);val>>=8;
	mem_writeb_inline(address+1,(uint8_t)val);
}

void mem_unalignedwrited(PhysPt address,uint32_t val) {
	mem_writeb_inline(address,(uint8_t)val);val>>=8;
	mem_writeb_inline(address+1,(uint8_t)val);val>>=8;
	mem_writeb_inline(address+2,(uint8_t)val);val>>=8;
	mem_writeb_inline(address+3,(uint8_t)val);
}


bool mem_unalignedreadw_checked(PhysPt address, uint16_t * val) {
	uint8_t rval1;
	if (mem_readb_checked(address + 0, &rval1))
		return true;

	uint8_t rval2;
	if (mem_readb_checked(address + 1, &rval2))
		return true;

	*val = static_cast<uint16_t>(rval1 | (rval2 << 8));
	return false;
}

bool mem_unalignedreadd_checked(PhysPt address, uint32_t * val) {
	uint8_t rval1;
	if (mem_readb_checked(address+0, &rval1)) return true;

	uint8_t rval2;
	if (mem_readb_checked(address + 1, &rval2))
		return true;

	uint8_t rval3;
	if (mem_readb_checked(address + 2, &rval3))
		return true;

	uint8_t rval4;
	if (mem_readb_checked(address + 3, &rval4))
		return true;

	*val = static_cast<uint32_t>(rval1 | (rval2 << 8) | (rval3 << 16) | (rval4 << 24));
	return false;
}

bool mem_unalignedwritew_checked(PhysPt address, uint16_t val) {
	if (mem_writeb_checked(address+0, (uint8_t)(val & 0xff))) return true;
	val >>= 8;
	if (mem_writeb_checked(address+1, (uint8_t)(val & 0xff))) return true;
	return false;
}

bool mem_unalignedwrited_checked(PhysPt address, uint32_t val) {
	if (mem_writeb_checked(address+0, (uint8_t)(val & 0xff))) return true;
	val >>= 8;
	if (mem_writeb_checked(address+1, (uint8_t)(val & 0xff))) return true;
	val >>= 8;
	if (mem_writeb_checked(address+2, (uint8_t)(val & 0xff))) return true;
	val >>= 8;
	if (mem_writeb_checked(address+3, (uint8_t)(val & 0xff))) return true;
	return false;
}

uint8_t mem_readb(PhysPt address) {
	return mem_readb_inline(address);
}

uint16_t mem_readw(PhysPt address) {
	return mem_readw_inline(address);
}

uint32_t mem_readd(PhysPt address) {
	return mem_readd_inline(address);
}

void mem_writeb(PhysPt address,uint8_t val) {
	mem_writeb_inline(address,val);
}

void mem_writew(PhysPt address,uint16_t val) {
	mem_writew_inline(address,val);
}

void mem_writed(PhysPt address,uint32_t val) {
	mem_writed_inline(address,val);
}

static void write_p92(io_port_t, io_val_t value, io_width_t)
{
	const auto val = check_cast<uint8_t>(value);
	// Bit 0 = system reset (switch back to real mode)
	if (val&1) E_Exit("XMS: CPU reset via port 0x92 not supported.");
	memory.a20.controlport = val & ~2;
	MEM_A20_Enable((val & 2)>0);
}

static uint8_t read_p92(io_port_t, io_width_t)
{
	return memory.a20.controlport | (memory.a20.enabled ? 0x02 : 0);
}

void MEM_RemoveEMSPageFrame()
{
	/* Setup rom at 0xe0000-0xf0000 */
	for (Bitu ct=0xe0;ct<0xf0;ct++) {
		memory.phandlers[ct] = &rom_page_handler;
	}
}

void MEM_PreparePCJRCartRom()
{
	/* Setup rom at 0xd0000-0xe0000 */
	for (Bitu ct=0xd0;ct<0xe0;ct++) {
		memory.phandlers[ct] = &rom_page_handler;
	}
}

HostPt GetMemBase(void) { return MemBase; }

class MEMORY final : public Module_base {
private:
	IO_ReadHandleObject ReadHandler{};
	IO_WriteHandleObject WriteHandler{};

public:
	MEMORY(Section *configuration) : Module_base(configuration)
	{
		Bitu i;
		Section_prop * section=static_cast<Section_prop *>(configuration);
	
		/* Setup the Physical Page Links */
		auto memsize = static_cast<uint16_t>(section->Get_int("memsize"));

		if (memsize < 1) memsize = 1;
		/* max 63 to solve problems with certain xms handlers */
		if (memsize > MAX_MEMORY - 1) {
			LOG_MSG("Maximum memory size is %d MB",MAX_MEMORY - 1);
			memsize = MAX_MEMORY - 1;
		}
		if (memsize > SAFE_MEMORY - 1) {
			LOG_MSG("Memory sizes above %d MB are NOT recommended.",SAFE_MEMORY - 1);
			LOG_MSG("Stick with the default values unless you are absolutely certain.");
		}
		memset((void*)MemBase, 0, memsize * 1024 * 1024);
		memory.pages = (memsize * 1024 * 1024) / 4096;
		LOG_MSG("MEMORY: Base address: %p", static_cast<void *>(MemBase));
		LOG_MSG("MEMORY: Using %d DOS memory pages (%u MiB)",
		        static_cast<int>(memory.pages), memsize);

		for (i = 0; i < memory.pages; i++) {
			memory.phandlers[i] = &ram_page_handler;
			memory.mhandles[i] = 0;				//Set to 0 for memory allocation
		}
		/* Setup rom at 0xc0000-0xc8000 */
		for (i=0xc0;i<0xc8;i++) {
			memory.phandlers[i] = &rom_page_handler;
		}
		/* Setup rom at 0xf0000-0x100000 */
		for (i=0xf0;i<0x100;i++) {
			memory.phandlers[i] = &rom_page_handler;
		}
		if (machine==MCH_PCJR) {
			/* Setup cartridge rom at 0xe0000-0xf0000 */
			for (i=0xe0;i<0xf0;i++) {
				memory.phandlers[i] = &rom_page_handler;
			}
		}
		/* Reset some links */
		memory.links.used = 0;
		// A20 Line - PS/2 system control port A
		WriteHandler.Install(0x92, write_p92, io_width_t::byte);
		ReadHandler.Install(0x92, read_p92, io_width_t::byte);
		InitA20();
	}
};

static MEMORY* test;

static void MEM_ShutDown([[maybe_unused]] Section *sec)
{
	delete test;
}

void MEM_Init(Section * sec) {
	/* shutdown function */
	test = new MEMORY(sec);
	sec->AddDestroyFunction(&MEM_ShutDown);
}
