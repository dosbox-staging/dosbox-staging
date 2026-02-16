// SPDX-FileCopyrightText:  2021-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "cpu/paging.h"

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <memory>

#include "cpu/cpu.h"
#include "cpu/registers.h"
#include "debugger/debugger.h"
#include "hardware/memory.h"
#include "lazyflags.h"

#define LINK_TOTAL		(64*1024)

#define USERWRITE_PROHIBITED			((cpu.cpl&cpu.mpl)==3)


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

#define PF_QUEUESIZE 16
static struct {
	uint8_t used = 0; // keeps track of number of entries
	PF_Entry entries[PF_QUEUESIZE];
} pf_queue;

static Bits PageFaultCore()
{
	CPU_CycleLeft+=CPU_Cycles;
	CPU_Cycles=1;
	Bits ret=CPU_Core_Full_Run();
	CPU_CycleLeft+=CPU_Cycles;
	if (ret<0) E_Exit("Got a dosbox close machine in pagefault core?");
	if (ret) 
		return ret;
	if (!pf_queue.used) E_Exit("PF Core without PF");
	PF_Entry * entry=&pf_queue.entries[pf_queue.used-1];
	X86PageEntry pentry;
	pentry.set(phys_readd(entry->page_addr));
	if (pentry.p && entry->cs == SegValue(cs) && entry->eip == reg_eip) {
		cpu.mpl = entry->mpl;
		return -1;
	}
	return 0;
}

bool first=false;

void PAGING_PageFault(PhysPt lin_addr,uint32_t page_addr,uint32_t faultcode) {
	/* Save the state of the cpu cores */
	const auto old_lflags = lflags;
	const auto old_cpudecoder=cpudecoder;
	cpudecoder=&PageFaultCore;
	paging.cr2=lin_addr;
	PF_Entry * entry=&pf_queue.entries[pf_queue.used++];
	LOG(LOG_PAGING, LOG_NORMAL)("PageFault at %X type [%x] queue %u", lin_addr, faultcode, pf_queue.used);
	// LOG_MSG("EAX:%04X ECX:%04X EDX:%04X EBX:%04X",reg_eax,reg_ecx,reg_edx,reg_ebx);
	// LOG_MSG("CS:%04X EIP:%08X SS:%04x SP:%08X",SegValue(cs),reg_eip,SegValue(ss),reg_esp);
	entry->cs=SegValue(cs);
	entry->eip=reg_eip;
	entry->page_addr=page_addr;
	entry->mpl=cpu.mpl;
	cpu.mpl=3;

	CPU_Exception(EXCEPTION_PF,faultcode);
	DOSBOX_RunMachine();
	pf_queue.used--;
	LOG(LOG_PAGING, LOG_NORMAL)("Left PageFault for %x queue %u", lin_addr, pf_queue.used);
	lflags = old_lflags;
	cpudecoder=old_cpudecoder;
//	LOG_MSG("SS:%04x SP:%08X",SegValue(ss),reg_esp);
}

static inline void InitPageUpdateLink(uint32_t relink,PhysPt addr) {
	if (relink==0) return;
	if (paging.links.used) {
		if (paging.links.entries[paging.links.used-1]==(addr>>12)) {
			paging.links.used--;
			PAGING_UnlinkPages(addr>>12,1);
		}
	}
	if (relink>1) PAGING_LinkPage_ReadOnly(addr>>12,relink);
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
		if (!table.p) {
			E_Exit("Pagefault didn't correct table");
		}
	}
	const auto entry_addr = (table.base << 12) + t_index * 4;
	entry.set(phys_readd(entry_addr));
	if (!entry.p) {
		//		LOG(LOG_PAGING,LOG_NORMAL)("NP Page");
		PAGING_PageFault(lin_addr,entry_addr,
			(writing?0x02:0x00) | (((cpu.cpl&cpu.mpl)==0)?0x00:0x04));
		entry.set(phys_readd(entry_addr));
		if (!entry.p) {
			E_Exit("Pagefault didn't correct page");
		}
	}
}
			
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
		return false;
	}
	const auto entry_addr = (table.base << 12) + t_index * 4;
	entry.set(phys_readd(entry_addr));
	if (!entry.p) {
		paging.cr2         = lin_addr;
		cpu.exception.which=EXCEPTION_PF;
		cpu.exception.error=(writing?0x02:0x00) | (((cpu.cpl&cpu.mpl)==0)?0x00:0x04);
		return false;
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
		return (u1 == 0) && (u2 == 0);
	case ArchitectureType::Intel486OldSlow:
	case ArchitectureType::Intel486NewSlow:
	case ArchitectureType::Pentium:
	case ArchitectureType::PentiumMmx:
		return (u1 == 0) || (u2 == 0);
	}
}

