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

#include <stdlib.h>
#include <string.h>
#include "dosbox.h"
#include "callback.h"
#include "mem.h"
#include "regs.h"
#include "dos_system.h"
#include "setup.h"


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
	Bit16u prev,next;
	Bit16u size;					/* Size in kb's */
	PhysPt phys;
	Bit8u locked;
	void * data;
	bool active;
};

static Bit16u call_xms;
static RealPt xms_callback;

static XMS_Block xms_handles[XMS_HANDLES];



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

Bitu XMS_Handler(void) {
	switch (reg_ah) {
	case XMS_GET_VERSION:										/* 00 */
		reg_ax=XMS_VERSION;
		reg_bx=XMS_DRIVER_VERSION;
		reg_dx=0;	/* No we don't have HMA */
		break;
	case XMS_ALLOCATE_HIGH_MEMORY:								/* 01 */
	case XMS_FREE_HIGH_MEMORY:									/* 02 */
	case XMS_GLOBAL_ENABLE_A20:									/* 03 */
	case XMS_GLOBAL_DISABLE_A20:								/* 04 */
	case XMS_LOCAL_ENABLE_A20:									/* 05 */
	case XMS_LOCAL_DISABLE_A20:									/* 06 */
	case XMS_QUERY_A20:											/* 07 */
		LOG_WARN("XMS:Unhandled call %2X",reg_ah);
		break;	
	case XMS_QUERY_FREE_EXTENDED_MEMORY:						/* 08 */
		/* Scan the tree for free memory and find largest free block */
		{ 
			Bit16u index=1;reg_ax=0;reg_dx=0;
			while (xms_handles[index].active) {
				if (!xms_handles[index].data) {
					if(xms_handles[index].size>reg_ax) reg_ax=xms_handles[index].size;
					reg_dx+=xms_handles[index].size;
				}
				if (xms_handles[index].next<XMS_HANDLES) index=xms_handles[index].next;
				else break;
			}
			if (!reg_dx) reg_bl=XMS_OUT_OF_SPACE;
			reg_bl=0;
			break;
		}
	case XMS_ALLOCATE_EXTENDED_MEMORY:							/* 09 */
		{
			Bit16u index=1;
			/* First make reg_dx a multiple of PAGE_KB */
			if (reg_dx & (PAGE_KB-1)) reg_dx=(reg_dx&(~(PAGE_KB-1)))+PAGE_KB;
			while (xms_handles[index].active) {
				/* Find a free block, check if there's enough size */
				if (!xms_handles[index].data && xms_handles[index].size>=reg_dx) {
					/* Check if block is bigger than request */
					if (xms_handles[index].size>reg_dx) {
						/* Split Block, find new handle and split up memory */
						Bit16u new_index=1;
						while (new_index<XMS_HANDLES) {
							if (!xms_handles[new_index].active) goto foundnew;
							new_index++;
						}
						reg_ax=0;
						reg_bl=XMS_OUT_OF_HANDLES;
						reg_dx=0;
						return CBRET_NONE;
foundnew:
						xms_handles[new_index].next=xms_handles[index].next;
						xms_handles[new_index].prev=index;
						xms_handles[index].next=new_index;
						xms_handles[index].locked=0;
						xms_handles[new_index].active=true;
						xms_handles[new_index].data=0;
						xms_handles[new_index].locked=0;
						xms_handles[new_index].size=xms_handles[index].size-reg_dx;
						xms_handles[new_index].phys=xms_handles[index].phys+reg_dx*1024;
						xms_handles[index].size=reg_dx;
					}
					/* Use the data from handle index to allocate the actual memory */
					reg_dx=index;
					reg_ax=1;reg_bl=0;
					xms_handles[index].data=malloc(xms_handles[index].size*1024);
					if (!xms_handles[index].data) E_Exit("XMS:Out of memory???");
					/* Now Setup the memory mapping for this range */
					MEM_SetupMapping(PAGE_COUNT(xms_handles[index].phys),PAGE_COUNT(xms_handles[index].size*1024),xms_handles[index].data);
					return CBRET_NONE;
				}	
				/* Not a free block or too small advance to next one if possible */
				if (xms_handles[index].next < XMS_HANDLES ) index=xms_handles[index].next;
				else break;
			}
			/* Found no good blocks give some errors */
			reg_ax=0;
			reg_bl=XMS_OUT_OF_SPACE;
			break;
		}
	case XMS_FREE_EXTENDED_MEMORY:								/* 0a */
		{
			/* Check for a valid handle */
			if (!reg_dx || (reg_dx>=XMS_HANDLES) || !xms_handles[reg_dx].active || !xms_handles[reg_dx].data ) {
				reg_ax=0;
				reg_bl=XMS_INVALID_HANDLE;
				return CBRET_NONE;
			}
			/* Remove the mapping to the memory */
			MEM_ClearMapping(PAGE_COUNT(xms_handles[reg_dx].phys),PAGE_COUNT(xms_handles[reg_dx].size*1024));
			/* Free the memory in the block and merge the blocks previous and next block */
			Bit16u prev=xms_handles[reg_dx].prev;
			Bit16u next=xms_handles[reg_dx].next;
			free(xms_handles[reg_dx].data);
			xms_handles[reg_dx].data=0;
			if ((next<XMS_HANDLES) && !xms_handles[next].data) {
				xms_handles[next].active=false;
				xms_handles[reg_dx].size+=xms_handles[next].size;
				xms_handles[reg_dx].next=xms_handles[next].next;
				next=xms_handles[reg_dx].next;
				if (next<XMS_HANDLES) xms_handles[next].prev=reg_dx;
			}
			if ((prev<XMS_HANDLES) && !xms_handles[prev].data) {
				xms_handles[reg_dx].active=false;
				xms_handles[prev].size+=xms_handles[reg_dx].size;
				next=xms_handles[reg_dx].next;
				xms_handles[prev].next=next;
				if (next<XMS_HANDLES) xms_handles[next].prev=prev;
			}
			reg_ax=1;reg_bl=0;
		}
		break;
	case XMS_MOVE_EXTENDED_MEMORY_BLOCK:						/* 0b */
		{
			PhysPt bpt=SegPhys(ds)+reg_si;
			XMS_MemMove block;
			/* Fill the block with mem_read's and shit */
			block.length=mem_readd(bpt+offsetof(XMS_MemMove,length));
			block.src_handle=mem_readw(bpt+offsetof(XMS_MemMove,src_handle));
			block.src.offset=mem_readd(bpt+offsetof(XMS_MemMove,src.offset));
			block.dest_handle=mem_readw(bpt+offsetof(XMS_MemMove,dest_handle));
			block.dest.offset=mem_readd(bpt+offsetof(XMS_MemMove,dest.offset));
			PhysPt src,dest;
			if (block.src_handle) {
				if ((block.src_handle>=XMS_HANDLES) || !xms_handles[block.src_handle].active ||!xms_handles[block.src_handle].data) {
					reg_ax=0;
					reg_bl=0xa3;	/* Src Handle invalid */
					return CBRET_NONE;
				}
				if (block.src.offset>=(xms_handles[block.src_handle].size*1024U)) {
					reg_ax=0;
					reg_bl=0xa4;	/* Src Offset invalid */
					return CBRET_NONE;
				}
				if (block.length>xms_handles[block.src_handle].size*1024U-block.src.offset) {
					reg_ax=0;
					reg_bl=0xa7;	/* Length invalid */
					return CBRET_NONE;

				}
				src=xms_handles[block.src_handle].phys+block.src.offset;
			} else {
				src=Real2Phys(block.src.realpt);
			}
			if (block.dest_handle) {
				if ((block.dest_handle>=XMS_HANDLES) || !xms_handles[block.dest_handle].active ||!xms_handles[block.dest_handle].data) {
					reg_ax=0;
					reg_bl=0xa3;	/* Dest Handle invalid */
					return CBRET_NONE;
				}
				if (block.dest.offset>=(xms_handles[block.dest_handle].size*1024U)) {
					reg_ax=0;
					reg_bl=0xa4;	/* Dest Offset invalid */
					return CBRET_NONE;
				}
				if (block.length>xms_handles[block.dest_handle].size*1024U-block.dest.offset) {
					reg_ax=0;
					reg_bl=0xa7;	/* Length invalid */
					return CBRET_NONE;
				}
				dest=xms_handles[block.dest_handle].phys+block.dest.offset;
			} else {
				dest=Real2Phys(block.dest.realpt);
			}
			MEM_BlockCopy(dest,src,block.length);
			reg_ax=1;reg_bl=0;
		}
		break;
	case XMS_LOCK_EXTENDED_MEMORY_BLOCK:						/* 0c */
		{
			/* Check for a valid handle */
			if (!reg_dx || (reg_dx>=XMS_HANDLES) || !xms_handles[reg_dx].active || !xms_handles[reg_dx].data ) {
				reg_ax=0;
				reg_bl=XMS_INVALID_HANDLE;
				return CBRET_NONE;
			}
			if (xms_handles[reg_dx].locked<255) xms_handles[reg_dx].locked++;
			reg_bx=(Bit16u)(xms_handles[reg_dx].phys & 0xFFFF);
			reg_dx=(Bit16u)(xms_handles[reg_dx].phys>>16);
			reg_ax=1;
			break;
		}
	case XMS_UNLOCK_EXTENDED_MEMORY_BLOCK:						/* 0d */
		/* Check for a valid handle */
		if (!reg_dx || (reg_dx>=XMS_HANDLES) || !xms_handles[reg_dx].active || !xms_handles[reg_dx].data ) {
			reg_ax=0;
			reg_bl=XMS_INVALID_HANDLE;
			return CBRET_NONE;
		}
		if (xms_handles[reg_dx].locked) {
			xms_handles[reg_dx].locked--;
			reg_ax=1;reg_bl=0;
		} else {
			reg_ax=0;
			reg_bl=XMS_BLOCK_NOT_LOCKED;
		}
		break;
	case XMS_GET_EMB_HANDLE_INFORMATION:						/* 0e */
		/* Check for a valid handle */
		if (!reg_dx || (reg_dx>=XMS_HANDLES) || !xms_handles[reg_dx].active || !xms_handles[reg_dx].data ) {
			reg_ax=0;
			reg_bl=XMS_INVALID_HANDLE;
			return CBRET_NONE;
		}
		reg_bh=xms_handles[reg_dx].locked;
		/* Find available blocks */
		reg_bx=0;{ for (Bitu i=1;i<XMS_HANDLES;i++) if (!xms_handles[i].data) reg_bx++;}
		reg_dx=xms_handles[reg_dx].size;
		break;
	case XMS_RESIZE_EXTENDED_MEMORY_BLOCK:						/* 0f */
		LOG_WARN("XMS:Unhandled call %2X",reg_ah);
		break;
	case XMS_ALLOCATE_UMB:										/* 10 */
		reg_ax=0;
		reg_bl=0xb1;	//No UMB Available
		reg_dx=0;
		break;
	case XMS_DEALLOCATE_UMB:									/* 11 */
	default:
		LOG_WARN("XMS:Unhandled call %2X",reg_ah);break;

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
		xms_handles[i].data=0;
		xms_handles[i].next=0xffff;
		xms_handles[i].prev=0xffff;
		xms_handles[i].size=0;
		xms_handles[i].locked=0;
	}
	/* Disable the 0 handle */
	xms_handles[0].active=true;
	xms_handles[0].data=(void *)0xFFFFFFFF;
	/* Setup the 1st handle */
	xms_handles[1].active=true;
	xms_handles[1].phys=1088*1024;		/* right behind the hma area */
	xms_handles[1].size=size*1024-64;
}

