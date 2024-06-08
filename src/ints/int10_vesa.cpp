/*
 *  Copyright (C) 2019-2024  The DOSBox Staging Team
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

#include "int10.h"

#include <cstddef>
#include <cstring>

#include "callback.h"
#include "dos_inc.h"
#include "inout.h"
#include "math_utils.h"
#include "mem.h"
#include "pci_bus.h"
#include "regs.h"
#include "string_utils.h"
#include "version.h"

#define VESA_SUCCESS          0x00
#define VESA_FAIL             0x01
#define VESA_HW_UNSUPPORTED   0x02
#define VESA_MODE_UNSUPPORTED 0x03
// internal definition to pass to the caller
#define VESA_UNIMPLEMENTED    0xFF

static struct {
	callback_number_t rmWindow  = 0;
	callback_number_t pmStart   = 0;
	callback_number_t pmWindow  = 0;
	callback_number_t pmPalette = 0;
} callback;

static const std::string string_oem         = "S3 Incorporated. Trio64";
static const std::string string_vendorname  = DOSBOX_TEAM;
static const std::string string_productname = DOSBOX_NAME;
static const std::string string_productrev  = DOSBOX_VERSION;

#ifdef _MSC_VER
#pragma pack (1)
#endif
struct MODE_INFO{
	uint16_t ModeAttributes;
	uint8_t WinAAttributes;
	uint8_t WinBAttributes;
	uint16_t WinGranularity;
	uint16_t WinSize;
	uint16_t WinASegment;
	uint16_t WinBSegment;
	uint32_t WinFuncPtr;
	uint16_t BytesPerScanLine;
	uint16_t XResolution;
	uint16_t YResolution;
	uint8_t XCharSize;
	uint8_t YCharSize;
	uint8_t NumberOfPlanes;
	uint8_t BitsPerPixel;
	uint8_t NumberOfBanks;
	uint8_t MemoryModel;
	uint8_t BankSize;
	uint8_t NumberOfImagePages;
	uint8_t Reserved_page;
	uint8_t RedMaskSize;
	uint8_t RedMaskPos;
	uint8_t GreenMaskSize;
	uint8_t GreenMaskPos;
	uint8_t BlueMaskSize;
	uint8_t BlueMaskPos;
	uint8_t ReservedMaskSize;
	uint8_t ReservedMaskPos;
	uint8_t DirectColorModeInfo;
	uint32_t PhysBasePtr;
	uint32_t OffScreenMemOffset;
	uint16_t OffScreenMemSize;
	uint8_t Reserved[206];
} GCC_ATTRIBUTE(packed);
#ifdef _MSC_VER
#pragma pack()
#endif

uint8_t VESA_GetSVGAInformation(const uint16_t segment, const uint16_t offset)
{
	// Fill buffer with VESA information
	PhysPt buffer = PhysicalMake(segment, offset);
	const auto id = mem_readd(buffer);

	const auto vbe2 = (((id == 0x56424532) || (id == 0x32454256)) &&
	                   !int10.vesa_oldvbe);

	const auto vbe_bufsize = vbe2 ? 0x200 : 0x100;
	for (auto i = 0; i < vbe_bufsize; i++) {
		mem_writeb(buffer + i, 0);
	}

	// Identification
	auto vesa_string = "VESA";
	MEM_BlockWrite(buffer, static_cast<const void*>(vesa_string), 4);

	// VESA version
	constexpr auto vesa_v1_2 = 0x0102;
	constexpr auto vesa_v2_0 = 0x0200;

	const auto vesa_version  = int10.vesa_oldvbe ? vesa_v1_2 : vesa_v2_0;
	mem_writew(buffer + 0x04, vesa_version);

	if (vbe2) {
		uint16_t vbe2_pos = 256 + offset;

		auto write_string = [&](const std::string& str, uint16_t& pos) {
			for (const char& c : str) {
				real_writeb(segment, pos++, c);
			}
			real_writeb(segment, pos++, 0);
		};

		// OEM string
		mem_writed(buffer + 0x06, RealMake(segment, vbe2_pos));
		write_string(string_oem, vbe2_pos);

		// VBE 2 software revision
		mem_writew(buffer + 0x14, 0x200);

		// Vendor name
		mem_writed(buffer + 0x16, RealMake(segment, vbe2_pos));
		write_string(string_vendorname, vbe2_pos);

		// Product name
		mem_writed(buffer + 0x1a, RealMake(segment, vbe2_pos));
		write_string(string_productname, vbe2_pos);

		// Product revision
		mem_writed(buffer + 0x1e, RealMake(segment, vbe2_pos));
		write_string(string_productrev, vbe2_pos);
	} else {
		// OEM string
		mem_writed(buffer + 0x06, int10.rom.oemstring);
	}

	// Capabilities and flags
	mem_writed(buffer + 0x0a, 0x0);

	// VESA mode list
	mem_writed(buffer + 0x0e, int10.rom.vesa_modes);

	// Memory size in 64KB blocks
	mem_writew(buffer + 0x12, (uint16_t)(vga.vmemsize / (64 * 1024)));

	return VESA_SUCCESS;
}

// Ref: https://android.googlesource.com/kernel/msm/+/android-msm-bullhead-3.10-marshmallow-dr/Documentation/svga.txt
//
// "2.7 (09-Apr-96) Accepted all VESA modes in range 0x100 to 0x7ff, because some
// cards use very strange mode numbers.'
bool VESA_IsVesaMode(const uint16_t bios_mode_number)
{
	return (bios_mode_number >= MinVesaBiosModeNumber &&
	        bios_mode_number <= MaxVesaBiosModeNumber);
}

// Build-engine games have problem timing some non-standard,
// low-resolution, 8-bit, linear-framebuffer VESA modes
static bool on_build_engine_denylist(const VideoModeBlock &m)
{
	if (m.type != M_LIN8)
		return false;

	const bool is_denied = (m.swidth == 320 && m.sheight == 240) ||
	                       (m.swidth == 400 && m.sheight == 300) ||
	                       (m.swidth == 512 && m.sheight == 384);
	return is_denied;
}

static bool can_triple_buffer_8bit(const VideoModeBlock &m)
{
	assert(m.type == M_LIN8);
	const auto padding = m.htotal;
	const uint32_t needed_bytes = (m.swidth + padding) * (m.vtotal + padding) * 3;
	return vga.vmemsize >= needed_bytes;
}

uint8_t VESA_GetSVGAModeInformation(uint16_t mode,uint16_t seg,uint16_t off) {
	MODE_INFO minfo;
	memset(&minfo,0,sizeof(minfo));
	PhysPt buf=PhysicalMake(seg,off);
	int modePageSize = 0;
	uint8_t modeAttributes;

	mode&=0x3fff;	// vbe2 compatible, ignore lfb and keep screen content bits
	if (mode < MinVesaBiosModeNumber) {
		return 0x01;
	}
	if (svga.accepts_mode) {
		if (!svga.accepts_mode(mode)) return 0x01;
	}

	// Find the requested mode in our table of VGA modes
	bool found_mode = false;
	assert(ModeList_VGA.size());
	auto mblock = ModeList_VGA.back();
	for (auto &v : ModeList_VGA) {
		if (v.mode == mode) {
			mblock = v;
			found_mode = true;
			break;
		}
	}
	if (!found_mode)
		return VESA_FAIL;

	// Was the found mode VESA 2.0 but the user requested VESA 1.2?
	if (mblock.mode >= vesa_2_0_modes_start && int10.vesa_oldvbe)
		return VESA_FAIL;

	// assume mode is OK until proven otherwise
	bool ok_per_mode_pref = true;
	switch (mblock.type) {
	case M_LIN4:
		modePageSize = mblock.sheight * mblock.swidth/8;
		minfo.BytesPerScanLine = host_to_le16(mblock.swidth / 8);
		minfo.NumberOfPlanes = 0x4;
		minfo.BitsPerPixel = 4u;
		minfo.MemoryModel = 3u; // ega planar mode
		modeAttributes = 0x1b; // Color, graphics, no linear buffer
		break;
	case M_LIN8:
		modePageSize = mblock.sheight * mblock.swidth;
		minfo.BytesPerScanLine = host_to_le16(mblock.swidth);
		minfo.NumberOfPlanes = 0x1;
		minfo.BitsPerPixel = 8u;
		minfo.MemoryModel = 4u; // packed pixel
		modeAttributes = 0x1b; // Color, graphics

		if (int10.vesa_mode_preference == VesaModePref::Compatible) {
			ok_per_mode_pref = can_triple_buffer_8bit(mblock) &&
			                   !on_build_engine_denylist(mblock);
		}
		if (!int10.vesa_nolfb && ok_per_mode_pref)
			modeAttributes |= 0x80; // linear framebuffer
		break;
	case M_LIN15:
		modePageSize = mblock.sheight * mblock.swidth*2;
		minfo.BytesPerScanLine = host_to_le16(mblock.swidth * 2);
		minfo.NumberOfPlanes = 0x1;
		minfo.BitsPerPixel = 15u;
		minfo.MemoryModel = 6u; // HiColour
		minfo.RedMaskSize = 5u;
		minfo.RedMaskPos = 10u;
		minfo.GreenMaskSize = 5u;
		minfo.GreenMaskPos = 5u;
		minfo.BlueMaskSize = 5u;
		minfo.BlueMaskPos = 0u;
		minfo.ReservedMaskSize = 0x01;
		minfo.ReservedMaskPos = 0x0f;
		modeAttributes = 0x1b; // Color, graphics
		if (!int10.vesa_nolfb)
			modeAttributes |= 0x80; // linear framebuffer
		break;
	case M_LIN16:
		modePageSize = mblock.sheight * mblock.swidth*2;
		minfo.BytesPerScanLine = host_to_le16(mblock.swidth * 2);
		minfo.NumberOfPlanes = 0x1;
		minfo.BitsPerPixel = 16u;
		minfo.MemoryModel = 6u; // HiColour
		minfo.RedMaskSize = 5u;
		minfo.RedMaskPos = 11u;
		minfo.GreenMaskSize = 6u;
		minfo.GreenMaskPos = 5u;
		minfo.BlueMaskSize = 5u;
		minfo.BlueMaskPos = 0u;
		modeAttributes = 0x1b; // Color, graphics
		if (!int10.vesa_nolfb)
			modeAttributes |= 0x80; // linear framebuffer
		break;
	case M_LIN24:
		// Mode 0x212 has 128 extra bytes per scan line for
		// compatibility with Windows 640x480 24-bit S3 Trio drivers
		if (mode == 0x212) {
			modePageSize = mblock.sheight * (mblock.swidth * 3 + 128);
			minfo.BytesPerScanLine = host_to_le16(mblock.swidth * 3 + 128);
		} else {
			modePageSize = mblock.sheight * (mblock.swidth * 3);
			minfo.BytesPerScanLine = host_to_le16(mblock.swidth * 3);
		}
		minfo.NumberOfPlanes = 0x1u;
		minfo.BitsPerPixel = 24u;
		minfo.MemoryModel = 6u; // HiColour
		minfo.RedMaskSize = 8u;
		minfo.RedMaskPos = 0x10;
		minfo.GreenMaskSize = 0x8;
		minfo.GreenMaskPos = 0x8;
		minfo.BlueMaskSize = 0x8;
		minfo.BlueMaskPos = 0x0;
		modeAttributes = 0x1b; // Color, graphics
		if (!int10.vesa_nolfb)
			modeAttributes |= 0x80; // linear framebuffer
		break;
	case M_LIN32:
		modePageSize = mblock.sheight * mblock.swidth*4;
		minfo.BytesPerScanLine = host_to_le16(mblock.swidth * 4);
		minfo.NumberOfPlanes = 0x1u;
		minfo.BitsPerPixel = 32u;
		minfo.MemoryModel = 6u; // HiColour
		minfo.RedMaskSize = 8u;
		minfo.RedMaskPos = 0x10;
		minfo.GreenMaskSize = 0x8;
		minfo.GreenMaskPos = 0x8;
		minfo.BlueMaskSize = 0x8;
		minfo.BlueMaskPos = 0x0;
		minfo.ReservedMaskSize = 0x8;
		minfo.ReservedMaskPos = 0x18;
		modeAttributes = 0x1b; // Color, graphics
		if (!int10.vesa_nolfb)
			modeAttributes |= 0x80; // linear framebuffer
		break;
	case M_TEXT:
		modePageSize = 0;
		minfo.BytesPerScanLine = host_to_le16(mblock.twidth * 2);
		minfo.NumberOfPlanes = 0x4;
		minfo.BitsPerPixel = 4u;
		minfo.MemoryModel = 0u; // text
		modeAttributes = 0x0f; // Color, text, bios output
		break;
	default:
		return VESA_FAIL;
	}
	if (modePageSize & 0xFFFF) {
		// It is documented that many applications assume 64k-aligned page sizes
		// VBETEST is one of them
		modePageSize += 0x10000;
		modePageSize &= ~0xFFFF;
	}
	int modePages = 0;
	if (modePageSize > static_cast<int>(vga.vmemsize)) {
		// mode not supported by current hardware configuration
		modeAttributes &= ~0x1;
	} else if (modePageSize) {
		modePages = (vga.vmemsize / modePageSize)-1;
	}	
	assert(modePages <= UINT8_MAX);
	minfo.NumberOfImagePages = static_cast<uint8_t>(modePages);
	minfo.ModeAttributes = host_to_le16(modeAttributes);
	minfo.WinAAttributes = 0x7; // Exists/readable/writable

	if (mblock.type==M_TEXT) {
		minfo.WinGranularity = host_to_le16(32u);
		minfo.WinSize = host_to_le16(32u);
		minfo.WinASegment = host_to_le16(0xb800);
		minfo.XResolution = host_to_le16(mblock.twidth);
		minfo.YResolution = host_to_le16(mblock.theight);
	} else {
		minfo.WinGranularity = host_to_le16(64u);
		minfo.WinSize = host_to_le16(64u);
		minfo.WinASegment = host_to_le16(0xa000);
		minfo.XResolution = host_to_le16(mblock.swidth);
		minfo.YResolution = host_to_le16(mblock.sheight);
	}
	minfo.WinFuncPtr = host_to_le32(int10.rom.set_window);
	minfo.NumberOfBanks = 0x1;
	minfo.Reserved_page = 0x1;
	minfo.XCharSize = mblock.cwidth;
	minfo.YCharSize = mblock.cheight;
	if (!int10.vesa_nolfb)
		minfo.PhysBasePtr = host_to_le32(PciGfxLfbBase);

	MEM_BlockWrite(buf,&minfo,sizeof(MODE_INFO));
	return VESA_SUCCESS;
}

uint8_t VESA_SetSVGAMode(uint16_t mode) {
	if (INT10_SetVideoMode(mode)) {
		int10.vesa_setmode=mode&0x7fff;
		return VESA_SUCCESS;
	}
	return VESA_FAIL;
}

uint8_t VESA_GetSVGAMode(uint16_t & mode) {
	if (int10.vesa_setmode!=0xffff) mode=int10.vesa_setmode;
	else mode=CurMode->mode;
	return VESA_SUCCESS;
}

uint8_t VESA_SetCPUWindow(uint8_t window,uint8_t address) {
	if (window) return VESA_FAIL;
	if ((uint32_t)(address)*64*1024 < vga.vmemsize) {
		IO_Write(0x3d4,0x6a);
		IO_Write(0x3d5, address);
		return VESA_SUCCESS;
	} else return VESA_FAIL;
}

uint8_t VESA_GetCPUWindow(uint8_t window,uint16_t & address) {
	if (window) return VESA_FAIL;
	IO_Write(0x3d4,0x6a);
	address=IO_Read(0x3d5);
	return VESA_SUCCESS;
}


uint8_t VESA_SetPalette(PhysPt data,Bitu index,Bitu count,bool wait) {
//Structure is (vesa 3.0 doc): blue,green,red,alignment
	uint8_t r,g,b;
	if (index>255) return VESA_FAIL;
	if (index+count>256) return VESA_FAIL;

	// Wait for retrace if requested
	if (wait) CALLBACK_RunRealFar(RealSegment(int10.rom.wait_retrace),RealOffset(int10.rom.wait_retrace));

	IO_Write(0x3c8,(uint8_t)index);
	while (count) {
		b = mem_readb(data++);
		g = mem_readb(data++);
		r = mem_readb(data++);
		data++;
		IO_Write(0x3c9,r);
		IO_Write(0x3c9,g);
		IO_Write(0x3c9,b);
		count--;
	}
	return VESA_SUCCESS;
}


uint8_t VESA_GetPalette(PhysPt data,Bitu index,Bitu count) {
	uint8_t r,g,b;
	if (index>255) return VESA_FAIL;
	if (index+count>256) return VESA_FAIL;
	IO_Write(0x3c7,(uint8_t)index);
	while (count) {
		r = IO_Read(0x3c9);
		g = IO_Read(0x3c9);
		b = IO_Read(0x3c9);
		mem_writeb(data++,b);
		mem_writeb(data++,g);
		mem_writeb(data++,r);
		data++;
		count--;
	}
	return VESA_SUCCESS;
}

uint8_t VESA_ScanLineLength(uint8_t subcall,
                            uint16_t val,
                            uint16_t &bytes,
                            uint16_t &pixels,
                            uint16_t &lines)
{
	// offset register: virtual scanline length
	auto new_offset = static_cast<int>(vga.config.scan_len);
	auto screen_height = CurMode->sheight;
	auto usable_vmem_bytes = vga.vmemsize;
	uint8_t bits_per_pixel = 0;
	uint8_t bytes_per_offset = 8;
	bool align_to_nearest_4th_pixel = false;

	// LOG_MSG("VESA_ScanLineLength: s-%lux%lu, t-%lux%lu, c-%lux%lu,
	//         p-tot=%lu p-start=%lu p-len=%lu, h-tol=%lu h-len=%lu",
	//         CurMode->swidth, CurMode->sheight, CurMode->twidth,
	//         CurMode->theight, CurMode->cwidth, CurMode->cheight,
	//         CurMode->ptotal,CurMode->pstart,CurMode->plength,
	//         CurMode->htotal,CurMode->vtotal);

	switch (CurMode->type) {
	case M_TEXT:
		// In text mode we only have a 32 KB window to operate on
		usable_vmem_bytes = 32 * 1024;
		screen_height = CurMode->theight;
		bytes_per_offset = 4;   // 2 characters + 2 attributes
		bits_per_pixel = 4;
		break;
	case M_LIN4:
		bytes_per_offset = 2;
		bits_per_pixel = 4;
		usable_vmem_bytes /= 4; // planar mode
		break;
	case M_LIN8: bits_per_pixel = 8; break;
	case M_LIN15:
	case M_LIN16: bits_per_pixel = 16; break;
	case M_LIN24:
		align_to_nearest_4th_pixel = true;
		bits_per_pixel = 24;
		break;
	case M_LIN32: bits_per_pixel = 32; break;
	default:
		return VESA_MODE_UNSUPPORTED;
	}
	constexpr int gcd = 8 * 8; // greatest common dividsor

	// The 'bytes' and 'pixels' return values
	// (reference-assigns) are multiplied up from the offset length, so here
	// we reverse those calculations using UINT16_MAX as of the offset
	// length to determine its maximum possible value that won't cause the
	// bytes or pixels calculations to overflow.
	const auto max_offset = std::min(UINT16_MAX / bytes_per_offset,
	                                 UINT16_MAX * bits_per_pixel / gcd);

	switch (subcall) {
	case 0x00: // set scan length in pixels
		new_offset = val * bits_per_pixel / gcd;
		if (align_to_nearest_4th_pixel)
			new_offset -= (new_offset % 3);

		if (new_offset > max_offset)
			return VESA_HW_UNSUPPORTED; // scanline too long
		vga.config.scan_len = check_cast<uint16_t>(new_offset);
		VGA_CheckScanLength();
		break;

	case 0x01: // get current scanline length
		// implemented at the end of this function
		break;

	case 0x02: // set scan length in bytes
		new_offset = ceil_udivide(val, bytes_per_offset);
		if (new_offset > max_offset)
			return VESA_HW_UNSUPPORTED; // scanline too long
		vga.config.scan_len = check_cast<uint16_t>(new_offset);
		VGA_CheckScanLength();
		break;

	case 0x03: // get maximum scan line length
		// the smaller of either the hardware maximum scanline length or
		// the limit to get full y resolution of this mode
		new_offset = max_offset;
		if ((new_offset * bytes_per_offset * screen_height) >
		    static_cast<int>(usable_vmem_bytes))
			new_offset = usable_vmem_bytes / (bytes_per_offset * screen_height);
		break;

	default:
		return VESA_UNIMPLEMENTED;
	}

	// set up the return values
	bytes = check_cast<uint16_t>(new_offset * bytes_per_offset);
	pixels = check_cast<uint16_t>(new_offset * gcd / bits_per_pixel);
	if (!bytes) {
		// return failure on division by zero
		// some real VESA BIOS implementations may crash here
		return VESA_FAIL;
	}
	const auto supported_lines = usable_vmem_bytes / bytes;
	const auto gap = supported_lines % screen_height;
	constexpr uint8_t max_gap = 8;
	lines = gap < max_gap ? screen_height
	                      : check_cast<uint16_t>(supported_lines);

	if (CurMode->type==M_TEXT)
		lines *= CurMode->cheight;

	// LOG_MSG("VESA_ScanLineLength subcall=%u, val=%u, vga.config.scan_len = %lu,"
	//         "pixels = %u, bytes = %u, lines = %u, supported_lines = %d ",
	//         subcall, val, vga.config.scan_len, pixels,
	//         bytes, lines, supported_lines);

	return VESA_SUCCESS;
}

uint8_t VESA_SetDisplayStart(uint16_t x,uint16_t y,bool wait) {
	uint8_t panning_factor = 1;
	uint8_t bits_per_pixel = 0;
	bool align_to_nearest_4th_pixel = false;
	switch (CurMode->type) {
	case M_TEXT:
	case M_LIN4: bits_per_pixel = 4; break;
	case M_LIN8:
		bits_per_pixel = 8;
		panning_factor = 2; // the panning register ignores bit0 in this mode
		break;
	case M_LIN15:
	case M_LIN16:
		bits_per_pixel = 16;
		panning_factor = 2; // this may be DOSBox specific
		break;
	case M_LIN24:
		align_to_nearest_4th_pixel = true;
		bits_per_pixel = 24;
		break;
	case M_LIN32: bits_per_pixel = 32; break;
	default: return VESA_MODE_UNSUPPORTED;
	}
	constexpr uint8_t lcf = 32; // least common factor
	uint32_t start = (vga.config.scan_len * lcf * 2 * y + x * bits_per_pixel) / lcf;
	if (align_to_nearest_4th_pixel)
		start -= (start % 3);
	vga.config.display_start = start;

	// Setting the panning register is nice as it allows for super smooth
	// scrolling, but if we hit the retrace pulse there may be flicker as
	// panning and display start are latched at different times. 

	IO_Read(0x3da);              // reset attribute flipflop
	IO_Write(0x3c0,0x13 | 0x20); // panning register, screen on

	const auto new_panning = x % (lcf / bits_per_pixel);
	IO_Write(0x3c0, static_cast<uint8_t>(new_panning * panning_factor));

	// Wait for retrace if requested
	if (wait) CALLBACK_RunRealFar(RealSegment(int10.rom.wait_retrace),RealOffset(int10.rom.wait_retrace));

	return VESA_SUCCESS;
}

uint8_t VESA_GetDisplayStart(uint16_t & x,uint16_t & y) {
	Bitu pixels_per_offset;
	Bitu panning_factor = 1;

	switch (CurMode->type) {
	case M_TEXT:
	case M_LIN4:
		pixels_per_offset = 16;
		break;
	case M_LIN8:
		panning_factor = 2;
		pixels_per_offset = 8;
		break;
	case M_LIN15:
	case M_LIN16:
		panning_factor = 2;
		pixels_per_offset = 4;
		break;
	case M_LIN32:
		pixels_per_offset = 2;
		break;
	default:
		return VESA_MODE_UNSUPPORTED;
	}

	IO_Read(0x3da);              // reset attribute flipflop
	IO_Write(0x3c0,0x13 | 0x20); // panning register, screen on
	uint8_t panning = IO_Read(0x3c1);

	Bitu virtual_screen_width = vga.config.scan_len * pixels_per_offset;
	Bitu start_pixel = vga.config.display_start * (pixels_per_offset/2) 
		+ panning / panning_factor;
	
	y = start_pixel / virtual_screen_width;
	x = start_pixel % virtual_screen_width;
	return VESA_SUCCESS;
}

static Bitu VESA_SetWindow(void) {
	if (reg_bh) reg_ah=VESA_GetCPUWindow(reg_bl,reg_dx);
	else reg_ah=VESA_SetCPUWindow(reg_bl,(uint8_t)reg_dx);
	reg_al=0x4f;
	return CBRET_NONE;
}

static Bitu VESA_PMSetWindow(void) {
	IO_Write(0x3d4,0x6a);
	IO_Write(0x3d5,reg_dl);
	return CBRET_NONE;
}
static Bitu VESA_PMSetPalette(void) {
	PhysPt data=SegPhys(es)+reg_edi;
	uint32_t count=reg_cx;
	IO_Write(0x3c8,reg_dl);
	do {
		IO_Write(0x3c9,mem_readb(data+2));
		IO_Write(0x3c9,mem_readb(data+1));
		IO_Write(0x3c9,mem_readb(data));
		data+=4;
	} while (--count);
	return CBRET_NONE;
}
static Bitu VESA_PMSetStart(void) {
	uint32_t start = (reg_dx << 16) | reg_cx;
	vga.config.display_start = start;
	return CBRET_NONE;
}




void INT10_SetupVESA(void) {
	/* Put the mode list somewhere in memory */
	Bitu i;
	i=0;
	int10.rom.vesa_modes=RealMake(0xc000,int10.rom.used);