class InitPageHandler : public PageHandler {
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
};

class InitPageUserROHandler : public PageHandler {
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
			    ((entry.wr == 0) || (table.wr == 0))) {
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
				return 0;
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

bool PAGING_MakePhysPage(Bitu & page) {
	assert(page <= UINT32_MAX);
	if (paging.enabled) {
		uint32_t d_index=page >> 10;
		uint32_t t_index=page & 0x3ff;
		X86PageEntry table;
		table.set(phys_readd((paging.base.page << 12) + d_index * 4));
		if (!table.p) {
			return false;
		}
		X86PageEntry entry;
		entry.set(phys_readd((table.base << 12) + t_index * 4));
		if (!entry.p) {
			return false;
		}
		page = entry.base;
	} else {
		if (page<LINK_START) page=paging.firstmb[page];
		//Else keep it the same
	}
	return true;
}

static InitPageHandler init_page_handler;
static InitPageUserROHandler init_page_handler_userro;

Bitu PAGING_GetDirBase()
{
	return paging.cr3;
}

bool PAGING_ForcePageInit(Bitu lin_addr) {
	PageHandler * handler=get_tlb_readhandler(lin_addr);
	if (handler==&init_page_handler) {
		init_page_handler.InitPageForced(lin_addr);
		return true;
	} else if (handler==&init_page_handler_userro) {
		PAGING_UnlinkPages(lin_addr>>12,1);
		init_page_handler_userro.InitPageForced(lin_addr);
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
		paging.tlb.readhandler[i]=&init_page_handler;
		paging.tlb.writehandler[i]=&init_page_handler;
	}
	paging.links.used=0;
}

void PAGING_ClearTLB()
{
	uint32_t * entries=&paging.links.entries[0];
	for (;paging.links.used>0;paging.links.used--) {
		const auto page=*entries++;
		paging.tlb.read[page]=nullptr;
		paging.tlb.write[page]=nullptr;
		paging.tlb.readhandler[page]=&init_page_handler;
		paging.tlb.writehandler[page]=&init_page_handler;
	}
	paging.links.used=0;
}

void PAGING_UnlinkPages(Bitu lin_page,Bitu pages) {
	for (;pages>0;pages--) {
		paging.tlb.read[lin_page]=nullptr;
		paging.tlb.write[lin_page]=nullptr;
		paging.tlb.readhandler[lin_page]=&init_page_handler;
		paging.tlb.writehandler[lin_page]=&init_page_handler;
		lin_page++;
	}
}

void PAGING_MapPage(Bitu lin_page,Bitu phys_page) {
	if (lin_page<LINK_START) {
		paging.firstmb[lin_page]=phys_page;
		paging.tlb.read[lin_page]=nullptr;
		paging.tlb.write[lin_page]=nullptr;
		paging.tlb.readhandler[lin_page]=&init_page_handler;
		paging.tlb.writehandler[lin_page]=&init_page_handler;
	} else {
		PAGING_LinkPage(lin_page,phys_page);
	}
}

void PAGING_LinkPage(uint32_t lin_page,uint32_t phys_page) {
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
	paging.tlb.writehandler[lin_page]=&init_page_handler_userro;
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

void PAGING_InitTLB()
{
	InitTLBInt(paging.tlbh);
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
		entry->readhandler=&init_page_handler;
		entry->writehandler=&init_page_handler;
	}
	paging.links.used=0;
}

void PAGING_UnlinkPages(Bitu lin_page,Bitu pages) {
	for (;pages>0;pages--) {
		tlb_entry *entry = get_tlb_entry(lin_page<<12);
		entry->read=0;
		entry->write=0;
		entry->readhandler=&init_page_handler;
		entry->writehandler=&init_page_handler;
		lin_page++;
	}
}

void PAGING_MapPage(Bitu lin_page,Bitu phys_page) {
	if (lin_page<LINK_START) {
		paging.firstmb[lin_page]=phys_page;
		paging.tlbh[lin_page].read=0;
		paging.tlbh[lin_page].write=0;
		paging.tlbh[lin_page].readhandler=&init_page_handler;
		paging.tlbh[lin_page].writehandler=&init_page_handler;
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
	entry->writehandler=&init_page_handler_userro;
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

bool PAGING_Enabled()
{
	return paging.enabled;
}

class PAGING final {
public:
	PAGING()
	{
		/* Setup default Page Directory, force it to update */
		paging.enabled=false;
		PAGING_InitTLB();
		for (auto i=0;i<LINK_START;i++) {
			paging.firstmb[i]=i;
		}
		pf_queue.used=0;
	}
};

static std::unique_ptr<PAGING> paging_instance = {};

void PAGING_Init()
{
	paging_instance = std::make_unique<PAGING>();
}
