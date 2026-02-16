// SPDX-FileCopyrightText:  2023-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dosbox.h"

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <memory>

#include "config/setup.h"
#include "cpu/callback.h"
#include "cpu/cpu.h"
#include "cpu/registers.h"
#include "dos/dos.h"
#include "hardware/memory.h"
#include "hardware/port.h"
#include "ints/bios.h"
#include "misc/support.h"
#include "utils/bitops.h"
#include "utils/checks.h"
#include "utils/math_utils.h"

CHECK_NARROWING();

// ***************************************************************************
// Constants and type definitions
// ***************************************************************************

constexpr uint16_t XmsVersion       = 0x0300; // version 3.00
constexpr uint16_t XmsDriverVersion = 0x0301; // driver version 3.01

// MS-DOS 6.22 defaults to 32 XMS handles, we can provide more without any
// significant cost.
constexpr uint8_t NumXmsHandles = 128;

constexpr auto KilobytesPerPage = MemPageSize / 1024;

enum class Result : uint8_t {
	OK                   = 0x00,
	NotImplemented       = 0x80,
	VDiskDetected        = 0x81, // not needed in DOSBox
	A20LineError         = 0x82,
	GeneralDriverError   = 0x8e, // not needed in DOSBox
	HMA_NOT_EXIST        = 0x90,
	HmaInUse             = 0x91,
	HmaNotBigEnough      = 0x92,
	HmaNotAllocated      = 0x93,
	A20StillEnabled      = 0x94,
	XmsOutOfSpace        = 0xa0,
	XmsOutOfHandles      = 0xa1,
	XmsInvalidHandle     = 0xa2,
	XmsInvalidSrcHandle  = 0xa3,
	XmsInvalidSrcOffset  = 0xa4,
	XmsInvalidDestHandle = 0xa5,
	XmsInvalidDestOffset = 0xa6,
	XmsInvalidLength     = 0xa7,
	XmsInvalidOverlap    = 0xa8, // TODO: add support for this error
	XmsParityError       = 0xa9,
	XmsBlockNotLocked    = 0xaa,
	XmsBlockLocked       = 0xab,
	XmsLockCountOverflow = 0xac,
	XmsLockFailed        = 0xad, // TODO: when should this be reported?
	UmbOnlySmallerBlock  = 0xb0,
	UmbNoBlocksAvailable = 0xb1,
	UmbInvalidSegment    = 0xb2, // TODO: when should this be reported?
};

struct XMS_Block {
	uint32_t size_kb     = 0;
	MemHandle mem_handle = -1;
	// locked blocks should not be quietly moved by the XMS driver
	uint8_t lock_count = 0;
	bool is_free       = true;
};

#ifdef _MSC_VER
#pragma pack (1)
#endif
struct XMS_MemMove {
	uint32_t length = 0;

	uint16_t src_handle = 0;
	union {
		RealPt realpt;
		uint32_t offset;
	} src = {};
	uint16_t dest_handle = 0;
	union {
		RealPt realpt;
		uint32_t offset;
	} dest = {};

} GCC_ATTRIBUTE(packed);
#ifdef _MSC_VER
#pragma pack ()
#endif

// ***************************************************************************
// Variables
// ***************************************************************************

static struct {
	bool enable_global = false;

	uint32_t num_times_enabled = 0;
} a20;

constexpr uint32_t A20MaxTimesEnabled = UINT32_MAX;

static struct {
	// TODO: HMA support for applications is not yet available in the core

	bool is_available = false;

	bool dos_has_control = true;
	bool app_has_control = false;

	uint16_t min_alloc_size = 0;
} hma;

static struct {
	bool is_available = false;
} umb;

static struct {
	bool is_available = false;

	RealPt callback = 0;

	XMS_Block handles[NumXmsHandles] = {};
} xms;

// ***************************************************************************
// Generic helper routines
// ***************************************************************************

static uint32_t get_num_pages(const uint32_t size_kb)
{
	// Number of pages needed for the given memory size in KB
	return size_kb / 4 + ((size_kb & 3) ? 1 : 0);
}

static uint32_t get_mem_free_total_kb()
{
	return static_cast<uint32_t>(MEM_FreeTotal() * KilobytesPerPage);
}

