/*
 *  Copyright (C) 2002-2003  The DOSBox Team
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
#include "pic.h"
#include "vga.h"

static Bit8u flip=0;

void write_p3d4(Bit32u port,Bit8u val);
Bit8u read_p3d4(Bit32u port);
void write_p3d5(Bit32u port,Bit8u val);
Bit8u read_p3d5(Bit32u port);

static Bit8u read_p3da(Bit32u port) {
	vga.internal.attrindex=false;
	if (vga.config.retrace) {
		return 9;
	}
	flip++;
	if (flip>10) flip=0;
	if (flip>5)	return 1;
	return 0;
	/*
		0	Either Vertical or Horizontal Retrace active if set
		3	Vertical Retrace in progress if set
	*/
}


static void write_p3d8(Bit32u port,Bit8u val) {
	switch (vga.mode) {
	case M_CGA4:

		break;
	default:
		break;
	}
	LOG(LOG_VGAMISC,LOG_NORMAL)("Write %2X to 3d8",val);
	/*
		3	Vertical Sync Select. If set Vertical Sync to the monitor is the
			logical OR of the vertical sync and the vertical display enable.
	*/
}

static void write_p3d9(Bit32u port,Bit8u val) {
	switch (vga.mode) {
	case M_CGA2:
		vga.cga.color_select=val;
		/* changes attribute 1 */
		vga.attr.palette[1]=(val & 7) + ((val & 8) ? 0x38 : 0);
		VGA_DAC_CombineColor(1,vga.attr.palette[0]);
		break;
	case M_CGA4:
		vga.cga.color_select=val;
		/* changes attribute 0 */
		VGA_ATTR_SetPalette(0,(val & 7) + ((val & 8) ? 0x38 : 0));
		if (val & 0x020) {
			VGA_ATTR_SetPalette(1,0x13);
			VGA_ATTR_SetPalette(2,0x15);
			VGA_ATTR_SetPalette(3,0x17);
		} else {
			VGA_ATTR_SetPalette(1,0x02);
			VGA_ATTR_SetPalette(2,0x04);
			VGA_ATTR_SetPalette(3,0x06);
		}
		break;
	/*	Color Select register
		Text modes:      320x200 modes:         640x200 mode:
		0  Blue border      Blue background        Blue ForeGround
		1  Green border     Green background       Green ForeGround
		2  Red border       Red background         Red ForeGround
		3  Bright border    Bright background      Bright ForeGround
		4  Backgr. color    Alt. intens. colors    Alt. intens colors
		5  No func.         Selects palette
				Palette 0 is Green, red and brown,
				Palette 1 is Cyan, magenta and white.
	*/
	default:
		LOG(LOG_VGAMISC,LOG_NORMAL)("Unhandled Write %2X to %X in mode %d",val,port,vga.mode);
	}
}

static void write_p3df(Bit32u port,Bit8u val) {
	switch (vga.mode) {
	case M_TANDY16:
		vga.tandy.disp_bank=val & ((val & 0x80) ? 0x6 : 0x7);
		vga.tandy.mem_bank=(val >> 3) & ((val & 0x80) ? 0x6 : 0x7);
		VGA_SetupHandlers();

		break;
		/*
			0-2	Identifies the page of main memory being displayed in units of 16K.
				0: 0K, 1: 16K...7: 112K. In 32K modes (bits 6-7 = 2) only 0,2,4 and
				6 are valid, as the next page will also be used.
			3-5	Identifies the page of main memory that can be read/written at B8000h
				in units of 16K. 0: 0K, 1: 16K...7: 112K. In 32K modes (bits 6-7 = 2)
				only 0,2,4 and 6 are valid, as the next page will also be used.
			6-7	Display mode. 0: Text, 1: 16K graphics mode (4,5,6,8)
				2: 32K graphics mode (9,Ah)
		*/
	default:
		LOG(LOG_VGAMISC,LOG_NORMAL)("Unhandled Write %2X to %X in mode %d",val,port,vga.mode);
		break;
	}
}

static Bit8u read_p3d9(Bit32u port) {
	switch (vga.mode) {
	case M_CGA2:
	case M_CGA4:
		return vga.cga.color_select;
	default:
		return 0xff;
	}
}


static void write_p3c2(Bit32u port,Bit8u val) {
	vga.misc_output=val;
	if (val & 0x1) {
		IO_RegisterWriteHandler(0x3d4,write_p3d4,"VGA:CRTC Index Select");
		IO_RegisterReadHandler(0x3d4,read_p3d4,"VGA:CRTC Index Select");
		IO_RegisterWriteHandler(0x3d5,write_p3d5,"VGA:CRTC Data Register");
		IO_RegisterReadHandler(0x3d5,read_p3d5,"VGA:CRTC Data Register");
		IO_FreeWriteHandler(0x3b4);
		IO_FreeReadHandler(0x3b4);
		IO_FreeWriteHandler(0x3b5);
		IO_FreeReadHandler(0x3b5);
	} else {
		IO_RegisterWriteHandler(0x3b4,write_p3d4,"VGA:CRTC Index Select");
		IO_RegisterReadHandler(0x3b4,read_p3d4,"VGA:CRTC Index Select");
		IO_RegisterWriteHandler(0x3b5,write_p3d5,"VGA:CRTC Data Register");
		IO_RegisterReadHandler(0x3b5,read_p3d5,"VGA:CRTC Data Register");
		IO_FreeWriteHandler(0x3d4);
		IO_FreeReadHandler(0x3d4);
		IO_FreeWriteHandler(0x3d5);
		IO_FreeReadHandler(0x3d5);
	}
	/*
		0	If set Color Emulation. Base Address=3Dxh else Mono Emulation. Base Address=3Bxh.
		2-3	Clock Select. 0: 25MHz, 1: 28MHz
		5	When in Odd/Even modes Select High 64k bank if set
		6	Horizontal Sync Polarity. Negative if set
		7	Vertical Sync Polarity. Negative if set
			Bit 6-7 indicates the number of lines on the display:
			1:  400, 2: 350, 3: 480
			Note: Set to all zero on a hardware reset.
			Note: This register can be read from port 3CCh.
	*/
}


static Bit8u read_p3cc(Bit32u port) {
	return vga.misc_output;
}

void VGA_SetupMisc(void) {
	IO_RegisterReadHandler(0x3da,read_p3da,"VGA Input Status 1");
	IO_RegisterReadHandler(0x3ba,read_p3da,"VGA Input Status 1");

	IO_RegisterWriteHandler(0x3d8,write_p3d8,"VGA Feature Control Register");

	
	IO_RegisterWriteHandler(0x3d9,write_p3d9,"CGA Color Select Register");
	IO_RegisterReadHandler(0x3d9,read_p3d9,"CGA Color Select Register");
	
	
	IO_RegisterWriteHandler(0x3c2,write_p3c2,"VGA Misc Output");

	IO_RegisterReadHandler(0x3cc,read_p3cc,"VGA Misc Output");
	IO_RegisterWriteHandler(0x3df,write_p3df,"PCJR Setting");

}


