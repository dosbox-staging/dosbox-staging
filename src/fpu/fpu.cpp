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
#if C_FPU

#include <math.h>
#include <float.h>
#include "mem.h"
#include "fpu.h"

typedef PhysPt EAPoint;

#define LoadMb(off) mem_readb(off)
#define LoadMw(off) mem_readw(off)
#define LoadMd(off) mem_readd(off)

#define SaveMb(off,val)	mem_writeb(off,val)
#define SaveMw(off,val)	mem_writew(off,val)
#define SaveMd(off,val)	mem_writed(off,val)

#include "fpu_types.h"

struct {
	FPU_Reg    regs[8];
	FPU_Tag    tags[8];
	Bitu       cw;
	FPU_Round  round;
	Bitu       ex_mask;
	Bitu       sw;
	Bitu       top;

} fpu;

INLINE void FPU_SetCW(Bitu word) {
	fpu.cw = word;
	fpu.round = (FPU_Round)((word >> 8) & 3);
	fpu.ex_mask = word & 0x3f;
}

#include "fpu_instructions.h"



void FPU_ESC0_EA(Bitu rm,PhysPt addr) {
}

void FPU_ESC0_Normal(Bitu rm) {
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	LOG(LOG_FPU,LOG_WARN)("ESC 0:Unhandled group %d subfunction %d",group,sub);
}


void FPU_ESC1_EA(Bitu rm,PhysPt addr) {

}

void FPU_ESC1_Normal(Bitu rm) {
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	LOG(LOG_FPU,LOG_WARN)("ESC 1:Unhandled group %d subfunction %d",group,sub);
}


void FPU_ESC2_EA(Bitu rm,PhysPt addr) {
}

void FPU_ESC2_Normal(Bitu rm) {
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	LOG(LOG_FPU,LOG_WARN)("ESC 2:Unhandled group %d subfunction %d",group,sub);
}


void FPU_ESC3_EA(Bitu rm,PhysPt addr) {
}

void FPU_ESC3_Normal(Bitu rm) {
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	switch (group) {
	case 0x04:
		switch (sub) {
		case 0x03:				//FINIT
			FPU_FINIT();
			break;
		default:
			LOG(LOG_FPU,LOG_WARN)("ESC 3:Unhandled group %d subfunction %d",group,sub);
		}
		break;
	default:
		LOG(LOG_FPU,LOG_WARN)("ESC 3:Unhandled group %d subfunction %d",group,sub);
		break;
	}
	return;
}


void FPU_ESC4_EA(Bitu rm,PhysPt addr) {
}

void FPU_ESC4_Normal(Bitu rm) {
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	LOG(LOG_FPU,LOG_WARN)("ESC 4:Unhandled group %d subfunction %d",group,sub);
}


void FPU_ESC5_EA(Bitu rm,PhysPt addr) {
}

void FPU_ESC5_Normal(Bitu rm) {
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	LOG(LOG_FPU,LOG_WARN)("ESC 5:Unhandled group %d subfunction %d",group,sub);
}


void FPU_ESC6_EA(Bitu rm,PhysPt addr) {
}

void FPU_ESC6_Normal(Bitu rm) {
}


void FPU_ESC7_EA(Bitu rm,PhysPt addr) {
}

void FPU_ESC7_Normal(Bitu rm) {
}


void FPU_Init(void) {
	FPU_FINIT();
}

#endif
