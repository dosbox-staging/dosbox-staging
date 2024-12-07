/*
 *  Copyright (C) 2021-2024  The DOSBox Staging Team
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

#include "paging.h"

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <memory>

#include "mem.h"
#include "regs.h"
#include "lazyflags.h"
#include "cpu.h"
#include "debug.h"
#include "setup.h"

/*
  DBP: Added improved paging implementation with C++ exception page faults from Taewoong's Daum branch
       It is used when running with the normal core to increase compatibility.
       Because it does not support the dynamic core, an enhanced version of the original paging behavior is used for that.
*/

#define LINK_TOTAL		(64*1024)

#define USERWRITE_PROHIBITED			((cpu.cpl&cpu.mpl)==3)

#define CPU_HAS_WP_FLAG					(cpu.cr0&CR0_WRITEPROTECT)

// Comment in when using verbose logging
//static uint32_t logcnt;

PagingBlock paging;

uint8_t PageHandler::readb(PhysPt addr)
{
	E_Exit("No byte handler for read from %d",addr);	
	return 0;
}
uint16_t PageHandler::readw(PhysPt addr)
{
	uint16_t ret = (readb(addr+0) << 0);
	ret     |= (readb(addr+1) << 8);
	return ret;
}
uint32_t PageHandler::readd(PhysPt addr)
{
	uint32_t ret = (readb(addr+0) << 0);
	ret     |= (readb(addr+1) << 8);
	ret     |= (readb(addr+2) << 16);
	ret     |= (readb(addr+3) << 24);
	return ret;
}
uint64_t PageHandler::readq(PhysPt addr) {
    uint64_t ret = 0;
    for (int i = 0; i < 8; ++i) {
        ret |= static_cast<uint64_t>(readb(addr + i)) << (i * 8);
    }
    return ret;
}

void PageHandler::writeb(PhysPt addr, uint8_t /*val*/)
{
	E_Exit("No byte handler for write to %d",addr);
}

void PageHandler::writew(PhysPt addr, uint16_t val)
{
	writeb(addr+0,(uint8_t) (val >> 0));
	writeb(addr+1,(uint8_t) (val >> 8));
}
void PageHandler::writed(PhysPt addr, uint32_t val)
{
	writeb(addr+0,(uint8_t) (val >> 0));
	writeb(addr+1,(uint8_t) (val >> 8));
	writeb(addr+2,(uint8_t) (val >> 16));
	writeb(addr+3,(uint8_t) (val >> 24));
}
void PageHandler::writeq(PhysPt addr, uint64_t val) {
    for (int i = 0; i < 8; ++i) {
        writeb(addr + i, static_cast<uint8_t>(val >> (i * 8)));
    }
}

HostPt PageHandler::GetHostReadPt(Bitu /*phys_page*/) {
	return nullptr;
}

HostPt PageHandler::GetHostWritePt(Bitu /*phys_page*/) {
	return nullptr;
}

bool PageHandler::readb_checked(PhysPt addr, uint8_t *val)
{
	*val = readb(addr);
	return false;
}
bool PageHandler::readw_checked(PhysPt addr, uint16_t *val)
{
	*val = readw(addr);
	return false;
}
bool PageHandler::readd_checked(PhysPt addr, uint32_t *val)
{
	*val = readd(addr);
	return false;
}
bool PageHandler::readq_checked(PhysPt addr, uint64_t* val)
{
	*val = readq(addr);
	return false;
}

bool PageHandler::writeb_checked(PhysPt addr, uint8_t val)
{
	writeb(addr,val);	return false;
}
bool PageHandler::writew_checked(PhysPt addr, uint16_t val)
{
	writew(addr,val);	return false;
}
bool PageHandler::writed_checked(PhysPt addr, uint32_t val)
{
	writed(addr,val);	return false;
}
bool PageHandler::writeq_checked(PhysPt addr, uint64_t val)
{
	writeq(addr, val);
	return false;
}

struct PF_Entry {
	uint32_t cs;
	uint32_t eip;
	uint32_t page_addr;
	uint32_t mpl;
};

#define PF_QUEUESIZE 384
static struct {
	uint8_t used = 0; // keeps track of number of entries
	PF_Entry entries[PF_QUEUESIZE];
} pf_queue;


#ifdef C_DBP_PAGE_FAULT_QUEUE_WIPE
extern void DOSBOX_WipePageFaultQueue();
extern void DOSBOX_ResetCPUDecoder();
extern bool DOSBOX_IsWipingPageFaultQueue;
static uint32_t DBP_PageFaultCycles;
#endif

#define ACCESS_KR  0
#define ACCESS_KRW 1
#define ACCESS_UR  2
#define ACCESS_URW 3
#define ACCESS_TABLEFAULT 4
//const char* const mtr[] = {"KR ","KRW","UR ","URW","PFL"};

// bit0 entry write
// bit1 entry access
// bit2 table write
// bit3 table access
// These arrays define how the access bits in the page table and entry
// result in access rights.
// The used array is switched depending on the CPU under emulation.

// Intel says the lowest numeric value wins for both 386 and 486+
// There's something strange about KR with WP=1 though
static const uint8_t translate_array[] = {
	ACCESS_KR,		// 00 00
	ACCESS_KR,		// 00 01
	ACCESS_KR,		// 00 10
	ACCESS_KR,		// 00 11
	ACCESS_KR,		// 01 00
	ACCESS_KRW,		// 01 01
	ACCESS_KR, //	ACCESS_KRW,		// 01 10
	ACCESS_KRW,		// 01 11
	ACCESS_KR,		// 10 00
	ACCESS_KR, //	ACCESS_KRW,		// 10 01
	ACCESS_UR,		// 10 10
	ACCESS_UR,		// 10 11
	ACCESS_KR,		// 11 00
	ACCESS_KRW,		// 11 01
	ACCESS_UR,		// 11 10
	ACCESS_URW		// 11 11
};

// This array defines how a page is mapped depending on 
// page access right, cpl==3, and WP.
// R = map handler as read, W = map handler as write, E = map exception handler
#define ACMAP_RW 0
#define ACMAP_RE 1
#define ACMAP_EE 2

//static const char* const lnm[] = {"RW ","RE ","EE "}; // debug stuff

// bit0-1 ACCESS_ type
// bit2   1=user mode
// bit3   WP on

static const uint8_t xlat_mapping[] = {
//  KR        KRW       UR        URW
	// index 0-3   kernel, wp 0
	ACMAP_RW, ACMAP_RW, ACMAP_RW, ACMAP_RW,
	// index 4-7   user,   wp 0
	ACMAP_EE, ACMAP_EE, ACMAP_RE, ACMAP_RW,
	// index 8-11  kernel, wp 1
	ACMAP_RE, ACMAP_RW, ACMAP_RE, ACMAP_RW,
	// index 11-15 user,   wp 1 (same as user, wp 0)
	ACMAP_EE, ACMAP_EE, ACMAP_RE, ACMAP_RW,
};

// This table can figure out if we are going to fault right now in the init handler
// (1=fault) 
// bit0-1 ACCESS_ type
// bit2   1=user mode
// bit3   1=writing
// bit4   wp

static const uint8_t fault_table[] = {
//	KR	KRW	UR	URW
	// WP 0
	// index 0-3   kernel, reading
	0,	0,	0,	0,
	// index 4-7   user,   reading
	1,	1,	0,	0,
	// index 8-11  kernel, writing
	0,	0,	0,	0,
	// index 11-15 user,   writing
	1,	1,	1,	0,
	// WP 1
	// index 0-3   kernel, reading
	0,	0,	0,	0,
	// index 4-7   user,   reading
	1,	1,	0,	0,
	// index 8-11  kernel, writing
	1,	0,	1,	0,
	// index 11-15 user,   writing
	1,	1,	1,	0,
};

#define PHYSPAGE_DITRY 0x10000000
#define PHYSPAGE_ADDR  0x000FFFFF

// helper functions for calculating table entry addresses
static inline PhysPt GetPageDirectoryEntryAddr(PhysPt lin_addr) {
	return paging.base.addr | ((lin_addr >> 22) << 2);
}
static inline PhysPt GetPageTableEntryAddr(PhysPt lin_addr, X86PageEntry& dir_entry) {
	return (dir_entry.base<<12) | ((lin_addr >> 10) & 0xffc);
}

static Bits PageFaultCore(void) {
	CPU_CycleLeft+=CPU_Cycles;
	CPU_Cycles=1;
	Bits ret=CPU_Core_Full_Run();
	CPU_CycleLeft+=CPU_Cycles;
	if (ret<0) E_Exit("Got a dosbox close machine in pagefault core?");
	if (ret) 
		return ret;
#ifndef C_DBP_PAGE_FAULT_QUEUE_WIPE
	if (!pf_queue.used) E_Exit("PF Core without PF");
	PF_Entry * entry=&pf_queue.entries[pf_queue.used-1];
#else // support loading save state into a pagefault
	if (!pf_queue.used) DOSBOX_ResetCPUDecoder();
	PF_Entry * entry=&pf_queue.entries[pf_queue.used?pf_queue.used-1:0];
#endif
	X86PageEntry pentry;
	pentry.set(phys_readd(entry->page_addr));
	if (pentry.p && entry->cs == SegValue(cs) && entry->eip==reg_eip) {
		cpu.mpl=entry->mpl;
		return -1;
	}
#ifdef C_DBP_PAGE_FAULT_QUEUE_WIPE
	if (DOSBOX_IsWipingPageFaultQueue) return -1;
	if ((pf_queue.used >= 2 && DBP_PageFaultCycles++ > 5000000) || pf_queue.used > 50)
	{
		LOG(LOG_PAGING,LOG_NORMAL)("Wiping page fault queue after %d queueups", pf_queue.used);
		DOSBOX_WipePageFaultQueue();
		DBP_PageFaultCycles = 0;
		return -1;
	}
#endif
	return 0;
}
#if C_DEBUG
Bitu DEBUG_EnableDebugger(void);
#endif


