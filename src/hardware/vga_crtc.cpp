/*
 *  Copyright (C) 2002-2004  The DOSBox Team
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

#include "dosbox.h"
#include "inout.h"
#include "vga.h"
#include "debug.h"
#include "cpu.h"

#define crtc(blah) vga.crtc.blah

void write_p3d4_vga(Bitu port,Bitu val,Bitu iolen) {
	crtc(index)=val;
}

Bitu read_p3d4_vga(Bitu port,Bitu iolen) {
	return crtc(index);
}

void write_p3d5_vga(Bitu port,Bitu val,Bitu iolen) {
//	if (crtc(index)>0x18) LOG_MSG("VGA CRCT write %X to reg %X",val,crtc(index));
	switch(crtc(index)) {
	case 0x00:	/* Horizontal Total Register */
		if (crtc(read_only)) break;
		crtc(horizontal_total)=val;
		/* 	0-7  Horizontal Total Character Clocks-5 */
		break;
	case 0x01:	/* Horizontal Display End Register */
		if (crtc(read_only)) break;
		if (val != crtc(horizontal_display_end)) {
			crtc(horizontal_display_end)=val;
			VGA_StartResize();
		}
		/* 	0-7  Number of Character Clocks Displayed -1 */
		break;
	case 0x02:	/* Start Horizontal Blanking Register */
		if (crtc(read_only)) break;
		crtc(start_horizontal_blanking)=val;
		/*	0-7  The count at which Horizontal Blanking starts */
		break;
	case 0x03:	/* End Horizontal Blanking Register */
		if (crtc(read_only)) break;
		crtc(end_horizontal_blanking)=val;
		/*
			0-4	Horizontal Blanking ends when the last 6 bits of the character
				counter equals this field. Bit 5 is at 3d4h index 5 bit 7.
			5-6	Number of character clocks to delay start of display after Horizontal
				Total has been reached.
			7	Access to Vertical Retrace registers if set. If clear reads to 3d4h
				index 10h and 11h access the Lightpen read back registers ??
		*/
		break;
	case 0x04:	/* Start Horizontal Retrace Register */
		if (crtc(read_only)) break;
		crtc(start_horizontal_retrace)=val;
		/*	0-7  Horizontal Retrace starts when the Character Counter reaches this value. */
		break;
	case 0x05:	/* End Horizontal Retrace Register */
		if (crtc(read_only)) break;
		crtc(end_horizontal_retrace)=val;
		/*
			0-4	Horizontal Retrace ends when the last 5 bits of the character counter
				equals this value.
			5-6	Number of character clocks to delay start of display after Horizontal
				Retrace.
			7	bit 5 of the End Horizontal Blanking count (See 3d4h index 3 bit 0-4)
		*/	
		break;
	case 0x06: /* Vertical Total Register */
		if (crtc(read_only)) break;
		if (val != crtc(vertical_total)) {
			crtc(vertical_total)=val;	
			VGA_StartResize();
		}
		/*	0-7	Lower 8 bits of the Vertical Total. Bit 8 is found in 3d4h index 7
				bit 0. Bit 9 is found in 3d4h index 7 bit 5.
			Note: For the VGA this value is the number of scan lines in the display -2.
		*/
		break;
	case 0x07:	/* Overflow Register */
		//Line compare bit ignores read only */
		vga.config.line_compare=(vga.config.line_compare & 0x6ff) | (val & 0x10) << 4;
		if (crtc(read_only)) break;
		if ((vga.crtc.overflow ^ val) & 0xd6) {
			crtc(overflow)=val;
			VGA_StartResize();
		} else crtc(overflow)=val;
		/*
			0  Bit 8 of Vertical Total (3d4h index 6)
			1  Bit 8 of Vertical Display End (3d4h index 12h)
			2  Bit 8 of Vertical Retrace Start (3d4h index 10h)
			3  Bit 8 of Start Vertical Blanking (3d4h index 15h)
			4  Bit 8 of Line Compare Register (3d4h index 18h)
			5  Bit 9 of Vertical Total (3d4h index 6)
			6  Bit 9 of Vertical Display End (3d4h index 12h)
			7  Bit 9 of Vertical Retrace Start (3d4h index 10h)
		*/
		break;
	case 0x08:	/* Preset Row Scan Register */
		crtc(preset_row_scan)=val;
		vga.config.hlines_skip=val&31;
		vga.config.bytes_skip=(val>>5)&3;
