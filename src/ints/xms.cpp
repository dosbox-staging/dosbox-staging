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
#include "callback.h"
#include "mem.h"
#include "regs.h"
#include "dos_system.h"
#include "setup.h"
#include "inout.h"
#include "xms.h"

#define XMS_HANDLES							50		/* 50 XMS Memory Blocks */ 
#define XMS_VERSION    						0x0300	/* version 3.00 */
#define XMS_DRIVER_VERSION					0x0301	/* my driver version 3.01 */

#define	XMS_GET_VERSION						0x00
#define	XMS_ALLOCATE_HIGH_MEMORY			0x01
#define	XMS_FREE_HIGH_MEMORY				0x02
#define	XMS_GLOBAL_ENABLE_A20				0x03
#define	XMS_GLOBAL_DISABLE_A20				0x04
#define	XMS_LOCAL_ENABLE_A20				0x05
#define	XMS_LOCAL_DISABLE_A20				0x06
#define	XMS_QUERY_A20						0x07
#define	XMS_QUERY_FREE_EXTENDED_MEMORY		0x08
#define	XMS_ALLOCATE_EXTENDED_MEMORY		0x09
#define	XMS_FREE_EXTENDED_MEMORY			0x0a
#define	XMS_MOVE_EXTENDED_MEMORY_BLOCK		0x0b
#define	XMS_LOCK_EXTENDED_MEMORY_BLOCK		0x0c
#define	XMS_UNLOCK_EXTENDED_MEMORY_BLOCK	0x0d
#define	XMS_GET_EMB_HANDLE_INFORMATION		0x0e
#define	XMS_RESIZE_EXTENDED_MEMORY_BLOCK	0x0f
#define	XMS_ALLOCATE_UMB					0x10
#define	XMS_DEALLOCATE_UMB					0x11

#define	HIGH_MEMORY_IN_USE					0x92
#define	HIGH_MEMORY_NOT_ALLOCATED			0x93
#define XMS_OUT_OF_SPACE					0xa0
#define XMS_OUT_OF_HANDLES					0xa1
#define XMS_INVALID_HANDLE					0xa2
#define XMS_BLOCK_NOT_LOCKED				0xaa
#define XMS_BLOCK_LOCKED					0xab

struct XMS_Block {
	Bit16u	prev,next;
	Bit16u	size;					/* Size in kb's */
	PhysPt	phys;
	Bit8u	locked;
	bool	allocated;
	bool	active;
};

#pragma pack (push,1)
struct XMS_MemMove{
	Bit32u length;
	Bit16u src_handle;
	union {
		RealPt realpt;
		Bit32u offset;
	} src;
	Bit16u dest_handle;
	union {
		RealPt realpt;
		Bit32u offset;
	} dest;

} GCC_ATTRIBUTE(packed);
#pragma pack (pop)

Bitu XMS_EnableA20(bool enable)
{
	Bit8u val = IO_Read	(0x92);
	if (enable) IO_Write(0x92,val | 2);
	else		IO_Write(0x92,val & ~2);
	return 0;
};

Bitu XMS_GetEnabledA20(void)
{
	return (IO_Read(0x92)&2)>0;
};

static Bit16u call_xms;
static RealPt xms_callback;

static XMS_Block xms_handles[XMS_HANDLES];

Bitu XMS_QueryFreeMemory(Bit16u& largestFree, Bit16u& totalFree) {
	/* Scan the tree for free memory and find largest free block */
	Bit16u index=1;
	largestFree=totalFree=0;
	while (xms_handles[index].active) {
		if (!xms_handles[index].allocated) {
			if (xms_handles[index].size>largestFree) largestFree=xms_handles[index].size;
			totalFree+=xms_handles[index].size;
		}
		if (xms_handles[index].next<XMS_HANDLES) index=xms_handles[index].next;
		else break;
	}
	if (!totalFree) return XMS_OUT_OF_SPACE;
	return 0;
};