void PAGING_PageFault(PhysPt lin_addr,uint32_t page_addr,uint32_t faultcode) {
#ifdef C_DBP_PAGE_FAULT_QUEUE_WIPE
	if (pf_queue.used > 60)
	{
		LOG(LOG_PAGING,LOG_NORMAL)("Emergency wiping page fault queue after %d queueups", pf_queue.used);
		DOSBOX_WipePageFaultQueue();
		return;
	}
	if (DOSBOX_IsWipingPageFaultQueue) return;
	DBP_PageFaultCycles = 0;
#endif
	/* Save the state of the cpu cores */
	const auto old_lflags = lflags;
	const auto old_cpudecoder=cpudecoder;
	cpudecoder=&PageFaultCore;
	paging.cr2=lin_addr;

	assert(pf_queue.used < PF_QUEUESIZE);

	PF_Entry * entry=&pf_queue.entries[pf_queue.used++];
	//LOG(LOG_PAGING,LOG_NORMAL)("PageFault at %X type [%x] queue %d (paging_prevent_exception_jump: %d)",lin_addr,faultcode,pf_queue.used, (int)paging_prevent_exception_jump);
	//LOG(LOG_PAGING,LOG_NORMAL)("    EAX:%04X ECX:%04X EDX:%04X EBX:%04X",reg_eax,reg_ecx,reg_edx,reg_ebx);
	//LOG(LOG_PAGING,LOG_NORMAL)("    CS:%04X EIP:%08X SS:%04x SP:%08X",SegValue(cs),reg_eip,SegValue(ss),reg_esp);
	//LOG_MSG("EAX:%04X ECX:%04X EDX:%04X EBX:%04X",reg_eax,reg_ecx,reg_edx,reg_ebx);
	//LOG_MSG("CS:%04X EIP:%08X SS:%04x SP:%08X",SegValue(cs),reg_eip,SegValue(ss),reg_esp);	entry->cs=SegValue(cs);

	entry->eip=reg_eip;
	entry->page_addr=page_addr;
	entry->mpl=cpu.mpl;
	cpu.mpl=3;

	CPU_Exception(EXCEPTION_PF,faultcode);
	DOSBOX_RunMachine();
	pf_queue.used--;
	//LOG(LOG_PAGING,LOG_NORMAL)("Left PageFault for %x queue %d",lin_addr,pf_queue.used);
	lflags = old_lflags;
#ifdef C_DBP_PAGE_FAULT_QUEUE_WIPE //DBP: Added this check to support page fault queue wiping
	if (DOSBOX_IsWipingPageFaultQueue) return;
#endif
	cpudecoder=old_cpudecoder;
//	LOG_MSG("SS:%04x SP:%08X",SegValue(ss),reg_esp);
}

static inline void InitPageCheckPresence(PhysPt lin_addr,bool writing,X86PageEntry& table,X86PageEntry& entry) {
	const auto lin_page=lin_addr >> 12;
	const auto d_index=lin_page >> 10;
	const auto t_index=lin_page & 0x3ff;
	const auto table_addr=(paging.base.page<<12)+d_index*4;
	table.set(phys_readd(table_addr));
	if (!table.p) {
		LOG(LOG_PAGING, LOG_NORMAL)("NP Table");
		PAGING_PageFault(lin_addr,table_addr,
			(writing?0x02:0x00) | (((cpu.cpl&cpu.mpl)==0)?0x00:0x04));
		table.set(phys_readd(table_addr));
		if (!table.p) [[unlikely]] {
#ifdef C_DBP_PAGE_FAULT_QUEUE_WIPE
			if (DOSBOX_IsWipingPageFaultQueue) return;
#else // avoid calling E_Exit after DBP_DOSBOX_ForceShutdown has been called
			E_Exit("Pagefault didn't correct table");
#endif
		}
	}
	const auto entry_addr = (table.base << 12) + t_index * 4;
	entry.set(phys_readd(entry_addr));
	if (!entry.p) {
		//		LOG(LOG_PAGING,LOG_NORMAL)("NP Page");
		PAGING_PageFault(lin_addr,entry_addr,
			(writing?0x02:0x00) | (((cpu.cpl&cpu.mpl)==0)?0x00:0x04));
		entry.set(phys_readd(entry_addr));
		if (!entry.p) [[unlikely]]  {
#ifdef C_DBP_PAGE_FAULT_QUEUE_WIPE
			if (DOSBOX_IsWipingPageFaultQueue) return;
#else // avoid calling E_Exit after DBP_DOSBOX_ForceShutdown has been called
			E_Exit("Pagefault didn't correct page");
#endif
		}
	}
}


bool paging_prevent_exception_jump;	/* when set, do recursive page fault mode (when not executing instruction) */

// PAGING_NewPageFault
// lin_addr, page_addr: the linear and page address the fault happened at
// prepare_only: true in case the calling core handles the fault, else the PageFaultCore does
static void PAGING_NewPageFault(PhysPt lin_addr, Bitu page_addr, bool prepare_only, Bitu faultcode) {
#ifdef C_DBP_PAGE_FAULT_QUEUE_WIPE
	if (DOSBOX_IsWipingPageFaultQueue) return;
#endif
	paging.cr2=lin_addr;
	//LOG_MSG("FAULT q%d, code %x",  pf_queue.used, faultcode);
	//PrintPageInfo("FA+",lin_addr,faultcode, prepare_only);
	if (prepare_only) {
		cpu.exception.which = EXCEPTION_PF;
		cpu.exception.error = faultcode;
		//LOG_MSG("[%8u] [@%8d] PageFault at %X type [%x] PREPARE",logcnt++, CPU_Cycles,lin_addr,cpu.exception.error);
	} else if (!paging_prevent_exception_jump) {
		//LOG_MSG("[%8u] [@%8d] PageFault at %X type [%x] queue 1",logcnt++, CPU_Cycles,lin_addr,faultcode);
		THROW_PAGE_FAULT(faultcode);
	} else {
		PAGING_PageFault(lin_addr,page_addr,faultcode);
	}
}

class PageFoilHandler : public PageHandler {
private:
	void work(PhysPt addr) {
		Bitu lin_page = addr >> 12;
		uint32_t phys_page = paging.tlb.phys_page[lin_page] & PHYSPAGE_ADDR;

		// set the page dirty in the tlb
		paging.tlb.phys_page[lin_page] |= PHYSPAGE_DITRY;

		// mark the page table entry dirty
		X86PageEntry dir_entry, table_entry;

		PhysPt dirEntryAddr = GetPageDirectoryEntryAddr(addr);
		dir_entry.set(phys_readd(dirEntryAddr));
		//if (!dir_entry.p) E_Exit("Undesired situation 1 in page foiler.");

		PhysPt tableEntryAddr = GetPageTableEntryAddr(addr, dir_entry);
		table_entry.set(phys_readd(tableEntryAddr));
		//if (!table_entry.p) E_Exit("Undesired situation 2 in page foiler.");

		//// for debugging...
		//if (table_entry.base != phys_page)
		//	E_Exit("Undesired situation 3 in page foiler.");

		// map the real write handler in our place
		PageHandler* handler = MEM_GetPageHandler(phys_page);

		// debug
		//LOG_MSG("FOIL            LIN% 8x PHYS% 8x [%x/%x/%x] WRP % 8x", addr, phys_page,
		//	dirEntryAddr, tableEntryAddr, table_entry.get(), wtest);

		// this can happen when the same page table is used at two different
		// page directory entries / linear locations (WfW311)
		// if (table_entry.d) E_Exit("% 8x Already dirty!!",table_entry.get());

		// set the dirty bit
		table_entry.d=1;
		phys_writed(tableEntryAddr,table_entry.get());

		// replace this handler with the real thing
		if (handler->flags & PFLAG_WRITEABLE)
			paging.tlb.write[lin_page] = handler->GetHostWritePt(phys_page) - (lin_page << 12);
		else paging.tlb.write[lin_page]=0;
		paging.tlb.writehandler[lin_page]=handler;
	}

	void read() {
		E_Exit("The page foiler shouldn't be read.");
	}
public:
	PageFoilHandler() { flags = PFLAG_INIT|PFLAG_NOCODE; }
	uint8_t readb(PhysPt addr) {read();return 0;}
	uint16_t readw(PhysPt addr) {read();return 0;}
	uint32_t readd(PhysPt addr) {read();return 0;}

	void writeb(PhysPt addr,uint8_t val) {
		work(addr);
		// execute the write:
		// no need to care about mpl because we won't be entered
		// if write isn't allowed
		mem_writeb(addr,val);
	}
	void writew(PhysPt addr,uint16_t val) {
		work(addr);
		mem_writew(addr,val);
	}
	void writed(PhysPt addr,uint32_t val) {
		work(addr);
		mem_writed(addr,val);
	}

	bool readb_checked(PhysPt addr, uint8_t * val) {read();return true;}
	bool readw_checked(PhysPt addr, uint16_t * val) {read();return true;}
	bool readd_checked(PhysPt addr, uint32_t * val) {read();return true;}

	bool writeb_checked(PhysPt addr,uint8_t val) {
		work(addr);
		mem_writeb(addr,val);
		return false;
	}
	bool writew_checked(PhysPt addr,uint16_t val) {
		work(addr);
		mem_writew(addr,val);
		return false;
	}
	bool writed_checked(PhysPt addr,uint32_t val) {
		work(addr);
		mem_writed(addr,val);
		return false;
	}
};

