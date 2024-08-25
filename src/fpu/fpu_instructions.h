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

#include "math_utils.h"

static constexpr uint16_t PrecisionModeMask = 0x0300;

static constexpr uint16_t SinglePrecisionMode   = 0x0000;
static constexpr uint16_t DoublePrecisionMode   = 0x0200;
static constexpr uint16_t ExtendedPrecisionMode = 0x0300;

static void FPU_FINIT(void)
{
	FPU_SetCW(0x37F);
	fpu.sw      = 0;
	TOP         = FPU_GET_TOP();
	fpu.tags[0] = TAG_Empty;
	fpu.tags[1] = TAG_Empty;
	fpu.tags[2] = TAG_Empty;
	fpu.tags[3] = TAG_Empty;
	fpu.tags[4] = TAG_Empty;
	fpu.tags[5] = TAG_Empty;
	fpu.tags[6] = TAG_Empty;
	fpu.tags[7] = TAG_Empty;
	fpu.tags[8] = TAG_Valid; // is only used by us

	for (auto& reg_memcpy : fpu.regs_memcpy) {
		reg_memcpy.reset();
	}
}

static void FPU_FCLEX(void)
{
	fpu.sw &= 0x7f00; // should clear exceptions
}

static void FPU_FNOP(void)
{
	return;
}

static void FPU_PREP_PUSH(void)
{
	TOP = (TOP - 1) & 7;
#if DB_FPU_STACK_CHECK_PUSH > DB_FPU_STACK_CHECK_NONE
	if (fpu.tags[TOP] != TAG_Empty) {
	#if DB_FPU_STACK_CHECK_PUSH == DB_FPU_STACK_CHECK_EXIT
		E_Exit("FPU stack overflow");
	#else
		if (fpu.cw & 1) {       // Masked ?
			fpu.sw |= 0x1;  // Invalid Operation
			fpu.sw |= 0x40; // Stack Fault
			FPU_SET_C1(1);  // Register is used.
			// No need to set 0x80 as the exception is masked.
			LOG(LOG_FPU, LOG_ERROR)
			("Masked stack overflow encountered!");
		} else {
			E_Exit("FPU stack overflow"); // Exit as this is bad
		}
	#endif
	}
#endif
	fpu.tags[TOP] = TAG_Valid;
	fpu.regs_memcpy[TOP].reset();
}

static void FPU_PUSH(double in)
{
	FPU_PREP_PUSH();
	fpu.regs[TOP].d = in;
	//	LOG(LOG_FPU,LOG_ERROR)("Pushed at %d  %g to the stack",newtop,in);
	return;
}

static void FPU_FPOP(void)
{
#if DB_FPU_STACK_CHECK_POP > DB_FPU_STACK_CHECK_NONE
	if (fpu.tags[TOP] == TAG_Empty) {
	#if DB_FPU_STACK_CHECK_POP == DB_FPU_STACK_CHECK_EXIT
		E_Exit("FPU stack underflow");
	#else
		if (fpu.cw & 1) {       // Masked ?
			fpu.sw |= 0x1;  // Invalid Operation
			fpu.sw |= 0x40; // Stack Fault
			FPU_SET_C1(0);  // Register is free.
			// No need to set 0x80 as the exception is masked.
			LOG(LOG_FPU, LOG_ERROR)
			("Masked stack underflow encountered!");
		} else {
			LOG_MSG("Unmasked Stack underflow!"); // Also log in
			                                      // release mode
		}
	#endif
	}
#endif
	fpu.tags[TOP] = TAG_Empty;
	fpu.regs_memcpy[TOP].reset();
	// maybe set zero in it as well
	TOP = ((TOP + 1) & 7);
	//	LOG(LOG_FPU,LOG_ERROR)("popped from %d  %g off the
	// stack",top,fpu.regs[top].d);
	return;
}

