// SPDX-FileCopyrightText: 2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef DOSBOX_FLOAT80_H
#define DOSBOX_FLOAT80_H

#include <cstdint>
namespace softfloat {
extern "C" {
#include "platform.h"
#include "softfloat.h"
extern uint_fast8_t softfloat_extF80_exceptionMask;
extFloat80_t softfloat_roundPackToExtF80(bool, int_fast32_t, uint_fast64_t,
                                         uint_fast64_t, uint_fast8_t);
}
} // namespace softfloat

// The explicit integer bit and all 15 exponent bits are retained on every host.
// Never alias this struct onto guest memory: the host struct can contain padding.
namespace x87 {
using Float = softfloat::extFloat80_t;
inline Float raw(uint16_t exponent, uint64_t significand)
{
	Float v   = {};
	v.signExp = exponent;
	v.signif  = significand;
	return v;
}
inline Float integer(int64_t v)
{
	return softfloat::i64_to_extF80(v);
}
inline Float zero()
{
	return raw(0, 0);
}
inline Float one()
{
	return raw(0x3fff, UINT64_C(0x8000000000000000));
}
inline Float indefinite()
{
	return raw(0xffff, UINT64_C(0xc000000000000000));
}
inline int exponent(Float a)
{
	return a.signExp & 0x7fff;
}
inline bool sign(Float a)
{
	return (a.signExp & 0x8000) != 0;
}
inline bool is_zero(Float a)
{
	return exponent(a) == 0 && a.signif == 0;
}
inline bool unsupported(Float a)
{
	return exponent(a) != 0 && !(a.signif >> 63);
}
inline bool nan(Float a)
{
	return exponent(a) == 0x7fff && (a.signif << 1);
}
inline bool inf(Float a)
{
	return exponent(a) == 0x7fff && a.signif == (UINT64_C(1) << 63);
}
inline bool denormal(Float a)
{
	return exponent(a) == 0 && a.signif && !(a.signif >> 63);
}
inline Float negate(Float a)
{
	a.signExp ^= 0x8000;
	return a;
}
inline Float abs(Float a)
{
	a.signExp &= 0x7fff;
	return a;
}
inline Float infinity(bool negative)
{
	return raw(negative ? 0xffff : 0x7fff, UINT64_C(1) << 63);
}
// These operations ignore PC; callers choose precision only for arithmetic.
Float scale(Float a, int n);
Float log2(Float a, bool plus_one = false);
Float log2_times(Float a, Float y, bool plus_one = false);
Float exp2m1(Float a);
Float atan2(Float y, Float x);
Float tangent(Float a);
void sincos(Float a, Float& sine, Float& cosine);
} // namespace x87
#endif
