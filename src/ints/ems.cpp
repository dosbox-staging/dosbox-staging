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

/* $Id: ems.cpp,v 1.32 2004-03-31 22:01:22 harekiet Exp $ */

#include <string.h>
#include <stdlib.h>
#include "dosbox.h"
#include "callback.h"
#include "mem.h"
#include "paging.h"
#include "bios.h"
#include "keyboard.h"
#include "regs.h"
#include "inout.h"
#include "dos_inc.h"
#include "setup.h"

#define EMM_PAGEFRAME	0xE000
#define EMM_PAGEFRAME4K	((EMM_PAGEFRAME*16)/4096)
#define	EMM_MAX_HANDLES	100				/* 255 Max */
#define EMM_PAGE_SIZE	(16*1024U)
#define EMM_MAX_PAGES	(32 * 1024 / 16 )
#define EMM_MAX_PHYS	4				/* 4 16kb pages in pageframe */

#define EMM_VERSION		0x40

#define NULL_HANDLE	0xffff
#define	NULL_PAGE	0xffff

/* EMM errors */
#define EMM_NO_ERROR			0x00
#define EMM_SOFT_MAL			0x80
#define EMM_HARD_MAL			0x81
#define EMM_INVALID_HANDLE		0x83
#define EMM_FUNC_NOSUP			0x84
#define EMM_OUT_OF_HANDLES		0x85
#define EMM_OUT_OF_PHYS			0x87
#define EMM_OUT_OF_LOG			0x88
#define EMM_ZERO_PAGES			0x89
#define EMM_LOG_OUT_RANGE		0x8a
#define EMM_ILL_PHYS			0x8b
#define EMM_PAGE_MAP_SAVED		0x8d
#define EMM_INVALID_SUB			0x8f
#define EMM_FEAT_NOSUP			0x91
#define EMM_MOVE_OVLAP			0x92
#define EMM_MOVE_OVLAPI			0x97
#define EMM_NOT_FOUND			0xa0


class device_EMM : public DOS_Device {
public:
	device_EMM(){name="EMMXXXX0";}
	bool Read(Bit8u * data,Bit16u * size) { return false;}
	bool Write(Bit8u * data,Bit16u * size){ 
		LOG(LOG_IOCTL,LOG_NORMAL)("EMS:Write to device");	
		return false;
	}
	bool Seek(Bit32u * pos,Bit32u type){return false;}
	bool Close(){return false;}
	Bit16u GetInformation(void){return 0x8093;}
private:
	Bit8u cache;
};

struct EMM_Mapping {
	Bit16u handle;
	Bit16u page;
};

struct EMM_Handle {
	Bit16u pages;
	MemHandle mem;
	char name[8];
	bool saved_page_map;
	EMM_Mapping page_map[EMM_MAX_PHYS];
};

static EMM_Handle emm_handles[EMM_MAX_HANDLES];
static EMM_Mapping emm_mappings[EMM_MAX_PHYS];
static Bitu call_int67,call_vdma;

struct MoveRegion {
	Bit32u bytes;
	Bit8u src_type;
	Bit16u src_handle;
	Bit16u src_offset;
	Bit16u src_page_seg;
	Bit8u dest_type;
	Bit16u dest_handle;
	Bit16u dest_offset;
	Bit16u dest_page_seg;
};

static Bit16u EMM_GetFreePages(void) {
	Bitu count=MEM_FreeTotal()/4;
	if (count>0x7fff) count=0x7fff;
	return (Bit16u)count;
}

static bool INLINE ValidHandle(Bit16u handle) {
	if (handle>=EMM_MAX_HANDLES) return false;
	if (emm_handles[handle].pages==NULL_HANDLE) return false;
	return true;
}