Bitu XMS_AllocateMemory(Bitu size, Bit16u& handle)
// size = kb
{
	Bit16u index=1;
	/* First make reg_dx a multiple of PAGE_KB */
	if (size & (PAGE_KB-1)) size=(size&(~(PAGE_KB-1)))+PAGE_KB;
	while (xms_handles[index].active) {
		/* Find a free block, check if there's enough size */
		if (!xms_handles[index].allocated && xms_handles[index].size>=size) {
			/* Check if block is bigger than request */
			if (xms_handles[index].size>size) {
				/* Split Block, find new handle and split up memory */
				Bit16u new_index=1;
				while (new_index<XMS_HANDLES) {
					if (!xms_handles[new_index].active) goto foundnew;
					new_index++;
				}
				handle = 0;
				return XMS_OUT_OF_HANDLES;
	foundnew:
				xms_handles[new_index].next=xms_handles[index].next;
				xms_handles[new_index].prev=index;
				xms_handles[index].next=new_index;
				xms_handles[index].locked=0;
				xms_handles[new_index].active=true;
				xms_handles[new_index].allocated=0;
				xms_handles[new_index].locked=0;
				xms_handles[new_index].size=xms_handles[index].size-size;
				xms_handles[new_index].phys=xms_handles[index].phys+size*1024;
				xms_handles[index].size=size;
			}
			/* Use the data from handle index to allocate the actual memory */
			handle = index;
			xms_handles[index].allocated = 1;
			//CheckAllocationArea(xms_handles[index].phys,xms_handles[index].size*1024);
			return 0;
		}	
		/* Not a free block or too small advance to next one if possible */
		if (xms_handles[index].next < XMS_HANDLES ) index=xms_handles[index].next;
		else break;
	}
	/* Found no good blocks give some errors */
	return XMS_OUT_OF_SPACE;
};

Bitu XMS_FreeMemory(Bitu handle)
{
	/* Check for a valid handle */
	if (!handle || (handle>=XMS_HANDLES) || !xms_handles[handle].active || !xms_handles[handle].allocated ) {
		return XMS_INVALID_HANDLE;
	}
	/* Free the memory in the block and merge the blocks previous and next block */
	Bit16u prev=xms_handles[handle].prev;
	Bit16u next=xms_handles[handle].next;
	xms_handles[handle].allocated=0;
	if ((next<XMS_HANDLES) && !xms_handles[next].allocated) {
		xms_handles[next].active=false;
		xms_handles[handle].size+=xms_handles[next].size;
		xms_handles[handle].next=xms_handles[next].next;
		next=xms_handles[handle].next;
		if (next<XMS_HANDLES) xms_handles[next].prev=handle;
	}
	if ((prev<XMS_HANDLES) && !xms_handles[prev].allocated) {
		xms_handles[handle].active=false;
		xms_handles[prev].size+=xms_handles[handle].size;
		next=xms_handles[handle].next;
		xms_handles[prev].next=next;
		if (next<XMS_HANDLES) xms_handles[next].prev=prev;
	}
	return 0;
};

Bitu XMS_MoveMemory(PhysPt bpt)
{
	XMS_MemMove block;
	/* Fill the block with mem_read's and shit */
	block.length=mem_readd(bpt+offsetof(XMS_MemMove,length));
	block.src_handle=mem_readw(bpt+offsetof(XMS_MemMove,src_handle));
	block.src.offset=mem_readd(bpt+offsetof(XMS_MemMove,src.offset));
	block.dest_handle=mem_readw(bpt+offsetof(XMS_MemMove,dest_handle));
	block.dest.offset=mem_readd(bpt+offsetof(XMS_MemMove,dest.offset));
	PhysPt src,dest;
	if (block.src_handle) {
		if ((block.src_handle>=XMS_HANDLES) || !xms_handles[block.src_handle].active ||!xms_handles[block.src_handle].allocated) {
			return 0xa3;	/* Src Handle invalid */
		}
		if (block.src.offset>=(xms_handles[block.src_handle].size*1024U)) {
			return 0xa4;	/* Src Offset invalid */
		}
		if (block.length>xms_handles[block.src_handle].size*1024U-block.src.offset) {
			return 0xa7;	/* Length invalid */
		}
		src=xms_handles[block.src_handle].phys+block.src.offset;
	} else {
		src=Real2Phys(block.src.realpt);
	}
	if (block.dest_handle) {
		if ((block.dest_handle>=XMS_HANDLES) || !xms_handles[block.dest_handle].active ||!xms_handles[block.dest_handle].allocated) {
			return 0xa3;	/* Dest Handle invalid */
		}
		if (block.dest.offset>=(xms_handles[block.dest_handle].size*1024U)) {
			return 0xa4;	/* Dest Offset invalid */
		}
		if (block.length>xms_handles[block.dest_handle].size*1024U-block.dest.offset) {
			return 0xa7;	/* Length invalid */
		}
		dest=xms_handles[block.dest_handle].phys+block.dest.offset;
	} else {
		dest=Real2Phys(block.dest.realpt);
	}
	MEM_BlockCopy(dest,src,block.length);
	return 0;
}

Bitu XMS_LockMemory(Bitu handle, Bit32u& address)
{
	/* Check for a valid handle */
	if (!handle || (handle>=XMS_HANDLES) || !xms_handles[handle].active || !xms_handles[handle].allocated ) {
		return XMS_INVALID_HANDLE;
	}
	if (xms_handles[handle].locked<255) xms_handles[handle].locked++;
	address = xms_handles[handle].phys;
	return 0;
};

