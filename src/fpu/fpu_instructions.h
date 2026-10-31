// SPDX-FileCopyrightText: 2002-2021 The DOSBox Team
// SPDX-FileCopyrightText: 2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_FPU_H
#include "fpu/fpu.h"
#endif

// Intel SDM Vol. 1, chapter 8, and Vol. 2, x87 instruction descriptions.
// SoftFloat provides arithmetic; x87 operand classification, stack semantics,
// status flags, constants, and instruction-specific precision are handled here.
using namespace softfloat;
using FPU_Float                  = x87::Float;
static bool fpu_discard_push     = false;
static bool fpu_prepared_push    = false;
static bool fpu_operand_denormal = false;
static bool fpu_operand_nan      = false;
static bool fpu_operand_invalid  = false;

static bool fpu_instruction_committed = false;

static void FPU_begin(bool arithmetic = false)
{
	softfloat_extF80_exceptionMask        = fpu.cw & 0x3f;
	static constexpr uint_fast8_t modes[] = {softfloat_round_near_even,
	                                         softfloat_round_min,
	                                         softfloat_round_max,
	                                         softfloat_round_minMag};
	softfloat_roundingMode                = modes[fpu.round];
	softfloat_exceptionFlags              = 0;
	softfloat_detectTininess = softfloat_tininess_afterRounding;
	fpu_operand_denormal = fpu_operand_nan = fpu_operand_invalid = false;
	fpu_instruction_committed                                    = false;
	const auto pc            = (fpu.cw >> 8) & 3;
	extF80_roundingPrecision = arithmetic ? (pc == 0   ? 32
	                                         : pc == 2 ? 64
	                                                   : 80)
	                                      : 80;
	if (!fpu_discard_push) {
		FPU_SET_C1(false);
	}
}

static bool FPU_exceptions(uint16_t flags)
{
	fpu.sw |= flags;
	if (flags & ~fpu.cw & 0x3f) {
		fpu.sw |= 0x8080; // ES and B: pending unmasked exception
		return false;
	}
	return true;
}

static bool FPU_finish(bool memory_destination = false)
{
	if (memory_destination &&
	    (((softfloat_exceptionFlags & softfloat_flag_overflow) && !(fpu.cw & 8)) ||
	     ((softfloat_exceptionFlags & softfloat_flag_underflow) &&
	      !(fpu.cw & 16)))) {
		softfloat_exceptionFlags &= ~softfloat_flag_inexact;
		FPU_SET_C1(false);
	}
	auto flags          = softfloat_exceptionFlags;
	const bool denormal = fpu_operand_denormal && !fpu_operand_nan &&
	                      !(flags & (softfloat_flag_invalid |
	                                 softfloat_flag_infinite));
	if (denormal && !(fpu.cw & 2)) {
		return FPU_exceptions(2);
	}
	const bool masked = FPU_exceptions(
	        (denormal ? 2 : 0) | ((flags & softfloat_flag_invalid) ? 1 : 0) |
	        ((flags & softfloat_flag_infinite) ? 4 : 0) |
	        ((flags & softfloat_flag_overflow) ? 8 : 0) |
	        ((flags & softfloat_flag_underflow) ? 16 : 0) |
	        ((flags & softfloat_flag_inexact) ? 32 : 0));
	// An unmasked precision exception still writes the rounded result.
	return masked || !(fpu.sw & ~fpu.cw & 0x1f);
}

static FPU_Tag FPU_classify(FPU_Float a)
{
	if (x87::is_zero(a)) {
		return TAG_Zero;
	}
	if (x87::exponent(a) == 0 || x87::exponent(a) == 0x7fff ||
	    x87::unsupported(a)) {
		return TAG_Weird;
	}
	return TAG_Valid;
}

static void FPU_put(Bitu reg, FPU_Float a)
{
	if (fpu_discard_push && reg == TOP) {
		fpu_discard_push  = false;
		fpu_prepared_push = false;
		return;
	}
	if (reg == TOP) {
		fpu_prepared_push = false;
	}
	fpu.regs[reg].value = a;
	fpu.tags[reg]       = FPU_classify(a);
}