static double FROUND(double in)
{
	const auto prec_mode = fpu.cw & PrecisionModeMask;

	// If the fpu is in single precision mode, cast to a float first
	// to correct any double values that aren't valid single precision
	// values.
	if (prec_mode == SinglePrecisionMode) {
		in = static_cast<float>(in);
	}

	switch (fpu.round) {
	case ROUND_Nearest: return std::nearbyint(in);
	case ROUND_Down: return std::floor(in);
	case ROUND_Up: return std::ceil(in);
	case ROUND_Chop: {
		// This is a fix for rounding to a close integer in extended
		// precision mode, e.g. 7.999999999999994; an example can be
		// seen in the Quake options screen size slider. In this case,
		// what is almost certainly wanted is 8.0, so return the closer
		// integer instead of chopping to the lower value.
		if (prec_mode == ExtendedPrecisionMode) {
			if (const auto lower = std::floor(in);
			    are_almost_equal_relative(in, lower)) {
				return lower;
			} else if (const auto upper = std::ceil(in);
			           are_almost_equal_relative(upper, in)) {
				return upper;
			}
		}
		return std::trunc(in);
	}
	default: return in;
	}
}

#define BIAS80 16383
#define BIAS64 1023

static Real64 FPU_FLD80(PhysPt addr) {
	struct {
		int16_t begin = 0;
		FPU_Reg eind  = {};
	} test = {};

	test.eind.ll = mem_readq(addr);
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
		int16_t begin = 0;
		FPU_Reg eind  = {};
	} test = {};

	int64_t sign80 = (fpu.regs[reg].ll & LONGTYPE(0x8000000000000000)) ? 1 : 0;
	int64_t exp80       = fpu.regs[reg].ll & LONGTYPE(0x7ff0000000000000);
	int64_t exp80final  = (exp80 >> 52);
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
	mem_writeq(addr, test.eind.ll);
	mem_writew(addr+8,test.begin);
}

static void FPU_FLD_F32(PhysPt addr, Bitu store_to)
{
	const auto mem_val = mem_readd(addr);
	float f;
	memcpy(&f, &mem_val, sizeof(mem_val));
	fpu.regs[store_to].d = static_cast<Real64>(f);
}

static void FPU_FLD_F64(PhysPt addr, Bitu store_to)
{
	fpu.regs[store_to].ll = static_cast<int64_t>(mem_readq(addr));
}

static void FPU_FLD_F80(PhysPt addr)
{
	fpu.regs[TOP].d = FPU_FLD80(addr);
}

static void FPU_FLD_I16(PhysPt addr, Bitu store_to)
{
	const auto mem_val   = static_cast<int16_t>(mem_readw(addr));
	fpu.regs[store_to].d = static_cast<Real64>(mem_val);
}

static void FPU_FLD_I32(PhysPt addr, Bitu store_to)
{
	const auto mem_val   = static_cast<int32_t>(mem_readd(addr));
	fpu.regs[store_to].d = static_cast<Real64>(mem_val);
}

static void FPU_FLD_I64(PhysPt addr, Bitu store_to)
{
	// The range of integers that can fit in the mantissa of a double.
	// https://en.wikipedia.org/wiki/Double-precision_floating-point_format
	constexpr int64_t MantissaMin = -(1LL << 53);
	constexpr int64_t MantissaMax = (1LL << 53);

	FPU_Reg temp_reg;
	temp_reg.ll = static_cast<int64_t>(mem_readq(addr));

	fpu.regs[store_to].d = static_cast<Real64>(temp_reg.ll);
	// This is a fix for vertical bars that appear in some games that
	// use FILD/FIST as a fast 64-bit integer memcpy, such as Carmageddon,
	// Motor Mash, and demos like Sunflower and Multikolor.

	// If the value won't fit in the mantissa of a double, save the
	// value into the regs_memcpy register to indicate that the integer
	// value should be read out by the FPU_FST_I64 function instead.
	if (temp_reg.ll > MantissaMax || temp_reg.ll < MantissaMin) {
		fpu.regs_memcpy[store_to] = temp_reg.ll;
	} else {
		fpu.regs_memcpy[store_to].reset();
	}
}

