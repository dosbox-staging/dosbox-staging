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
#include "pic.h"
#include "vga.h"
#include "../ints/int10.h"

static Bit8u flip=0;

void write_p3d4(Bit32u port,Bit8u val);
Bit8u read_p3d4(Bit32u port);
void write_p3d5(Bit32u port,Bit8u val);
Bit8u read_p3d5(Bit32u port);

static void write_p3d9(Bit32u port,Bit8u val);

static Bit8u read_p3da(Bit32u port) {
	vga.internal.attrindex=false;
	vga.tandy.set_reg=true;
	if (vga.config.retrace) {
		switch (vga.mode) {
		case M_HERC:
			 return 0x81;
		case M_TEXT2:
			if (machine==MCH_HERC) return 0x81;
			if (machine==MCH_AUTO) return 0x89;
		default:
			return 9;
		}
	}
	flip++;
	if (flip>20) flip=0;
	if (flip>10) return 1;
	return 0;
	/*
		0	Either Vertical or Horizontal Retrace active if set
		3	Vertical Retrace in progress if set
	*/
}


static void write_p3d8(Bit32u port,Bit8u val) {
	switch (machine) {
	case MCH_CGA:
		goto cga_3d8;
	};
	switch (vga.mode) {
	case M_CGA16:
	case M_CGA4:
	case M_CGA2:
cga_3d8:
		if (val & 0x2) {
			if (val & 0x10) {
				if (val & 0x8) {
					VGA_SetMode(M_CGA16);		//Video burst 16 160x200 color mode
				} else {
					VGA_SetMode(M_CGA2);
				}
			} else VGA_SetMode(M_CGA4);
			write_p3d9(0x3d9,vga.cga.color_select);	//Setup the correct palette
		} else {
			VGA_SetMode(M_TEXT16);
		}
		break;
	default:
		LOG(LOG_VGAMISC,LOG_NORMAL)("Write %2X to 3d8 in mode %d",val,vga.mode);
		break;
	}
	/*
		3	Vertical Sync Select. If set Vertical Sync to the monitor is the
			logical OR of the vertical sync and the vertical display enable.
	*/
}

static void write_p3d9(Bit32u port,Bit8u val) {
	Bitu i;
	vga.cga.color_select=val;
	switch (vga.mode) {
	case M_CGA2:
		/* changes attribute 1 */
		VGA_ATTR_SetPalette(0,0);
		VGA_ATTR_SetPalette(1,val & 0xf);
		break;
	case M_CGA4:
			/* changes attribute 0 */
		{
			VGA_ATTR_SetPalette(0,(val & 0xf));
			Bit8u pal_base=(val & 0x10) ? 0x08 : 0;
			if (val & 0x020) {
				VGA_ATTR_SetPalette(1,0x03+pal_base);
				VGA_ATTR_SetPalette(2,0x05+pal_base);
				VGA_ATTR_SetPalette(3,0x07+pal_base);
			} else {
				VGA_ATTR_SetPalette(1,0x02+pal_base);
				VGA_ATTR_SetPalette(2,0x04+pal_base);
				VGA_ATTR_SetPalette(3,0x06+pal_base);
			}
		}
		break;
	case M_CGA16:
		for(i=0;i<0x10;i++) VGA_ATTR_SetPalette(i,i);
		break;
	default:
		LOG(LOG_VGAMISC,LOG_NORMAL)("Unhandled Write %2X to %X in mode %d",val,port,vga.mode);
	}
}

