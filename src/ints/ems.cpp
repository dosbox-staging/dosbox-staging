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
#include <stdlib.h>
#include "dosbox.h"
#include "callback.h"
#include "mem.h"
#include "bios.h"
#include "keyboard.h"
#include "regs.h"
#include "inout.h"
#include "dos_inc.h"


#define EMM_PAGEFRAME	0xE000
#define	EMM_MAX_HANDLES	50				/* 255 Max */
#define EMM_PAGE_SIZE	(16*1024)
#define EMM_MAX_PAGES	(C_MEM_EMS_SIZE * 1024 / 16 )
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
		LOG_DEBUG("Write to ems device");	
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

struct EMM_Page {
	void * memory;
	Bit16u handle;
	Bit16u next;
};

struct EMM_Handle {
	Bit16u first_page;
	Bit16u pages;
	char name[9];
	bool saved_page_map;
	EMM_Mapping page_map[EMM_MAX_PHYS];
};

static EMM_Handle emm_handles[EMM_MAX_HANDLES];
static EMM_Page emm_pages[EMM_MAX_PAGES];
static EMM_Mapping emm_mappings[EMM_MAX_PHYS];
Bitu call_int67;

static Bit16u EMM_GetFreePages(void) {
	Bit16u count=0;
	for (Bitu index=0;index<EMM_MAX_PAGES;index++) {
		if (emm_pages[index].handle==NULL_HANDLE) count++;
	}
	return count;
}

static Bit8u EMM_AllocateMemory(Bit16u pages,Bit16u & handle) {
	/* Check for 0 page allocation */
	if (!pages) return EMM_ZERO_PAGES;
	/* Check for enough free pages */
	if (EMM_GetFreePages()<pages){ handle=NULL_HANDLE; return EMM_OUT_OF_LOG;}
	handle=1;
	/* Check for a free handle */
	while (emm_handles[handle].first_page!=NULL_PAGE) {
		if (++handle>=EMM_MAX_HANDLES) {handle=NULL_HANDLE;return EMM_OUT_OF_HANDLES;}
	}
	/* Allocate the pages */
	Bit16u page=0;Bit16u last=NULL_PAGE;
	emm_handles[handle].pages=pages;
	while (pages) {
		if (emm_pages[page].handle==NULL_HANDLE) {
			emm_pages[page].handle=handle;
			emm_pages[page].memory=malloc(EMM_PAGE_SIZE);
			if (!emm_pages[page].memory) E_Exit("EMM:Cannont allocate memory");
			if (last!=NULL_PAGE) emm_pages[last].next=page;
			else emm_handles[handle].first_page=page;
			last=page;
			pages--;
		} else {
			if (++page>=EMM_MAX_PAGES) E_Exit("EMM:Ran out of pages");
		}
	}
	return EMM_NO_ERROR;
}

static Bit8u EMM_ReallocatePages(Bit16u handle,Bit16u & pages) {
	/* Check for valid handle */
	if (handle>=EMM_MAX_HANDLES || emm_handles[handle].first_page==NULL_PAGE) return EMM_INVALID_HANDLE;
	/* Check for enough pages */
	if ((emm_handles[handle].pages+EMM_GetFreePages())<pages) return EMM_OUT_OF_LOG;
	Bit16u page=emm_handles[handle].first_page;
	Bit16u last=NULL_PAGE;
	Bit16u page_count=emm_handles[handle].pages;
	while (pages>0 && page_count>0) {
		if (emm_pages[page].handle!=handle) E_Exit("EMM:Error illegal handle reference");
		last=page;
		page=emm_pages[page].next;
		pages--;
		page_count--;
	}
	/* Free the rest of the handles */
	if (page_count && !pages) {
		emm_handles[handle].pages-=page_count;
		while (page_count>0) {
			free(emm_pages[page].memory);
			emm_pages[page].memory=0;
			emm_pages[page].handle=NULL_HANDLE;
			Bit16u next_page=emm_pages[page].next;
			emm_pages[page].next=NULL_PAGE;
			page=next_page;page_count--;
		}
		pages=emm_handles[handle].pages;
		return EMM_NO_ERROR;
	} 
	if (!page_count && pages) {
	/* Allocate extra pages */
		emm_handles[handle].pages+=pages;	
		page=0;
		while (pages) {
			if (emm_pages[page].handle==NULL_HANDLE) {
				emm_pages[page].handle=handle;
				emm_pages[page].memory=malloc(EMM_PAGE_SIZE);
				if (!emm_pages[page].memory) E_Exit("EMM:Cannont allocate memory");
				emm_pages[last].next=page;
				last=page;
				pages--;
			} else {
				if (++page>=EMM_MAX_PAGES) E_Exit("EMM:Ran out of pages");
			}
		}
		pages=emm_handles[handle].pages;
		return EMM_NO_ERROR;
	} 
	/* Size exactly the same as the original size */
	pages=emm_handles[handle].pages;
	return EMM_NO_ERROR;
}