static Bit8u EMM_AllocateMemory(Bit16u pages,Bit16u & handle) {
	/* Check for 0 page allocation */
	if (!pages) return EMM_ZERO_PAGES;
	/* Check for enough free pages */
	if ((MEM_FreeTotal()/4)<pages) { handle=NULL_HANDLE; return EMM_OUT_OF_LOG;}
	handle=1;
	/* Check for a free handle */
	while (emm_handles[handle].pages!=NULL_HANDLE) {
		if (++handle>=EMM_MAX_HANDLES) {handle=NULL_HANDLE;return EMM_OUT_OF_HANDLES;}
	}
	MemHandle mem=MEM_AllocatePages(pages*4,false);
	if (!mem) E_Exit("EMS:Memory allocation failure");
	emm_handles[handle].pages=pages;
	emm_handles[handle].mem=mem;
	return EMM_NO_ERROR;
}

static Bit8u EMM_ReallocatePages(Bit16u handle,Bit16u & pages) {
	/* Check for valid handle */
	if (!ValidHandle(handle)) return EMM_INVALID_HANDLE;
	/* Check for enough pages */
	if (!MEM_ReAllocatePages(emm_handles[handle].mem,pages*4,false)) return EMM_OUT_OF_LOG;
	/* Update size */
	emm_handles[handle].pages=pages;
	return EMM_NO_ERROR;
}

static Bit8u EMM_MapPage(Bitu phys_page,Bit16u handle,Bit16u log_page) {
//	LOG_MSG("EMS MapPage handle %d phys %d log %d",handle,phys_page,log_page);
	/* Check for too high physical page */
	if (phys_page>=EMM_MAX_PHYS) return EMM_ILL_PHYS;
	/* Check for valid handle */
	if (!ValidHandle(handle)) return EMM_INVALID_HANDLE;
	/* Check to do unmapping or mappning */
	if (log_page<emm_handles[handle].pages) {
		/* Mapping it is */
		emm_mappings[phys_page].handle=handle;
		emm_mappings[phys_page].page=log_page;
		
		MemHandle memh=MEM_NextHandleAt(emm_handles[handle].mem,log_page*4);;
		for (Bitu i=0;i<4;i++) {
			PAGING_MapPage(EMM_PAGEFRAME4K+phys_page*4+i,memh);
			memh=MEM_NextHandle(memh);
		}
		PAGING_ClearTLB();
		return EMM_NO_ERROR;
	} else if (log_page==NULL_PAGE) {
		/* Unmapping it is */
		emm_mappings[phys_page].handle=NULL_HANDLE;
		emm_mappings[phys_page].page=NULL_PAGE;
		for (Bitu i=0;i<4;i++) 
			PAGING_MapPage(EMM_PAGEFRAME4K+phys_page*4+i,EMM_PAGEFRAME4K+phys_page*4+i);
		PAGING_ClearTLB();
		return EMM_NO_ERROR;
	} else {
		/* Illegal logical page it is */
		return EMM_LOG_OUT_RANGE;
	}
}

static Bit8u EMM_ReleaseMemory(Bit16u handle) {
	/* Check for valid handle */
	if (!ValidHandle(handle)) return EMM_INVALID_HANDLE;
	MEM_ReleasePages(emm_handles[handle].mem);
	/* Reset handle */
	emm_handles[handle].mem=0;
	emm_handles[handle].pages=NULL_HANDLE;
	emm_handles[handle].saved_page_map=false;
	memset(&emm_handles[handle].name,0,8);
	return EMM_NO_ERROR;
}

static Bit8u EMM_SavePageMap(Bit16u handle) {
	/* Check for valid handle */
	if (handle>=EMM_MAX_HANDLES || emm_handles[handle].pages==NULL_HANDLE) return EMM_INVALID_HANDLE;
	/* Check for previous save */
	if (emm_handles[handle].saved_page_map) return EMM_PAGE_MAP_SAVED;
	/* Copy the mappings over */
	for (Bitu i=0;i<EMM_MAX_PHYS;i++) {
		emm_handles[handle].page_map[i].page=emm_mappings[i].page;
		emm_handles[handle].page_map[i].handle=emm_mappings[i].handle;
	}
	emm_handles[handle].saved_page_map=true;
	return EMM_NO_ERROR;
}

