// SPDX-FileCopyrightText: 2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later
#include "fpu/float80.h"
#include <array>
#include <bit>

namespace x87 {
using namespace softfloat;
namespace {
// Transcendental evaluation uses software binary128 guard precision. No host
// floating-point operations or host rounding mode enter these calculations.
using Float128 = float128_t;
Float128 to_float128(const int64_t value)
{
	return i64_to_f128(value);
}
Float128 to_float128(const Float value)
{
	return extF80_to_f128(value);
}
Float128 operator+(Float128 a, Float128 b)
{
	return f128_add(a, b);
}
Float128 operator-(Float128 a, Float128 b)
{
	return f128_sub(a, b);
}
Float128 operator*(Float128 a, Float128 b)
{
	return f128_mul(a, b);
}
Float128 operator/(Float128 a, Float128 b)
{
	return f128_div(a, b);
}
bool operator==(Float128 a, Float128 b)
{
	return f128_eq(a, b);
}
bool operator<(Float128 a, Float128 b)
{
	return f128_lt(a, b);
}
Float128 bits(uint64_t hi, uint64_t lo)
{
	Float128 f = {};
#ifdef LITTLEENDIAN
	f.v[1] = hi;
	f.v[0] = lo;
#else
	f.v[0] = hi;
	f.v[1] = lo;
#endif
	return f;
}
Float128 pi()
{
	return bits(0x4000921fb54442d1, 0x8469898cc51701b8);
}
Float128 ln2()
{
	return bits(0x3ffe62e42fefa39e, 0xf35793c7673007e6);
}
struct SoftFloatStateGuard {
	uint_fast8_t rounding = softfloat_roundingMode;
	uint_fast8_t flags    = softfloat_exceptionFlags;
	SoftFloatStateGuard()
	{
		softfloat_roundingMode   = softfloat_round_near_even;
		softfloat_exceptionFlags = 0;
	}
	~SoftFloatStateGuard()
	{
		softfloat_roundingMode   = rounding;
		softfloat_exceptionFlags = flags;
	}
	Float result(Float128 a)
	{
		softfloat_roundingMode   = rounding;
		extF80_roundingPrecision = 80;
		// Underflow of a discarded series term is not underflow of the
		// instruction result; the final conversion detects result
		// tininess.
		softfloat_exceptionFlags &= ~softfloat_flag_underflow;
		const auto out = f128_to_extF80(a);
		flags |= softfloat_exceptionFlags;
		return out;
	}
};
Float128 twice_atanh(Float128 z)
{
	// After range reduction, |z| <= 1/3.
	Float128 zz = z * z, term = z, sum = z;
	for (int n = 3; n < 513; n += 2) {
		term          = term * zz;
		Float128 next = sum + term / to_float128(n);
		if (next == sum) {
			break;
		}
		sum = next;
	}
	return to_float128(2) * sum;
}
Float128 arctangent(Float128 a)
{
	bool reciprocal = to_float128(1) < a;
	if (reciprocal) {
		a = to_float128(1) / a;
	}
	bool shifted = to_float128(1) / to_float128(2) < a;
	if (shifted) {
		a = (a - to_float128(1)) / (a + to_float128(1));
	}
	Float128 aa = to_float128(0) - a * a, term = a, sum = a;
	for (int n = 3; n < 513; n += 2) {
		term          = term * aa;
		Float128 next = sum + term / to_float128(n);
		if (next == sum) {
			if (sum == a && !(a == to_float128(0))) {
				// atan(a) is strictly below a. Preserve that
				// direction when the correction is smaller than
				// a binary128 ULP.
#ifdef LITTLEENDIAN
				if (sum.v[0]-- == 0) {
					--sum.v[1];
				}
#else
				if (sum.v[1]-- == 0) {
					--sum.v[0];
				}
#endif
			}
			break;
		}
		sum = next;
	}
	if (shifted) {
		sum = sum + pi() / to_float128(4);
	}
	return reciprocal ? pi() / to_float128(2) - sum : sum;
}
} // namespace

Float scale(Float a, int n)
{
	if (is_zero(a) || inf(a) || nan(a)) {
		return a;
	}
	int e = exponent(a);
	if (e == 0) {
		const int shift = std::countl_zero(a.signif);
		a.signif <<= shift;
		e = 1 - shift;
	}
	return softfloat_roundPackToExtF80(sign(a), e + n, a.signif, 0, 80);
}

static Float128 log2q(Float a, bool plus_one)
{
	Float128 input = to_float128(a);
	if (plus_one) {
		if (to_float128(abs(a)) < to_float128(1) / to_float128(2)) {
			return twice_atanh(input / (input + to_float128(2))) / ln2();
		}
		input = input + to_float128(1);
	}
	if (input == to_float128(0)) {
		return to_float128(infinity(true));
	}
	if (input < to_float128(0)) {
		return to_float128(indefinite());
	}
	// Extract the binary128 exponent and normalize without a host conversion.
#ifdef LITTLEENDIAN
	auto& hi = input.v[1];
#else
	auto& hi = input.v[0];
#endif
	int e = static_cast<int>((hi >> 48) & 0x7fff) - 16383;
	if (e == 16384) {
		return to_float128(inf(a) && !sign(a) ? a : indefinite());
	}
	if (e == -16383) {
		// Normalize binary128 subnormals through exact multiplication.
		input = input * bits(0x407f000000000000, 0);
		e     = static_cast<int>((hi >> 48) & 0x7fff) - 16383 - 128;
	}
	hi = (hi & UINT64_C(0x0000ffffffffffff)) | UINT64_C(0x3fff000000000000);
	return to_float128(e) +
	       twice_atanh((input - to_float128(1)) / (input + to_float128(1))) /
	               ln2();
}

Float log2(Float a, bool plus_one)
{
	SoftFloatStateGuard guard;
	return guard.result(log2q(a, plus_one));
}

Float log2_times(Float a, Float y, bool plus_one)
{
	SoftFloatStateGuard guard;
	return guard.result(log2q(a, plus_one) * to_float128(y));
}

Float exp2m1(Float a)
{
	if (is_zero(a)) {
		return a;
	}
	SoftFloatStateGuard guard;
	Float128 x = to_float128(a) * ln2(), term = x, sum = x;
	for (int n = 2; n < 256; ++n) {
		term          = term * x / to_float128(n);
		Float128 next = sum + term;
		if (next == sum) {
			break;
		}
		sum = next;
	}
	return guard.result(sum);
}

Float atan2(Float y, Float x)
{
	SoftFloatStateGuard guard;
	Float128 angle;
	if (inf(x) && inf(y)) {
		angle = sign(x) ? pi() * to_float128(3) / to_float128(4)
		                : pi() / to_float128(4);
	} else if (is_zero(y)) {
		angle = sign(x) ? pi() : to_float128(0);
	} else if (is_zero(x) || inf(y)) {
		angle = pi() / to_float128(2);
	} else if (inf(x)) {
		angle = sign(x) ? pi() : to_float128(0);
	} else {
		angle = arctangent(to_float128(abs(y)) / to_float128(abs(x)));
		if (sign(x)) {
			angle = pi() - angle;
		}
	}
	if (angle == to_float128(0)) {
		return raw(sign(y) ? 0x8000 : 0, 0);
	}
	return guard.result(sign(y) ? to_float128(0) - angle : angle);
}

static void sincosq(Float a, Float128& sine, Float128& cosine)
{
	Float128 half_pi = pi() / to_float128(2);
	Float128 r       = to_float128(a);
	int quadrant     = 0;
	const int e      = exponent(a) - 16383;
	if (e >= -1) {
		// Payne-Hanek reduction with 256 fixed-point bits of 2/pi.
		// Reducing a 63-bit argument directly in binary128 would lose
		// up to 63 guard bits through cancellation. Keep the fractional
		// product separately before converting it to binary128.
		constexpr std::array<uint32_t, 8> inv_pi = {0xdebbc561,
		                                            0xfe5163ab,
		                                            0x3c439041,
		                                            0xdb629599,
		                                            0xf534ddc0,
		                                            0xfc2757d1,
		                                            0x4e441529,
		                                            0xa2f9836e};
		std::array<uint32_t, 10> product         = {};
		for (int i = 0; i < 2; ++i) {
			uint64_t carry      = 0;
			const uint32_t limb = static_cast<uint32_t>(a.signif >>
			                                            (32 * i));
			for (int j = 0; j < 8; ++j) {
				const uint64_t v = uint64_t(limb) * inv_pi[j] +
				                   product[i + j] + carry;
				product[i + j]   = static_cast<uint32_t>(v);
				carry            = v >> 32;
			}
			product[i + 8] = static_cast<uint32_t>(carry);
		}
		const int shift = 319 - e;
		auto bit        = [&](int i) {
			return i < 320 && ((product[i / 32] >> (i % 32)) & 1);
		};
		quadrant = int(bit(shift)) | (int(bit(shift + 1)) << 1);
		const bool round_up = bit(shift - 1);
		if (round_up) {
			quadrant = (quadrant + 1) & 3;
		}
		// Discard integer bits. If rounding selected the next quadrant,
		// form the magnitude of the negative fraction by two's complement.
		uint64_t carry = round_up ? 1 : 0;
		for (int i = 0; i < 10; ++i) {
			const int valid     = shift - 32 * i;
			const uint32_t mask = valid >= 32 ? UINT32_MAX
			                    : valid <= 0
			                            ? 0
			                            : (UINT32_C(1) << valid) - 1;
			const uint64_t v    = uint64_t(round_up ? ~product[i]
			                                        : product[i]) +
			                      carry;
			product[i]          = static_cast<uint32_t>(v) & mask;
			carry               = v >> 32;
		}
		Float128 fraction = to_float128(0);
		for (int i = 9; i >= 0; --i) {
			fraction = fraction +
			           to_float128(product[i]) *
			                   bits(uint64_t(16383 + 32 * i - shift) << 48,
			                        0);
		}
		r = fraction * half_pi;
		if (round_up != sign(a)) {
			r = to_float128(0) - r;
		}
		if (sign(a)) {
			quadrant = (-quadrant) & 3;
		}
	}
	Float128 rr = to_float128(0) - r * r;
	Float128 s = r, c = to_float128(1), st = r, ct = to_float128(1);
	for (int n = 2; n < 160; n += 2) {
		st          = st * rr / to_float128(n * (n + 1));
		ct          = ct * rr / to_float128((n - 1) * n);
		Float128 sn = s + st, cn = c + ct;
		if (sn == s && cn == c) {
			break;
		}
		s = sn;
		c = cn;
	}
	if (quadrant == 1) {
		Float128 t = s;
		s          = c;
		c          = to_float128(0) - t;
	} else if (quadrant == 2) {
		s = to_float128(0) - s;
		c = to_float128(0) - c;
	} else if (quadrant == 3) {
		Float128 t = s;
		s          = to_float128(0) - c;
		c          = t;
	}
	sine   = s;
	cosine = c;
}

static Float adjacent_magnitude(Float a, bool increase)
{
	const auto sign_bit = a.signExp & 0x8000;
	int e               = exponent(a);
	if (increase) {
		if (++a.signif == 0) {
			++e;
			a.signif = UINT64_C(1) << 63;
		} else if (e == 0 && (a.signif >> 63)) {
			e = 1;
		}
	} else {
		if (a.signif == (UINT64_C(1) << 63) && e > 1) {
			--e;
			a.signif = UINT64_MAX;
		} else {
			--a.signif;
			if (e == 1 && !(a.signif >> 63)) {
				e = 0;
			}
		}
	}
	a.signExp = static_cast<uint16_t>(sign_bit | e);
	return a;
}

void sincos(Float a, Float& sine, Float& cosine)
{
	if (is_zero(a)) {
		sine   = a;
		cosine = one();
		return;
	}
	if (exponent(a) < 16383 - 33) {
		// For tiny sine arguments the correction is below half an
		// extended ULP, but directed rounding must still retain its
		// direction.
		const auto rc          = softfloat_roundingMode;
		const bool toward_zero = rc == softfloat_round_minMag ||
		                         rc == (sign(a) ? softfloat_round_max
		                                        : softfloat_round_min);
		sine = toward_zero ? adjacent_magnitude(a, false) : a;
		// Match the native DOSBox x87 backend's tiny-cosine
		// approximation. Its FCOS wrapper uses the host's usual nearest
		// rounding mode. Here |x| < 2^-33, so returning +1 differs from
		// cos(x) by less than 1/8 ULP below 1, within the SDM 8.3.10
		// error allowance for every RC. Keep this even in x; do not
		// special-case its sign.
		cosine = one();
		return;
	}
	SoftFloatStateGuard guard;
	Float128 s, c;
	sincosq(a, s, c);
	sine   = guard.result(s);
	cosine = guard.result(c);
}

Float tangent(Float a)
{
	if (is_zero(a)) {
		return a;
	}
	if (exponent(a) < 16383 - 33) {
		const bool away = softfloat_roundingMode ==
		                  (sign(a) ? softfloat_round_min
		                           : softfloat_round_max);
		return away ? adjacent_magnitude(a, true) : a;
	}
	SoftFloatStateGuard guard;
	Float128 s, c;
	sincosq(a, s, c);
	return guard.result(s / c);
}
} // namespace x87
