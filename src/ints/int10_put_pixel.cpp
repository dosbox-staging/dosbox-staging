/*
 *  Copyright (C) 2002  The DOSBox Team
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
#include "mem.h"
#include "inout.h"
#include "int10.h"

union VGA_Memory {
	Bit8u linear[64*1024*4];
	Bit8u paged[64*1024][4];
};	
extern VGA_Memory vga_mem;

static Bit8u cga_masks[4]={~192,~48,~12,~3};

void INT10_PutPixel(Bit16u x,Bit16u y,Bit8u page,Bit8u color) {

	VGAMODES * curmode=GetCurrentMode();	

	switch (curmode->memmodel) {
	case CGA:
		{
			Bit16u off=(y>>1)*80+(x>>2);
			if (y&1) off+=8*1024;
			Bit8u old=real_readb(0xb800,off);
			old=old&cga_masks[x&3]|((color&3) << (2*(3-(x&3))));
			real_writeb(0xb800,off,old);
		}
		break;
	case PLANAR4:
		{
			/* Set the correct bitmask for the pixel position */
			IO_Write(0x3ce,0x8);Bit8u mask=128>>(x&7);IO_Write(0x3cf,mask);
			/* Set the color to set/reset register */
			IO_Write(0x3ce,0x0);IO_Write(0x3cf,color);
			/* Enable all the set/resets */
			IO_Write(0x3ce,0x1);IO_Write(0x3cf,0xf);
			//Perhaps also set mode 1 
			/* Calculate where the pixel is in video memory */
			Bit16u base_address=((((curmode->sheight*curmode->swidth)>>3)|0xff)+1)*page;	
			PhysPt off=0xa0000+base_address+((y*curmode->swidth+x)>>3);
			/* Bitmask and set/reset should do the rest */
			mem_readb(off);
			mem_writeb(off,0xff);
			break;
		}
	case CTEXT:
	case MTEXT:
	case PLANAR1:
	case PLANAR2:
	case LINEAR8:
	default:
		LOG_WARN("INT10:PutPixel Unhanled memory model");
		break;
	}	
}

void INT10_GetPixel(Bit16u x,Bit16u y,Bit8u page,Bit8u * color) {

	VGAMODES * curmode=GetCurrentMode();	
	switch (curmode->memmodel) {
	case CGA:
		{
			Bit16u off=(y>>1)*80+(x>>2);
			if (y&1) off+=8*1024;
			Bit8u val=real_readb(0xb800,off);
			*color=val<<((x&3)*2);
		}
		break;
	default:
		LOG_WARN("INT10:GetPixel Unhanled memory model");
		break;
	}	
}