static void FPU_FBLD(PhysPt addr, Bitu store_to)
{
	uint64_t val  = 0;
	uint8_t in    = 0;
	uint64_t base = 1;

	for (int i = 0; i < 9; i++) {
		in = mem_readb(addr + i);
		val += ((in & 0xf) * base); // in&0xf shouldn't be higher then 9
		base *= 10;
		val += (((in >> 4) & 0xf) * base);
		base *= 10;
	}

	// last number, only now convert to float in order to get
	// the best signification
	auto temp = static_cast<Real64>(val);
	in        = mem_readb(addr + 9);
	temp += ((in & 0xf) * base);

	if (in & 0x80) {
		temp = -temp;
	}

	fpu.regs[store_to].d = temp;
}

static inline void FPU_FLD_F32_EA(PhysPt addr)
{
	FPU_FLD_F32(addr, 8);
}
static inline void FPU_FLD_F64_EA(PhysPt addr)
{
	FPU_FLD_F64(addr, 8);
}
static inline void FPU_FLD_I32_EA(PhysPt addr)
{
	FPU_FLD_I32(addr, 8);
}
static inline void FPU_FLD_I16_EA(PhysPt addr)
{
	FPU_FLD_I16(addr, 8);
}

static void FPU_FST_F32(PhysPt addr)
{
	// should depend on rounding method
	const auto f = static_cast<float>(fpu.regs[TOP].d);
	uint32_t l;
	memcpy(&l, &f, sizeof(f));
	mem_writed(addr, l);
}

static void FPU_FST_F64(PhysPt addr)
{
	mem_writeq(addr, static_cast<uint64_t>(fpu.regs[TOP].ll));
}

static void FPU_FST_F80(PhysPt addr)
{
	FPU_ST80(addr, TOP);
}

static void FPU_FST_I16(PhysPt addr)
{
	const auto val = FROUND(fpu.regs[TOP].d);
	mem_writew(addr,
	           (val < 32768.0 && val >= -32768.0) ? static_cast<int16_t>(val)
	                                              : 0x8000);
}

static void FPU_FST_I32(PhysPt addr)
{
	const auto val = FROUND(fpu.regs[TOP].d);
	mem_writed(addr,
	           (val < 2147483648.0 && val >= -2147483648.0)
	                   ? static_cast<int32_t>(val)
	                   : 0x80000000);
}

static void FPU_FST_I64(PhysPt addr)
{
	int64_t temp_reg;
	// If the value was loaded with the FILD/FIST 64-bit memcpy trick,
	// read the 64-bit integer value instead.
	if (fpu.regs_memcpy[TOP].has_value()) {
		temp_reg = fpu.regs_memcpy[TOP].value();
	} else {
		double val  = FROUND(fpu.regs[TOP].d);
		temp_reg    = (val <= static_cast<double>(INT64_MAX) &&
                            val >= static_cast<double>(INT64_MIN))
		                    ? static_cast<int64_t>(val)
		                    : LONGTYPE(0x8000000000000000);
	}

	mem_writeq(addr, static_cast<uint64_t>(temp_reg));
}