static FPU_Float FPU_operand(Bitu reg)
{
	if (fpu.tags[reg] == TAG_Empty) {
		FPU_exceptions(0x41);
		fpu_operand_invalid = true;
		return x87::indefinite();
	}
	auto a = fpu.regs[reg].value;
	if (x87::unsupported(a)) {
		softfloat_raiseFlags(softfloat_flag_invalid);
		fpu_operand_invalid = true;
		return x87::indefinite();
	}
	if (x87::exponent(a) == 0 && a.signif) {
		fpu_operand_denormal = true;
	}
	if (x87::nan(a)) {
		fpu_operand_nan = true;
	}
	// Canonicalize pseudo-denormals; Intel treats them as the corresponding
	// normal number, with a denormal-operand exception (SDM 8.2.2).
	if (x87::exponent(a) == 0 && (a.signif >> 63)) {
		++a.signExp;
	}
	return a;
}

static void FPU_result(Bitu reg, FPU_Float a)
{
	FPU_finish();
	if (!(fpu.sw & ~fpu.cw & 0x07)) {
		FPU_put(reg, fpu_operand_invalid ? x87::indefinite() : a);
		fpu_instruction_committed = true;
	} else if (fpu_prepared_push) {
		fpu.tags[TOP]     = TAG_Empty;
		TOP               = (TOP + 1) & 7;
		fpu_prepared_push = false;
	}
}

static void FPU_FINIT()
{
	FPU_SetCW(0x37f);
	fpu.sw = 0;
	TOP    = 0;
	for (int i = 0; i < 8; ++i) {
		fpu.tags[i] = TAG_Empty;
	}
	fpu.tags[8]       = TAG_Valid;
	fpu_discard_push  = false;
	fpu_prepared_push = false;
}
static void FPU_FCLEX()
{
	fpu.sw &= 0x7f00;
}
static void FPU_FNOP() {}
static void FPU_PREP_PUSH()
{
	fpu_discard_push  = false;
	fpu_prepared_push = false;
	FPU_SET_C1(false);
	const auto next = (TOP - 1) & 7;
	if (fpu.tags[next] != TAG_Empty) {
		const bool masked = FPU_exceptions(0x41);
		FPU_SET_C1(true);
		if (masked) {
			TOP = next;
			FPU_put(TOP, x87::indefinite());
		}
		fpu_discard_push = true;
		return;
	}
	TOP               = next;
	fpu_prepared_push = true;
	fpu.tags[TOP]     = TAG_Valid;
}
static void FPU_PUSH(FPU_Float a)
{
	FPU_PREP_PUSH();
	FPU_put(TOP, a);
}
static void FPU_FPOP()
{
	if ((fpu.sw & ~fpu.cw & 0x1f) && !fpu_instruction_committed) {
		return;
	}
	if (fpu.tags[TOP] == TAG_Empty) {
		FPU_SET_C1(false);
		if (!FPU_exceptions(0x41)) {
			return;
		}
	}
	fpu.tags[TOP] = TAG_Empty;
	TOP           = (TOP + 1) & 7;
}

static FPU_Float FPU_FLD80(PhysPt addr)
{
	return x87::raw(mem_readw(addr + 8), mem_readq(addr));
}
static void FPU_ST80(PhysPt addr, Bitu reg)
{
	const auto a = fpu.regs[reg].value;
	mem_writeq(addr, a.signif);
	mem_writew(addr + 8, a.signExp);
}
static void FPU_FLD_F80(PhysPt addr)
{
	FPU_put(TOP, FPU_FLD80(addr));
}
static void FPU_FLD_F32(PhysPt addr, Bitu reg)
{
	FPU_begin();
	softfloat::float32_t a = {mem_readd(addr)};
	if (!(a.v & 0x7f800000) && (a.v & 0x7fffff)) {
		FPU_exceptions(2);
	}
	FPU_result(reg, f32_to_extF80(a));
}
static void FPU_FLD_F64(PhysPt addr, Bitu reg)
{
	FPU_begin();
	softfloat::float64_t a = {mem_readq(addr)};
	if (!(a.v & UINT64_C(0x7ff0000000000000)) &&
	    (a.v & UINT64_C(0xfffffffffffff))) {
		FPU_exceptions(2);
	}
	FPU_result(reg, f64_to_extF80(a));
}
static void FPU_FLD_I16(PhysPt addr, Bitu reg)
{
	FPU_put(reg, i32_to_extF80(static_cast<int16_t>(mem_readw(addr))));
}
static void FPU_FLD_I32(PhysPt addr, Bitu reg)
{
	FPU_put(reg, i32_to_extF80(static_cast<int32_t>(mem_readd(addr))));
}
static void FPU_FLD_I64(PhysPt addr, Bitu reg)
{
	FPU_put(reg, i64_to_extF80(std::bit_cast<int64_t>(mem_readq(addr))));
}
static void FPU_FLD_F32_EA(PhysPt addr)
{
	FPU_FLD_F32(addr, 8);
}
static void FPU_FLD_F64_EA(PhysPt addr)
{
	FPU_FLD_F64(addr, 8);
}
static void FPU_FLD_I16_EA(PhysPt addr)
{
	FPU_FLD_I16(addr, 8);
}
static void FPU_FLD_I32_EA(PhysPt addr)
{
	FPU_FLD_I32(addr, 8);
}

