// SPDX-FileCopyrightText:  2021-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "int10.h"

#include "hardware/port.h"
#include "hardware/memory.h"
#include "hardware/pci_bus.h"

static uint8_t cga_masks[4]={0x3f,0xcf,0xf3,0xfc};
static uint8_t cga_masks2[8]={0x7f,0xbf,0xdf,0xef,0xf7,0xfb,0xfd,0xfe};

void INT10_PutPixel(uint16_t x,uint16_t y,uint8_t page,uint8_t color) {
	static bool putpixelwarned = false;

	switch (CurMode->type) {
	case M_CGA4:
	{
		if (real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_MODE)<=5) {
			// this is a 16k mode
			uint16_t off=(y>>1)*80+(x>>2);
			if (y&1) off+=8*1024;

			uint8_t old=real_readb(0xb800,off);
			if (color & 0x80) {
				color&=3;
				old^=color << (2*(3-(x&3)));
			} else {
				old=(old&cga_masks[x&3])|((color&3) << (2*(3-(x&3))));
			}
			real_writeb(0xb800,off,old);
		} else {
			// a 32k mode: PCJr special case (see M_TANDY16)
			uint16_t seg;
			if (is_machine_pcjr()) {
				Bitu cpupage =
					(real_readb(BIOSMEM_SEG, BIOSMEM_CRTCPU_PAGE) >> 3) & 0x7;
				seg = cpupage << 10; // A14-16 to addr bits 14-16
			} else
				seg = 0xb800;

			uint16_t off=(y>>2)*160+((x>>2)&(~1));
			off+=(8*1024) * (y & 3);

			uint16_t old=real_readw(seg,off);
			if (color & 0x80) {
				old^=(color&1) << (7-(x&7));
				old^=((color&2)>>1) << ((7-(x&7))+8);
			} else {
				old=(old&(~(0x101<<(7-(x&7))))) | ((color&1) << (7-(x&7))) | (((color&2)>>1) << ((7-(x&7))+8));
			}
			real_writew(seg,off,old);
		}
	}
	break;
	case M_CGA2:
		{
				uint16_t off=(y>>1)*80+(x>>3);
				if (y&1) off+=8*1024;
				uint8_t old=real_readb(0xb800,off);
				if (color & 0x80) {
					color&=1;
					old^=color << ((7-(x&7)));
				} else {
					old=(old&cga_masks2[x&7])|((color&1) << ((7-(x&7))));
				}
				real_writeb(0xb800,off,old);
		}
		break;
	case M_TANDY16:
	{
		// find out if we are in a 32k mode (0x9 or 0xa)
		// This requires special handling on the PCJR
		// because only 16k are mapped at 0xB800
		bool is_32k = (real_readb(BIOSMEM_SEG, BIOSMEM_CURRENT_MODE) >= 9)?
			true:false;

		uint16_t segment, offset;
		if (is_32k) {
			if (is_machine_pcjr()) {
				Bitu cpupage =
					(real_readb(BIOSMEM_SEG, BIOSMEM_CRTCPU_PAGE) >> 3) & 0x7;
				segment = cpupage << 10; // A14-16 to addr bits 14-16
			} else
				segment = 0xb800;
			// bits 1 and 0 of y select the bank
			// two pixels per byte (thus x>>1)
			offset = (y >> 2) * (CurMode->swidth >> 1) + (x>>1);
			// select the scanline bank
			offset += (8*1024) * (y & 3);
		} else {
			segment = 0xb800;
			// bit 0 of y selects the bank
			offset = (y >> 1) * (CurMode->swidth >> 1) + (x>>1);
			offset += (8*1024) * (y & 1);
		}

		// update the pixel
		uint8_t old=real_readb(segment, offset);
		uint8_t p[2];
		p[1] = (old >> 4) & 0xf;
		p[0] = old & 0xf;
		Bitu ind = 1-(x & 0x1);

		if (color & 0x80) {
			// color is to be XORed
	 		p[ind]^=(color & 0x7f);
		} else {
			p[ind]=color;
		}
		old = (p[1] << 4) | p[0];
		real_writeb(segment,offset, old);
	}
	break;
	case M_LIN4:
		if (!is_machine_vga_or_better() || svga_type != SvgaType::TsengEt4k ||
		    (CurMode->swidth > 800)) {
			// the ET4000 BIOS supports text output in 800x600 SVGA
			// (Gateway 2)
			putpixelwarned = true;
			LOG(LOG_INT10, LOG_ERROR)("PutPixel unhandled mode type %d", CurMode->type);
			break;
		}
		[[fallthrough]];
	case M_EGA: {
		/* Enable writing to all planes */
		IO_Write(0x3c4, 0x2);
		IO_Write(0x3c5, 0xf);
		/* Set the correct bitmask for the pixel position */
		IO_Write(0x3ce, 0x8);
		uint8_t mask = 128 >> (x & 7);
		IO_Write(0x3cf, mask);
		/* Set the color to set/reset register */
		IO_Write(0x3ce, 0x0);
		IO_Write(0x3cf, color);
		/* Enable all the set/resets */
		IO_Write(0x3ce, 0x1);
		IO_Write(0x3cf, 0xf);
		/* test for xorring */
		if (color & 0x80) {
			IO_Write(0x3ce, 0x3);
			IO_Write(0x3cf, 0x18);
		}
		// Perhaps also set mode 1
		/* Calculate where the pixel is in video memory */
		if (CurMode->plength !=
		    (Bitu)real_readw(BIOSMEM_SEG, BIOSMEM_PAGE_SIZE))
			LOG(LOG_INT10, LOG_ERROR)
			("PutPixel_EGA_p: %u != %x", CurMode->plength,
			 real_readw(BIOSMEM_SEG, BIOSMEM_PAGE_SIZE));
		if (CurMode->swidth !=
		    (Bitu)real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS) * 8)
			LOG(LOG_INT10, LOG_ERROR)
			("PutPixel_EGA_w: %u!=%x", CurMode->swidth,
			 real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS) * 8);
		PhysPt off = 0xa0000 +
		             real_readw(BIOSMEM_SEG, BIOSMEM_PAGE_SIZE) * page +
		             ((y * real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS) * 8 + x) >>
		              3);
		/* Bitmask and set/reset should do the rest */
		mem_readb(off);
		mem_writeb(off, 0xff);
		/* Restore bitmask */
		IO_Write(0x3ce, 0x8);
		IO_Write(0x3cf, 0xff);
		IO_Write(0x3ce, 0x1);
		IO_Write(0x3cf, 0);
		/* Restore write operating if changed */
		if (color & 0x80) {
			IO_Write(0x3ce, 0x3);
			IO_Write(0x3cf, 0x0);
		}
		break;
	}

	case M_VGA:
		mem_writeb(PhysicalMake(0xa000,y*320+x),color);
		break;
	case M_LIN8: {
			if (CurMode->swidth!=(Bitu)real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS)*8)
				LOG(LOG_INT10,LOG_ERROR)("PutPixel_VGA_w: %u!=%x",CurMode->swidth,real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS)*8);
		        PhysPt off = PciGfxLfbBase + x +
		                     y * real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS) * 8;
		        mem_writeb(off,color);
			break;
		}
	default:
		if (!putpixelwarned) {
			putpixelwarned = true;		
			LOG(LOG_INT10,LOG_ERROR)("PutPixel unhandled mode type %d",CurMode->type);
		}
		break;
	}	
}

