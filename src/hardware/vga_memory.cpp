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



	
#include <stdlib.h>
#include <string.h>
#include "dosbox.h"
#include "mem.h"
#include "vga.h"


static Bit8u VGA_NormalReadHandler(PhysPt start) {
	start-=0xa0000;
	vga.latch.d=vga.mem.latched[start].d;
	switch (vga.config.read_mode) {
	case 0:
		return(vga.latch.b[vga.config.read_map_select]);
	case 1:
		VGA_Latch templatch;
		templatch.d=(vga.latch.d &	FillTable[vga.config.color_dont_care]) ^ FillTable[vga.config.color_compare & vga.config.color_dont_care];
		return ~(templatch.b[0] | templatch.b[1] | templatch.b[2] | templatch.b[3]);
	}
	return 0;
}

//Nice one from DosEmu

INLINE static Bit32u RasterOp(Bit32u input,Bit32u mask) {
	switch (vga.config.raster_op) {
	case 0x00:	/* None */
		return (input & mask) | (vga.latch.d & ~mask);
	case 0x01:	/* AND */
		return (input | ~mask) & vga.latch.d;
	case 0x02:	/* OR */
		return (input & mask) | vga.latch.d;
	case 0x03:	/* XOR */
		return (input & mask) ^ vga.latch.d;
	};
	return 0;
}


INLINE static Bit32u ModeOperation(Bit8u val) {
	Bit32u full;
	switch (vga.config.write_mode) {
	case 0x00:
		// Write Mode 0: In this mode, the host data is first rotated as per the Rotate Count field, then the Enable Set/Reset mechanism selects data from this or the Set/Reset field. Then the selected Logical Operation is performed on the resulting data and the data in the latch register. Then the Bit Mask field is used to select which bits come from the resulting data and which come from the latch register. Finally, only the bit planes enabled by the Memory Plane Write Enable field are written to memory. 
		full=ExpandTable[val];
		full=(full & vga.config.full_not_enable_set_reset) | vga.config.full_enable_and_set_reset; 
		full=RasterOp(full,vga.config.full_bit_mask);
		break;
	case 0x01:
		// Write Mode 1: In this mode, data is transferred directly from the 32 bit latch register to display memory, affected only by the Memory Plane Write Enable field. The host data is not used in this mode. 
		full=vga.latch.d;
		break;
	case 0x02:
		//Write Mode 2: In this mode, the bits 3-0 of the host data are replicated across all 8 bits of their respective planes. Then the selected Logical Operation is performed on the resulting data and the data in the latch register. Then the Bit Mask field is used to select which bits come from the resulting data and which come from the latch register. Finally, only the bit planes enabled by the Memory Plane Write Enable field are written to memory. 
		full=RasterOp(FillTable[val&0xF],vga.config.full_bit_mask);
		break;
	case 0x03:
		// Write Mode 3: In this mode, the data in the Set/Reset field is used as if the Enable Set/Reset field were set to 1111b. Then the host data is first rotated as per the Rotate Count field, then logical ANDed with the value of the Bit Mask field. The resulting value is used on the data obtained from the Set/Reset field in the same way that the Bit Mask field would ordinarily be used. to select which bits come from the expansion of the Set/Reset field and which come from the latch register. Finally, only the bit planes enabled by the Memory Plane Write Enable field are written to memory.
		full=RasterOp(vga.config.full_set_reset,ExpandTable[val] & vga.config.full_bit_mask);
		break;
	default:
		LOG(LOG_VGAMISC,LOG_NORMAL)("VGA:Unsupported write mode %d",vga.config.write_mode);
	}
	return full;
}

static Bit8u VGA_CGA_4_ReadHandler(PhysPt start) {
	return vga.mem.linear[start-0xb8000];
}

static void VGA_CGA_4_WriteHandler(PhysPt start,Bit8u val) {
	start-=0xb8000;
	vga.mem.linear[start]=val;
	Bitu off;
	if (start>0x2000) off=320*(((start-0x2000)/80)*2+1)+((start-0x2000) % 80)*4;
	else off=320*(((start)/80)*2)+(start % 80)*4;
	Bit32u * draw=(Bit32u *)&vga.mem.linear[512*1024+off];
	/* TODO Could also use a Bit32u lookup table for this */
	*draw=CGA_4_Table[val];
	draw=(Bit32u *)&vga.mem.linear[512*1024+off+0x4000*4];
	*draw=CGA_4_Table[val];

}

