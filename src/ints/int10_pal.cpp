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
}


void INT10_SetOverscanBorderColor(Bit8u val) {
	IO_Read(VGAREG_ACTL_RESET);
	IO_Write(VGAREG_ACTL_ADDRESS,0x11);
	IO_Write(VGAREG_ACTL_WRITE_DATA,val);
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
}

void INT10_GetSinglePaletteRegister(Bit8u reg,Bit8u * val) {
	if(reg<=ACTL_MAX_REG) {
		IO_Read(VGAREG_ACTL_RESET);
		IO_Write(VGAREG_ACTL_ADDRESS,reg);
		*val=IO_Read(VGAREG_ACTL_READ_DATA);
	}
}

void INT10_GetOverscanBorderColor(Bit8u * val) {
	IO_Read(VGAREG_ACTL_RESET);
	IO_Write(VGAREG_ACTL_ADDRESS,0x11);
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
	IO_Write(VGAREG_ACTL_ADDRESS,0x11);
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
};