static Bit8u EMM_RestoreMappingTable(void) {
	Bit8u result;
	/* Move through the mappings table and setup mapping accordingly */
	for (Bitu i=0;i<EMM_MAX_PHYS;i++) {
		result=EMM_MapPage(i,emm_mappings[i].handle,emm_mappings[i].page);
	}
	return EMM_NO_ERROR;
}
static Bit8u EMM_RestorePageMap(Bit16u handle) {
	/* Check for valid handle */
	if (handle>=EMM_MAX_HANDLES || emm_handles[handle].pages==NULL_HANDLE) return EMM_INVALID_HANDLE;
	/* Check for previous save */
	if (!emm_handles[handle].saved_page_map) return EMM_INVALID_HANDLE;
	/* Restore the mappings */
	emm_handles[handle].saved_page_map=false;
	for (Bitu i=0;i<EMM_MAX_PHYS;i++) {
		emm_mappings[i].page=emm_handles[handle].page_map[i].page;
		emm_mappings[i].handle=emm_handles[handle].page_map[i].handle;
	}
	return EMM_RestoreMappingTable();
}

static Bit8u EMM_GetPagesForAllHandles(PhysPt table,Bit16u & handles) {
	handles=0;
	for (Bit16u i=0;i<EMM_MAX_HANDLES;i++) {
		if (emm_handles[i].pages!=NULL_HANDLE) {
			handles++;
			mem_writew(table,i);
			mem_writew(table+2,emm_handles[i].pages);
			table+=4;
		}
	}
	return EMM_NO_ERROR;
}

static Bit8u EMM_PartialPageMapping(void) {
	PhysPt list,data;Bit16u count;
	switch (reg_al) {
	case 0x00:	/* Save Partial Page Map */
		list = SegPhys(ds)+reg_si;
		data = SegPhys(es)+reg_di;
		count=mem_readw(list);list+=2;
		mem_writew(data,count);data+=2;
		for (;count>0;count--) {
			Bit16u page=mem_readw(list);list+=2;
			if ((page<EMM_PAGEFRAME) || (page>=EMM_PAGEFRAME+0x1000)) return EMM_ILL_PHYS;
			page = (page-EMM_PAGEFRAME) / (EMM_PAGE_SIZE>>4);
			mem_writew(data,page);data+=2;
			MEM_BlockWrite(data,&emm_mappings[page],sizeof(EMM_Mapping));
			data+=sizeof(EMM_Mapping);
		}
		break;
	case 0x01:	/* Restore Partial Page Map */
		data = SegPhys(ds)+reg_si;
		count= mem_readw(data);data+=2;
		for (;count>0;count--) {
			Bit16u page=mem_readw(data);data+=2;
			if (page>=EMM_MAX_PHYS) return EMM_ILL_PHYS;
			MEM_BlockRead(data,&emm_mappings[page],sizeof(EMM_Mapping));
			data+=sizeof(EMM_Mapping);
		}
		return EMM_RestoreMappingTable();
		break;
	case 0x02:	/* Get Partial Page Map Array Size */
		reg_al=2+reg_bx*(2+sizeof(EMM_Mapping));
		break;
	default:
		LOG(LOG_MISC,LOG_ERROR)("EMS:Call %2X Subfunction %2X not supported",reg_ah,reg_al);
		return EMM_FUNC_NOSUP;
	}
	return EMM_NO_ERROR;
}

static Bit8u HandleNameSearch(void) {
	char name[9];
	Bit16u handle=0;PhysPt data;
	switch (reg_al) {
	case 0x00:	/* Get all handle names */
		reg_al=0;data=SegPhys(es)+reg_di;
		for (handle=0;handle<EMM_MAX_HANDLES;handle++) {
			if (emm_handles[handle].pages!=NULL_HANDLE) {
				reg_al++;
				mem_writew(data,handle);
				MEM_BlockWrite(data+2,emm_handles[handle].name,8);
				data+=10;
			}
		}
		break;
	case 0x01: /* Search for a handle name */
		MEM_StrCopy(SegPhys(ds)+reg_si,name,8);name[8]=0;
		for (handle=0;handle<EMM_MAX_HANDLES;handle++) {
			if (emm_handles[handle].pages!=NULL_HANDLE) {
				if (!strncmp(name,emm_handles[handle].name,8)) {
					reg_dx=handle;
					return EMM_NO_ERROR;
				}
			}
		}
		return EMM_NOT_FOUND;
		break;
	case 0x02: /* Get Total number of handles */
	  reg_bx=EMM_MAX_HANDLES;
	  break;
	default:
		LOG(LOG_MISC,LOG_ERROR)("EMS:Call %2X Subfunction %2X not supported",reg_ah,reg_al);
		return EMM_FUNC_NOSUP;
	}
	return EMM_NO_ERROR;
}

