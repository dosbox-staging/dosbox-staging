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

#include <string.h>
#include "dosbox.h"
#include "inout.h"
#include "vga.h"

static void write_crtc_index_other(Bitu port,Bitu val,Bitu iolen) {
	vga.other.index=val;
}

static Bitu read_crtc_index_other(Bitu port,Bitu iolen) {
	return vga.other.index;
}

static void write_crtc_data_other(Bitu port,Bitu val,Bitu iolen) {
	switch (vga.other.index) {
	case 0x00:		//Horizontal total
		if (vga.other.htotal ^ val) VGA_StartResize();
		vga.other.htotal=val;
		break;
	case 0x01:		//Horizontal displayed chars
		if (vga.other.hdend ^ val) VGA_StartResize();
		vga.other.hdend=val;
		break;
	case 0x02:		//Horizontal sync position
		vga.other.hsyncp=val;
		break;
	case 0x03:		//Horizontal sync width
		vga.other.hsyncw=val;
		break;
	case 0x04:		//Vertical total
		if (vga.other.vtotal ^ val) VGA_StartResize();
		vga.other.vtotal=val;
		break;
	case 0x05:		//Vertical display adjust
		if (vga.other.vadjust ^ val) VGA_StartResize();
		vga.other.vadjust=val;
		break;
	case 0x06:		//Vertical rows
		if (vga.other.vdend ^ val) VGA_StartResize();
		vga.other.vdend=val;
		break;
	case 0x07:		//Vertical sync position
		vga.other.vsyncp=val;
		break;
	case 0x09:		//Max scanline
		if (vga.other.max_scanline ^ val) VGA_StartResize();
		vga.other.max_scanline=val;
		break;
		vga.config.display_start=(vga.config.display_start & 0xFF00FF)| (val << 8);
		break;
	case 0x0C:	/* Start Address High Register */
		vga.config.display_start=(vga.config.display_start & 0xFF00FF)| (val << 8);
		break;
	case 0x0D:	/* Start Address Low Register */
		vga.config.display_start=(vga.config.display_start & 0xFFFF00)| val;
		break;
	case 0x0E:	/*Cursor Location High Register */
		vga.config.cursor_start&=0xff00ff;
		vga.config.cursor_start|=val << 8;
		break;
	case 0x0F:	/* Cursor Location Low Register */
		vga.config.cursor_start&=0xffff00;
		vga.config.cursor_start|=val;
		break;
	default:
		LOG_MSG("Write %X to illgal index %x",val,vga.other.index);
	}
}
static Bitu read_crtc_data_other(Bitu port,Bitu iolen) {
	switch (vga.other.index) {
	case 0x00:		//Horizontal total
		return vga.other.htotal;
	case 0x01:		//Horizontal displayed chars
		return vga.other.hdend;
	case 0x02:		//Horizontal sync position
		return vga.other.hsyncp;
	case 0x03:		//Horizontal sync width
		return vga.other.hsyncw;
	case 0x04:		//Vertical total
		return vga.other.vtotal;
	case 0x05:		//Vertical display adjust
		return vga.other.vadjust;
	case 0x06:		//Vertical rows
		return vga.other.vdend;
	case 0x07:		//Vertical sync position
		return vga.other.vsyncp;
	case 0x09:		//Max scanline
		return vga.other.max_scanline;
	case 0x0C:	/* Start Address High Register */
		return vga.config.display_start >> 8;
	case 0x0D:	/* Start Address Low Register */
		return vga.config.display_start;
	case 0x0E:	/*Cursor Location High Register */
		return vga.config.cursor_start>>8;
	case 0x0F:	/* Cursor Location Low Register */
		return vga.config.cursor_start;
	default:
		LOG_MSG("Read from illgal index %x",vga.other.index);
	}
	return (Bitu)-1;
}

static void write_color_select(Bit8u val) {
	vga.tandy.color_select=val;
	switch (vga.mode) {
	case M_TANDY2:
		VGA_SetCGA2Table(0,val & 0xf);
		break;
	case M_TANDY4:
		{
			if (machine == MCH_TANDY && (vga.tandy.gfx_control & 0x8)) {
				VGA_SetCGA4Table(0,1,2,3);
				return;
			}
			Bit8u base=(val & 0x10) ? 0x08 : 0;
			/* Check for BW Mode */
			if (vga.tandy.mode_control & 0x4) {
				if (val & 0x20) VGA_SetCGA4Table(val & 0xf,3+base,4+base,7+base);
				else VGA_SetCGA4Table(val & 0xf,2+base,4+base,6+base);
			} else {
				if (val & 0x20) VGA_SetCGA4Table(val & 0xf,3+base,5+base,7+base);
				else VGA_SetCGA4Table(val & 0xf,2+base,4+base,6+base);
			}
		}
		break;
	case M_TEXT:
	case M_TANDY16:
		break;
	}
}

static void write_mode_control(Bit8u val) {
	/* Check if someone changes the blinking/hi intensity bit */
	vga.tandy.mode_control=val;
	VGA_SetBlinking((val & 0x20));
	if (val & 0x2) {
		if (val & 0x10) {
		} else VGA_SetMode(M_CGA4);
		write_color_select(vga.tandy.color_select);	//Setup the correct palette
	} else {
		VGA_SetMode(M_TEXT);
	}
}

static void TANDY_FindMode(void) {
	if (vga.tandy.mode_control & 0x2) {
		if (vga.tandy.gfx_control & 0x10) VGA_SetMode(M_TANDY16);
		else if (vga.tandy.gfx_control & 0x08) VGA_SetMode(M_TANDY4);
		else if (vga.tandy.mode_control & 0x10) VGA_SetMode(M_TANDY2);
		else VGA_SetMode(M_TANDY4);
		write_color_select(vga.tandy.color_select);
	} else {
		VGA_SetMode(M_TANDY_TEXT);
	}
}