Bitu XMS_UnlockMemory(Bitu handle)
{
	/* Check for a valid handle */
	if (!handle || (handle>=XMS_HANDLES) || !xms_handles[handle].active || !xms_handles[handle].allocated ) {
		return XMS_INVALID_HANDLE;
	}
	if (xms_handles[handle].locked) {
		xms_handles[handle].locked--;
		return 0;
	}
	return XMS_BLOCK_NOT_LOCKED;
};

Bitu XMS_GetHandleInformation(Bitu handle, Bit8u& lockCount, Bit8u& numFree, Bit16u& size)
{
	/* Check for a valid handle */
	if (!handle || (handle>=XMS_HANDLES) || !xms_handles[handle].active || !xms_handles[handle].allocated ) {
		return XMS_INVALID_HANDLE;
	}
	lockCount = xms_handles[handle].locked;
	/* Find available blocks */
	numFree=0;{ for (Bitu i=1;i<XMS_HANDLES;i++) if (!xms_handles[i].allocated) numFree++;}
	size=xms_handles[handle].size;
	return 0;
};

Bitu XMS_ResizeMemory(Bitu handle, Bitu newSize)
{
	/* Check for a valid handle */
	if (!handle || (handle>=XMS_HANDLES) || !xms_handles[handle].active || !xms_handles[handle].allocated ) {
		return XMS_INVALID_HANDLE;
	}
	// Block has to be unlocked
	if (xms_handles[handle].locked>0) return XMS_BLOCK_LOCKED;
	
	if (newSize<xms_handles[handle].size) {
		// shrink block...
		Bit16u	sizeDelta	= xms_handles[handle].size - newSize;
		Bit16u	next		= xms_handles[handle].next;
		
		if (next<XMS_HANDLES) {
			if (xms_handles[next].active && !xms_handles[next].allocated) {
				// add size / set phys
				xms_handles[next].size += sizeDelta;
				xms_handles[next].phys -= sizeDelta*1024;
			} else {
				// next block is in use, create a new lonely unused one :)
				Bit16u newindex=1;
				while (newindex<XMS_HANDLES) {
					if (!xms_handles[newindex].active) break;
					newindex++;
				}
				if (newindex>=XMS_HANDLES) return XMS_OUT_OF_HANDLES;
				xms_handles[newindex].active	= true;
				xms_handles[newindex].allocated = false;
				xms_handles[newindex].locked	= 0;
				xms_handles[newindex].prev		= handle;
				xms_handles[newindex].next		= next;
				xms_handles[newindex].phys		= xms_handles[handle].phys+newSize*1024;
				xms_handles[newindex].size		= sizeDelta;

				xms_handles[handle]  .next		= newindex;
				xms_handles[next]    .prev		= newindex;
			}
		}
		// Resize and allocate new mem 
		xms_handles[handle].size	  = newSize;
		xms_handles[handle].allocated = 1;
		//CheckAllocationArea(xms_handles[handle].phys,xms_handles[handle].size*1024);
		
	} else if (newSize>xms_handles[handle].size) {
		// Lets see if successor has enough free space to do that 
		Bit16u	oldSize		= xms_handles[handle].size;
		Bit16u	next		= xms_handles[handle].next;
		Bit16u	sizeDelta	= newSize - xms_handles[handle].size;
		if (next<XMS_HANDLES && !xms_handles[next].allocated) {
			if (xms_handles[next].size>sizeDelta) {
				// remove size from it
				xms_handles[next].size-=sizeDelta;
				xms_handles[next].phys+=sizeDelta*1024;
			} else if (xms_handles[next].size==sizeDelta) {
				// exact match, skip it 
				xms_handles[handle].next	= xms_handles[next].next;
				xms_handles[next].active	= false;
			} else {
				// Not enough mem available
				LOG(LOG_ERROR,"XMS: Resize failure: out of mem 1");
				return XMS_OUT_OF_SPACE;
			};
			// Resize and allocate new mem 
			xms_handles[handle].size		= newSize;
			xms_handles[handle].allocated	= 1;
			//CheckAllocationArea(xms_handles[handle].phys,xms_handles[handle].size*1024);
		} else {
			// No more free mem ?
			LOG(LOG_ERROR,"XMS: Resize failure: out of mem 2");
			return XMS_OUT_OF_SPACE;
		};
	};	
	return 0;
};


static bool multiplex_xms(void) {
	switch (reg_ax) {
	case 0x4300:					/* XMS installed check */
			reg_al=0x80;
			return true;
	case 0x4310:					/* XMS handler seg:offset */
			SegSet16(es,RealSeg(xms_callback));
			reg_bx=RealOff(xms_callback);
			return true;			
	}
	return false;

};

