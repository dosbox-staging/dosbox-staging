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
	
#include <stdlib.h>
#include <string.h>
#include "dosbox.h"
#include "mem.h"
#include "vga.h"
#include "paging.h"
#include "pic.h"
#include "inout.h"

void VGA_MapMMIO(void);

static Bitu VGA_NormalReadHandler(PhysPt start) {
	vga.latch.d=vga.mem.latched[start].d;
	switch (vga.config.read_mode) {
	case 0:
		return (vga.latch.b[vga.config.read_map_select]);
	case 1:
		VGA_Latch templatch;
		templatch.d=(vga.latch.d &	FillTable[vga.config.color_dont_care]) ^ FillTable[vga.config.color_compare & vga.config.color_dont_care];
		return (Bit8u)~(templatch.b[0] | templatch.b[1] | templatch.b[2] | templatch.b[3]);
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

static void VGA_GFX_16_WriteHandler(PhysPt start,Bit8u val) {
	val=((val >> vga.config.data_rotate) | (val << (8-vga.config.data_rotate)));
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
	Bit32u data=ModeOperation(val);
	VGA_Latch pixels;
	pixels.d=vga.mem.latched[start].d;
	pixels.d&=vga.config.full_not_map_mask;
	pixels.d|=(data & vga.config.full_map_mask);
	vga.mem.latched[start].d=pixels.d;
	vga.mem.latched[start+64*1024].d=pixels.d;
}

/* Gonna assume that whoever maps vga memory, maps it on 32/64kb boundary */

#define VGA_PAGES		(128/4)
#define VGA_PAGE_A0		(0xA0000/4096)
#define VGA_PAGE_B0		(0xB0000/4096)
#define VGA_PAGE_B8		(0xB8000/4096)

static struct {
	Bitu base,mask;
} vgapages;
	
class VGARead_PageHandler : public PageHandler {
public:
	Bitu readb(PhysPt addr) {
		addr&=0xffff;
		return VGA_NormalReadHandler(addr);
	}
	Bitu readw(PhysPt addr) {
		addr&=0xffff;
		return 
			(VGA_NormalReadHandler(addr+0) << 0) |
			(VGA_NormalReadHandler(addr+1) << 8);
	}
	Bitu readd(PhysPt addr) {
		addr&=0xffff;
		return 
			(VGA_NormalReadHandler(addr+0) << 0)  |
			(VGA_NormalReadHandler(addr+1) << 8)  |
			(VGA_NormalReadHandler(addr+2) << 16) |
			(VGA_NormalReadHandler(addr+3) << 24);
	}
};

class VGA_16_PageHandler : public VGARead_PageHandler {
public:	
	VGA_16_PageHandler()  {
		flags=PFLAG_NOCODE;
	}
	void writeb(PhysPt addr,Bitu val) {
		addr&=0xffff;
		VGA_GFX_16_WriteHandler(addr+0,(Bit8u)(val >> 0));
	}
	void writew(PhysPt addr,Bitu val) {
		addr&=0xffff;
		VGA_GFX_16_WriteHandler(addr+0,(Bit8u)(val >> 0));
		VGA_GFX_16_WriteHandler(addr+1,(Bit8u)(val >> 8));
	}
	void writed(PhysPt addr,Bitu val) {
		addr&=0xffff;
		VGA_GFX_16_WriteHandler(addr+0,(Bit8u)(val >> 0));
		VGA_GFX_16_WriteHandler(addr+1,(Bit8u)(val >> 8));
		VGA_GFX_16_WriteHandler(addr+2,(Bit8u)(val >> 16));
		VGA_GFX_16_WriteHandler(addr+3,(Bit8u)(val >> 24));
	}
};

class VGA_256_PageHandler : public VGARead_PageHandler {
public:	
	VGA_256_PageHandler()  {
		flags=PFLAG_NOCODE;
	}
	void writeb(PhysPt addr,Bitu val) {
		addr&=0xffff;
		VGA_GFX_256U_WriteHandler(addr+0,(Bit8u)(val >> 0));
	}
	void writew(PhysPt addr,Bitu val) {
		addr&=0xffff;
		VGA_GFX_256U_WriteHandler(addr+0,(Bit8u)(val >> 0));
		VGA_GFX_256U_WriteHandler(addr+1,(Bit8u)(val >> 8));
	}
	void writed(PhysPt addr,Bitu val) {
		addr&=0xffff;
		VGA_GFX_256U_WriteHandler(addr+0,(Bit8u)(val >> 0));
		VGA_GFX_256U_WriteHandler(addr+1,(Bit8u)(val >> 8));
		VGA_GFX_256U_WriteHandler(addr+2,(Bit8u)(val >> 16));
		VGA_GFX_256U_WriteHandler(addr+3,(Bit8u)(val >> 24));
	}
};

class VGA_TEXT_PageHandler : public PageHandler {
public:
	VGA_TEXT_PageHandler() {
		flags=PFLAG_NOCODE;
	}
	Bitu readb(PhysPt addr) {
		addr&=vgapages.mask;
		return vga.draw.font[addr];
	}
	void writeb(PhysPt addr,Bitu val){
		addr&=vgapages.mask;
		if (vga.seq.map_mask & 0x4) {
			vga.draw.font[addr]=(Bit8u)val;
		}
	}
};

class VGA_MAP_PageHandler : public PageHandler {
public:
	VGA_MAP_PageHandler() {
		flags=PFLAG_READABLE|PFLAG_WRITEABLE|PFLAG_NOCODE;
	}
	HostPt GetHostPt(Bitu phys_page) {
 		phys_page-=vgapages.base;
		return &vga.mem.linear[vga.s3.bank*64*1024+phys_page*4096];
	}
};

class VGA_MMIO_PageHandler : public PageHandler {
public:
	Bit16u regmem[16384];

	VGA_MMIO_PageHandler() {
		flags=PFLAG_NOCODE;
		//memset(&regmem[0], 0, sizeof(regmem));
	}
	void writeb(PhysPt addr,Bitu val) {
		Bitu port = addr & 0xffff;
		if(port >= 0x82E8) IO_WriteB(port, val);
		if(port <= 0x4000) {
			if(port == 0x0000) {
				IO_WriteB(0xe2e0, val);
			} else {
				IO_WriteB(0xe2e8, val);
			}
		}
		//LOG_MSG("MMIO: Write byte to %x with %x", addr, val);
	}
	void writew(PhysPt addr,Bitu val) {
		Bitu port = addr & 0xffff;
		if(port >= 0x82E8) IO_WriteW(port, val);
		if(port == 0x8118) IO_WriteW(0x9ae8, val);
		if(port <= 0x4000) {
			if(port == 0x0000) {
                IO_WriteW(0xe2e0, val);
			} else {
			IO_WriteW(0xe2e8, val);
		}
		}
		//LOG_MSG("MMIO: Write word to %x with %x", addr, val);	
	}
	void writed(PhysPt addr,Bitu val) {
		Bitu port = addr & 0xffff;
		if(port >= 0x82E8) IO_WriteD(port, val);
		if(port == 0x8100) {
			IO_WriteW(0x86e8, (val >> 16));
			IO_WriteW(0x82e8, (val & 0xffff));
		}
		if(port == 0x8148) {
			IO_WriteW(0x96e8, (val >> 16));
			IO_WriteW(0xbee8, (val & 0xffff));
		}
		if(port <= 0x4000) {
			if(port == 0x0000) {
				IO_WriteW(0xe2e0, (val & 0xffff));
				IO_WriteW(0xe2e8, (val >> 16));
			} else {
			IO_WriteW(0xe2e8, (val & 0xffff));
			IO_WriteW(0xe2e8, (val >> 16));
		}
		}

		//LOG_MSG("MMIO: Write dword to %x with %x", addr, val);
	}

	Bitu readb(PhysPt addr) {
		//LOG_MSG("MMIO: Read byte from %x", addr);

		return 0x00;
	}
	Bitu readw(PhysPt addr) {
		Bitu port = addr & 0xffff;
		if(port >= 0x82E8) return IO_ReadW(port);
		//LOG_MSG("MMIO: Read word from %x", addr);
		return 0x00;
	}
	Bitu readd(PhysPt addr) {
		//LOG_MSG("MMIO: Read dword from %x", addr);
		return 0x00;
	}

};

class VGA_TANDY_PageHandler : public PageHandler {
public:
	VGA_TANDY_PageHandler() {
		flags=PFLAG_READABLE|PFLAG_WRITEABLE;
//			|PFLAG_NOCODE;
	}
	HostPt GetHostPt(Bitu phys_page) {
		if (phys_page>=0xb8) {
			phys_page-=0xb8;
			return &vga.mem.linear[(vga.tandy.mem_bank << 14)+(phys_page * 4096)];
		} else {
			phys_page-=0x80;
			return &vga.mem.linear[phys_page * 4096];
		}
	}
};


static struct vg {
	VGA_MAP_PageHandler hmap;
	VGA_TEXT_PageHandler htext;
	VGA_TANDY_PageHandler htandy;
	VGA_256_PageHandler h256;
	VGA_16_PageHandler h16;
	VGA_MMIO_PageHandler mmio;
} vgaph;


void VGA_SetupHandlers(void) {
	PageHandler * range_handler;
	switch (machine) {
	case MCH_CGA:
		range_handler=&vgaph.hmap;
		goto range_b800;
	case MCH_HERC:
		range_handler=&vgaph.hmap;
		if (vga.herc.mode_control&0x80) goto range_b800;
		else goto range_b000;
	case MCH_TANDY:
		range_handler=&vgaph.htandy;
		MEM_SetPageHandler(0x80,32,range_handler);
		goto range_b800;
	}
	switch (vga.mode) {
	case M_ERROR:
		return;
	case M_LIN8:
		range_handler=&vgaph.hmap;
		break;
	case M_VGA:
		if (vga.config.chained) {
			range_handler=&vgaph.hmap;
		} else {
			range_handler=&vgaph.h256;
		}
		break;
	case M_EGA16:
		range_handler=&vgaph.h16;
		break;	
	case M_TEXT:
		/* Check if we're not in odd/even mode */
		if (vga.gfx.miscellaneous & 0x2) range_handler=&vgaph.hmap;
		else range_handler=&vgaph.htext;
		break;
	case M_CGA4:
	case M_CGA2:
		range_handler=&vgaph.hmap;
		break;
	}
	switch ((vga.gfx.miscellaneous >> 2) & 3) {
	case 0:
		vgapages.base=VGA_PAGE_A0;
		vgapages.mask=0x1ffff;
		MEM_SetPageHandler(VGA_PAGE_A0,32,range_handler);
		break;
	case 1:
		vgapages.base=VGA_PAGE_A0;
		vgapages.mask=0xffff;
		MEM_SetPageHandler(VGA_PAGE_A0,16,range_handler);
		MEM_ResetPageHandler(VGA_PAGE_B0,16);
		break;
	case 2:
range_b000:
		vgapages.base=VGA_PAGE_B0;
		vgapages.mask=0x7fff;
		MEM_SetPageHandler(VGA_PAGE_B0,8,range_handler);
		MEM_ResetPageHandler(VGA_PAGE_A0,16);
		MEM_ResetPageHandler(VGA_PAGE_B8,8);
		break;
	case 3:
range_b800:
		vgapages.base=VGA_PAGE_B8;
		vgapages.mask=0x7fff;
		MEM_SetPageHandler(VGA_PAGE_B8,8,range_handler);
		MEM_ResetPageHandler(VGA_PAGE_A0,16);
		MEM_ResetPageHandler(VGA_PAGE_B0,8);
		break;
	}

	if(((vga.s3.ext_mem_ctrl & 0x10) != 0x00) && (vga.mode == M_LIN8)) MEM_SetPageHandler(VGA_PAGE_A0, 16, &vgaph.mmio);

	PAGING_ClearTLB();
}

static bool lfb_update;

static void VGA_DoUpdateLFB(Bitu val) {
	lfb_update=false;
	MEM_SetLFB(vga.s3.la_window << 4 ,sizeof(vga.mem.linear)/4096,&vga.mem.linear[0]);
}

void VGA_StartUpdateLFB(void) {
	if (!lfb_update) {
		lfb_update=true;
		PIC_AddEvent(VGA_DoUpdateLFB,0.100f);	//100 microseconds later
	}
}

void VGA_MapMMIO(void) {
	MEM_SetPageHandler(VGA_PAGE_A0, 16, &vgaph.mmio);

}

void VGA_UnmapMMIO(void) {
	//MEM_SetPageHandler(VGA_PAGE_A0, &ram_page_handler);
}


void VGA_SetupMemory() {
	memset((void *)&vga.mem,0,512*1024*4);
}
