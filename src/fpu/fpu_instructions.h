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

/* $Id: fpu_instructions.h,v 1.14 2003-10-19 19:21:12 qbix79 Exp $ */


static void FPU_FINIT(void) {
	FPU_SetCW(0x37F);
	fpu.sw=0;
	TOP=FPU_GET_TOP();
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
	TOP = (TOP - 1) &7;
	//actually check if empty
	fpu.tags[TOP]=TAG_Valid;
	fpu.regs[TOP].d=in;
//	LOG(LOG_FPU,LOG_ERROR)("Pushed at %d  %g to the stack",newtop,in);
	return;
}
static void FPU_PUSH_ZERO(void){
	FPU_PUSH(0.0);
	return; //maybe oneday needed
}
static void FPU_FPOP(void){
	fpu.tags[TOP]=TAG_Empty;
	//maybe set zero in it as well
	TOP = ((TOP+1)&7);
//	LOG(LOG_FPU,LOG_ERROR)("popped from %d  %g off the stack",top,fpu.regs[top].d);
	return;
}

static void FPU_FADD(Bitu op1, Bitu op2){
	fpu.regs[op1].d+=fpu.regs[op2].d;
	//flags and such :)
	return;
}

static void FPU_FSIN(void){
	fpu.regs[TOP].d = sin(fpu.regs[TOP].d);
	FPU_SET_C2(0);
	//flags and such :)
	return;
}

static void FPU_FSINCOS(void){
	Real64 temp = fpu.regs[TOP].d;
	fpu.regs[TOP].d = sin(temp);
	FPU_PUSH(cos(temp));
	FPU_SET_C2(0);
	//flags and such :)
	return;
}

static void FPU_FCOS(void){
	fpu.regs[TOP].d = cos(fpu.regs[TOP].d);
	FPU_SET_C2(0);
	//flags and such :)
	return;
}

static void FPU_FSQRT(void){
	fpu.regs[TOP].d = sqrt(fpu.regs[TOP].d);
	//flags and such :)
	return;
}
static void FPU_FPATAN(void){
	fpu.regs[ST(1)].d = atan(fpu.regs[ST(1)].d/fpu.regs[TOP].d);
	FPU_FPOP();
	FPU_SET_C2(0);
	//flags and such :)
	return;
}
static void FPU_FPTAN(void){
	fpu.regs[TOP].d = tan(fpu.regs[TOP].d);
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
		if (in-floor(in)>0.5) return (floor(in)+1);
		else if (in-floor(in)<0.5) return (floor(in));
		else return (((static_cast<Bit64s>(floor(in)))&1)!=0)?(floor(in)+1):(floor(in));
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

static void FPU_FPREM(void){
	Real64 valtop = fpu.regs[TOP].d;
	Real64 valdiv = fpu.regs[ST(1)].d;
	Bit64s ressaved = static_cast<Bit64s>( (valtop/valdiv) );
// Some backups
//	Real64 res=valtop - ressaved*valdiv; 
//      res= fmod(valtop,valdiv);
	fpu.regs[TOP].d = valtop - ressaved*valdiv;
	FPU_SET_C0(static_cast<Bitu>(ressaved&4));
	FPU_SET_C3(static_cast<Bitu>(ressaved&2));
	FPU_SET_C1(static_cast<Bitu>(ressaved&1));
	FPU_SET_C2(0);
}

static void FPU_FXAM(void){
	if(fpu.tags[TOP] == TAG_Empty)
	{
		FPU_SET_C3(1);FPU_SET_C0(1);
		return;
	}
	if(fpu.regs[TOP].ll & LONGTYPE(0x8000000000000000))	//sign
	{ 
		FPU_SET_C1(1);
	}
	else
	{
		FPU_SET_C1(0);
	}
	if(fpu.regs[TOP].d == 0.0)		//zero or normalized number.
	{ 
		FPU_SET_C3(1);FPU_SET_C2(0);FPU_SET_C0(0);
	}
	else{
		FPU_SET_C3(0);FPU_SET_C2(1);FPU_SET_C0(0);
	}
}

static void FPU_FBST(PhysPt addr)
{
	FPU_Reg val = fpu.regs[TOP];
	bool sign = false;
	if(val.d<0.0){ //sign
		sign=true;
		val.d=-val.d;
	}
	//numbers from back to front
	Real64 temp=val.d;
	Bitu p;
	for(Bitu i=0;i<9;i++){
		val.d=temp;
		temp = static_cast<Real64>(static_cast<Bit64s>(floor(val.d/10.0)));
		p = static_cast<Bitu>(val.d - 10.0*temp);  
		val.d=temp;
		temp = static_cast<Real64>(static_cast<Bit64s>(floor(val.d/10.0)));
		p |= (static_cast<Bitu>(val.d - 10.0*temp)<<4);

		mem_writeb(addr+i,p);
	}
	val.d=temp;
	temp = static_cast<Real64>(static_cast<Bit64s>(floor(val.d/10.0)));
	p = static_cast<Bitu>(val.d - 10.0*temp);
	if(sign)
		p|=0x80;
	mem_writeb(addr+9,p);
}

#define BIAS80 16383
#define BIAS64 1023

static void FPU_FLD80(PhysPt addr)
{
	struct{
		Bit16s begin;
		FPU_Reg eind;
	} test;
	test.eind.l.lower=mem_readd(addr);
	test.eind.l.upper =mem_readd(addr+4);
	test.begin=mem_readw(addr+8);

	Bit64s exp64= (((test.begin & 0x7fff) - BIAS80));
	Bit64s blah= ((exp64 >0)?exp64:-exp64)&0x3ff;
	Bit64s exp64final= ((exp64 >0)?blah:-blah) +BIAS64;

	Bit64s mant64= (test.eind.ll >> 11) & LONGTYPE(0xfffffffffffff);
	Bit64s sign = (test.begin &0x8000)?1:0;
	FPU_Reg result;
	result.ll= (sign <<63)|(exp64final << 52)| mant64;
	FPU_PUSH(result.d);
	//mant64= test.mant80/2***64    * 2 **53 
}

static void FPU_ST80(PhysPt addr)
{
	struct{
		Bit16s begin;
		FPU_Reg eind;
	} test;
	Bit64s sign80= (fpu.regs[TOP].ll&LONGTYPE(0x8000000000000000))?1:0;
	Bit64s exp80 =  fpu.regs[TOP].ll&LONGTYPE(0x7ff0000000000000);
	Bit64s exp80final= (exp80>>52) - BIAS64 + BIAS80;
	Bit64s mant80 = fpu.regs[TOP].ll&LONGTYPE(0x000fffffffffffff);
	Bit64s mant80final= (mant80 << 11) | LONGTYPE(0x8000000000000000);
	test.begin= (static_cast<Bit16s>(sign80)<<15)| static_cast<Bit16s>(exp80final);
	test.eind.ll=mant80final;
	mem_writed(addr,test.eind.l.lower);
	mem_writed(addr+4,test.eind.l.upper);
	mem_writew(addr+8,test.begin);
}

static void FPU_F2XM1(void){
	fpu.regs[TOP].d=pow(2.0,fpu.regs[TOP].d) -1;
	return;
}

static void FPU_FYL2X(void){
	fpu.regs[ST(1)].d*=log(fpu.regs[TOP].d)/log(static_cast<Real64>(2.0));
	FPU_FPOP();
	return;
}
static void FPU_FSCALE(void){
	fpu.regs[TOP].d *= pow(2.0,static_cast<Real64>(static_cast<Bit64s>(fpu.regs[ST(1)].d)));
	return; //2^x where x is chopped.
}