static uint32_t get_mem_free_largest_kb()
{
	return static_cast<uint32_t>(MEM_FreeLargest() * KilobytesPerPage);
}

static uint32_t get_mem_highest_address()
{
	return static_cast<uint32_t>((MEM_TotalPages() * MemPageSize) - 1);
}

static void warn_umb_realloc()
{
	static bool first_time = true;
	if (first_time) {
		first_time = false;
		LOG_WARNING("XMS: UMB realloc not implemented");
	}
}

// ***************************************************************************
// Gate A20 support
// ***************************************************************************

static void a20_enable(const bool enable)
{
	uint8_t val = IO_Read(port_num_fast_a20);
	bit::set_to(val, bit::literals::b1, enable);
	IO_Write(port_num_fast_a20, val);
}

static bool a20_is_enabled()
{
	return bit::is(IO_Read(port_num_fast_a20), bit::literals::b1);
}

static Result a20_local_enable()
{
	// Microsoft HIMEM.SYS appears to set A20 only if the local count is 0
	// at entering this call

	if (a20.num_times_enabled == A20MaxTimesEnabled) {
		// Counter overflow protection

		static bool first_time = true;
		if (first_time) {
			LOG_WARNING("XMS: A20 local count already at maximum");
			first_time = false;
		}

		return Result::A20LineError;
	}

	if (a20.num_times_enabled++ == 0) {
		a20_enable(true);
	}

	return Result::OK;
}

static Result a20_local_disable()
{
	// Microsoft HIMEM.SYS appears to disable A20 only if the local count is
	// 1 at entering this call

	if (a20.num_times_enabled == 0) {
		return Result::A20LineError; // HIMEM.SYS behavior
	}

	if (--a20.num_times_enabled != 0) {
		return Result::A20StillEnabled;
	}

	a20_enable(false);
	return Result::OK;
}

// ***************************************************************************
// XMS support
// ***************************************************************************

static bool xms_is_handle_valid(const uint16_t handle)
{
	return handle && (handle < NumXmsHandles) && !xms.handles[handle].is_free;
}

static Result xms_query_free_memory(uint32_t& largest_kb, uint32_t& total_kb)
{
	// Scan the tree for free memory and find largest free block

	total_kb   = get_mem_free_total_kb();
	largest_kb = get_mem_free_largest_kb();

	return total_kb ? Result::OK : Result::XmsOutOfSpace;
}

static Result xms_allocate_memory(const uint32_t size_kb, uint16_t& handle)
{
	// Find free handle

	uint16_t index = 1;
	while (!xms.handles[index].is_free) {
		if (++index >= NumXmsHandles) {
			return Result::XmsOutOfHandles;
		}
	}

	// Allocate (size is in kb)

	MemHandle mem_handle = -1;
	if (size_kb) {
		constexpr bool sequence = true;
		mem_handle = MEM_AllocatePages(get_num_pages(size_kb), sequence);
		if (!mem_handle) {
			return Result::XmsOutOfSpace;
		}
	} else {
		mem_handle = MEM_GetNextFreePage();
		if (!mem_handle) {
			// Windows 3.1 does this really often
			// LOG_MSG("XMS: Allocate zero pages with no memory left");
		}
	}

	xms.handles[index].is_free    = false;
	xms.handles[index].mem_handle = mem_handle;
	xms.handles[index].lock_count = 0;
	xms.handles[index].size_kb    = size_kb;

	handle = index;
	return Result::OK;
}

static Result xms_free_memory(const uint16_t handle)
{
	if (!xms_is_handle_valid(handle)) {
		return Result::XmsInvalidHandle;
	}
	if (xms.handles[handle].lock_count != 0) {
		return Result::XmsBlockLocked;
	}

	MEM_ReleasePages(xms.handles[handle].mem_handle);

	xms.handles[handle] = XMS_Block();
	return Result::OK;
}

