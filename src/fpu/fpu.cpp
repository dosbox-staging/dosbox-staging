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

#include <math.h>
#include <float.h>
#include "mem.h"
#include "dosbox.h"
#include "fpu.h"

typedef PhysPt EAPoint;

#define LoadMb(off) mem_readb(off)
#define LoadMw(off) mem_readw(off)
#define LoadMd(off) mem_readd(off)

#define LoadMbs(off) (Bit8s)(LoadMb(off))
#define LoadMws(off) (Bit16s)(LoadMw(off))
#define LoadMds(off) (Bit32s)(LoadMd(off))

#define SaveMb(off,val)	mem_writeb(off,val)
#define SaveMw(off,val)	mem_writew(off,val)
#define SaveMd(off,val)	mem_writed(off,val)

FPU_Flag_Info fpu_flags;
FPU_Regs fpu_regs;

#define FPU_GetZF	fpu_flags.sw.zf = FPU_get_ZF();

#include "fpu_core_16/instructions.h"

#define FPU_ParseCW(newcw) { \
	fpu_flags.cw.ic = ((bool)((newcw&0x1000)>>12)?true:false); \
	fpu_flags.cw.rc = (Bit8u)((newcw&0x0C00)>>10); \
	fpu_flags.cw.pc = (Bit8u)((newcw&0x0300)>>8); \
	fpu_flags.cw.ie = ((bool)((newcw&0x0080)>>7)?true:false); \
	fpu_flags.cw.sf = ((bool)((newcw&0x0040)>>6)?true:false); \
	fpu_flags.cw.pf = ((bool)((newcw&0x0020)>>5)?true:false); \
	fpu_flags.cw.uf = ((bool)((newcw&0x0010)>>4)?true:false); \
	fpu_flags.cw.of = ((bool)((newcw&0x0008)>>3)?true:false); \
	fpu_flags.cw.zf = ((bool)((newcw&0x0004)>>2)?true:false); \
	fpu_flags.cw.df = ((bool)((newcw&0x0002)>>1)?true:false); \
	fpu_flags.cw.in = ((bool)(newcw&0x0001)?true:false); \
}

#define FPU_makeCW(newcw) { \
	newcw = (Bit16u)fpu_flags.cw.in; \
	newcw |= (fpu_flags.cw.df<<1); \
	newcw |= (fpu_flags.cw.zf<<2); \
	newcw |= (fpu_flags.cw.of<<3); \
	newcw |= (fpu_flags.cw.uf<<4); \
	newcw |= (fpu_flags.cw.pf<<5); \
	newcw |= (fpu_flags.cw.sf<<6); \
	newcw |= (fpu_flags.cw.ie<<7); \
	newcw |= (fpu_flags.cw.pc<<8); \
	newcw |= (fpu_flags.cw.rc<<10); \
	newcw |= (fpu_flags.cw.ic<<12); \
}

#define FPU_ParseSW(newsw) { \
	fpu_flags.sw.bf = ((bool)((newsw&0x8000)>>15)?true:false); \
	fpu_flags.sw.c3 = ((bool)((newsw&0x4000)>>14)?true:false); \
	fpu_flags.sw.tos = (Bit8s)((newsw&0x3800)>>11); \
	fpu_flags.sw.c2 = ((bool)((newsw&0x0400)>>10)?true:false); \
	fpu_flags.sw.c1 = ((bool)((newsw&0x0200)>>9)?true:false); \
	fpu_flags.sw.c0 = ((bool)((newsw&0x0100)>>8)?true:false); \
	fpu_flags.sw.ir = ((bool)((newsw&0x0080)>>7)?true:false); \
	fpu_flags.sw.sf = ((bool)((newsw&0x0040)>>6)?true:false); \
	fpu_flags.sw.pf = ((bool)((newsw&0x0020)>>5)?true:false); \
	fpu_flags.sw.uf = ((bool)((newsw&0x0010)>>4)?true:false); \
	fpu_flags.sw.of = ((bool)((newsw&0x0008)>>3)?true:false); \
	fpu_flags.sw.zf = ((bool)((newsw&0x0004)>>2)?true:false); \
	fpu_flags.sw.df = ((bool)((newsw&0x0002)>>1)?true:false); \
	fpu_flags.sw.in = ((bool)(newsw&0x0001)?true:false); \
}

