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

/* $Id: fpu.cpp,v 1.16 2004-01-10 14:03:34 qbix79 Exp $ */

#include "dosbox.h"
#if C_FPU

#include <math.h>
#include <float.h>
#include "cross.h"
#include "mem.h"
#include "fpu.h"
#include "cpu.h"

typedef PhysPt EAPoint;

#define TOP fpu.top
#define ST(i)  ( (fpu.top+ (i) ) & 7 )

#define LoadMb(off) mem_readb(off)
#define LoadMw(off) mem_readw(off)
#define LoadMd(off) mem_readd(off)

#define SaveMb(off,val)	mem_writeb(off,val)
#define SaveMw(off,val)	mem_writew(off,val)
#define SaveMd(off,val)	mem_writed(off,val)

#include "fpu_types.h"

struct {
	FPU_Reg    regs[9];
	FPU_Tag    tags[9];
	Bitu       cw;
	FPU_Round  round;
	Bitu       ex_mask;
	Bitu       sw;
	Bitu       top;

} fpu;

INLINE void FPU_SetCW(Bitu word) {
	fpu.cw = word;
	fpu.round = (FPU_Round)((word >> 10) & 3);
	// word >>8 &3 is precission
	fpu.ex_mask = word & 0x3f;
}
	


INLINE Bitu FPU_GET_TOP(void){
	return (fpu.sw & 0x3800)>>11;
}
INLINE void FPU_SET_TOP(Bitu val){
	fpu.sw &= ~0x3800;
	fpu.sw |= (val&7)<<11;
	return;
}

INLINE void FPU_SET_C0(Bitu C){
	fpu.sw &= ~0x0100;
	if(C) fpu.sw |=  0x0100;
}
INLINE void FPU_SET_C1(Bitu C){
	fpu.sw &= ~0x0200;
	if(C) fpu.sw |=  0x0200;
}
INLINE void FPU_SET_C2(Bitu C){
	fpu.sw &= ~0x0400;
	if(C) fpu.sw |=  0x0400;
}
INLINE void FPU_SET_C3(Bitu C){
	fpu.sw &= ~0x4000;
	if(C) fpu.sw |= 0x4000;
}

INLINE Bitu FPU_GET_C0(void){
	return (fpu.sw & 0x0100)>>8;
}
INLINE Bitu FPU_GET_C1(void){
	return (fpu.sw & 0x0200)>>9;
}
INLINE Bitu FPU_GET_C2(void){
	return (fpu.sw & 0x0400)>>10;
}
INLINE Bitu FPU_GET_C3(void){
	return (fpu.sw & 0x4000)>>14;
}

#include "fpu_instructions.h"

/* TODO   : ESC6normal => esc4normal+pop  or a define as well 
*/

/* WATCHIT : ALWAYS UPDATE REGISTERS BEFORE AND AFTER USING THEM 
			STATUS WORD =>	FPU_SET_TOP(TOP) BEFORE a read
							TOP=FPU_GET_TOP() after a write;
			*/
static void EATREE(Bitu _rm){
	Bitu group=(_rm >> 3) & 7;
	/* data will allready be put in register 8 by caller */
	switch(group){
		case 0x00:	/* FIADD */
			FPU_FADD(TOP, 8);
			break;
		case 0x01:	/* FIMUL  */
			FPU_FMUL(TOP, 8);
			break;
		case 0x02:	/* FICOM */
			FPU_FCOM(TOP,8);
			break;
		case 0x03:  /* FICOMP */
			FPU_FCOM(TOP,8);
			FPU_FPOP();
			break;
		case 0x04:	/* FISUB */
			FPU_FSUB(TOP,8);
			break;
		case 0x05:  /* FISUBR */
			FPU_FSUBR(TOP,8);
			break;
		case 0x06: /* FIDIV */
			FPU_FDIV(TOP, 8);
			break;
		case 0x07:  /* FIDIVR */
			FPU_FDIVR(TOP,8);
			break;
		default:
			break;
	}

}

void FPU_ESC0_EA(Bitu rm,PhysPt addr) {
	/* REGULAR TREE WITH 32 BITS REALS -> float */
	union {
		float f;
		Bit32u l;
	}	blah;
    blah.l = mem_readd(addr);
	fpu.regs[8].d = static_cast<double>(blah.f);
	EATREE(rm);
}