class ExceptionPageHandler : public PageHandler {
private:
	PageHandler* getHandler(PhysPt addr) {
		Bitu lin_page = addr >> 12;
		uint32_t phys_page = paging.tlb.phys_page[lin_page] & PHYSPAGE_ADDR;
		PageHandler* handler = MEM_GetPageHandler(phys_page);
		return handler;
	}

	bool hack_check(PhysPt addr) {
		// First Encounters
		// They change the page attributes without clearing the TLB.
		// On a real 486 they get away with it because its TLB has only 32 or so 
		// elements. The changed page attribs get overwritten and re-read before
		// the exception happens. Here we have gazillions of TLB entries so the
		// exception occurs if we don't check for it.

		Bitu old_attirbs = paging.tlb.phys_page[addr>>12] >> 30;
		X86PageEntry dir_entry, table_entry;
		
		dir_entry.set(phys_readd(GetPageDirectoryEntryAddr(addr)));
		if (!dir_entry.p) return false;
		table_entry.set(phys_readd(GetPageTableEntryAddr(addr, dir_entry)));
		if (!table_entry.p) return false;
		Bitu result =
		translate_array[((dir_entry.get()<<1)&0xc) | ((table_entry.get()>>1)&0x3)];
		if (result != old_attirbs) return true;
		return false;
	}

	void Exception(PhysPt addr, bool writing, bool checked) {
		//PrintPageInfo("XCEPT",addr,writing, checked);
		//LOG_MSG("XCEPT LIN% 8x wr%d, ch%d, cpl%d, mpl%d",addr, writing, checked, cpu.cpl, cpu.mpl);
		PhysPt tableaddr = 0;
		if (!checked) {
			X86PageEntry dir_entry;
			dir_entry.set(phys_readd(GetPageDirectoryEntryAddr(addr)));
			if (!dir_entry.p) E_Exit("Undesired situation 1 in exception handler.");
			
			// page table entry
			tableaddr = GetPageTableEntryAddr(addr, dir_entry);
			//Bitu d_index=(addr >> 12) >> 10;
			//tableaddr=(paging.base.page<<12) | (d_index<<2);
		} 
		PAGING_NewPageFault(addr, tableaddr, checked,
			1 | (writing? 2:0) | (((cpu.cpl&cpu.mpl)==3)? 4:0));
		
		PAGING_ClearTLB(); // TODO got a better idea?
	}

	uint8_t readb_through(PhysPt addr) {
		Bitu lin_page = addr >> 12;
		uint32_t phys_page = paging.tlb.phys_page[lin_page] & PHYSPAGE_ADDR;
		PageHandler* handler = MEM_GetPageHandler(phys_page);
		if (handler->flags & PFLAG_READABLE) {
			return host_readb(handler->GetHostReadPt(phys_page) + (addr&0xfff));
		}
		else return handler->readb(addr);
	}
	uint16_t readw_through(PhysPt addr) {
		Bitu lin_page = addr >> 12;
		uint32_t phys_page = paging.tlb.phys_page[lin_page] & PHYSPAGE_ADDR;
		PageHandler* handler = MEM_GetPageHandler(phys_page);
		if (handler->flags & PFLAG_READABLE) {
			return host_readw(handler->GetHostReadPt(phys_page) + (addr&0xfff));
		}
		else return handler->readw(addr);
	}
	uint32_t readd_through(PhysPt addr) {
		Bitu lin_page = addr >> 12;
		uint32_t phys_page = paging.tlb.phys_page[lin_page] & PHYSPAGE_ADDR;
		PageHandler* handler = MEM_GetPageHandler(phys_page);
		if (handler->flags & PFLAG_READABLE) {
			return host_readd(handler->GetHostReadPt(phys_page) + (addr&0xfff));
		}
		else return handler->readd(addr);
	}
	void writeb_through(PhysPt addr, uint8_t val) {
		Bitu lin_page = addr >> 12;
		uint32_t phys_page = paging.tlb.phys_page[lin_page] & PHYSPAGE_ADDR;
		PageHandler* handler = MEM_GetPageHandler(phys_page);
		if (handler->flags & PFLAG_WRITEABLE) {
			return host_writeb(handler->GetHostWritePt(phys_page) + (addr&0xfff), (uint8_t)val);
		}
		else return handler->writeb(addr, val);
	}
	void writew_through(PhysPt addr, uint16_t val) {
		Bitu lin_page = addr >> 12;
		uint32_t phys_page = paging.tlb.phys_page[lin_page] & PHYSPAGE_ADDR;
		PageHandler* handler = MEM_GetPageHandler(phys_page);
		if (handler->flags & PFLAG_WRITEABLE) {
			return host_writew(handler->GetHostWritePt(phys_page) + (addr&0xfff), (uint16_t)val);
		}
		else return handler->writew(addr, val);
	}
	void writed_through(PhysPt addr, uint32_t val) {
		Bitu lin_page = addr >> 12;
		uint32_t phys_page = paging.tlb.phys_page[lin_page] & PHYSPAGE_ADDR;
		PageHandler* handler = MEM_GetPageHandler(phys_page);
		if (handler->flags & PFLAG_WRITEABLE) {
			return host_writed(handler->GetHostWritePt(phys_page) + (addr&0xfff), val);
		}
		else return handler->writed(addr, val);
	}

public:
	ExceptionPageHandler() { flags = PFLAG_INIT|PFLAG_NOCODE; }
	uint8_t readb(PhysPt addr) {
		if (!cpu.mpl) return readb_through(addr);

		Exception(addr, false, false);
		return mem_readb(addr); // read the updated page (unlikely to happen?)
	}
	uint16_t readw(PhysPt addr) {
		// access type is supervisor mode (temporary)
		// we are always allowed to read in superuser mode
		// so get the handler and address and read it
		if (!cpu.mpl) return readw_through(addr);

		Exception(addr, false, false);
		return mem_readw(addr);
	}
	uint32_t readd(PhysPt addr) {
		if (!cpu.mpl) return readd_through(addr);

		Exception(addr, false, false);
		return mem_readd(addr);
	}
	void writeb(PhysPt addr,uint8_t val) {
		if (!cpu.mpl) {
			writeb_through(addr, val);
			return;
		}
		Exception(addr, true, false);
		mem_writeb(addr, val);
	}
	void writew(PhysPt addr,uint16_t val) {
		if (!cpu.mpl) {
			// TODO Exception on a KR-page?
			writew_through(addr, val);
			return;
		}
		if (hack_check(addr)) {
			LOG_MSG("Page attributes modified without clear");
			PAGING_ClearTLB();
			mem_writew(addr,val);
			return;
		}
		// firstenc here
		Exception(addr, true, false);
		mem_writew(addr, val);
	}
	void writed(PhysPt addr,uint32_t val) {
		if (!cpu.mpl) {
			writed_through(addr, val);
			return;
		}
		Exception(addr, true, false);
		mem_writed(addr, val);
	}
	// returning true means an exception was triggered for these _checked functions
	bool readb_checked(PhysPt addr, uint8_t * val) {
		Exception(addr, false, true);
		return true;
	}
	bool readw_checked(PhysPt addr, uint16_t * val) {
		Exception(addr, false, true);
		return true;
	}
	bool readd_checked(PhysPt addr, uint32_t * val) {
		Exception(addr, false, true);
		return true;
	}
	bool writeb_checked(PhysPt addr,uint8_t val) {
		Exception(addr, true, true);
		return true;
	}
	bool writew_checked(PhysPt addr,uint16_t val) {
		if (hack_check(addr)) {
			LOG_MSG("Page attributes modified without clear");
			PAGING_ClearTLB();
			mem_writew(addr,val); // TODO this makes us recursive again?
			return false;
		}
		Exception(addr, true, true);
		return true;
	}
	bool writed_checked(PhysPt addr,uint32_t val) {
		Exception(addr, true, true);
		return true;
	}
};

