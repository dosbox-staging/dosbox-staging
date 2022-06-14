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

#ifndef DOSBOX_PAGING_H
#define DOSBOX_PAGING_H

#include "dosbox.h"

#include "mem.h"

// disable this to reduce the size of the TLB
// NOTE: does not work with the dynamic core (dynrec is fine)
#define USE_FULL_TLB

class PageDirectory;

#define MEM_PAGE_SIZE	(4096)
#define XMS_START		(0x110)

#if defined(USE_FULL_TLB)
#define TLB_SIZE		(1024*1024)
#else
#define TLB_SIZE		65536	// This must a power of 2 and greater then LINK_START
#define BANK_SHIFT		28
#define BANK_MASK		0xffff // always the same as TLB_SIZE-1?
#define TLB_BANKS		((1024*1024/TLB_SIZE)-1)
#endif

#define PFLAG_READABLE		0x1
#define PFLAG_WRITEABLE		0x2
#define PFLAG_HASROM		0x4
#define PFLAG_HASCODE32		0x8				//Page contains 32-bit dynamic code
#define PFLAG_NOCODE		0x10			//No dynamic code can be generated here
#define PFLAG_INIT			0x20			//No dynamic code can be generated here
#define PFLAG_HASCODE16		0x40			//Page contains 16-bit dynamic code
#define PFLAG_HASCODE		(PFLAG_HASCODE32|PFLAG_HASCODE16)

#define LINK_START	((1024+64)/4)			//Start right after the HMA

//Allow 128 mb of memory to be linked
#define PAGING_LINKS (128*1024/4)

class PageHandler {
public:
	virtual ~PageHandler() = default;

	virtual uint8_t readb(PhysPt addr);
	virtual uint16_t readw(PhysPt addr);
	virtual uint32_t readd(PhysPt addr);
	virtual void writeb(PhysPt addr, uint8_t val);
	virtual void writew(PhysPt addr, uint16_t val);
	virtual void writed(PhysPt addr, uint32_t val);
	virtual HostPt GetHostReadPt(Bitu phys_page);
	virtual HostPt GetHostWritePt(Bitu phys_page);
	virtual bool readb_checked(PhysPt addr,uint8_t * val);
	virtual bool readw_checked(PhysPt addr,uint16_t * val);
	virtual bool readd_checked(PhysPt addr,uint32_t * val);
	virtual bool writeb_checked(PhysPt addr, uint8_t val);
	virtual bool writew_checked(PhysPt addr, uint16_t val);
	virtual bool writed_checked(PhysPt addr, uint32_t val);

	uint_fast8_t flags = 0x0;
};

/* Some other functions */
void PAGING_Enable(bool enabled);
bool PAGING_Enabled(void);

Bitu PAGING_GetDirBase(void);
void PAGING_SetDirBase(Bitu cr3);
void PAGING_InitTLB(void);
void PAGING_ClearTLB(void);

void PAGING_LinkPage(uint32_t lin_page,uint32_t phys_page);
void PAGING_LinkPage_ReadOnly(uint32_t lin_page,uint32_t phys_page);
void PAGING_UnlinkPages(Bitu lin_page,Bitu pages);
/* This maps the page directly, only use when paging is disabled */
void PAGING_MapPage(Bitu lin_page,Bitu phys_page);
bool PAGING_MakePhysPage(Bitu & page);
bool PAGING_ForcePageInit(Bitu lin_addr);

void MEM_SetLFB(Bitu page, Bitu pages, PageHandler *handler, PageHandler *mmiohandler);
void MEM_SetPageHandler(Bitu phys_page, Bitu pages, PageHandler * handler);
void MEM_ResetPageHandler(Bitu phys_page, Bitu pages);


#ifdef _MSC_VER
#pragma pack (1)
#endif
struct X86_PageEntryBlock{
#ifdef WORDS_BIGENDIAN
	uint32_t		base:20;
	uint32_t		avl:3;
	uint32_t		g:1;
	uint32_t		pat:1;
	uint32_t		d:1;
	uint32_t		a:1;
	uint32_t		pcd:1;
	uint32_t		pwt:1;
	uint32_t		us:1;
	uint32_t		wr:1;
	uint32_t		p:1;
#else
	uint32_t		p:1;
	uint32_t		wr:1;
	uint32_t		us:1;
	uint32_t		pwt:1;
	uint32_t		pcd:1;
	uint32_t		a:1;
	uint32_t		d:1;
	uint32_t		pat:1;
	uint32_t		g:1;
	uint32_t		avl:3;
	uint32_t		base:20;
#endif
} GCC_ATTRIBUTE(packed);
#ifdef _MSC_VER
#pragma pack ()
#endif