//		LOG_DEBUG("Skip lines %d bytes %d",vga.config.hlines_skip,vga.config.bytes_skip);
		/*
			0-4	Number of lines we have scrolled down in the first character row.
				Provides Smooth Vertical Scrolling.b
			5-6	Number of bytes to skip at the start of scanline. Provides Smooth
				Horizontal Scrolling together with the Horizontal Panning Register
				(3C0h index 13h).
		*/
		break;
	case 0x09: /* Maximum Scan Line Register */
		vga.config.line_compare=(vga.config.line_compare & 0x5ff)|(val&0x40)<<3;
		if ((vga.crtc.maximum_scan_line ^ val) & 0xbf) {
			crtc(maximum_scan_line)=val;
			VGA_StartResize();
		} else crtc(maximum_scan_line)=val;
		/*
			0-4	Number of scan lines in a character row -1. In graphics modes this is
				the number of times (-1) the line is displayed before passing on to
				the next line (0: normal, 1: double, 2: triple...).
				This is independent of bit 7, except in CGA modes which seems to
				require this field to be 1 and bit 7 to be set to work.
			5	Bit 9 of Start Vertical Blanking
			6	Bit 9 of Line Compare Register
			7	Doubles each scan line if set. I.e. displays 200 lines on a 400 display.
		*/
		break;
	case 0x0A:	/* Cursor Start Register */
		crtc(cursor_start)=val;
		vga.draw.cursor.sline=val&0x1f;
		vga.draw.cursor.enabled=!(val&0x20);
		/*
			0-4	First scanline of cursor within character.
			5	Turns Cursor off if set
		*/
		break;
	case 0x0B:	/* Cursor End Register */
		crtc(cursor_end)=val;
		vga.draw.cursor.eline=val&0x1f;
		vga.draw.cursor.delay=(val>>5)&0x3;

		/* 
			0-4	Last scanline of cursor within character
			5-6	Delay of cursor data in character clocks.
		*/
		break;
	case 0x0C:	/* Start Address High Register */
		crtc(start_address_high)=val;
		vga.config.display_start=(vga.config.display_start & 0xFF00FF)| (val << 8);
		/* 0-7  Upper 8 bits of the start address of the display buffer */
		break;
	case 0x0D:	/* Start Address Low Register */
		crtc(start_address_low)=val;
		vga.config.display_start=(vga.config.display_start & 0xFFFF00)| val;
		/*	0-7	Lower 8 bits of the start address of the display buffer */
		break;
	case 0x0E:	/*Cursor Location High Register */
		crtc(cursor_location_high)=val;
		vga.config.cursor_start&=0xff00ff;
		vga.config.cursor_start|=val << 8;
		/*	0-7  Upper 8 bits of the address of the cursor */
		break;
	case 0x0F:	/* Cursor Location Low Register */