static Bit8u GetSetHandleName(void) {
	Bit16u handle=reg_dx;
	switch (reg_al) {
	case 0x00:	/* Get Handle Name */
		if (handle>=EMM_MAX_HANDLES || emm_handles[handle].pages==NULL_HANDLE) return EMM_INVALID_HANDLE;
		MEM_BlockWrite(SegPhys(es)+reg_di,emm_handles[handle].name,8);
		break;
	case 0x01:	/* Set Handle Name */
		if (handle>=EMM_MAX_HANDLES || emm_handles[handle].pages==NULL_HANDLE) return EMM_INVALID_HANDLE;
		MEM_BlockRead(SegPhys(es)+reg_di,emm_handles[handle].name,8);
		break;
	default:
		LOG(LOG_MISC,LOG_ERROR)("EMS:Call %2X Subfunction %2X not supported",reg_ah,reg_al);
		return EMM_FUNC_NOSUP;
	}
	return EMM_NO_ERROR;

}


static void LoadMoveRegion(PhysPt data,MoveRegion & region) {
	region.bytes=mem_readd(data+0x0);

	region.src_type=mem_readb(data+0x4);
	region.src_handle=mem_readw(data+0x5);
	region.src_offset=mem_readw(data+0x7);
	region.src_page_seg=mem_readw(data+0x9);

	region.dest_type=mem_readb(data+0xb);
	region.dest_handle=mem_readw(data+0xc);
	region.dest_offset=mem_readw(data+0xe);
	region.dest_page_seg=mem_readw(data+0x10);
}

