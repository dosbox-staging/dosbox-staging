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
#include "mem.h"
#include "inout.h"
#include "int10.h"

static Bit8u cga_masks[4]={~192,~48,~12,~3};
static Bit8u cga_masks2[8]={~128,~64,~32,~16,~8,~4,~2,~1};
void INT10_PutPixel(Bit16u x,Bit16u y,Bit8u page,Bit8u color) {

	switch (CurMode->type) {
	case M_CGA4:
		{
				Bit16u off=(y>>1)*80+(x>>2);
				if (y&1) off+=8*1024;
				Bit8u old=real_readb(0xb800,off);
				if (color & 0x80) {
					color&=3;
					old^=color << (2*(3-(x&3)));
				} else {
					old=old&cga_masks[x&3]|((color&3) << (2*(3-(x&3))));
				}
				real_writeb(0xb800,off,old);
		}
		break;
	case M_CGA2:
		{
				Bit16u off=(y>>1)*80+(x>>3);
				if (y&1) off+=8*1024;
				Bit8u old=real_readb(0xb800,off);
				if (color & 0x80) {
					color&=1;
					old^=color << ((7-(x&7)));
				} else {
					old=old&cga_masks2[x&7]|((color&1) << ((7-(x&7))));
				}
				real_writeb(0xb800,off,old);
		}
		break;
	case M_TANDY16:
		{
			Bit16u off=(y>>2)*160+(x>>1);
			off+=(8*1024) * (y & 3);
			Bit8u old=real_readb(0xb800,off);
			Bit8u p[2];
			p[1] = (old >> 4) & 0xf;
			p[0] = old & 0xf;
			Bitu ind = 1-(x & 0x1);

			if (color & 0x80) {
	 			p[ind]^=color;
			} else {
				p[ind]=color;
			}
			old = (p[1] << 4) | p[0];
			real_writeb(0xb800,off,old);
		}
		break;
	case M_EGA16:
		{
			/* Set the correct bitmask for the pixel position */
			IO_Write(0x3ce,0x8);Bit8u mask=128>>(x&7);IO_Write(0x3cf,mask);
			/* Set the color to set/reset register */
			IO_Write(0x3ce,0x0);IO_Write(0x3cf,color);
			/* Enable all the set/resets */
			IO_Write(0x3ce,0x1);IO_Write(0x3cf,0xf);
			/* test for xorring */
			if (color & 0x80) { IO_Write(0x3ce,0x3);IO_Write(0x3cf,0x18); }
			//Perhaps also set mode 1 
			/* Calculate where the pixel is in video memory */
			PhysPt off=0xa0000+CurMode->plength*page+((y*CurMode->swidth+x)>>3);
			/* Bitmask and set/reset should do the rest */
			mem_readb(off);
			mem_writeb(off,0xff);
			/* Restore bitmask */	
			IO_Write(0x3ce,0x8);IO_Write(0x3cf,0xff);
			IO_Write(0x3ce,0x1);IO_Write(0x3cf,0);
			/* Restore write operating if changed */
			if (color & 0x80) { IO_Write(0x3ce,0x3);IO_Write(0x3cf,0x0); }
			break;
		}

	case M_VGA:
		mem_writeb(PhysMake(0xa000,y*320+x),color);
		break;
	default:
		LOG(LOG_INT10,LOG_ERROR)("PutPixel unhandled mode type %d",CurMode->type);
		break;
	}	
}

void INT10_GetPixel(Bit16u x,Bit16u y,Bit8u page,Bit8u * color) {
	switch (CurMode->type) {
	case M_CGA4:
		{
			Bit16u off=(y>>1)*80+(x>>2);
			if (y&1) off+=8*1024;
			Bit8u val=real_readb(0xb800,off);
			*color=(val>>(((3-x&3))*2)) & 3 ;
		}
		break;
	case M_CGA2:
		{
			Bit16u off=(y>>1)*80+(x>>3);
			if (y&1) off+=8*1024;
			Bit8u val=real_readb(0xb800,off);
			*color=(val>>(((7-x&7)))) & 1 ;
		}
		break;
	case M_EGA16:
		{
			/* Calculate where the pixel is in video memory */
			PhysPt off=0xa0000+CurMode->plength*page+((y*CurMode->swidth+x)>>3);
			Bitu shift=7-(x & 7);
			/* Set the read map */
			*color=0;
			IO_Write(0x3ce,0x4);IO_Write(0x3cf,0);
			*color|=((mem_readb(off)>>shift) & 1) << 0;
			IO_Write(0x3ce,0x4);IO_Write(0x3cf,1);
			*color|=((mem_readb(off)>>shift) & 1) << 1;
			IO_Write(0x3ce,0x4);IO_Write(0x3cf,2);
			*color|=((mem_readb(off)>>shift) & 1) << 2;
			IO_Write(0x3ce,0x4);IO_Write(0x3cf,3);
			*color|=((mem_readb(off)>>shift) & 1) << 3;
			break;
		}
	case M_VGA:
		*color=mem_readb(PhysMake(0xa000,320*y+x));
		break;
	default:
		LOG(LOG_INT10,LOG_ERROR)("GetPixel unhandled mode type %d",CurMode->type);
		break;
	}	
}