void FPU_ESC0_Normal(Bitu rm) {
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	switch (group){
	case 0x00:		/* FADD ST,STi */
		FPU_FADD(TOP,ST(sub));
		break;
	case 0x01:		/* FMUL  ST,STi */
		FPU_FMUL(TOP,ST(sub));
		break;
	case 0x02:		/* FCOM  STi */
		FPU_FCOM(TOP,ST(sub));
		break;
	case 0x03:		/* FCOMP STi */
		FPU_FCOM(TOP,ST(sub));
		FPU_FPOP();
		break;
	case 0x04:		/* FSUB  ST,STi */
		FPU_FSUB(TOP,ST(sub));
		break;	
	case 0x05:		/* FSUBR ST,STi */
		FPU_FSUBR(TOP,ST(sub));
		break;
	case 0x06:		/* FDIV  ST,STi */
		FPU_FDIV(TOP,ST(sub));
		break;
	case 0x07:		/* FDIVR ST,STi */
		FPU_FDIVR(TOP,ST(sub));
		break;
	default:
		break;

	}
}

void FPU_ESC1_EA(Bitu rm,PhysPt addr) {
// floats
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	switch(group){
	case 0x00: /* FLD float*/
		{
		union {
			float f;
			Bit32u l;
		}	blah;
		blah.l = mem_readd(addr);
		FPU_PUSH(static_cast<double>(blah.f));
		}
		break;

	case 0x01: /* UNKNOWN */
		LOG(LOG_FPU,LOG_WARN)("ESC EA 1:Unhandled group %d subfunction %d",group,sub);
		break;
	case 0x02: /* FST float*/
		{
		union {
			float f;
			Bit32u l;
		}	blah;
		//should depend on rounding method
		blah.f = static_cast<float>(fpu.regs[TOP].d);
		mem_writed(addr,blah.l);
		}
		break;

	case 0x03: /* FSTP float*/
		{
		union {
			float f;
			Bit32u l;
		}	blah;
		blah.f = static_cast<float>(fpu.regs[TOP].d);
		mem_writed(addr,blah.l);
		}
		FPU_FPOP();
		break;
	case 0x05: /*FLDCW */
		{
			Bit16u temp =mem_readw(addr);
			FPU_SetCW(temp);
		}
		break;
	case 0x07:  /* FNSTCW*/
		mem_writew(addr,fpu.cw);
		break;
	default:
		LOG(LOG_FPU,LOG_WARN)("ESC EA 1:Unhandled group %d subfunction %d",group,sub);
		break;
	}

}

