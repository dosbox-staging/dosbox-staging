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
#include <stdio.h>
#include "dosbox.h"
#include "mem.h"

#define MEM_MAXSIZE 16				/* The Size of memory used to get size of page table */
#define memsize 8					/* 8 mb of memory */
#define EMM_HANDLECOUNT 250

EMM_Handle EMM_Handles[EMM_HANDLECOUNT];
PageEntry * PageEntries[MEM_MAXSIZE*1024*16]; /* Number of pages */
Bit8u * memory=0;

bool MEMORY_TestSpecial(PhysPt off) {
	return (PageEntries[off >> 12]>0);
}

void MEMORY_SetupHandler(Bit32u page,Bit32u pages,PageEntry * entry) {
	for (Bit32u i=page;i<page+pages;i++) {
		PageEntries[i]=entry;
	}
}


void MEMORY_ResetHandler(Bit32u page,Bit32u pages) {
	for (Bit32u i=page;i<page+pages;i++) {
		PageEntries[i]=0;
	}
};

void MEM_BlockRead(PhysPt off,void * data,Bitu size) {
	Bitu c;
	Bit8u * idata=(Bit8u *)data;
	for (c=1;c<=(size>>2);c++) {
		writed(idata,mem_readd(off));
		idata+=4;off+=4;
	}
	for (c=1;c<=(size&3);c++) {
		writeb(idata,mem_readb(off));
		idata+=1;off+=1;
	}
}

void MEM_BlockWrite(PhysPt off,void * data,Bitu size) {
	Bitu c;
	Bit8u * idata=(Bit8u *)data;
	for (c=1;c<=(size>>2);c++) {
		mem_writed(off,readd(idata));
		idata+=4;off+=4;
	}
	for (c=1;c<=(size&3);c++) {
		mem_writeb(off,readb(idata));
		idata+=1;off+=1;
	}
}

void MEM_BlockCopy(PhysPt dest,PhysPt src,Bitu size) {
	Bitu c;
	for (c=1;c<=(size>>2);c++) {
		mem_writed(dest,mem_readd(src));
		dest+=4;src+=4;
	}
	for (c=1;c<=(size&3);c++) {
		mem_writeb(dest,mem_readb(src));
		dest+=1;src+=1;
	}


};

void MEM_StrCopy(PhysPt off,char * data,Bitu size) {
	Bit8u c;
	while ((c=mem_readb(off)) && size) {
		*data=c;
		off++;data++;size--;
	}
	*data='\0';
}




/* TODO Maybe check for page boundaries but that would be wasting lot's of time */
void mem_writeb(PhysPt off,Bit8u val) {
	PageEntry * entry=PageEntries[off >> 12];
	if (!entry) { writeb(memory+off,val);return; }
	switch (entry->type) {
	case MEMORY_RELOCATE:
		writeb(entry->relocate+(off-entry->base),val);
		break;
	case MEMORY_HANDLER:
		entry->handler.write(off-entry->base,val);
		break;
	default:
		E_Exit("Write to Illegal Memory Address %4x",off);
	}
}

void mem_writew(PhysPt off,Bit16u val) {
	PageEntry * entry=PageEntries[off >> 12];
	if (!entry) { writew(memory+off,val);return; }
	switch (entry->type) {
	case MEMORY_RELOCATE:
		writew(entry->relocate+(off-entry->base),val);
		break;
	case MEMORY_HANDLER:
		entry->handler.write(off-entry->base,(val & 0xFF));
		entry->handler.write(off-entry->base+1,(val >> 8));
		break;
	default:
		E_Exit("Write to Illegal Memory Address %4x",off);
	}
}

void mem_writed(PhysPt off,Bit32u val) {
	PageEntry * entry=PageEntries[off >> 12];
	if (!entry) { writed(memory+off,val);return; }
	switch (entry->type) {
	case MEMORY_RELOCATE:
		writed(entry->relocate+(off-entry->base),val);
		break;
	case MEMORY_HANDLER:
		entry->handler.write(off-entry->base,	(Bit8u)(val & 0xFF));
		entry->handler.write(off-entry->base+1,(Bit8u)(val >> 8) & 0xFF);
		entry->handler.write(off-entry->base+2,(Bit8u)(val >> 16) & 0xFF);
		entry->handler.write(off-entry->base+3,(Bit8u)(val >> 24) & 0xFF);
		break;
	default:
		E_Exit("Write to Illegal Memory Address %4x",off);
	}
}

Bit8u mem_readb(PhysPt off) {
	PageEntry * entry=PageEntries[off >> 12];
	if (!entry) { return readb(memory+off);}
	switch (entry->type) {
	case MEMORY_RELOCATE:
		return readb(entry->relocate+(off-entry->base));
	case MEMORY_HANDLER:
		return entry->handler.read(off-entry->base);
		break;
	default:
		E_Exit("Read from Illegal Memory Address %4x",off);
	}
	return 0;			/* Keep compiler happy */
}