static void FPU_FBST(PhysPt addr)
{
	FPU_Reg val = fpu.regs[TOP];
	if (val.ll & LONGTYPE(0x8000000000000000)) { // MSB = sign
		mem_writeb(addr + 9, 0x80);
		val.d = -val.d;
	} else {
		mem_writeb(addr + 9, 0);
	}

	auto rndint = static_cast<uint64_t>(FROUND(val.d));
	// BCD (18 decimal digits) overflow? (0x0DE0B6B3A763FFFF max)
	if (rndint > LONGTYPE(999999999999999999)) {
		// write BCD integer indefinite value
		mem_writed(addr + 0, 0);
		mem_writed(addr + 4, 0xC0000000);
		mem_writew(addr + 8, 0xFFFF);
		return;
	}

	static_assert(sizeof(rndint) == sizeof(long long int),
	              "passing rndint to lldiv function");
	// numbers from back to front
	for (int i = 0; i < 9; i++) {
		const lldiv_t div10  = lldiv(rndint, 10);
		const lldiv_t div100 = lldiv(div10.quot, 10);
		const uint8_t p      = static_cast<uint8_t>(div10.rem) |
		                  (static_cast<uint8_t>(div100.rem) << 4);
		mem_writeb(addr++, p);
		rndint = div100.quot;
	}
	// flags? C1 should indicate if value was rounded up
}

static void FPU_FADD(Bitu op1, Bitu op2)
{
	fpu.regs[op1].d += fpu.regs[op2].d;
	// flags and such :)
	return;
}

static void FPU_FSIN(void)
{
	fpu.regs[TOP].d = std::sin(fpu.regs[TOP].d);
	FPU_SET_C2(false);
	// flags and such :)
	return;
}

static void FPU_FSINCOS(void)
{
	Real64 temp     = fpu.regs[TOP].d;
	fpu.regs[TOP].d = std::sin(temp);
	FPU_PUSH(std::cos(temp));
	FPU_SET_C2(false);
	// flags and such :)
	return;
}

static void FPU_FCOS(void)
{
	fpu.regs[TOP].d = std::cos(fpu.regs[TOP].d);
	FPU_SET_C2(false);
	// flags and such :)
	return;
}

static void FPU_FSQRT(void)
{
	fpu.regs[TOP].d = std::sqrt(fpu.regs[TOP].d);
	// flags and such :)
	return;
}
static void FPU_FPATAN(void)
{
	fpu.regs[STV(1)].d = std::atan2(fpu.regs[STV(1)].d, fpu.regs[TOP].d);
	FPU_FPOP();
	// flags and such :)
	return;
}
static void FPU_FPTAN(void)
{
	fpu.regs[TOP].d = std::tan(fpu.regs[TOP].d);
	FPU_PUSH(1.0);
	FPU_SET_C2(false);
	// flags and such :)
	return;
}
static void FPU_FDIV(Bitu st, Bitu other)
{
	fpu.regs[st].d = fpu.regs[st].d / fpu.regs[other].d;
	// flags and such :)
	return;
}

static void FPU_FDIVR(Bitu st, Bitu other)
{
	fpu.regs[st].d = fpu.regs[other].d / fpu.regs[st].d;
	// flags and such :)
	return;
}

static void FPU_FMUL(Bitu st, Bitu other)
{
	fpu.regs[st].d *= fpu.regs[other].d;
	// flags and such :)
	return;
}

static void FPU_FSUB(Bitu st, Bitu other)
{
	fpu.regs[st].d = fpu.regs[st].d - fpu.regs[other].d;
	// flags and such :)
	return;
}

static void FPU_FSUBR(Bitu st, Bitu other)
{
	fpu.regs[st].d = fpu.regs[other].d - fpu.regs[st].d;
	// flags and such :)
	return;
}

static void FPU_FXCH(Bitu st, Bitu other)
{
	const auto tag         = fpu.tags[other];
	const auto reg         = fpu.regs[other];
	const auto reg_memcpy  = fpu.regs_memcpy[other];
	fpu.tags[other]        = fpu.tags[st];
	fpu.regs[other]        = fpu.regs[st];
	fpu.regs_memcpy[other] = fpu.regs_memcpy[st];
	fpu.tags[st]           = tag;
	fpu.regs[st]           = reg;
	fpu.regs_memcpy[st]    = reg_memcpy;
}

