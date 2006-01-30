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
#include "render.h"
#include "vga.h"

/*
3C6h (R/W):  PEL Mask
bit 0-7  This register is anded with the palette index sent for each dot.
         Should be set to FFh.

3C7h (R):  DAC State Register
bit 0-1  0 indicates the DAC is in Write Mode and 3 indicates Read mode.

3C7h (W):  PEL Address Read Mode
bit 0-7  The PEL data register (0..255) to be read from 3C9h.
Note: After reading the 3 bytes at 3C9h this register will increment,
      pointing to the next data register.

3C8h (R/W):  PEL Address Write Mode
bit 0-7  The PEL data register (0..255) to be written to 3C9h.
Note: After writing the 3 bytes at 3C9h this register will increment, pointing
      to the next data register.

3C9h (R/W):  PEL Data Register
bit 0-5  Color value
Note:  Each read or write of this register will cycle through first the
       registers for Red, Blue and Green, then increment the appropriate
       address register, thus the entire palette can be loaded by writing 0 to
       the PEL Address Write Mode register 3C8h and then writing all 768 bytes
       of the palette to this register.
*/

enum {DAC_READ,DAC_WRITE};


static void write_p3c6(Bitu port,Bitu val,Bitu iolen) {
	if (val!=0xff) LOG(LOG_VGAGFX,LOG_NORMAL)("VGA:Pel Mask not 0xff");
	vga.dac.pel_mask=val;
}


static Bitu read_p3c6(Bitu port,Bitu iolen) {
	return vga.dac.pel_mask;
}


static void write_p3c7(Bitu port,Bitu val,Bitu iolen) {
	vga.dac.read_index=val;
	vga.dac.pel_index=0;
	vga.dac.state=DAC_READ;
	vga.dac.write_index= val + 1;
}

static Bitu read_p3c7(Bitu port,Bitu iolen) {
	if (vga.dac.state==DAC_READ) return 0x3;
	else return 0x0;
}

static void write_p3c8(Bitu port,Bitu val,Bitu iolen) {
	vga.dac.write_index=val;
	vga.dac.pel_index=0;
	vga.dac.state=DAC_WRITE;
}

static Bitu read_p3c8(Bitu port, Bitu iolen){
	return vga.dac.write_index;
}

static void write_p3c9(Bitu port,Bitu val,Bitu iolen) {
	val&=0x3f;
	switch (vga.dac.pel_index) {
	case 0:
		vga.dac.rgb[vga.dac.write_index].red=val;
		vga.dac.pel_index=1;
		break;
	case 1:
		vga.dac.rgb[vga.dac.write_index].green=val;
		vga.dac.pel_index=2;
		break;
	case 2:
		vga.dac.rgb[vga.dac.write_index].blue=val;
		switch (vga.mode) {
		case M_VGA:
		case M_LIN8:
		case M_LIN16:
				RENDER_SetPal(vga.dac.write_index,
					vga.dac.rgb[vga.dac.write_index].red << 2,
					vga.dac.rgb[vga.dac.write_index].green << 2,
					vga.dac.rgb[vga.dac.write_index].blue << 2
				);
			break;
		default:
			/* Check for attributes and DAC entry link */
			for (Bitu i=0;i<16;i++) {
				if (vga.attr.palette[i]==vga.dac.write_index) {
					RENDER_SetPal(i,
					vga.dac.rgb[vga.dac.write_index].red << 2,
					vga.dac.rgb[vga.dac.write_index].green << 2,
					vga.dac.rgb[vga.dac.write_index].blue << 2);
				}
			}
		}
		vga.dac.write_index++;
//		vga.dac.read_index = vga.dac.write_index - 1;//disabled as it breaks Wari
		vga.dac.pel_index=0;
		break;
	default:
		LOG(LOG_VGAGFX,LOG_NORMAL)("VGA:DAC:Illegal Pel Index");			//If this can actually happen that will be the day
	};
}

static Bitu read_p3c9(Bitu port,Bitu iolen) {
	Bit8u ret;
	switch (vga.dac.pel_index) {
	case 0:
		ret=vga.dac.rgb[vga.dac.read_index].red;
		vga.dac.pel_index=1;
		break;
	case 1:
		ret=vga.dac.rgb[vga.dac.read_index].green;
		vga.dac.pel_index=2;
		break;
	case 2:
		ret=vga.dac.rgb[vga.dac.read_index].blue;
		vga.dac.read_index++;
		vga.dac.pel_index=0;
//		vga.dac.write_index=vga.dac.read_index+1;//disabled as it breaks wari
		break;
	default:
		LOG(LOG_VGAMISC,LOG_NORMAL)("VGA:DAC:Illegal Pel Index");			//If this can actually happen that will be the day
	}
	return ret;
}

void VGA_DAC_CombineColor(Bit8u attr,Bit8u pal) {
	/* Check if this is a new color */
	vga.attr.palette[attr]=pal;
	switch (vga.mode) {
	case M_VGA:
	case M_LIN8:
	case M_LIN16:
		break;
	default:
		RENDER_SetPal(attr,
			vga.dac.rgb[pal].red << 2,
			vga.dac.rgb[pal].green << 2,
			vga.dac.rgb[pal].blue << 2
		);
	}
}

void VGA_DAC_SetEntry(Bitu entry,Bit8u red,Bit8u green,Bit8u blue) {
	vga.dac.rgb[entry].red=red;
	vga.dac.rgb[entry].green=green;
	vga.dac.rgb[entry].blue=blue;
	switch (vga.mode) {
	case M_VGA:
	case M_LIN8:
	case M_LIN16:
		return;
	}
	for (Bitu i=0;i<16;i++) 
		if (vga.attr.palette[i]==entry)
			RENDER_SetPal(i,red << 2,green << 2,blue << 2);
}

void VGA_SetupDAC(void) {
	vga.dac.first_changed=256;
	vga.dac.bits=6;
	vga.dac.pel_mask=0xff;
	vga.dac.pel_index=0;
	vga.dac.state=DAC_READ;
	vga.dac.read_index=0;
	vga.dac.write_index=0;
	if (machine==MCH_VGA) {
		/* Setup the DAC IO port Handlers */
		IO_RegisterWriteHandler(0x3c6,write_p3c6,IO_MB);
		IO_RegisterReadHandler(0x3c6,read_p3c6,IO_MB);
		IO_RegisterWriteHandler(0x3c7,write_p3c7,IO_MB);
		IO_RegisterReadHandler(0x3c7,read_p3c7,IO_MB);
		IO_RegisterReadHandler(0x3c8,read_p3c8,IO_MB);
		IO_RegisterWriteHandler(0x3c8,write_p3c8,IO_MB);
		IO_RegisterWriteHandler(0x3c9,write_p3c9,IO_MB);
		IO_RegisterReadHandler(0x3c9,read_p3c9,IO_MB);
	}
};