static Bit8u MemoryRegion(void) {
	MoveRegion region;
	Bit8u buf_src[MEM_PAGE_SIZE];
	Bit8u buf_dest[MEM_PAGE_SIZE];
	if (reg_al>1) {
		LOG(LOG_MISC,LOG_ERROR)("EMS:Call %2X Subfunction %2X not supported",reg_ah,reg_al);
		return EMM_FUNC_NOSUP;
	}
	LoadMoveRegion(SegPhys(ds)+reg_si,region);
	/* Parse the region for information */
	PhysPt src_mem,dest_mem;
	MemHandle src_handle,dest_handle;
	Bitu src_off,dest_off;Bitu src_remain,dest_remain;
	if (!region.src_type) {
		src_mem=region.src_page_seg*16+region.src_offset;
	} else {
		if (!ValidHandle(region.src_handle)) return EMM_INVALID_HANDLE;
		if ((emm_handles[region.src_handle].pages*EMM_PAGE_SIZE) < ((region.src_page_seg*EMM_PAGE_SIZE)+region.src_offset+region.bytes)) return EMM_LOG_OUT_RANGE;
		src_handle=emm_handles[region.src_handle].mem;
		Bitu pages=region.src_page_seg*4+(region.src_offset/MEM_PAGE_SIZE);
		for (;pages>0;pages--) src_handle=MEM_NextHandle(src_handle);
		src_off=region.src_offset&(MEM_PAGE_SIZE-1);
		src_remain=MEM_PAGE_SIZE-src_off;
	}
	if (!region.dest_type) {
		dest_mem=region.dest_page_seg*16+region.dest_offset;
	} else {
		if (!ValidHandle(region.dest_handle)) return EMM_INVALID_HANDLE;
		if (emm_handles[region.dest_handle].pages*EMM_PAGE_SIZE < (region.dest_page_seg*EMM_PAGE_SIZE)+region.dest_offset+region.bytes) return EMM_LOG_OUT_RANGE;
		dest_handle=emm_handles[region.dest_handle].mem;
		Bitu pages=region.dest_page_seg*4+(region.dest_offset/MEM_PAGE_SIZE);
		for (;pages>0;pages--) dest_handle=MEM_NextHandle(dest_handle);
		dest_off=region.dest_offset&(MEM_PAGE_SIZE-1);
		dest_remain=MEM_PAGE_SIZE-dest_off;
	}
	Bitu toread;
	while (region.bytes>0) {
		if (region.bytes>MEM_PAGE_SIZE) toread=MEM_PAGE_SIZE;
		else toread=region.bytes;
		/* Read from the source */
		if (!region.src_type) {
			MEM_BlockRead(src_mem,buf_src,toread);
		} else {
			if (toread<src_remain) {
				MEM_BlockRead((src_handle*MEM_PAGE_SIZE)+src_off,buf_src,toread);
			} else {
				MEM_BlockRead((src_handle*MEM_PAGE_SIZE)+src_off,buf_src,src_remain);
				MEM_BlockRead((MEM_NextHandle(src_handle)*MEM_PAGE_SIZE),&buf_src[src_remain],toread-src_remain);
			}
		}
		/* Check for a move */
		if (reg_al==1) {
			/* Read from the destination */
			if (!region.dest_type) {
				MEM_BlockRead(dest_mem,buf_dest,toread);
			} else {
				if (toread<dest_remain) {
					MEM_BlockRead((dest_handle*MEM_PAGE_SIZE)+dest_off,buf_dest,toread);
				} else {
					MEM_BlockRead((dest_handle*MEM_PAGE_SIZE)+dest_off,buf_dest,dest_remain);
					MEM_BlockRead((MEM_NextHandle(dest_handle)*MEM_PAGE_SIZE),&buf_dest[dest_remain],toread-dest_remain);
				}
			}
			/* Write to the source */
			if (!region.src_type) {
				MEM_BlockWrite(src_mem,buf_dest,toread);
			} else {
				if (toread<src_remain) {
					MEM_BlockWrite((src_handle*MEM_PAGE_SIZE)+src_off,buf_dest,toread);
				} else {
					MEM_BlockWrite((src_handle*MEM_PAGE_SIZE)+src_off,buf_dest,src_remain);
					MEM_BlockWrite((MEM_NextHandle(src_handle)*MEM_PAGE_SIZE),&buf_dest[src_remain],toread-src_remain);
				}
			}
		}
		/* Write to the destination */
		if (!region.dest_type) {
			MEM_BlockWrite(dest_mem,buf_src,toread);
		} else {
			if (toread<dest_remain) {
				MEM_BlockWrite((dest_handle*MEM_PAGE_SIZE)+dest_off,buf_src,toread);
			} else {
				MEM_BlockWrite((dest_handle*MEM_PAGE_SIZE)+dest_off,buf_src,dest_remain);
				MEM_BlockWrite((MEM_NextHandle(dest_handle)*MEM_PAGE_SIZE),&buf_src[dest_remain],toread-dest_remain);
			}
		}
		/* Advance the pointers */
		if (!region.src_type) src_mem+=toread;
		else src_handle=MEM_NextHandle(src_handle);
		if (!region.dest_type) dest_mem+=toread;
		else dest_handle=MEM_NextHandle(dest_handle);
		region.bytes-=toread;
	}
	return EMM_NO_ERROR;
}