class NewInitPageHandler : public PageHandler {
public:
	NewInitPageHandler() { flags = PFLAG_INIT|PFLAG_NOCODE; }
	uint8_t readb(PhysPt addr) {
		if (InitPage(addr, false, false)) return 0;
		return mem_readb(addr);
	}
	uint16_t readw(PhysPt addr) {
		if (InitPage(addr, false, false)) return 0;
		return mem_readw(addr);
	}
	uint32_t readd(PhysPt addr) {
		if (InitPage(addr, false, false)) return 0;
		return mem_readd(addr);
	}
	void writeb(PhysPt addr,uint8_t val) {
		if (InitPage(addr, true, false)) return;
		mem_writeb(addr,val);
	}
	void writew(PhysPt addr,uint16_t val) {
		if (InitPage(addr, true, false)) return;
		mem_writew(addr,val);
	}
	void writed(PhysPt addr,uint32_t val) {
		if (InitPage(addr, true, false)) return;
		mem_writed(addr,val);
	}
	bool readb_checked(PhysPt addr, uint8_t * val) {
		if (InitPage(addr, false, true)) return true;
		*val=mem_readb(addr);
		return false;
	}
	bool readw_checked(PhysPt addr, uint16_t * val) {
		if (InitPage(addr, false, true)) return true;
		*val=mem_readw(addr);
		return false;
	}
	bool readd_checked(PhysPt addr, uint32_t * val) {
		if (InitPage(addr, false, true)) return true;
		*val=mem_readd(addr);
		return false;
	}
	bool writeb_checked(PhysPt addr,uint8_t val) {
		if (InitPage(addr, true, true)) return true;
		mem_writeb(addr,val);
		return false;
	}
	bool writew_checked(PhysPt addr,uint16_t val) {
		if (InitPage(addr, true, true)) return true;
		mem_writew(addr,val);
		return false;
	}
	bool writed_checked(PhysPt addr,uint32_t val) {
		if (InitPage(addr, true, true)) return true;
		mem_writed(addr,val);
		return false;
	}
	bool InitPage(PhysPt lin_addr, bool writing, bool prepare_only) {
		Bitu lin_page=lin_addr >> 12;
		Bitu phys_page;
		if (paging.enabled) {
			initpage_retry:
#ifdef C_DBP_PAGE_FAULT_QUEUE_WIPE
			if (DOSBOX_IsWipingPageFaultQueue) return true;
#endif
			X86PageEntry dir_entry, table_entry;
			bool isUser = (((cpu.cpl & cpu.mpl)==3)? true:false);

			// Read the paging stuff, throw not present exceptions if needed
			// and find out how the page should be mapped
			PhysPt dirEntryAddr = GetPageDirectoryEntryAddr(lin_addr);
			// Range check to avoid emulator segfault: phys_readd() reads from MemBase+addr and does NOT range check.
			// Needed to avoid segfault when running 1999 demo "Void Main" in a bootable Windows 95 image in pure DOS mode.
			if ((dirEntryAddr+4) <= (MEM_TotalPages()<<12u)) {
				dir_entry.set(phys_readd(dirEntryAddr));
			}
			else {
				LOG(LOG_CPU,LOG_WARN)("Page directory access beyond end of memory, page %08x >= %08x",(unsigned int)(dirEntryAddr>>12u),(unsigned int)MEM_TotalPages());
				dir_entry.set(0xFFFFFFFF);
			}

			if (!dir_entry.p) {
				// table pointer is not present, do a page fault
				PAGING_NewPageFault(lin_addr, dirEntryAddr, prepare_only,
					(writing? 2:0) | (isUser? 4:0));

				if (prepare_only) return true;
				else goto initpage_retry; // TODO maybe E_Exit after a few loops
			}
			PhysPt tableEntryAddr = GetPageTableEntryAddr(lin_addr, dir_entry);
			// Range check to avoid emulator segfault: phys_readd() reads from MemBase+addr and does NOT range check.
			if ((tableEntryAddr+4) <= (MEM_TotalPages()<<12u)) {
				table_entry.set(phys_readd(tableEntryAddr));
			}
			else {
				LOG(LOG_CPU,LOG_WARN)("Page table entry access beyond end of memory, page %08x >= %08x",(unsigned int)(tableEntryAddr>>12u),(unsigned int)MEM_TotalPages());
				table_entry.set(0xFFFFFFFF);
			}
			table_entry.set(phys_readd(tableEntryAddr));

			// set page table accessed (IA manual: A is set whenever the entry is 
			// used in a page translation)
			if (!dir_entry.a) {
				dir_entry.a = 1;		
				phys_writed(dirEntryAddr, dir_entry.get());
			}

			if (!table_entry.p) {
				// physpage pointer is not present, do a page fault
				PAGING_NewPageFault(lin_addr, tableEntryAddr, prepare_only,
					 (writing? 2:0) | (isUser? 4:0));

				if (prepare_only) return true;
				else goto initpage_retry;
			}
			//PrintPageInfo("INI",lin_addr,writing,prepare_only);

			Bitu result =
				translate_array[((dir_entry.get()<<1)&0xc) | ((table_entry.get()>>1)&0x3)];
			
			// If a page access right exception occurs we shouldn't change a or d
			// I'd prefer running into the prepared exception handler but we'd need
			// an additional handler that sets the 'a' bit - idea - foiler read?
			Bitu ft_index = result | (writing? 8:0) | (isUser? 4:0) | (CPU_HAS_WP_FLAG? 16:0);
			
			if (fault_table[ft_index]) [[unlikely]] {
				// exception error code format: 
				// bit0 - protection violation, bit1 - writing, bit2 - user mode
				PAGING_NewPageFault(lin_addr, tableEntryAddr, prepare_only,
					1 | (writing? 2:0) | (isUser? 4:0));

				if (prepare_only) return true;
				else goto initpage_retry; // unlikely to happen?
			}
			// save load to see if it changed later
			uint32_t table_load = table_entry.get();

			// if we are writing we can set it right here to save some CPU
			if (writing) table_entry.d = 1;

			// set page accessed
			table_entry.a = 1;
			
			// update if needed
			if (table_load != table_entry.get())
				phys_writed(tableEntryAddr, table_entry.get());

			// if the page isn't dirty and we are reading we need to map the foiler
			// (dirty = false)
			bool dirty = table_entry.d? true:false;
			/*
			LOG_MSG("INIT  %s LIN% 8x PHYS% 5x wr%x ch%x wp%x d%x c%x m%x a%x [%x/%x/%x]",
				mtr[result], lin_addr, table_entry.base,
				writing, prepare_only, paging.wp,
				dirty, cpu.cpl, cpu.mpl,
				((dir_entry.get()<<1)&0xc) | ((table_entry.get()>>1)&0x3),
				dirEntryAddr, tableEntryAddr, table_entry.get());
			*/
			// finally install the new page
			void PAGING_LinkPageNew(Bitu lin_page, Bitu phys_page, Bitu linkmode, bool dirty);
			PAGING_LinkPageNew(lin_page, table_entry.base, result, dirty);

		} else { // paging off
			if (lin_page<LINK_START) phys_page=paging.firstmb[lin_page];
			else phys_page=lin_page;
			PAGING_LinkPage(lin_page,phys_page);
		}
		return false;
	}
	void InitPageForced(Bitu lin_addr) {
		Bitu lin_page=lin_addr >> 12;
		Bitu phys_page;
		if (paging.enabled) {
			X86PageEntry table;
			X86PageEntry entry;
			InitPageCheckPresence((PhysPt)lin_addr,false,table,entry);

			if (!table.a) {
				table.a=1;		//Set access
				phys_writed((PhysPt)((paging.base.page<<12)+(lin_page >> 10)*4),table.get());
			}
			if (!entry.a) {
				entry.a=1;					//Set access
				phys_writed((table.base<<12)+(lin_page & 0x3ff)*4,entry.get());
			}
			phys_page=entry.base;
			// maybe use read-only page here if possible
		} else {
			if (lin_page<LINK_START) phys_page=paging.firstmb[lin_page];
			else phys_page=lin_page;
		}
		PAGING_LinkPage(lin_page,phys_page);
	}
};
			
static inline bool InitPageCheckPresence_CheckOnly(PhysPt lin_addr,bool writing,X86PageEntry& table,X86PageEntry& entry) {
	const auto lin_page=lin_addr >> 12;
	const auto d_index=lin_page >> 10;
	const auto t_index=lin_page & 0x3ff;
	const auto table_addr=(paging.base.page<<12)+d_index*4;
	table.set(phys_readd(table_addr));
	if (!table.p) {
		paging.cr2         = lin_addr;
		cpu.exception.which=EXCEPTION_PF;
		cpu.exception.error=(writing?0x02:0x00) | (((cpu.cpl&cpu.mpl)==0)?0x00:0x04);
		//LOG(LOG_PAGING,LOG_NORMAL)("PageFault at %X type [%x] PREPARE",lin_addr,cpu.exception.error);
		//LOG(LOG_PAGING,LOG_NORMAL)("    EAX:%04X ECX:%04X EDX:%04X EBX:%04X",reg_eax,reg_ecx,reg_edx,reg_ebx);
		//LOG(LOG_PAGING,LOG_NORMAL)("    CS:%04X EIP:%08X SS:%04x SP:%08X",SegValue(cs),reg_eip,SegValue(ss),reg_esp);		return false;
	}
	const auto entry_addr = (table.base << 12) + t_index * 4;
	entry.set(phys_readd(entry_addr));
	if (!entry.p) {
		paging.cr2         = lin_addr;
		cpu.exception.which=EXCEPTION_PF;
		cpu.exception.error=(writing?0x02:0x00) | (((cpu.cpl&cpu.mpl)==0)?0x00:0x04);
		//LOG(LOG_PAGING,LOG_NORMAL)("PageFault at %X type [%x] PREPARE",lin_addr,cpu.exception.error);
		//LOG(LOG_PAGING,LOG_NORMAL)("    EAX:%04X ECX:%04X EDX:%04X EBX:%04X",reg_eax,reg_ecx,reg_edx,reg_ebx);
		//LOG(LOG_PAGING,LOG_NORMAL)("    CS:%04X EIP:%08X SS:%04x SP:%08X",SegValue(cs),reg_eip,SegValue(ss),reg_esp);		return false;
	}
	return true;
}

// check if a user-level memory access would trigger a privilege page fault
static inline bool InitPage_CheckUseraccess(uint32_t u1,uint32_t u2) {
	switch (CPU_ArchitectureType) {
	case ArchitectureType::Mixed:
	case ArchitectureType::Intel386Slow:
	case ArchitectureType::Intel386Fast:
	default:
		return ((u1)==0) && ((u2)==0);
	case ArchitectureType::Intel486OldSlow:
	case ArchitectureType::Intel486NewSlow:
	case ArchitectureType::Pentium:
	case ArchitectureType::PentiumMmx:
		return ((u1)==0) || ((u2)==0);
	}
}


