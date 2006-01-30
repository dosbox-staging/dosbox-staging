/*
 *  Copyright (C) 2002-2005  The DOSBox Team
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "dosbox.h"
#include "inout.h"
#include "vga.h"

void SVGA_S3_WriteCRTC(Bitu reg,Bitu val,Bitu iolen) {
	switch (reg) {
	case 0x31:	/* CR31 Memory Configuration */
//TODO Base address
		vga.s3.reg_31 = val;	
		VGA_DetermineMode();
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
	case 0x40:  /* CR40 System Config */
		vga.s3.reg_40 = val;
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
	case 0x45:  /* Hardware cursor mode */
		vga.s3.hgc.curmode = val;
		// Activate hardware cursor code if needed
		VGA_ActivateHardwareCursor();
		break;
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
		vga.s3.hgc.startaddr = vga.s3.hgc.startaddr | ((val & 0xff) << 8);
		break;
	case 0x4d:  /* HGC start address low byte*/
		vga.s3.hgc.startaddr = vga.s3.hgc.startaddr | (val & 0xff);
		break;
	case 0x4e:  /* HGC pattern start X */
		vga.s3.hgc.posx = val;
		break;
	case 0x4f:  /* HGC pattern start X */
		vga.s3.hgc.posy = val;
		break;
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
	case 0x53:
		if((val & 0x10) != (vga.s3.ext_mem_ctrl & 0x10)) {
			/* Map or unmap MMIO */
			if ((val & 0x10) != 0) {
				//LOG_MSG("VGA: Mapping Memory Mapped I/O to 0xA0000");
//				VGA_MapMMIO();
			} else {
//				VGA_UnmapMMIO();
			}
		}
		vga.s3.ext_mem_ctrl = val;
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
		LOG(LOG_VGAMISC,LOG_NORMAL)("VGA:SEQ:Write to illegal index %2X", reg );
		break;
	}
}

Bitu SVGA_S3_ReadCRTC( Bitu reg, Bitu iolen) {
	switch (reg) {
	case 0x2d:	/* Extended Chip ID. */
		return 0x88;
		//	Always 88h ?
	case 0x2e:	/* New Chip ID */
		return 0x11;		
		//Trio 64 id
	case 0x2f:	/* Revision */
		return 0x00;
	case 0x30:	/* CR30 Chip ID/REV register */
		return 0xe0;		//Trio+ dual byte
		// Trio32/64 has 0xe0. extended
	case 0x31:	/* CR31 Memory Configuration */
//TODO mix in bits from baseaddress;
		return 	vga.s3.reg_31;	
	case 0x35:	/* CR35 CRT Register Lock */
		return vga.s3.reg_35|(vga.s3.bank & 0xf);
	case 0x36: /* CR36 Reset State Read 1 */
		//return 0x8f;
		return 0x8e; /* PCI version */
		//2 Mb PCI and some bios settings
	case 0x37: /* Reset state read 2 */
		return 0x2b;
	case 0x38: /* CR38 Register Lock 1 */
		return vga.s3.reg_lock1;
	case 0x39: /* CR39 Register Lock 2 */
		return vga.s3.reg_lock2;
	case 0x40: /* CR40 system config */
		return vga.s3.reg_40;
	case 0x43:	/* CR43 Extended Mode */
		return vga.s3.reg_43|((vga.config.scan_len>>6)&0x4);
	case 0x45:  /* Hardware cursor mode */
		vga.s3.hgc.bstackpos = 0;
		vga.s3.hgc.fstackpos = 0;
		return vga.s3.hgc.curmode;
	case 0x51:	/* Extended System Control 2 */
		return ((vga.config.display_start >> 16) & 3 ) |
				((vga.s3.bank & 0x30) >> 2) |
				((vga.config.scan_len & 0x300) >> 4) |
				vga.s3.reg_51;
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
		return 0x00;
	}
}

void SVGA_S3_WriteSEQ(Bitu reg,Bitu val,Bitu iolen) {
	switch (reg) {
	case 0x08:
		vga.s3.pll.lock=val;
		break;
	case 0x10:		/* Memory PLL Data Low */
		vga.s3.mclk.n=val & 0x1f;
		vga.s3.mclk.r=val >> 5;
		break;
	case 0x11:		/* Memory PLL Data High */
		vga.s3.mclk.m=val & 0x7f;
		break;
	case 0x12:		/* Video PLL Data Low */
		vga.s3.clk[3].n=val & 0x1f;
		vga.s3.clk[3].r=val >> 5;
		break;
	case 0x13:		/* Video PLL Data High */
		vga.s3.clk[3].m=val & 0x7f;
		break;
	case 0x15:
		vga.s3.pll.cmd=val;
		VGA_StartResize();
		break;
	default:
		LOG(LOG_VGAMISC,LOG_NORMAL)("VGA:S3:SEQ:Write to illegal index %2X", reg );
		break;
	}
}

Bitu SVGA_S3_ReadSEQ(Bitu reg,Bitu iolen) {
	/* S3 specific group */
	switch (reg) {
	case 0x08:		/* PLL Unlock */
		return vga.s3.pll.lock;
	case 0x10:		/* Memory PLL Data Low */
		return vga.s3.mclk.n || (vga.s3.mclk.r << 5);
	case 0x11:		/* Memory PLL Data High */
		return vga.s3.mclk.m;
	case 0x12:		/* Video PLL Data Low */
		return vga.s3.clk[3].n || (vga.s3.clk[3].r << 5);
	case 0x13:		/* Video Data High */
		return vga.s3.clk[3].m;
	case 0x15:
		return vga.s3.pll.cmd;
	default:
		LOG(LOG_VGAMISC,LOG_NORMAL)("VGA:S3:SEQ:Read from illegal index %2X", reg);
		return 0;
	}
}

Bitu SVGA_S3_GetClock(void) {
	Bitu clock = (vga.misc_output >> 2) & 3;
	if (clock == 0)
		clock = 25175000;
	else if (clock == 1)
		clock = 28322000;
	else 
		clock=1000*S3_CLOCK(vga.s3.clk[clock].m,vga.s3.clk[clock].n,vga.s3.clk[clock].r);
	return clock;
}