static void FPU_FST_F32(PhysPt addr)
{
	FPU_begin();
	const auto input     = FPU_operand(TOP);
	fpu_operand_denormal = false; // Stores do not raise denormal-operand
	                              // exceptions.
	const auto a         = extF80_to_f32(input);
	if (!(fpu.cw & 16) && !(a.v & 0x7f800000) && !x87::is_zero(input)) {
		softfloat_raiseFlags(softfloat_flag_underflow);
	}
	if ((softfloat_exceptionFlags & softfloat_flag_inexact) &&
	    !(softfloat_exceptionFlags & softfloat_flag_invalid)) {
		FPU_SET_C1(extF80_lt(x87::abs(input), x87::abs(f32_to_extF80(a))));
	}
	if (FPU_finish(true) && !(fpu.sw & ~fpu.cw & 0x1f)) {
		mem_writed(addr, a.v);
	}
}
static void FPU_FST_F64(PhysPt addr)
{
	FPU_begin();
	const auto input     = FPU_operand(TOP);
	fpu_operand_denormal = false; // Stores do not raise denormal-operand
	                              // exceptions.
	const auto a         = extF80_to_f64(input);
	if (!(fpu.cw & 16) && !(a.v & UINT64_C(0x7ff0000000000000)) &&
	    !x87::is_zero(input)) {
		softfloat_raiseFlags(softfloat_flag_underflow);
	}
	if ((softfloat_exceptionFlags & softfloat_flag_inexact) &&
	    !(softfloat_exceptionFlags & softfloat_flag_invalid)) {
		FPU_SET_C1(extF80_lt(x87::abs(input), x87::abs(f64_to_extF80(a))));
	}
	if (FPU_finish(true) && !(fpu.sw & ~fpu.cw & 0x1f)) {
		mem_writeq(addr, a.v);
	}
}
static void FPU_FST_F80(PhysPt addr)
{
	FPU_begin();
	if (fpu.tags[TOP] == TAG_Empty) {
		if (FPU_exceptions(0x41)) {
			mem_writeq(addr, UINT64_C(0xc000000000000000));
			mem_writew(addr + 8, 0xffff);
		}
	} else {
		FPU_ST80(addr, TOP);
	}
}
static int64_t FPU_integer(int width)
{
	const auto a         = FPU_operand(TOP);
	fpu_operand_denormal = false;
	const auto out       = extF80_to_i64(a, softfloat_roundingMode, true);
	if (!(softfloat_exceptionFlags & softfloat_flag_invalid)) {
		const auto rounded = extF80_roundToInt(a, softfloat_roundingMode, false);
		FPU_SET_C1(extF80_lt(x87::abs(a), x87::abs(rounded)));
	}
	if (width < 64 && (out < -(INT64_C(1) << (width - 1)) ||
	                   out >= (INT64_C(1) << (width - 1)))) {
		softfloat_exceptionFlags = softfloat_flag_invalid;
		FPU_SET_C1(false);
		return -(INT64_C(1) << (width - 1));
	}
	if (softfloat_exceptionFlags & softfloat_flag_invalid) {
		softfloat_exceptionFlags = softfloat_flag_invalid;
		return INT64_MIN;
	}
	return out;
}
static void FPU_FST_I16(PhysPt addr)
{
	FPU_begin();
	const auto n = FPU_integer(16);
	if (FPU_finish() && !(fpu.sw & ~fpu.cw & 0x1f)) {
		mem_writew(addr, n);
	}
}
static void FPU_FST_I32(PhysPt addr)
{
	FPU_begin();
	const auto n = FPU_integer(32);
	if (FPU_finish() && !(fpu.sw & ~fpu.cw & 0x1f)) {
		mem_writed(addr, n);
	}
}
static void FPU_FST_I64(PhysPt addr)
{
	FPU_begin();
	const auto n = FPU_integer(64);
	if (FPU_finish() && !(fpu.sw & ~fpu.cw & 0x1f)) {
		mem_writeq(addr, n);
	}
}
static void FPU_FBLD(PhysPt addr, Bitu reg)
{
	uint64_t n = 0;
	for (int i = 8; i >= 0; --i) {
		auto b = mem_readb(addr + i);
		n      = n * 100 + (b >> 4) * 10 + (b & 15);
	}
	auto a = ui64_to_extF80(n);
	if (mem_readb(addr + 9) & 0x80) {
		a = x87::negate(a);
	}
	FPU_put(reg, a);
}
static void FPU_FBST(PhysPt addr)
{
	FPU_begin();
	const auto a         = FPU_operand(TOP);
	fpu_operand_denormal = false;
	// Round with the original sign before taking the magnitude (RC matters).
	const auto rounded = extF80_roundToInt(a, softfloat_roundingMode, true);
	auto n = extF80_to_ui64(x87::abs(rounded), softfloat_round_minMag, false);
	if (n > UINT64_C(999999999999999999)) {
		softfloat_exceptionFlags = softfloat_flag_invalid;
	}
	const bool invalid = softfloat_exceptionFlags & softfloat_flag_invalid;
	if (invalid) {
		softfloat_exceptionFlags = softfloat_flag_invalid;
	} else {
		FPU_SET_C1(extF80_lt(x87::abs(a), x87::abs(rounded)));
	}
	if (!FPU_finish() || (fpu.sw & ~fpu.cw & 0x1f)) {
		return;
	}
	if (invalid) {
		mem_writeq(addr, UINT64_C(0xc000000000000000));
		mem_writew(addr + 8, 0xffff);
		return;
	}
	for (int i = 0; i < 9; ++i) {
		const auto lo = n % 10;
		n /= 10;
		const auto hi = n % 10;
		n /= 10;
		mem_writeb(addr + i, lo | (hi << 4));
	}
	mem_writeb(addr + 9, x87::sign(a) ? 0x80 : 0);
}