union X86PageEntry {
	uint32_t load;
	X86_PageEntryBlock block;
};

#if !defined(USE_FULL_TLB)
typedef struct {
	HostPt read;
	HostPt write;
	PageHandler * readhandler;
	PageHandler * writehandler;
	uint32_t phys_page;
} tlb_entry;
#endif

struct PagingBlock {
	uint32_t			cr3;
	uint32_t			cr2;
	struct {
		uint32_t page;
		PhysPt addr;
	} base;
#if defined(USE_FULL_TLB)
	struct {
		HostPt read[TLB_SIZE];
		HostPt write[TLB_SIZE];
		PageHandler * readhandler[TLB_SIZE];
		PageHandler * writehandler[TLB_SIZE];
		uint32_t	phys_page[TLB_SIZE];
	} tlb;
#else
	tlb_entry tlbh[TLB_SIZE];
	tlb_entry *tlbh_banks[TLB_BANKS];
#endif
	struct {
		uint32_t used;
		uint32_t entries[PAGING_LINKS];
	} links;
	uint32_t		firstmb[LINK_START];
	bool		enabled;
};

extern PagingBlock paging; 

/* Some support functions */

PageHandler * MEM_GetPageHandler(Bitu phys_page);


/* Unaligned address handlers */
uint16_t mem_unalignedreadw(PhysPt address);
uint32_t mem_unalignedreadd(PhysPt address);
void mem_unalignedwritew(PhysPt address,uint16_t val);
void mem_unalignedwrited(PhysPt address,uint32_t val);

bool mem_unalignedreadw_checked(PhysPt address,uint16_t * val);
bool mem_unalignedreadd_checked(PhysPt address,uint32_t * val);
bool mem_unalignedwritew_checked(PhysPt address,uint16_t val);
bool mem_unalignedwrited_checked(PhysPt address,uint32_t val);

#if defined(USE_FULL_TLB)

static inline HostPt get_tlb_read(PhysPt address) {
	return paging.tlb.read[address>>12];
}
static inline HostPt get_tlb_write(PhysPt address) {
	return paging.tlb.write[address>>12];
}
static inline PageHandler* get_tlb_readhandler(PhysPt address) {
	return paging.tlb.readhandler[address>>12];
}
static inline PageHandler* get_tlb_writehandler(PhysPt address) {
	return paging.tlb.writehandler[address>>12];
}

/* Use these helper functions to access linear addresses in readX/writeX functions */
static inline PhysPt PAGING_GetPhysicalPage(PhysPt linePage) {
	return (paging.tlb.phys_page[linePage>>12]<<12);
}

static inline PhysPt PAGING_GetPhysicalAddress(PhysPt linAddr) {
	return (paging.tlb.phys_page[linAddr>>12]<<12)|(linAddr&0xfff);
}

#else

void PAGING_InitTLBBank(tlb_entry **bank);

static inline tlb_entry *get_tlb_entry(PhysPt address) {
	Bitu index=(address>>12);
	if (TLB_BANKS && (index >= TLB_SIZE)) {
		Bitu bank=(address>>BANK_SHIFT) - 1;
		if (!paging.tlbh_banks[bank])
			PAGING_InitTLBBank(&paging.tlbh_banks[bank]);
		return &paging.tlbh_banks[bank][index & BANK_MASK];
	}
	return &paging.tlbh[index];
}

static inline HostPt get_tlb_read(PhysPt address) {
	return get_tlb_entry(address)->read;
}
static inline HostPt get_tlb_write(PhysPt address) {
	return get_tlb_entry(address)->write;
}
static inline PageHandler* get_tlb_readhandler(PhysPt address) {
	return get_tlb_entry(address)->readhandler;
}
static inline PageHandler* get_tlb_writehandler(PhysPt address) {
	return get_tlb_entry(address)->writehandler;
}

/* Use these helper functions to access linear addresses in readX/writeX functions */
static inline PhysPt PAGING_GetPhysicalPage(PhysPt linePage) {
	tlb_entry *entry = get_tlb_entry(linePage);
	return (entry->phys_page<<12);
}