void FPU_ESC1_Normal(Bitu rm) {
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	switch (group){
	case 0x00: /* FLD STi */
			FPU_PUSH(fpu.regs[ST(sub)].d);
		break;
	case 0x01: /* FXCH STi */
			FPU_FXCH(TOP,ST(sub));
		break;
	case 0x02: /* FNOP */
			FPU_FNOP();
		break;
	case 0x03: /* FSTP STi */
			FPU_FST(TOP,ST(sub));
			FPU_FPOP();
		break;   
	case 0x04:
		switch(sub){
		case 0x00:       /* FCHS */
			fpu.regs[TOP].d = -1.0*(fpu.regs[TOP].d);
			break;
		case 0x01:       /* FABS */
			fpu.regs[TOP].d = fabs(fpu.regs[TOP].d);
			break;
		case 0x02:       /* UNKNOWN */
		case 0x03:       /* ILLEGAL */
			LOG(LOG_FPU,LOG_WARN)("ESC 1:Unhandled group %X subfunction %X",group,sub);
			break;
		case 0x04:       /* FTST */
			fpu.regs[8].d=0.0;
			FPU_FCOM(TOP,8);
			break;
		case 0x05:       /* FXAM */
			FPU_FXAM();
			break;
		case 0x06:       /* FTSTP (cyrix)*/
		case 0x07:       /* UNKNOWN */
			LOG(LOG_FPU,LOG_WARN)("ESC 1:Unhandled group %X subfunction %X",group,sub);
			break;
		}
		break;
	case 0x05:
		switch(sub){	
		case 0x00:       /* FLD1 */
			FPU_PUSH(1.0);
			break;
		case 0x01:       /* FLDL2T */
			FPU_PUSH(L2T);
			break;
		case 0x02:       /* FLDL2E */
			FPU_PUSH(L2E);
			break;
		case 0x03:       /* FLDPI */
			FPU_PUSH(PI);
			break;
		case 0x04:       /* FLDLG2 */
			FPU_PUSH(LG2);
			break;
		case 0x05:       /* FLDLN2 */
			FPU_PUSH(LN2);
			break;
		case 0x06:       /* FLDZ*/
			FPU_PUSH_ZERO();
			break;
		case 0x07:       /* ILLEGAL */
			LOG(LOG_FPU,LOG_WARN)("ESC 1:Unhandled group %X subfunction %X",group,sub);
			break;
		}
		break;
	case 0x06:
		switch(sub){
		case 0x00:   /* F2XM1 */
			FPU_F2XM1();
			break;
		case 0x01:	  /* FYL2X */
			FPU_FYL2X();
			break;
		case 0x02:	 /* FPTAN  */
			FPU_FPTAN();
			break;
		case 0x03:   /* FPATAN */
			FPU_FPATAN();
			break;
		default:
		LOG(LOG_FPU,LOG_WARN)("ESC 1:Unhandled group %X subfunction %X",group,sub);
		break;
		}
		break;
	case 0x07:
		switch(sub){
		case 0x00:		/* FPREM */
			FPU_FPREM();
			break;
		case 0x02:		/* FSQRT */
			FPU_FSQRT();
			break;
		case 0x03:		/* FSINCOS */
			FPU_FSINCOS();
			break;
		case 0x04:		/* FRNDINT */
			{
//TODO
				Bit64s temp= static_cast<Bit64s>(FROUND(fpu.regs[TOP].d));
				fpu.regs[TOP].d=static_cast<double>(temp);
			}			
			//TODO
			break;
		case 0x05:		/* FSCALE */
			FPU_FSCALE();
			break;
		case 0x06:		/* FSIN */
			FPU_FSIN();
			break;
		case 0x07:		/* FCOS */
			FPU_FCOS();
			break;
		case 0x01:      /* FYL2XP1 */
		default:
			LOG(LOG_FPU,LOG_WARN)("ESC 1:Unhandled group %X subfunction %X",group,sub);
			break;
		}
		break;
		default:
			LOG(LOG_FPU,LOG_WARN)("ESC 1:Unhandled group %X subfunction %X",group,sub);
	}

//	LOG(LOG_FPU,LOG_WARN)("ESC 1:Unhandled group %X subfunction %X",group,sub);
}


void FPU_ESC2_EA(Bitu rm,PhysPt addr) {
	/* 32 bits integer operants */
	Bit32s blah = mem_readd(addr);
	fpu.regs[8].d = static_cast<Real64>(blah);
	EATREE(rm);
}

void FPU_ESC2_Normal(Bitu rm) {
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	LOG(LOG_FPU,LOG_WARN)("ESC 2:Unhandled group %d subfunction %d",group,sub);
}


void FPU_ESC3_EA(Bitu rm,PhysPt addr) {
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	switch(group){
	case 0x00:  /* FLD */
		{
				Bit32s blah = mem_readd(addr);
				FPU_PUSH( static_cast<double>(blah));
		}
		break;
	case 0x01:  /* FISTTP */
		LOG(LOG_FPU,LOG_WARN)("ESC 3 EA:Unhandled group %d subfunction %d",group,sub);
		break;

	case 0x02:   /* FIST */
		mem_writed(addr,static_cast<Bit32s>(FROUND(fpu.regs[TOP].d)));
		break;
	case 0x03:	/*FISTP */
		mem_writed(addr,static_cast<Bit32s>(FROUND(fpu.regs[TOP].d)));	
		FPU_FPOP();
		break;
	case 0x05:	/* FLD 80 Bits Real */
		FPU_FLD80(addr);
		break;
	case 0x07:	/* FSTP 80 Bits Real */
		FPU_ST80(addr);
		FPU_FPOP();
		break;
	default:
		LOG(LOG_FPU,LOG_WARN)("ESC 3 EA:Unhandled group %d subfunction %d",group,sub);
	}
}

