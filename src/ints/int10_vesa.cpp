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

#include <string.h>
#include <stddef.h>

#include "dosbox.h"
#include "callback.h"
#include "regs.h"
#include "mem.h"
#include "inout.h"
#include "int10.h"
#include "dos_inc.h"

static struct {
	Bitu setwindow;
} callback;


#ifdef _MSC_VER
#pragma pack (1)
#endif
struct MODE_INFO{
	Bit16u ModeAttributes;
	Bit8u WinAAttributes;
	Bit8u WinBAttributes;
	Bit16u WinGranularity;
	Bit16u WinSize;
	Bit16u WinASegment;
	Bit16u WinBSegment;
	Bit32u WinFuncPtr;
	Bit16u BytesPerScanLine;
	Bit16u XResolution;
	Bit16u YResolution;
	Bit8u XCharSize;
	Bit8u YCharSize;
	Bit8u NumberOfPlanes;
	Bit8u BitsPerPixel;
	Bit8u NumberOfBanks;
	Bit8u MemoryModel;
	Bit8u BankSize;
	Bit8u NumberOfImagePages;
	Bit8u Reserved_page;
	Bit8u RedMaskSize;
	Bit8u RedMaskPos;
	Bit8u GreenMaskSize;
	Bit8u GreenMaskPos;
	Bit8u BlueMaskSize;
	Bit8u BlueMaskPos;
	Bit8u ReservedMaskSize;
	Bit8u ReservedMaskPos;
	Bit8u DirectColorModeInfo;
	Bit32u PhysBasePtr;
	Bit32u OffScreenMemOffset;
	Bit16u OffScreenMemSize;
	Bit8u Reserved[206];
} GCC_ATTRIBUTE(packed);
#ifdef _MSC_VER
#pragma pack()
#endif



Bit8u VESA_GetSVGAInformation(Bit16u seg,Bit16u off) {
	/* Fill 256 byte buffer with VESA information */
	PhysPt buffer=PhysMake(seg,off);
	Bitu i;
	bool vbe2=false;
	if (mem_readd(buffer)==0x32454256) vbe2=true;
	if (vbe2) {
		for (i=0;i<0x200;i++) mem_writeb(buffer+i,0);		
	} else {
		for (i=0;i<0x100;i++) mem_writeb(buffer+i,0);
	}
	/* Fill common data */
	MEM_BlockWrite(buffer,(void *)"VESA",4);		//Identification
	mem_writew(buffer+0x04,0x102);					//Vesa version
	mem_writed(buffer+0x06,int10.rom.oemstring);	//Oemstring
	mem_writed(buffer+0x0a,0x0);					//Capabilities and flags
	mem_writed(buffer+0x0e,int10.rom.vesa_modes);	//VESA Mode list
	mem_writew(buffer+0x12,32);						//32 64kb blocks for 2 mb memory
	return 0x00;
}



Bit8u VESA_GetSVGAModeInformation(Bit16u mode,Bit16u seg,Bit16u off) {
	MODE_INFO minfo;
	memset(&minfo,0,sizeof(minfo));
	PhysPt buf=PhysMake(seg,off);
	
	Bitu i=0;
	if (mode<0x100) return 0x01;
	while (ModeList[i].mode!=0xffff) {
		if (mode==ModeList[i].mode) goto foundit; else i++;
	}
	return 0x01;
foundit:
	VideoModeBlock * mblock=&ModeList[i];
	switch (mblock->type) {
	case M_LIN8:		//Linear 8-bit
		WLE(minfo.ModeAttributes,0x1b);	//No bios output support, although would be easy
		WLE(minfo.WinAAttributes,0x7);	//Exists/readable/writable
		WLE(minfo.WinGranularity,64);
		WLE(minfo.WinSize,64);
		WLE(minfo.WinASegment,0xa000);
		WLE(minfo.WinBSegment,0xa000);
		WLE(minfo.WinFuncPtr,CALLBACK_RealPointer(callback.setwindow));
		WLE(minfo.BytesPerScanLine,mblock->swidth);
		WLE(minfo.NumberOfPlanes,0x1);
		WLE(minfo.BitsPerPixel,0x08);
		WLE(minfo.NumberOfBanks,0x1);
		WLE(minfo.MemoryModel,0x04);	//packed pixel
		WLE(minfo.NumberOfImagePages,0x05);
		WLE(minfo.Reserved_page,0x1);
		break;
	}
	WLE(minfo.XResolution,mblock->swidth);
	WLE(minfo.YResolution,mblock->sheight);
	WLE(minfo.XCharSize,mblock->cwidth);
	WLE(minfo.YCharSize,mblock->cheight);
	MEM_BlockWrite(buf,&minfo,sizeof(MODE_INFO));
	return 0x00;
}


Bit8u VESA_SetSVGAMode(Bit16u mode) {
	if (INT10_SetVideoMode(mode)) return 0x00;
	return 0x01;
};