class InitPageHandler final : public PageHandler {
public:
	InitPageHandler() {
		flags=PFLAG_INIT|PFLAG_NOCODE;
	}
	uint8_t readb(PhysPt addr) override
	{
		const auto needs_reset = InitPage(addr, false);
		const auto val = mem_readb(addr);
		InitPageUpdateLink(needs_reset, addr);
		return val;
	}
	uint16_t readw(PhysPt addr) override
	{
		const auto needs_reset = InitPage(addr, false);
		const auto val = mem_readw(addr);
		InitPageUpdateLink(needs_reset, addr);
		return val;
	}
	uint32_t readd(PhysPt addr) override
	{
		const auto needs_reset = InitPage(addr, false);
		const auto val = mem_readd(addr);
		InitPageUpdateLink(needs_reset, addr);
		return val;
	}
	uint64_t readq(PhysPt addr) override
	{
		const auto needs_reset = InitPage(addr, false);
		const auto val         = mem_readq(addr);
		InitPageUpdateLink(needs_reset, addr);
		return val;
	}
	void writeb(PhysPt addr, uint8_t val) override
	{
		const auto needs_reset = InitPage(addr, true);
		mem_writeb(addr, val);
		InitPageUpdateLink(needs_reset,addr);
	}
	void writew(PhysPt addr, uint16_t val) override
	{
		const auto needs_reset = InitPage(addr, true);
		mem_writew(addr, val);
		InitPageUpdateLink(needs_reset,addr);
	}
	void writed(PhysPt addr, uint32_t val) override
	{
		const auto needs_reset = InitPage(addr, true);
		mem_writed(addr, val);
		InitPageUpdateLink(needs_reset,addr);
	}
	void writeq(PhysPt addr, uint64_t val) override
	{
		const auto needs_reset = InitPage(addr, true);
		mem_writeq(addr, val);
		InitPageUpdateLink(needs_reset, addr);
	}
	bool readb_checked(PhysPt addr, uint8_t* val) override
	{
		if (InitPageCheckOnly(addr,false)) {
			*val=mem_readb(addr);
			return false;
		} else return true;
	}
	bool readw_checked(PhysPt addr, uint16_t *val) override
	{
		if (InitPageCheckOnly(addr,false)){
			*val=mem_readw(addr);
			return false;
		} else return true;
	}
	bool readd_checked(PhysPt addr, uint32_t *val) override
	{
		if (InitPageCheckOnly(addr,false)) {
			*val=mem_readd(addr);
			return false;
		} else return true;
	}
	bool readq_checked(PhysPt addr, uint64_t* val) override
	{
		if (InitPageCheckOnly(addr, false)) {
			*val = mem_readq(addr);
			return false;
		} else {
			return true;
		}
	}
	bool writeb_checked(PhysPt addr, uint8_t val) override
	{
		if (InitPageCheckOnly(addr,true)) {
			mem_writeb(addr,val);
			return false;
		} else return true;
	}
	bool writew_checked(PhysPt addr, uint16_t val) override
	{
		if (InitPageCheckOnly(addr,true)) {
			mem_writew(addr,val);
			return false;
		} else return true;
	}
	bool writed_checked(PhysPt addr, uint32_t val) override
	{
		if (InitPageCheckOnly(addr,true)) {
			mem_writed(addr,val);
			return false;
		} else return true;
	}
	bool writeq_checked(PhysPt addr, uint64_t val) override
	{
		if (InitPageCheckOnly(addr, true)) {
			mem_writeq(addr, val);
			return false;
		} else {
			return true;
		}
	}
	uint32_t InitPage(uint32_t lin_addr, bool writing)
	{
		const auto lin_page = lin_addr >> 12;
		uint32_t phys_page;
		if (paging.enabled) {
			X86PageEntry table;
			X86PageEntry entry;
			InitPageCheckPresence(lin_addr,writing,table,entry);

			// 0: no action
			// 1: can (but currently does not) fail a user-level access privilege check
			// 2: can (but currently does not) fail a write privilege check
			// 3: fails a privilege check
			int priv_check=0;
			if (InitPage_CheckUseraccess(entry.us, table.us)) {
				if (USERWRITE_PROHIBITED) priv_check=3;
				else {
					switch (CPU_ArchitectureType) {
					case ArchitectureType::Mixed:
					case ArchitectureType::Intel386Fast:
					default:
//						priv_check=0;	// default
						break;
					case ArchitectureType::Intel386Slow:
					case ArchitectureType::Intel486OldSlow:
					case ArchitectureType::Intel486NewSlow:
					case ArchitectureType::Pentium:
					case ArchitectureType::PentiumMmx:
						priv_check=1;
						break;
					}
				}
			}
			if ((entry.wr == 0) || (table.wr == 0)) {
				// page is write-protected for user mode
				if (priv_check==0) {
					switch (CPU_ArchitectureType) {
					case ArchitectureType::Mixed:
					case ArchitectureType::Intel386Fast:
					default:
//						priv_check=0;	// default
						break;
					case ArchitectureType::Intel386Slow:
					case ArchitectureType::Intel486OldSlow:
					case ArchitectureType::Intel486NewSlow:
					case ArchitectureType::Pentium:
					case ArchitectureType::PentiumMmx:
						priv_check=2;
						break;
					}
				}
				// check if actually failing the write-protected check
				if (writing && USERWRITE_PROHIBITED) priv_check=3;
			}
			if (priv_check==3) {
				LOG(LOG_PAGING, LOG_NORMAL)
				("Page access denied: cpl=%i, %x:%x:%x:%x",
				 static_cast<int>(cpu.cpl),
				 entry.us,
				 table.us,
				 entry.wr,
				 table.wr);
				PAGING_PageFault(lin_addr,
				                 (table.base << 12) +
				                         (lin_page & 0x3ff) * 4,
				                 0x05 | (writing ? 0x02 : 0x00));
				LOG(LOG_PAGING,LOG_NORMAL)("PageFault at %X type [%x] PREPARE",lin_addr,cpu.exception.error);
				priv_check = 0;
			}

			if (!table.a) {
				table.a = 1; // set page table accessed
				phys_writed((paging.base.page << 12) +
				                    (lin_page >> 10) * 4,
				            table.get());
			}
			if (!entry.a || !entry.d) {
				entry.a = 1; // set page accessed

				// page is dirty if we're writing to it, or if we're reading but the
				// page will be fully linked so we can't track later writes
				if (writing || (priv_check == 0)) {
					entry.d = 1; // mark page as dirty
				}

				phys_writed((table.base << 12) +
				                    (lin_page & 0x3ff) * 4,
				            entry.get());
			}

			phys_page = entry.base;

			// now see how the page should be linked best, if we need to catch privilege
			// checks later on it should be linked as read-only page
			if (priv_check==0) {
				// if reading we could link the page as read-only to later cacth writes,
				// will slow down pretty much but allows catching all dirty events
				PAGING_LinkPage(lin_page,phys_page);
			} else {
				if (priv_check==1) {
					PAGING_LinkPage(lin_page,phys_page);
					return 1;
				} else if (writing) {
					PageHandler * handler=MEM_GetPageHandler(phys_page);
					PAGING_LinkPage(lin_page,phys_page);
					if (!(handler->flags & PFLAG_READABLE)) return 1;
					if (!(handler->flags & PFLAG_WRITEABLE)) return 1;
					if (get_tlb_read(lin_addr)!=get_tlb_write(lin_addr)) return 1;
					if (phys_page>1) return phys_page;
					else return 1;
				} else {
					PAGING_LinkPage_ReadOnly(lin_page,phys_page);
				}
			}
		} else {
			if (lin_page<LINK_START) phys_page=paging.firstmb[lin_page];
			else phys_page=lin_page;
			PAGING_LinkPage(lin_page,phys_page);
		}
		return 0;
	}
	bool InitPageCheckOnly(uint32_t lin_addr,bool writing) {
		const auto lin_page=lin_addr >> 12;
		if (paging.enabled) {
			X86PageEntry table;
			X86PageEntry entry;
			if (!InitPageCheckPresence_CheckOnly(lin_addr,writing,table,entry)) return false;

			if (!USERWRITE_PROHIBITED) return true;

			if (InitPage_CheckUseraccess(entry.us, table.us) ||
			    (((entry.wr == 0) || (table.wr == 0)) && writing)) {
				LOG(LOG_PAGING, LOG_NORMAL)
				("Page access denied: cpl=%i, %x:%x:%x:%x",
				 static_cast<int>(cpu.cpl),
				 entry.us,
				 table.us,
				 entry.wr,
				 table.wr);
				paging.cr2=lin_addr;
				cpu.exception.which=EXCEPTION_PF;
				cpu.exception.error=0x05 | (writing?0x02:0x00);
				LOG(LOG_PAGING,LOG_NORMAL)("PageFault at %X type [%x] PREPARE",lin_addr,cpu.exception.error);
				//LOG(LOG_PAGING,LOG_NORMAL)("    EAX:%04X ECX:%04X EDX:%04X EBX:%04X",reg_eax,reg_ecx,reg_edx,reg_ebx);
				//LOG(LOG_PAGING,LOG_NORMAL)("    CS:%04X EIP:%08X SS:%04x SP:%08X",SegValue(cs),reg_eip,SegValue(ss),reg_esp);
				return false;
			}
		} else {
			uint32_t phys_page;
			if (lin_page<LINK_START) phys_page=paging.firstmb[lin_page];
			else phys_page=lin_page;
			PAGING_LinkPage(lin_page,phys_page);
		}
		return true;
	}
	void InitPageForced(uint32_t lin_addr) {
		const auto lin_page=lin_addr >> 12;
		uint32_t phys_page;
		if (paging.enabled) {
			X86PageEntry table;
			X86PageEntry entry;
			InitPageCheckPresence(lin_addr,false,table,entry);

			if (!table.a) {
				table.a = 1; // Set access
				phys_writed((paging.base.page << 12) +
				                    (lin_page >> 10) * 4,
				            table.get());
			}
			if (!entry.a) {
				entry.a = 1; // Set access
				phys_writed((table.base << 12) +
				                    (lin_page & 0x3ff) * 4,
				            entry.get());
			}
			phys_page = entry.base;
			// maybe use read-only page here if possible
		} else {
			if (lin_page<LINK_START) phys_page=paging.firstmb[lin_page];
			else phys_page=lin_page;
		}
		PAGING_LinkPage(lin_page,phys_page);
	}

