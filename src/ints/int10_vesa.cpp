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

/* $Id: int10_vesa.cpp,v 1.17 2005-09-27 11:05:44 c2woody Exp $ */

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

static char string_oem[]="S3 Incorporated. Trio64";
static char string_vendorname[]="DOSBox Development Team";
static char string_productname[]="DOSBox - The DOS Emulator";
static char string_productrev[]="DOSBox "VERSION;

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
	bool vbe2=false;Bit16u vbe2_pos=256+off;
	if (mem_readd(buffer)==0x32454256) vbe2=true;
	if (vbe2) {
		for (i=0;i<0x200;i++) mem_writeb(buffer+i,0);		
	} else {
		for (i=0;i<0x100;i++) mem_writeb(buffer+i,0);
	}
	/* Fill common data */
	MEM_BlockWrite(buffer,(void *)"VESA",4);			//Identification
	mem_writew(buffer+0x04,0x200);					//Vesa version 0x200
	if (vbe2) {
		mem_writed(buffer+0x06,RealMake(seg,vbe2_pos));
		for (i=0;i<sizeof(string_oem);i++) real_writeb(seg,vbe2_pos++,string_oem[i]);
		mem_writew(buffer+0x14,0x200);					//VBE 2 software revision
		mem_writed(buffer+0x16,RealMake(seg,vbe2_pos));
		for (i=0;i<sizeof(string_vendorname);i++) real_writeb(seg,vbe2_pos++,string_vendorname[i]);
		mem_writed(buffer+0x1a,RealMake(seg,vbe2_pos));
		for (i=0;i<sizeof(string_productname);i++) real_writeb(seg,vbe2_pos++,string_productname[i]);
		mem_writed(buffer+0x1e,RealMake(seg,vbe2_pos));
		for (i=0;i<sizeof(string_productrev);i++) real_writeb(seg,vbe2_pos++,string_productrev[i]);
	} else {
		mem_writed(buffer+0x06,int10.rom.oemstring);	//Oemstring
	}
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
	while (ModeList_VGA[i].mode!=0xffff) {
		if (mode==ModeList_VGA[i].mode) goto foundit; else i++;
	}
	return 0x01;
foundit:
	VideoModeBlock * mblock=&ModeList_VGA[i];
	switch (mblock->type) {
	case M_LIN8:		//Linear 8-bit
		var_write(&minfo.ModeAttributes,0x9b);
		var_write(&minfo.WinAAttributes,0x7);	//Exists/readable/writable
		var_write(&minfo.WinGranularity,64);
		var_write(&minfo.WinSize,64);
		var_write(&minfo.WinASegment,0xa000);
//		var_write(&minfo.WinBSegment,0xa000);
		var_write(&minfo.WinFuncPtr,CALLBACK_RealPointer(callback.setwindow));
		var_write(&minfo.BytesPerScanLine,mblock->swidth);
		var_write(&minfo.NumberOfPlanes,0x1);
		var_write(&minfo.BitsPerPixel,0x08);
		var_write(&minfo.NumberOfBanks,0x1);
		var_write(&minfo.MemoryModel,0x04);	//packed pixel
		var_write(&minfo.NumberOfImagePages,0x05);
		var_write(&minfo.Reserved_page,0x1);
		break;
	}
	var_write(&minfo.XResolution,mblock->swidth);
	var_write(&minfo.YResolution,mblock->sheight);
	var_write(&minfo.XCharSize,mblock->cwidth);
	var_write(&minfo.YCharSize,mblock->cheight);
	var_write(&minfo.PhysBasePtr,S3_LFB_BASE);

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

Bit8u VESA_SetCPUWindow(Bit8u window,Bit8u address) {
	if (window) return 0x1;
	if ((address<32)) {
		IO_Write(0x3d4,0x6a);
		IO_Write(0x3d5,(Bit8u)address);
		return 0x0;
	} else return 0x1;
}

Bit8u VESA_GetCPUWindow(Bit8u window,Bit16u & address) {
	if (window) return 0x1;
	IO_Write(0x3d4,0x6a);
	address=IO_Read(0x3d5);
	return 0x0;
}


Bit8u VESA_SetPalette(PhysPt data,Bitu index,Bitu count) {
//Structure is (vesa 3.0 doc): blue,green,red,alignment
	Bit8u r,g,b;
	if (index>255) return 0x1;
	if (index+count>256) return 0x1;
	IO_Write(0x3c8,index);
	while (count) {
		b = mem_readb(data++);
		g = mem_readb(data++);
		r = mem_readb(data++);
		data++;
		IO_Write(0x3c9,r);
		IO_Write(0x3c9,g);
		IO_Write(0x3c9,b);
		count--;
	}
	return 0x00;
}