static Result xms_move_memory(const PhysPt bpt)
{
	// TODO: Detect invalid overlaps, report XmsInvalidOverlap

	// Read the block with mem_read's
	const auto length = mem_readd(bpt + offsetof(XMS_MemMove, length));

	// "Length must be even" --Microsoft XMS Spec 3.0
	if (length % 2) {
		return Result::XmsParityError;
	}

	union {
		RealPt realpt;
		uint32_t offset;
	} src, dest;

	const auto src_handle = mem_readw(
		static_cast<PhysPt>(bpt + offsetof(XMS_MemMove, src_handle)));
	src.offset = mem_readd(
		static_cast<PhysPt>(bpt + offsetof(XMS_MemMove, src.offset)));

	const auto dest_handle = mem_readw(
		static_cast<PhysPt>(bpt + offsetof(XMS_MemMove, dest_handle)));
	dest.offset = mem_readd(
		static_cast<PhysPt>(bpt + offsetof(XMS_MemMove, dest.offset)));

	PhysPt srcpt = 0;
	PhysPt destpt = 0;

	if (src_handle) {
		if (!xms_is_handle_valid(src_handle)) {
			return Result::XmsInvalidSrcHandle;
		}
		if (src.offset >= (xms.handles[src_handle].size_kb * 1024U)) {
			return Result::XmsInvalidSrcOffset;
		}
		if (length > xms.handles[src_handle].size_kb * 1024U - src.offset) {
			return Result::XmsInvalidLength;
		}
		srcpt = (static_cast<uint32_t>(xms.handles[src_handle].mem_handle) * MemPageSize) +
		        src.offset;
	} else {
		srcpt = RealToPhysical(src.realpt);

		// Microsoft TEST.C considers it an error to allow real mode
		// pointers + length to extend past the end of the
		// 8086-accessible conventional memory area.
		if ((srcpt + length) > 0x10FFF0u) {
			return Result::XmsInvalidLength;
		}
	}

	if (dest_handle) {
		if (!xms_is_handle_valid(dest_handle)) {
			return Result::XmsInvalidDestHandle;
		}
		if (dest.offset >= (xms.handles[dest_handle].size_kb * 1024U)) {
			return Result::XmsInvalidDestOffset;
		}
		if (length > xms.handles[dest_handle].size_kb * 1024U - dest.offset) {
			return Result::XmsInvalidLength;
		}
		destpt = (static_cast<uint32_t>(xms.handles[dest_handle].mem_handle) * MemPageSize) +
		        dest.offset;
	} else {
		destpt = RealToPhysical(dest.realpt);

		// Microsoft TEST.C considers it an error to allow real mode
		// pointers + length to extend past the end of the
		// 8086-accessible conventional memory area.
		if ((destpt + length) > 0x10FFF0u) {
			return Result::XmsInvalidLength;
		}
	}

	// LOG_MSG("XMS: move src %X dest %X length %X",srcpt,destpt,length);

	// We must enable the A20 gate during this copy; masked A20 would cause
	// memory corruption

	if (length != 0) {
		bool a20_was_enabled = a20_is_enabled();

		++a20.num_times_enabled;
		a20_enable(true);

		mem_memcpy(destpt, srcpt, length);

		--a20.num_times_enabled;
		if (!a20_was_enabled) {
			a20_enable(false);
		}
	}

	return Result::OK;
}

static Result xms_lock_memory(const uint16_t handle, uint32_t& address)
{
	if (!xms_is_handle_valid(handle)) {
		return Result::XmsInvalidHandle;
	}

	if (xms.handles[handle].lock_count >= UINT8_MAX) {
		return Result::XmsLockCountOverflow;
	}

	xms.handles[handle].lock_count++;
	address = static_cast<uint32_t>(xms.handles[handle].mem_handle * MemPageSize);
	return Result::OK;
}

static Result xms_unlock_memory(const uint16_t handle)
{
	if (!xms_is_handle_valid(handle)) {
		return Result::XmsInvalidHandle;
	}

	if (xms.handles[handle].lock_count) {
		xms.handles[handle].lock_count--;
		return Result::OK;
	}

	return Result::XmsBlockNotLocked;
}

static Result xms_get_handle_information(const uint16_t handle, uint8_t& lock_count,
                                         uint8_t& num_free, uint32_t& size_kb)
{
	if (!xms_is_handle_valid(handle)) {
		return Result::XmsInvalidHandle;
	}

	lock_count = xms.handles[handle].lock_count;

	// Find available blocks

	num_free = 0;

	for (uint16_t index = 1; index < NumXmsHandles; ++index) {
		if (xms.handles[index].is_free) {
			++num_free;
		}
	}

	size_kb = xms.handles[handle].size_kb;
	return Result::OK;
}