	static inline void InitPageUpdateLink(Bitu relink,PhysPt addr) {
		if (relink==0) return;
		if (paging.links.used) {
			if (paging.links.entries[paging.links.used-1]==(addr>>12)) {
				paging.links.used--;
				PAGING_UnlinkPages(addr>>12,1);
			}
		}
		if (relink>1) PAGING_LinkPage_ReadOnly(addr>>12,relink);
	}
};

class InitPageUserROHandler final : public PageHandler {
public:
	InitPageUserROHandler() {
		flags=PFLAG_INIT|PFLAG_NOCODE;
	}
	void writeb(PhysPt addr, uint8_t val) override
	{
		InitPage(addr, val);
		host_writeb(get_tlb_read(addr) + addr, val);
	}
	void writew(PhysPt addr, uint16_t val) override
	{
		InitPage(addr, val);
		host_writew(get_tlb_read(addr) + addr, val);
	}
	void writed(PhysPt addr, uint32_t val) override
	{
		InitPage(addr, val);
		host_writed(get_tlb_read(addr) + addr, val);
	}
	bool writeb_checked(PhysPt addr, uint8_t val) override
	{
		const auto writecode = InitPageCheckOnly(addr, val);
		if (writecode) {
			HostPt tlb_addr;
			if (writecode>1) tlb_addr=get_tlb_read(addr);
			else tlb_addr=get_tlb_write(addr);
			host_writeb(tlb_addr + addr, val);
			return false;
		}
		return true;
	}
	bool writew_checked(PhysPt addr, uint16_t val) override
	{
		const auto writecode = InitPageCheckOnly(addr, val);
		if (writecode) {
			HostPt tlb_addr;
			if (writecode>1) tlb_addr=get_tlb_read(addr);
			else tlb_addr=get_tlb_write(addr);
			host_writew(tlb_addr + addr, val);
			return false;
		}
		return true;
	}
	bool writed_checked(PhysPt addr, uint32_t val) override
	{
		const auto writecode = InitPageCheckOnly(addr, val);
		if (writecode) {
			HostPt tlb_addr;
			if (writecode>1) tlb_addr=get_tlb_read(addr);
			else tlb_addr=get_tlb_write(addr);
			host_writed(tlb_addr + addr, val);
			return false;
		}
		return true;
	}
	void InitPage(uint32_t lin_addr, [[maybe_unused]] uint32_t val) {
		const auto lin_page=lin_addr >> 12;
		uint32_t phys_page;
		if (paging.enabled) {
			if (!USERWRITE_PROHIBITED) return;

			X86PageEntry table;
			X86PageEntry entry;
			InitPageCheckPresence(lin_addr,true,table,entry);

			LOG(LOG_PAGING, LOG_NORMAL)
			("Page access denied: cpl=%i, %x:%x:%x:%x",
			 static_cast<int>(cpu.cpl),
			 entry.us,
			 table.us,
			 entry.wr,
			 table.wr);
			PAGING_PageFault(lin_addr,
			                 (table.base << 12) + (lin_page & 0x3ff) * 4,
			                 0x07);

			if (!table.a) {
				table.a = 1; // Set access
				phys_writed((paging.base.page << 12) +
				                    (lin_page >> 10) * 4,
				            table.get());
			}
			if ((!entry.a) || (!entry.d)) {
				entry.a = 1; // Set access
				entry.d = 1; // Set dirty
				phys_writed((table.base << 12) +
				                    (lin_page & 0x3ff) * 4,
				            entry.get());
			}
			phys_page = entry.base;
			PAGING_LinkPage(lin_page,phys_page);
		} else {
			if (lin_page<LINK_START) phys_page=paging.firstmb[lin_page];
			else phys_page=lin_page;
			PAGING_LinkPage(lin_page,phys_page);
		}
	}
	uint32_t InitPageCheckOnly(uint32_t lin_addr, [[maybe_unused]] uint32_t val) {
		const auto lin_page=lin_addr >> 12;
		if (paging.enabled) {
			if (!USERWRITE_PROHIBITED) return 2;

			X86PageEntry table;
			X86PageEntry entry;
			if (!InitPageCheckPresence_CheckOnly(lin_addr,true,table,entry)) return 0;

			if (InitPage_CheckUseraccess(entry.us, table.us) ||
			    (((entry.wr == 0) || (table.wr == 0)))) {
				LOG(LOG_PAGING, LOG_NORMAL)
				("Page access denied: cpl=%i, %x:%x:%x:%x",
				 static_cast<int>(cpu.cpl),
				 entry.us,
				 table.us,
				 entry.wr,
				 table.wr);
				paging.cr2=lin_addr;
				cpu.exception.which=EXCEPTION_PF;
				cpu.exception.error=0x07;
				//LOG(LOG_PAGING,LOG_NORMAL)("PageFault at %X type [%x] PREPARE RO",lin_addr,cpu.exception.error);
				//LOG(LOG_PAGING,LOG_NORMAL)("    EAX:%04X ECX:%04X EDX:%04X EBX:%04X",reg_eax,reg_ecx,reg_edx,reg_ebx);
				//LOG(LOG_PAGING,LOG_NORMAL)("    CS:%04X EIP:%08X SS:%04x SP:%08X",SegValue(cs),reg_eip,SegValue(ss),reg_esp);				return 0;
			}
			PAGING_LinkPage(lin_page, entry.base);
		} else {
			uint32_t phys_page;
			if (lin_page<LINK_START) phys_page=paging.firstmb[lin_page];
			else phys_page=lin_page;
			PAGING_LinkPage(lin_page,phys_page);
		}
		return 1;
	}
	void InitPageForced(uint32_t lin_addr) {
		const auto lin_page=lin_addr >> 12;
		uint32_t phys_page;
		if (paging.enabled) {
			X86PageEntry table;
			X86PageEntry entry;
			InitPageCheckPresence(lin_addr,true,table,entry);

			if (!table.a) {
				table.a = 1; // Set access
				phys_writed((paging.base.page << 12) +
				                    (lin_page >> 10) * 4,
				            table.get());
			}
			if (!entry.a) {
				entry.a = 1; // Set access
				phys_writed((table.base << 12) +
				                    (lin_page & 0x3ff) * 4,
				            entry.get());
			}
			phys_page = entry.base;
		} else {
			if (lin_page<LINK_START) phys_page=paging.firstmb[lin_page];
			else phys_page=lin_page;
		}
		PAGING_LinkPage(lin_page,phys_page);
	}
};


bool PAGING_MakePhysPage(Bitu& page) {
	// page is the linear address on entry
	if (paging.enabled) {
		// check the page directory entry for this address
		X86PageEntry dir_entry;
		dir_entry.set(phys_readd(GetPageDirectoryEntryAddr(page<<12)));
		if (!dir_entry.p) return false;

		// check the page table entry
		X86PageEntry tbl_entry;
		tbl_entry.set(phys_readd(GetPageTableEntryAddr(page<<12, dir_entry)));
		if (!tbl_entry.p) return false;

		// return it
		page = tbl_entry.base;
	} else {
		if (page<LINK_START) page=paging.firstmb[page];
		//Else keep it the same
	}
	return true;
}

static InitPageHandler dyncore_init_page_handler;
static InitPageUserROHandler dyncore_init_page_handler_userro;
static NewInitPageHandler normalcore_init_page_handler;
static ExceptionPageHandler normalcore_exception_handler;
static PageFoilHandler normalcore_foiling_handler;
static PageHandler* init_page_handler;

Bitu PAGING_GetDirBase()
{
	return paging.cr3;
}

bool PAGING_ForcePageInit(Bitu lin_addr) {
	PageHandler * handler=get_tlb_readhandler(lin_addr);
	if (handler==&dyncore_init_page_handler) {
		dyncore_init_page_handler.InitPageForced(lin_addr);
		return true;
	} else if (handler==&dyncore_init_page_handler_userro) {
		PAGING_UnlinkPages(lin_addr>>12,1);
		dyncore_init_page_handler_userro.InitPageForced(lin_addr);
		return true;
	}
	return false;
}

#if defined(USE_FULL_TLB)
void PAGING_InitTLB()
{
	for (auto i=0;i<TLB_SIZE;i++) {
		paging.tlb.read[i]=nullptr;
		paging.tlb.write[i]=nullptr;
		paging.tlb.readhandler[i]=init_page_handler;
		paging.tlb.writehandler[i]=init_page_handler;
	}
	paging.links.used=0;
}

void PAGING_ClearTLB()
{
	//LOG_MSG("[%8u] [@%8d] [CLEARTLB] m% 4u, kr%d, krw%d, ur%d", logcnt++, CPU_Cycles,
	//	paging.links.used, paging.kr_links.used, paging.krw_links.used, paging.ur_links.used);

	uint32_t * entries=&paging.links.entries[0];
	for (;paging.links.used>0;paging.links.used--) {
		const auto page=*entries++;
		paging.tlb.read[page]=nullptr;
		paging.tlb.write[page]=nullptr;
		paging.tlb.readhandler[page]=init_page_handler;
		paging.tlb.writehandler[page]=init_page_handler;
	}
	paging.links.used=0;
}

void PAGING_UnlinkPages(Bitu lin_page,Bitu pages) {
	for (;pages>0;pages--) {
		paging.tlb.read[lin_page]=nullptr;
		paging.tlb.write[lin_page]=nullptr;
		paging.tlb.readhandler[lin_page]=init_page_handler;
		paging.tlb.writehandler[lin_page]=init_page_handler;
		lin_page++;
	}
}

void PAGING_MapPage(Bitu lin_page,Bitu phys_page) {
	//LOG_MSG("[%8u] [@%8d] [MAPPAGE] Page: %x - Phys: %x", logcnt++, CPU_Cycles, lin_page, phys_page);

	if (lin_page<LINK_START) {
		paging.firstmb[lin_page]=phys_page;
		paging.tlb.read[lin_page]=nullptr;
		paging.tlb.write[lin_page]=nullptr;
		paging.tlb.readhandler[lin_page]=init_page_handler;
		paging.tlb.writehandler[lin_page]=init_page_handler;
	} else {
		PAGING_LinkPage(lin_page,phys_page);
	}
}

