// SPDX-FileCopyrightText:  2022-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dosbox.h"

#include <algorithm>
#include <cassert>
#include <string>

#include "vga.h"

#include "hardware/memory.h"
#include "hardware/pci_bus.h"
#include "hardware/port.h"
#include "ints/int10.h"
#include "misc/support.h"
#include "utils/string_utils.h"

void PCI_AddSVGAS3_Device();

static bool SVGA_S3_HWCursorActive()
{
	return (vga.s3.hgc.curmode & 0x1) != 0;
}

void SVGA_S3_WriteCRTC(io_port_t reg, io_val_t value, io_width_t)
{
	const auto val = check_cast<uint8_t>(value);
	switch (reg) {
	case 0x31:	/* CR31 Memory Configuration */
//TODO Base address
		vga.s3.reg_31 = val;
		vga.config.compatible_chain4 = !(val&0x08);
		if (vga.config.compatible_chain4) vga.vmemwrap = 256*1024;
 		else vga.vmemwrap = vga.vmemsize;
		vga.config.display_start = (vga.config.display_start&~0x30000)|((val&0x30)<<12);
		VGA_DetermineMode();
		VGA_SetupHandlers();
		break;
		/*
			0	Enable Base Address Offset (CPUA BASE). Enables bank operation if
				set, disables if clear.
			1	Two Page Screen Image. If set enables 2048 pixel wide screen setup
			2	VGA 16bit Memory Bus Width. Set for 16bit, clear for 8bit
			3	Use Enhanced Mode Memory Mapping (ENH MAP). Set to enable access to
				video memory above 256k.
			4-5	Bit 16-17 of the Display Start Address. For the 801/5,928 see index
				51h, for the 864/964 see index 69h.
			6	High Speed Text Display Font Fetch Mode. If set enables Page Mode
				for Alpha Mode Font Access.
			7	(not 864/964) Extended BIOS ROM Space Mapped out. If clear the area
				C6800h-C7FFFh is mapped out, if set it is accessible.
		*/
	case 0x35:	/* CR35 CRT Register Lock */
		if (vga.s3.reg_lock1 != 0x48) return;	//Needed for uvconfig detection
		vga.s3.reg_35=val & 0xf0;
		if ((vga.svga.bank_read & 0xf) ^ (val & 0xf)) {
			vga.svga.bank_read&=0xf0;
			vga.svga.bank_read|=val & 0xf;
			vga.svga.bank_write = vga.svga.bank_read;
			VGA_SetupHandlers();
		}
		break;
		/*
			0-3	CPU Base Address. 64k bank number. For the 801/5 and 928 see 3d4h
				index 51h bits 2-3. For the 864/964 see index 6Ah.
			4	Lock Vertical Timing Registers (LOCK VTMG). Locks 3d4h index 6, 7
				(bits 0,2,3,5,7), 9 bit 5, 10h, 11h bits 0-3, 15h, 16h if set
			5	Lock Horizontal Timing Registers (LOCK HTMG). Locks 3d4h index
				0,1,2,3,4,5,17h bit 2 if set
			6	(911/924) Lock VSync Polarity.
			7	(911/924) Lock HSync Polarity.
		*/
	case 0x38:	/* CR38 Register Lock 1 */
		vga.s3.reg_lock1=val;
		break;
	case 0x39:	/* CR39 Register Lock 2 */
		vga.s3.reg_lock2=val;
		break;
	case 0x3a:
		vga.s3.reg_3a = val;
		break;
	case 0x40:  /* CR40 System Config */
		vga.s3.reg_40 = val;
		break;
	case 0x41:  /* CR41 BIOS flags */
		vga.s3.reg_41 = val;
		break;
	case 0x43:	/* CR43 Extended Mode */
		vga.s3.reg_43=val & ~0x4;
		if (((val & 0x4) ^ (vga.config.scan_len >> 6)) & 0x4) {
			vga.config.scan_len&=0x2ff;
			vga.config.scan_len|=(val & 0x4) << 6;
			VGA_CheckScanLength();
		}
		break;
		/*
			2  Logical Screen Width bit 8. Bit 8 of the Display Offset Register/
			(3d4h index 13h). (801/5,928) Only active if 3d4h index 51h bits 4-5
			are 0
		*/
	case 0x45: { /* Hardware cursor mode */
		const bool was_active = SVGA_S3_HWCursorActive();
		vga.s3.hgc.curmode = val;
		if (SVGA_S3_HWCursorActive() != was_active) {
			// Activate hardware cursor code if needed
			VGA_ActivateHardwareCursor();
		}
		break;
	}
	case 0x46:
		vga.s3.hgc.originx = (vga.s3.hgc.originx & 0x00ff) | (val << 8);
		break;
	case 0x47:  /*  HGC orgX */
		vga.s3.hgc.originx = (vga.s3.hgc.originx & 0xff00) | val;
		break;
	case 0x48:
		vga.s3.hgc.originy = (vga.s3.hgc.originy & 0x00ff) | (val << 8);
		break;
	case 0x49:  /*  HGC orgY */
		vga.s3.hgc.originy = (vga.s3.hgc.originy & 0xff00) | val;
		break;
	case 0x4A:  /* HGC foreground stack */
		if (vga.s3.hgc.fstackpos > 2) vga.s3.hgc.fstackpos = 0;
		vga.s3.hgc.forestack[vga.s3.hgc.fstackpos] = val;
		vga.s3.hgc.fstackpos++;
		break;
	case 0x4B:  /* HGC background stack */
		if (vga.s3.hgc.bstackpos > 2) vga.s3.hgc.bstackpos = 0;
		vga.s3.hgc.backstack[vga.s3.hgc.bstackpos] = val;
		vga.s3.hgc.bstackpos++;
		break;
	case 0x4c:  /* HGC start address high byte*/
		vga.s3.hgc.startaddr &=0xff;
		vga.s3.hgc.startaddr |= ((val & 0xf) << 8);
		if ((((Bitu)vga.s3.hgc.startaddr)<<10)+((64*64*2)/8) > vga.vmemsize) {
			vga.s3.hgc.startaddr &= 0xff;	// put it back to some sane area;
			                                // if read back of this address is ever implemented this needs to change
			LOG(LOG_VGAMISC,LOG_NORMAL)("VGA:S3:CRTC: HGC pattern address beyond video memory" );
		}
		break;
	case 0x4d:  /* HGC start address low byte*/
		vga.s3.hgc.startaddr &=0xff00;
		vga.s3.hgc.startaddr |= (val & 0xff);
		break;
	case 0x4e:  /* HGC pattern start X */
		vga.s3.hgc.posx = val & 0x3f;	// bits 0-5
		break;
	case 0x4f:  /* HGC pattern start Y */
		vga.s3.hgc.posy = val & 0x3f;	// bits 0-5
		break;
	case 0x50:  // Extended System Control 1
		vga.s3.reg_50 = val;
		switch (val & S3_XGA_CMASK) {
			case S3_XGA_32BPP: vga.s3.xga_color_mode = M_LIN32; break;
			case S3_XGA_16BPP: vga.s3.xga_color_mode = M_LIN16; break;
			case S3_XGA_8BPP: vga.s3.xga_color_mode = M_LIN8; break;
		}
		switch (val & S3_XGA_WMASK) {
		case S3_XGA_640: vga.s3.xga_screen_width = 640; break;
		case S3_XGA_800: vga.s3.xga_screen_width = 800; break;
		case S3_XGA_1024: vga.s3.xga_screen_width = 1024; break;
		case S3_XGA_1152: vga.s3.xga_screen_width = 1152; break;
		case S3_XGA_1280: vga.s3.xga_screen_width = 1280; break;
		case S3_XGA_1600: vga.s3.xga_screen_width = 1600; break;
		default: vga.s3.xga_screen_width = 1024; break;
		}
		break;
	case 0x51:                          /* Extended System Control 2 */
		vga.s3.reg_51 = val & 0xc0; // Only store bits 6,7
		vga.config.display_start &= 0xF3FFFF;
		vga.config.display_start |= (val & 3) << 18;
		if ((vga.svga.bank_read & 0x30) ^ ((val & 0xc) << 2)) {
			vga.svga.bank_read&=0xcf;
			vga.svga.bank_read|=(val&0xc)<<2;
			vga.svga.bank_write = vga.svga.bank_read;
			VGA_SetupHandlers();
		}
		if (((val & 0x30) ^ (vga.config.scan_len >> 4)) & 0x30) {
			vga.config.scan_len&=0xff;
			vga.config.scan_len|=(val & 0x30) << 4;
			VGA_CheckScanLength();
		}
		break;
		/*
			0	(80x) Display Start Address bit 18
			0-1	(928 +) Display Start Address bit 18-19
				Bits 16-17 are in index 31h bits 4-5, Bits 0-15 are in 3d4h index
				0Ch,0Dh. For the 864/964 see 3d4h index 69h
			2	(80x) CPU BASE. CPU Base Address Bit 18.
			2-3	(928 +) Old CPU Base Address Bits 19-18.
				64K Bank register bits 4-5. Bits 0-3 are in 3d4h index 35h.
				For the 864/964 see 3d4h index 6Ah
			4-5	Logical Screen Width Bit [8-9]. Bits 8-9 of the CRTC Offset register
				(3d4h index 13h). If this field is 0, 3d4h index 43h bit 2 is active
			6	(928,964) DIS SPXF. Disable Split Transfers if set. Spilt Transfers
				allows transferring one half of the VRAM shift register data while
				the other half is being output. For the 964 Split Transfers
				must be enabled in enhanced modes (4AE8h bit 0 set). Guess: They
				probably can't time the VRAM load cycle closely enough while the
				graphics engine is running.
			7	(not 864/964) Enable EPROM Write. If set enables flash memory write
				control to the BIOS ROM address
		*/
	case 0x52:  // Extended System Control 1
		vga.s3.reg_52 = val;
		break;
	case 0x53:
		// Map or unmap MMIO
		// bit 4 = MMIO at A0000
		// bit 3 = MMIO at LFB + 16M (should be fine if its always enabled for now)
		if(vga.s3.ext_mem_ctrl!=val) {
			vga.s3.ext_mem_ctrl = val;
			VGA_SetupHandlers();
		}
		break;
	case 0x55:	/* Extended Video DAC Control */
		vga.s3.reg_55=val;
		break;
		/*
			0-1	DAC Register Select Bits. Passed to the RS2 and RS3 pins on the
				RAMDAC, allowing access to all 8 or 16 registers on advanced RAMDACs.
				If this field is 0, 3d4h index 43h bit 1 is active.
			2	Enable General Input Port Read. If set DAC reads are disabled and the
				STRD strobe for reading the General Input Port is enabled for reading
				while DACRD is active, if clear DAC reads are enabled.
			3	(928) Enable External SID Operation if set. If set video data is
				passed directly from the VRAMs to the DAC rather than through the
				VGA chip
			4	Hardware Cursor MS/X11 Mode. If set the Hardware Cursor is in X11
				mode, if clear in MS-Windows mode
			5	(80x,928) Hardware Cursor External Operation Mode. If set the two
				bits of cursor data ,is output on the HC[0-1] pins for the video DAC
				The SENS pin becomes HC1 and the MID2 pin becomes HC0.
			6	??
			7	(80x,928) Disable PA Output. If set PA[0-7] and VCLK are tristated.
				(864/964) TOFF VCLK. Tri-State Off VCLK Output. VCLK output tri
				-stated if set
		*/
	case 0x58:	/* Linear Address Window Control */
		vga.s3.reg_58=val;
		break;
		/*
			0-1	Linear Address Window Size. Must be less than or equal to video
				memory size. 0: 64K, 1: 1MB, 2: 2MB, 3: 4MB (928)/8Mb (864/964)
			2	(not 864/964) Enable Read Ahead Cache if set
			3	(80x,928) ISA Latch Address. If set latches address during every ISA
				cycle, unlatches during every ISA cycle if clear.
				(864/964) LAT DEL. Address Latch Delay Control (VL-Bus only). If set
				address latching occours in the T1 cycle, if clear in the T2 cycle
				(I.e. one clock cycle delayed).
			4	ENB LA. Enable Linear Addressing if set.
			5	(not 864/964) Limit Entry Depth for Write-Post. If set limits Write
				-Post Entry Depth to avoid ISA bus timeout due to wait cycle limit.
			6	(928,964) Serial Access Mode (SAM) 256 Words Control. If set SAM
				control is 256 words, if clear 512 words.
			7	(928) RAS 6-MCLK. If set the random read/write cycle time is 6MCLKs,
				if clear 7MCLKs
		*/
	case 0x59:	/* Linear Address Window Position High */
		if ((vga.s3.la_window&0xff00) ^ (val << 8)) {
			vga.s3.la_window=(vga.s3.la_window&0x00ff) | (val << 8);
			VGA_StartUpdateLFB();
		}
		break;
	case 0x5a:	/* Linear Address Window Position Low */
		if ((vga.s3.la_window&0x00ff) ^ val) {
			vga.s3.la_window=(vga.s3.la_window&0xff00) | val;
			VGA_StartUpdateLFB();
		}
		break;
	case 0x5D:	/* Extended Horizontal Overflow */
		if ((val ^ vga.s3.ex_hor_overflow) & 3) {
			vga.s3.ex_hor_overflow=val;
			VGA_StartResize();
		} else vga.s3.ex_hor_overflow=val;
		break;
		/*
			0	Horizontal Total bit 8. Bit 8 of the Horizontal Total register (3d4h
				index 0)
			1	Horizontal Display End bit 8. Bit 8 of the Horizontal Display End
				register (3d4h index 1)
			2	Start Horizontal Blank bit 8. Bit 8 of the Horizontal Start Blanking
				register (3d4h index 2).
			3	(864,964) EHB+64. End Horizontal Blank +64. If set the /BLANK pulse
				is extended by 64 DCLKs. Note: Is this bit 6 of 3d4h index 3 or
				does it really extend by 64 ?
			4	Start Horizontal Sync Position bit 8. Bit 8 of the Horizontal Start
				Retrace register (3d4h index 4).
			5	(864,964) EHS+32. End Horizontal Sync +32. If set the HSYNC pulse
				is extended by 32 DCLKs. Note: Is this bit 5 of 3d4h index 5 or
				does it really extend by 32 ?
			6	(928,964) Data Transfer Position bit 8. Bit 8 of the Data Transfer
				Position register (3d4h index 3Bh)
			7	(928,964) Bus-Grant Terminate Position bit 8. Bit 8 of the Bus Grant
				Termination register (3d4h index 5Fh).
		*/
	case 0x5e:	/* Extended Vertical Overflow */
		vga.config.line_compare=(vga.config.line_compare & 0x3ff) | (val & 0x40) << 4;
		if ((val ^ vga.s3.ex_ver_overflow) & 0x3) {
			vga.s3.ex_ver_overflow=val;
			VGA_StartResize();
		} else vga.s3.ex_ver_overflow=val;
		break;
		/*
			0	Vertical Total bit 10. Bit 10 of the Vertical Total register (3d4h
				index 6). Bits 8 and 9 are in 3d4h index 7 bit 0 and 5.
			1	Vertical Display End bit 10. Bit 10 of the Vertical Display End
				register (3d4h index 12h). Bits 8 and 9 are in 3d4h index 7 bit 1
				and 6
			2	Start Vertical Blank bit 10. Bit 10 of the Vertical Start Blanking
				register (3d4h index 15h). Bit 8 is in 3d4h index 7 bit 3 and bit 9
				in 3d4h index 9 bit 5
			4	Vertical Retrace Start bit 10. Bit 10 of the Vertical Start Retrace
				register (3d4h index 10h). Bits 8 and 9 are in 3d4h index 7 bit 2
				and 7.
			6	Line Compare Position bit 10. Bit 10 of the Line Compare register
				(3d4h index 18h). Bit 8 is in 3d4h index 7 bit 4 and bit 9 in 3d4h
				index 9 bit 6.
		*/
	case 0x63: // Extended Control Register CR63
		vga.s3.reg_63 = val;
		break;
	case 0x67:	/* Extended Miscellaneous Control 2 */
		/*
			0	VCLK PHS. VCLK Phase With Respect to DCLK. If clear VLKC is inverted
				DCLK, if set VCLK = DCLK.
			2-3 (Trio64V+) streams mode
					00 disable Streams Processor
					01 overlay secondary stream on VGA-mode background
					10 reserved
					11 full Streams Processor operation
			4-7	Pixel format.
					0  Mode  0: 8bit (1 pixel/VCLK)
					1  Mode  8: 8bit (2 pixels/VCLK)
					3  Mode  9: 15bit (1 pixel/VCLK)
					5  Mode 10: 16bit (1 pixel/VCLK)
					7  Mode 11: 24/32bit (2 VCLKs/pixel)
					13  (732/764) 32bit (1 pixel/VCLK)
		*/
		vga.s3.misc_control_2=val;
		VGA_DetermineMode();
		break;
	case 0x69:	/* Extended System Control 3 */
		if (((vga.config.display_start & 0x1f0000)>>16) ^ (val & 0x1f)) {
			vga.config.display_start&=0xffff;
			vga.config.display_start|=(val & 0x1f) << 16;
		}
		break;
	case 0x6a:	/* Extended System Control 4 */
		vga.svga.bank_read=val & 0x7f;
		vga.svga.bank_write = vga.svga.bank_read;
		VGA_SetupHandlers();
		break;
	case 0x6b:	// BIOS scratchpad: LFB address
		vga.s3.reg_6b = val;
		break;
	default:
		LOG(LOG_VGAMISC,LOG_NORMAL)("VGA:S3:CRTC:Write to illegal index %2X", static_cast<uint32_t>(reg));
		break;
	}
}

