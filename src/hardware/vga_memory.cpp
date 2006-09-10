/*
 *  Copyright (C) 2002-2006  The DOSBox Team
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
		val=((val >> vga.config.data_rotate) | (val << (8-vga.config.data_rotate)));
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

/* Gonna assume that whoever maps vga memory, maps it on 32/64kb boundary */

#define VGA_PAGES		(128/4)
#define VGA_PAGE_A0		(0xA0000/4096)
#define VGA_PAGE_B0		(0xB0000/4096)
#define VGA_PAGE_B8		(0xB8000/4096)

static struct {
	Bitu base, mask;
} vgapages;
	
class VGA_UnchainedRead_Handler : public PageHandler {
public:
	Bitu readHandler(PhysPt start) {
		start += vga.s3.svga_bank.fullbank;
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
public:
	Bitu readb(PhysPt addr) {
		addr = PAGING_GetPhysicalAddress(addr) & 0xffff;
		return readHandler(addr);
	}
	Bitu readw(PhysPt addr) {
		addr = PAGING_GetPhysicalAddress(addr) & 0xffff;
		return 
			(readHandler(addr+0) << 0) |
			(readHandler(addr+1) << 8);
	}
	Bitu readd(PhysPt addr) {
		addr = PAGING_GetPhysicalAddress(addr) & 0xffff;
		return 
			(readHandler(addr+0) << 0)  |
			(readHandler(addr+1) << 8)  |
			(readHandler(addr+2) << 16) |
			(readHandler(addr+3) << 24);
	}
};

class VGA_Chained_ReadHandler : public PageHandler {
public:
	Bitu readHandler(PhysPt addr) {
		if(vga.mode == M_VGA)
			return vga.mem.linear[((addr&~3)<<2)|(addr&3)];
		return vga.mem.linear[addr];
	}
public:
	Bitu readb(PhysPt addr) {
		addr = PAGING_GetPhysicalAddress(addr) & 0xffff;
		return readHandler(addr);
	}
	Bitu readw(PhysPt addr) {
		addr = PAGING_GetPhysicalAddress(addr) & 0xffff;
		return 
			(readHandler(addr+0) << 0) |
			(readHandler(addr+1) << 8);
	}
	Bitu readd(PhysPt addr) {
		addr = PAGING_GetPhysicalAddress(addr) & 0xffff;
		return 
			(readHandler(addr+0) << 0)  |
			(readHandler(addr+1) << 8)  |
			(readHandler(addr+2) << 16) |
			(readHandler(addr+3) << 24);
	}
};

class VGA_ChainedEGA_Handler : public VGA_Chained_ReadHandler {
public:
	void writeHandler(PhysPt start, Bit8u val) {
		Bit32u data=ModeOperation(val);
		/* Update video memory and the pixel buffer */
		VGA_Latch pixels;
		vga.mem.linear[start] = val;
		start >>= 2;
		pixels.d=vga.mem.latched[start].d;

		Bit8u * write_pixels=&vga.mem.linear[512*1024+(start<<3)];

		Bit32u colors0_3, colors4_7;
		VGA_Latch temp;temp.d=(pixels.d>>4) & 0x0f0f0f0f;
		colors0_3 = 
			Expand16Table[0][temp.b[0]] |
			Expand16Table[1][temp.b[1]] |
			Expand16Table[2][temp.b[2]] |
			Expand16Table[3][temp.b[3]];
		*(Bit32u *)write_pixels=colors0_3;
		temp.d=pixels.d & 0x0f0f0f0f;
		colors4_7 = 
			Expand16Table[0][temp.b[0]] |
			Expand16Table[1][temp.b[1]] |
			Expand16Table[2][temp.b[2]] |
			Expand16Table[3][temp.b[3]];
		*(Bit32u *)(write_pixels+4)=colors4_7;
	}
public:	
	VGA_ChainedEGA_Handler()  {
		flags=PFLAG_NOCODE;
	}
	void writeb(PhysPt addr,Bitu val) {
		addr = PAGING_GetPhysicalAddress(addr) & 0xffff;
		writeHandler(addr+0,(Bit8u)(val >> 0));
	}
	void writew(PhysPt addr,Bitu val) {
		addr = PAGING_GetPhysicalAddress(addr) & 0xffff;
		writeHandler(addr+0,(Bit8u)(val >> 0));
		writeHandler(addr+1,(Bit8u)(val >> 8));
	}
	void writed(PhysPt addr,Bitu val) {
		addr = PAGING_GetPhysicalAddress(addr) & 0xffff;
		writeHandler(addr+0,(Bit8u)(val >> 0));
		writeHandler(addr+1,(Bit8u)(val >> 8));
		writeHandler(addr+2,(Bit8u)(val >> 16));
		writeHandler(addr+3,(Bit8u)(val >> 24));
	}
};

class VGA_UnchainedEGA_Handler : public VGA_UnchainedRead_Handler {
public:
	void writeHandler(PhysPt start, Bit8u val) {
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
public:	
	VGA_UnchainedEGA_Handler()  {
		flags=PFLAG_NOCODE;
	}
	void writeb(PhysPt addr,Bitu val) {
		addr = PAGING_GetPhysicalAddress(addr) & 0xffff;
		writeHandler(addr+0,(Bit8u)(val >> 0));
	}
	void writew(PhysPt addr,Bitu val) {
		addr = PAGING_GetPhysicalAddress(addr) & 0xffff;
		writeHandler(addr+0,(Bit8u)(val >> 0));
		writeHandler(addr+1,(Bit8u)(val >> 8));
	}
	void writed(PhysPt addr,Bitu val) {
		addr = PAGING_GetPhysicalAddress(addr) & 0xffff;
		writeHandler(addr+0,(Bit8u)(val >> 0));
		writeHandler(addr+1,(Bit8u)(val >> 8));
		writeHandler(addr+2,(Bit8u)(val >> 16));
		writeHandler(addr+3,(Bit8u)(val >> 24));
	}
};


class VGA_ChainedVGA_Handler : public VGA_Chained_ReadHandler {
public:
	void writeHandler(PhysPt addr, Bitu val) {
		// No need to check for compatible chains here, this one is only enabled if that bit is set
		vga.mem.linear[((addr&~3)<<2)|(addr&3)] = val;
		// Linearized version for faster rendering
		vga.mem.linear[512*1024+addr] = val;
		if (addr >= 320) return;
		// And replicate the first line
		vga.mem.linear[512*1024+addr+64*1024] = val;
	}
public:	
	VGA_ChainedVGA_Handler()  {
		flags=PFLAG_NOCODE;
	}
	void writeb(PhysPt addr,Bitu val) {
		addr = PAGING_GetPhysicalAddress(addr) & 0xffff;
		writeHandler(addr+0,(Bit8u)(val >> 0));
	}
	void writew(PhysPt addr,Bitu val) {
		addr = PAGING_GetPhysicalAddress(addr) & 0xffff;
		writeHandler(addr+0,(Bit8u)(val >> 0));
		writeHandler(addr+1,(Bit8u)(val >> 8));
	}
	void writed(PhysPt addr,Bitu val) {
		addr = PAGING_GetPhysicalAddress(addr) & 0xffff;
		writeHandler(addr+0,(Bit8u)(val >> 0));
		writeHandler(addr+1,(Bit8u)(val >> 8));
		writeHandler(addr+2,(Bit8u)(val >> 16));
		writeHandler(addr+3,(Bit8u)(val >> 24));
	}
};

class VGA_UnchainedVGA_Handler : public VGA_UnchainedRead_Handler {
public:
	void writeHandler( PhysPt addr, Bit8u val ) {
		addr += vga.s3.svga_bank.fullbank;
		Bit32u data=ModeOperation(val);
		VGA_Latch pixels;
		pixels.d=vga.mem.latched[addr].d;
		pixels.d&=vga.config.full_not_map_mask;
		pixels.d|=(data & vga.config.full_map_mask);
		vga.mem.latched[addr].d=pixels.d;
		if(vga.config.compatible_chain4)
			vga.mem.latched[addr+64*1024].d=pixels.d; 
	}
public:
	VGA_UnchainedVGA_Handler()  {
		flags=PFLAG_NOCODE;
	}
	void writeb(PhysPt addr,Bitu val) {
		addr = PAGING_GetPhysicalAddress(addr) & 0xffff;
		writeHandler(addr+0,(Bit8u)(val >> 0));
	}
	void writew(PhysPt addr,Bitu val) {
		addr = PAGING_GetPhysicalAddress(addr) & 0xffff;
		writeHandler(addr+0,(Bit8u)(val >> 0));
		writeHandler(addr+1,(Bit8u)(val >> 8));
	}
	void writed(PhysPt addr,Bitu val) {
		addr = PAGING_GetPhysicalAddress(addr) & 0xffff;
		writeHandler(addr+0,(Bit8u)(val >> 0));
		writeHandler(addr+1,(Bit8u)(val >> 8));
		writeHandler(addr+2,(Bit8u)(val >> 16));
		writeHandler(addr+3,(Bit8u)(val >> 24));
	}
};

class VGA_TEXT_PageHandler : public PageHandler {
public:
	VGA_TEXT_PageHandler() {
		flags=PFLAG_NOCODE;
	}
	Bitu readb(PhysPt addr) {
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
		return vga.draw.font[addr];
	}
	void writeb(PhysPt addr,Bitu val){
		addr = PAGING_GetPhysicalAddress(addr) & vgapages.mask;
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
	HostPt GetHostReadPt(Bitu phys_page) {
 		phys_page-=vgapages.base;
		return &vga.mem.linear[vga.s3.svga_bank.fullbank+phys_page*4096];
	}
	HostPt GetHostWritePt(Bitu phys_page) {
 		phys_page-=vgapages.base;
		return &vga.mem.linear[vga.s3.svga_bank.fullbank+phys_page*4096];
	}
};


class VGA_LIN4Linear_Handler : public VGA_UnchainedEGA_Handler {
public:
	VGA_LIN4Linear_Handler() {
		flags=PFLAG_READABLE|PFLAG_WRITEABLE|PFLAG_NOCODE;
	}
	void writeb(PhysPt addr,Bitu val) {
		addr = (PAGING_GetPhysicalAddress(addr) - vga.lfb.addr) & (512*1024-1);
		writeHandler(addr+0,(Bit8u)(val >> 0));
	}
	void writew(PhysPt addr,Bitu val) {
		addr = (PAGING_GetPhysicalAddress(addr) - vga.lfb.addr) & (512*1024-1);
		writeHandler(addr+0,(Bit8u)(val >> 0));
		writeHandler(addr+1,(Bit8u)(val >> 8));
	}
	void writed(PhysPt addr,Bitu val) {
		addr = (PAGING_GetPhysicalAddress(addr) - vga.lfb.addr) & (512*1024-1);
		writeHandler(addr+0,(Bit8u)(val >> 0));
		writeHandler(addr+1,(Bit8u)(val >> 8));
		writeHandler(addr+2,(Bit8u)(val >> 16));
		writeHandler(addr+3,(Bit8u)(val >> 24));
	}
	Bitu readb(PhysPt addr) {
		addr = (PAGING_GetPhysicalAddress(addr) - vga.lfb.addr) & (512*1024-1);
		return readHandler(addr);
	}
	Bitu readw(PhysPt addr) {
		addr = (PAGING_GetPhysicalAddress(addr) - vga.lfb.addr) & (512*1024-1);
		return 
			(readHandler(addr+0) << 0) |
			(readHandler(addr+1) << 8);
	}
	Bitu readd(PhysPt addr) {
		addr = (PAGING_GetPhysicalAddress(addr) - vga.lfb.addr) & (512*1024-1);
		return 
			(readHandler(addr+0) << 0)  |
			(readHandler(addr+1) << 8)  |
			(readHandler(addr+2) << 16) |
			(readHandler(addr+3) << 24);
	}
};

class VGA_LIN4Banked_Handler : public VGA_UnchainedEGA_Handler {
public:
	VGA_LIN4Banked_Handler() {
		flags=PFLAG_NOCODE;
	}
	void writeb(PhysPt addr,Bitu val) {
		addr = vga.s3.svga_bank.fullbank + (PAGING_GetPhysicalAddress(addr) & 0xffff);
		addr &= (512*1024-1);
		writeHandler(addr+0,(Bit8u)(val >> 0));
	}
	void writew(PhysPt addr,Bitu val) {
		addr = vga.s3.svga_bank.fullbank + (PAGING_GetPhysicalAddress(addr) & 0xffff);
		addr &= (512*1024-1);
		writeHandler(addr+0,(Bit8u)(val >> 0));
		writeHandler(addr+1,(Bit8u)(val >> 8));
	}
	void writed(PhysPt addr,Bitu val) {
		addr = vga.s3.svga_bank.fullbank + (PAGING_GetPhysicalAddress(addr) & 0xffff);
		addr &= (512*1024-1);
		writeHandler(addr+0,(Bit8u)(val >> 0));
		writeHandler(addr+1,(Bit8u)(val >> 8));
		writeHandler(addr+2,(Bit8u)(val >> 16));
		writeHandler(addr+3,(Bit8u)(val >> 24));
	}
	Bitu readb(PhysPt addr) {
		addr = vga.s3.svga_bank.fullbank + (PAGING_GetPhysicalAddress(addr) & 0xffff);
		addr &= (512*1024-1);
		return readHandler(addr);
	}
	Bitu readw(PhysPt addr) {
		addr = vga.s3.svga_bank.fullbank + (PAGING_GetPhysicalAddress(addr) & 0xffff);
		addr &= (512*1024-1);
		return 
			(readHandler(addr+0) << 0) |
			(readHandler(addr+1) << 8);
	}
	Bitu readd(PhysPt addr) {
		addr = vga.s3.svga_bank.fullbank + (PAGING_GetPhysicalAddress(addr) & 0xffff);
		addr &= (512*1024-1);
		return 
			(readHandler(addr+0) << 0)  |
			(readHandler(addr+1) << 8)  |
			(readHandler(addr+2) << 16) |
			(readHandler(addr+3) << 24);
	}
};


class VGA_LFBChanges_Handler : public PageHandler {
public:
	VGA_LFBChanges_Handler() {
		flags=PFLAG_NOCODE;
	}
	Bitu readb(PhysPt addr) {
		addr = PAGING_GetPhysicalAddress(addr) - vga.lfb.addr;
		return *(Bit8u*)(&vga.mem.linear[addr]);
	}
	Bitu readw(PhysPt addr) {
		addr = PAGING_GetPhysicalAddress(addr) - vga.lfb.addr;
		return *(Bit16u*)(&vga.mem.linear[addr]);
	}
	Bitu readd(PhysPt addr) {
		addr = PAGING_GetPhysicalAddress(addr) - vga.lfb.addr;
		return *(Bit32u*)(&vga.mem.linear[addr]);
	}
	void writeb(PhysPt addr,Bitu val) {
		addr = PAGING_GetPhysicalAddress(addr) - vga.lfb.addr;
		*(Bit8u*)(&vga.mem.linear[addr]) = val;
		vga.changed[addr >> VGA_CHANGE_SHIFT] = 1;
	}
	void writew(PhysPt addr,Bitu val) {
		addr = PAGING_GetPhysicalAddress(addr) - vga.lfb.addr;
		*(Bit16u*)(&vga.mem.linear[addr]) = val;
		vga.changed[addr >> VGA_CHANGE_SHIFT] = 1;
	}
	void writed(PhysPt addr,Bitu val) {
		addr = PAGING_GetPhysicalAddress(addr) - vga.lfb.addr;
		*(Bit32u*)(&vga.mem.linear[addr]) = val;
		vga.changed[addr >> VGA_CHANGE_SHIFT] = 1;
	}
};

class VGA_LFB_Handler : public PageHandler {
public:
	VGA_LFB_Handler() {
		flags=PFLAG_READABLE|PFLAG_WRITEABLE|PFLAG_NOCODE;
	}
	HostPt GetHostReadPt( Bitu phys_page ) {
		phys_page -= vga.lfb.page;
		return &vga.mem.linear[phys_page * 4096];
	}
	HostPt GetHostWritePt( Bitu phys_page ) {
		return GetHostReadPt( phys_page );
	}
};

class VGA_MMIO_Handler : public PageHandler {
public:
	Bit16u regmem[16384];
	VGA_MMIO_Handler() {
		flags=PFLAG_NOCODE;
		//memset(&regmem[0], 0, sizeof(regmem));
	}
	void writeb(PhysPt addr,Bitu val) {
		Bitu port = PAGING_GetPhysicalAddress(addr) & 0xffff;
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
		Bitu port = PAGING_GetPhysicalAddress(addr) & 0xffff;
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
		Bitu port = PAGING_GetPhysicalAddress(addr) & 0xffff;
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
		Bitu port = PAGING_GetPhysicalAddress(addr) & 0xffff;
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
	HostPt GetHostReadPt(Bitu phys_page) {
		if (phys_page>=0xb8) {
			phys_page-=0xb8;
			return &vga.mem.linear[(vga.tandy.mem_bank << 14)+(phys_page * 4096)];
		} else {
			if (machine==MCH_TANDY) phys_page-=0x80;
			return &vga.mem.linear[phys_page * 4096];
		}
	}
	HostPt GetHostWritePt(Bitu phys_page) {
		return GetHostReadPt( phys_page );
	}
};

class VGA_PCJR_PageHandler : public PageHandler {
public:
	VGA_PCJR_PageHandler() {
		flags=PFLAG_READABLE|PFLAG_WRITEABLE;
	}
	HostPt GetHostReadPt(Bitu phys_page) {
		phys_page-=0xb8;
		if (!vga.tandy.is_32k_mode) phys_page&=0x03;
		return MemBase+(vga.tandy.mem_bank << 14)+(phys_page * 4096);
	}
	HostPt GetHostWritePt(Bitu phys_page) {
		return GetHostReadPt( phys_page );
	}
};


static struct vg {
	VGA_MAP_PageHandler			map;
	VGA_TEXT_PageHandler		text;
	VGA_TANDY_PageHandler		tandy;
	VGA_ChainedEGA_Handler		cega;
	VGA_ChainedVGA_Handler		cvga;
	VGA_UnchainedEGA_Handler	uega;
	VGA_UnchainedVGA_Handler	uvga;
	VGA_PCJR_PageHandler		hpcjr;
	VGA_LIN4Banked_Handler		l4banked;
	VGA_LIN4Linear_Handler		l4linear;
	VGA_LFB_Handler				lfb;
	VGA_LFBChanges_Handler		lfbchanges;
	VGA_MMIO_Handler			mmio;
} vgaph;


void VGA_SetupHandlers(void) {
	PageHandler * range_handler;
	switch (machine) {
	case MCH_CGA:
		range_handler=&vgaph.map;
		goto range_b800;
	case MCH_HERC:
		range_handler=&vgaph.map;
		if (vga.herc.mode_control&0x80) goto range_b800;
		else goto range_b000;
	case MCH_TANDY:
		range_handler=&vgaph.tandy;
		MEM_SetPageHandler(0x80,32,range_handler);
		goto range_b800;
	case MCH_PCJR:
		range_handler=&vgaph.hpcjr;
//		MEM_SetPageHandler(vga.tandy.mem_bank<<2,vga.tandy.is_32k_mode ? 0x08 : 0x04,range_handler);
		goto range_b800;
	}
	switch (vga.mode) {
	case M_ERROR:
		return;
	case M_LIN4:
		range_handler=&vgaph.l4banked;
		break;	
	case M_LIN15:
	case M_LIN16:
	case M_LIN32:
		range_handler=&vgaph.map;
		break;
	case M_LIN8:
	case M_VGA:
		if (vga.config.chained) {
			if(vga.config.compatible_chain4)
				range_handler = &vgaph.cvga;
			else
				range_handler=&vgaph.map;
		} else {
			range_handler=&vgaph.uvga;
		}
		break;
	case M_EGA:
		if (vga.config.chained) 
			range_handler=&vgaph.cega;
		else
			range_handler=&vgaph.uega;
		break;	
	case M_TEXT:
		/* Check if we're not in odd/even mode */
		if (vga.gfx.miscellaneous & 0x2) range_handler=&vgaph.map;
		else range_handler=&vgaph.text;
		break;
	case M_CGA4:
	case M_CGA2:
		range_handler=&vgaph.map;
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

	if(((vga.s3.ext_mem_ctrl & 0x10) != 0x00) && (vga.mode == M_LIN8))
		MEM_SetPageHandler(VGA_PAGE_A0, 16, &vgaph.mmio);

	PAGING_ClearTLB();
}

void VGA_StartUpdateLFB(void) {
	vga.lfb.page = vga.s3.la_window << 4;
	vga.lfb.addr = vga.s3.la_window << 16;
	switch (vga.mode) {
	case M_LIN4:
		vga.lfb.handler = &vgaph.l4linear;
		break;
	default:
		vga.lfb.handler = &vgaph.lfbchanges;
		break;
	}
	MEM_SetLFB(vga.s3.la_window << 4 ,sizeof(vga.mem.linear)/4096, vga.lfb.handler );
}

void VGA_MapMMIO(void) {
	MEM_SetPageHandler(VGA_PAGE_A0, 16, &vgaph.mmio);
}

void VGA_UnmapMMIO(void) {
	//MEM_SetPageHandler(VGA_PAGE_A0, &ram_page_handler);
}


void VGA_SetupMemory() {
	memset((void *)&vga.mem,0,512*1024*4);
	vga.s3.svga_bank.fullbank=0;
	if (machine==MCH_PCJR) {
		/* PCJr does not have dedicated graphics memory but uses
		   conventional memory below 128k */
		vga.gfxmem_start=GetMemBase();
	} else vga.gfxmem_start=&vga.mem.linear[0];
}