void FPU_ESC3_Normal(Bitu rm) {
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	switch (group) {
	case 0x04:
		switch (sub) {
		case 0x00:				//FNENI
		case 0x01:				//FNDIS
			LOG(LOG_FPU,LOG_ERROR)("8087 only fpu code used esc 3: group 4: subfuntion :%d",sub);
			break;
		case 0x02:				//FNCLEX FCLEX
			FPU_FCLEX();
			break;
		case 0x03:				//FNINIT FINIT
			FPU_FINIT();
			break;
		case 0x04:				//FNSETPM
		case 0x05:				//FRSTPM
			LOG(LOG_FPU,LOG_ERROR)("80267 protected mode (un)set. Nothing done");
			FPU_FNOP();
			break;
		default:
			E_Exit("ESC 3:ILLEGAL OPCODE group %d subfunction %d",group,sub);
		}
		break;
	default:
		LOG(LOG_FPU,LOG_WARN)("ESC 3:Unhandled group %d subfunction %d",group,sub);
		break;
	}
	return;
}


void FPU_ESC4_EA(Bitu rm,PhysPt addr) {
	/* REGULAR TREE WITH 64 BITS REALS: double  */
	fpu.regs[8].l.lower=mem_readd(addr);
	fpu.regs[8].l.upper=mem_readd(addr+4);
	EATREE(rm);
}

void FPU_ESC4_Normal(Bitu rm) {
	//LOOKS LIKE number 6 without popping*/
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	switch(group){
	case 0x00:	/*FADDP STi,ST*/
		FPU_FADD(ST(sub),TOP);
		break;
	case 0x01:	/* FMULP STi,ST*/
		FPU_FMUL(ST(sub),TOP);
		break;
	case 0x02:  /* FCOM*/
		FPU_FCOM(TOP,ST(sub));
		break;     /* TODO IS THIS ALLRIGHT ????????? (maybe reverse operators) */
	case 0x03:  /* FCOMP*/
		FPU_FCOM(TOP,ST(sub));
		FPU_FPOP();
		break;
	case 0x04:  /* FSUBR STi,ST*/
		FPU_FSUBR(ST(sub),TOP);
		break;
	case 0x05:  /* FSUB  STi,ST*/
		FPU_FSUB(ST(sub),TOP);
		break;
	case 0x06:  /* FDIVR STi,ST*/
		FPU_FDIVR(ST(sub),TOP);
		break;
	case 0x07:  /* FDIV STi,ST*/
		FPU_FDIV(ST(sub),TOP);
		break;
	default:
		break;
	}
}

void FPU_ESC5_EA(Bitu rm,PhysPt addr) {
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	switch(group){
	case 0x00:  /* FLD double real*/
		{
				FPU_Reg blah;
				blah.l.lower=mem_readd(addr);
				blah.l.upper=mem_readd(addr+4);
				FPU_PUSH(blah.d);
		}
		break;
	case 0x01:  /* FISTTP longint*/
		LOG(LOG_FPU,LOG_WARN)("ESC 5 EA:Unhandled group %d subfunction %d",group,sub);
		break;

	case 0x02:   /* FIST double real*/
		mem_writed(addr,fpu.regs[TOP].l.lower);
		mem_writed(addr+4,fpu.regs[TOP].l.upper);
		break;
	case 0x03:	/*FISTP double real*/
		mem_writed(addr,fpu.regs[TOP].l.lower);
		mem_writed(addr+4,fpu.regs[TOP].l.upper);
		FPU_FPOP();
		break;
	case 0x07:   /*FNSTSW    NG DISAGREES ON THIS*/
		FPU_SET_TOP(TOP);
		mem_writew(addr,fpu.sw);
		//seems to break all dos4gw games :)
		break;
	default:
		LOG(LOG_FPU,LOG_WARN)("ESC 5 EA:Unhandled group %d subfunction %d",group,sub);
	}
}

void FPU_ESC5_Normal(Bitu rm) {
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	switch(group){
	case 0x00: /* FFREE STi */
		fpu.tags[ST(sub)]=TAG_Empty;
		break;
	case 0x01: /* FXCH STi*/
		FPU_FXCH(TOP,ST(sub));
		break;
	case 0x02: /* FST STi */
		FPU_FST(TOP,ST(sub));
		break;
	case 0x03:  /* FSTP STi*/
		FPU_FST(TOP,ST(sub));
		FPU_FPOP();
		break;
	case 0x04:	/* FUCOM STi */
		FPU_FUCOM(TOP,ST(sub));
		break;
	case 0x05:	/*FUCOMP STi */
		FPU_FUCOM(TOP,ST(sub));
		FPU_FPOP();
		break;
	default:
	LOG(LOG_FPU,LOG_WARN)("ESC 5:Unhandled group %d subfunction %d",group,sub);
	break;
	}
}


