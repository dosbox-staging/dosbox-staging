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
#include <stdio.h>
#include "dosbox.h"
#include "mem.h"
#include "inout.h"
#include "setup.h"

HostPt memory;
HostPt ReadHostTable[MAX_PAGES];
HostPt WriteHostTable[MAX_PAGES];
MEMORY_ReadHandler ReadHandlerTable[MAX_PAGES];
MEMORY_WriteHandler WriteHandlerTable[MAX_PAGES];

/* Page handlers only work in lower memory */
#define LOW_PAGE_LIMIT PAGE_COUNT(1024*1024)
#define MAX_PAGE_LIMIT PAGE_COUNT(C_MEM_MAX_SIZE*1024*1024)

void MEM_BlockRead(PhysPt off,void * data,Bitu size) {
	Bit8u * idata=(Bit8u *)data;
	while (size>0) {
		Bitu page=off >> PAGE_SHIFT;
		Bitu start=off & (PAGE_SIZE-1);
		Bitu tocopy=PAGE_SIZE-start;
		if (tocopy>size) tocopy=size;
		size-=tocopy;
		if (ReadHostTable[page]) {
			memcpy(idata,ReadHostTable[page]+off,tocopy);
			idata+=tocopy;off+=tocopy;
		} else {
			for (;tocopy>0;tocopy--) *idata++=ReadHandlerTable[page](off++);
		}
	}
}

