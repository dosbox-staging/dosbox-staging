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
#include "cpu.h"

typedef PhysPt EAPoint;

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
	fpu.round = (FPU_Round)((word >> 8) & 3);
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


#include "fpu_instructions.h"

/* TODO   : MAKE REGULAR TREE A DEFINE or a function 
          : ESC6normal => esc4normal+pop  or a define as well 
          : Make a smarter top system. won't matter that much in speed though.
          : #define ST fpu.top #define ST(i) (fpu.top+(i))&7  maybe*/

static void EATREE(Bitu _rm){
	Bitu group=(_rm >> 3) & 7;
	Bitu top = FPU_GET_TOP();
	/* data will allready be put in register 8 by caller */
	switch(group){
		case 0x00:	/* FIADD */
			FPU_FADD(top, 8);
			break;
		case 0x01:	/* FIMUL  */
			FPU_FMUL(top, 8);
			break;
		case 0x02:	/* FICOM */
		case 0x03:  /* FICOMP */
			LOG(LOG_FPU,LOG_WARN)("ESC EATREE:Unhandled group %d",group);
			break;
		case 0x04:	/* FISUB */
			FPU_FSUB(top,8);
			break;
		case 0x05:  /* FISUBR */
			FPU_FSUBR(top,8);
		case 0x06: /* FIDIV */
			FPU_FDIV(top, 8);
			break;
		case 0x07:  /* FIDIVR */
			FPU_FDIVR(top,8);
			break;
		default:
			break;
	}

}

void FPU_ESC0_EA(Bitu rm,PhysPt addr) {
	/* REGULAR TREE WITH 32 BITS REALS ? float ? */
	//THIS SHOULD GO ALLRIGHT !?!
	LOG(LOG_FPU, LOG_WARN)("ESC 0 EA used (check the result!)");
	
	union {
		float f;
		Bit32s l;
	}	blah;
    blah.l = mem_readd(addr);
	fpu.regs[8].d = static_cast<double>(blah.f);
	EATREE(rm);
}

void FPU_ESC0_Normal(Bitu rm) {
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	Bitu top = FPU_GET_TOP();
	switch (group){
	case 0x00:		/* FADD ST,STi */
		FPU_FADD(top,(top+sub)&7);
		break;
	case 0x01:		/* FMUL  ST,STi */
		FPU_FMUL(top,(top+sub)&7);
		break;
	case 0x02:		/* FCOM  STi */
	case 0x03:		/* FCOMP STi */
		LOG(LOG_FPU,LOG_WARN)("ESC 0:Unhandled group %d subfunction %d",group,sub);
		break;
	case 0x04:		/* FSUB  ST,STi */
		FPU_FSUB(top,(top+sub)&7);
		break;	
	case 0x05:		/* FSUBR ST,STi */
		FPU_FSUBR(top,(top+sub)&7);
		break;
	case 0x06:		/* FDIV  ST,STi */
		FPU_FDIV(top,(top+sub)&7);
		break;
	case 0x07:		/* FDIVR ST,STi */
		FPU_FDIVR(top,(top+sub)&7);
		break;
	default:
		break;

	}
}

void FPU_ESC1_EA(Bitu rm,PhysPt addr) {
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	LOG(LOG_FPU,LOG_WARN)("ESC EA 1:Unhandled group %d subfunction %d",group,sub);
}

