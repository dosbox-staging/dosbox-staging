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

#include <string.h>
#include "dosbox.h"
#include "callback.h"
#include "mem.h"
#include "regs.h"
#include "dos_system.h"


#define XMS_VERSION    		0x0300	/* version 3.00 */
#define XMS_DRIVER_VERSION	0x0301	/* my driver version 3.01 */

#define	 XMS_GET_VERSION		0x00
#define	 XMS_ALLOCATE_HIGH_MEMORY	0x01
#define	 XMS_FREE_HIGH_MEMORY		0x02
#define	 XMS_GLOBAL_ENABLE_A20		0x03
#define	 XMS_GLOBAL_DISABLE_A20		0x04
#define	 XMS_LOCAL_ENABLE_A20		0x05
#define	 XMS_LOCAL_DISABLE_A20		0x06
#define	 XMS_QUERY_A20			0x07
#define	 XMS_QUERY_FREE_EXTENDED_MEMORY	0x08
#define	 XMS_ALLOCATE_EXTENDED_MEMORY	0x09
#define	 XMS_FREE_EXTENDED_MEMORY	0x0a
#define	 XMS_MOVE_EXTENDED_MEMORY_BLOCK	0x0b
#define	 XMS_LOCK_EXTENDED_MEMORY_BLOCK	0x0c
#define	 XMS_UNLOCK_EXTENDED_MEMORY_BLOCK 0x0d
#define	 XMS_GET_EMB_HANDLE_INFORMATION	0x0e
#define	 XMS_RESIZE_EXTENDED_MEMORY_BLOCK 0x0f
#define	 XMS_ALLOCATE_UMB		0x10
#define	 XMS_DEALLOCATE_UMB		0x11

#define	HIGH_MEMORY_IN_USE		0x92
#define	HIGH_MEMORY_NOT_ALLOCATED	0x93
#define XMS_OUT_OF_SPACE		0xa0
#define XMS_INVALID_HANDLE		0xa2


static Bit16u call_xms;
static RealPt xms_callback;


static bool multiplex_xms(void) {
	switch (reg_ax) {
	case 0x4300:					/* XMS installed check */
			reg_al=0x80;
			return true;
	case 0x4310:					/* XMS handler seg:offset */
			SetSegment_16(es,RealSeg(xms_callback));
			reg_bx=RealOff(xms_callback);
			return true;			
	}
	return false;

};

#if defined (_MSC_VER)
#pragma pack(1)
#endif
struct XMS_MemMove{
	Bit32u length;
	Bit16u src_handle;
	RealPt src_offset;
	Bit16u dest_handle;
	RealPt dest_offset;
}
#if defined (_MSC_VER)
;
#pragma pack()
#else
__attribute__ ((packed));
#endif

static void XMS_MoveBlock(PhysPt block,Bit8u * result) {
	XMS_MemMove moveblock;
//TODO Will not work on other endian, probably base it on a class would be nice
	MEM_BlockRead(block,(Bit8u *)&moveblock,sizeof(XMS_MemMove));
	HostPt src;
	PhysPt dest;
	if (moveblock.src_handle) {
		src=memory+EMM_Handles[moveblock.src_handle].phys_base+moveblock.src_offset;
	} else {
		src=Real2Host(moveblock.src_offset);
	}
	if (moveblock.dest_handle) {
		dest=EMM_Handles[moveblock.dest_handle].phys_base+moveblock.dest_offset;
	} else {
		dest=Real2Phys(moveblock.dest_offset);
	}
	//memcpy((void *)dest,(void *)src,moveblock.length);
	MEM_BlockWrite(dest,src,moveblock.length);
	*result=0;
};


Bitu XMS_Handler(void) {
	switch (reg_ah) {
	case XMS_GET_VERSION:										/* 00 */
		reg_ax=XMS_VERSION;
		reg_bx=XMS_DRIVER_VERSION;
		reg_dx=0;						//TODO HMA Maybe
		break;
	case XMS_ALLOCATE_HIGH_MEMORY:								/* 01 */
	case XMS_FREE_HIGH_MEMORY:									/* 02 */
	case XMS_GLOBAL_ENABLE_A20:									/* 03 */
	case XMS_GLOBAL_DISABLE_A20:								/* 04 */
	case XMS_LOCAL_ENABLE_A20:									/* 05 */
	case XMS_LOCAL_DISABLE_A20:									/* 06 */
	case XMS_QUERY_A20:											/* 07 */
		LOG_WARN("XMS:Unhandled call %2X",reg_ah);break;	
	case XMS_QUERY_FREE_EXTENDED_MEMORY:						/* 08 */
		EMM_GetFree(&reg_ax,&reg_dx);
		reg_ax<<=2;reg_dx<<=2;
		reg_bl=0;
		break;
	case XMS_ALLOCATE_EXTENDED_MEMORY:							/* 09 */
		EMM_Allocate(PAGES(reg_dx*1024),&reg_dx);
		if (reg_dx) reg_ax=1;
		else { reg_ax=0;reg_bl=0xb0; }
		break;
	case XMS_FREE_EXTENDED_MEMORY:								/* 0a */
		EMM_Free(reg_dx);
		reg_ax=1;
		break;

	case XMS_MOVE_EXTENDED_MEMORY_BLOCK:						/* 0b */
		XMS_MoveBlock(real_phys(Segs[ds].value,reg_si),&reg_bl);
		if (reg_bl) reg_ax=0;
		else reg_ax=1;
		break;
	case XMS_LOCK_EXTENDED_MEMORY_BLOCK:						/* 0c */
		if ((!EMM_Handles[reg_dx].active) || (EMM_Handles[reg_dx].free)) {
			reg_ax=0;
			reg_bl=0xa2;			/* Invalid block */
			break;
		}
		reg_ax=1;
		reg_bx=(Bit16u)((EMM_Handles[reg_dx].phys_base) & 0xffff);
		reg_dx=(Bit16u)((EMM_Handles[reg_dx].phys_base >> 16) & 0xffff);
		break;
	case XMS_UNLOCK_EXTENDED_MEMORY_BLOCK:						/* 0d */
		reg_ax=1;
		break;
	case XMS_GET_EMB_HANDLE_INFORMATION:						/* 0e */
		LOG_WARN("XMS:Unhandled call %2X",reg_ah);break;
	case XMS_RESIZE_EXTENDED_MEMORY_BLOCK:						/* 0f */
		LOG_WARN("XMS:Unhandled call %2X",reg_ah);break;
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



void XMS_Init(void) {
	DOS_AddMultiplexHandler(multiplex_xms);
	call_xms=CALLBACK_Allocate();
	CALLBACK_Setup(call_xms,&XMS_Handler,CB_RETF);
	xms_callback=CALLBACK_RealPointer(call_xms);
}