static Result xms_resize_memory(const uint16_t handle, const uint32_t new_size_kb)
{
	if (!xms_is_handle_valid(handle)) {
		return Result::XmsInvalidHandle;
	}

	// Block has to be unlocked

	if (xms.handles[handle].lock_count > 0) {
		return Result::XmsBlockLocked;
	}

	constexpr bool sequence = true;
	if (MEM_ReAllocatePages(xms.handles[handle].mem_handle,
	                        get_num_pages(new_size_kb),
	                        sequence)) {
		xms.handles[handle].size_kb = new_size_kb;
		return Result::OK;
	}

	return Result::XmsOutOfSpace;
}

static bool xms_multiplex()
{
	switch (reg_ax) {
	case 0x4300: // XMS installed check
		reg_al = 0x80;
		return true;
	case 0x4310: // XMS handler seg:offset
		SegSet16(es, RealSegment(xms.callback));
		reg_bx = RealOffset(xms.callback);
		return true;
	}

	return false;
}

// ***************************************************************************
// Main XMS API handler
// ***************************************************************************

static Bitu XMS_Handler()
{
	assert(xms.is_available);

	Result result = Result::OK;

	auto set_return_value = [](const Result result) {
		reg_bl = static_cast<uint8_t>(result);
		reg_ax = (result == Result::OK) ? 1 : 0;
	};

	auto set_return_value_bl_only_fail = [](const Result result) {
		if (result != Result::OK) {
			reg_bl = static_cast<uint8_t>(result);
		}
		reg_ax = (result == Result::OK) ? 1 : 0;
	};

	switch (reg_ah) {
	case 0x00: // Get XMS Version Number
		reg_ax = XmsVersion;
		reg_bx = XmsDriverVersion;
		reg_dx = hma.is_available ? 1 : 0;
		break;
	case 0x01: // Request High Memory Area
		if (!hma.is_available) {
			set_return_value(Result::HMA_NOT_EXIST);
			break;
		}
		if (hma.app_has_control || hma.dos_has_control) {
			// HMA already controlled by application or DOS
			set_return_value(Result::HmaInUse);
		} else if (reg_dx < hma.min_alloc_size) {
			// Request for a block not big enough
			set_return_value(Result::HmaNotBigEnough);
		} else {
			reg_ax = 1; // HMA allocated succesfully
			LOG_MSG("XMS: HMA allocated by application/TSR");
			hma.app_has_control = true;
		}
		break;
	case 0x02: // Release High Memory Area
		if (!hma.is_available) {
			LOG_WARNING("XMS: Application attempted to free HMA while it does not exist!");
			set_return_value(Result::HMA_NOT_EXIST);
			break;
		}
		if (hma.dos_has_control) {
			LOG_WARNING("XMS: Application attempted to free HMA while DOS kernel occupies it!");
		}

		if (hma.app_has_control) {
			reg_ax = 1; // HMA released succesfully
			LOG_MSG("XMS: HMA freed by application/TSR");
			hma.app_has_control = false;
		} else {
			LOG_WARNING("XMS: Application attempted to free HMA while it is not allocated!");
			set_return_value(Result::HmaNotAllocated);
		}
		break;
	case 0x03: // Global Enable A20
		// This appears to be how Microsoft HIMEM.SYS implements this
		if (!a20.enable_global) {
			result = a20_local_enable();
			if (result == Result::OK) {
				a20.enable_global = true;
			}
		}
		set_return_value(result);
		break;
	case 0x04: // Global Disable A20
		// This appears to be how Microsoft HIMEM.SYS implements this
		if (a20.enable_global) {
			result = a20_local_disable();
			if (result == Result::OK) {
				a20.enable_global = false;
			}
		}
		set_return_value(result);
		break;
	case 0x05: // Local Enable A20
		set_return_value(a20_local_enable());
		break;
	case 0x06: // Local Disable A20
		set_return_value(a20_local_disable());
		break;
	case 0x07: // Query A20
		reg_ax = a20_is_enabled() ? 1 : 0;
		reg_bl = 0;
		break;
	case 0x08: // Query Free Extended Memory
		result = xms_query_free_memory(reg_eax, reg_edx);
		reg_bl = static_cast<uint8_t>(result);
		// Cap sizes for older programs; newer ones use function 0x88
		reg_eax = clamp_to_uint16(reg_eax);
		reg_edx = clamp_to_uint16(reg_edx);
		break;
	case 0x09: // Allocate Extended Memory Block
	{
		uint16_t handle = 0;
		set_return_value(xms_allocate_memory(reg_dx, handle));
		reg_dx = handle;
	} break;
	case 0x0a: // Free Extended Memory Block
		set_return_value(xms_free_memory(reg_dx));
		break;
	case 0x0b: // Move Extended Memory Block
		result = xms_move_memory(SegPhys(ds) + reg_si);
		set_return_value_bl_only_fail(result);
		break;
	case 0x0c: // Lock Extended Memory Block
	{
		uint32_t address = 0;
		result           = xms_lock_memory(reg_dx, address);
		set_return_value(result);
		if (result == Result::OK) {
			// success
			reg_bx = RealOffset(address);
			reg_dx = RealSegment(address);
		}
	} break;
	case 0x0d: // Unlock Extended Memory Block
		set_return_value(xms_unlock_memory(reg_dx));
		break;
	case 0x0e: // Get Handle Information
		result = xms_get_handle_information(reg_dx, reg_bh, reg_bl, reg_edx);
		set_return_value_bl_only_fail(result);
		reg_edx &= 0xffff;
		break;
	case 0x0f: // Reallocate Extended Memory Block
		set_return_value(xms_resize_memory(reg_dx, reg_bx));
		break;
	case 0x10: // Request Upper Memory Block
		if (!umb.is_available) {
			set_return_value(Result::NotImplemented);
			break;
		} else {
			const uint16_t umb_start = dos_infoblock.GetStartOfUMBChain();
			if (umb_start == 0xffff) {
				set_return_value(Result::UmbNoBlocksAvailable);
				reg_dx = 0; // no upper memory available
				break;
			}
			// Save status and linkage of upper UMB chain and link
			// upper memory to the regular MCB chain
			uint8_t umb_flag = dos_infoblock.GetUMBChainState();
			if ((umb_flag & 1) == 0) {
				DOS_LinkUMBsToMemChain(1);
			}
			auto old_memstrat = static_cast<uint8_t>(
			        DOS_GetMemAllocStrategy() & 0xff);
			DOS_SetMemAllocStrategy(0x40); // search in UMBs only

			uint16_t size = reg_dx;
			uint16_t seg  = 0;
			if (DOS_AllocateMemory(&seg, &size)) {
				reg_ax = 1;
				reg_bx = seg;
			} else {
				set_return_value(
				        size == 0 ? Result::UmbNoBlocksAvailable
				                  : Result::UmbOnlySmallerBlock);
				reg_dx = size; // size of largest available UMB
			}

			// Restore status and linkage of upper UMB chain
			uint8_t current_umb_flag = dos_infoblock.GetUMBChainState();
			if ((current_umb_flag & 1) != (umb_flag & 1)) {
				DOS_LinkUMBsToMemChain(umb_flag);
			}
			DOS_SetMemAllocStrategy(old_memstrat);
		}
		break;
	case 0x11: // Release Upper Memory Block
		if (!umb.is_available) {
			set_return_value(Result::NotImplemented);
			break;
		}
		if (dos_infoblock.GetStartOfUMBChain() != 0xffff) {
			if (DOS_FreeMemory(reg_dx)) {
				reg_ax = 1;
				break;
			}
		}
		set_return_value(Result::UmbNoBlocksAvailable);
		break;
	case 0x12: // Realloc Upper Memory Block
		// TODO: implement this!
		warn_umb_realloc();
		set_return_value(Result::NotImplemented);
		break;
	case 0x88: // Query any Free Extended Memory
		result = xms_query_free_memory(reg_eax, reg_edx);
		reg_bl = static_cast<uint8_t>(result);
		// highest known physical memory address
		reg_ecx = get_mem_highest_address();
		break;
	case 0x89: // Allocate any Extended Memory Block
	{
		uint16_t handle = 0;
		set_return_value(xms_allocate_memory(reg_edx, handle));
		reg_dx = handle;
	} break;
	case 0x8e: // Get Extended EMB Handle
	{
		uint8_t free_handles = 0;
		result = xms_get_handle_information(reg_dx, reg_bh, free_handles, reg_edx);
		set_return_value_bl_only_fail(result);
		if (result == Result::OK) {
			reg_cx = free_handles;
		}
	} break;
	case 0x8f: // Realloc any Extended Memory
		set_return_value(xms_resize_memory(reg_dx, reg_ebx));
		break;
	default:
		LOG_ERR("XMS: unknown function %02X", reg_ah);
		set_return_value(Result::NotImplemented);
	}

	return CBRET_NONE;
}