//TODO Maybe add normal vga modes too, but only seems to complicate things
	while (ModeList_VGA[i].mode!=0xffff) {
		bool canuse_mode=false;
		if (!svga.accepts_mode) canuse_mode=true;
		else {
			if (svga.accepts_mode(ModeList_VGA[i].mode)) canuse_mode=true;
		}
		if (ModeList_VGA[i].mode >= MinVesaBiosModeNumber && canuse_mode) {
			if (!int10.vesa_oldvbe || ModeList_VGA[i].mode < 0x120) {
				phys_writew(PhysicalMake(0xc000, int10.rom.used), ModeList_VGA[i].mode);
				int10.rom.used += 2;
			}
		}
		i++;
	}
	phys_writew(PhysicalMake(0xc000,int10.rom.used),0xffff);
	int10.rom.used+=2;

	int10.rom.oemstring = RealMake(0xc000, int10.rom.used);
	for (const char& c : string_oem) {
		phys_writeb(0xc0000 + int10.rom.used++, c);
	}
	phys_writeb(0xc0000 + int10.rom.used++, 0);

	/* Prepare the real mode interface */
	int10.rom.wait_retrace=RealMake(0xc000,int10.rom.used);
	int10.rom.used += (uint16_t)CALLBACK_Setup(0, nullptr, CB_VESA_WAIT, PhysicalMake(0xc000,int10.rom.used), "");
	callback.rmWindow=CALLBACK_Allocate();
	int10.rom.set_window=RealMake(0xc000,int10.rom.used);
	int10.rom.used += (uint16_t)CALLBACK_Setup(callback.rmWindow, VESA_SetWindow, CB_RETF, PhysicalMake(0xc000,int10.rom.used), "VESA Real Set Window");
	/* Prepare the pmode interface */
	int10.rom.pmode_interface=RealMake(0xc000,int10.rom.used);
	int10.rom.used += 8;		//Skip the byte later used for offsets
	/* PM Set Window call */
	int10.rom.pmode_interface_window = int10.rom.used - RealOffset( int10.rom.pmode_interface );
	phys_writew( RealToPhysical(int10.rom.pmode_interface) + 0, int10.rom.pmode_interface_window );
	callback.pmWindow=CALLBACK_Allocate();
	int10.rom.used += (uint16_t)CALLBACK_Setup(callback.pmWindow, VESA_PMSetWindow, CB_RETN, PhysicalMake(0xc000,int10.rom.used), "VESA PM Set Window");
	/* PM Set start call */
	int10.rom.pmode_interface_start = int10.rom.used - RealOffset( int10.rom.pmode_interface );
	phys_writew( RealToPhysical(int10.rom.pmode_interface) + 2, int10.rom.pmode_interface_start);
	callback.pmStart=CALLBACK_Allocate();
	int10.rom.used += (uint16_t)CALLBACK_Setup(callback.pmStart,
	                                         VESA_PMSetStart, CB_VESA_PM,
	                                         PhysicalMake(0xc000, int10.rom.used),
	                                         "VESA PM Set Start");
	/* PM Set Palette call */
	int10.rom.pmode_interface_palette = int10.rom.used - RealOffset( int10.rom.pmode_interface );
	phys_writew( RealToPhysical(int10.rom.pmode_interface) + 4, int10.rom.pmode_interface_palette);
	callback.pmPalette=CALLBACK_Allocate();
	int10.rom.used += (uint16_t)CALLBACK_Setup(0, nullptr, CB_VESA_PM, PhysicalMake(0xc000,int10.rom.used), "");
	int10.rom.used += (uint16_t)CALLBACK_Setup(callback.pmPalette, VESA_PMSetPalette, CB_RETN, PhysicalMake(0xc000,int10.rom.used), "VESA PM Set Palette");
	/* Finalize the size and clear the required ports pointer */
	phys_writew( RealToPhysical(int10.rom.pmode_interface) + 6, 0);
	int10.rom.pmode_interface_size=int10.rom.used - RealOffset( int10.rom.pmode_interface );
}