static Bit8u EMM_MapPage(Bitu phys_page,Bit16u handle,Bit16u log_page) {
	/* Check for too high physical page */
	if (phys_page>=EMM_MAX_PHYS) return EMM_ILL_PHYS;
	/* Check for valid handle */
	if (handle>=EMM_MAX_HANDLES || emm_handles[handle].first_page==NULL_PAGE) return EMM_INVALID_HANDLE;
	/* Check to do unmapping or mappning */
	if (log_page<emm_handles[handle].pages) {
		/* Mapping it is */
		emm_mappings[phys_page].handle=handle;
		emm_mappings[phys_page].page=log_page;
		Bit16u index=emm_handles[handle].first_page;
		while (log_page) {
			index=emm_pages[index].next;
			if (index==NULL_PAGE) E_Exit("EMM:Detected NULL Page in chain");
			log_page--;
		}
		/* Do the actual mapping */
		MEM_SetupMapping(PAGE_COUNT(PhysMake(EMM_PAGEFRAME,phys_page*EMM_PAGE_SIZE)),PAGE_COUNT(PAGE_SIZE),emm_pages[index].memory);
		return EMM_NO_ERROR;
	} else if (log_page==NULL_PAGE) {
		/* Unmapping it is */
		emm_mappings[phys_page].handle=NULL_HANDLE;
		emm_mappings[phys_page].page=NULL_PAGE;
		MEM_ClearMapping(PAGE_COUNT(PhysMake(EMM_PAGEFRAME,phys_page*EMM_PAGE_SIZE)),PAGE_COUNT(PAGE_SIZE));
		return EMM_NO_ERROR;
	} else {
		/* Illegal logical page it is */
		return EMM_LOG_OUT_RANGE;
	}
}

static Bit8u EMM_ReleaseMemory(Bit16u handle) {
	/* Check for valid handle */
	if (handle>=EMM_MAX_HANDLES || emm_handles[handle].first_page==NULL_PAGE) return EMM_INVALID_HANDLE;
	Bit16u page=emm_handles[handle].first_page;
	Bit16u pages=emm_handles[handle].pages;
	while (pages) {
		free(emm_pages[page].memory);
		emm_pages[page].memory=0;
		emm_pages[page].handle=NULL_HANDLE;
		Bit16u next_page=emm_pages[page].next;
		emm_pages[page].next=NULL_PAGE;
		page=next_page;pages--;
	}
	/* Reset handle */
	emm_handles[handle].first_page=NULL_PAGE;
	emm_handles[handle].pages=0;
	emm_handles[handle].saved_page_map=false;
	memset(&emm_handles[handle].name,0,9);
	return EMM_NO_ERROR;
}

static Bit8u EMM_SavePageMap(Bit16u handle) {
	/* Check for valid handle */
	if (handle>=EMM_MAX_HANDLES || emm_handles[handle].first_page==NULL_PAGE) return EMM_INVALID_HANDLE;
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

static Bitu EMM_RestoreMappingTable(void) {
	Bit8u result;
	/* Move through the mappings table and setup mapping accordingly */
	for (Bitu i=0;i<EMM_MAX_PHYS;i++) {
		result=EMM_MapPage(i,emm_mappings[i].handle,emm_mappings[i].page);
	}
	return EMM_NO_ERROR;
}
static Bit8u EMM_RestorePageMap(Bit16u handle) {
	/* Check for valid handle */
	if (handle>=EMM_MAX_HANDLES || emm_handles[handle].first_page==NULL_PAGE) return EMM_INVALID_HANDLE;
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
		if (emm_handles[i].first_page!=NULL_PAGE) {
			handles++;
			mem_writew(table,i);
			mem_writew(table+2,emm_handles[i].pages);
			table+=4;
		}
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
		reg_dx=EMM_MAX_PAGES;
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
		for (i=0;i<EMM_MAX_HANDLES;i++) if (emm_handles[i].first_page!=NULL_PAGE) reg_bx++;
		reg_ah=EMM_NO_ERROR;
		break;
	case 0x4c:		/* Get Pages for one Handle */
		/* Check for valid handle */
		if (reg_bx>=EMM_MAX_HANDLES || emm_handles[reg_bx].first_page==NULL_PAGE) {reg_ah=EMM_INVALID_HANDLE;break;}
		reg_bx=emm_handles[reg_dx].pages;
		reg_ah=EMM_NO_ERROR;
		break;
	case 0x4d:		/* Get Pages for all Handles */
		reg_ah=EMM_GetPagesForAllHandles(SegPhys(es)+reg_di,reg_bx);
		break;
	case 0x4e:		/*Save/Restore Page Map */
		LOG_WARN("Save/resetore %d",reg_al);
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
			LOG_ERROR("EMS:Call %2X Subfunction %2X not supported",reg_ah,reg_al);
			reg_ah=EMM_FUNC_NOSUP;
			break;
		}
		break;
	case 0x50: // Map/Unmap multiple handle pages
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
		if (reg_al==0x00) { // Get Name not supported
			LOG_ERROR("EMS:Get handle name not supported",reg_ah);
			reg_ah=EMM_FUNC_NOSUP;					
		} else { // Set name, not supported but faked
			reg_ah=EMM_NO_ERROR;	
		}
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
	case 0xDE:		/* VCPI Functions */
		LOG_ERROR("VCPI Functions not supported");
		reg_ah=EMM_FUNC_NOSUP;
		break;
	default:
		LOG_ERROR("EMS:Call %2X not supported",reg_ah);
		reg_ah=EMM_FUNC_NOSUP;
		break;
	}
	return CBRET_NONE;
}



void EMS_Init(void) {
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
	for (i=0;i<EMM_MAX_PAGES;i++) {
		emm_pages[i].memory=0;
		emm_pages[i].handle=NULL_HANDLE;
		emm_pages[i].next=NULL_PAGE;
	}
	for (i=0;i<EMM_MAX_HANDLES;i++) {
		emm_handles[i].first_page=NULL_PAGE;
		memset(&emm_handles[i].name,0,9);
	}
	for (i=0;i<EMM_MAX_PHYS;i++) {
		emm_mappings[i].page=NULL_PAGE;
		emm_mappings[i].handle=NULL_HANDLE;
	}

}

