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

#define ACTL_MAX_REG   0x14

void INT10_SetSinglePaletteRegister(Bit8u reg,Bit8u val) {
	if(reg<=ACTL_MAX_REG) {
		IO_Read(VGAREG_ACTL_RESET);
		IO_Write(VGAREG_ACTL_ADDRESS,reg);
		IO_Write(VGAREG_ACTL_WRITE_DATA,val);
	}
	IO_Write(VGAREG_ACTL_ADDRESS,32);		//Enable output and protect palette
}


void INT10_SetOverscanBorderColor(Bit8u val) {
	IO_Read(VGAREG_ACTL_RESET);
	IO_Write(VGAREG_ACTL_ADDRESS,0x11);
	IO_Write(VGAREG_ACTL_WRITE_DATA,val);
	IO_Write(VGAREG_ACTL_ADDRESS,32);		//Enable output and protect palette
}

void INT10_SetAllPaletteRegisters(PhysPt data) {
	IO_Read(VGAREG_ACTL_RESET);
	// First the colors
	for(Bit8u i=0;i<0x10;i++) {
		IO_Write(VGAREG_ACTL_ADDRESS,i);
		IO_Write(VGAREG_ACTL_WRITE_DATA,mem_readb(data));
		data++;
	}
	// Then the border
	IO_Write(VGAREG_ACTL_ADDRESS,0x11);
	IO_Write(VGAREG_ACTL_WRITE_DATA,mem_readb(data));
	IO_Write(VGAREG_ACTL_ADDRESS,32);		//Enable output and protect palette
}

void INT10_ToggleBlinkingBit(Bit8u state) {
	Bit8u value;
	state&=0x01;
	IO_Read(VGAREG_ACTL_RESET);
	
	IO_Write(VGAREG_ACTL_ADDRESS,0x10);
	value=IO_Read(VGAREG_ACTL_READ_DATA);
	value&=0xf7;
	value|=state<<3;

	IO_Read(VGAREG_ACTL_RESET);
	IO_Write(VGAREG_ACTL_ADDRESS,0x10);
	IO_Write(VGAREG_ACTL_WRITE_DATA,value);
	IO_Write(VGAREG_ACTL_ADDRESS,32);		//Enable output and protect palette
}

void INT10_GetSinglePaletteRegister(Bit8u reg,Bit8u * val) {
	if(reg<=ACTL_MAX_REG) {
		IO_Read(VGAREG_ACTL_RESET);
		IO_Write(VGAREG_ACTL_ADDRESS,reg+32);
		*val=IO_Read(VGAREG_ACTL_READ_DATA);
	}
}

void INT10_GetOverscanBorderColor(Bit8u * val) {
	IO_Read(VGAREG_ACTL_RESET);
	IO_Write(VGAREG_ACTL_ADDRESS,0x11+32);
	*val=IO_Read(VGAREG_ACTL_READ_DATA);
}

void INT10_GetAllPaletteRegisters(PhysPt data) {
	IO_Read(VGAREG_ACTL_RESET);
	// First the colors
	for(Bit8u i=0;i<0x10;i++) {
		IO_Write(VGAREG_ACTL_ADDRESS,i);
		mem_writeb(data,IO_Read(VGAREG_ACTL_READ_DATA));
		data++;
	}
	// Then the border
	IO_Write(VGAREG_ACTL_ADDRESS,0x11+32);
	mem_writeb(data,IO_Read(VGAREG_ACTL_READ_DATA));
}

void INT10_SetSingleDacRegister(Bit8u index,Bit8u red,Bit8u green,Bit8u blue) {
	IO_Write(VGAREG_DAC_WRITE_ADDRESS,(Bit8u)index);
	IO_Write(VGAREG_DAC_DATA,red);
	IO_Write(VGAREG_DAC_DATA,green);
	IO_Write(VGAREG_DAC_DATA,blue);
}

void INT10_GetSingleDacRegister(Bit8u index,Bit8u * red,Bit8u * green,Bit8u * blue) {
	IO_Write(VGAREG_DAC_READ_ADDRESS,index);
	*red=IO_Read(VGAREG_DAC_DATA);
	*green=IO_Read(VGAREG_DAC_DATA);
	*blue=IO_Read(VGAREG_DAC_DATA);
}

void INT10_SetDACBlock(Bit16u index,Bit16u count,PhysPt data) {
 	IO_Write(VGAREG_DAC_WRITE_ADDRESS,(Bit8u)index);
	for (;count>0;count--) {
		IO_Write(VGAREG_DAC_DATA,mem_readb(data++));
		IO_Write(VGAREG_DAC_DATA,mem_readb(data++));
		IO_Write(VGAREG_DAC_DATA,mem_readb(data++));
	}
}

void INT10_GetDACBlock(Bit16u index,Bit16u count,PhysPt data) {
 	IO_Write(VGAREG_DAC_WRITE_ADDRESS,(Bit8u)index);
	for (;count>0;count--) {
		mem_writeb(data++,IO_Read(VGAREG_DAC_DATA));
		mem_writeb(data++,IO_Read(VGAREG_DAC_DATA));
		mem_writeb(data++,IO_Read(VGAREG_DAC_DATA));
	}
}

void INT10_SelectDACPage(Bit8u function,Bit8u mode) {
	IO_Read(VGAREG_ACTL_RESET);
	IO_Write(VGAREG_ACTL_ADDRESS,0x10);
	Bit8u old10=IO_Read(VGAREG_ACTL_READ_DATA);
	if (!function) {		//Select paging mode
		if (mode) old10|=0x80;
		else old10&=0x7f;
		IO_Write(VGAREG_ACTL_ADDRESS,0x10);
		IO_Write(VGAREG_ACTL_WRITE_DATA,old10);
	} else {				//Select page
		if (!(old10 & 0x80)) mode<<=2;
		mode&=0xf;
		IO_Write(VGAREG_ACTL_ADDRESS,0x14);
		IO_Write(VGAREG_ACTL_WRITE_DATA,mode);
	}
	IO_Write(VGAREG_ACTL_ADDRESS,32);		//Enable output and protect palette
}

void INT10_SetPelMask(Bit8u mask) {
	IO_Write(VGAREG_PEL_MASK,mask);
}	

void INT10_GetPelMask(Bit8u & mask) {
	mask=IO_Read(VGAREG_PEL_MASK);
}	

void INT10_SetBackgroundBorder(Bit8u val) {
	Bitu temp=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAL);
	temp=(temp & 0xe0) | (val & 0x1f);
	real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAL,temp);
	IO_Write(0x3d9,temp);
}

void INT10_SetColorSelect(Bit8u val) {
	Bitu temp=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAL);
	temp=(temp & 0xdf) | ((val & 1) ? 0x20 : 0x0);
	real_writeb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAL,temp);
	IO_Write(0x3d9,temp);
}

