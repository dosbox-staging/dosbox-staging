// SPDX-FileCopyrightText: 2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later
#include "cpu/cpu.h"
#include "dosbox.h"
#include "dosbox_test_fixture.h"
#include "fpu/fpu.h"
#include <gtest/gtest.h>
#if !C_FPU_X86
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "fpu/fpu_instructions.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace {
class Fpu80 : public ::testing::Test {
protected:
	void SetUp() override
	{
		FPU_FINIT();
	}
	void put(int reg, FPU_Float a)
	{
		FPU_put(reg, a);
	}
	void expect(int reg, uint16_t exp, uint64_t sig)
	{
		EXPECT_EQ(fpu.regs[reg].value.signExp, exp);
		EXPECT_EQ(fpu.regs[reg].value.signif, sig);
	}
};
TEST_F(Fpu80, ExtendedSignificandSurvivesCancellation)
{
	put(0, x87::integer(INT64_MAX));
	put(1, x87::integer(INT64_MAX - 1));
	FPU_FSUB(0, 1);
	expect(0, 0x3fff, UINT64_C(1) << 63);
}
TEST_F(Fpu80, IntegerRoundTripUsesAll64Bits)
{
	for (const auto n : {INT64_MIN, INT64_MIN + 1, INT64_MAX - 1, INT64_MAX}) {
		put(0, x87::integer(n));
		FPU_begin();
		EXPECT_EQ(FPU_integer(64), n);
		EXPECT_EQ(softfloat_exceptionFlags, 0);
	}
}
TEST_F(Fpu80, PrecisionAndRoundingControl)
{
	for (int pc : {0, 2, 3}) {
		for (int rc = 0; rc < 4; ++rc) {
			FPU_FINIT();
			FPU_SetCW(0x7f | (pc << 8) | (rc << 10));
			put(0, x87::one());
			put(1, x87::raw(0x3fbf, UINT64_C(1) << 63)); // 2^-64, a
			                                             // tie at
			                                             // PC=64
			FPU_FADD(0, 1);
			const uint64_t increment = rc == 2 ? (UINT64_C(1)
			                                      << (pc == 0 ? 40
			                                          : pc == 2 ? 11
			                                                    : 0))
			                                   : 0;
			expect(0, 0x3fff, (UINT64_C(1) << 63) + increment);
			EXPECT_EQ(fpu.sw & 0x220, rc == 2 ? 0x220 : 0x20);
		}
	}
}
TEST_F(Fpu80, ChopDoesNotSnapAdjacentInteger)
{
	FPU_SetCW(0xf7f);
	put(0, x87::raw(0x4001, UINT64_C(0xdfffffffffffffff)));
	FPU_FRNDINT();
	EXPECT_EQ(extF80_to_i64(fpu.regs[0].value, 0, false), 6);
}
TEST_F(Fpu80, NearestIntegerTiesAreEven)
{
	for (int n : {1, 3, 5, -1, -3, -5}) {
		put(0, extF80_div(x87::integer(n), x87::integer(2)));
		FPU_FRNDINT();
		const int expected = n == 3  ? 2
		                   : n == 5  ? 2
		                   : n == -3 ? -2
		                   : n == -5 ? -2
		                             : 0;
		EXPECT_EQ(extF80_to_i64(fpu.regs[0].value, 0, false), expected);
	}
}
TEST_F(Fpu80, ExtendedExponentDoesNotOverflowAtDoubleLimit)
{
	put(0, x87::raw(0x6000, UINT64_C(1) << 63));
	put(1, x87::integer(2));
	FPU_FMUL(0, 1);
	expect(0, 0x6001, UINT64_C(1) << 63);
	EXPECT_EQ(fpu.sw & 0x3f, 0);
}
TEST_F(Fpu80, InvalidAndDivideByZero)
{
	put(0, x87::one());
	put(1, x87::zero());
	FPU_FDIV(0, 1);
	expect(0, 0x7fff, UINT64_C(1) << 63);
	EXPECT_EQ(fpu.sw & 0x3f, 4);
	FPU_FCLEX();
	put(0, x87::zero());
	FPU_FDIV(0, 1);
	expect(0, 0xffff, UINT64_C(0xc000000000000000));
	EXPECT_EQ(fpu.sw & 0x3f, 1);
}
TEST_F(Fpu80, UnmaskedExceptionPreservesDestination)
{
	FPU_SetCW(0x37b);
	put(0, x87::one());
	put(1, x87::zero());
	FPU_FDIV(0, 1);
	expect(0, 0x3fff, UINT64_C(1) << 63);
	EXPECT_EQ(fpu.sw & 0x8084, 0x8084);
	const auto top = TOP;
	FPU_FPOP();
	EXPECT_EQ(TOP, top);
}
TEST_F(Fpu80, UnmaskedPrecisionStillWritesRoundedResult)
{
	FPU_SetCW(0x35f);
	put(0, x87::one());
	put(1, x87::raw(0x3fbf, UINT64_C(1) << 63));
	FPU_FADD(0, 1);
	expect(0, 0x3fff, UINT64_C(1) << 63);
	EXPECT_EQ(fpu.sw & 0x80a0, 0x80a0);
	FPU_FPOP();
	EXPECT_EQ(TOP, 1);
}
TEST_F(Fpu80, UnmaskedOverflowWrapsRegisterExponent)
{
	FPU_SetCW(0x377);
	put(0, x87::raw(0x7ffe, UINT64_MAX));
	put(1, x87::integer(2));
	FPU_FMUL(0, 1);
	expect(0, 0x1fff, UINT64_MAX);
	EXPECT_EQ(fpu.sw & 0x3f, 8);
	FPU_FPOP();
	EXPECT_EQ(TOP, 1);
}
TEST_F(Fpu80, UnmaskedExactUnderflowWrapsRegisterExponent)
{
	FPU_SetCW(0x36f);
	put(0, x87::raw(1, UINT64_C(1) << 63));
	put(1, x87::raw(0x3ffe, UINT64_C(1) << 63));
	FPU_FMUL(0, 1);
	expect(0, 0x6000, UINT64_C(1) << 63);
	EXPECT_EQ(fpu.sw & 0x3f, 16);
}
TEST_F(Fpu80, QuietCompareDistinguishesQuietAndSignalingNan)
{
	put(0, x87::raw(0x7fff, UINT64_C(0xc000000000000001)));
	put(1, x87::one());
	FPU_FUCOM(0, 1);
	EXPECT_EQ(fpu.sw & 0x4501, 0x4500);
	FPU_FCOM(0, 1);
	EXPECT_EQ(fpu.sw & 0x4501, 0x4501);
	FPU_FCLEX();
	put(0, x87::raw(0x7fff, UINT64_C(0x8000000000000001)));
	FPU_FUCOM(0, 1);
	EXPECT_EQ(fpu.sw & 1, 1);
}
TEST_F(Fpu80, InfinityComparesAsAnOrderedNumber)
{
	put(0, x87::infinity(false));
	put(1, x87::one());
	FPU_FCOM(0, 1);
	EXPECT_EQ(fpu.sw & 0x4501, 0);
}
TEST_F(Fpu80, ExchangePreservesUnsupportedAndSignalingPayloads)
{
	put(0, x87::raw(0, 1));
	put(1, x87::raw(0x4567, 1));
	FPU_FXCH(0, 1);
	expect(0, 0x4567, 1);
	expect(1, 0, 1);
	EXPECT_EQ(fpu.sw & 0x3f, 0);
	put(0, x87::raw(0x7fff, UINT64_C(0x8000000000000001)));
	FPU_FXCH(0, 1);
	expect(1, 0x7fff, UINT64_C(0x8000000000000001));
	EXPECT_EQ(fpu.sw & 0x3f, 0);
}
TEST_F(Fpu80, StackFaultsReturnIndefinite)
{
	FPU_FSQRT();
	expect(0, 0xffff, UINT64_C(0xc000000000000000));
	EXPECT_EQ(fpu.sw & 0x241, 0x41);
	FPU_FINIT();
	for (int i = 0; i < 9; ++i) {
		FPU_FLD1();
	}
	expect(TOP, 0xffff, UINT64_C(0xc000000000000000));
	EXPECT_EQ(fpu.sw & 0x241, 0x241);
}
TEST_F(Fpu80, ConstantsUseRoundingControlAndIgnorePrecisionControl)
{
	for (int rc = 0; rc < 4; ++rc) {
		FPU_FINIT();
		FPU_SetCW(0x7f | (rc << 10));
		FPU_FLDLN2();
		expect(TOP,
		       0x3ffe,
		       UINT64_C(0xb17217f7d1cf79ac) - (rc == 1 || rc == 3));
		EXPECT_EQ(fpu.sw & 0x220, 0);
	}
}
TEST_F(Fpu80, Log10TwoConstantUsesCorrectRoundingDirection)
{
	for (int rc = 0; rc < 4; ++rc) {
		FPU_FINIT();
		FPU_SetCW(0x37f | (rc << 10));
		FPU_FLDLG2();
		expect(TOP,
		       0x3ffd,
		       UINT64_C(0x9a209a84fbcff799) - (rc == 1 || rc == 3));
		EXPECT_EQ(fpu.sw & 0x23f, 0);
	}
}
TEST_F(Fpu80, UpwardDivisionOfLoadedConstantsMatchesX87)
{
	FPU_SetCW(0xb60);
	FPU_FLDPI();
	FPU_FLDLG2();
	FPU_FDIV(STV(1), TOP);
	FPU_FPOP();
	expect(TOP, 0x4002, UINT64_C(0xa6fa7312c06b3aec));
	FPU_PUSH(x87::raw(0x4002, UINT64_C(0xa6fa7312c06b3aec)));
	FPU_FCOM(TOP, STV(1));
	FPU_SET_TOP(TOP);
	EXPECT_EQ(fpu.cw, 0xb60);
	EXPECT_EQ(fpu.sw, 0x7020);
}
TEST_F(Fpu80, RemainderAndNearestRemainder)
{
	put(0, x87::integer(7));
	put(1, x87::integer(2));
	FPU_FPREM();
	expect(0, 0x3fff, UINT64_C(1) << 63);
	EXPECT_EQ(fpu.sw & 0x4700, 0x4200);
	put(0, x87::integer(7));
	FPU_FPREM1();
	expect(0, 0xbfff, UINT64_C(1) << 63);
	EXPECT_EQ(fpu.sw & 0x4700, 0x100);
}
TEST_F(Fpu80, NearestRemainderHandlesDenormalDivisors)
{
	put(0, x87::raw(0, 3));
	put(1, x87::raw(0, 4));
	FPU_FPREM1();
	expect(0, 0x8000, 1);
	EXPECT_EQ(fpu.sw & 0x473f, 0x202);
}
TEST_F(Fpu80, PartialRemainderMakesProgress)
{
	put(0, x87::raw(0x5000, UINT64_C(0xabcdef1234567890)));
	put(1, x87::integer(3));
	FPU_FPREM();
	EXPECT_NE(fpu.sw & 0x400, 0);
	int iterations = 0;
	while ((fpu.sw & 0x400) && ++iterations < 200) {
		FPU_FPREM();
	}
	EXPECT_LT(iterations, 200);
	EXPECT_TRUE(extF80_lt(x87::abs(fpu.regs[0].value), x87::integer(3)));
}
TEST_F(Fpu80, TrigRangeLeavesStackAndOperandUnchanged)
{
	put(0, x87::raw(0x403e, UINT64_C(1) << 63));
	FPU_FPTAN();
	EXPECT_EQ(TOP, 0);
	EXPECT_NE(fpu.sw & 0x400, 0);
	expect(0, 0x403e, UINT64_C(1) << 63);
}
TEST_F(Fpu80, TinySineReportsUnderflowAndRoundingDirection)
{
	const auto tiny_values = {x87::raw(1, UINT64_C(0x8000000000000000)),
	                          x87::raw(1, UINT64_C(0x8000000000000001)),
	                          x87::raw(2, UINT64_C(0x8000000000000000)),
	                          x87::raw(2, UINT64_C(0x8000000000000003)),
	                          x87::raw(0, UINT64_C(0x7ffffffffffffffe)),
	                          x87::raw(1, UINT64_C(0xfffffffffffffffb)),
	                          x87::raw(0, 1),
	                          x87::raw(0, 9)};
	for (auto a : tiny_values) {
		for (bool negative : {false, true}) {
			for (int rc = 0; rc < 4; ++rc) {
				SCOPED_TRACE(::testing::Message()
				             << "exp=" << a.signExp
				             << " sig=" << a.signif << " negative="
				             << negative << " rc=" << rc);
				FPU_FINIT();
				FPU_SetCW(0x37f | (rc << 10));
				if (negative) {
					a = x87::negate(a);
				}
				put(0, a);
				FPU_FSIN();
				const bool toward_zero = rc == 3 ||
				                         rc == (negative ? 2 : 1);
				const bool tiny_result = x87::exponent(a) == 0 ||
				                         (x87::exponent(a) == 1 &&
				                          a.signif == (UINT64_C(1)
				                                       << 63) &&
				                          toward_zero);
				EXPECT_EQ(bool(fpu.sw & 16), tiny_result);
				EXPECT_NE(fpu.sw & 32, 0);
				EXPECT_EQ(bool(fpu.sw & 0x200), !toward_zero);
				if (negative) {
					a = x87::negate(a);
				}
			}
		}
	}
}
TEST_F(Fpu80, TinyCosineMatchesNativeBackend)
{
	const auto tiny_values = {x87::raw(1, UINT64_C(0x8000000000000000)),
	                          x87::raw(1, UINT64_C(0x8000000000000001)),
	                          x87::raw(2, UINT64_C(0x8000000000000000)),
	                          x87::raw(2, UINT64_C(0x8000000000000003)),
	                          x87::raw(0, UINT64_C(0x7ffffffffffffffe)),
	                          x87::raw(1, UINT64_C(0xfffffffffffffffb)),
	                          x87::raw(0, 1),
	                          x87::raw(0, 9)};
	for (auto a : tiny_values) {
		for (int rc = 0; rc < 4; ++rc) {
			for (bool negative : {false, true}) {
				SCOPED_TRACE(::testing::Message()
				             << "exp=" << a.signExp
				             << " sig=" << a.signif << " negative="
				             << negative << " rc=" << rc);
				FPU_FINIT();
				FPU_SetCW(0x37f | (rc << 10));
				put(0, negative ? x87::negate(a) : a);
				FPU_FCOS();
				expect(0, 0x3fff, UINT64_C(1) << 63);
				EXPECT_EQ(fpu.sw & 0x3f,
				          x87::exponent(a) == 0 ? 0x22 : 0x20);
				EXPECT_NE(fpu.sw & 0x200, 0);
			}
		}
	}
}
TEST_F(Fpu80, UnmaskedTinySineWrapsBeforeSubnormalRounding)
{
	for (int rc = 0; rc < 4; ++rc) {
		FPU_FINIT();
		FPU_SetCW(0x36f | (rc << 10));
		put(0, x87::raw(0, 1));
		FPU_FSIN();
		const bool toward_zero = rc == 1 || rc == 3;
		expect(0,
		       toward_zero ? 0x5fc1 : 0x5fc2,
		       toward_zero ? UINT64_MAX : (UINT64_C(1) << 63));
		EXPECT_EQ(fpu.sw & 0xb0, 0xb0);
	}
}
TEST_F(Fpu80, TranscendentalIdentities)
{
	put(0, x87::zero());
	FPU_FSIN();
	expect(0, 0, 0);
	put(0, x87::zero());
	FPU_FCOS();
	expect(0, 0x3fff, UINT64_C(1) << 63);
	put(0, x87::one());
	FPU_F2XM1();
	expect(0, 0x3fff, UINT64_C(1) << 63);
	put(0, x87::integer(-1));
	FPU_F2XM1();
	expect(0, 0xbffe, UINT64_C(1) << 63);
}
TEST_F(Fpu80, LogarithmRetainsSmallLog1pArguments)
{
	FPU_begin();
	const auto a   = x87::raw(0x3f9b, UINT64_C(1) << 63); // 2^-100
	const auto out = x87::log2(a, true);
	EXPECT_FALSE(x87::is_zero(out));
}
TEST_F(Fpu80, ScalePreservesExtendedRange)
{
	put(0, x87::one());
	put(1, x87::integer(4000));
	FPU_FSCALE();
	expect(0, 0x3fff + 4000, UINT64_C(1) << 63);
}
TEST_F(Fpu80, ExtractHandlesZeroAndSignedInfinity)
{
	put(0, x87::zero());
	FPU_FXTRACT();
	expect(TOP, 0, 0);
	expect(STV(1), 0xffff, UINT64_C(1) << 63);
	EXPECT_NE(fpu.sw & 4, 0);
}