static inline PhysPt PAGING_GetPhysicalAddress(PhysPt linAddr) {
	tlb_entry *entry = get_tlb_entry(linAddr);
	return (entry->phys_page<<12)|(linAddr&0xfff);
}
#endif

/* Special inlined memory reading/writing */

static inline uint8_t mem_readb_inline(PhysPt address) {
	HostPt tlb_addr=get_tlb_read(address);
	if (tlb_addr) return host_readb(tlb_addr+address);
	else
		return (get_tlb_readhandler(address))->readb(address);
}

static inline uint16_t mem_readw_inline(PhysPt address) {
	if ((address & 0xfff)<0xfff) {
		HostPt tlb_addr=get_tlb_read(address);
		if (tlb_addr) return host_readw(tlb_addr+address);
		else
			return (get_tlb_readhandler(address))->readw(address);
	} else return mem_unalignedreadw(address);
}

static inline uint32_t mem_readd_inline(PhysPt address)
{
	if ((address & 0xfff) < 0xffd) {
		HostPt tlb_addr = get_tlb_read(address);
		if (tlb_addr)
			return host_readd(tlb_addr + address);
		else
			return get_tlb_readhandler(address)->readd(address);
	} else {
		return mem_unalignedreadd(address);
	}
}

static inline void mem_writeb_inline(PhysPt address,uint8_t val) {
	HostPt tlb_addr=get_tlb_write(address);
	if (tlb_addr) host_writeb(tlb_addr+address,val);
	else (get_tlb_writehandler(address))->writeb(address,val);
}

static inline void mem_writew_inline(PhysPt address,uint16_t val) {
	if ((address & 0xfff)<0xfff) {
		HostPt tlb_addr=get_tlb_write(address);
		if (tlb_addr) host_writew(tlb_addr+address,val);
		else (get_tlb_writehandler(address))->writew(address,val);
	} else mem_unalignedwritew(address,val);
}

static inline void mem_writed_inline(PhysPt address,uint32_t val) {
	if ((address & 0xfff)<0xffd) {
		HostPt tlb_addr=get_tlb_write(address);
		if (tlb_addr) host_writed(tlb_addr+address,val);
		else (get_tlb_writehandler(address))->writed(address,val);
	} else mem_unalignedwrited(address,val);
}


static inline bool mem_readb_checked(PhysPt address, uint8_t * val) {
	HostPt tlb_addr=get_tlb_read(address);
	if (tlb_addr) {
		*val=host_readb(tlb_addr+address);
		return false;
	} else return (get_tlb_readhandler(address))->readb_checked(address, val);
}

static inline bool mem_readw_checked(PhysPt address, uint16_t * val) {
	if ((address & 0xfff)<0xfff) {
		HostPt tlb_addr=get_tlb_read(address);
		if (tlb_addr) {
			*val=host_readw(tlb_addr+address);
			return false;
		} else return (get_tlb_readhandler(address))->readw_checked(address, val);
	} else return mem_unalignedreadw_checked(address, val);
}

static inline bool mem_readd_checked(PhysPt address, uint32_t * val) {
	if ((address & 0xfff)<0xffd) {
		HostPt tlb_addr=get_tlb_read(address);
		if (tlb_addr) {
			*val=host_readd(tlb_addr+address);
			return false;
		} else return (get_tlb_readhandler(address))->readd_checked(address, val);
	} else return mem_unalignedreadd_checked(address, val);
}

static inline bool mem_writeb_checked(PhysPt address,uint8_t val) {
	HostPt tlb_addr=get_tlb_write(address);
	if (tlb_addr) {
		host_writeb(tlb_addr+address,val);
		return false;
	} else return (get_tlb_writehandler(address))->writeb_checked(address,val);
}

static inline bool mem_writew_checked(PhysPt address,uint16_t val) {
	if ((address & 0xfff)<0xfff) {
		HostPt tlb_addr=get_tlb_write(address);
		if (tlb_addr) {
			host_writew(tlb_addr+address,val);
			return false;
		} else return (get_tlb_writehandler(address))->writew_checked(address,val);
	} else return mem_unalignedwritew_checked(address,val);
}

static inline bool mem_writed_checked(PhysPt address,uint32_t val) {
	if ((address & 0xfff)<0xffd) {
		HostPt tlb_addr=get_tlb_write(address);
		if (tlb_addr) {
			host_writed(tlb_addr+address,val);
			return false;
		} else return (get_tlb_writehandler(address))->writed_checked(address,val);
	} else return mem_unalignedwrited_checked(address,val);
}


#endif