using FPU_BinaryOp = FPU_Float (*)(FPU_Float, FPU_Float);
static void FPU_binary(Bitu dst, Bitu left, Bitu right, FPU_BinaryOp op)
{
	FPU_begin(true);
	const auto a = FPU_operand(left), b = FPU_operand(right);
	const auto out   = op(a, b);
	const auto flags = softfloat_exceptionFlags;
	if ((flags & softfloat_flag_inexact) && !x87::nan(out)) {
		// A second evaluation toward zero identifies a carry in the
		// significand, including negative results and directed rounding.
		const auto mode        = softfloat_roundingMode;
		softfloat_roundingMode = softfloat_round_minMag;
		const auto chopped     = op(a, b);
		FPU_SET_C1(extF80_lt(x87::abs(chopped), x87::abs(out)));
		softfloat_roundingMode   = mode;
		softfloat_exceptionFlags = flags;
	}
	FPU_result(dst, out);
}
static void FPU_FADD(Bitu a, Bitu b)
{
	FPU_binary(a, a, b, extF80_add);
}
static void FPU_FMUL(Bitu a, Bitu b)
{
	FPU_binary(a, a, b, extF80_mul);
}
static void FPU_FSUB(Bitu a, Bitu b)
{
	FPU_binary(a, a, b, extF80_sub);
}
static void FPU_FSUBR(Bitu a, Bitu b)
{
	FPU_binary(a, b, a, extF80_sub);
}
static void FPU_FDIV(Bitu a, Bitu b)
{
	FPU_binary(a, a, b, extF80_div);
}
static void FPU_FDIVR(Bitu a, Bitu b)
{
	FPU_binary(a, b, a, extF80_div);
}
static void FPU_FSQRT()
{
	FPU_begin(true);
	auto a           = FPU_operand(TOP);
	const auto out   = extF80_sqrt(a);
	const auto flags = softfloat_exceptionFlags;
	if ((flags & softfloat_flag_inexact) && !x87::nan(out)) {
		const auto mode        = softfloat_roundingMode;
		softfloat_roundingMode = softfloat_round_minMag;
		FPU_SET_C1(extF80_lt(extF80_sqrt(a), out));
		softfloat_roundingMode   = mode;
		softfloat_exceptionFlags = flags;
	}
	FPU_result(TOP, out);
}
static void FPU_FRNDINT()
{
	FPU_begin();
	const auto a   = FPU_operand(TOP);
	const auto out = extF80_roundToInt(a, softfloat_roundingMode, true);
	if (!x87::nan(out)) {
		FPU_SET_C1(extF80_lt(x87::abs(a), x87::abs(out)));
	}
	FPU_result(TOP, out);
}
static void FPU_FXCH(Bitu a, Bitu b)
{
	FPU_begin();
	const auto av = fpu.tags[a] == TAG_Empty ? FPU_operand(a) : fpu.regs[a].value;
	const auto bv = fpu.tags[b] == TAG_Empty ? FPU_operand(b) : fpu.regs[b].value;
	if (FPU_finish() && !(fpu.sw & ~fpu.cw & 0x1f)) {
		FPU_put(a, bv);
		FPU_put(b, av);
	}
}
static void FPU_FST(Bitu a, Bitu b)
{
	FPU_begin();
	// Register moves preserve unsupported encodings and signaling NaNs.
	const auto value = fpu.tags[a] == TAG_Empty ? FPU_operand(a)
	                                            : fpu.regs[a].value;
	FPU_result(b, value);
}
static void FPU_compare(Bitu a, Bitu b, bool quiet)
{
	FPU_begin();
	const auto av = FPU_operand(a), bv = FPU_operand(b);
	const bool unordered = x87::nan(av) || x87::nan(bv);
	if (unordered &&
	    (!quiet || extF80_isSignalingNaN(av) || extF80_isSignalingNaN(bv))) {
		softfloat_raiseFlags(softfloat_flag_invalid);
	}
	if (!FPU_finish() || (fpu.sw & ~fpu.cw & 0x1f)) {
		return;
	}
	FPU_SET_C3(unordered || extF80_eq(av, bv));
	FPU_SET_C2(unordered);
	FPU_SET_C0(unordered || extF80_lt(av, bv));
}
static void FPU_FCOM(Bitu a, Bitu b)
{
	FPU_compare(a, b, false);
}
static void FPU_FUCOM(Bitu a, Bitu b)
{
	FPU_compare(a, b, true);
}
static void FPU_FTST()
{
	FPU_put(8, x87::zero());
	FPU_FCOM(TOP, 8);
}
static void FPU_FCHS()
{
	FPU_begin();
	const auto a = fpu.tags[TOP] == TAG_Empty ? FPU_operand(TOP)
	                                          : fpu.regs[TOP].value;
	FPU_result(TOP, x87::negate(a));
}
static void FPU_FABS()
{
	FPU_begin();
	const auto a = fpu.tags[TOP] == TAG_Empty ? FPU_operand(TOP)
	                                          : fpu.regs[TOP].value;
	FPU_result(TOP, x87::abs(a));
}
static void FPU_FXAM()
{
	const auto a = fpu.regs[TOP].value;
	FPU_SET_C1(x87::sign(a));
	int code = 0;
	if (fpu.tags[TOP] == TAG_Empty) {
		code = 5;
	} else if (x87::unsupported(a)) {
		code = 0;
	} else if (x87::nan(a)) {
		code = 1;
	} else if (x87::inf(a)) {
		code = 3;
	} else if (x87::is_zero(a)) {
		code = 4;
	} else if (x87::denormal(a)) {
		code = 6;
	} else {
		code = 2;
	}
	FPU_SET_C3(code & 4);
	FPU_SET_C2(code & 2);
	FPU_SET_C0(code & 1);
}

