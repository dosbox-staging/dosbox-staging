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

#if !defined __MEM_H
#define __MEM_H
#include <dosbox.h>

#define bmemcpy(mem1,mem2,size) memcpy((void *)mem1,(void *)mem2,size)

typedef Bit32u PhysPt;
typedef Bit8u * HostPt;
typedef Bit32u RealPt;

typedef Bit8u (*MEMORY_ReadHandler)(PhysPt pt);
typedef void (*MEMORY_WriteHandler)(PhysPt pt,Bit8u val);

#define PAGE_KB	16
#define PAGE_SIZE (PAGE_KB*1024)
#define PAGE_SHIFT 14
#define PAGE_COUNT(A) (A & ((1 << PAGE_SHIFT)-1) ? 1+(A >> PAGE_SHIFT) : (A >> PAGE_SHIFT) )
#define MAX_PAGES PAGE_COUNT(C_MEM_MAX_SIZE*1024*1024)

extern HostPt ReadHostTable[MAX_PAGES];
extern HostPt WriteHostTable[MAX_PAGES];
extern MEMORY_ReadHandler ReadHandlerTable[MAX_PAGES];
extern MEMORY_WriteHandler WriteHandlerTable[MAX_PAGES];


INLINE Bit16u PAGES(Bit32u bytes) {
	if ((bytes & 4095) == 0) return (Bit16u)(bytes>>12);
	return (Bit16u)(1+(bytes>>12));
}

void MEM_SetupPageHandlers(Bitu startpage,Bitu pages,MEMORY_ReadHandler read,MEMORY_WriteHandler write);
void MEM_ClearPageHandlers(Bitu startpage,Bitu pages);

void MEM_SetupMapping(Bitu startpage,Bitu pages,void * data);
void MEM_ClearMapping(Bitu startpage,Bitu pages);

bool MEM_A20_Enabled(void);
void MEM_A20_Enable(bool enable);

Bitu MEM_TotalSize(void);			//Memory size in KB 

extern HostPt memory;

/* 
	The folowing six functions are used everywhere in the end so these should be changed for
	Working on big or little endian machines 
*/

#ifdef WORDS_BIGENDIAN

INLINE Bit8u readb(HostPt off) {
	return off[0];
};
INLINE Bit16u readw(HostPt off) {
	return off[0] | (off[1] << 8);
};
INLINE Bit32u readd(HostPt off) {
	return off[0] | (off[1] << 8) | (off[2] << 16) | (off[3] << 24);
};
INLINE void writeb(HostPt off,Bit8u val) {
	off[0]=val;
};
INLINE void writew(HostPt off,Bit16u val) {
	off[0]=(Bit8u)((val & 0x00ff));
	off[1]=(Bit8u)((val & 0xff00) >> 8);
};
INLINE void writed(HostPt off,Bit32u val) {
	off[0]=(Bit8u)((val & 0x000000ff));
	off[1]=(Bit8u)((val & 0x0000ff00) >> 8);
	off[2]=(Bit8u)((val & 0x00ff0000) >> 16);
	off[3]=(Bit8u)((val & 0xff000000) >> 24);
};

#else

INLINE Bit8u readb(HostPt off) {
	return *(Bit8u *)off;
};
INLINE Bit16u readw(HostPt off) {
	return *(Bit16u *)off;
};
INLINE Bit32u readd(HostPt off) {
	return *(Bit32u *)off;
};
INLINE void writeb(HostPt off,Bit8u val) {
	*(Bit8u *)(off)=val;
};
INLINE void writew(HostPt off,Bit16u val) {
	*(Bit16u *)(off)=val;
};
INLINE void writed(HostPt off,Bit32u val) {
	*(Bit32u *)(off)=val;
};

#endif

/* The Folowing six functions are slower but they recognize the paged memory system */
//TODO maybe make em inline to go a bit faster 


Bit8u  mem_readb(PhysPt pt);
Bit16u mem_readw(PhysPt pt);
Bit32u mem_readd(PhysPt pt);

void mem_writeb(PhysPt pt,Bit8u val);
void mem_writew(PhysPt pt,Bit16u val);
void mem_writed(PhysPt pt,Bit32u val);

INLINE void mem_writeb_inline(PhysPt pt,Bit8u val) {
	if (WriteHostTable[pt >> PAGE_SHIFT]) writeb(WriteHostTable[pt >> PAGE_SHIFT]+pt,val);
	else {
		WriteHandlerTable[pt >> PAGE_SHIFT](pt,val);
	}
}

INLINE void mem_writew_inline(PhysPt pt,Bit16u val) {
	if (WriteHostTable[pt >> PAGE_SHIFT]) writew(WriteHostTable[pt >> PAGE_SHIFT]+pt,val);
	else {
		WriteHandlerTable[pt >> PAGE_SHIFT](pt+0,(Bit8u)(val & 0xff));
		WriteHandlerTable[pt >> PAGE_SHIFT](pt+1,(Bit8u)((val >> 8) & 0xff)  );
	}
}