static void VGA_GFX_16_WriteHandler(PhysPt start,Bit8u val) {
	start-=0xa0000;
	val=(val >> vga.config.data_rotate) | (val << (8-vga.config.data_rotate));
	Bit32u data=ModeOperation(val);
	/* Update video memory and the pixel buffer */
	VGA_Latch pixels;
	pixels.d=vga.mem.latched[start].d;
	pixels.d&=vga.config.full_not_map_mask;
	pixels.d|=(data & vga.config.full_map_mask);
	vga.mem.latched[start].d=pixels.d;
	Bit8u * write_pixels=&vga.mem.linear[512*1024+(start<<3)];

	Bit32u colors0_3, colors4_7;
	VGA_Latch temp;temp.d=(pixels.d>>4) & 0x0f0f0f0f;
		colors0_3 = 
		Expand16Table[0][temp.b[0]] |
		Expand16Table[1][temp.b[1]] |
		Expand16Table[2][temp.b[2]] |
		Expand16Table[3][temp.b[3]];
	*(Bit32u *)write_pixels=colors0_3;
	*(Bit32u *)(write_pixels+512*1024)=colors0_3;
	temp.d=pixels.d & 0x0f0f0f0f;
	colors4_7 = 
		Expand16Table[0][temp.b[0]] |
		Expand16Table[1][temp.b[1]] |
		Expand16Table[2][temp.b[2]] |
		Expand16Table[3][temp.b[3]];
	*(Bit32u *)(write_pixels+4)=colors4_7;
	*(Bit32u *)(write_pixels+512*1024+4)=colors4_7;

}

static void VGA_GFX_256U_WriteHandler(PhysPt start,Bit8u val) {
	start-=0xa0000;
	Bit32u data=ModeOperation(val);
//	Bit32u data=ExpandTable[val];
	VGA_Latch pixels;
	pixels.d=vga.mem.latched[start].d;
	pixels.d&=vga.config.full_not_map_mask;
	pixels.d|=(data & vga.config.full_map_mask);
	vga.mem.latched[start].d=pixels.d;
	vga.mem.latched[start+64*1024].d=pixels.d;
}

static void VGA_TEXT16_Write(PhysPt start,Bit8u val) {
	start-=vga.config.mem_base;
	/* Check for page 2 being enabled for writing */
	if (vga.seq.map_mask & 0x4) {
		vga.draw.font[start]=val;
	}
	//TODO Check for writes to other pages with normal text characters/attributes
}

static Bit8u VGA_TEXT16_Read(PhysPt start) {
	start-=vga.config.mem_base;
	return vga.draw.font[start];
}

void VGA_SetupHandlers(void) {
	/* Sets up the correct memory handler from the vga.mode setting */
	MEM_ClearPageHandlers(PAGE_COUNT(0xa0000),PAGE_COUNT(0x20000));
	switch (vga.mode) {
	case M_LIN8:
		MEM_SetupMapping(PAGE_COUNT(0xa0000),PAGE_COUNT(0x10000),&vga.mem.linear[vga.s3.bank*64*1024]);
		break;
	case M_VGA:
		if (vga.config.chained) {
			MEM_SetupMapping(PAGE_COUNT(0xa0000),PAGE_COUNT(0x10000),&vga.mem.linear[vga.s3.bank*64*1024]);
		} else {
			MEM_SetupPageHandlers(PAGE_COUNT(0xa0000),PAGE_COUNT(0x10000),
				&VGA_NormalReadHandler,&VGA_GFX_256U_WriteHandler);
		}
		break;
	case M_EGA16:
		MEM_SetupPageHandlers(PAGE_COUNT(0xa0000),PAGE_COUNT(0x10000),
			&VGA_NormalReadHandler,&VGA_GFX_16_WriteHandler);
		break;	
	case M_TEXT16:
		/* Check if we're not in odd/even mode */
		if (vga.gfx.miscellaneous & 0x2) break;
		MEM_SetupPageHandlers(PAGE_COUNT(0xa0000),PAGE_COUNT(0x10000),&VGA_TEXT16_Read,&VGA_TEXT16_Write);
		//Could also do it using 0xb8000, let's double map
		MEM_SetupPageHandlers(PAGE_COUNT(0xb8000),PAGE_COUNT(0x8000),&VGA_TEXT16_Read,&VGA_TEXT16_Write);
		break;
	case M_TANDY16:
		MEM_SetupMapping(PAGE_COUNT(0xb8000),PAGE_COUNT(0x8000),&vga.mem.linear[vga.tandy.mem_bank << 14]);
		break;
	case M_CGA4:
	case M_CGA2:
		break;
	default:
		LOG_MSG("Unhandled vga mode %X",vga.mode);
	}
}





void VGA_SetupMemory() {
	memset((void *)&vga.mem,0,512*1024*4);
	/* Map linear frame buffer at 32 mb */
	MEM_SetupMapping(PAGE_COUNT(32*1024*1024),PAGE_COUNT(2*1024*1024),&vga.mem);
}