void INT10_GetPixel(uint16_t x,uint16_t y,uint8_t page,uint8_t * color) {
	switch (CurMode->type) {
	case M_CGA4:
		{
			uint16_t off=(y>>1)*80+(x>>2);
			if (y&1) off+=8*1024;
			uint8_t val=real_readb(0xb800,off);
			*color=(val>>(((3-(x&3)))*2)) & 3 ;
		}
		break;
	case M_CGA2:
		{
			uint16_t off=(y>>1)*80+(x>>3);
			if (y&1) off+=8*1024;
			uint8_t val=real_readb(0xb800,off);
			*color=(val>>(((7-(x&7))))) & 1 ;
		}
		break;
	case M_TANDY16:
		{
			bool is_32k = (real_readb(BIOSMEM_SEG, BIOSMEM_CURRENT_MODE) >= 9)?true:false;
			uint16_t segment, offset;
			if (is_32k) {
				if (is_machine_pcjr()) {
					Bitu cpupage = (real_readb(BIOSMEM_SEG, BIOSMEM_CRTCPU_PAGE) >> 3) & 0x7;
					segment = cpupage << 10;
				} else segment = 0xb800;
				offset = (y >> 2) * (CurMode->swidth >> 1) + (x>>1);
				offset += (8*1024) * (y & 3);
			} else {
				segment = 0xb800;
				offset = (y >> 1) * (CurMode->swidth >> 1) + (x>>1);
				offset += (8*1024) * (y & 1);
			}
			uint8_t val=real_readb(segment,offset);
			*color=(val>>((x&1)?0:4)) & 0xf;
		}
		break;
	case M_EGA:
		{
			/* Calculate where the pixel is in video memory */
			if (CurMode->plength!=(Bitu)real_readw(BIOSMEM_SEG,BIOSMEM_PAGE_SIZE))
				LOG(LOG_INT10,LOG_ERROR)("GetPixel_EGA_p: %u!=%x",CurMode->plength,real_readw(BIOSMEM_SEG,BIOSMEM_PAGE_SIZE));
			if (CurMode->swidth!=(Bitu)real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS)*8)
				LOG(LOG_INT10,LOG_ERROR)("GetPixel_EGA_w: %u!=%x",CurMode->swidth,real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS)*8);
			PhysPt off=0xa0000+real_readw(BIOSMEM_SEG,BIOSMEM_PAGE_SIZE)*page+
				((y*real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS)*8+x)>>3);
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
		*color=mem_readb(PhysicalMake(0xa000,320*y+x));
		break;
	case M_LIN8: {
			if (CurMode->swidth!=(Bitu)real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS)*8)
				LOG(LOG_INT10,LOG_ERROR)("GetPixel_VGA_w: %u!=%x",CurMode->swidth,real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS)*8);
		        PhysPt off = PciGfxLfbBase + x +
		                     y * real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS) * 8;
		        *color = mem_readb(off);
			break;
		}
	default:
		LOG(LOG_INT10,LOG_ERROR)("GetPixel unhandled mode type %d",CurMode->type);
		break;
	}	
}
