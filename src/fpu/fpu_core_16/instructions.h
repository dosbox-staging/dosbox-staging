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

#define FLD(op1) {												\
	FPU_GetZF; \
	fpu_flags.type=t_FLD;									\
	if(--fpu_flags.sw.tos < 0)									\
		fpu_flags.sw.tos = 7;									\
	if( fpu_regs.st[fpu_flags.sw.tos].tag != FPUREG_EMPTY ) {	\
		fpu_flags.result.tag = fpu_regs.st[fpu_flags.sw.tos].tag = FPUREG_NNAN;	\
		break;													\
	}															\
	if(op1)														\
		fpu_flags.result.tag = fpu_regs.st[fpu_flags.sw.tos].tag = FPUREG_VALID;		\
	else														\
		fpu_flags.result.tag = fpu_regs.st[fpu_flags.sw.tos].tag = FPUREG_ZERO;		\
	fpu_flags.result.r = fpu_regs.st[fpu_flags.sw.tos].r = op1;						\
}

#define FLDST(op1) { \
	FPU_GetZF; \
	fpu_flags.type=t_FLDST;									\
	Bit8u reg = fpu_flags.sw.tos+op1; \
	if(reg>7) reg-=8; \
	if(--fpu_flags.sw.tos < 0)									\
		fpu_flags.sw.tos = 7;									\
	if(fpu_regs.st[fpu_flags.sw.tos].tag!=FPUREG_EMPTY) { \
		fpu_flags.result.tag = fpu_regs.st[fpu_flags.sw.tos].tag = FPUREG_NNAN;	\
		break;													\
	} \
	fpu_flags.result.tag = fpu_regs.st[fpu_flags.sw.tos].tag = fpu_regs.st[reg].tag; \
	fpu_flags.result.r = fpu_regs.st[fpu_flags.sw.tos].r = fpu_regs.st[reg].r; \
}

#define FPOP { \
	fpu_regs.st[fpu_flags.sw.tos].tag = FPUREG_EMPTY; \
	if(++fpu_flags.sw.tos > 7 ) \
		fpu_flags.sw.tos = 0; \
}
/* FPOP:	fpu_flags.result.r = fpu_regs.st[fpu_flags.sw.tos].r = 0;  is not really neccessary */

#define FDIVP(op1,op2) { \
	Bit8u reg1 = fpu_flags.sw.tos+op1; \
	Bit8u reg2 = fpu_flags.sw.tos+op2; \
	fpu_flags.type=t_FDIVP;									\
	if(reg1>7) reg1-=8; \
	if(reg2>7) reg2-=8; \
	if((fpu_regs.st[reg1].tag!=FPUREG_VALID && fpu_regs.st[reg1].tag!=FPUREG_ZERO)||(fpu_regs.st[reg2].tag!=FPUREG_VALID && fpu_regs.st[reg2].tag!=FPUREG_ZERO)) { \
		fpu_flags.result.tag = fpu_regs.st[reg1].tag = FPUREG_NNAN; \
		FPOP; \
		break; \
	} \
	if(fpu_regs.st[reg2].tag == FPUREG_ZERO) { \
		if(fpu_regs.st[reg1].r > 0) \
			fpu_flags.result.tag = fpu_regs.st[reg1].tag = FPUREG_PNAN; \
		else \
			fpu_flags.result.tag = fpu_regs.st[reg1].tag = FPUREG_NNAN; \
		FPOP; \
		break; \
	} \
	if(fpu_regs.st[reg1].tag == FPUREG_ZERO) { \
		fpu_flags.result.tag = fpu_regs.st[reg1].tag = FPUREG_ZERO; \
		FPOP; \
		break; \
	} \
	fpu_flags.result.tag = FPUREG_VALID; \
	fpu_flags.result.r = fpu_regs.st[reg1].r = fpu_regs.st[reg1].r / fpu_regs.st[reg2].r; \
	FPOP; \
}