//TODO update cursor on screen
		crtc(cursor_location_low)=val;
		vga.config.cursor_start&=0xffff00;
		vga.config.cursor_start|=val;
		/*	0-7  Lower 8 bits of the address of the cursor */
		break;
	case 0x10:	/* Vertical Retrace Start Register */
		crtc(vertical_retrace_start)=val;
		/*	
			0-7	Lower 8 bits of Vertical Retrace Start. Vertical Retrace starts when
			the line counter reaches this value. Bit 8 is found in 3d4h index 7
			bit 2. Bit 9 is found in 3d4h index 7 bit 7.
		*/
		break;
	case 0x11:	/* Vertical Retrace End Register */
		crtc(vertical_retrace_end)=val;
		crtc(read_only)=(val & 128)>0;
		/*
			0-3	Vertical Retrace ends when the last 4 bits of the line counter equals
				this value.
			4	if clear Clears pending Vertical Interrupts.
			5	Vertical Interrupts (IRQ 2) disabled if set. Can usually be left
				disabled, but some systems (including PS/2) require it to be enabled.
			6	If set selects 5 refresh cycles per scanline rather than 3.
			7	Disables writing to registers 0-7 if set 3d4h index 7 bit 4 is not
				affected by this bit.
		*/
		break;
	case 0x12:	/* Vertical Display End Register */
		if (val!=crtc(vertical_display_end)) {
			crtc(vertical_display_end)=val;
			VGA_StartResize();
		}
		/*
			0-7	Lower 8 bits of Vertical Display End. The display ends when the line
				counter reaches this value. Bit 8 is found in 3d4h index 7 bit 1.
			Bit 9 is found in 3d4h index 7 bit 6.
		*/
		break;
	case 0x13:	/* Offset register */
		crtc(offset)=val;
		vga.config.scan_len&=0x300;
		vga.config.scan_len|=val;
		VGA_CheckScanLength();
		/*
			0-7	Number of bytes in a scanline / K. Where K is 2 for byte mode, 4 for
				word mode and 8 for Double Word mode.
		*/
		break;
	case 0x14:	/* Underline Location Register */
		crtc(underline_location)=val;
		/*
			0-4	Position of underline within Character cell.
			5	If set memory address is only changed every fourth character clock.
			6	Double Word mode addressing if set
		*/
		break;
	case 0x15:	/* Start Vertical Blank Register */
		if (val!=crtc(start_vertical_blanking)) {
			crtc(start_vertical_blanking)=val;
			VGA_StartResize();
		}
		/* 
			0-7	Lower 8 bits of Vertical Blank Start. Vertical blanking starts when
				the line counter reaches this value. Bit 8 is found in 3d4h index 7
				bit 3.
		*/
		break;
	case 0x16:	/*  End Vertical Blank Register */
		crtc(end_vertical_blanking)=val;
		 /*
			0-6	Vertical blanking stops when the lower 7 bits of the line counter
				equals this field. Some SVGA chips uses all 8 bits!
		*/
		break;
	case 0x17:	/* Mode Control Register */
		crtc(mode_control)=val;
		VGA_DetermineMode();
		/*
			0	If clear use CGA compatible memory addressing system
				by substituting character row scan counter bit 0 for address bit 13,
				thus creating 2 banks for even and odd scan lines.
			1	If clear use Hercules compatible memory addressing system by
				substituting character row scan counter bit 1 for address bit 14,
				thus creating 4 banks.
			2	If set increase scan line counter only every second line.
			3	If set increase memory address counter only every other character clock.
			5	When in Word Mode bit 15 is rotated to bit 0 if this bit is set else
				bit 13 is rotated into bit 0.
			6	If clear system is in word mode. Addresses are rotated 1 position up
				bringing either bit 13 or 15 into bit 0.
			7	Clearing this bit will reset the display system until the bit is set again.
		*/
		break;
	case 0x18:	/* Line Compare Register */
		crtc(line_compare)=val;
		vga.config.line_compare=(vga.config.line_compare & 0x700) | val;
		/*
			0-7	Lower 8 bits of the Line Compare. When the Line counter reaches this
				value, the display address wraps to 0. Provides Split Screen
				facilities. Bit 8 is found in 3d4h index 7 bit 4.
				Bit 9 is found in 3d4h index 9 bit 6.
		*/
		break;
/* S3 specific group */
	case 0x31:	/* CR31 Memory Configuration */