TEST_F(Fpu80, PseudoDenormalAndExceptionPriority)
{
	put(0, x87::raw(0, UINT64_C(1) << 63));
	put(1, x87::one());
	FPU_FADD(0, 1);
	EXPECT_EQ(fpu.sw & 0x3f, 0x22);
	FPU_FCLEX();
	put(0, x87::raw(0, 1));
	put(1, x87::raw(0x7fff, UINT64_C(0xc000000000000001)));
	FPU_FADD(0, 1);
	EXPECT_EQ(fpu.sw & 0x3f, 0);
	FPU_FCLEX();
	put(0, x87::raw(0x3fff, 1));
	FPU_FADD(0, 1);
	expect(0, 0xffff, UINT64_C(0xc000000000000000));
	EXPECT_EQ(fpu.sw & 0x3f, 1);
}
TEST_F(Fpu80, LargeTrigUsesFixedPointArgumentReduction)
{
	put(0, x87::raw(0x403d, UINT64_C(0xabcdef1234567890)));
	FPU_FSIN();
	expect(0, 0x3ffc, UINT64_C(0xc64371b7ed4cf3db));
	put(0, x87::raw(0x403d, UINT64_C(0xabcdef1234567890)));
	FPU_FCOS();
	expect(0, 0x3ffe, UINT64_C(0xfb27e11cf89838b0));
}
TEST_F(Fpu80, TinyLog1pDoesNotLoseArgument)
{
	FPU_begin();
	const auto out = x87::log2(x87::raw(0x3c17, UINT64_C(1) << 63), true);
	EXPECT_EQ(out.signExp, 0x3c17);
	EXPECT_EQ(out.signif, UINT64_C(0xb8aa3b295c17f0bc));
}
TEST_F(Fpu80, TinyNormalLog1pDoesNotReportSeriesUnderflow)
{
	FPU_begin();
	const auto out = x87::log2(x87::raw(16383 - 10000, UINT64_C(1) << 63), true);
	EXPECT_NE(out.signExp & 0x7fff, 0);
	EXPECT_EQ(softfloat_exceptionFlags & softfloat_flag_underflow, 0);
}
TEST_F(Fpu80, TinyAtanRetainsDirectedRoundingCorrection)
{
	FPU_SetCW(0x77f);
	FPU_begin();
	const auto out = x87::atan2(x87::raw(0x3fa3, UINT64_C(0xd428cf7a12ab34cd)),
	                            x87::one());
	EXPECT_EQ(out.signExp, 0x3fa3);
	EXPECT_EQ(out.signif, UINT64_C(0xd428cf7a12ab34cc));
}