uint8_t SVGA_S3_ReadCRTC(io_port_t reg, io_width_t)
{
	switch (reg) {
	case 0x24:	/* attribute controller index (read only) */
	case 0x26:
		return ((vga.attr.disabled & 1)?0x00:0x20) | (vga.attr.index & 0x1f);
	case 0x2d:	/* Extended Chip ID (high byte of PCI device ID) */
		return 0x88;
	case 0x2e:	/* New Chip ID  (low byte of PCI device ID) */
		return 0x11;	// Trio64
	case 0x2f:	/* Revision */
		return 0x00;	// Trio64 (exact value?)
//		return 0x44;	// Trio64 V+
	case 0x30:	/* CR30 Chip ID/REV register */
		return 0xe1;	// Trio+ dual byte
	case 0x31:	/* CR31 Memory Configuration */
//TODO mix in bits from baseaddress;
		return 	vga.s3.reg_31;
	case 0x35:	/* CR35 CRT Register Lock */
		return vga.s3.reg_35|(vga.svga.bank_read & 0xf);
	case 0x36: /* CR36 Reset State Read 1 */
		return vga.s3.reg_36;
	case 0x37: /* Reset state read 2 */
		return 0x2b;
	case 0x38: /* CR38 Register Lock 1 */
		return vga.s3.reg_lock1;
	case 0x39: /* CR39 Register Lock 2 */
		return vga.s3.reg_lock2;
	case 0x3a:
		return vga.s3.reg_3a;
	case 0x40: /* CR40 system config */
		return vga.s3.reg_40;
	case 0x41: /* CR40 system config */
		return vga.s3.reg_41;
	case 0x42: // not interlaced
		return 0x0d;
	case 0x43:	/* CR43 Extended Mode */
		return vga.s3.reg_43|((vga.config.scan_len>>6)&0x4);
	case 0x45:  /* Hardware cursor mode */
		vga.s3.hgc.bstackpos = 0;
		vga.s3.hgc.fstackpos = 0;
		return vga.s3.hgc.curmode|0xa0;
	case 0x46:
		return vga.s3.hgc.originx>>8;
	case 0x47:  /*  HGC orgX */
		return vga.s3.hgc.originx&0xff;
	case 0x48:
		return vga.s3.hgc.originy>>8;
	case 0x49:  /*  HGC orgY */
		return vga.s3.hgc.originy&0xff;
	case 0x4A:  /* HGC foreground stack */
		return vga.s3.hgc.forestack[vga.s3.hgc.fstackpos];
	case 0x4B:  /* HGC background stack */
		return vga.s3.hgc.backstack[vga.s3.hgc.bstackpos];
	case 0x50:	// CR50 Extended System Control 1
		return vga.s3.reg_50;
	case 0x51:	/* Extended System Control 2 */
		return ((vga.config.display_start >> 16) & 3 ) |
				((vga.svga.bank_read & 0x30) >> 2) |
				((vga.config.scan_len & 0x300) >> 4) |
				vga.s3.reg_51;
	case 0x52:	// CR52 Extended BIOS flags 1
		return vga.s3.reg_52;
	case 0x53:
		return vga.s3.ext_mem_ctrl;
	case 0x55:	/* Extended Video DAC Control */
		return vga.s3.reg_55;
	case 0x58:	/* Linear Address Window Control */
		return	vga.s3.reg_58;
	case 0x59:	/* Linear Address Window Position High */
		return (vga.s3.la_window >> 8);
	case 0x5a:	/* Linear Address Window Position Low */
		return (vga.s3.la_window & 0xff);
	case 0x63: // Extended Control Register CR63
		return vga.s3.reg_63;
	case 0x5D:	/* Extended Horizontal Overflow */
		return vga.s3.ex_hor_overflow;
	case 0x5e:	/* Extended Vertical Overflow */
		return vga.s3.ex_ver_overflow;
	case 0x67:	/* Extended Miscellaneous Control 2 */
		return vga.s3.misc_control_2;
	case 0x69:	/* Extended System Control 3 */
		return (uint8_t)((vga.config.display_start & 0x1f0000)>>16);
	case 0x6a:	/* Extended System Control 4 */
		return (uint8_t)(vga.svga.bank_read & 0x7f);
	case 0x6b:	// BIOS scatchpad: LFB address
		return vga.s3.reg_6b;
	default:
		return 0x00;
	}
}

