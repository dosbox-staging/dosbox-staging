// SPDX-FileCopyrightText:  2023-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "memory.h"

#include <cstring>
#include <memory>

#include "config/setup.h"
#include "cpu/paging.h"
#include "cpu/registers.h"
#include "hardware/pci_bus.h"
#include "hardware/port.h"
#include "misc/support.h"

constexpr auto Megabyte = 1024 * 1024;

constexpr auto PagesPerMegabyte = Megabyte / DosPageSize;

constexpr auto MinMegabytes = static_cast<uint16_t>(1);
constexpr auto MaxMegabytes = static_cast<uint16_t>(PciMemoryBase / Megabyte);

constexpr auto SafeMegabytesDos   = 31;
constexpr auto SafeMegabytesWin95 = 480;
constexpr auto SafeMegabytesWin98 = 512;

static struct MemoryBlock {
	struct page_t {
		uint8_t bytes[DosPageSize] = {};
	};
	std::vector<page_t> pages           = {};
	std::vector<PageHandler*> phandlers = {};
	std::vector<MemHandle> mhandles     = {};
	struct {
		Bitu start_page = 0;
		Bitu end_page   = 0;
		Bitu pages      = 0;

		PageHandler* handler     = {};
		PageHandler* mmiohandler = {};
	} lfb = {};
	struct {
		bool enabled = false;

		uint8_t controlport = 0;
	} a20 = {};
} memory = {};

// Points to the first byte of the first DOS memory page
HostPt MemBase = {};