void PAGING_LinkPageNew(Bitu lin_page, Bitu phys_page, Bitu linkmode, bool dirty) {
	Bitu xlat_index = linkmode | (CPU_HAS_WP_FLAG? 8:0) | ((cpu.cpl==3)? 4:0);
	uint8_t outcome = xlat_mapping[xlat_index];

	// get the physpage handler we are going to map 
	PageHandler * handler=MEM_GetPageHandler(phys_page);
	Bitu lin_base=lin_page << 12;

	//static const char* const lnm[] = {"RW ","RE ","EE "}; // debug stuff
	//LOG_MSG("[%8u] [@%8d] [LINKPAGE] Page: %x - Phys: %x - Dirty: %d - Outcome: %s", logcnt++, CPU_Cycles, lin_page, phys_page, dirty, lnm[outcome]);
	//LOG_MSG("MAPPG %s",lnm[outcome]);

	if (lin_page>=TLB_SIZE || phys_page>=TLB_SIZE) [[unlikely]]
		E_Exit("Illegal page");
	if (paging.links.used>=PAGING_LINKS) [[unlikely]] {
		LOG(LOG_PAGING,LOG_NORMAL)("Not enough paging links, resetting cache");
		PAGING_ClearTLB();
	}
	// re-use some of the unused bits in the phys_page variable
	// needed in the exception handler and foiler so they can replace themselves appropriately
	// bit31-30 ACMAP_
	// bit29	dirty
	// these bits are shifted off at the places paging.tlb.phys_page is read
	paging.tlb.phys_page[lin_page]= phys_page | (linkmode<< 30) | (dirty? PHYSPAGE_DITRY:0);
	switch(outcome) {
	case ACMAP_RW:
		// read
		if (handler->flags & PFLAG_READABLE) paging.tlb.read[lin_page] = 
			handler->GetHostReadPt(phys_page)-lin_base;
		else paging.tlb.read[lin_page]=0;
		paging.tlb.readhandler[lin_page]=handler;

		// write
		if (dirty) { // in case it is already dirty we don't need to check
			if (handler->flags & PFLAG_WRITEABLE) paging.tlb.write[lin_page] = 
				handler->GetHostWritePt(phys_page)-lin_base;
			else paging.tlb.write[lin_page]=0;
			paging.tlb.writehandler[lin_page]=handler;
		} else {
			paging.tlb.writehandler[lin_page]= &normalcore_foiling_handler;
			paging.tlb.write[lin_page]=0;
		}
		break;
	case ACMAP_RE:
		// read
		if (handler->flags & PFLAG_READABLE) paging.tlb.read[lin_page] = 
			handler->GetHostReadPt(phys_page)-lin_base;
		else paging.tlb.read[lin_page]=0;
		paging.tlb.readhandler[lin_page]=handler;
		// exception
		paging.tlb.writehandler[lin_page]= &normalcore_exception_handler;
		paging.tlb.write[lin_page]=0;
		break;
	case ACMAP_EE:
		paging.tlb.readhandler[lin_page]= &normalcore_exception_handler;
		paging.tlb.writehandler[lin_page]= &normalcore_exception_handler;
		paging.tlb.read[lin_page]=0;
		paging.tlb.write[lin_page]=0;
		break;
	}

	switch(linkmode) {
	case ACCESS_KR:
		paging.kr_links.entries[paging.kr_links.used++]=lin_page;
		break;
	case ACCESS_KRW:
		paging.krw_links.entries[paging.krw_links.used++]=lin_page;
		break;
	case ACCESS_UR:
		paging.ur_links.entries[paging.ur_links.used++]=lin_page;
		break;
	case ACCESS_URW:	// with this access right everything is possible
						// thus no need to modify it on a us <-> sv switch
		break;
	}
	paging.links.entries[paging.links.used++]=lin_page; // "master table"
}

void PAGING_LinkPage(uint32_t lin_page,uint32_t phys_page) {
	//LOG_MSG("[%8u] [@%8d] [LINKPAGE] Page: %x - Phys: %x", logcnt++, CPU_Cycles, lin_page, phys_page);

	const auto handler=MEM_GetPageHandler(phys_page);
	const auto lin_base=lin_page << 12;
	if (lin_page>=TLB_SIZE || phys_page>=TLB_SIZE) 
		E_Exit("Illegal page");

	if (paging.links.used >= PAGING_LINKS) {
		LOG(LOG_PAGING,LOG_NORMAL)("Not enough paging links, resetting cache");
		PAGING_ClearTLB();
		assert(paging.links.used == 0);
	}

	paging.tlb.phys_page[lin_page]=phys_page;
	if (handler->flags & PFLAG_READABLE) paging.tlb.read[lin_page]=handler->GetHostReadPt(phys_page)-lin_base;
	else paging.tlb.read[lin_page]=nullptr;
	if (handler->flags & PFLAG_WRITEABLE) paging.tlb.write[lin_page]=handler->GetHostWritePt(phys_page)-lin_base;
	else paging.tlb.write[lin_page]=nullptr;

	paging.links.entries[paging.links.used++]=lin_page;
	paging.tlb.readhandler[lin_page]=handler;
	paging.tlb.writehandler[lin_page]=handler;
}

void PAGING_LinkPage_ReadOnly(uint32_t lin_page,uint32_t phys_page) {
	//LOG_MSG("[%8u] [@%8d] [LINKPAGERO] Page: %x - Phys: %x", logcnt++, CPU_Cycles, lin_page, phys_page);

	const auto handler=MEM_GetPageHandler(phys_page);
	const auto lin_base=lin_page << 12;
	if (lin_page>=TLB_SIZE || phys_page>=TLB_SIZE) 
		E_Exit("Illegal page");

	if (paging.links.used >= PAGING_LINKS) {
		LOG(LOG_PAGING,LOG_NORMAL)("Not enough paging links, resetting cache");
		PAGING_ClearTLB();
		assert(paging.links.used == 0);
	}

	paging.tlb.phys_page[lin_page]=phys_page;
	if (handler->flags & PFLAG_READABLE) paging.tlb.read[lin_page]=handler->GetHostReadPt(phys_page)-lin_base;
	else paging.tlb.read[lin_page]=nullptr;
	paging.tlb.write[lin_page]=nullptr;

	paging.links.entries[paging.links.used++]=lin_page;
	paging.tlb.readhandler[lin_page]=handler;
	paging.tlb.writehandler[lin_page]=&dyncore_init_page_handler_userro;
}

#else

static inline void InitTLBInt(tlb_entry *bank) {
 	for (Bitu i=0;i<TLB_SIZE;i++) {
		bank[i].read=0;
		bank[i].write=0;
		bank[i].readhandler=&init_page_handler;
		bank[i].writehandler=&init_page_handler;
 	}
}

void PAGING_InitTLBBank(tlb_entry **bank) {
	*bank = (tlb_entry *)malloc(sizeof(tlb_entry)*TLB_SIZE);
	if(!*bank) E_Exit("Out of Memory");
	InitTLBInt(*bank);
}

void PAGING_InitTLB(void) {
	//DBP: Performance improvement
	static_assert(offsetof(PagingBlock, tlb.write) == offsetof(PagingBlock, tlb.read) + sizeof(paging.tlb.read), "PAGING: inconsistent paging block layout");
	memset(paging.tlb.read, 0, sizeof(paging.tlb.read) + sizeof(paging.tlb.write));
	for (Bitu i=0;i<TLB_SIZE;i++) paging.tlb.readhandler[i]=init_page_handler;
	for (Bitu i=0;i<TLB_SIZE;i++) paging.tlb.writehandler[i]=init_page_handler;
	paging.ur_links.used=0;
	paging.krw_links.used=0;
	paging.kr_links.used=0;
	paging.links.used=0;
}

void PAGING_ClearTLB()
{
	uint32_t* entries = &paging.links.entries[0];
	for (;paging.links.used>0;paging.links.used--) {
		Bitu page=*entries++;
		tlb_entry *entry = get_tlb_entry(page<<12);
		entry->read=0;
		entry->write=0;
		entry->readhandler=init_page_handler;
		entry->writehandler=init_page_handler;
	}
	paging.ur_links.used=0;
	paging.krw_links.used=0;
	paging.kr_links.used=0;
	paging.links.used=0;
}

void PAGING_UnlinkPages(Bitu lin_page,Bitu pages) {
	for (;pages>0;pages--) {
		tlb_entry *entry = get_tlb_entry(lin_page<<12);
		entry->read=0;
		entry->write=0;
		entry->readhandler=init_page_handler;
		entry->writehandler=init_page_handler;
		lin_page++;
	}
}

void PAGING_MapPage(Bitu lin_page,Bitu phys_page) {
	if (lin_page<LINK_START) {
		paging.firstmb[lin_page]=phys_page;
		paging.tlbh[lin_page].read=0;
		paging.tlbh[lin_page].write=0;
		paging.tlbh[lin_page].readhandler=init_page_handler;
		paging.tlbh[lin_page].writehandler=init_page_handler;
	} else {
		PAGING_LinkPage(lin_page,phys_page);
	}
}