static void FPU_constant(uint16_t exp, uint64_t nearest, bool nearest_above = false)
{
	FPU_PREP_PUSH();
	if (fpu.round == ROUND_Up && !nearest_above) {
		++nearest;
	}
	if ((fpu.round == ROUND_Down || fpu.round == ROUND_Chop) && nearest_above) {
		--nearest;
	}
	FPU_put(TOP, x87::raw(exp, nearest));
}
static void FPU_FLD1()
{
	FPU_PUSH(x87::one());
}
static void FPU_FLDZ()
{
	FPU_PUSH(x87::zero());
}
static void FPU_FLDPI()
{
	FPU_constant(0x4000, UINT64_C(0xc90fdaa22168c235), true);
}
static void FPU_FLDL2T()
{
	FPU_constant(0x4000, UINT64_C(0xd49a784bcd1b8afe), false);
}
static void FPU_FLDL2E()
{
	FPU_constant(0x3fff, UINT64_C(0xb8aa3b295c17f0bc), true);
}
static void FPU_FLDLG2()
{
	FPU_constant(0x3ffd, UINT64_C(0x9a209a84fbcff799), true);
}
static void FPU_FLDLN2()
{
	FPU_constant(0x3ffe, UINT64_C(0xb17217f7d1cf79ac), true);
}