class IllegalPageHandler final : public PageHandler {
public:
	IllegalPageHandler() {
		flags=PFLAG_INIT|PFLAG_NOCODE;
	}
	uint8_t readb(PhysPt addr) override
	{
#if C_DEBUGGER
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
	void writeb(PhysPt addr, [[maybe_unused]] uint8_t val) override
	{
#if C_DEBUGGER
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
	// Get the starting byte address for the give page
	HostPt GetHostReadPt(const size_t phys_page) override
	{
		assert(phys_page < memory.pages.size());
		return &(memory.pages[phys_page].bytes[0]);
	}
	HostPt GetHostWritePt(const size_t phys_page) override
	{
		return GetHostReadPt(phys_page); // same
	}
};

class ROMPageHandler final : public RAMPageHandler {
public:
	ROMPageHandler() {
		flags=PFLAG_READABLE|PFLAG_HASROM;
	}
	void writeb(PhysPt addr,uint8_t val) override{
		LOG(LOG_CPU, LOG_ERROR)("Write 0x%x to rom at %x", val, addr);
	}
	void writew(PhysPt addr,uint16_t val) override{
		LOG(LOG_CPU, LOG_ERROR)("Write 0x%x to rom at %x", val, addr);
	}
	void writed(PhysPt addr,uint32_t val) override{
		LOG(LOG_CPU, LOG_ERROR)("Write 0x%x to rom at %x", val, addr);
	}
};

uint16_t MEM_GetMinMegabytes()
{
	return MinMegabytes;
}

uint16_t MEM_GetMaxMegabytes()
{
	return MaxMegabytes;
}

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
	if (phys_page < memory.pages.size()) {
		return memory.phandlers[phys_page];
	}
	if (phys_page >= memory.lfb.start_page && phys_page < memory.lfb.end_page) {
		return memory.lfb.handler;
	}

	constexpr uint32_t PagesIn16Mb = 16 * PagesPerMegabyte;

	const auto last_page_in_first_16mb = memory.lfb.start_page + PagesIn16Mb;
	const auto sixteen_pages_beyond_first_16mb = last_page_in_first_16mb + 16u;

	if (phys_page >= last_page_in_first_16mb &&
	    phys_page < sixteen_pages_beyond_first_16mb) {
		return memory.lfb.mmiohandler;
	} else {
		PageHandler* VOODOO_PCI_GetLFBPageHandler(Bitu);
		if (PageHandler* vph = VOODOO_PCI_GetLFBPageHandler(phys_page)) {
			return vph;
		}
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
	auto write = reinterpret_cast<uint8_t *>(data);
	while (size--) {
		*write++=mem_readb_inline(pt++);
	}
}

void MEM_BlockWrite(PhysPt pt, const void *data, size_t size)
{
	auto read = static_cast<const uint8_t *>(data);
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

uint32_t MEM_TotalPages(void)
{
	return check_cast<uint32_t>(memory.pages.size());
}

uint32_t MEM_FreeLargest()
{
	uint32_t size    = 0;
	uint32_t largest = 0;
	size_t   index   = XMS_START;
	while (index < memory.pages.size()) {
		if (!memory.mhandles[index]) {
			++size;
		} else {
			largest = std::max(size, largest);
			size = 0;
		}
		++index;
	}
	largest = std::max(size, largest);
	return largest;
}

uint32_t MEM_FreeTotal()
{
	uint32_t free  = 0;
	size_t   index = XMS_START;
	while (index < memory.pages.size()) {
		if (!memory.mhandles[index]) {
			++free;
		}
		++index;
	}
	return free;
}

uint32_t MEM_AllocatedPages(MemHandle handle) 
{
	uint32_t pages = 0;
	while (handle > 0) {
		++pages;
		assert(pages != 0);
		handle = memory.mhandles[handle];
	}
	return pages;
}

//TODO Maybe some protection for this whole allocation scheme

inline Bitu BestMatch(Bitu size) {
	Bitu index=XMS_START;	
	Bitu first=0;
	Bitu best=0xfffffff;
	Bitu best_first=0;
	while (index < memory.pages.size()) {
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
			while (static_cast<uint32_t>(index) < memory.pages.size() &&
			       !memory.mhandles[index]) {
				index++;
				free++;
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
	// Is A20 already in the requested state?
	if (memory.a20.enabled == enabled) {
		return;
	}
	constexpr uint32_t a20_base_page = Megabyte / DosPageSize;

	const uint32_t phys_base_page = enabled ? a20_base_page : 0;

	for (uint8_t page = 0; page < 16; ++page) {
		PAGING_MapPage(a20_base_page + page, phys_base_page + page);
	}
	memory.a20.enabled = enabled;
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

uint64_t mem_unalignedreadq(PhysPt address)
{
	uint64_t ret = 0;
	for (int i = 0; i < 8; ++i) {
		ret |= static_cast<uint64_t>(mem_readb_inline(address + i))
		    << (i * 8);
	}
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

void mem_unalignedwriteq(PhysPt address, uint64_t val)
{
	for (int i = 0; i < 8; ++i) {
		mem_writeb_inline(address + i, static_cast<uint8_t>(val));
		val >>= 8;
	}
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

bool mem_unalignedreadq_checked(PhysPt address, uint64_t* val)
{
	uint8_t rval[8];
	for (int i = 0; i < 8; ++i) {
		if (mem_readb_checked(address + i, &rval[i])) {
			return true;
		}
	}

	*val = 0;
	for (int i = 0; i < 8; ++i) {
		*val |= static_cast<uint64_t>(rval[i]) << (i * 8);
	}
	return false;
}

bool mem_unalignedwritew_checked(PhysPt address, uint16_t val)
{
	if (mem_writeb_checked(address + 0, (uint8_t)(val & 0xff))) {
		return true;
	}
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

bool mem_unalignedwriteq_checked(PhysPt address, uint64_t val)
{
	for (int i = 0; i < 8; ++i) {
		if (mem_writeb_checked(address + i,
		                       static_cast<uint8_t>(val & 0xff))) {
			return true;
		}
		val >>= 8;
	}
	return false;
}

template <MemOpMode op_mode>
uint8_t mem_readb(const PhysPt address)
{
	return mem_readb_inline<op_mode>(address);
}

template <MemOpMode op_mode>
uint16_t mem_readw(const PhysPt address)
{
	return mem_readw_inline<op_mode>(address);
}

template <MemOpMode op_mode>
uint32_t mem_readd(const PhysPt address)
{
	return mem_readd_inline<op_mode>(address);
}

template <MemOpMode op_mode>
uint64_t mem_readq(PhysPt address)
{
	return mem_readq_inline<op_mode>(address);
}

// Explicit instantiations for mem_readb, mem_readw, and mem_readd
template uint8_t mem_readb<MemOpMode::WithBreakpoints>(const PhysPt address);
template uint8_t mem_readb<MemOpMode::SkipBreakpoints>(const PhysPt address);

template uint16_t mem_readw<MemOpMode::WithBreakpoints>(const PhysPt address);
template uint16_t mem_readw<MemOpMode::SkipBreakpoints>(const PhysPt address);

template uint32_t mem_readd<MemOpMode::WithBreakpoints>(const PhysPt address);
template uint32_t mem_readd<MemOpMode::SkipBreakpoints>(const PhysPt address);

template uint64_t mem_readq<MemOpMode::WithBreakpoints>(const PhysPt address);
template uint64_t mem_readq<MemOpMode::SkipBreakpoints>(const PhysPt address);

void mem_writeb(PhysPt address, uint8_t val)
{
	mem_writeb_inline(address,val);
}

void mem_writew(PhysPt address,uint16_t val) {
	mem_writew_inline(address,val);
}

void mem_writed(PhysPt address,uint32_t val) {
	mem_writed_inline(address,val);
}

void mem_writeq(PhysPt address, uint64_t val)
{
	mem_writeq_inline(address, val);
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

static void check_num_megabytes(const int num_megabytes)
{
	assert(num_megabytes >= MinMegabytes);
	assert(num_megabytes <= MaxMegabytes);

	if (num_megabytes > SafeMegabytesDos) {
		LOG_WARNING("MEMORY: Memory sizes above %d MB aren't recommended for most DOS games",
		            SafeMegabytesDos);
	}
	if (num_megabytes > SafeMegabytesWin95) {
		LOG_WARNING("MEMORY: Memory sizes above %d/%d MB aren't compatible with unpatched Windows 95/98, respectively",
		            SafeMegabytesWin95,
		            SafeMegabytesWin98);
		// Limitation can be lifted with PATCHMEM by Rudolph R. Loew
	}
}

HostPt GetMemBase(void)
{
	return MemBase;
}

class MEMORY {
private:
	IO_ReadHandleObject ReadHandler   = {};
	IO_WriteHandleObject WriteHandler = {};

public:
	MEMORY(Section* sec)
	{
		// Get the users memory size preference
		const auto section       = static_cast<SectionProp*>(sec);
		const auto num_megabytes = section->GetInt("memsize");
		check_num_megabytes(num_megabytes);

		const auto num_pages = num_megabytes * PagesPerMegabyte;

		// Size the actual memory pages
		memory.pages.resize(num_pages);

		// The MemBase is address of the first page's first byte
		MemBase = &(memory.pages[0].bytes[0]);

		LOG_MSG("MEMORY: Using %d DOS memory pages (%u MB) at address: %p",
		        static_cast<int>(memory.pages.size()),
		        num_megabytes,
		        static_cast<void*>(MemBase));

		// Setup the page handlers, defaulting to the RAM handler
		memory.phandlers.clear();
		memory.phandlers.resize(num_pages, &ram_page_handler);

		// Setup the memory handers, defaulting to 0 which means
		// memory-allocation
		memory.mhandles.clear();
		memory.mhandles.resize(num_pages, 0);

		using page_range_t = std::pair<uint16_t, uint16_t>;
		auto install_rom_page_handlers = [&](const page_range_t& page_range) {
			for (auto p = page_range.first; p < page_range.second; ++p) {
				memory.phandlers.at(p) = &rom_page_handler;
			}
		};

		// Setup ROM page handers between 0xc0000-0xc8000
		constexpr page_range_t xt_rom_range = {0xc0, 0xc8};
		install_rom_page_handlers(xt_rom_range);

		// Setup ROM page handlers between 0xf0000-0x100000
		constexpr page_range_t pc_rom_range = {0xf0, 0x100};
		install_rom_page_handlers(pc_rom_range);

		// Setup PCjr Cartridge ROM page handlers between 0xe0000-0xf0000
		if (is_machine_pcjr()) {
			constexpr page_range_t pcjr_rom_range = {0xe0, 0xf0};
			install_rom_page_handlers(pcjr_rom_range);
		}

		// A20 Line - PS/2 system control port A
		WriteHandler.Install(0x92, write_p92, io_width_t::byte);
		ReadHandler.Install(0x92, read_p92, io_width_t::byte);
		InitA20();
	}
};

static std::unique_ptr<MEMORY> memory_module = {};

void MEM_Init(Section* section)
{
	assert(section);
	memory_module = std::make_unique<MEMORY>(section);
}

void MEM_Destroy()
{
	memory_module = {};
}