INLINE void mem_writed_inline(PhysPt pt,Bit32u val) {
	if (WriteHostTable[pt >> PAGE_SHIFT]) writed(WriteHostTable[pt >> PAGE_SHIFT]+pt,val);
	else {
		WriteHandlerTable[pt >> PAGE_SHIFT](pt+0,(Bit8u)(val & 0xff));
		WriteHandlerTable[pt >> PAGE_SHIFT](pt+1,(Bit8u)((val >>  8) & 0xff)  );
		WriteHandlerTable[pt >> PAGE_SHIFT](pt+2,(Bit8u)((val >> 16) & 0xff)  );
		WriteHandlerTable[pt >> PAGE_SHIFT](pt+3,(Bit8u)((val >> 24) & 0xff)  );
	}
}

INLINE Bit8u mem_readb_inline(PhysPt pt) {
	if (ReadHostTable[pt >> PAGE_SHIFT]) return readb(ReadHostTable[pt >> PAGE_SHIFT]+pt);
	else {
		return ReadHandlerTable[pt >> PAGE_SHIFT](pt);
	}
}

INLINE Bit16u mem_readw_inline(PhysPt pt) {
	if (ReadHostTable[pt >> PAGE_SHIFT]) return readw(ReadHostTable[pt >> PAGE_SHIFT]+pt);
	else {
		return 
			(ReadHandlerTable[pt >> PAGE_SHIFT](pt+0)) |
			(ReadHandlerTable[pt >> PAGE_SHIFT](pt+1)) << 8;
	}
}

INLINE Bit32u mem_readd_inline(PhysPt pt){
	if (ReadHostTable[pt >> PAGE_SHIFT]) return readd(ReadHostTable[pt >> PAGE_SHIFT]+pt);
	else {
		return 
			(ReadHandlerTable[pt >> PAGE_SHIFT](pt+0))       |
			(ReadHandlerTable[pt >> PAGE_SHIFT](pt+1)) << 8  |
			(ReadHandlerTable[pt >> PAGE_SHIFT](pt+2)) << 16 |
			(ReadHandlerTable[pt >> PAGE_SHIFT](pt+3)) << 24;
	}
}

void MEM_BlockWrite(PhysPt pt,void * data,Bitu size);
void MEM_BlockRead(PhysPt pt,void * data,Bitu size);
void MEM_BlockCopy(PhysPt dest,PhysPt src,Bitu size);
void MEM_StrCopy(PhysPt pt,char * data,Bitu size);

/* The folowing functions are all shortcuts to the above functions using physical addressing */

INLINE Bit8u real_readb(Bit16u seg,Bit16u off) {
	return mem_readb((seg<<4)+off);
}
INLINE Bit16u real_readw(Bit16u seg,Bit16u off) {
	return mem_readw((seg<<4)+off);
}
INLINE Bit32u real_readd(Bit16u seg,Bit16u off) {
	return mem_readd((seg<<4)+off);
}

INLINE void real_writeb(Bit16u seg,Bit16u off,Bit8u val) {
	mem_writeb(((seg<<4)+off),val);
}
INLINE void real_writew(Bit16u seg,Bit16u off,Bit16u val) {
	mem_writew(((seg<<4)+off),val);
}
INLINE void real_writed(Bit16u seg,Bit16u off,Bit32u val) {
	mem_writed(((seg<<4)+off),val);
}

INLINE HostPt HostMake(Bit16u seg,Bit16u off) {
	return memory+(seg<<4)+off;
}

INLINE HostPt Phys2Host(PhysPt pt) {
	return memory+pt;
}

INLINE Bit16u RealSeg(RealPt pt) {
	return (Bit16u)(pt>>16);
}

INLINE Bit16u RealOff(RealPt pt) {
	return (Bit16u)(pt&0xffff);
}

INLINE PhysPt Real2Phys(RealPt pt) {
	return (RealSeg(pt)<<4) +RealOff(pt);
}

INLINE PhysPt PhysMake(Bit16u seg,Bit16u off) {
	return (seg<<4)+off;
}

INLINE HostPt Real2Host(RealPt pt) {
	return memory+(RealSeg(pt)<<4) +RealOff(pt);
}

INLINE RealPt RealMake(Bit16u seg,Bit16u off) {
	return (seg<<16)+off;
}

INLINE void RealSetVec(Bit8u vec,RealPt pt) {
	mem_writed(vec<<2,pt);
}	

INLINE RealPt RealGetVec(Bit8u vec) {
	return mem_readd(vec<<2);
}	

#endif