Bit8u VESA_GetSVGAMode(Bit16u & mode) {
	mode=CurMode->mode;
	return 0x00;
}

Bit8u VESA_SetCPUWindow(Bit8u window,Bit16u address) {
	if (!window && (address<32)) {
		IO_Write(0x3d4,0x6a);
		IO_Write(0x3d5,(Bit8u)address);
		return 0x0;
	} else return 0x1;
}

Bit8u VESA_GetCPUWindow(Bit8u window,Bit16u & address) {
	if (!window) {
		IO_Write(0x3d4,0x6a);
		address=IO_Read(0x3d5);
		return 0x0;
	} else return 0x1;
}


Bit8u VESA_SetPalette(PhysPt data,Bitu index,Bitu count) {
	if (index>255) return 0x1;
	if (index+count>256) return 0x1;
	IO_Write(0x3c8,index);
	while (count) {
		IO_Write(0x3c9,mem_readb(data++));
		IO_Write(0x3c9,mem_readb(data++));
		IO_Write(0x3c9,mem_readb(data++));
		data++;
		count--;
	}
	return 0x00;
}


Bit8u VESA_GetPalette(PhysPt data,Bitu index,Bitu count) {
	if (index>255) return 0x1;
	if (index+count>256) return 0x1;
	IO_Write(0x3c7,index);
	while (count) {
		mem_writeb(data++,IO_Read(0x3c9));
		mem_writeb(data++,IO_Read(0x3c9));
		mem_writeb(data++,IO_Read(0x3c9));
		data++;
		count--;
	}
	return 0x00;
}


Bit8u VESA_ScanLineLength(Bit8u subcall,Bit16u & bytes,Bit16u & pixels,Bit16u & lines) {
	Bit8u bpp;
	switch (CurMode->type) {
	case M_LIN8:
		bpp=1;break;
	default:
		return 0x1;
	}
	Bitu scan_len;
	lines=0xfff;		//Does anyone care?
	switch (subcall) {
	case 0x00:	/* Set in pixels */
		bytes=(pixels*bpp);
	case 0x02:	/* Set in bytes */
		scan_len=bytes/4;
		if (bytes % 4) scan_len++;
		vga.config.scan_len=scan_len;
		VGA_StartResize();
		break;
	case 0x03:	/* Get maximum */
		bytes=0x400*4;
		pixels=bytes/bpp;
		return 0x00;
	case 0x01:	/* Get lengths */
		break;
	default:
		return 0x1;			//Illegal call
	}
	/* Write the scan line to video card the simple way */
	pixels=(vga.config.scan_len*4)/bpp;
	bytes=vga.config.scan_len*4;
	return 0x0;
}

Bit8u VESA_SetDisplayStart(Bit16u x,Bit16u y) {
	//TODO Maybe do things differently with lowres double line modes?	
	Bitu start;
	switch (CurMode->type) {
	case M_LIN8:
		start=vga.config.scan_len*4*y+x;
		vga.config.display_start=start/4;
		IO_Read(0x3da);
		IO_Write(0x3c0,0x13+32);
		IO_Write(0x3c0,(start % 4)*2);
		break;
	default:
		return 0x1;
	}
	return 0x00;
}

Bit8u VESA_GetDisplayStart(Bit16u & x,Bit16u & y) {
	Bitu times=vga.config.display_start/(vga.config.scan_len*4);
	Bitu rem=vga.config.display_start % (vga.config.scan_len*4);
	Bitu pan=vga.config.pel_panning;
	switch (CurMode->type) {
	case M_LIN8:
		y=times;
		x=rem*4+pan;
		break;
	default:
		return 0x1;
	}
	return 0x00;


}


static char oemstring[]="S3 Incorporated. Trio64";

static Bitu SetWindowPositionHandler(void) {
	VESA_SetCPUWindow(reg_bl,reg_dx);
	return 0;
}


void INT10_SetupVESA(void) {
	/* Put the mode list somewhere in memory */
	Bitu i;
	i=0;
	int10.rom.vesa_modes=RealMake(0xc000,int10.rom.used);
//TODO Maybe add normal vga modes too, but only seems to complicate things
	while (ModeList[i].mode!=0xffff) {
		if (ModeList[i].mode>=0x100){
			phys_writew(PhysMake(0xc000,int10.rom.used),ModeList[i].mode);
			int10.rom.used+=2;
		}
		i++;
	}
	phys_writew(PhysMake(0xc000,int10.rom.used),0xffff);
	int10.rom.used+=2;
	int10.rom.oemstring=RealMake(0xc000,int10.rom.used);
	Bitu len=strlen(oemstring)+1;
	for (i=0;i<len;i++) {
		phys_writeb(0xc0000+i+int10.rom.used++,oemstring[i]);
	}
	callback.setwindow=CALLBACK_Allocate();
	CALLBACK_Setup(callback.setwindow,SetWindowPositionHandler,CB_RETF);
}