static void write_p3df(Bit32u port,Bit8u val) {
	if (machine==MCH_TANDY) goto tandy_3df;
	switch (vga.mode) {
	case M_TANDY16:
tandy_3df:
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
	switch (machine) {
	case MCH_AUTO:
	case MCH_CGA:
	case MCH_TANDY:
		return vga.cga.color_select;
	default:
		return 0xff;
	};
}


static void write_p3c2(Bit32u port,Bit8u val) {
	vga.misc_output=val;

	if (val & 0x1) {
		IO_RegisterWriteHandler(0x3d4,write_p3d4,"VGA:CRTC Index Select");
		IO_RegisterReadHandler(0x3d4,read_p3d4,"VGA:CRTC Index Select");
		IO_RegisterWriteHandler(0x3d5,write_p3d5,"VGA:CRTC Data Register");
		IO_RegisterReadHandler(0x3d5,read_p3d5,"VGA:CRTC Data Register");
		IO_RegisterReadHandler(0x3da,read_p3da,"VGA Input Status 1");
		
		IO_FreeWriteHandler(0x3b4);
		IO_FreeReadHandler(0x3b4);
		IO_FreeWriteHandler(0x3b5);
		IO_FreeReadHandler(0x3b5);
		IO_FreeReadHandler(0x3ba);
	} else {
		IO_RegisterWriteHandler(0x3b4,write_p3d4,"VGA:CRTC Index Select");
		IO_RegisterReadHandler(0x3b4,read_p3d4,"VGA:CRTC Index Select");
		IO_RegisterWriteHandler(0x3b5,write_p3d5,"VGA:CRTC Data Register");
		IO_RegisterReadHandler(0x3b5,read_p3d5,"VGA:CRTC Data Register");
		IO_RegisterReadHandler(0x3ba,read_p3da,"VGA Input Status 1");

		IO_FreeWriteHandler(0x3d4);
		IO_FreeReadHandler(0x3d4);
		IO_FreeWriteHandler(0x3d5);
		IO_FreeReadHandler(0x3d5);
		IO_FreeReadHandler(0x3da);
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


static void write_hercules(Bit32u port,Bit8u val) {
	Bit8u mask;
	switch (port) {
	case 0x3b8:
		mask=0xff-0x80-0x2;;
		if (vga.herc.enable_bits & 1) {
			mask|=0x2;
			if (vga.mode != M_HERC || vga.mode != M_TEXT2) {
				VGA_ATTR_SetPalette(1,0x07);
				/* Force 0x3b4/5 registers */
				if (vga.misc_output & 1) write_p3c2(0,vga.misc_output & ~1);
			}
			if (val & 0x2) {
				if (vga.mode != M_HERC) VGA_SetMode(M_HERC);
			} else {
				if (vga.mode != M_TEXT2) VGA_SetMode(M_TEXT2);
			}
		}
		if (vga.herc.enable_bits & 0x2) {
			mask|=0x80;
			VGA_SetupHandlers();
		}
		vga.herc.mode_control=(vga.herc.mode_control & ~mask) | (val&mask);
		break;
	case 0x3bf:
		vga.herc.enable_bits=val;
		break;
	default:
		LOG_MSG("write %x to Herc port %x",val,port);
	}
}

static Bit8u read_hercules(Bit32u port) {
	switch (port) {
	case 0x3b8:
	default:
		LOG_MSG("read from Herc port %x",port);
	}
	return 0;
}




void VGA_SetupMisc(void) {
	vga.herc.enable_bits=0;
	IO_RegisterWriteHandler(0x3d8,write_p3d8,"VGA Feature Control Register");
	IO_RegisterWriteHandler(0x3d9,write_p3d9,"CGA Color Select Register");
	IO_RegisterReadHandler(0x3d9,read_p3d9,"CGA Color Select Register");

	IO_RegisterWriteHandler(0x3c2,write_p3c2,"VGA Misc Output");
	IO_RegisterReadHandler(0x3cc,read_p3cc,"VGA Misc Output");

	if (machine==MCH_HERC || machine==MCH_AUTO) {
		vga.herc.mode_control=0x8;
		IO_RegisterWriteHandler(0x3b8,write_hercules,"Hercules");
		IO_RegisterWriteHandler(0x3bf,write_hercules,"Hercules");
	}
	IO_RegisterWriteHandler(0x3df,write_p3df,"PCJR Setting");

}