void PAGING_LinkPage(uint32_t lin_page, uint32_t phys_page)
{
	PageHandler* handler = MEM_GetPageHandler(phys_page);
	Bitu lin_base=lin_page << 12;
	if (lin_page>=(TLB_SIZE*(TLB_BANKS+1)) || phys_page>=(TLB_SIZE*(TLB_BANKS+1))) 
		E_Exit("Illegal page");

	if (paging.links.used>=PAGING_LINKS) {
		LOG(LOG_PAGING,LOG_NORMAL)("Not enough paging links, resetting cache");
		PAGING_ClearTLB();
	}

	tlb_entry *entry = get_tlb_entry(lin_base);
	entry->phys_page=phys_page;
	if (handler->flags & PFLAG_READABLE) entry->read=handler->GetHostReadPt(phys_page)-lin_base;
	else entry->read=0;
	if (handler->flags & PFLAG_WRITEABLE) entry->write=handler->GetHostWritePt(phys_page)-lin_base;
	else entry->write=0;

 	paging.links.entries[paging.links.used++]=lin_page;
	entry->readhandler=handler;
	entry->writehandler=handler;
}

void PAGING_LinkPage_ReadOnly(uint32_t lin_page, uint32_t phys_page)
{
	PageHandler* handler = MEM_GetPageHandler(phys_page);
	Bitu lin_base=lin_page << 12;
	if (lin_page>=(TLB_SIZE*(TLB_BANKS+1)) || phys_page>=(TLB_SIZE*(TLB_BANKS+1))) 
		E_Exit("Illegal page");

	if (paging.links.used>=PAGING_LINKS) {
		LOG(LOG_PAGING,LOG_NORMAL)("Not enough paging links, resetting cache");
		PAGING_ClearTLB();
	}

	tlb_entry *entry = get_tlb_entry(lin_base);
	entry->phys_page=phys_page;
	if (handler->flags & PFLAG_READABLE) entry->read=handler->GetHostReadPt(phys_page)-lin_base;
	else entry->read=0;
	entry->write=0;

 	paging.links.entries[paging.links.used++]=lin_page;
	entry->readhandler=handler;
	entry->writehandler=&dyncore_init_page_handler_userro;
}

#endif


void PAGING_SetDirBase(Bitu cr3) {
	assert(cr3 <= UINT32_MAX);
	paging.cr3=static_cast<uint32_t>(cr3);
	
	paging.base.page=static_cast<uint32_t>(cr3 >> 12);
	paging.base.addr=static_cast<PhysPt>(cr3 & ~4095);
//	LOG(LOG_PAGING,LOG_NORMAL)("CR3:%X Base %X",cr3,paging.base.page);
	if (paging.enabled) {
		PAGING_ClearTLB();
	}
}

void PAGING_ChangedWP(void) {
	if (paging.enabled)
		PAGING_ClearTLB();
}

// parameter is the new cpl mode
void PAGING_SwitchCPL(bool isUser) {
	if (!paging.krw_links.used && !paging.kr_links.used && !paging.ur_links.used) return;
	//LOG_MSG("SWCPL u%d kr%d, krw%d, ur%d",
	//	isUser, paging.kr_links.used, paging.krw_links.used, paging.ur_links.used);
	
	// this function is worth optimizing
	// some of this cold be pre-stored?

	// krw - same for WP1 and WP0
	if (isUser) {
		// sv -> us: rw -> ee 
		for(Bitu i = 0; i < paging.krw_links.used; i++) {
			Bitu tlb_index = paging.krw_links.entries[i];
			paging.tlb.readhandler[tlb_index] = &normalcore_exception_handler;
			paging.tlb.writehandler[tlb_index] = &normalcore_exception_handler;
			paging.tlb.read[tlb_index] = 0;
			paging.tlb.write[tlb_index] = 0;
		}
	} else {
		// us -> sv: ee -> rw
		for(Bitu i = 0; i < paging.krw_links.used; i++) {
			Bitu tlb_index = paging.krw_links.entries[i];
			Bitu phys_page = paging.tlb.phys_page[tlb_index];
			Bitu lin_base = tlb_index << 12;
			bool dirty = (phys_page & PHYSPAGE_DITRY)? true:false;
			phys_page &= PHYSPAGE_ADDR;
			PageHandler* handler = MEM_GetPageHandler(phys_page);
			
			// map read handler
			paging.tlb.readhandler[tlb_index] = handler;
			if (handler->flags&PFLAG_READABLE)
				paging.tlb.read[tlb_index] = handler->GetHostReadPt(phys_page)-lin_base;
			else paging.tlb.read[tlb_index] = 0;
			
			// map write handler
			if (dirty) {
				paging.tlb.writehandler[tlb_index] = handler;
				if (handler->flags&PFLAG_WRITEABLE)
					paging.tlb.write[tlb_index] = handler->GetHostWritePt(phys_page)-lin_base;
				else paging.tlb.write[tlb_index] = 0;
			} else {
				paging.tlb.writehandler[tlb_index] = &normalcore_foiling_handler;
				paging.tlb.write[tlb_index] = 0;
			}
		}
	}
	
	if (CPU_HAS_WP_FLAG) [[unlikely]] {
		// ur: no change with WP=1
		// kr
		if (isUser) {
			// sv -> us: re -> ee 
			for(Bitu i = 0; i < paging.kr_links.used; i++) {
				Bitu tlb_index = paging.kr_links.entries[i];
				paging.tlb.readhandler[tlb_index] = &normalcore_exception_handler;
				paging.tlb.read[tlb_index] = 0;
			}
		} else {
			// us -> sv: ee -> re
			for(Bitu i = 0; i < paging.kr_links.used; i++) {
				Bitu tlb_index = paging.kr_links.entries[i];
				Bitu lin_base = tlb_index << 12;
				Bitu phys_page = paging.tlb.phys_page[tlb_index] & PHYSPAGE_ADDR;
				PageHandler* handler = MEM_GetPageHandler(phys_page);

				paging.tlb.readhandler[tlb_index] = handler;
				if (handler->flags&PFLAG_READABLE)
					paging.tlb.read[tlb_index] = handler->GetHostReadPt(phys_page)-lin_base;
				else paging.tlb.read[tlb_index] = 0;
			}
		}
	} else { // WP=0
		// ur
		if (isUser) {
			// sv -> us: rw -> re 
			for(Bitu i = 0; i < paging.ur_links.used; i++) {
				Bitu tlb_index = paging.ur_links.entries[i];
				paging.tlb.writehandler[tlb_index] = &normalcore_exception_handler;
				paging.tlb.write[tlb_index] = 0;
			}
		} else {
			// us -> sv: re -> rw
			for(Bitu i = 0; i < paging.ur_links.used; i++) {
				Bitu tlb_index = paging.ur_links.entries[i];
				Bitu phys_page = paging.tlb.phys_page[tlb_index];
				bool dirty = (phys_page & PHYSPAGE_DITRY)? true:false;
				phys_page &= PHYSPAGE_ADDR;
				PageHandler* handler = MEM_GetPageHandler(phys_page);

				if (dirty) {
					Bitu lin_base = tlb_index << 12;
					paging.tlb.writehandler[tlb_index] = handler;
					if (handler->flags&PFLAG_WRITEABLE)
						paging.tlb.write[tlb_index] = handler->GetHostWritePt(phys_page)-lin_base;
					else paging.tlb.write[tlb_index] = 0;
				} else {
					paging.tlb.writehandler[tlb_index] = &normalcore_foiling_handler;
					paging.tlb.write[tlb_index] = 0;
				}
			}
		}
	}
}


void PAGING_Enable(bool enabled) {
	/* If paging is disabled, we work from a default paging table */
	if (paging.enabled==enabled) return;
	paging.enabled=enabled;
	if (enabled) {
		if (cpudecoder == CPU_Core_Simple_Run) {
			// LOG_MSG("CPU core simple won't
			// run this game,switching to normal");
			cpudecoder = CPU_Core_Normal_Run;
			CPU_CycleLeft += CPU_Cycles;
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

static void PAGING_ShutDown(Section* /*sec*/) {
	init_page_handler = NULL;
	paging_prevent_exception_jump = false;
}

void PAGING_Init(Section * sec) {
	//logcnt = 0;
	sec->AddDestroyFunction(&PAGING_ShutDown);

	Bitu i;

	// log
	LOG(LOG_PAGING,LOG_NORMAL)("Initializing paging system (CPU linear -> physical mapping system)");

	PAGING_OnChangeCore();
	//paging_prevent_exception_jump = false;
	//const char* core = static_cast<Section_prop *>(control->GetSection("cpu"))->Get_string("core");
	//init_page_handler = ((core[0] == 'a' || core[0] == 'd') ? (PageHandler*)&dyncore_init_page_handler : (PageHandler*)&normalcore_init_page_handler);
	//dosbox_allow_nonrecursive_page_fault = !(core[0] == 'a' || core[0] == 'd');

	/* Setup default Page Directory, force it to update */
	paging.enabled=false;
	PAGING_InitTLB();
	for (i=0;i<LINK_START;i++) paging.firstmb[i]=i;
	pf_queue.used=0;
}

#include "control.h"
void PAGING_OnChangeCore(void) {
	// Use dynamic core compatible init page handler when core is set to 'dynamic' or 'auto'
	const auto core = static_cast<Section_prop *>(control->GetSection("cpu"))->Get_string("core");
	PageHandler* next_init_page_handler = ((core[0] == 'a' || core[0] == 'd') ? (PageHandler*)&dyncore_init_page_handler : (PageHandler*)&normalcore_init_page_handler);
	PageHandler* prev_init_page_handler = init_page_handler;
	if (prev_init_page_handler == next_init_page_handler) return;

	if (prev_init_page_handler)
	{
		for (Bitu i=0;i<TLB_SIZE;i++)
		{
			if (paging.tlb.readhandler[i]==prev_init_page_handler) paging.tlb.readhandler[i]=next_init_page_handler;
			if (paging.tlb.writehandler[i]==prev_init_page_handler) paging.tlb.writehandler[i]=next_init_page_handler;
		}
	}
	init_page_handler = next_init_page_handler;
}