Bitu XMS_Handler(void) {
	switch (reg_ah) {

	case XMS_GET_VERSION:										/* 00 */
		reg_ax=XMS_VERSION;
		reg_bx=XMS_DRIVER_VERSION;
		reg_dx=0;	/* No we don't have HMA */
		break;
	case XMS_GLOBAL_ENABLE_A20:									/* 03 */
	case XMS_LOCAL_ENABLE_A20:									/* 05 */
		reg_bl = XMS_EnableA20(true);
		reg_ax = (reg_bl==0);
		break;
	case XMS_GLOBAL_DISABLE_A20:								/* 04 */
	case XMS_LOCAL_DISABLE_A20:									/* 06 */
		reg_bl = XMS_EnableA20(false);
		reg_ax = (reg_bl==0);
		break;	
	case XMS_QUERY_A20:											/* 07 */
		reg_ax = XMS_GetEnabledA20();
		reg_bl = 0;
		break;
	case XMS_QUERY_FREE_EXTENDED_MEMORY:						/* 08 */
		reg_bl = XMS_QueryFreeMemory(reg_ax,reg_dx);
		break;
	case XMS_ALLOCATE_EXTENDED_MEMORY:							/* 09 */
		reg_bl = XMS_AllocateMemory(reg_ax,reg_dx);
		reg_ax = (reg_bl==0);		// set ax to success/failure
		break;
	case XMS_FREE_EXTENDED_MEMORY:								/* 0a */
		reg_bl = XMS_FreeMemory(reg_dx);
		reg_ax = (reg_bl==0);
		break;
	case XMS_MOVE_EXTENDED_MEMORY_BLOCK:						/* 0b */
		reg_bl = XMS_MoveMemory(SegPhys(ds)+reg_si);
		reg_ax = (reg_bl==0);
		break;
	case XMS_LOCK_EXTENDED_MEMORY_BLOCK: {						/* 0c */
		Bit32u address;
		reg_bl = XMS_LockMemory(reg_dx, address);
		if (reg_bl==0) { // success
			reg_bx=(Bit16u)(address & 0xFFFF);
			reg_dx=(Bit16u)(address >> 16);
		};
		reg_ax = (reg_bl==0);
		}; break;
	case XMS_UNLOCK_EXTENDED_MEMORY_BLOCK:						/* 0d */
		reg_bl = XMS_UnlockMemory(reg_dx);
		reg_ax = (reg_bl==0);
		break;
	case XMS_GET_EMB_HANDLE_INFORMATION:						/* 0e */
		reg_bl = XMS_GetHandleInformation(reg_dx,reg_bh,reg_bl,reg_dx);
		reg_ax = (reg_bl==0);
		break;
	case XMS_RESIZE_EXTENDED_MEMORY_BLOCK:						/* 0f */
		reg_bl = XMS_ResizeMemory(reg_dx, reg_bx);
		reg_ax = (reg_bl==0);
		break;
	case XMS_ALLOCATE_UMB:										/* 10 */
		reg_ax=0;
		reg_bl=0xb1;	//No UMB Available
		reg_dx=0;
		break;
	case XMS_ALLOCATE_HIGH_MEMORY:								/* 01 */
	case XMS_FREE_HIGH_MEMORY:									/* 02 */
	case XMS_DEALLOCATE_UMB:									/* 11 */
		LOG(LOG_ERROR|LOG_MISC,"XMS:Unhandled call %2X",reg_ah);
		break;

	}
	return CBRET_NONE;
}

void XMS_Init(Section* sec) {
	Section_prop * section=static_cast<Section_prop *>(sec);
	Bitu size=section->Get_int("xmssize");
	if (!size) return;
	if (size>C_MEM_MAX_SIZE-1) size=C_MEM_MAX_SIZE-1;
	DOS_AddMultiplexHandler(multiplex_xms);
	call_xms=CALLBACK_Allocate();
	CALLBACK_Setup(call_xms,&XMS_Handler,CB_RETF);
	xms_callback=CALLBACK_RealPointer(call_xms);
	/* Setup the handler table */
	Bitu i;
	for (i=0;i<XMS_HANDLES;i++) {
		xms_handles[i].active=false;
		xms_handles[i].allocated=0;
		xms_handles[i].next=0xffff;
		xms_handles[i].prev=0xffff;
		xms_handles[i].size=0;
		xms_handles[i].locked=0;
	}
	/* Disable the 0 handle */
	xms_handles[0].active	=true;
	xms_handles[0].allocated=true;
	/* Setup the 1st handle */
	xms_handles[1].active=true;
	xms_handles[1].phys=1088*1024;		/* right behind the hma area */
	xms_handles[1].size=size*1024-64;
}