void MEM_BlockWrite(PhysPt off,void * data,Bitu size) {
	Bit8u * idata=(Bit8u *)data;
	while (size>0) {
		Bitu page=off >> PAGE_SHIFT;
		Bitu start=off & (PAGE_SIZE-1);
		Bitu tocopy=PAGE_SIZE-start;
		if (tocopy>size) tocopy=size;
		size-=tocopy;
		if (WriteHostTable[page]) {
			memcpy(WriteHostTable[page]+off,idata,tocopy);
			idata+=tocopy;off+=tocopy;
		} else {
			for (;tocopy>0;tocopy--) WriteHandlerTable[page](off++,*idata++);
		}
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

static Bit8u Illegal_ReadHandler(PhysPt pt) {
	LOG(LOG_ERROR,"MEM:Illegal read from address %4X",pt);
	return 0;
}
static void Illegal_WriteHandler(PhysPt pt,Bit8u val) {
	LOG(LOG_ERROR,"Illegal write val %2X to address %4X",val,pt);
}

/* Could only be called when the pt host entry is 0 ah well :) */
static Bit8u Default_ReadHandler(PhysPt pt) {
	return readb(WriteHostTable[pt >> PAGE_SHIFT]+pt);
}
static void Default_WriteHandler(PhysPt pt,Bit8u val) {
	writeb(WriteHostTable[pt >> PAGE_SHIFT]+pt,val);
}


void MEM_SetupPageHandlers(Bitu startpage,Bitu pages,MEMORY_ReadHandler read,MEMORY_WriteHandler write) {
	if (startpage+pages>=LOW_PAGE_LIMIT) E_Exit("Memory:Illegal page for handler");
	for (Bitu i=startpage;i<startpage+pages;i++) {
		ReadHostTable[i]=0;
		WriteHostTable[i]=0;
		ReadHandlerTable[i]=read;
		WriteHandlerTable[i]=write;
	}
}


void MEM_ClearPageHandlers(Bitu startpage,Bitu pages) {
	if (startpage+pages>=LOW_PAGE_LIMIT) E_Exit("Memory:Illegal page for handler");
	for (Bitu i=startpage;i<startpage+pages;i++) {
		ReadHostTable[i]=memory;
		WriteHostTable[i]=memory;
		ReadHandlerTable[i]=&Illegal_ReadHandler;;
		WriteHandlerTable[i]=&Illegal_WriteHandler;
	}
}

void MEM_SetupMapping(Bitu startpage,Bitu pages,void * data) {
	if (startpage+pages>=MAX_PAGE_LIMIT) E_Exit("Memory:Illegal page for handler");
	HostPt base=(HostPt)(data)-startpage*PAGE_SIZE;
	if (!base) LOG_MSG("MEMORY:Unlucky memory allocation");
	for (Bitu i=startpage;i<startpage+pages;i++) {
		ReadHostTable[i]=base;
		WriteHostTable[i]=base;
		ReadHandlerTable[i]=&Default_ReadHandler;;
		WriteHandlerTable[i]=&Default_WriteHandler;
	}
}

void MEM_ClearMapping(Bitu startpage,Bitu pages) {
	if (startpage+pages>=MAX_PAGE_LIMIT) E_Exit("Memory:Illegal page for handler");
	for (Bitu i=startpage;i<startpage+pages;i++) {
		ReadHostTable[i]=0;
		WriteHostTable[i]=0;
		ReadHandlerTable[i]=&Illegal_ReadHandler;;
		WriteHandlerTable[i]=&Illegal_WriteHandler;
	}
}

#if (!C_EXTRAINLINE)
static void HandlerWritew(Bitu page,PhysPt pt,Bit16u val) {
		WriteHandlerTable[page](pt+0,(Bit8u)(val & 0xff));
		WriteHandlerTable[page](pt+1,(Bit8u)((val >>  8) & 0xff)  );
}

static void HandlerWrited(Bitu page,PhysPt pt,Bit32u val) {
		WriteHandlerTable[page](pt+0,(Bit8u)(val & 0xff));
		WriteHandlerTable[page](pt+1,(Bit8u)((val >>  8) & 0xff)  );
		WriteHandlerTable[page](pt+2,(Bit8u)((val >> 16) & 0xff)  );
		WriteHandlerTable[page](pt+3,(Bit8u)((val >> 24) & 0xff)  );
}

void mem_writeb(PhysPt pt,Bit8u val) {
	if (WriteHostTable[pt >> PAGE_SHIFT]) writeb(WriteHostTable[pt >> PAGE_SHIFT]+pt,val);
	else {
		WriteHandlerTable[pt >> PAGE_SHIFT](pt,val);
	}
}

void mem_writew(PhysPt pt,Bit16u val) {
	if (!WriteHostTable[pt >> PAGE_SHIFT]) {
//		HandlerWritew(pt >> PAGE_SHIFT,pt,val);
		WriteHandlerTable[pt >>	PAGE_SHIFT](pt+0,(Bit8u)(val & 0xff));
		WriteHandlerTable[pt >> PAGE_SHIFT](pt+1,(Bit8u)((val >> 8) & 0xff)  );
	} else writew(WriteHostTable[pt >> PAGE_SHIFT]+pt,val);
}

void mem_writed(PhysPt pt,Bit32u val) {
	if (!WriteHostTable[pt >> PAGE_SHIFT]) {
//		HandlerWrited(pt >> PAGE_SHIFT,pt,val);
		WriteHandlerTable[pt >>	PAGE_SHIFT](pt+0,(Bit8u)(val & 0xff));
		WriteHandlerTable[pt >> PAGE_SHIFT](pt+1,(Bit8u)((val >> 8) & 0xff)  );
		WriteHandlerTable[pt >> PAGE_SHIFT](pt+2,(Bit8u)((val >> 16) & 0xff)  );
		WriteHandlerTable[pt >> PAGE_SHIFT](pt+3,(Bit8u)((val >> 24) & 0xff)  );
	} else writed(WriteHostTable[pt >> PAGE_SHIFT]+pt,val);
}


static Bit16u HandlerReadw(Bitu page,PhysPt pt) {
	return	(ReadHandlerTable[page](pt+0))       |
			(ReadHandlerTable[page](pt+1)) << 8;
}

static Bit32u HandlerReadd(Bitu page,PhysPt pt) {
	return	(ReadHandlerTable[page](pt+0))       |
			(ReadHandlerTable[page](pt+1)) << 8  |
			(ReadHandlerTable[page](pt+2)) << 16 |
			(ReadHandlerTable[page](pt+3)) << 24;
}

Bit8u mem_readb(PhysPt pt) {
	if (ReadHostTable[pt >> PAGE_SHIFT]) return readb(ReadHostTable[pt >> PAGE_SHIFT]+pt);
	else {
		return ReadHandlerTable[pt >> PAGE_SHIFT](pt);
	}
}

Bit16u mem_readw(PhysPt pt) {
	if (!ReadHostTable[pt >> PAGE_SHIFT]) {
//		return HandlerReadw(pt >> PAGE_SHIFT,pt);
		return	
			(ReadHandlerTable[pt >> PAGE_SHIFT](pt+0)) |
			(ReadHandlerTable[pt >> PAGE_SHIFT](pt+1)) << 8;
	} else return readw(ReadHostTable[pt >> PAGE_SHIFT]+pt);
}

Bit32u mem_readd(PhysPt pt){
	if (ReadHostTable[pt >> PAGE_SHIFT]) return readd(ReadHostTable[pt >> PAGE_SHIFT]+pt);
	else {
//		return HandlerReadd(pt >> PAGE_SHIFT,pt);
		return 
			(ReadHandlerTable[pt >> PAGE_SHIFT](pt+0))       |
			(ReadHandlerTable[pt >> PAGE_SHIFT](pt+1)) << 8  |
			(ReadHandlerTable[pt >> PAGE_SHIFT](pt+2)) << 16 |
			(ReadHandlerTable[pt >> PAGE_SHIFT](pt+3)) << 24;
	}
}
#endif

// Big block memory alloction

#define GETBIGBLOCKNR(nr) nr/(1024*1024)

static Bit8u*	mem_block[C_MEM_MAX_SIZE];

static bool AllocateBigBlock(Bitu block)
{
	if ((block<1) || (block>C_MEM_MAX_SIZE)) return false;
	if (!mem_block[block]) {
		// Allocate it 
		Bitu start = (block==1)? (1024+64)*1024:1024*1024*block;
		Bitu size  = (block==1)? (1024-64)*1024:1024*1024;
		mem_block[block] = (Bit8u*)malloc(size);
		if (!mem_block[block]) E_Exit("XMS: Failed to allocate XMS block.");
//		else LOG(LOG_ERROR,"XMS: Allocated big block %d.",block);
		// Map it with default handler
		MEM_SetupMapping(PAGE_COUNT(start),PAGE_COUNT(size),mem_block[block]);
		memset(mem_block[block],0,size);
	}
	return true;
};

static Bit8u AllocateMem_ReadHandler(PhysPt pt) 
{
	// Allocate mem, set deafult handler
	if (AllocateBigBlock(GETBIGBLOCKNR(pt))) {
		// Pass request to new handler
		return mem_readb(pt);
	}
	return 0;
}

static void AllocateMem_WriteHandler(PhysPt pt,Bit8u val) 
{
	// Allocate mem, if needed
	if (AllocateBigBlock(GETBIGBLOCKNR(pt))) {
		// Pass request to new handler
		mem_writeb(pt,val);
	}
}

// A20 Line Handlers
static Bit8u controlport_data = 0;
static bool a20_enabled;

bool MEM_A20_Enabled(void) {
	return a20_enabled;
}

void MEM_A20_Enable(bool enable) {
	if (a20_enabled==enable) return;
	a20_enabled=enable;
	if (enable) {
		MEM_SetupMapping(PAGE_COUNT(1024*1024),PAGE_COUNT(64*1024),((Bit8u*)memory)+1024*1024);
	} else {
		MEM_SetupMapping(PAGE_COUNT(1024*1024),PAGE_COUNT(64*1024),memory);
	}
	LOG(LOG_MISC,"A20 Line is %s",enable ? "Enabled" : "Disabled");
}

static void write_p92(Bit32u port,Bit8u val) {	
	// Bit 0 = system reset (switch back to real mode)
	if (val&1) E_Exit("XMS: CPU reset via port 0x92 not supported.");
	controlport_data = val & ~2;
	MEM_A20_Enable((val & 2)>0);
}

static Bit8u read_p92(Bit32u port) {
	return controlport_data | a20_enabled ? 0x02 : 0;
}

static void MEM_ShutDown(Section * sec) {
	for (Bitu i=0; i<C_MEM_MAX_SIZE; i++) {
		free(mem_block[i]);
		mem_block[i] = 0;
	};
	free(memory); memory=0;
};

void MEM_Init(Section * sect) {
	/* Init all tables */
	Bitu i;
	/* Allocate the first mb of memory + hma */
	memory=(Bit8u *)malloc(1024*1024+64*1024);	
	if (!memory) {
		throw("Can't allocate memory for memory");
	}
	memset(memory,0xcd,1024*1024);
	/* Setup tables for first mb */
	MEM_SetupMapping(0,PAGE_COUNT(1024*1024),memory);
	/* Setup tables for HMA Area */
	a20_enabled=false;
	MEM_SetupMapping(PAGE_COUNT(1024*1024),PAGE_COUNT(64*1024),memory);
	// Setup default handlers for unallocated xms
	for (Bitu p=PAGE_COUNT((1024+64)*1024);p<MAX_PAGES;p++) {
		ReadHostTable[p]=0;
		WriteHostTable[p]=0;
		ReadHandlerTable[p]=&AllocateMem_ReadHandler;
		WriteHandlerTable[p]=&AllocateMem_WriteHandler;
	}
	// clear unallocated memory blocks
	for (i=0; i<C_MEM_MAX_SIZE; i++) mem_block[i] = 0;
	// A20 Line - PS/2 system control port A
	IO_RegisterWriteHandler(0x92,write_p92,"Control Port");
	IO_RegisterReadHandler(0x92,read_p92,"Control Port");
	/* shutdown function */
	sect->AddDestroyFunction(&MEM_ShutDown);
}