void SVGA_S3_WriteSEQ(io_port_t reg, io_val_t value, io_width_t)
{
	const auto val = check_cast<uint8_t>(value);
	if (reg > 0x8 && vga.s3.pll.lock != 0x6)
		return;

	// The PLL M value can be programmed with any integer value from 1 to
	// 127. The binary equivalent of this value is programmed in bits 6-0 of
	// SR11 for the MCLK and in bits 6-0 of SR13 for the DCLK.
	auto to_ppl_m = [](const uint8_t val) -> uint8_t {
		return val & 0b0111'1111;
	};

	// The PLL N value can be programmed with any integer value from 1
	// to 31. The binary equivalent of this value is programmed in bits
	// 4-0 of SR10 for the MCLK and in bits 4-0 of SR12 for the DCLK.
	auto to_ppl_n = [](const uint8_t val) -> uint8_t {
		return val & 0b0001'1111;
	};

	// The PLL R value is a 2-bit range value that can be programmed with
	// any integer value from 0 to 3. The R value is programmed in bits 6-5
	// of SR 10 for MCLK and bits 6-5 of SR12 for DCLK.
	auto to_ppl_r = [](const uint8_t val) -> uint8_t {
		return (val & 0b0110'0000) >> 5;
	};

	switch (reg) {
	case 0x08: // Register lock / unlock
		vga.s3.pll.lock=val;
		break;
	case 0x10: // Memory PLL Data Low
		vga.s3.mclk.n = to_ppl_n(val);
		vga.s3.mclk.r = to_ppl_r(val);
		break;
	case 0x11: // Memory PLL Data High
		vga.s3.mclk.m = to_ppl_m(val);
		break;
	case 0x12: // Video PLL Data Low
		vga.s3.clk[3].n = to_ppl_n(val);
		vga.s3.clk[3].r = to_ppl_r(val);
		break;
	case 0x13: // Video PLL Data High
		vga.s3.clk[3].m = to_ppl_m(val);
		break;
	case 0x15: // CLKSYN Control 2 Register
		vga.s3.pll.control_2 = val;

		/*
		CLKSYN Control 2 (SR15), pp 130
		~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		Bit 0 MFRQ EN - Enable new MCLK frequency load
		0 = Register bit clear
		1 = Load new MCLK frequency

		When new MCLK PLL values are programmed, this bit can be set to
		1 to load these values in the PLL. The loading may be delayed a
		small but variable amount of time. This bit should be cleared to
		0 after loading to prevent repeated loading. Alternately, use
		bit 5 of this register to produce an immediate load.

		Bit 1 DFRQ EN - Enable new DCLK frequency load
		0 = Register bit clear
		1 - Load new DCLK frequency

		When new DCLK PLL values are programmed, this bit can be set to
		1 to load these values in the PLL. Bits 3-2 of 3C2H must also be
		set to 11b if they are not already at this value. The loading
		may be delayed a small but variable amount of time. This bit
		should be programmed to 1 at power-up to allow loading of the
		VGA DCLK value and then left at this setting. Use bit 5 of this
		register to produce an immediate load.

		Bit 2 MCLK OUT - Output internally generated MCLK
		0 = Pin 147 acts as the STWR strobe
		1 = Pin 147 outputs the internally generated MCLK

		This is used only for testing.

		Bit 3 VCLK OUT - VCLK direction determined by EVCLK

		0 = Pin 148 outputs the internally generated VCLK regardless of
		the state of EVCLK

		1 = VCLK direction is determined by the EVCLK signal

		Bit 4 DCLK/2 - Divide DCLK by 2
		0 = DCLK unchanged
		1 = Divide DCLK by 2

		Either this bit or bit 6 of this register must be set to 1 for
		clock doubled RAMDAC op- eration (mode 0001).

		Bit 5 CLK LOAD - MCLK, DCLK load
		0 = Clock loading is controlled by bits 0 and 1 of this register
		1 = Load MCLK and DCLK PLL values immediately

		To produce an immediate MCLK and DCLK load, program this bit to
		1 and then to 0. Bits 3-2 of 3C2H must also then be programmed
		to 11b to load the DCLK values if they are not already
		programmed to this value. This register must never be left set
		to 1.

		Bit 6 DCLK INV - Invert DCLK
		0 = DCLK unchanged
		1 = Invert DCLK

		Either this bit or bit 4 of this register must be set to 1 for
		clock doubled RAMDAC op- eration (mode 0001).

		Bit 7 2 CYC MWR - Enable 2 cycle memory write
		0 = 3 MCLK memory write
		1 = 2 MCLK memory write

		Setting this bit to 1 bypasses the VGA logic for linear
		addressing when bit 7 of SRA is set to 1. This can allow 2 MCLK
		operation for MCLK frequencies between 55 and 57 MHz.
		*/

		// Only initiate a mode change if bit 0, 1, or 5 are set
		if (val & 0b0010'0011)
			VGA_StartResize();
		break;
	case 0x18: // RADAC/CLKSYN Control Register (SR18)
		vga.s3.pll.control = val;
		break;
	default:
		LOG(LOG_VGAMISC,LOG_NORMAL)("VGA:S3:SEQ:Write to illegal index %2X", static_cast<uint32_t>(reg));
		break;
	}
}

uint8_t SVGA_S3_ReadSEQ(io_port_t reg, io_width_t)
{
	/* S3 specific group */
	if (reg > 0x8 && vga.s3.pll.lock != 0x6)
		return (reg < 0x1b) ? 0 : static_cast<uint8_t>(reg);

	switch (reg) {
	case 0x08: // PLL Unlock
		return vga.s3.pll.lock;
	case 0x10: // Memory PLL Data Low
		return (vga.s3.mclk.r << 5) | vga.s3.mclk.n;
	case 0x11: // Memory PLL Data High
		return vga.s3.mclk.m;
	case 0x12: // Video PLL Data Low
		return (vga.s3.clk[3].r << 5) | vga.s3.clk[3].n;
	case 0x13: // Video Data High
		return vga.s3.clk[3].m;
	case 0x15: // CLKSYN Control 2 Register
		return vga.s3.pll.control_2;
	case 0x18: // RADAC/CLKSYN Control Register (SR18)
		return vga.s3.pll.control;
	default:
		LOG(LOG_VGAMISC,LOG_NORMAL)("VGA:S3:SEQ:Read from illegal index %2X", static_cast<uint32_t>(reg));
		return 0;
	}
}

uint32_t SVGA_S3_GetClock(void)
{
	uint32_t clock = (vga.misc_output >> 2) & 3;
	if (clock == 0) {
		clock = Vga640PixelClockHz;
	} else if (clock == 1) {
		clock = Vga720PixelClockHz;
	} else {
		clock = 1000 * S3_CLOCK(vga.s3.clk[clock].m,
		                        vga.s3.clk[clock].n,
		                        vga.s3.clk[clock].r);
	}
	// Check for dual transfer, clock/2
	if (vga.s3.pll.control_2 & 0x10) {
		clock /= 2;
	}
	return clock;
}

bool SVGA_S3_AcceptsMode(Bitu mode) {
	return VideoModeMemSize(mode) < vga.vmemsize;
}

void replace_mode_120h_with_halfline()
{
	constexpr VideoModeBlock halfline_block = {.mode     = 0x120,
	                                           .type     = M_LIN16,
	                                           .swidth   = 640,
	                                           .sheight  = 400,
	                                           .twidth   = 80,
	                                           .theight  = 25,
	                                           .cwidth   = 8,
	                                           .cheight  = 16,
	                                           .ptotal   = 1,
	                                           .pstart   = 0xa0000,
	                                           .plength  = 0x10000,
	                                           .htotal   = 200,
	                                           .vtotal   = 449,
	                                           .hdispend = 160,
	                                           .vdispend = 400,
	                                           .special  = 0};

	constexpr auto halfline_mode = halfline_block.mode;

	for (auto& block : ModeList_VGA) {
		if (block.mode == halfline_mode) {
			block = halfline_block;
			break;
		}
	}
}

// Planar 4-bit modes store four planes, so they take up four times the
// page size.
static int to_required_vmem_bytes(const VideoModeBlock& mode_block)
{
	const auto num_planes = (mode_block.type == M_LIN4) ? 4 : 1;
	return VESA_GetModePageSize(mode_block) * num_planes;
}

void replace_1152x864_modes_with_custom_resolution(const VesaCustomResolution& custom)
{
	// The custom resolution is enumerated by the VESA BIOS under the
	// well-established 1152x864 mode numbers, so programs that build
	// their mode list via VESA BIOS calls pick it up automatically
	// (e.g., the VBESVGA Windows 3.x driver).
	constexpr auto _1152x864_4bit  = 0x17a;
	constexpr auto _1152x864_8bit  = 0x207;
	constexpr auto _1152x864_15bit = 0x209;
	constexpr auto _1152x864_16bit = 0x20a;
	constexpr auto _1152x864_24bit = 0x17b;
	constexpr auto _1152x864_32bit = 0x20b;

	// The exact blanking margins only affect the pixel clock, which is
	// derived from the totals to hit the fixed 70 Hz VESA refresh rate.
	constexpr auto BlankingOverheadPercent = 10;

	constexpr auto CharCellHeightPixels = 16;

	const auto width_chars = custom.width / 8;

	const auto htotal = width_chars * (100 + BlankingOverheadPercent) / 100;
	const auto vtotal = custom.height * (100 + BlankingOverheadPercent) / 100;

	for (auto& block : ModeList_VGA) {
		if (!contains(std::vector({_1152x864_4bit,
		                           _1152x864_8bit,
		                           _1152x864_15bit,
		                           _1152x864_16bit,
		                           _1152x864_24bit,
		                           _1152x864_32bit}),
		              block.mode)) {
			continue;
		}

		// The horizontal display end of the 15 and 16-bit modes is
		// doubled (two memory fetches per pixel), but the horizontal
		// total is not, following the original 1152x864 entries. The
		// CRTC setup only handles 9-bit horizontal values, which a
		// doubled total would exceed.
		const auto is_doubled = (block.type == M_LIN15 ||
		                         block.type == M_LIN16);

		block.swidth  = check_cast<uint16_t>(custom.width);
		block.sheight = check_cast<uint16_t>(custom.height);
		block.twidth  = check_cast<uint8_t>(width_chars);
		block.theight = check_cast<uint8_t>(custom.height /
		                                    CharCellHeightPixels);
		block.htotal  = check_cast<uint16_t>(htotal);
		block.vtotal  = check_cast<uint16_t>(vtotal);
		block.hdispend = check_cast<uint16_t>(is_doubled ? width_chars * 2
		                                                 : width_chars);
		block.vdispend = check_cast<uint16_t>(custom.height);
		block.special |= VESA_CUSTOM_RESOLUTION;

		const auto required_vmem_bytes = to_required_vmem_bytes(block);

		if (required_vmem_bytes > static_cast<int>(vga.vmemsize)) {
			auto bits_per_pixel = [&] {
				switch (block.type) {
				case M_LIN4: return 4;
				case M_LIN8: return 8;
				case M_LIN15: return 15;
				case M_LIN16: return 16;
				case M_LIN24: return 24;
				case M_LIN32: return 32;
				default:
					assertm(false, "Invalid VGAModes value");
					return 0;
				}
			};

			LOG_WARNING(
			        "VIDEO: The %dx%d %d-bit custom VESA mode needs "
			        "%d KB of video memory but only %u KB is "
			        "available; increase 'vmemsize' to make the "
			        "mode available",
			        custom.width,
			        custom.height,
			        bits_per_pixel(),
			        required_vmem_bytes / 1024,
			        vga.vmemsize / 1024);
		}
	}
}

void filter_compatible_vesa_modes()
{
	// VESA modes allowed in compatible mode, provided there is enough video
	// memory for them (that is checked dynamically in mode_allowed() below).
	//
	static const std::vector<uint16_t> compatible_modes = {
	        0x151, //  320x240  8-bit
	        0x155, //  320x240  15-bit
	        0x160, //  320x240  15-bit
	        0x156, //  320x240  16-bit
	        0x170, //  320x240  16-bit
	        0x157, //  320x240  24-bit
	        0x158, //  320x240  32-bit
	        0x190, //  320x240  32-bit

	        0x166, //  400x300  8-bit
	        0x167, //  400x300  15-bit
	        0x168, //  400x300  16-bit
	        0x169, //  400x300  24-bit
	        0x16a, //  400x300  32-bit

	        0x215, //  512x384  8-bit
	        0x216, //  512x384  15-bit
	        0x16c, //  512x384  24-bit

	        0x213, //  640x400  32-bit

	        0x177, //  640x480  4-bit
	        0x212, //  640x480  24-bit

	        0x178, //  800x600  24-bit

	        0x207, // 1152x864  8-bit
	        0x209, // 1152x864  15-bit
	        0x20a, // 1152x864  16-bit
	        0x17b, // 1152x864  24-bit
	        0x20b, // 1152x864  32-bit

	        0x17c, // 1280x960  4-bit
	        0x17d, // 1280x960  8-bit
	        0x17e, // 1280x960  15-bit
	        0x17f, // 1280x960  16-bit
	        0x180, // 1280x960  24-bit
	        0x181, // 1280x960  32-bit

	        0x182, // 1280x1024 24-bit

	        0x183, // 1600x1200 4-bit
	        0x184, // 1600x1200 24-bit
	        0x185, // 1600x1200 32-bit
	};

	auto mode_allowed = [&](const VideoModeBlock& m) {
		const auto is_allowed = [&] {
			// Only allow standard text modes
			if (m.type == M_TEXT) {
				constexpr auto _132x28 = 0x230;
				constexpr auto _132x30 = 0x231;
				constexpr auto _132x34 = 0x232;

				return !contains(std::vector({_132x28, _132x30, _132x34}),
				                 m.mode);
			}

			// Allow all non-VESA modes (standard VGA modes, and CGA
			// and EGA as emulated by VGA adapters)
			if (!VESA_IsVesaMode(m.mode)) {
				return true;
			}

			// Allow all common VBA 1.2 modes, regardless of whether
			// they fit into video memory, except 320x200 hi-color
			// modes that were rarely properly supported until the
			// late 90s.
			//
			// VESA_GetSVGAModeInformation() in int_vesa.cpp will
			// set the "Mode supported by hardware configuration"
			// (bit 0 of ModeAttributes) to 0 in the returned mode
			// info block.
			//
			if (m.mode < Vesa20ModesStart) {
				constexpr auto _320x200_15bit = 0x10d;
				constexpr auto _320x200_16bit = 0x10e;
				constexpr auto _320x200_32bit = 0x10f;

				return !contains(std::vector({_320x200_15bit,
				                              _320x200_16bit,
				                              _320x200_32bit}),
				                 m.mode);
			}

			// Selectively allow specific VBE 2.0 modes. This means
			// VESA_GetSVGAModeInformation() won't even return them
			// when a program queries the list of available VESA
			// modes. We do this culling of VBE 2.0 modes to protect
			// programs that crash or misbehave (due to buffer
			// overflow errors) if then encounter "too many" VESA
			// modes.
			if (!contains(compatible_modes, m.mode)) {
				return false;
			}

			// Allow only VBE 2.0 that fit into the video memory.
			const bool fits_vmem = (to_required_vmem_bytes(m) <=
			                        static_cast<int>(vga.vmemsize));
			return fits_vmem;
		}();
#if 0
		auto mode_info = format_str("VGA:S3: mode %04Xh: %4u x %4u - %s",
		                            m.mode,
		                            m.swidth,
		                            m.sheight,
		                            to_string(m.type));
		if (is_allowed) {
			LOG_DEBUG("%s", mode_info.c_str());
		} else {
			LOG_ERR("%s", mode_info.c_str());
		}
#endif
		return is_allowed;
	};

	auto mode_not_allowed = [&](const VideoModeBlock& m) {
		return !mode_allowed(m);
	};

	std::erase_if(ModeList_VGA, mode_not_allowed);

	CurMode = std::prev(ModeList_VGA.end());
}

void SVGA_Setup_S3()
{
	svga.write_p3d5 = &SVGA_S3_WriteCRTC;
	svga.read_p3d5  = &SVGA_S3_ReadCRTC;
	svga.write_p3c5 = &SVGA_S3_WriteSEQ;
	svga.read_p3c5  = &SVGA_S3_ReadSEQ;

	// No S3-specific functionality
	svga.write_p3c0 = nullptr;

	// No S3-specific functionality
	svga.read_p3c1 = nullptr;

	// Implemented in core
	svga.set_video_mode = nullptr;

	// Implemented in core
	svga.determine_mode = nullptr;

	// Implemented in core
	svga.set_clock              = nullptr;
	svga.get_clock              = &SVGA_S3_GetClock;
	svga.hardware_cursor_active = &SVGA_S3_HWCursorActive;
	svga.accepts_mode           = &SVGA_S3_AcceptsMode;

	if (vga.vmemsize == 0) {
		vga.vmemsize = 4 * 1024 * 1024;
	}

	// Set CRTC reg 36 to specify amount of VRAM and PCI
	std::string ram_type = "EDO DRAM";

	if (vga.vmemsize < 1024 * 1024) {
		vga.vmemsize = 512 * 1024;
		// Less than 1 MB EDO RAM
		vga.s3.reg_36 = 0b1111'1010;

	} else if (vga.vmemsize < 2048 * 1024) {
		vga.vmemsize = 1024 * 1024;
		// 1 MB EDO RAM
		vga.s3.reg_36 = 0b1101'1010;

	} else if (vga.vmemsize < 4096 * 1024) {
		vga.vmemsize = 2048 * 1024;
		// 2 MB EDO RAM
		vga.s3.reg_36 = 0b1001'1010;

	} else if (vga.vmemsize < 8192 * 1024) {
		vga.vmemsize = 4096 * 1024;
		// 4 MB fast page mode RAM
		vga.s3.reg_36 = 0b0001'1110;
		ram_type      = "FP DRAM";

	} else {
		vga.vmemsize = 8192 * 1024;
		// 8 MB fast page mode RAM
		vga.s3.reg_36 = 0b0111'1110;
		ram_type      = "FP DRAM";
	}

	std::string description = "S3 Trio64 ";

	description += int10.vesa_oldvbe ? "VESA 1.2" : "VESA 2.0";

	// Must come before the 'vesa_modes' filtering so the custom modes are
	// filtered by their real dimensions
	if (int10.vesa_custom_resolution) {
		replace_1152x864_modes_with_custom_resolution(
		        *int10.vesa_custom_resolution);
	}

	switch (int10.vesa_modes) {
	case VesaModes::Compatible:
		filter_compatible_vesa_modes();
		description += " compatible";
		break;

	case VesaModes::Halfline:
		replace_mode_120h_with_halfline();
		description += " halfline";
		break;

	case VesaModes::All: break;
	}

	if (int10.vesa_nolfb) {
		description += " without LFB";
	}

	const auto num_modes = ModeList_VGA.size();
	VGA_LogInitialization(description.c_str(), ram_type.c_str(), num_modes);

	PCI_AddSVGAS3_Device();
}

struct PCI_VGADevice : public PCI_Device {
	enum { vendor = 0x5333 }; // S3
	enum { device = 0x8811 }; // trio64
	//enum { device = 0x8810 }; // trio32

	PCI_VGADevice():PCI_Device(vendor,device) { }

	Bits ParseReadRegister(uint8_t regnum) override
	{
		return regnum;
	}

	bool OverrideReadRegister([[maybe_unused]] uint8_t regnum,
	                          [[maybe_unused]] uint8_t* rval,
	                          [[maybe_unused]] uint8_t* rval_mask) override
	{
		return false;
	}

	Bits ParseWriteRegister(uint8_t regnum, uint8_t value) override
	{
		if ((regnum>=0x18) && (regnum<0x28)) return -1;	// base addresses are read-only
		if ((regnum>=0x30) && (regnum<0x34)) return -1;	// expansion rom addresses are read-only
		switch (regnum) {
			case 0x10:
				return (PCI_GetCFGData(PCIId(), PCISubfunction(), 0x10)&0x0f);
			case 0x11:
				return 0x00;
			case 0x12:
				//return (value&0xc0);	// -> 4mb addressable
				return (value&0x00);	// -> 16mb addressable
			case 0x13:
				return value;
			case 0x14:
				return (PCI_GetCFGData(PCIId(), PCISubfunction(), 0x10)&0x0f);
			case 0x15:
				return 0x00;
			case 0x16:
				return value;	// -> 64kb addressable
			case 0x17:
				return value;
			default:
				break;
		}
		return value;
	}

	bool InitializeRegisters(uint8_t registers[256]) override
	{
		// init (S3 graphics card)
		//registers[0x08] = 0x44;	// revision ID (s3 trio64v+)
		registers[0x08] = 0x00;	// revision ID
		registers[0x09] = 0x00;	// interface
		registers[0x0a] = 0x00;	// subclass type (vga compatible)
		//registers[0x0a] = 0x01;	// subclass type (xga device)
		registers[0x0b] = 0x03;	// class type (display controller)
		registers[0x0c] = 0x00;	// cache line size
		registers[0x0d] = 0x00;	// latency timer
		registers[0x0e] = 0x00;	// header type (other)

		// reset
		registers[0x04] = 0x23;	// command register (vga palette snoop, ports enabled, memory space enabled)
		registers[0x05] = 0x00;
		registers[0x06] = 0x80;	// status register (medium timing, fast back-to-back)
		registers[0x07] = 0x02;

		//registers[0x3c] = 0x0b;	// irq line
		//registers[0x3d] = 0x01;	// irq pin

		// BAR0 - memory space, within first 4GB
		// Check 8-byte alignment of LFB base
		static_assert((PciGfxLfbBase & 0xf) == 0);
		registers[0x10] = static_cast<uint8_t>(PciGfxLfbBase & 0xff);
		registers[0x11] = static_cast<uint8_t>((PciGfxLfbBase >> 8) & 0xff);
		registers[0x12] = static_cast<uint8_t>((PciGfxLfbBase >> 16) & 0xff);
		registers[0x13] = static_cast<uint8_t>((PciGfxLfbBase >> 24) & 0xff);

		// BAR1 - MMIO space, within first 4GB
		// Check 8-byte alignment of MMIO base
		static_assert((PciGfxMmioBase & 0xf) == 0);
		registers[0x14] = static_cast<uint8_t>(PciGfxMmioBase & 0xff);
		registers[0x15] = static_cast<uint8_t>((PciGfxMmioBase >> 8) & 0xff);
		registers[0x16] = static_cast<uint8_t>((PciGfxMmioBase >> 16) & 0xff);
		registers[0x17] = static_cast<uint8_t>((PciGfxMmioBase >> 24) & 0xff);

		return true;
	}
};

void PCI_AddSVGAS3_Device() {
	PCI_AddDevice(new PCI_VGADevice());
}