// ***************************************************************************
// Module object
// ***************************************************************************

Bitu GetEMSType(SectionProp& section);

class XMS {
private:
	CALLBACK_HandlerObject callbackhandler;

public:
	XMS(SectionProp& section);
	~XMS();
};

XMS::XMS(SectionProp& section) : callbackhandler{}
{
	umb = {};
	a20 = {};

	if (!section.GetBool("xms")) {
		return;
	}

	// NTS: Disable XMS emulation if CPU type is less than a 286, because
	// extended memory did not exist until the CPU had enough address lines
	// to read past the 1MB mark.
	//
	// The other reason we do this is that there is plenty of software that
	// assumes 286+ instructions if they detect XMS services, including but
	// not limited to:
	//
	//      MSD.EXE Microsoft Diagnostics
	//      Microsoft Windows 3.0
	//
	// Not emulating XMS for 8086/80186 emulation prevents the software
	// from crashing.

	if (CPU_ArchitectureType < ArchitectureType::Intel286) {
		LOG_WARNING("XMS: CPU 80186 or lower lacks address lines needed for XMS, disabling");
		return;
	}

	xms.is_available = true;
	// TODO: read HMA configuration

	BIOS_ZeroExtendedSize(true);
	DOS_AddMultiplexHandler(xms_multiplex);

	// Place hookable callback in writable memory area
	xms.callback = RealMake(static_cast<uint16_t>(DOS_GetMemory(0x1) - 1),
	                        0x10);
	callbackhandler.Install(&XMS_Handler,
	                        CB_HOOKABLE,
	                        RealToPhysical(xms.callback),
	                        "XMS Handler");
	// pseudocode for CB_HOOKABLE:
	//	jump near skip
	//	nop,nop,nop
	//	label skip:
	//	callback XMS_Handler
	//	retf

	for (uint16_t index = 0; index < NumXmsHandles; ++index) {
		xms.handles[index] = XMS_Block();
	}
	xms.handles[0].is_free = false;

	// Set up UMB chain
	umb.is_available         = section.GetBool("umb");
	const bool ems_available = GetEMSType(section) > 0;
	DOS_BuildUMBChain(section.GetBool("umb"), ems_available);

	// TODO: If implementing CP/M compatibility, mirror the JMP
	//       instruction in HMA
}

XMS::~XMS()
{
	// Remove upper memory information
	dos_infoblock.SetStartOfUMBChain(0xffff);
	if (umb.is_available) {
		dos_infoblock.SetUMBChainState(0);
		umb.is_available = false;
	}

	if (!xms.is_available) {
		return;
	}

	// Undo biosclearing
	BIOS_ZeroExtendedSize(false);

	// Remove Multiplex
	DOS_DeleteMultiplexHandler(xms_multiplex);

	// Free used memory while skipping the 0 handle
	for (uint16_t index = 1; index < NumXmsHandles; ++index) {
		xms.handles[index].lock_count = 0;
		if (!xms.handles[index].is_free) {
			xms_free_memory(index);
		}
	}

	xms.is_available = false;
}

// ***************************************************************************
// Lifecycle
// ***************************************************************************

static std::unique_ptr<XMS> xms_module = {};

void XMS_Init(SectionProp& section)
{
	xms_module = std::make_unique<XMS>(section);
}

void XMS_Destroy()
{
	xms_module = {};
}