static void FPU_FST(Bitu st, Bitu other)
{
	fpu.tags[other]        = fpu.tags[st];
	fpu.regs[other]        = fpu.regs[st];
	fpu.regs_memcpy[other] = fpu.regs_memcpy[st];
}

static void FPU_FCOM(Bitu st, Bitu other)
{
	const bool tags_are_invalid =
	        ((fpu.tags[st] != TAG_Valid && fpu.tags[st] != TAG_Zero) ||
	         (fpu.tags[other] != TAG_Valid && fpu.tags[other] != TAG_Zero));

	if (tags_are_invalid) {
		FPU_SET_C3(true);
		FPU_SET_C2(true);
		FPU_SET_C0(true);
	} else if (fpu.regs[st].d == fpu.regs[other].d) {
		FPU_SET_C3(true);
		FPU_SET_C2(false);
		FPU_SET_C0(false);
	} else if (fpu.regs[st].d < fpu.regs[other].d) {
		FPU_SET_C3(false);
		FPU_SET_C2(false);
		FPU_SET_C0(true);
	} else {
		// st > other
		FPU_SET_C3(false);
		FPU_SET_C2(false);
		FPU_SET_C0(false);
	}
}

static void FPU_FUCOM(Bitu st, Bitu other)
{
	// does atm the same as fcom
	FPU_FCOM(st, other);
}