class Fpu80Memory : public DOSBoxTestFixture {
protected:
	void SetUp() override
	{
		DOSBoxTestFixture::SetUp();
		FPU_FINIT();
	}
	static constexpr PhysPt addr = 0x10000;
};
TEST_F(Fpu80Memory, ExtendedLoadStorePreservesAllBits)
{
	for (const auto a : {x87::raw(0x7fff, UINT64_C(0x8000000000000001)),
	                     x87::raw(0x3fff, UINT64_C(0x8000000000000001)),
	                     x87::raw(0, 1),
	                     x87::raw(0x4567, 1)}) {
		FPU_FINIT();
		mem_writeq(addr, a.signif);
		mem_writew(addr + 8, a.signExp);
		FPU_PREP_PUSH();
		FPU_FLD_F80(addr);
		FPU_FST_F80(addr + 16);
		EXPECT_EQ(mem_readq(addr + 16), a.signif);
		EXPECT_EQ(mem_readw(addr + 24), a.signExp);
		EXPECT_EQ(fpu.sw & 0x3f, 0);
	}
}
TEST_F(Fpu80Memory, Int64MemoryRoundTrip)
{
	for (const auto n : {INT64_MIN, INT64_MIN + 1, INT64_MAX - 1, INT64_MAX}) {
		FPU_FINIT();
		mem_writeq(addr, n);
		FPU_PREP_PUSH();
		FPU_FLD_I64(addr, TOP);
		FPU_FST_I64(addr + 16);
		EXPECT_EQ(mem_readq(addr + 16), static_cast<uint64_t>(n));
		EXPECT_EQ(fpu.sw & 0x3f, 0);
	}
}
TEST_F(Fpu80Memory, StoresUseGuestRounding)
{
	for (int rc = 0; rc < 4; ++rc) {
		FPU_FINIT();
		FPU_SetCW(0x37f | (rc << 10));
		FPU_put(TOP, x87::raw(0x3fff, UINT64_C(0x8000000000000001)));
		FPU_FST_F64(addr);
		EXPECT_EQ(mem_readq(addr), UINT64_C(0x3ff0000000000000) + (rc == 2));
		EXPECT_EQ(bool(fpu.sw & 0x200), rc == 2);
		FPU_FST_F32(addr);
		EXPECT_EQ(mem_readd(addr), UINT32_C(0x3f800000) + (rc == 2));
	}
}
TEST_F(Fpu80Memory, UnmaskedStoreOverflowPreservesMemoryAndTop)
{
	FPU_SetCW(0x377);
	FPU_put(TOP, x87::raw(0x6000, UINT64_C(1) << 63));
	mem_writeq(addr, 123);
	FPU_FST_F64(addr);
	FPU_FPOP();
	EXPECT_EQ(mem_readq(addr), 123);
	EXPECT_EQ(TOP, 0);
	EXPECT_EQ(fpu.sw & 0x23f, 8);
}
TEST_F(Fpu80Memory, UnmaskedExactStoreUnderflowPreservesMemory)
{
	FPU_SetCW(0x36f);
	FPU_put(TOP, x87::raw(16383 - 1023, UINT64_C(1) << 63));
	mem_writeq(addr, 123);
	FPU_FST_F64(addr);
	FPU_FPOP();
	EXPECT_EQ(mem_readq(addr), 123);
	EXPECT_EQ(TOP, 0);
	EXPECT_EQ(fpu.sw & 0x23f, 16);
}
TEST_F(Fpu80Memory, StoresDoNotRaiseDenormalOperand)
{
	FPU_put(TOP, x87::raw(0, 1));
	FPU_FST_F64(addr);
	EXPECT_EQ(fpu.sw & 0x3f, 0x30);
	FPU_FCLEX();
	FPU_FST_I64(addr);
	EXPECT_EQ(fpu.sw & 0x3f, 0x20);
	FPU_FCLEX();
	FPU_FBST(addr);
	EXPECT_EQ(fpu.sw & 0x3f, 0x20);
}
TEST_F(Fpu80Memory, BcdRoundTripAndNegativeDirectedRounding)
{
	FPU_put(TOP, x87::integer(999999999999999999));
	FPU_FBST(addr);
	for (int i = 0; i < 9; ++i) {
		EXPECT_EQ(mem_readb(addr + i), 0x99);
	}
	FPU_FBLD(addr, TOP);
	EXPECT_EQ(fpu.regs[TOP].value.signif, x87::integer(999999999999999999).signif);
	FPU_SetCW(0x77f);
	FPU_begin();
	FPU_put(TOP, extF80_div(x87::integer(-3), x87::integer(2)));
	FPU_FBST(addr);
	EXPECT_EQ(mem_readb(addr), 2);
	EXPECT_EQ(mem_readb(addr + 9), 0x80);
}
TEST_F(Fpu80Memory, SaveRestorePreservesPayloadsAndStack)
{
	const auto old_big = cpu.code.big;
	cpu.code.big       = true;
	FPU_FLDLN2();
	FPU_PUSH(x87::raw(0x7fff, UINT64_C(0x8000000000000001)));
	const auto top  = TOP;
	const auto tags = FPU_GetTag();
	FPU_SetCW(0xb7f);
	FPU_FSAVE(addr);
	EXPECT_EQ(TOP, 0);
	EXPECT_EQ(FPU_GetTag(), 0xffff);
	FPU_FRSTOR(addr);
	EXPECT_EQ(TOP, top);
	EXPECT_EQ(FPU_GetTag(), tags);
	EXPECT_EQ(fpu.cw, 0xb7f);
	EXPECT_EQ(fpu.regs[TOP].value.signif, UINT64_C(0x8000000000000001));
	EXPECT_EQ(fpu.regs[STV(1)].value.signif, UINT64_C(0xb17217f7d1cf79ac));
	cpu.code.big = old_big;
}
} // namespace
#endif