void FPU_ESC6_EA(Bitu rm,PhysPt addr) {
	/* 16 bit (word integer) operants */
	Bit16s blah = mem_readw(addr);
	fpu.regs[8].d = static_cast<double>(blah);
	EATREE(rm);
}

void FPU_ESC6_Normal(Bitu rm) {
	/* all P variants working only on registers */
	/* get top before switch and pop afterwards */
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	switch(group){
	case 0x00:	/*FADDP STi,ST*/
		FPU_FADD(ST(sub),TOP);
		break;
	case 0x01:	/* FMULP STi,ST*/
		FPU_FMUL(ST(sub),TOP);
		break;
	case 0x02:  /* FCOMP5*/
		FPU_FCOM(TOP,ST(sub));
		break;     /* TODO IS THIS ALLRIGHT ????????? */
	case 0x03:  /* weird*/ /*FCOMPP*/
		if(sub != 1){
		LOG(LOG_FPU,LOG_WARN)("ESC 6:Unhandled group %d subfunction %d",group,sub);
		;
		break;
		}
		FPU_FCOM(TOP,ST(1));
		FPU_FPOP(); /* extra pop at the bottom*/
		break;
	case 0x04:  /* FSUBRP STi,ST*/
		FPU_FSUBR(ST(sub),TOP);
		break;
	case 0x05:  /* FSUBP  STi,ST*/
		FPU_FSUB(ST(sub),TOP);
		break;
	case 0x06:	/* FDIVRP STi,ST*/
		FPU_FDIVR(ST(sub),TOP);
		break;
	case 0x07:  /* FDIVP STi,ST*/
		FPU_FDIV(ST(sub),TOP);
		break;
	default:
		break;
	}
	FPU_FPOP();		
}


void FPU_ESC7_EA(Bitu rm,PhysPt addr) {
	/* ROUNDING*/
	
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	switch(group){
	case 0x00:  /* FILD Bit16s */
		{
				Bit16s blah = mem_readw(addr);
				FPU_PUSH( static_cast<double>(blah));
		}
		break;
	case 0x01:  /* FISTTP Bit16s */
		LOG(LOG_FPU,LOG_WARN)("ESC 7 EA:Unhandled group %d subfunction %d",group,sub);
		break;

	case 0x02:   /* FIST Bit16s */
		mem_writew(addr,static_cast<Bit16s>(FROUND(fpu.regs[TOP].d)));
		break;
	case 0x03:	/* FISTP Bit16s */
		mem_writew(addr,static_cast<Bit16s>(FROUND(fpu.regs[TOP].d)));	
		FPU_FPOP();
		break;
	case 0x05:  /* FILD Bit32s */
		{
				Bit32s blah = mem_readd(addr);
				FPU_PUSH( static_cast<double>(blah));
		}
		break;
	case 0x06:	/* FBSTP packed BCD */
		FPU_FBST(addr);
		FPU_FPOP();
		break;
	case 0x07:  /* FISTP Bit32s */
		mem_writed(addr,static_cast<Bit32s>(FROUND(fpu.regs[TOP].d)));	
		FPU_FPOP();
		break;
	case 0x04:   /* FBLD packed BCD */
		//Don't think anybody will ever use this.
	default:
		LOG(LOG_FPU,LOG_WARN)("ESC 7 EA:Unhandled group %d subfunction %d",group,sub);
		break;
	}
}

void FPU_ESC7_Normal(Bitu rm) {
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	switch (group){
	case 0x01: /* FXCH STi*/
			FPU_FXCH(TOP,ST(sub));
		break;
	case 0x02:  /* FSTP STi*/
	case 0x03:  /* FSTP STi*/
			FPU_FST(TOP,ST(sub));
			FPU_FPOP();
		break;
	case 0x04:
		switch(sub){
			case 0x00:     /* FNSTSW AX*/
				FPU_SET_TOP(TOP);
				reg_ax = fpu.sw;
				break;
			default:
			LOG(LOG_FPU,LOG_WARN)("ESC 7:Unhandled group %d subfunction %d",group,sub);
				break;
		}
		break;
	default:
		LOG(LOG_FPU,LOG_WARN)("ESC 7:Unhandled group %d subfunction %d",group,sub);
		break;
	}
	
}


void FPU_Init(Section*) {
	FPU_FINIT();
}

#endif
