// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_FPU_H
#include "fpu/fpu.h"
#endif

#include "utils/math_utils.h"
#include <bit>

static constexpr uint16_t PrecisionModeMask = 0x0300;

static constexpr uint16_t SinglePrecisionMode   = 0x0000;
static constexpr uint16_t DoublePrecisionMode   = 0x0200;
static constexpr uint16_t ExtendedPrecisionMode = 0x0300;

static constexpr uint16_t InvalidArithmeticFlag = 0x0001;
static constexpr uint16_t ZeroDivideFlag        = 0x0004;
static constexpr uint16_t PrecisionFlag         = 0x0020;

static void FPU_FINIT(void)
{
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
	if (fpu.tags[TOP] != TAG_Empty) {
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
	if (fpu.tags[TOP] == TAG_Empty) {
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
	fpu.tags[TOP] = TAG_Empty;
	//maybe set zero in it as well
	TOP = ((TOP+1)&7);
//	LOG(LOG_FPU,LOG_ERROR)("popped from %d  %g off the stack",top,fpu.regs[top].d);
	return;
}

static inline double FROUND(double in)
{
	switch (fpu.round) {
	case ROUND_Nearest: return std::nearbyint(in);
	case ROUND_Down: return std::floor(in);
	case ROUND_Up: return std::ceil(in);
	case ROUND_Chop: return std::trunc(in);
	default: return in;
	}
}

#define BIAS80 16383
#define BIAS64 1023

// Rounds sig to nearest even, shifting right by s bits
static inline uint64_t round_to_nearest_even(uint64_t sig, unsigned s)
{
	if (s == 0) {
		return sig;
	}

	const bool big     = (s >= 64);
	const uint64_t hi  = big ? 0 : (sig >> s);
	const uint64_t rem = big ? sig : (sig & ((1ULL << s) - 1));
	if (!rem) {
		return hi;
	}

	const uint64_t half = big ? (1ULL << 63) : (1ULL << (s - 1));
	const uint64_t lsb  = hi & 1ULL;
	const bool round_up = (rem > half) || (rem == half && lsb);
	return hi + (round_up ? 1ULL : 0ULL);
}

static Real64 FPU_FLD80(PhysPt addr)
{
	const auto raw_mant = mem_readq(addr);
	const auto raw_exp  = mem_readw(addr + 8);

	const auto sign  = static_cast<uint16_t>(raw_exp) >> 15;
	const auto exp80 = static_cast<uint16_t>(raw_exp) & 0x7fff;
	// J bit is at bit 63 for normals
	uint64_t sig64 = raw_mant;

	uint64_t out = 0;

	// Special cases
	if (exp80 == 0x7fff) {
		// Inf or NaN
		// J==1, frac==0
		const bool is_inf = (sig64 & 0x7fffffffffffffffULL) == 0;
		if (is_inf) {
			out = (uint64_t(sign) << 63) | (0x7ffULL << 52);
			return std::bit_cast<double>(out);
		}
		// Quiet NaN
		out = (uint64_t(sign) << 63) | (0x7ffULL << 52) | (1ULL << 51);
		return std::bit_cast<double>(out);
	}

	if (exp80 == 0) {
		// Zero or subnormal (J==0)
		if (sig64 == 0) {
			// signed zero
			out = (uint64_t(sign) << 63);
			return std::bit_cast<double>(out);
		}
		// Normalize the subnormal significand so that J lands in bit 63
		const auto lz = std::countl_zero(sig64);
		sig64 <<= lz;
		// Unbiased exponent for subnormals is 1 - BIAS80, minus the shifts
		int64_t E = (1 - BIAS80) - lz + BIAS64;

		// Will be subnormal or underflow to zero in double
		if (E <= 0) {
			// Shift so that the implicit 1 would align just above
			// the fraction
			const auto s = static_cast<unsigned>(12 - E); // 11 + (1
			                                              // - E)
			uint64_t q = round_to_nearest_even(sig64, s);
			if (q == 0) {
				// underflow to signed zero
				out = (uint64_t(sign) << 63);
				return std::bit_cast<double>(out);
			}
			// If rounding created a carry into the hidden-1 place,
			// it became the smallest normal
			if (q == (1ULL << 53)) {
				out = (uint64_t(sign) << 63) |
				      (1ULL << 52); // exponent=1, mantissa=0
				return std::bit_cast<double>(out);
			}
			const uint64_t mant = q & ((1ULL << 52) - 1);
			// exponent=0 (subnormal)
			out = (uint64_t(sign) << 63) | mant;
			return std::bit_cast<double>(out);
		}
		// Else it turned normal; fall through to normal path with E >= 1
		int64_t exp64    = E;
		const auto sig53 = round_to_nearest_even(sig64, 11);
		uint64_t mant    = sig53 & ((1ULL << 52) - 1);
		// Carry from rounding?
		if (sig53 == (1ULL << 53)) {
			mant = 0;
			++exp64;
		}
		if (exp64 >= 0x7ff) {
			out = (uint64_t(sign) << 63) | (0x7ffULL << 52);
			return std::bit_cast<double>(out);
		}
		out = (uint64_t(sign) << 63) |
		      (static_cast<uint64_t>(exp64) << 52) | mant;
		return std::bit_cast<double>(out);
	}

	// Normal finite: ensure J bit set (defensive against pseudo-denorms)
	if ((sig64 >> 63) == 0) {
		sig64 |= (1ULL << 63);
	}

	auto exp64 = static_cast<int64_t>(exp80) - BIAS80 + BIAS64;

	// Subnormal result?
	if (exp64 <= 0) {
		// 11 + (1 - exp64)
		const unsigned s = static_cast<unsigned>(12 - exp64);
		uint64_t q       = round_to_nearest_even(sig64, s);
		if (q == 0) {
			out = (uint64_t(sign) << 63);
			return std::bit_cast<double>(out);
		}
		if (q == (1ULL << 53)) {
			// Rounded up to the smallest normal
			out = (uint64_t(sign) << 63) | (1ULL << 52);
			return std::bit_cast<double>(out);
		}
		const uint64_t mant = q & ((1ULL << 52) - 1);
		// exponent=0
		out = (uint64_t(sign) << 63) | mant;
		return std::bit_cast<double>(out);
	}

	// Normal result
	const uint64_t sig53 = round_to_nearest_even(sig64, 11);
	uint64_t mant        = sig53 & ((1ULL << 52) - 1);

	// Carry from rounding?
	if (sig53 == (1ULL << 53)) {
		mant = 0;
		++exp64;
	}
	// Overflow?
	if (exp64 >= 0x7ff) {
		out = (uint64_t(sign) << 63) | (0x7ffULL << 52);
		return std::bit_cast<double>(out);
	}

	out = (uint64_t(sign) << 63) | (static_cast<uint64_t>(exp64) << 52) | mant;

	return std::bit_cast<double>(out);
}

static void FPU_ST80(PhysPt addr, Bitu reg)
{
	const uint64_t val64 = fpu.regs[reg].ll;

	const uint64_t sign64 = val64 >> 63;
	const uint64_t exp64  = (val64 >> 52) & 0x7ff;
	const uint64_t mant64 = val64 & ((1ULL << 52) - 1);

	uint64_t mant80;
	uint16_t exp80;

	if (exp64 == 0x7ff) {
		// Infinity or NaN
		exp80 = 0x7fff;
		if (mant64 == 0) {
			// Infinity - set J bit, clear fraction
			mant80 = 1ULL << 63;
		} else {
			// NaN - set J bit and quiet bit, preserve some fraction
			// bits
			mant80 = (1ULL << 63) | (1ULL << 62) | (mant64 << 11);
		}
	} else if (exp64 == 0) {
		if (mant64 == 0) {
			// Zero
			exp80  = 0;
			mant80 = 0;
		} else {
			// Subnormal - normalize it
			// align to bit 63
			const auto lz = std::countl_zero(mant64 << 12);
			// set J bit
			mant80 = (mant64 << (12 + lz)) | (1ULL << 63);
			exp80 = static_cast<uint16_t>(1 - BIAS64 + BIAS80 - lz);
		}
	} else {
		// Normal number
		exp80 = static_cast<uint16_t>(exp64 - BIAS64 + BIAS80);
		// Set J bit (implicit 1) and shift mantissa
		mant80 = (1ULL << 63) | (mant64 << 11);
	}

	// Combine sign and exponent for the high word
	const uint16_t high_word = (static_cast<uint16_t>(sign64) << 15) | exp80;

	mem_writeq(addr, mant80);
	mem_writew(addr + 8, high_word);
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
	fpu.regs[store_to].ll = mem_readq(addr);
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

static void FPU_FLD_I64(PhysPt addr, Bitu store_to)
{
	const int64_t val = mem_readq(addr);

	// This is a fix for vertical bars that appear in some games that
	// use FILD/FIST as a fast 64-bit memcpy, such as Carmageddon,
	// Motor Mash, and demos like Sunflower and Multikolor. Native x87
	// registers are 80-bit with a 64-bit mantissa, but a C++
	// double only has a 53-bit mantissa, so store the full 64-bit
	// integer value in regs_memcpy to be used by the FPU_FST_I64()
	// function.
	fpu.regs[store_to].d      = static_cast<Real64>(val);
	fpu.regs_memcpy[store_to] = val;
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
	mem_writeq(addr, fpu.regs[TOP].ll);
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

static void FPU_FST_I64(PhysPt addr)
{
	constexpr double MaxInt64Value = 9223372036854775808.0;

	// Handle the 64-bit memcpy trick. See comment in FPU_FLD_I64().
	auto val_i       = fpu.regs_memcpy[TOP];
	const auto val_d = fpu.regs[TOP].d;

	if (val_d != static_cast<Real64>(val_i)) {
		const auto rounded = FROUND(val_d);
		val_i = (rounded < MaxInt64Value && rounded >= -MaxInt64Value)
		              ? static_cast<int64_t>(rounded)
		              : LONGTYPE(0x8000000000000000);
	}

	mem_writeq(addr, val_i);
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
	fpu.regs[TOP].d = std::sin(fpu.regs[TOP].d);
	FPU_SET_C2(0);
	//flags and such :)
	return;
}

static void FPU_FSINCOS(void){
	Real64 temp = fpu.regs[TOP].d;
	fpu.regs[TOP].d = std::sin(temp);
	FPU_PUSH(std::cos(temp));
	FPU_SET_C2(0);
	//flags and such :)
	return;
}

static void FPU_FCOS(void){
	fpu.regs[TOP].d = std::cos(fpu.regs[TOP].d);
	FPU_SET_C2(0);
	//flags and such :)
	return;
}

static void FPU_FSQRT(void){
	const auto val = fpu.regs[TOP].d;

	if (val < 0.0) {
		fpu.sw |= InvalidArithmeticFlag;
		fpu.regs[TOP].d = std::numeric_limits<double>::quiet_NaN();
		return;
	}

	fpu.regs[TOP].d = std::sqrt(val);
	// flags and such :)
	return;
}
static void FPU_FPATAN(void){
	fpu.regs[STV(1)].d = std::atan2(fpu.regs[STV(1)].d, fpu.regs[TOP].d);
	FPU_FPOP();
	//flags and such :)
	return;
}
static void FPU_FPTAN(void){
	fpu.regs[TOP].d = std::tan(fpu.regs[TOP].d);
	FPU_PUSH(1.0);
	FPU_SET_C2(0);
	//flags and such :)
	return;
}

static void FPU_FDIV(Bitu st, Bitu other){
	const auto a = fpu.regs[st].d;
	const auto b = fpu.regs[other].d;

	if (b != 0.0 && std::isfinite(a) && std::isfinite(b)) {
		fpu.regs[st].d = a / b;
		return;
	}

	if (b == 0.0 && (std::isfinite(a) && a != 0.0)) {
		fpu.sw |= ZeroDivideFlag;
		fpu.regs[st].d = std::copysign(
		        std::numeric_limits<double>::infinity(),
		        (std::signbit(a) ^ std::signbit(b)) ? -1.0 : 1.0);
		return;
	}

	if (std::isnan(a) || std::isnan(b)) {
		fpu.regs[st].d = std::numeric_limits<double>::quiet_NaN();
		return;
	}

	if ((a == 0.0 && b == 0.0) || (std::isinf(a) && std::isinf(b))) {
		fpu.sw |= InvalidArithmeticFlag;
		fpu.regs[st].d = std::numeric_limits<double>::quiet_NaN();
		return;
	}

	fpu.regs[st].d = a / b;
}

static void FPU_FDIVR(Bitu st, Bitu other){
	const auto a = fpu.regs[other].d;
	const auto b = fpu.regs[st].d;

	if (b != 0.0 && std::isfinite(a) && std::isfinite(b)) {
		fpu.regs[st].d = a / b;
		return;
	}

	if (b == 0.0 && (std::isfinite(a) && a != 0.0)) {
		fpu.sw |= ZeroDivideFlag;
		fpu.regs[st].d = std::copysign(
		        std::numeric_limits<double>::infinity(),
		        (std::signbit(a) ^ std::signbit(b)) ? -1.0 : 1.0);
		return;
	}

	if (std::isnan(a) || std::isnan(b)) {
		fpu.regs[st].d = std::numeric_limits<double>::quiet_NaN();
		return;
	}

	if ((a == 0.0 && b == 0.0) || (std::isinf(a) && std::isinf(b))) {
		fpu.sw |= InvalidArithmeticFlag;
		fpu.regs[st].d = std::numeric_limits<double>::quiet_NaN();
		return;
	}

	fpu.regs[st].d = a / b;
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
	const auto tag             = fpu.tags[other];
	const auto reg             = fpu.regs[other];
	const auto reg_memcpy      = fpu.regs_memcpy[other];
	fpu.tags[other]            = fpu.tags[st];
	fpu.regs[other]            = fpu.regs[st];
	fpu.regs_memcpy[other]     = fpu.regs_memcpy[st];
	fpu.tags[st]               = tag;
	fpu.regs[st]               = reg;
	fpu.regs_memcpy[st]        = reg_memcpy;
}

static void FPU_FST(Bitu st, Bitu other){
	fpu.tags[other]            = fpu.tags[st];
	fpu.regs[other]            = fpu.regs[st];
	fpu.regs_memcpy[other]     = fpu.regs_memcpy[st];
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

static void FPU_FRNDINT(void)
{
	const auto val = fpu.regs[TOP].d;
	if (std::isnan(val) || std::isinf(val)) {
		return;
	}
	const auto rounded = FROUND(val);
	if (rounded != val) {
		fpu.sw |= PrecisionFlag;
	}
	FPU_SET_C1(rounded > val ? 1 : 0);
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
	fpu.regs[TOP].d = std::pow(2.0, fpu.regs[TOP].d) - 1.0;
	return;
}

static void FPU_FYL2X(void){
	fpu.regs[STV(1)].d *= std::log2(fpu.regs[TOP].d);
	FPU_FPOP();
	return;
}

static void FPU_FYL2XP1(void){
	fpu.regs[STV(1)].d *= std::log2(fpu.regs[TOP].d + 1.0);
	FPU_FPOP();
	return;
}

static void FPU_FSCALE(void){
	fpu.regs[TOP].d *= std::pow(2.0, std::trunc(fpu.regs[STV(1)].d));
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
		fpu.sw = static_cast<uint16_t>(mem_readd(addr + 4));
		tagbig = mem_readd(addr+8);
		tag    = static_cast<uint16_t>(tagbig);
	}
	FPU_SetTag(tag);
	FPU_SetCW(cw);
	TOP = FPU_GET_TOP();
}

static void FPU_FSAVE(PhysPt addr){
	FPU_FSTENV(addr);
	PhysPt start = (cpu.code.big ? 28 : 14);
	for (int i = 0; i < 8; i++) {
		FPU_ST80(addr + start, STV(i));
		start += 10;
	}
	FPU_FINIT();
}

static void FPU_FRSTOR(PhysPt addr){
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

static void FPU_FCHS(void){
	fpu.regs[TOP].d = -(fpu.regs[TOP].d);
}

static void FPU_FABS(void){
	fpu.regs[TOP].d = std::fabs(fpu.regs[TOP].d);
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
	constexpr auto L2T = M_LN10 / M_LN2;
	FPU_PREP_PUSH();
	fpu.regs[TOP].d = L2T;
}

static void FPU_FLDL2E(void){
	FPU_PREP_PUSH();
	fpu.regs[TOP].d = M_LOG2E;
}

static void FPU_FLDPI(void){
	FPU_PREP_PUSH();
	fpu.regs[TOP].d = M_PI;
}

static void FPU_FLDLG2(void){
	constexpr auto LG2 = M_LOG10E / M_LOG2E;
	FPU_PREP_PUSH();
	fpu.regs[TOP].d = LG2;
}

static void FPU_FLDLN2(void){
	FPU_PREP_PUSH();
	fpu.regs[TOP].d = M_LN2;
}

static void FPU_FLDZ(void){
	FPU_PREP_PUSH();
	fpu.regs[TOP].d = 0.0;
	fpu.tags[TOP]   = TAG_Zero;
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

