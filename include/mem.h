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

enum { MEMORY_HANDLER=1,MEMORY_RELOCATE=2};

#define bmemcpy(mem1,mem2,size) memcpy((void *)mem1,(void *)mem2,size)

typedef Bit8u (MEMORY_ReadHandler)(Bit32u start);
typedef void (MEMORY_WriteHandler)(Bit32u start,Bit8u val);

typedef Bit32u PhysPt;
typedef Bit8u * HostPt;
typedef Bit32u RealPt;

struct PageEntry {
	Bit8u type;
	PhysPt base;						/* Used to calculate relative offset */
	struct {
		MEMORY_WriteHandler * write;
		MEMORY_ReadHandler * read;
	} handler;
	HostPt relocate;					/* This points to host machine address */
};

struct EMM_Handle {
	Bit16u next;
	Bit16u size;					/* Size in pages */
	PhysPt phys_base;
	HostPt host_base;
	bool active;
	bool free;
};

INLINE Bit16u PAGES(Bit32u bytes) {
	if ((bytes & 4095) == 0) return (Bit16u)(bytes>>12);
	return (Bit16u)(1+(bytes>>12));
}

extern Bit8u * memory;
extern EMM_Handle EMM_Handles[];
extern PageEntry * PageEntries[]; /* Number of pages */

bool MEMORY_TestSpecial(PhysPt off);
void MEMORY_SetupHandler(Bit32u page,Bit32u extra,PageEntry * handler);
void MEMORY_ResetHandler(Bit32u page,Bit32u pages);


void EMM_GetFree(Bit16u * maxblock,Bit16u * total);
void EMM_Allocate(Bit16u size,Bit16u * handle);
void EMM_Free(Bit16u handle);


/* 
	The folowing six functions are used everywhere in the end so these should be changed for
	Working on big or little endian machines 
*/
 

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


/* The Folowing six functions are slower but they recognize the paged memory system */
//TODO maybe make em inline to go a bit faster 

Bit8u  mem_readb(PhysPt pt);
Bit16u mem_readw(PhysPt pt);
Bit32u mem_readd(PhysPt pt);

void mem_writeb(PhysPt pt,Bit8u val);
void mem_writew(PhysPt pt,Bit16u val);
void mem_writed(PhysPt pt,Bit32u val);





void MEM_BlockWrite(PhysPt pt,void * data,Bitu size);
void MEM_BlockRead(PhysPt pt,void * data,Bitu size);
void MEM_BlockCopy(PhysPt dest,PhysPt src,Bitu size);
void MEM_StrCopy(PhysPt pt,char * data,Bitu size);



/* The folowing functions are all shortcuts to the above functions using physical addressing */

INLINE HostPt real_off(Bit16u seg,Bit32u off) {
	return memory+(seg<<4)+off;
};

INLINE HostPt real_host(Bit16u seg,Bit32u off) {
	return memory+(seg<<4)+off;
};
INLINE PhysPt real_phys(Bit16u seg,Bit32u off) {
	return (seg<<4)+off;
};

INLINE Bit8u real_readb(Bit16u seg,Bit16u off) {
	return mem_readb((seg<<4)+off);
}
INLINE Bit16u real_readw(Bit16u seg,Bit16u off) {
	return mem_readw((seg<<4)+off);
}
INLINE Bit32u real_readd(Bit16u seg,Bit16u off) {
	return mem_readd((seg<<4)+off);
}
//#define real_readb(seg,off) mem_readb(((seg)<<4)+(off))
//#define real_readw(seg,off) mem_readw(((seg)<<4)+(off))
//#define real_readd(seg,off) mem_readd(((seg)<<4)+(off))

INLINE void real_writeb(Bit16u seg,Bit16u off,Bit8u val) {
	mem_writeb(((seg<<4)+off),val);
}
INLINE void real_writew(Bit16u seg,Bit16u off,Bit16u val) {
	mem_writew(((seg<<4)+off),val);
}
INLINE void real_writed(Bit16u seg,Bit16u off,Bit32u val) {
	mem_writed(((seg<<4)+off),val);
}


//#define real_writeb(seg,off,val) mem_writeb((((seg)<<4)+(off)),val)
//#define real_writew(seg,off,val) mem_writew((((seg)<<4)+(off)),val)
//#define real_writed(seg,off,val) mem_writed((((seg)<<4)+(off)),val)

inline Bit32u real_getvec(Bit8u num) {
	return real_readd(0,(num<<2));
}
/*
inline void real_setvec(Bit8u num,Bit32u addr) {
	real_writed(0,(num<<2),addr);
};

*/

INLINE Bit16u RealSeg(RealPt pt) {
	return (Bit16u)(pt>>16);
}

INLINE Bit16u RealOff(RealPt pt) {
	return (Bit16u)(pt&0xffff);
}

INLINE PhysPt Real2Phys(RealPt pt) {
	return (RealSeg(pt)<<4) +RealOff(pt);
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