static int FPU_unbiased(FPU_Float& a)
{
	int e = x87::exponent(a) ? x87::exponent(a) - 16383 : -16382;
	if (a.signif) {
		const auto shift = std::countl_zero(a.signif);
		a.signif <<= shift;
		e -= shift;
	}
	return e;
}
static void FPU_remainder(bool nearest)
{
	FPU_begin();
	auto a = FPU_operand(TOP), b = FPU_operand(STV(1));
	if (x87::nan(a) || x87::nan(b)) {
		FPU_result(TOP, extF80_add(a, b));
		return;
	}
	if (x87::inf(a) || x87::is_zero(b)) {
		softfloat_raiseFlags(softfloat_flag_invalid);
		FPU_result(TOP, x87::indefinite());
		return;
	}
	if (x87::inf(b) || x87::is_zero(a)) {
		FPU_SET_C0(false);
		FPU_SET_C3(false);
		FPU_SET_C1(false);
		FPU_SET_C2(false);
		FPU_result(TOP, a);
		return;
	}
	const auto original   = a;
	const auto original_b = b;
	const int ea = FPU_unbiased(a), eb = FPU_unbiased(b);
	int d               = ea - eb;
	const bool partial  = d >= 64;
	const int reduction = partial ? d - 32 : 0;
	d -= reduction;
	uint64_t remainder = a.signif, q = 0;
	bool carry = false;
	if (d >= 0) {
		for (int i = d; i >= 0; --i) {
			const bool bit = carry || remainder >= b.signif;
			if (bit) {
				remainder -= b.signif;
			}
			q     = (q << 1) | bit;
			carry = false;
			if (i) {
				carry = remainder >> 63;
				remainder <<= 1;
			}
		}
	} else {
		// |a/b| < 1; FPREM1 may still select quotient +/-1.
		if (nearest && d == -1 && a.signif > b.signif) {
			auto signed_divisor = original_b;
			signed_divisor.signExp = (signed_divisor.signExp & 0x7fff) |
			                         (original.signExp & 0x8000);
			const auto out = extF80_sub(original, signed_divisor);
			FPU_SET_C0(false);
			FPU_SET_C3(false);
			FPU_SET_C1(true);
			FPU_SET_C2(false);
			FPU_result(TOP, out);
			return;
		}
		FPU_SET_C0(false);
		FPU_SET_C3(false);
		FPU_SET_C1(false);
		FPU_SET_C2(false);
		FPU_result(TOP, original);
		return;
	}
	bool reverse = false;
	if (nearest && !partial) {
		const bool high  = remainder >> 63;
		const auto twice = remainder << 1;
		if (high || twice > b.signif || (twice == b.signif && (q & 1))) {
			remainder = b.signif - remainder;
			++q;
			reverse = true;
		}
	}
	auto out = x87::scale(ui64_to_extF80(remainder), eb + reduction - 63);
	if (x87::sign(original) != reverse) {
		out = x87::negate(out);
	}
	FPU_SET_C2(partial);
	if (!partial) {
		FPU_SET_C0(q & 4);
		FPU_SET_C3(q & 2);
		FPU_SET_C1(q & 1);
	}
	FPU_result(TOP, out);
}
static void FPU_FPREM()
{
	FPU_remainder(false);
}
static void FPU_FPREM1()
{
	FPU_remainder(true);
}
static void FPU_FSCALE()
{
	FPU_begin();
	const auto a = FPU_operand(TOP), b = FPU_operand(STV(1));
	if (x87::nan(a) || x87::nan(b)) {
		FPU_result(TOP, extF80_add(a, b));
		return;
	}
	if (x87::inf(b)) {
		if ((x87::is_zero(a) && !x87::sign(b)) ||
		    (x87::inf(a) && x87::sign(b))) {
			softfloat_raiseFlags(softfloat_flag_invalid);
			FPU_result(TOP, x87::indefinite());
		} else {
			FPU_result(TOP,
			           x87::sign(b) ? x87::raw(a.signExp & 0x8000, 0)
			                        : x87::infinity(x87::sign(a)));
		}
		return;
	}
	if (x87::inf(a) || x87::is_zero(a)) {
		FPU_result(TOP, a);
		return;
	}
	int n;
	if (x87::exponent(b) >= 16383 + 16) {
		n = x87::sign(b) ? -65536 : 65536;
	} else {
		n = static_cast<int>(extF80_to_i32(b, softfloat_round_minMag, false));
	}
	const auto out   = x87::scale(a, n);
	const auto flags = softfloat_exceptionFlags;
	if (flags & softfloat_flag_inexact) {
		const auto mode        = softfloat_roundingMode;
		softfloat_roundingMode = softfloat_round_minMag;
		FPU_SET_C1(extF80_lt(x87::abs(x87::scale(a, n)), x87::abs(out)));
		softfloat_roundingMode   = mode;
		softfloat_exceptionFlags = flags;
	}
	FPU_result(TOP, out);
}
static void FPU_FXTRACT()
{
	FPU_begin();
	auto a = FPU_operand(TOP);
	FPU_Float e;
	if (x87::nan(a)) {
		a = extF80_add(a, x87::zero());
		e = a;
	} else if (x87::is_zero(a)) {
		softfloat_raiseFlags(softfloat_flag_infinite);
		e = x87::infinity(true);
	} else if (x87::inf(a)) {
		e = x87::infinity(false);
	} else {
		const int exponent = FPU_unbiased(a);
		a.signExp          = (a.signExp & 0x8000) | 0x3fff;
		e                  = i32_to_extF80(exponent);
	}
	if (!FPU_finish() || (fpu.sw & ~fpu.cw & 0x1f)) {
		return;
	}
	FPU_put(TOP, e);
	FPU_PUSH(a);
}

