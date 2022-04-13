/*
 *  Copyright (C) 2002-2021  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef DOSBOX_FPU_H
#include "fpu.h"
#endif


static void FPU_FINIT(void) {
	FPU_SetCW(0x37F);
	fpu.sw = 0;
	TOP=FPU_GET_TOP();
	fpu.tags[0] = TAG_Empty;
	fpu.tags[1] = TAG_Empty;
	fpu.tags[2] = TAG_Empty;
	fpu.tags[3] = TAG_Empty;
	fpu.tags[4] = TAG_Empty;
	fpu.tags[5] = TAG_Empty;
	fpu.tags[6] = TAG_Empty;
	fpu.tags[7] = TAG_Empty;
	fpu.tags[8] = TAG_Valid; // is only used by us
}

static void FPU_FCLEX(void){
	fpu.sw &= 0x7f00;			//should clear exceptions
}

static void FPU_FNOP(void){
	return;
}

static void FPU_PREP_PUSH(void){
	TOP = (TOP - 1) &7;
#if DB_FPU_STACK_CHECK_PUSH > DB_FPU_STACK_CHECK_NONE
	if (GCC_UNLIKELY(fpu.tags[TOP] != TAG_Empty)) {
#if DB_FPU_STACK_CHECK_PUSH == DB_FPU_STACK_CHECK_EXIT
		E_Exit("FPU stack overflow");
#else
		if (fpu.cw&1) { // Masked ?
			fpu.sw |= 0x1; //Invalid Operation
			fpu.sw |= 0x40; //Stack Fault
			FPU_SET_C1(1); //Register is used.
			//No need to set 0x80 as the exception is masked.
			LOG(LOG_FPU,LOG_ERROR)("Masked stack overflow encountered!");
		} else {
			E_Exit("FPU stack overflow"); //Exit as this is bad
		}
#endif
	}
#endif
	fpu.tags[TOP] = TAG_Valid;
}

static void FPU_PUSH(double in){
	FPU_PREP_PUSH();
	fpu.regs[TOP].d = in;
//	LOG(LOG_FPU,LOG_ERROR)("Pushed at %d  %g to the stack",newtop,in);
	return;
}


static void FPU_FPOP(void){
#if DB_FPU_STACK_CHECK_POP > DB_FPU_STACK_CHECK_NONE
	if (GCC_UNLIKELY(fpu.tags[TOP] == TAG_Empty)) {
#if DB_FPU_STACK_CHECK_POP == DB_FPU_STACK_CHECK_EXIT
		E_Exit("FPU stack underflow");
#else
		if (fpu.cw&1) { // Masked ?
			fpu.sw |= 0x1; //Invalid Operation
			fpu.sw |= 0x40; //Stack Fault
			FPU_SET_C1(0); //Register is free.
			//No need to set 0x80 as the exception is masked.
			LOG(LOG_FPU,LOG_ERROR)("Masked stack underflow encountered!");
		} else {
			LOG_MSG("Unmasked Stack underflow!"); //Also log in release mode
		}
#endif
	}
#endif
	fpu.tags[TOP]=TAG_Empty;
	//maybe set zero in it as well
	TOP = ((TOP+1)&7);
//	LOG(LOG_FPU,LOG_ERROR)("popped from %d  %g off the stack",top,fpu.regs[top].d);
	return;
}

static double FROUND(double in){
	switch(fpu.round){
	case ROUND_Nearest:	
		if (in-floor(in)>0.5) return (floor(in)+1);
		else if (in-floor(in)<0.5) return (floor(in));
		else return (((static_cast<int64_t>(floor(in)))&1)!=0)?(floor(in)+1):(floor(in));
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

#define BIAS80 16383
#define BIAS64 1023

static Real64 FPU_FLD80(PhysPt addr) {
	struct {
		int16_t begin;
		FPU_Reg eind;
	} test;
	test.eind.l.lower = mem_readd(addr);
	test.eind.l.upper = mem_readd(addr+4);
	test.begin = mem_readw(addr+8);
   
	int64_t exp64 = (((test.begin&0x7fff) - BIAS80));
	int64_t blah = ((exp64 >0)?exp64:-exp64)&0x3ff;
	int64_t exp64final = ((exp64 >0)?blah:-blah) +BIAS64;

	int64_t mant64 = (test.eind.ll >> 11) & LONGTYPE(0xfffffffffffff);
	int64_t sign = (test.begin&0x8000)?1:0;
	FPU_Reg result;
	result.ll = (sign <<63)|(exp64final << 52)| mant64;

	if (test.eind.l.lower == 0 && test.eind.l.upper == INT32_MIN &&
	    (test.begin & INT16_MAX) == INT16_MAX) {
		//Detect INF and -INF (score 3.11 when drawing a slur.)
		result.d = sign?-HUGE_VAL:HUGE_VAL;
	}
	return result.d;

	//mant64= test.mant80/2***64    * 2 **53 
}

static void FPU_ST80(PhysPt addr,Bitu reg) {
	struct {
		int16_t begin;
		FPU_Reg eind;
	} test;
	int64_t sign80 = (fpu.regs[reg].ll&LONGTYPE(0x8000000000000000))?1:0;
	int64_t exp80 =  fpu.regs[reg].ll&LONGTYPE(0x7ff0000000000000);
	int64_t exp80final = (exp80>>52);
	int64_t mant80 = fpu.regs[reg].ll&LONGTYPE(0x000fffffffffffff);
	int64_t mant80final = (mant80 << 11);
	if(fpu.regs[reg].d != 0){ //Zero is a special case
		// Elvira wants the 8 and tcalc doesn't
		mant80final |= LONGTYPE(0x8000000000000000);
		//Ca-cyber doesn't like this when result is zero.
		exp80final += (BIAS80 - BIAS64);
	}
	test.begin = (static_cast<int16_t>(sign80)<<15)| static_cast<int16_t>(exp80final);
	test.eind.ll = mant80final;
	mem_writed(addr,test.eind.l.lower);
	mem_writed(addr+4,test.eind.l.upper);
	mem_writew(addr+8,test.begin);
}


static void FPU_FLD_F32(PhysPt addr,Bitu store_to) {
	union {
		float f;
		uint32_t l;
	}	blah;
	blah.l = mem_readd(addr);
	fpu.regs[store_to].d = static_cast<Real64>(blah.f);
}

static void FPU_FLD_F64(PhysPt addr,Bitu store_to) {
	fpu.regs[store_to].l.lower = mem_readd(addr);
	fpu.regs[store_to].l.upper = mem_readd(addr+4);
}

static void FPU_FLD_F80(PhysPt addr) {
	fpu.regs[TOP].d = FPU_FLD80(addr);
}

static void FPU_FLD_I16(PhysPt addr,Bitu store_to) {
	int16_t blah = mem_readw(addr);
	fpu.regs[store_to].d = static_cast<Real64>(blah);
}

static void FPU_FLD_I32(PhysPt addr,Bitu store_to) {
	int32_t blah = mem_readd(addr);
	fpu.regs[store_to].d = static_cast<Real64>(blah);
}

static void FPU_FLD_I64(PhysPt addr,Bitu store_to) {
	FPU_Reg blah;
	blah.l.lower = mem_readd(addr);
	blah.l.upper = mem_readd(addr+4);
	fpu.regs[store_to].d = static_cast<Real64>(blah.ll);
}

static void FPU_FBLD(PhysPt addr,Bitu store_to) {
	uint64_t val = 0;
	Bitu in = 0;
	uint64_t base = 1;
	for(Bitu i = 0;i < 9;i++){
		in = mem_readb(addr + i);
		val += ( (in&0xf) * base); //in&0xf shouldn't be higher then 9
		base *= 10;
		val += ((( in>>4)&0xf) * base);
		base *= 10;
	}

	//last number, only now convert to float in order to get
	//the best signification
	Real64 temp = static_cast<Real64>(val);
	in = mem_readb(addr + 9);
	temp += ( (in&0xf) * base );
	if(in&0x80) temp *= -1.0;
	fpu.regs[store_to].d = temp;
}


static inline void FPU_FLD_F32_EA(PhysPt addr) {
	FPU_FLD_F32(addr,8);
}
static inline void FPU_FLD_F64_EA(PhysPt addr) {
	FPU_FLD_F64(addr,8);
}
static inline void FPU_FLD_I32_EA(PhysPt addr) {
	FPU_FLD_I32(addr,8);
}
static inline void FPU_FLD_I16_EA(PhysPt addr) {
	FPU_FLD_I16(addr,8);
}


static void FPU_FST_F32(PhysPt addr) {
	union {
		float f;
		uint32_t l;
	}	blah;
	//should depend on rounding method
	blah.f = static_cast<float>(fpu.regs[TOP].d);
	mem_writed(addr,blah.l);
}

static void FPU_FST_F64(PhysPt addr) {
	mem_writed(addr,fpu.regs[TOP].l.lower);
	mem_writed(addr+4,fpu.regs[TOP].l.upper);
}

static void FPU_FST_F80(PhysPt addr) {
	FPU_ST80(addr,TOP);
}

static void FPU_FST_I16(PhysPt addr) {
	double val = FROUND(fpu.regs[TOP].d);
	mem_writew(addr,(val < 32768.0 && val >= -32768.0)?static_cast<int16_t>(val):0x8000);
}

static void FPU_FST_I32(PhysPt addr) {
	double val = FROUND(fpu.regs[TOP].d);
	mem_writed(addr,(val < 2147483648.0 && val >= -2147483648.0)?static_cast<int32_t>(val):0x80000000);
}

static void FPU_FST_I64(PhysPt addr) {
	double val = FROUND(fpu.regs[TOP].d);
	FPU_Reg blah;
	blah.ll = (val < 9223372036854775808.0 && val >= -9223372036854775808.0)?static_cast<int64_t>(val):LONGTYPE(0x8000000000000000);

	mem_writed(addr,blah.l.lower);
	mem_writed(addr+4,blah.l.upper);
}

static void FPU_FBST(PhysPt addr) {
	FPU_Reg val = fpu.regs[TOP];
	if(val.ll & LONGTYPE(0x8000000000000000)) { // MSB = sign
		mem_writeb(addr+9,0x80);
		val.d = -val.d;
	} else mem_writeb(addr+9,0);

	uint64_t rndint = static_cast<uint64_t>(FROUND(val.d));
	// BCD (18 decimal digits) overflow? (0x0DE0B6B3A763FFFF max)
	if (rndint > LONGTYPE(999999999999999999)) {
		// write BCD integer indefinite value
		mem_writed(addr+0,0);
		mem_writed(addr+4,0xC0000000);
		mem_writew(addr+8,0xFFFF);
		return;
	}

	static_assert(sizeof(rndint) == sizeof(long long int),
	              "passing rndint to lldiv function");
	// numbers from back to front
	for (int i = 0; i < 9; i++) {
		const lldiv_t div10 = lldiv(rndint, 10);
		const lldiv_t div100 = lldiv(div10.quot, 10);
		const uint8_t p = static_cast<uint8_t>(div10.rem) |
		                  (static_cast<uint8_t>(div100.rem) << 4);
		mem_writeb(addr++, p);
		rndint = div100.quot;
	}
	// flags? C1 should indicate if value was rounded up
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
	fpu.regs[STV(1)].d = atan2(fpu.regs[STV(1)].d,fpu.regs[TOP].d);
	FPU_FPOP();
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
}

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
	if(((fpu.tags[st] != TAG_Valid) && (fpu.tags[st] != TAG_Zero)) || 
		((fpu.tags[other] != TAG_Valid) && (fpu.tags[other] != TAG_Zero))){
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

static void FPU_FRNDINT(void){
	int64_t temp  = static_cast<int64_t>(FROUND(fpu.regs[TOP].d));
	double tempd = static_cast<double>(temp);
	if (fpu.cw&0x20) { //As we don't generate exceptions; only do it when masked
		if (tempd != fpu.regs[TOP].d)
			fpu.sw |= 0x20; //Set Precision Exception
	}
	fpu.regs[TOP].d = tempd;
}

static void FPU_FPREM(void){
	Real64 valtop = fpu.regs[TOP].d;
	Real64 valdiv = fpu.regs[STV(1)].d;
	int64_t ressaved = static_cast<int64_t>( (valtop/valdiv) );
// Some backups
//	Real64 res=valtop - ressaved*valdiv; 
//      res= fmod(valtop,valdiv);
	fpu.regs[TOP].d = valtop - ressaved*valdiv;
	FPU_SET_C0(static_cast<Bitu>(ressaved&4));
	FPU_SET_C3(static_cast<Bitu>(ressaved&2));
	FPU_SET_C1(static_cast<Bitu>(ressaved&1));
	FPU_SET_C2(0);
}

static void FPU_FPREM1(void){
	Real64 valtop = fpu.regs[TOP].d;
	Real64 valdiv = fpu.regs[STV(1)].d;
	double quot = valtop/valdiv;
	double quotf = floor(quot);
	int64_t ressaved;
	if (quot-quotf>0.5) ressaved = static_cast<int64_t>(quotf+1);
	else if (quot-quotf<0.5) ressaved = static_cast<int64_t>(quotf);
	else ressaved = static_cast<int64_t>((((static_cast<int64_t>(quotf))&1)!=0)?(quotf+1):(quotf));
	fpu.regs[TOP].d = valtop - ressaved*valdiv;
	FPU_SET_C0(static_cast<Bitu>(ressaved&4));
	FPU_SET_C3(static_cast<Bitu>(ressaved&2));
	FPU_SET_C1(static_cast<Bitu>(ressaved&1));
	FPU_SET_C2(0);
}

static void FPU_FXAM(void){
	if(fpu.regs[TOP].ll & LONGTYPE(0x8000000000000000))	//sign
	{ 
		FPU_SET_C1(1);
	} 
	else 
	{
		FPU_SET_C1(0);
	}
	if(fpu.tags[TOP] == TAG_Empty)
	{
		FPU_SET_C3(1);FPU_SET_C2(0);FPU_SET_C0(1);
		return;
	}
	if(fpu.regs[TOP].d == 0.0)		//zero or normalized number.
	{ 
		FPU_SET_C3(1);FPU_SET_C2(0);FPU_SET_C0(0);
	}
	else
	{
		FPU_SET_C3(0);FPU_SET_C2(1);FPU_SET_C0(0);
	}
}


static void FPU_F2XM1(void){
	fpu.regs[TOP].d = pow(2.0,fpu.regs[TOP].d) - 1;
	return;
}

static void FPU_FYL2X(void){
	fpu.regs[STV(1)].d*=log(fpu.regs[TOP].d)/log(static_cast<Real64>(2.0));
	FPU_FPOP();
	return;
}

static void FPU_FYL2XP1(void){
	fpu.regs[STV(1)].d*=log(fpu.regs[TOP].d+1.0)/log(static_cast<Real64>(2.0));
	FPU_FPOP();
	return;
}

static void FPU_FSCALE(void){
	fpu.regs[TOP].d *= pow(2.0,static_cast<Real64>(static_cast<int64_t>(fpu.regs[STV(1)].d)));
	//FPU_SET_C1(0);
	return; //2^x where x is chopped.
}

static void FPU_FSTENV(PhysPt addr){
	FPU_SET_TOP(TOP);
	if(!cpu.code.big) {
		mem_writew(addr+0,static_cast<uint16_t>(fpu.cw));
		mem_writew(addr+2,static_cast<uint16_t>(fpu.sw));
		mem_writew(addr+4,static_cast<uint16_t>(FPU_GetTag()));
	} else { 
		mem_writed(addr+0,static_cast<uint32_t>(fpu.cw));
		mem_writed(addr+4,static_cast<uint32_t>(fpu.sw));
		mem_writed(addr+8,static_cast<uint32_t>(FPU_GetTag()));
	}
}

static void FPU_FLDENV(PhysPt addr){
	uint16_t tag;
	uint32_t tagbig;
	Bitu cw;
	if(!cpu.code.big) {
		cw     = mem_readw(addr+0);
		fpu.sw = mem_readw(addr+2);
		tag    = mem_readw(addr+4);
	} else { 
		cw     = mem_readd(addr+0);
		fpu.sw = (uint16_t)mem_readd(addr+4);
		tagbig = mem_readd(addr+8);
		tag    = static_cast<uint16_t>(tagbig);
	}
	FPU_SetTag(tag);
	FPU_SetCW(cw);
	TOP = FPU_GET_TOP();
}

static void FPU_FSAVE(PhysPt addr){
	FPU_FSTENV(addr);
	Bitu start = (cpu.code.big?28:14);
	for(Bitu i = 0;i < 8;i++){
		FPU_ST80(addr+start,STV(i));
		start += 10;
	}
	FPU_FINIT();
}

static void FPU_FRSTOR(PhysPt addr){
	FPU_FLDENV(addr);
	Bitu start = (cpu.code.big?28:14);
	for(Bitu i = 0;i < 8;i++){
		fpu.regs[STV(i)].d = FPU_FLD80(addr+start);
		start += 10;
	}
}

static void FPU_FXTRACT(void) {
	// function stores real bias in st and 
	// pushes the significant number onto the stack
	// if double ever uses a different base please correct this function

	FPU_Reg test = fpu.regs[TOP];
	int64_t exp80 =  test.ll&LONGTYPE(0x7ff0000000000000);
	int64_t exp80final = (exp80>>52) - BIAS64;
	Real64 mant = test.d / (pow(2.0,static_cast<Real64>(exp80final)));
	fpu.regs[TOP].d = static_cast<Real64>(exp80final);
	FPU_PUSH(mant);
}

static void FPU_FCHS(void){
	fpu.regs[TOP].d = -1.0*(fpu.regs[TOP].d);
}

static void FPU_FABS(void){
	fpu.regs[TOP].d = fabs(fpu.regs[TOP].d);
}

static void FPU_FTST(void){
	fpu.regs[8].d = 0.0;
	FPU_FCOM(TOP,8);
}

static void FPU_FLD1(void){
	FPU_PREP_PUSH();
	fpu.regs[TOP].d = 1.0;
}

static void FPU_FLDL2T(void){
	FPU_PREP_PUSH();
	fpu.regs[TOP].d = L2T;
}

static void FPU_FLDL2E(void){
	FPU_PREP_PUSH();
	fpu.regs[TOP].d = L2E;
}

static void FPU_FLDPI(void){
	FPU_PREP_PUSH();
	fpu.regs[TOP].d = M_PI;
}

static void FPU_FLDLG2(void){
	FPU_PREP_PUSH();
	fpu.regs[TOP].d = LG2;
}

static void FPU_FLDLN2(void){
	FPU_PREP_PUSH();
	fpu.regs[TOP].d = LN2;
}

static void FPU_FLDZ(void){
	FPU_PREP_PUSH();
	fpu.regs[TOP].d = 0.0;
	fpu.tags[TOP] = TAG_Zero;
}


static inline void FPU_FADD_EA(Bitu op1){
	FPU_FADD(op1,8);
}
static inline void FPU_FMUL_EA(Bitu op1){
	FPU_FMUL(op1,8);
}
static inline void FPU_FSUB_EA(Bitu op1){
	FPU_FSUB(op1,8);
}
static inline void FPU_FSUBR_EA(Bitu op1){
	FPU_FSUBR(op1,8);
}
static inline void FPU_FDIV_EA(Bitu op1){
	FPU_FDIV(op1,8);
}
static inline void FPU_FDIVR_EA(Bitu op1){
	FPU_FDIVR(op1,8);
}
static inline void FPU_FCOM_EA(Bitu op1){
	FPU_FCOM(op1,8);
}