#define FPU_makeSW(newsw) { \
	newsw = (Bit16u)fpu_flags.sw.in; \
	newsw |= (fpu_flags.sw.df<<1); \
	newsw |= (fpu_flags.sw.zf<<2); \
	newsw |= (fpu_flags.sw.of<<3); \
	newsw |= (fpu_flags.sw.uf<<4); \
	newsw |= (fpu_flags.sw.pf<<5); \
	newsw |= (fpu_flags.sw.sf<<6); \
	newsw |= (fpu_flags.sw.ir<<7); \
	newsw |= (fpu_flags.sw.c0<<8); \
	newsw |= (fpu_flags.sw.c1<<9); \
	newsw |= (fpu_flags.sw.c2<<10); \
	newsw |= (fpu_flags.sw.tos<<11); \
	newsw |= (fpu_flags.sw.c3<<14); \
	newsw |= (fpu_flags.sw.bf<<15); \
}

#define FPU_LOADFLAGS { \
	fpu_flags.sw.bf = false; \
	fpu_flags.sw.c3 = FPU_get_C3(); \
	fpu_flags.sw.c2 = FPU_get_C2(); \
	fpu_flags.sw.c1 = FPU_get_C1(); \
	fpu_flags.sw.c0 = FPU_get_C0(); \
	fpu_flags.sw.ir = FPU_get_IR(); \
	fpu_flags.sw.sf = FPU_get_SF(); \
	fpu_flags.sw.pf = FPU_get_PF(); \
	fpu_flags.sw.uf = FPU_get_UF(); \
	fpu_flags.sw.of = FPU_get_OF(); \
	fpu_flags.sw.zf = FPU_get_ZF(); \
	fpu_flags.sw.df = FPU_get_DF(); \
	fpu_flags.sw.in = FPU_get_IN(); \
}

void FPU_ESC0_EA(Bitu rm,PhysPt addr) {
}

void FPU_ESC0_Normal(Bitu rm) {
}


void FPU_ESC1_EA(Bitu rm,PhysPt addr) {
	Bit16u cw;
	Bitu opcode = (rm&0x38)>>3;

	switch(opcode) {
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
		case 6:
			break;
		case 5:
			FPU_ParseCW(LoadMw(addr));							/* FLDCW */
			break;
		case 7:													/* FSTCW */
			FPU_makeCW(cw);	
			SaveMw(addr,cw);
			break;
	}
}

void FPU_ESC1_Normal(Bitu rm) {
	Bitu opcode = (rm&0xF0);

	switch(opcode) {
		case 0xC0:
//			if(rm&8)
//			else
				FLDST(rm-0xC0);									/* FLDST */
			break;
		case 0xD0:
			break;
	}
	switch(rm) {
		case 0xE0:												/* FCHS */
			FCHS;
			break;
		case 0xE8:												/* FLD1 */
			FLD(1);
			break;
		case 0xEE:												/* FLDZ */
			FLD(0);
			break;
	}
}


void FPU_ESC2_EA(Bitu rm,PhysPt addr) {
}

void FPU_ESC2_Normal(Bitu rm) {
}


void FPU_ESC3_EA(Bitu rm,PhysPt addr) {
}

void FPU_ESC3_Normal(Bitu rm) {
	switch( rm ) {
		case 0xE3:												/* FINIT */
			FPU_ParseCW(0x037F);
			for(int i=0;i<8;i++) {
				fpu_regs.st[i].r = 0;
				fpu_regs.st[i].tag = FPUREG_EMPTY;
			}
			break;
	}
}


void FPU_ESC4_EA(Bitu rm,PhysPt addr) {
}

void FPU_ESC4_Normal(Bitu rm) {
}


void FPU_ESC5_EA(Bitu rm,PhysPt addr) {
	Bit16u sw;
	Bitu opcode = (rm&0x38)>>3;

	switch(opcode) {
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
		case 7:													/* FSTSW */
			FPU_LOADFLAGS;
			FPU_makeSW(sw);	
			SaveMw(addr,sw);
			break;
	}
}

void FPU_ESC5_Normal(Bitu rm) {
}


void FPU_ESC6_EA(Bitu rm,PhysPt addr) {
}

void FPU_ESC6_Normal(Bitu rm) {
	Bitu opcode = (rm&0xF0);

	if(rm==0xD9) {												/* FCOMPP */
		FCOMPP;
		return;
	}
	switch(opcode) {
		case 0xC0:
//			if(rm&8)
			break;
		case 0xD0:
//			if(rm&8)
			break;
		case 0xE0:
//			if(rm&8)
			break;
		case 0xF0:
			if(rm&8)
				FDIVP(rm-0xF8,0);
			break;
	}
}


void FPU_ESC7_EA(Bitu rm,PhysPt addr) {
}

void FPU_ESC7_Normal(Bitu rm) {
}

void FPU_Init(void) {
	fpu_flags.type = t_FUNKNOWN;
}