Bit16u mem_readw(PhysPt off) {
	PageEntry * entry=PageEntries[off >> 12];
	if (!entry) { return readw(memory+off);}
	switch (entry->type) {
	case MEMORY_RELOCATE:
		return readw(entry->relocate+(off-entry->base));
	case MEMORY_HANDLER:
		return 	entry->handler.read(off-entry->base) |
			(entry->handler.read(off-entry->base+1) << 8);
		break;
	default:
		E_Exit("Read from Illegal Memory Address %4x",off);
	}
	return 0;			/* Keep compiler happy */
}

Bit32u mem_readd(PhysPt off) {
	PageEntry * entry=PageEntries[off >> 12];
	if (!entry) { return readd(memory+off);}
	switch (entry->type) {
	case MEMORY_RELOCATE:
		return readd(entry->relocate+(off-entry->base));
	case MEMORY_HANDLER:
		return	entry->handler.read(off-entry->base) |
			(entry->handler.read(off-entry->base+1) << 8) |
			(entry->handler.read(off-entry->base+2) << 16)|
			(entry->handler.read(off-entry->base+3) << 24);
		break;
	default:
		E_Exit("Read from Illegal Memory Address %4x",off);
	}
	return 0;			/* Keep compiler happy */
}

/* The EMM Allocation Part */

/* If this returns 0 we got and error since 0 is always taken */
static Bit16u EMM_GetFreeHandle(void) {
	Bit16u i=0;
	while (i<EMM_HANDLECOUNT) {
		if (!EMM_Handles[i].active) return i;
		i++;
	}
	E_Exit("MEMORY:Out of EMM Memory handles");
	return 0;
}

void EMM_GetFree(Bit16u * maxblock,Bit16u * total) {
	Bit32u index=0;
	*maxblock=0;*total=0;
	while (EMM_Handles[index].active) {
		if (EMM_Handles[index].free) {
			if(EMM_Handles[index].size>*maxblock) *maxblock=EMM_Handles[index].size;
			*total+=EMM_Handles[index].size;
		}
		if (EMM_Handles[index].next) index=EMM_Handles[index].next;
		else break;
	}
}

void EMM_Allocate(Bit16u size,Bit16u * handle) {
	Bit16u index=0;*handle=0;
	while (EMM_Handles[index].active) {
		if (EMM_Handles[index].free) {
			/* Use entire block */
			if(EMM_Handles[index].size==size) {
				EMM_Handles[index].free=false;
				*handle=index;
				break;
			}
			/* Split up block */
			if(EMM_Handles[index].size>size) {
				Bit16u newindex=EMM_GetFreeHandle();
				EMM_Handles[newindex].active=true;
				EMM_Handles[newindex].phys_base=EMM_Handles[newindex].phys_base+size*4096;
				EMM_Handles[newindex].size=EMM_Handles[index].size-size;
				EMM_Handles[newindex].free=true;
				EMM_Handles[newindex].next=EMM_Handles[index].next;
				EMM_Handles[index].next=newindex;
				EMM_Handles[index].free=false;
				EMM_Handles[index].size=size;
				*handle=index;
				break;
			}
		}
		if (EMM_Handles[index].next) index=EMM_Handles[index].next;
		else break;
	}
}

void EMM_Free(Bit16u handle) {
	if (!EMM_Handles[handle].active) E_Exit("EMM:Tried to free illegal handle");
	EMM_Handles[handle].free=true;
	//TODO join memory blocks
}


PageEntry HMA_PageEntry;

void MEM_Init(void) {
	memset((void *)&PageEntries,0,sizeof(PageEntries));
	memory=(Bit8u *)malloc(memsize*1024*1024);	
	if (!memory) {
		E_Exit("Can't allocate memory for memory");
	}
	/* Setup the HMA to wrap */
	HMA_PageEntry.type=MEMORY_RELOCATE;;
	HMA_PageEntry.base=1024*1024;
	HMA_PageEntry.relocate=memory;
	Bitu i;
	for (i=0;i<16;i++) {
		PageEntries[i+256]=&HMA_PageEntry;
	}
	/* Setup the EMM Structures */
	for (i=0;i<EMM_HANDLECOUNT;i++) {
		EMM_Handles[i].active=false;
		EMM_Handles[i].size=0;
	}
	/* Setup the first handle with free and max memory */
	EMM_Handles[0].active=true;
	EMM_Handles[0].free=false;
	EMM_Handles[0].phys_base=0x110000;
	EMM_Handles[0].next=1;
	if (memsize>1) {
		EMM_Handles[1].size=(memsize-1)*256-16;
	} else {
		EMM_Handles[0].size=0;;
	}
	EMM_Handles[1].active=true;
	EMM_Handles[1].free=true;
	EMM_Handles[1].phys_base=0x110000;
};