static Bitu INT67_Handler(void) {
	Bitu i;
	switch (reg_ah) {
	case 0x40:		/* Get Status */
		reg_ah=EMM_NO_ERROR;	
		break;
	case 0x41:		/* Get PageFrame Segment */
		reg_bx=EMM_PAGEFRAME;
		reg_ah=EMM_NO_ERROR;
		break;
	case 0x42:		/* Get number of pages */
		reg_dx=MEM_TotalPages()/4;		//Not entirely correct but okay
		reg_bx=EMM_GetFreePages();
		reg_ah=EMM_NO_ERROR;
		break;
	case 0x43:		/* Get Handle and Allocate Pages */
		reg_ah=EMM_AllocateMemory(reg_bx,reg_dx);
		break;
	case 0x44:		/* Map Expanded Memory Page */
		reg_ah=EMM_MapPage(reg_al,reg_dx,reg_bx);
		break;
	case 0x45:		/* Release handle and free pages */
		reg_ah=EMM_ReleaseMemory(reg_dx);
		break;
	case 0x46:		/* Get EMM Version */
		reg_ah=EMM_NO_ERROR;
		reg_al=EMM_VERSION;
		break;
	case 0x47:		/* Save Page Map */
		reg_ah=EMM_SavePageMap(reg_dx);
		break;
	case 0x48:		/* Restore Page Map */
		reg_ah=EMM_RestorePageMap(reg_dx);
		break;
	case 0x4b:		/* Get Handle Count */
		reg_bx=0;
		for (i=0;i<EMM_MAX_HANDLES;i++) if (emm_handles[i].pages!=NULL_HANDLE) reg_bx++;
		reg_ah=EMM_NO_ERROR;
		break;
	case 0x4c:		/* Get Pages for one Handle */
		if (!ValidHandle(reg_dx)) {reg_ah=EMM_INVALID_HANDLE;break;}
		reg_bx=emm_handles[reg_dx].pages;
		reg_ah=EMM_NO_ERROR;
		break;
	case 0x4d:		/* Get Pages for all Handles */
		reg_ah=EMM_GetPagesForAllHandles(SegPhys(es)+reg_di,reg_bx);
		break;
	case 0x4e:		/*Save/Restore Page Map */
		switch (reg_al) {
		case 0x00:	/* Save Page Map */
			MEM_BlockWrite(SegPhys(es)+reg_di,emm_mappings,sizeof(emm_mappings));
			reg_ah=EMM_NO_ERROR;
			break;
		case 0x01:	/* Restore Page Map */
			MEM_BlockRead(SegPhys(ds)+reg_si,emm_mappings,sizeof(emm_mappings));
			reg_ah=EMM_RestoreMappingTable();
			break;
		case 0x02:	/* Save and Restore Page Map */
			MEM_BlockWrite(SegPhys(es)+reg_di,emm_mappings,sizeof(emm_mappings));
			MEM_BlockRead(SegPhys(ds)+reg_si,emm_mappings,sizeof(emm_mappings));
			reg_ah=EMM_RestoreMappingTable();
			break;	
		case 0x03:	/* Get Page Map Array Size */
			reg_al=sizeof(emm_mappings);
			reg_ah=EMM_NO_ERROR;
			break;
		default:
			LOG(LOG_MISC,LOG_ERROR)("EMS:Call %2X Subfunction %2X not supported",reg_ah,reg_al);
			reg_ah=EMM_FUNC_NOSUP;
			break;
		}
		break;
	case 0x4f:	/* Save/Restore Partial Page Map */
		reg_ah=EMM_PartialPageMapping();
		break;
	case 0x50:	/* Map/Unmap multiple handle pages */
		reg_ah = EMM_NO_ERROR;
		switch (reg_al) {
			case 0x00: // use physical page numbers
				{	PhysPt data = SegPhys(ds)+reg_si;
					for (int i=0; i<reg_cx; i++) {
						Bit16u logPage	= mem_readw(data); data+=2;
						Bit16u physPage = mem_readw(data); data+=2;
						reg_ah = EMM_MapPage(physPage,reg_dx,logPage);
						if (reg_ah!=EMM_NO_ERROR) break;
					};
				} break;
			case 0x01: // use segment address 
				{	PhysPt data = SegPhys(ds)+reg_si;
					for (int i=0; i<reg_cx; i++) {
						Bit16u logPage	= mem_readw(data); data+=2;
						Bit16u physPage = (mem_readw(data)-EMM_PAGEFRAME)/(0x1000/EMM_MAX_PHYS); data+=2;
						reg_ah = EMM_MapPage(physPage,reg_dx,logPage);
						if (reg_ah!=EMM_NO_ERROR) break;
					};
				}
				break;
		}
		break;
	case 0x51:	/* Reallocate Pages */
		reg_ah=EMM_ReallocatePages(reg_dx,reg_bx);
		break;
	case 0x53: // Set/Get Handlename
		reg_ah=GetSetHandleName();
		break;
	case 0x54:	/* Handle Functions */
		reg_ah=HandleNameSearch();
		break;
	case 0x57:	/* Memory region */
		reg_ah=MemoryRegion();
		if (reg_ah) LOG(LOG_MISC,LOG_ERROR)("EMS:Function 57 move failed");
		break;
	case 0x58: // Get mappable physical array address array
		if (reg_al==0x00) {
			PhysPt data = SegPhys(es)+reg_di;
			Bit16u step = 0x1000 / EMM_MAX_PHYS;
			for (Bit16u i=0; i<EMM_MAX_PHYS; i++) {
				mem_writew(data,EMM_PAGEFRAME+step*i);	data+=2;
				mem_writew(data,i);						data+=2;
			};
		};
		// Set number of pages
		reg_cx = EMM_MAX_PHYS;
		reg_ah = EMM_NO_ERROR;
		break;
	case 0x5A:              /* Allocate standard/raw Pages */
		if (reg_al==0x00) {
			reg_ah=EMM_AllocateMemory(reg_bx,reg_dx);
		} else {
			LOG(LOG_MISC,LOG_ERROR)("EMS:Call 5A subfct %2X not supported",reg_al);
			reg_ah=EMM_FUNC_NOSUP;
		};
		break;
	case 0xDE:		/* VCPI Functions */
		LOG(LOG_MISC,LOG_ERROR)("EMS:VCPI Call %2X not supported",reg_al);
		reg_ah=EMM_FUNC_NOSUP;
		break;
	default:
		LOG(LOG_MISC,LOG_ERROR)("EMS:Call %2X not supported",reg_ah);
		reg_ah=EMM_FUNC_NOSUP;
		break;
	}
	return CBRET_NONE;
}