static void write_tandy_reg(Bit8u val) {
	switch (vga.tandy.reg_index) {
	case 0x2:	/* Border color */
		vga.tandy.border_color=val;
		break;
	case 0x3:	/* More control */
		vga.tandy.gfx_control=val;
		TANDY_FindMode();
		break;
	/* palette colors */
	case 0x10: case 0x11: case 0x12: case 0x13:
	case 0x14: case 0x15: case 0x16: case 0x17:
	case 0x18: case 0x19: case 0x1a: case 0x1b:
	case 0x1c: case 0x1d: case 0x1e: case 0x1f:
		VGA_ATTR_SetPalette(vga.tandy.reg_index-0x10,val & 0xf);
		break;
	default:		
		LOG(LOG_VGAMISC,LOG_NORMAL)("Unhandled Write %2X to tandy reg %X",val,vga.tandy.reg_index);
	}
}

static void write_cga(Bitu port,Bitu val,Bitu iolen) {
	switch (port) {
	case 0x3d8:
		vga.tandy.mode_control=val;
		if (vga.tandy.mode_control & 0x2) {
			if (vga.tandy.mode_control & 0x10) {
				if (val & 0x8 && machine==MCH_CGA) {
					VGA_SetMode(M_CGA16);		//Video burst 16 160x200 color mode
				} else {
					VGA_SetMode(M_TANDY2);
				}
			} else VGA_SetMode(M_TANDY4);
			write_color_select(vga.tandy.color_select);
		} else {
			VGA_SetMode(M_TANDY_TEXT);
		}
		VGA_SetBlinking(val & 0x20);
		break;
	case 0x3d9:
		write_color_select(val);
		break;
	}
}

static void write_tandy(Bitu port,Bitu val,Bitu iolen) {
	switch (port) {
	case 0x3d8:
		vga.tandy.mode_control=val;
		VGA_SetBlinking(val & 0x20);
		TANDY_FindMode();
		break;
	case 0x3d9:
		write_color_select(val);
		break;
	case 0x3da:
		vga.tandy.reg_index=val;
		break;
	case 0x3de:
		write_tandy_reg(val);
		break;
	case 0x3df:
		vga.tandy.disp_bank=val & ((val & 0x80) ? 0x6 : 0x7);
		vga.tandy.mem_bank=(val >> 3) & ((val & 0x80) ? 0x6 : 0x7);
		VGA_SetupHandlers();
		break;
	}
}

static void write_hercules(Bitu port,Bitu val,Bitu iolen) {
	switch (port) {
	case 0x3b8:
		if (vga.herc.enable_bits & 1) {
			vga.herc.mode_control&=~0x2;
			vga.herc.mode_control|=(val&0x2);
			if (val & 0x2) {
				VGA_SetMode(M_HERC_GFX);
			} else {
				VGA_SetMode(M_HERC_TEXT);
			}
		}
		if ((vga.herc.enable_bits & 0x2) && ((vga.herc.mode_control ^ val)&0x80)) {
			vga.herc.mode_control^=0x80;
			VGA_SetupHandlers();
		}
		break;
	case 0x3bf:
		vga.herc.enable_bits=val;
		break;
	}
}

static Bitu read_hercules(Bitu port,Bitu iolen) {
	LOG_MSG("read from Herc port %x",port);
	return 0;
}


void VGA_SetupOther(void) {
	Bitu i;
	if (machine==MCH_CGA || machine==MCH_TANDY) {
		extern Bit8u int10_font_08[256 * 8];
		for (i=0;i<256;i++)	memcpy(&vga.draw.font[i*32],&int10_font_08[i*8],8);
		vga.draw.font_tables[0]=vga.draw.font_tables[1]=vga.draw.font;
	}
	if (machine==MCH_HERC) {
		extern Bit8u int10_font_14[256 * 14];
		for (i=0;i<256;i++)	memcpy(&vga.draw.font[i*32],&int10_font_14[i*14],14);
		vga.draw.font_tables[0]=vga.draw.font_tables[1]=vga.draw.font;
	}
	if (machine==MCH_CGA) {
		IO_RegisterWriteHandler(0x3d8,write_cga,IO_MB);
		IO_RegisterWriteHandler(0x3d9,write_cga,IO_MB);
	}
	if (machine==MCH_HERC) {
		vga.herc.enable_bits=0;
		vga.herc.mode_control=0x8;
		IO_RegisterWriteHandler(0x3b8,write_hercules,IO_MB);
		IO_RegisterWriteHandler(0x3bf,write_hercules,IO_MB);
	}
	if (machine==MCH_TANDY) {
		IO_RegisterWriteHandler(0x3d8,write_tandy,IO_MB);
		IO_RegisterWriteHandler(0x3d9,write_tandy,IO_MB);
		IO_RegisterWriteHandler(0x3de,write_tandy,IO_MB);
		IO_RegisterWriteHandler(0x3df,write_tandy,IO_MB);
		IO_RegisterWriteHandler(0x3da,write_tandy,IO_MB);
	}
	if (machine==MCH_CGA || machine==MCH_HERC || machine==MCH_TANDY) {
		Bitu base=machine==MCH_HERC ? 0x3b4 : 0x3d4;
		IO_RegisterWriteHandler(base,write_crtc_index_other,IO_MB);
		IO_RegisterWriteHandler(base+1,write_crtc_data_other,IO_MB);
		IO_RegisterReadHandler(base,read_crtc_index_other,IO_MB);
		IO_RegisterReadHandler(base+1,read_crtc_data_other,IO_MB);
	}

}

