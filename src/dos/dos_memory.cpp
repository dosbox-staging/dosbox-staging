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

#include "dosbox.h"
#include "mem.h"
#include "dos_inc.h"


#define MEM_START 0x60					//First Segment that DOS can use
//#define MEM_START 4000					//First Segment that DOS can use

static Bit16u memAllocStrategy = 0x00;

static void DOS_CompressMemory(void) {
	Bit16u mcb_segment=dos.firstMCB;
	DOS_MCB mcb(mcb_segment);
	DOS_MCB mcb_next(0);

	while (mcb.GetType()!=0x5a) {
		mcb_next.SetPt((Bit16u)(mcb_segment+mcb.GetSize()+1));
		if ((mcb.GetPSPSeg()==0) && (mcb_next.GetPSPSeg()==0)) {
			mcb.SetSize(mcb.GetSize()+mcb_next.GetSize()+1);
			mcb.SetType(mcb_next.GetType());
		} else {
			mcb_segment+=mcb.GetSize()+1;
			mcb.SetPt(mcb_segment);
		}
	}
}

void DOS_FreeProcessMemory(Bit16u pspseg) {
	Bit16u mcb_segment=dos.firstMCB;
	DOS_MCB mcb(mcb_segment);
	while (true) {
		if (mcb.GetPSPSeg()==pspseg) {
			mcb.SetPSPSeg(MCB_FREE);
		}
		if (mcb.GetType()==0x5a) break;
		mcb_segment+=mcb.GetSize()+1;
		mcb.SetPt(mcb_segment);
	}
	DOS_CompressMemory();
};

Bit16u DOS_GetMemAllocStrategy()
{
	return memAllocStrategy;
};

void DOS_SetMemAllocStrategy(Bit16u strat)
{
	memAllocStrategy = strat;
};

bool DOS_AllocateMemory(Bit16u * segment,Bit16u * blocks) {
	DOS_CompressMemory();
	Bit16u bigsize=0;Bit16u mcb_segment=dos.firstMCB;
	DOS_MCB mcb(0);
	DOS_MCB mcb_next(0);
	DOS_MCB psp_mcb(dos.psp()-1);
	char psp_name[9];
	psp_mcb.GetFileName(psp_name);
	bool stop=false;
	while(!stop) {
		mcb.SetPt(mcb_segment);
		if (mcb.GetPSPSeg()==0) {
			/* Check for enough free memory in current block */
			Bit16u block_size=mcb.GetSize();			
			if (block_size<(*blocks)) {
				if (bigsize<block_size) {
					bigsize=block_size;
				}
			} else if (block_size==*blocks) {
				mcb.SetPSPSeg(dos.psp());
				*segment=mcb_segment+1;
				return true;
			} else {
				// TODO: Strategy "1": Best matching block
				/* If so allocate it */
				if ((memAllocStrategy & 0x03)==0) {	
					mcb_next.SetPt((Bit16u)(mcb_segment+*blocks+1));
					mcb_next.SetPSPSeg(MCB_FREE);
					mcb_next.SetType(mcb.GetType());
					mcb_next.SetSize(block_size-*blocks-1);
					mcb.SetSize(*blocks);
					mcb.SetType(0x4d);		
					mcb.SetPSPSeg(dos.psp());
					mcb.SetFileName(psp_name);
					//TODO Filename
					*segment=mcb_segment+1;
					return true;
				} else {
					// * Last Block *
					// New created block
					*segment = mcb_segment+1+block_size - *blocks;
					mcb_next.SetPt((Bit16u)(*segment-1));
					mcb_next.SetSize(*blocks);
					mcb_next.SetType(mcb.GetType());
					mcb_next.SetPSPSeg(dos.psp());
					mcb_next.SetFileName(psp_name);
					// Old Block
					mcb.SetSize(block_size-*blocks-1);
					mcb.SetPSPSeg(MCB_FREE);
					mcb.SetType(0x4D);
					return true;
				};
			}
		}
		/* Onward to the next MCB if there is one */
		if (mcb.GetType()==0x5a) {
			*blocks=bigsize;
			DOS_SetError(DOSERR_INSUFFICIENT_MEMORY);
			return false;
		}
		mcb_segment+=mcb.GetSize()+1;
	}
	return false;
}


bool DOS_ResizeMemory(Bit16u segment,Bit16u * blocks) {
	DOS_CompressMemory();
	DOS_MCB mcb(segment-1);
	Bit16u total=mcb.GetSize();
	DOS_MCB	mcb_next(segment+total);
	if (mcb.GetType()!=0x5a) {
		if (mcb_next.GetPSPSeg()==MCB_FREE) {
			total+=mcb_next.GetSize()+1;
		}
	}
	if (*blocks<total) {
		if (mcb.GetType()!=0x5a) {
			mcb.SetType(mcb_next.GetType());
		}
		mcb.SetSize(*blocks);
		mcb_next.SetPt((Bit16u)(segment+*blocks));
		mcb_next.SetSize(total-*blocks-1);
		mcb_next.SetType(mcb.GetType());
		mcb_next.SetPSPSeg(MCB_FREE);
		mcb.SetType(0x4d);
		return true;
	}
	if (*blocks==total) {
		if (mcb.GetType()!=0x5a) {
			mcb.SetType(mcb_next.GetType());
		}
		mcb.SetSize(*blocks);
		return true;
	}
	*blocks=total;
	DOS_SetError(DOSERR_INSUFFICIENT_MEMORY);
	return false;
}


bool DOS_FreeMemory(Bit16u segment) {
//TODO Check if allowed to free this segment
	if ((segment-1) < MEM_START){
		LOG(LOG_DOSMISC,LOG_ERROR)("Program tried to free %X ---ERROR",segment);
		return false;
	}
      
	DOS_MCB mcb(segment-1);
	mcb.SetPSPSeg(MCB_FREE);
	DOS_CompressMemory();
	return true;
}




void DOS_SetupMemory(void) {
	// Create a dummy device MCB with PSPSeg=0x0008
	DOS_MCB mcb_devicedummy((Bit16u)MEM_START);
	mcb_devicedummy.SetPSPSeg(0x0008);				// Devices
	mcb_devicedummy.SetSize(1);
	mcb_devicedummy.SetType(0x4d);					// More blocks will follow
	
	DOS_MCB mcb((Bit16u)MEM_START+2);
	mcb.SetPSPSeg(MCB_FREE);						//Free
	if (machine==MCH_TANDY) {
		mcb.SetSize(0x97FE - MEM_START - 2);
	} else mcb.SetSize(0x9FFE - MEM_START - 2);
	mcb.SetType(0x5a);								//Last Block

	dos.firstMCB=MEM_START;
	dos_infoblock.SetFirstMCB(MEM_START);
}