static Bitu INT4B_Handler() {
	switch (reg_ah) {
	case 0x81:
		CALLBACK_SCF(true);
		reg_ax=0x1;
		break;
	default:
		LOG(LOG_MISC,LOG_WARN)("Unhandled interrupt 4B function %x",reg_ah);
		break;
	}
	return CBRET_NONE;
}

void EMS_Init(Section* sec) {
	/* Virtual DMA interrupt callback */
	call_vdma=CALLBACK_Allocate();
	CALLBACK_Setup(call_vdma,&INT4B_Handler,CB_IRET);
	RealSetVec(0x4b,CALLBACK_RealPointer(call_vdma));

	Section_prop * section=static_cast<Section_prop *>(sec);
	if (!section->Get_bool("ems")) return;
	BIOS_ZeroExtendedSize();
	call_int67=CALLBACK_Allocate();	
	CALLBACK_Setup(call_int67,&INT67_Handler,CB_IRET);
/* Register the ems device */
	DOS_Device * newdev = new device_EMM();
	DOS_AddDevice(newdev);

/* Add a little hack so it appears that there is an actual ems device installed */
	char * emsname="EMMXXXX0";
	Bit16u seg=DOS_GetMemory(2);	//We have 32 bytes
	MEM_BlockWrite(PhysMake(seg,0xa),emsname,strlen(emsname)+1);
/* Copy the callback piece into the beginning, and set the interrupt vector to it*/
	char buf[16];
	MEM_BlockRead(PhysMake(CB_SEG,call_int67<<4),buf,0xa);
	MEM_BlockWrite(PhysMake(seg,0),buf,0xa);
	RealSetVec(0x67,RealMake(seg,0));
/* Clear handle and page tables */
	Bitu i;
	for (i=0;i<EMM_MAX_HANDLES;i++) {
		emm_handles[i].mem=0;
		emm_handles[i].pages=NULL_HANDLE;
		memset(&emm_handles[i].name,0,8);
	}
	for (i=0;i<EMM_MAX_PHYS;i++) {
		emm_mappings[i].page=NULL_PAGE;
		emm_mappings[i].handle=NULL_HANDLE;
	}
}

