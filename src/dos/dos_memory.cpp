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

#include "dosbox.h"
#include "mem.h"
#include "dos_inc.h"


#define MEM_START 0x60					//First Segment that DOS can use
//#define MEM_START 4000					//First Segment that DOS can use


static void DOS_CompressMemory(void) {
	MCB * pmcb;MCB * pmcbnext;
	Bit16u mcb_segment;
	mcb_segment=dos.firstMCB;
	pmcb=(MCB *)HostMake(mcb_segment,0);
	while (pmcb->type!=0x5a) {
		pmcbnext=pmcbnext=(MCB *)HostMake(mcb_segment+pmcb->size+1,0);
		if ((pmcb->psp_segment==0) && (pmcbnext->psp_segment==0)) {
			pmcb->size+=pmcbnext->size+1;
			pmcb->type=pmcbnext->type;
		} else {
			mcb_segment+=pmcb->size+1;
			pmcb=(MCB *)HostMake(mcb_segment,0);
		}
	}
}

void DOS_FreeProcessMemory(Bit16u pspseg) {
	MCB * pmcb;
	Bit16u mcb_segment=dos.firstMCB;
	pmcb=(MCB *)HostMake(mcb_segment,0);
	while (true) {
		if (pmcb->psp_segment==pspseg) {
			pmcb->psp_segment=MCB_FREE;
		}
		mcb_segment+=pmcb->size+1;
		if (pmcb->type==0x5a) break;
		pmcb=(MCB *)HostMake(mcb_segment,0);
	}
	DOS_CompressMemory();
};


bool DOS_AllocateMemory(Bit16u * segment,Bit16u * blocks) {
	MCB * pmcb;MCB * pmcbnext;
	Bit16u bigsize=0;Bit16u mcb_segment;
	bool stop=false;mcb_segment=dos.firstMCB;
	DOS_CompressMemory();
	while(!stop) {
		pmcb=(MCB *)HostMake(mcb_segment,0);
		if (pmcb->psp_segment==0) {
			/* Check for enough free memory in current block */
			if (pmcb->size<(*blocks)) {
				if (bigsize<pmcb->size) {
					bigsize=pmcb->size;
				}
				
			} else if (pmcb->size==*blocks) {
				pmcb->psp_segment=dos.psp;
				*segment=mcb_segment+1;
				return true;
			} else {
				/* If so allocate it */
				pmcbnext=(MCB *)HostMake(mcb_segment+*blocks+1,0);
				pmcbnext->psp_segment=MCB_FREE;
				pmcbnext->type=pmcb->type;
				pmcbnext->size=pmcb->size-*blocks-1;
				pmcb->size=*blocks;
				pmcb->type=0x4D;
				pmcb->psp_segment=dos.psp;
				//TODO Filename
				*segment=mcb_segment+1;
				return true;
			}
		}
		/* Onward to the next MCB if there is one */
		if (pmcb->type==0x5a) {
			*blocks=bigsize;
			DOS_SetError(DOSERR_INSUFFICIENT_MEMORY);
			return false;
		}
		mcb_segment+=pmcb->size+1;
	}
	return false;
}


bool DOS_ResizeMemory(Bit16u segment,Bit16u * blocks) {
	DOS_CompressMemory();
	MCB * pmcb,* pmcbnext,* pmcbnew;
	pmcb=(MCB *)HostMake(segment-1,0);
	pmcbnext=(MCB *)HostMake(segment+pmcb->size,0);
	Bit16u total=pmcb->size;
	if (pmcb->type!=0x5a) {
		if (pmcbnext->psp_segment==MCB_FREE) {
			total+=pmcbnext->size+1;
		}
	};
	if (*blocks<total) {
		if (pmcb->type!=0x5a) {
			pmcb->type=pmcbnext->type;
		}
		pmcb->size=*blocks;
		pmcbnew=(MCB *)HostMake(segment+*blocks,0);
		pmcbnew->size=total-*blocks-1;
		pmcbnew->type=pmcb->type;
		pmcbnew->psp_segment=MCB_FREE;
		pmcb->type=0x4D;
		return true;
	}
	if (*blocks==total) {
		if (pmcb->type!=0x5a) {
			pmcb->type=pmcbnext->type;
		}
		pmcb->size=*blocks;
		return true;
	}
	*blocks=total;
	DOS_SetError(DOSERR_INSUFFICIENT_MEMORY);
	return false;
}


bool DOS_FreeMemory(Bit16u segment) {
//TODO Check if allowed to free this segment
	MCB * pmcb;
	pmcb=(MCB *)HostMake(segment-1,0);
	pmcb->psp_segment=MCB_FREE;
	DOS_CompressMemory();
	return true;
}




void DOS_SetupMemory(void) {
	MCB * mcb=(MCB *) HostMake(MEM_START,0);
	mcb->psp_segment=MCB_FREE;						//Free
	mcb->size=0x9FFE - MEM_START;
	mcb->type=0x5a;									//Last Block
	dos.firstMCB=MEM_START;
	dos_infoblock.SetFirstMCB(RealMake(MEM_START,0));
}