Bit8u VESA_GetPalette(PhysPt data,Bitu index,Bitu count) {
	Bit8u r,g,b;
	if (index>255) return 0x1;
	if (index+count>256) return 0x1;
	IO_Write(0x3c7,index);
	while (count) {
		r = IO_Read(0x3c9);
		g = IO_Read(0x3c9);
		b = IO_Read(0x3c9);
		mem_writeb(data++,b);
		mem_writeb(data++,g);
		mem_writeb(data++,r);
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
		scan_len=bytes/8;
		if (bytes % 8) scan_len++;
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
	pixels=(vga.config.scan_len*8)/bpp;
	bytes=vga.config.scan_len*8;
	return 0x0;
}


/* Based of the s3 univbe driver */
static Bit8u PmodeInterface[]={
	0x08,0x00,0x19,0x00,0x57,0x00,0x00,0x00,0x50,0x52,0x8b,0xc2,0x8a,0xe0,0xb0,0x6a,
	0x66,0xba,0xd4,0x03,0x66,0xef,0x5a,0x58,0xc3,0x52,0x66,0xba,0xda,0x03,0xec,0xa8,
	0x01,0x75,0xfb,0x5a,0x53,0x8a,0xf9,0xb3,0x0d,0xb1,0x0c,0x66,0x8b,0xf2,0x66,0xba,
	0xd4,0x03,0x66,0x8b,0xc3,0x66,0xef,0x66,0x8b,0xc1,0x66,0xef,0x66,0x8b,0xde,0x8a,
	0xe3,0xb0,0x69,0x66,0xef,0x5b,0x52,0xf6,0xc3,0x80,0x74,0x09,0x66,0xba,0xda,0x03,
	0xec,0xa8,0x08,0x74,0xfb,0x5a,0xc3,0xf6,0xc3,0x80,0x74,0x10,0x52,0x66,0xba,0xda,
	0x03,0xec,0xa8,0x08,0x75,0xfb,0xec,0xa8,0x08,0x74,0xfb,0x5a,0x1e,0x06,0x1f,0x0f,
	0xb7,0xc9,0x8b,0xc2,0x66,0xba,0xc8,0x03,0xee,0x42,0xfc,0x8a,0x47,0x02,0xee,0x8a,
	0x47,0x01,0xee,0x8a,0x07,0xee,0x83,0xc7,0x04,0x49,0x75,0xef,0x1f,0xc3
};

Bit8u VESA_SetDisplayStart(Bit16u x,Bit16u y) {
	//TODO Maybe do things differently with lowres double line modes?	
	Bitu start;
	switch (CurMode->type) {
	case M_LIN8:
		start=vga.config.scan_len*8*y+x;
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
	Bitu times=(vga.config.display_start*4)/(vga.config.scan_len*8);
	Bitu rem=(vga.config.display_start*4) % (vga.config.scan_len*8);
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

static Bitu SetWindowPositionHandler(void) {
	if (reg_bh) reg_ah=VESA_GetCPUWindow(reg_bl,reg_dx);
	else reg_ah=VESA_SetCPUWindow(reg_bl,(Bit8u)reg_dx);
	reg_al=0x4f;
	return 0;
}

void INT10_SetupVESA(void) {
	/* Put the mode list somewhere in memory */
	Bitu i;
	i=0;
	int10.rom.vesa_modes=RealMake(0xc000,int10.rom.used);
//TODO Maybe add normal vga modes too, but only seems to complicate things
	while (ModeList_VGA[i].mode!=0xffff) {
		if (ModeList_VGA[i].mode>=0x100){
			phys_writew(PhysMake(0xc000,int10.rom.used),ModeList_VGA[i].mode);
			int10.rom.used+=2;
		}
		i++;
	}
	phys_writew(PhysMake(0xc000,int10.rom.used),0xffff);
	int10.rom.used+=2;
	int10.rom.oemstring=RealMake(0xc000,int10.rom.used);
	Bitu len=strlen(string_oem)+1;
	for (i=0;i<len;i++) {
		phys_writeb(0xc0000+int10.rom.used++,string_oem[i]);
	}
	/* Copy the pmode interface block */
	int10.rom.pmode_interface=RealMake(0xc000,int10.rom.used);
	int10.rom.pmode_interface_size=sizeof(PmodeInterface);
	for (i=0;i<sizeof(PmodeInterface);i++) {
		phys_writeb(0xc0000+int10.rom.used++,PmodeInterface[i]);
	}

	callback.setwindow=CALLBACK_Allocate();
	CALLBACK_Setup(callback.setwindow,SetWindowPositionHandler,CB_RETF);
}

