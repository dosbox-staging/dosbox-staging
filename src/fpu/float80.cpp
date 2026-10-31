// SPDX-FileCopyrightText: 2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later
#include "float80.h"
#include <array>
#include <bit>

namespace x87 {
using namespace softfloat;
namespace {
// Transcendental evaluation uses software binary128 guard precision. No host
// floating-point operations or host rounding mode enter these calculations.
struct Q {
	float128_t v = {};
	Q()          = default;
	Q(int64_t n) : v(i64_to_f128(n)) {}
	explicit Q(Float f) : v(extF80_to_f128(f)) {}
	explicit Q(float128_t f) : v(f) {}
};
Q operator+(Q a, Q b)
{
	return Q(f128_add(a.v, b.v));
}
Q operator-(Q a, Q b)
{
	return Q(f128_sub(a.v, b.v));
}
Q operator*(Q a, Q b)
{
	return Q(f128_mul(a.v, b.v));
}
Q operator/(Q a, Q b)
{
	return Q(f128_div(a.v, b.v));
}
bool operator==(Q a, Q b)
{
	return f128_eq(a.v, b.v);
}
bool operator<(Q a, Q b)
{
	return f128_lt(a.v, b.v);
}
Q bits(uint64_t hi, uint64_t lo)
{
	float128_t f = {};
#ifdef LITTLEENDIAN
	f.v[1] = hi;
	f.v[0] = lo;
#else
	f.v[0] = hi;
	f.v[1] = lo;
#endif
	return Q(f);
}
Q pi()
{
	return bits(0x4000921fb54442d1, 0x8469898cc51701b8);
}
Q ln2()
{
	return bits(0x3ffe62e42fefa39e, 0xf35793c7673007e6);
}
struct Guard {
	uint_fast8_t rounding = softfloat_roundingMode;
	uint_fast8_t flags    = softfloat_exceptionFlags;
	Guard()
	{
		softfloat_roundingMode   = softfloat_round_near_even;
		softfloat_exceptionFlags = 0;
	}
	~Guard()
	{
		softfloat_roundingMode   = rounding;
		softfloat_exceptionFlags = flags;
	}
	Float result(Q a)
	{
		softfloat_roundingMode   = rounding;
		extF80_roundingPrecision = 80;
		// Underflow of a discarded series term is not underflow of the
		// instruction result; the final conversion detects result
		// tininess.
		softfloat_exceptionFlags &= ~softfloat_flag_underflow;
		const auto out = f128_to_extF80(a.v);
		flags |= softfloat_exceptionFlags;
		return out;
	}
};
Q twice_atanh(Q z)
{
	// After range reduction, |z| <= 1/3.
	Q zz = z * z, term = z, sum = z;
	for (int n = 3; n < 513; n += 2) {
		term   = term * zz;
		Q next = sum + term / Q(n);
		if (next == sum) {
			break;
		}
		sum = next;
	}
	return Q(2) * sum;
}
Q arctangent(Q a)
{
	bool reciprocal = Q(1) < a;
	if (reciprocal) {
		a = Q(1) / a;
	}
	bool shifted = Q(1) / Q(2) < a;
	if (shifted) {
		a = (a - Q(1)) / (a + Q(1));
	}
	Q aa = Q(0) - a * a, term = a, sum = a;
	for (int n = 3; n < 513; n += 2) {
		term   = term * aa;
		Q next = sum + term / Q(n);
		if (next == sum) {
			if (sum == a && !(a == Q(0))) {
				// atan(a) is strictly below a. Preserve that
				// direction when the correction is smaller than
				// a binary128 ULP.
#ifdef LITTLEENDIAN
				if (sum.v.v[0]-- == 0) {
					--sum.v.v[1];
				}
#else
				if (sum.v.v[1]-- == 0) {
					--sum.v.v[0];
				}
#endif
			}
			break;
		}
		sum = next;
	}
	if (shifted) {
		sum = sum + pi() / Q(4);
	}
	return reciprocal ? pi() / Q(2) - sum : sum;
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

static Q log2q(Float a, bool plus_one)
{
	Q input(a);
	if (plus_one) {
		if (Q(abs(a)) < Q(1) / Q(2)) {
			return twice_atanh(input / (input + Q(2))) / ln2();
		}
		input = input + Q(1);
	}
	if (input == Q(0)) {
		return Q(infinity(true));
	}
	if (input < Q(0)) {
		return Q(indefinite());
	}
	// Extract the binary128 exponent and normalize without a host conversion.
#ifdef LITTLEENDIAN
	auto& hi = input.v.v[1];
#else
	auto& hi = input.v.v[0];
#endif
	int e = static_cast<int>((hi >> 48) & 0x7fff) - 16383;
	if (e == 16384) {
		return Q(inf(a) && !sign(a) ? a : indefinite());
	}
	if (e == -16383) {
		// Normalize binary128 subnormals through exact multiplication.
		input = input * bits(0x407f000000000000, 0);
		e     = static_cast<int>((hi >> 48) & 0x7fff) - 16383 - 128;
	}
	hi = (hi & UINT64_C(0x0000ffffffffffff)) | UINT64_C(0x3fff000000000000);
	return Q(e) + twice_atanh((input - Q(1)) / (input + Q(1))) / ln2();
}

Float log2(Float a, bool plus_one)
{
	Guard guard;
	return guard.result(log2q(a, plus_one));
}

Float log2_times(Float a, Float y, bool plus_one)
{
	Guard guard;
	return guard.result(log2q(a, plus_one) * Q(y));
}

Float exp2m1(Float a)
{
	if (is_zero(a)) {
		return a;
	}
	Guard guard;
	Q x = Q(a) * ln2(), term = x, sum = x;
	for (int n = 2; n < 256; ++n) {
		term   = term * x / Q(n);
		Q next = sum + term;
		if (next == sum) {
			break;
		}
		sum = next;
	}
	return guard.result(sum);
}

Float atan2(Float y, Float x)
{
	Guard guard;
	Q angle;
	if (inf(x) && inf(y)) {
		angle = sign(x) ? pi() * Q(3) / Q(4) : pi() / Q(4);
	} else if (is_zero(y)) {
		angle = sign(x) ? pi() : Q(0);
	} else if (is_zero(x) || inf(y)) {
		angle = pi() / Q(2);
	} else if (inf(x)) {
		angle = sign(x) ? pi() : Q(0);
	} else {
		angle = arctangent(Q(abs(y)) / Q(abs(x)));
		if (sign(x)) {
			angle = pi() - angle;
		}
	}
	if (angle == Q(0)) {
		return raw(sign(y) ? 0x8000 : 0, 0);
	}
	return guard.result(sign(y) ? Q(0) - angle : angle);
}

static void sincosq(Float a, Q& sine, Q& cosine)
{
	Q half_pi = pi() / Q(2);
	Q r(a);
	int quadrant = 0;
	const int e  = exponent(a) - 16383;
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
		Q fraction(0);
		for (int i = 9; i >= 0; --i) {
			fraction = fraction +
			           Q(product[i]) *
			                   bits(uint64_t(16383 + 32 * i - shift) << 48,
			                        0);
		}
		r = fraction * half_pi;
		if (round_up != sign(a)) {
			r = Q(0) - r;
		}
		if (sign(a)) {
			quadrant = (-quadrant) & 3;
		}
	}
	Q rr = Q(0) - r * r, s = r, c(1), st = r, ct(1);
	for (int n = 2; n < 160; n += 2) {
		st   = st * rr / Q(n * (n + 1));
		ct   = ct * rr / Q((n - 1) * n);
		Q sn = s + st, cn = c + ct;
		if (sn == s && cn == c) {
			break;
		}
		s = sn;
		c = cn;
	}
	if (quadrant == 1) {
		Q t = s;
		s   = c;
		c   = Q(0) - t;
	} else if (quadrant == 2) {
		s = Q(0) - s;
		c = Q(0) - c;
	} else if (quadrant == 3) {
		Q t = s;
		s   = Q(0) - c;
		c   = t;
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
		// For tiny sine arguments the correction is below half an extended
		// ULP, but directed rounding must still retain its direction.
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
	Guard guard;
	Q s, c;
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
	Guard guard;
	Q s, c;
	sincosq(a, s, c);
	return guard.result(s / c);
}
} // namespace x87