void FPU_ESC1_Normal(Bitu rm) {
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	switch (group){
	case 0x00: /* FLD STi */
		{
			Bitu top = FPU_GET_TOP();
			FPU_PUSH(fpu.regs[(top+sub)&7].d);
		}
		break;
	case 0x04:
		switch(sub){
		case 0x00:       /* FCHS */
		case 0x01:       /* FABS */
		case 0x02:       /* UNKNOWN */
		case 0x03:       /* ILLEGAL */
		case 0x04:       /* FTST */
		case 0x05:       /* FXAM */
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
		case 0x02:       /* FLDL2E */
			LOG(LOG_FPU,LOG_WARN)("ESC 1:Unhandled group %X subfunction %X",group,sub);
			break;
		case 0x03:       /* FLDPI */
			FPU_PUSH(PI);
			break;
		case 0x04:       /* FLDLG2 */
		case 0x05:       /* FLDLN2 */
			LOG(LOG_FPU,LOG_WARN)("ESC 1:Unhandled group %X subfunction %X",group,sub);
		case 0x06:       /* FLDZ*/
			FPU_PUSH_ZERO();
			break;
		case 0x07:       /* ILLEGAL */
			LOG(LOG_FPU,LOG_WARN)("ESC 1:Unhandled group %X subfunction %X",group,sub);
			break;
		}
		break;
	case 0x06:
		LOG(LOG_FPU,LOG_WARN)("ESC 1:Unhandled group %X subfunction %X",group,sub);
		break;
	case 0x07:
		switch(sub){


		case 0x03:		/* FSINCOS */
			FPU_FSINCOS();
			break;
		case 0x06:		/* FSIN */
			FPU_FSIN();
			break;
		case 0x07:		/* FCOS */
			FPU_FCOS();
			break;
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
	fpu.regs[8].d = static_cast<double>(blah);
	EATREE(rm);
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
	/* REGULAR TREE WITH 64 BITS REALS ? double ? */
	E_Exit("how to load a double in esc 4 ea");
	//SEE ESC 0 EA 
//	double blah = mem_readd(addr); //wrong wrong wrong
//	fpu.regs[8].d = static_cast<double>(blah);
	EATREE(rm);
}

void FPU_ESC4_Normal(Bitu rm) {
	//LOOKS LIKE number 6 without popping*/
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	Bitu top = FPU_GET_TOP();
	switch(group){
	case 0x00:	/*FADDP STi,ST*/
		FPU_FADD((top+sub)&7,top);
		break;
	case 0x01:	/* FMULP STi,ST*/
		FPU_FMUL((top+sub)&7,top);
		break;
	case 0x02:  /* FCOMP5*/
	case 0x03:  /* weird*/
		LOG(LOG_FPU,LOG_WARN)("ESC 6:Unhandled group %d subfunction %d",group,sub);
		break;
	case 0x04:  /* FSUBRP STi,ST*/
		FPU_FSUBR((top+sub)&7,top);
		break;
	case 0x05:  /* FSUBP  STi,ST*/
		FPU_FSUB((top+sub)&7,top);
		break;
	case 0x06:	/* FDIVRP STi,ST*/
		FPU_FDIVR((top+sub)&7,top);
		break;
	case 0x07:  /* FDIVP STi,ST*/
		FPU_FDIV((top+sub)&7,top);
		break;
	default:
		break;
	}
}


void FPU_ESC5_EA(Bitu rm,PhysPt addr) {
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	LOG(LOG_FPU,LOG_WARN)("ESC EA 5:Unhandled group %d subfunction %d",group,sub);
}

void FPU_ESC5_Normal(Bitu rm) {
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	LOG(LOG_FPU,LOG_WARN)("ESC 5:Unhandled group %d subfunction %d",group,sub);
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
	Bitu top = FPU_GET_TOP();
	switch(group){
	case 0x00:	/*FADDP STi,ST*/
		FPU_FADD((top+sub)&7,top);
		break;
	case 0x01:	/* FMULP STi,ST*/
		FPU_FMUL((top+sub)&7,top);
		break;
	case 0x02:  /* FCOMP5*/
	case 0x03:  /* weird*/
		LOG(LOG_FPU,LOG_WARN)("ESC 6:Unhandled group %d subfunction %d",group,sub);
		// maybe return and don't pop
		break;
	case 0x04:  /* FSUBRP STi,ST*/
		FPU_FSUBR((top+sub)&7,top);
		break;
	case 0x05:  /* FSUBP  STi,ST*/
		FPU_FSUB((top+sub)&7,top);
		break;
	case 0x06:	/* FDIVRP STi,ST*/
		FPU_FDIVR((top+sub)&7,top);
		break;
	case 0x07:  /* FDIVP STi,ST*/
		FPU_FDIV((top+sub)&7,top);
		break;
	default:
		break;
	}
	FPU_FPOP();		
}


void FPU_ESC7_EA(Bitu rm,PhysPt addr) {
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	switch(group){
	case 0x03:	/*FISTP */
		{	Bitu top = FPU_GET_TOP();
			mem_writew(addr,static_cast<Bit16s>(fpu.regs[top].d));	
			FPU_FPOP();
		}
		break;
	default:
		LOG(LOG_FPU,LOG_WARN)("ESC 7:Unhandled group %d subfunction %d",group,sub);
	}
}

void FPU_ESC7_Normal(Bitu rm) {
	Bitu group=(rm >> 3) & 7;
	Bitu sub=(rm & 7);
	switch (group){
	case 0x04:
		switch(sub){
			case 0x00:     /* FNSTSW AX*/
				reg_ax = fpu.sw;
				break;
			default:
				break;
		}
		break;
	default:
		break;
	}
	LOG(LOG_FPU,LOG_WARN)("ESC 7:Unhandled group %d subfunction %d",group,sub);
}


void FPU_Init(Section*) {
	FPU_FINIT();
}

#endif