#define FDIV(op1,op2) { \
	Bit8u reg1 = fpu_flags.sw.tos+op1; \
	Bit8u reg2 = fpu_flags.sw.tos+op2; \
	fpu_flags.type=t_FDIV;									\
	if(reg1>7) reg1-=7; \
	if(reg2>7) reg2-=7; \
	if((fpu_regs.st[reg1].tag!=FPUREG_VALID && fpu_regs.st[reg1].tag!=FPUREG_ZERO)||(fpu_regs.st[reg2].tag!=FPUREG_VALID && fpu_regs.st[reg2].tag!=FPUREG_ZERO)) { \
		fpu_flags.result.tag = fpu_regs.st[reg1].tag = FPUREG_NNAN; \
		break; \
	} \
	if(fpu_regs.st[reg2].tag == FPUREG_ZERO) { \
		if(fpu_regs.st[reg1].r > 0) \
			fpu_flags.result.tag = fpu_regs.st[reg1].tag = FPUREG_PNAN; \
		else \
			fpu_flags.result.tag = fpu_regs.st[reg1].tag = FPUREG_NNAN; \
		break; \
	} \
	if(fpu_regs.st[reg1].tag == FPUREG_ZERO) { \
		fpu_flags.result.tag = fpu_regs.st[reg1].tag = FPUREG_ZERO; \
		break; \
	} \
	fpu_flags.result.tag = FPUREG_VALID; \
	fpu_flags.result.r = fpu_regs.st[reg1].r = fpu_regs.st[reg1].r / fpu_regs.st[reg2].r; \
}

#define FCHS { \
	FPU_GetZF; \
	fpu_flags.type=t_FCHS;	\
	if(fpu_regs.st[fpu_flags.sw.tos].tag == FPUREG_PNAN) { \
		fpu_regs.st[fpu_flags.sw.tos].tag = FPUREG_NNAN; \
	} else if(fpu_regs.st[fpu_flags.sw.tos].tag == FPUREG_NNAN) { \
		fpu_regs.st[fpu_flags.sw.tos].tag = FPUREG_PNAN; \
	} else \
	fpu_regs.st[fpu_flags.sw.tos].r = -fpu_regs.st[fpu_flags.sw.tos].r; \
}

#define FCOMPP { \
	Bit8u reg = fpu_flags.sw.tos+1; \
	FPU_GetZF; \
	fpu_flags.type=t_FCOMP;									\
	if(reg>7) \
		reg=0; \
	if((fpu_regs.st[reg].tag==FPUREG_VALID||fpu_regs.st[reg].tag==FPUREG_ZERO)&&(fpu_regs.st[fpu_flags.sw.tos].tag==FPUREG_VALID||fpu_regs.st[fpu_flags.sw.tos].tag==FPUREG_ZERO)) { \
		fpu_flags.result.r = fpu_regs.st[reg].r - fpu_regs.st[fpu_flags.sw.tos].r; \
		if(fpu_flags.result.r==0) \
			fpu_flags.result.tag = FPUREG_ZERO; \
		else \
			fpu_flags.result.tag = FPUREG_VALID; \
		FPOP; \
		FPOP; \
		return; \
	} else if(((fpu_regs.st[reg].tag==FPUREG_EMPTY)||(fpu_regs.st[fpu_flags.sw.tos].tag==FPUREG_EMPTY))||((fpu_regs.st[reg].tag==FPUREG_VALID||fpu_regs.st[reg].tag==FPUREG_ZERO)||(fpu_regs.st[fpu_flags.sw.tos].tag==FPUREG_VALID||fpu_regs.st[fpu_flags.sw.tos].tag==FPUREG_ZERO))) { \
		fpu_flags.result.tag = FPUREG_EMPTY; \
		FPOP; \
		FPOP; \
		return; \
	} \
	Bit8s res = (fpu_regs.st[reg].tag-fpu_regs.st[fpu_flags.sw.tos].tag); \
	if(res==0||fpu_flags.cw.ic==0) { \
		fpu_flags.result.tag = FPUREG_ZERO; \
		FPOP; \
		FPOP; \
		return; \
	} else if(res>0) { \
		fpu_flags.result.tag = FPUREG_NNAN; \
		FPOP; \
		FPOP; \
		return; \
	} \
	fpu_flags.result.tag = FPUREG_PNAN; \
	FPOP; \
	FPOP; \
}