static void FPU_FRNDINT(void)
{
	const auto rounded = FROUND(fpu.regs[TOP].d);
	if (fpu.cw & 0x20) { // As we don't generate exceptions; only do it when
		             // masked
		if (rounded != fpu.regs[TOP].d) {
			fpu.sw |= 0x20; // Set Precision Exception
		}
	}
	fpu.regs[TOP].d = rounded;
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

static void FPU_FXAM(void)
{
	if (fpu.regs[TOP].ll & LONGTYPE(0x8000000000000000)) // sign
	{
		FPU_SET_C1(true);
	} else {
		FPU_SET_C1(false);
	}

	if (fpu.tags[TOP] == TAG_Empty) {
		FPU_SET_C3(true);
		FPU_SET_C2(false);
		FPU_SET_C0(true);
		return;
	}

	if (fpu.regs[TOP].d == 0.0) // zero or normalized number.
	{
		FPU_SET_C3(true);
		FPU_SET_C2(false);
		FPU_SET_C0(false);
	} else {
		FPU_SET_C3(false);
		FPU_SET_C2(true);
		FPU_SET_C0(false);
	}
}

static void FPU_F2XM1(void)
{
	fpu.regs[TOP].d = std::pow(2.0, fpu.regs[TOP].d) - 1.0;
	return;
}

static void FPU_FYL2X(void)
{
	fpu.regs[STV(1)].d *= std::log2(fpu.regs[TOP].d);
	FPU_FPOP();
	return;
}

static void FPU_FYL2XP1(void)
{
	fpu.regs[STV(1)].d *= std::log2(fpu.regs[TOP].d + 1.0);
	FPU_FPOP();
	return;
}

static void FPU_FSCALE(void){
	fpu.regs[TOP].d *= std::pow(2.0, std::trunc(fpu.regs[STV(1)].d));
	//FPU_SET_C1(0);
	return; //2^x where x is chopped.
}

static void FPU_FSTENV(PhysPt addr)
{
	FPU_SET_TOP(TOP);

	if (!cpu.code.big) {
		mem_writew(addr + 0, static_cast<uint16_t>(fpu.cw));
		mem_writew(addr + 2, static_cast<uint16_t>(fpu.sw));
		mem_writew(addr + 4, static_cast<uint16_t>(FPU_GetTag()));
	} else {
		mem_writed(addr + 0, static_cast<uint32_t>(fpu.cw));
		mem_writed(addr + 4, static_cast<uint32_t>(fpu.sw));
		mem_writed(addr + 8, static_cast<uint32_t>(FPU_GetTag()));
	}
}

static void FPU_FLDENV(PhysPt addr)
{
	uint16_t tag;
	uint32_t tagbig;
	uint16_t cw;
	uint16_t sw;

	if (!cpu.code.big) {
		cw  = mem_readw(addr + 0);
		sw  = mem_readw(addr + 2);
		tag = mem_readw(addr + 4);
	} else {
		cw     = mem_readd(addr + 0);
		sw     = static_cast<uint16_t>(mem_readd(addr + 4));
		tagbig = mem_readd(addr + 8);
		tag    = static_cast<uint16_t>(tagbig);
	}

	FPU_SetTag(tag);
	FPU_SetCW(cw);
	FPU_SetSW(sw);
	TOP = FPU_GET_TOP();
}

static void FPU_FSAVE(PhysPt addr)
{
	FPU_FSTENV(addr);
	PhysPt start = (cpu.code.big ? 28 : 14);

	for (int i = 0; i < 8; i++) {
		FPU_ST80(addr + start, STV(i));
		start += 10;
	}

	FPU_FINIT();
}

static void FPU_FRSTOR(PhysPt addr)
{
	FPU_FLDENV(addr);
	PhysPt start = (cpu.code.big ? 28 : 14);

	for (int i = 0; i < 8; i++) {
		fpu.regs[STV(i)].d = FPU_FLD80(addr + start);
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
	Real64 mant = test.d / (std::pow(2.0, static_cast<Real64>(exp80final)));
	fpu.regs[TOP].d = static_cast<Real64>(exp80final);
	FPU_PUSH(mant);
}

static void FPU_FCHS(void)
{
	fpu.regs[TOP].d = -(fpu.regs[TOP].d);
}

static void FPU_FABS(void)
{
	fpu.regs[TOP].d = std::fabs(fpu.regs[TOP].d);
}

static void FPU_FTST(void)
{
	fpu.regs[8].d = 0.0;
	FPU_FCOM(TOP, 8);
}

static void FPU_FLD1(void)
{
	FPU_PREP_PUSH();
	fpu.regs[TOP].d = 1.0;
}

static void FPU_FLDL2T(void)
{
	constexpr auto L2T = M_LN10 / M_LN2;
	FPU_PREP_PUSH();
	fpu.regs[TOP].d = L2T;
}

static void FPU_FLDL2E(void)
{
	FPU_PREP_PUSH();
	fpu.regs[TOP].d = M_LOG2E;
}

static void FPU_FLDPI(void)
{
	FPU_PREP_PUSH();
	fpu.regs[TOP].d = M_PI;
}

static void FPU_FLDLG2(void)
{
	constexpr auto LG2 = M_LOG10E / M_LOG2E;
	FPU_PREP_PUSH();
	fpu.regs[TOP].d = LG2;
}

static void FPU_FLDLN2(void)
{
	FPU_PREP_PUSH();
	fpu.regs[TOP].d = M_LN2;
}

static void FPU_FLDZ(void)
{
	FPU_PREP_PUSH();
	fpu.regs[TOP].d = 0.0;
	fpu.tags[TOP]   = TAG_Zero;
	fpu.regs_memcpy[TOP].reset();
}

static inline void FPU_FADD_EA(Bitu op1)
{
	FPU_FADD(op1, 8);
}
static inline void FPU_FMUL_EA(Bitu op1)
{
	FPU_FMUL(op1, 8);
}
static inline void FPU_FSUB_EA(Bitu op1)
{
	FPU_FSUB(op1, 8);
}
static inline void FPU_FSUBR_EA(Bitu op1)
{
	FPU_FSUBR(op1, 8);
}
static inline void FPU_FDIV_EA(Bitu op1)
{
	FPU_FDIV(op1, 8);
}
static inline void FPU_FDIVR_EA(Bitu op1)
{
	FPU_FDIVR(op1, 8);
}
static inline void FPU_FCOM_EA(Bitu op1)
{
	FPU_FCOM(op1, 8);
}
