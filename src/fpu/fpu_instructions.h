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



static void FPU_FINIT(void) {
	FPU_SetCW(0x37F);
	fpu.sw=0;
	fpu.tags[0]=TAG_Empty;
	fpu.tags[1]=TAG_Empty;
	fpu.tags[2]=TAG_Empty;
	fpu.tags[3]=TAG_Empty;
	fpu.tags[4]=TAG_Empty;
	fpu.tags[5]=TAG_Empty;
	fpu.tags[6]=TAG_Empty;
	fpu.tags[7]=TAG_Empty;
	fpu.tags[8]=TAG_Valid; // is only used by us
}
static void FPU_FCLEX(void){
	fpu.sw&=0x7f00;				//should clear exceptions
};

static void FPU_FNOP(void){
	return;
}

static void FPU_PUSH(double in){
	Bitu newtop = (FPU_GET_TOP() - 1) &7;
	FPU_SET_TOP(newtop);
	//actually check if empty
	fpu.tags[newtop]=TAG_Valid;
	fpu.regs[newtop].d=in;
	return;
}
static void FPU_PUSH_ZERO(void){
	Bitu newtop = (FPU_GET_TOP() - 1) &7;
	FPU_SET_TOP(newtop);
	//actually check if empty
	fpu.tags[newtop]=TAG_Zero;
	fpu.regs[newtop].d=0.0;
	return;
}
static void FPU_FPOP(void){
	Bitu top = FPU_GET_TOP();
	fpu.tags[top]=TAG_Empty;
	//maybe set zero in it as well
	FPU_SET_TOP((top+1)&7);
	return;
}

static void FPU_FADD(Bitu op1, Bitu op2){
	fpu.regs[op1].d+=fpu.regs[op2].d;
	//flags and such :)
	return;
}

static void FPU_FSIN(void){
	Bitu top = FPU_GET_TOP();
	fpu.regs[top].d = sin(fpu.regs[top].d);
	FPU_SET_C2(0);
	//flags and such :)
	return;
}

static void FPU_FSINCOS(void){
	Bitu top = FPU_GET_TOP();

	double temp = sin(fpu.regs[top].d);
	FPU_PUSH(cos(fpu.regs[top].d));
	fpu.regs[top].d = temp;
	FPU_SET_C2(0);
	//flags and such :)
	return;
}

static void FPU_FCOS(void){
	Bitu top = FPU_GET_TOP();
	fpu.regs[top].d = cos(fpu.regs[top].d);
	FPU_SET_C2(0);
	//flags and such :)
	return;
}

static void FPU_FSQRT(void){
	Bitu top = FPU_GET_TOP();
	fpu.regs[top].d = sqrt(fpu.regs[top].d);
	//flags and such :)
	return;
}
static void FPU_FPATAN(void){
	Bitu top = FPU_GET_TOP();
	fpu.regs[(top+1)&7].d = atan(fpu.regs[(top+1)&7].d/fpu.regs[top].d);
	FPU_FPOP();
	FPU_SET_C2(0);
	//flags and such :)
	return;
}
static void FPU_FPTAN(void){
	Bitu top = FPU_GET_TOP();
	fpu.regs[top].d = tan(fpu.regs[top].d);
	FPU_PUSH(1.0);
	FPU_SET_C2(0);
	//flags and such :)
	return;
}
static void FPU_FDIV(Bitu st, Bitu other){
	fpu.regs[st].d= fpu.regs[st].d/fpu.regs[other].d;
	//flags and such :)
	return;
}

static void FPU_FDIVR(Bitu st, Bitu other){
	fpu.regs[st].d= fpu.regs[other].d/fpu.regs[st].d;
	// flags and such :)
	return;
};

static void FPU_FMUL(Bitu st, Bitu other){
	fpu.regs[st].d*=fpu.regs[other].d;
	//flags and such :)
	return;
}

static void FPU_FSUB(Bitu st, Bitu other){
	fpu.regs[st].d = fpu.regs[st].d - fpu.regs[other].d;
	//flags and such :)
	return;
}

static void FPU_FSUBR(Bitu st, Bitu other){
	fpu.regs[st].d= fpu.regs[other].d - fpu.regs[st].d;
	//flags and such :)
	return;
}

static void FPU_FXCH(Bitu st, Bitu other){
	FPU_Tag tag = fpu.tags[other];
	FPU_Reg reg = fpu.regs[other];
	fpu.tags[other] = fpu.tags[st];
	fpu.regs[other] = fpu.regs[st];
	fpu.tags[st] = tag;
	fpu.regs[st] = reg;
}

static void FPU_FST(Bitu st, Bitu other){
	fpu.tags[other] = fpu.tags[st];
	fpu.regs[other] = fpu.regs[st];
}



static void FPU_FCOM(Bitu st, Bitu other){
	if((fpu.tags[st] != TAG_Valid) || (fpu.tags[other] != TAG_Valid)){
		FPU_SET_C3(1);FPU_SET_C2(1);FPU_SET_C0(1);return;
	}
	if(fpu.regs[st].d == fpu.regs[other].d){
		FPU_SET_C3(1);FPU_SET_C2(0);FPU_SET_C0(0);return;
	}
	if(fpu.regs[st].d < fpu.regs[other].d){
		FPU_SET_C3(0);FPU_SET_C2(0);FPU_SET_C0(1);return;
	}
	// st > other
	FPU_SET_C3(0);FPU_SET_C2(0);FPU_SET_C0(0);return;
}

static void FPU_FUCOM(Bitu st, Bitu other){
	//does atm the same as fcom 
	FPU_FCOM(st,other);
}

static double FROUND(double in){
	switch(fpu.round){
	case ROUND_Nearest:
		return((in-floor(in)>=0.5)?(floor(in)+1):(floor(in)));
		break;
	case ROUND_Down:
		return (floor(in));
		break;
	case ROUND_Up:
		return (ceil(in));
		break;
	case ROUND_Chop:
		return in; //the cast afterwards will do it right maybe cast here
		break;
	default:
		return in;
		break;
	}
}