//TODO Base address
		vga.s3.reg_31=val;	
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
		if ((vga.s3.bank & 0xf) ^ (val & 0xf)) {
			vga.s3.bank&=0xf0;
			vga.s3.bank|=val & 0xf;
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
	case 0x51:	/* Extended System Control 2 */
		vga.s3.reg_51=val & 0xc0;		//Only store bits 6,7
		//TODO Display start
		vga.config.display_start&=0xFCFFFF;
		vga.config.display_start|=(val & 3) << 16;
		if ((vga.s3.bank&0xcf) ^ ((val&0xc)<<2)) {
			vga.s3.bank&=0xcf;
			vga.s3.bank|=(val&0xc)<<2;
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
		if ((val & vga.s3.ex_hor_overflow) ^ 3) {
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
	case 0x67:	/* Extended Miscellaneous Control 2 */
		/*
			0	VCLK PHS. VCLK Phase With Respect to DCLK. If clear VLKC is inverted
				DCLK, if set VCLK = DCLK.
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
		vga.s3.bank=val & 0x3f;
		VGA_SetupHandlers();
		break;
	
	default:
		LOG(LOG_VGAMISC,LOG_NORMAL)("VGA:CRTC:Write %X to unknown index %2X",val,crtc(index));
	}
}

Bitu read_p3d5_vga(Bitu port,Bitu iolen) {
//	LOG_MSG("VGA CRCT read from reg %X",crtc(index));
	switch(crtc(index)) {
	case 0x00:	/* Horizontal Total Register */
		return crtc(horizontal_total);
	case 0x01:	/* Horizontal Display End Register */
		return crtc(horizontal_display_end);
	case 0x02:	/* Start Horizontal Blanking Register */
		return crtc(start_horizontal_blanking);
	case 0x03:	/* End Horizontal Blanking Register */
		return crtc(end_horizontal_blanking);
	case 0x04:	/* Start Horizontal Retrace Register */
		return crtc(start_horizontal_retrace);
	case 0x05:	/* End Horizontal Retrace Register */
		return crtc(end_horizontal_retrace);
	case 0x06: /* Vertical Total Register */
		return crtc(vertical_total);	
	case 0x07:	/* Overflow Register */
		return crtc(overflow);
	case 0x08:	/* Preset Row Scan Register */
		return crtc(preset_row_scan);
	case 0x09: /* Maximum Scan Line Register */
		return crtc(maximum_scan_line);
	case 0x0A:	/* Cursor Start Register */
		return crtc(cursor_start);
	case 0x0B:	/* Cursor End Register */
		return crtc(cursor_end);
	case 0x0C:	/* Start Address High Register */
		return crtc(start_address_high);
	case 0x0D:	/* Start Address Low Register */
		return crtc(start_address_low);
	case 0x0E:	/*Cursor Location High Register */
		return crtc(cursor_location_high);
	case 0x0F:	/* Cursor Location Low Register */
		return crtc(cursor_location_low);
	case 0x10:	/* Vertical Retrace Start Register */
		return crtc(vertical_retrace_start);
	case 0x11:	/* Vertical Retrace End Register */
		return crtc(vertical_retrace_end);
	case 0x12:	/* Vertical Display End Register */
		return crtc(vertical_display_end);
	case 0x13:	/* Offset register */
		return crtc(offset);
	case 0x14:	/* Underline Location Register */
		return crtc(underline_location);
	case 0x15:	/* Start Vertical Blank Register */
		return crtc(start_vertical_blanking);
	case 0x16:	/*  End Vertical Blank Register */
		return crtc(end_vertical_blanking);
	case 0x17:	/* Mode Control Register */
		return crtc(mode_control);
	case 0x18:	/* Line Compare Register */
		return crtc(line_compare);

	
/* Additions for S3 SVGA Support */
	case 0x2d:	/* Extended Chip ID. */
		return 0x88;
		//	Always 88h ?
	case 0x2e:	/* New Chip ID */
		return 0x11;		
		//Trio 64 id
	case 0x2f:	/* Revision */
		return 0x80;
	case 0x30:	/* CR30 Chip ID/REV register */
		return 0xe0;		//Trio+ dual byte
		// Trio32/64 has 0xe0. extended
	case 0x31:	/* CR31 Memory Configuration */
//TODO mix in bits from baseaddress;
		return 	vga.s3.reg_31;	
	case 0x35:	/* CR35 CRT Register Lock */
		return vga.s3.reg_35|(vga.s3.bank & 0xf);
	case 0x36: /* CR36 Reset State Read 1 */
		return 0x8f;
		//2 Mb PCI and some bios settings
	case 0x37: /* Reset state read 2 */
		return 0x2b;
	case 0x38: /* CR38 Register Lock 1 */
		return vga.s3.reg_lock1;
	case 0x39: /* CR39 Register Lock 2 */
		return vga.s3.reg_lock2;
	case 0x43:	/* CR43 Extended Mode */
		return vga.s3.reg_43|((vga.config.scan_len>>6)&0x4);
	case 0x51:	/* Extended System Control 2 */
		return ((vga.config.display_start >> 16) & 3 ) |
				((vga.s3.bank & 0x30) >> 2) |
				((vga.config.scan_len & 0x300) >> 4) |
				vga.s3.reg_51;
	case 0x55:	/* Extended Video DAC Control */
		return vga.s3.reg_55;
	case 0x58:	/* Linear Address Window Control */
		return	vga.s3.reg_58;
	case 0x5D:	/* Extended Horizontal Overflow */
		return vga.s3.ex_hor_overflow;
	case 0x5e:	/* Extended Vertical Overflow */
		return vga.s3.ex_ver_overflow;
	case 0x67:	/* Extended Miscellaneous Control 2 */		
		return vga.s3.misc_control_2;
	case 0x69:	/* Extended System Control 3 */
		return (Bit8u)((vga.config.display_start & 0x1f0000)>>16); 
	case 0x6a:	/* Extended System Control 4 */
		return (Bit8u)(vga.s3.bank & 0x3f);
	default:
		LOG(LOG_VGAMISC,LOG_NORMAL)("VGA:CRTC:Read from unknown index %X",crtc(index));
	}
	return 0x0;
}




