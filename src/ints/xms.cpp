/*
 *  Copyright (C) 2002-2004  The DOSBox Team
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

/* $Id: xms.cpp,v 1.32 2004-03-09 20:13:44 qbix79 Exp $ */

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
#include "bios.h"

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
#define XMS_QUERY_ANY_FREE_MEMORY			0x88
#define XMS_ALLOCATE_ANY_MEMORY				0x89

#define	HIGH_MEMORY_NOT_EXIST				0x90
#define	HIGH_MEMORY_IN_USE					0x91
#define	HIGH_MEMORY_NOT_ALLOCATED			0x93
#define XMS_OUT_OF_SPACE					0xa0
#define XMS_OUT_OF_HANDLES					0xa1
#define XMS_INVALID_HANDLE					0xa2
#define XMS_BLOCK_NOT_LOCKED				0xaa
#define XMS_BLOCK_LOCKED					0xab

struct XMS_Block {
	Bitu	size;
	MemHandle mem;
	Bit8u	locked;
	bool	free;
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

static INLINE bool InvalidHandle(Bitu handle) {
	return (!handle || (handle>=XMS_HANDLES) || xms_handles[handle].free);
}

Bitu XMS_QueryFreeMemory(Bit16u& largestFree, Bit16u& totalFree) {
	/* Scan the tree for free memory and find largest free block */
	totalFree=(Bit16u)(MEM_FreeTotal()*4);
	largestFree=(Bit16u)(MEM_FreeLargest()*4);
	if (!totalFree) return XMS_OUT_OF_SPACE;
	return 0;
};

Bitu XMS_AllocateMemory(Bitu size, Bit16u& handle)
// size = kb
{
	/* Find free handle */
	Bit16u index=1;
	while (!xms_handles[index].free) {
		if (++index>=XMS_HANDLES) return XMS_OUT_OF_HANDLES;
	}
	Bitu pages=(size/4) + ((size & 3) ? 1 : 0);
	MemHandle mem=MEM_AllocatePages(pages,true);
	if ((!mem) && (size != 0)) return XMS_OUT_OF_SPACE;
	xms_handles[index].free=false;
	xms_handles[index].mem=mem;
	xms_handles[index].locked=0;
	xms_handles[index].size=size;
	handle=index;
	return 0;
};

Bitu XMS_FreeMemory(Bitu handle)
{
	if (InvalidHandle(handle)) return XMS_INVALID_HANDLE;
	MEM_ReleasePages(xms_handles[handle].mem);
	xms_handles[handle].mem=-1;
	xms_handles[handle].size=0;
	xms_handles[handle].free=true;
	return 0;
};

Bitu XMS_MoveMemory(PhysPt bpt)
{
	/* Read the block with mem_read's */
	Bitu length=mem_readd(bpt+offsetof(XMS_MemMove,length));
	Bitu src_handle=mem_readw(bpt+offsetof(XMS_MemMove,src_handle));
	union {
		RealPt realpt;
		Bit32u offset;
	} src,dest;
	src.offset=mem_readd(bpt+offsetof(XMS_MemMove,src.offset));
	Bitu dest_handle=mem_readw(bpt+offsetof(XMS_MemMove,dest_handle));
	dest.offset=mem_readd(bpt+offsetof(XMS_MemMove,dest.offset));
	PhysPt srcpt,destpt;
	if (src_handle) {
		if (InvalidHandle(src_handle)) {
			return 0xa3;	/* Src Handle invalid */
		}
		if (src.offset>=(xms_handles[src_handle].size*1024U)) {
			return 0xa4;	/* Src Offset invalid */
		}
		if (length>xms_handles[src_handle].size*1024U-src.offset) {
			return 0xa7;	/* Length invalid */
		}
		srcpt=(xms_handles[src_handle].mem*4096)+src.offset;
	} else {
		srcpt=Real2Phys(src.realpt);
	}
	if (dest_handle) {
		if (InvalidHandle(dest_handle)) {
			return 0xa3;	/* Dest Handle invalid */
		}
		if (dest.offset>=(xms_handles[dest_handle].size*1024U)) {
			return 0xa4;	/* Dest Offset invalid */
		}
		if (length>xms_handles[dest_handle].size*1024U-dest.offset) {
			return 0xa7;	/* Length invalid */
		}
		destpt=(xms_handles[dest_handle].mem*4096)+dest.offset;
	} else {
		destpt=Real2Phys(dest.realpt);
	}
//	LOG_MSG("XMS move src %X dest %X length %X",srcpt,destpt,length);
	mem_memcpy(destpt,srcpt,length);
	return 0;
}

Bitu XMS_LockMemory(Bitu handle, Bit32u& address)
{
	if (InvalidHandle(handle)) return XMS_INVALID_HANDLE;
	if (xms_handles[handle].locked<255) xms_handles[handle].locked++;
	address = xms_handles[handle].mem*4096;
	return 0;
};

Bitu XMS_UnlockMemory(Bitu handle)
{
 	if (InvalidHandle(handle)) return XMS_INVALID_HANDLE;
	if (xms_handles[handle].locked) {
		xms_handles[handle].locked--;
		return 0;
	}
	return XMS_BLOCK_NOT_LOCKED;
};

Bitu XMS_GetHandleInformation(Bitu handle, Bit8u& lockCount, Bit8u& numFree, Bit16u& size)
{
	if (InvalidHandle(handle)) return XMS_INVALID_HANDLE;
	lockCount = xms_handles[handle].locked;
	/* Find available blocks */
	numFree=0;
	for (Bitu i=1;i<XMS_HANDLES;i++) {
		if (xms_handles[i].free) numFree++;
	}
	size=xms_handles[handle].size;
	return 0;
};

Bitu XMS_ResizeMemory(Bitu handle, Bitu newSize)
{
	if (InvalidHandle(handle)) return XMS_INVALID_HANDLE;	
	// Block has to be unlocked
	if (xms_handles[handle].locked>0) return XMS_BLOCK_LOCKED;
	Bitu pages=newSize/4 + ((newSize & 3) ? 1 : 0);
	if (MEM_ReAllocatePages(xms_handles[handle].mem,pages,true)) {
		return 0;
	} else return XMS_OUT_OF_SPACE;
}

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
//	LOG(LOG_MISC,LOG_ERROR)("XMS: CALL %02X",reg_ah);
	switch (reg_ah) {

	case XMS_GET_VERSION:										/* 00 */
		reg_ax=XMS_VERSION;
		reg_bx=XMS_DRIVER_VERSION;
		reg_dx=0;	/* No we don't have HMA */
		break;
	case XMS_ALLOCATE_HIGH_MEMORY:								/* 01 */
		reg_ax=0;
		reg_bl=HIGH_MEMORY_NOT_EXIST;
		break;
	case XMS_FREE_HIGH_MEMORY:									/* 02 */
		reg_ax=0;
		reg_bl=HIGH_MEMORY_NOT_EXIST;
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
	case XMS_ALLOCATE_ANY_MEMORY:								/* 89 */
		reg_edx &= 0xffff;
		// fall through
	case XMS_ALLOCATE_EXTENDED_MEMORY:							/* 09 */
		{
		Bit16u handle = 0;
		reg_bl = XMS_AllocateMemory(reg_dx,handle);
		reg_dx = handle;
		reg_ax = (reg_bl==0);		// set ax to success/failure
		}; break;
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
		reg_ax = (reg_bl==0);
		if (reg_bl==0) { // success
			reg_bx=(Bit16u)(address & 0xFFFF);
			reg_dx=(Bit16u)(address >> 16);
		};
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
	case XMS_DEALLOCATE_UMB:									/* 11 */
		LOG(LOG_MISC,LOG_ERROR)("XMS:Unhandled call %2X",reg_ah);
		break;
	case XMS_QUERY_ANY_FREE_MEMORY:								/* 88 */
		reg_bl = XMS_QueryFreeMemory(reg_ax,reg_dx);
        reg_eax &= 0xffff;
        reg_edx &= 0xffff;
        reg_ecx = (MEM_TotalPages()*MEM_PAGESIZE)-1;			// highest known physical memory address
		break;
	}
//	LOG(LOG_MISC,LOG_ERROR)("XMS: CALL Result: %02X",reg_bl);
	return CBRET_NONE;
}