static bool FPU_trans_operand(FPU_Float& a)
{
	a = FPU_operand(TOP);
	if (x87::nan(a)) {
		FPU_result(TOP, extF80_add(a, x87::zero()));
		return false;
	}
	return true;
}
static void FPU_trig(int operation)
{
	FPU_begin();
	FPU_Float a;
	if (!FPU_trans_operand(a)) {
		return;
	}
	if (x87::inf(a)) {
		softfloat_raiseFlags(softfloat_flag_invalid);
		FPU_result(TOP, x87::indefinite());
		return;
	}
	if (x87::exponent(a) >= 16383 + 63) {
		FPU_SET_C2(true);
		return;
	}
	FPU_SET_C2(false);
	FPU_Float sine, cosine;
	x87::sincos(a, sine, cosine);
	const bool tiny = x87::exponent(a) < 16383 - 33 && !x87::is_zero(a);
	if (tiny && (operation == 0 || operation == 2) && x87::exponent(sine) == 0) {
		// sin(x) is inexact for nonzero x even when its rounded value
		// equals x. A tiny rounded result must report underflow as well.
		softfloat_raiseFlags(softfloat_flag_underflow);
		if (!(fpu.cw & 16)) {
			// Unmasked #U stores the result with its exponent
			// biased by 24576, before any loss of significand bits
			// to subnormal storage.
			const bool toward_zero = !extF80_eq(sine, a);
			sine                   = x87::scale(a, 24576);
			if (toward_zero) {
				const int ulp_shift = sine.signif == (UINT64_C(1)
				                                      << 63)
				                            ? 64
				                            : 63;
				const auto ulp = x87::raw(static_cast<uint16_t>(
				                                  sine.signExp -
				                                  ulp_shift),
				                          UINT64_C(1) << 63);
				sine           = extF80_sub(sine, ulp);
			}
		}
	}
	if (tiny && operation == 0) {
		const auto rc = softfloat_roundingMode;
		const bool toward_zero = rc == softfloat_round_minMag ||
		                         rc == (x87::sign(a) ? softfloat_round_max
		                                             : softfloat_round_min);
		FPU_SET_C1(!toward_zero);
	} else if (tiny && operation == 1) {
		FPU_SET_C1(extF80_eq(cosine, x87::one()));
	}

	if (!x87::is_zero(a)) {
		softfloat_raiseFlags(softfloat_flag_inexact);
	}
	if (operation == 0) {
		FPU_result(TOP, sine);
	} else if (operation == 1) {
		FPU_result(TOP, cosine);
	} else if (operation == 2) {
		FPU_result(TOP, sine);
		if (!(fpu.sw & ~fpu.cw & 0x1f)) {
			FPU_PUSH(cosine);
		}
	} else {
		FPU_result(TOP, x87::tangent(a));
		if (!(fpu.sw & ~fpu.cw & 0x1f)) {
			FPU_PUSH(x87::one());
		}
	}
}
static void FPU_FSIN()
{
	FPU_trig(0);
}
static void FPU_FCOS()
{
	FPU_trig(1);
}
static void FPU_FSINCOS()
{
	FPU_trig(2);
}
static void FPU_FPTAN()
{
	FPU_trig(3);
}
static void FPU_F2XM1()
{
	FPU_begin();
	FPU_Float a;
	if (!FPU_trans_operand(a)) {
		return;
	}
	// Intel leaves results outside [-1,+1] undefined. Retain the operand.
	if (extF80_lt(x87::one(), x87::abs(a))) {
		return;
	}
	const auto out = x87::exp2m1(a);
	if (!x87::is_zero(a) && !extF80_eq(x87::abs(a), x87::one())) {
		softfloat_raiseFlags(softfloat_flag_inexact);
	}
	FPU_result(TOP, out);
}
static void FPU_FPATAN()
{
	FPU_begin();
	const auto x = FPU_operand(TOP), y = FPU_operand(STV(1));
	FPU_Float out;
	if (x87::nan(x) || x87::nan(y)) {
		out = extF80_add(x, y);
	} else {
		out = x87::atan2(y, x);
		if (!x87::is_zero(out)) {
			softfloat_raiseFlags(softfloat_flag_inexact);
		}
	}
	FPU_result(STV(1), out);
	FPU_FPOP();
}
static void FPU_logarithm(bool plus_one)
{
	FPU_begin();
	const auto x = FPU_operand(TOP), y = FPU_operand(STV(1));
	FPU_Float out;
	if (x87::nan(x) || x87::nan(y)) {
		out = extF80_add(x, y);
	} else {
		const auto argument = plus_one ? extF80_add(x, x87::one()) : x;
		if (x87::sign(argument) && !x87::is_zero(argument)) {
			softfloat_raiseFlags(softfloat_flag_invalid);
			out = x87::indefinite();
		} else if (x87::is_zero(argument)) {
			if (x87::is_zero(y)) {
				softfloat_raiseFlags(softfloat_flag_invalid);
				out = x87::indefinite();
			} else {
				softfloat_raiseFlags(softfloat_flag_infinite);
				out = x87::infinity(!x87::sign(y));
			}
		} else {
			out = x87::log2_times(x, y, plus_one);
			if (!x87::is_zero(out) && !x87::inf(out)) {
				softfloat_raiseFlags(softfloat_flag_inexact);
			}
		}
	}
	FPU_result(STV(1), out);
	FPU_FPOP();
}
static void FPU_FYL2X()
{
	FPU_logarithm(false);
}
static void FPU_FYL2XP1()
{
	FPU_logarithm(true);
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
	FPU_SetCW(fpu.cw | 0x3f);
}

static void FPU_FLDENV(PhysPt addr)
{
	uint16_t tag;
	uint32_t tagbig;
	Bitu cw;
	if (!cpu.code.big) {
		cw     = mem_readw(addr + 0);
		fpu.sw = mem_readw(addr + 2);
		tag    = mem_readw(addr + 4);
	} else {
		cw     = mem_readd(addr + 0);
		fpu.sw = static_cast<uint16_t>(mem_readd(addr + 4));
		tagbig = mem_readd(addr + 8);
		tag    = static_cast<uint16_t>(tagbig);
	}
	FPU_SetTag(tag);
	FPU_SetCW(cw);
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
		fpu.regs[STV(i)].value = FPU_FLD80(addr + start);
		start += 10;
	}
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