void XMS_Init(Section* sec) {
	
	Section_prop * section=static_cast<Section_prop *>(sec);
	if (!section->Get_bool("xms")) return;
	Bitu i;
	BIOS_ZeroExtendedSize();
	DOS_AddMultiplexHandler(multiplex_xms);
	call_xms=CALLBACK_Allocate();
	CALLBACK_Setup(call_xms,&XMS_Handler,CB_RETF, "XMS Handler");
	xms_callback=CALLBACK_RealPointer(call_xms);
   
	/* Overide the callback with one that can be hooked */
	phys_writeb(CB_BASE+(call_xms<<4)+0,(Bit8u)0xeb);       //jump near
	phys_writeb(CB_BASE+(call_xms<<4)+1,(Bit8u)0x03);       //offset
	phys_writeb(CB_BASE+(call_xms<<4)+2,(Bit8u)0x90);       //NOP
	phys_writeb(CB_BASE+(call_xms<<4)+3,(Bit8u)0x90);       //NOP
	phys_writeb(CB_BASE+(call_xms<<4)+4,(Bit8u)0x90);       //NOP
	phys_writeb(CB_BASE+(call_xms<<4)+5,(Bit8u)0xFE);       //GRP 4
	phys_writeb(CB_BASE+(call_xms<<4)+6,(Bit8u)0x38);       //Extra Callback instruction
	phys_writew(CB_BASE+(call_xms<<4)+7,call_xms);		//The immediate word          
	phys_writeb(CB_BASE+(call_xms<<4)+9,(Bit8u)0xCB);       //A RETF Instruction
   
	for (i=0;i<XMS_HANDLES;i++) {
		xms_handles[i].free=true;
		xms_handles[i].mem=-1;
		xms_handles[i].size=0;
		xms_handles[i].locked=0;
	}
	/* Disable the 0 handle */
	xms_handles[0].free	= false;
}

