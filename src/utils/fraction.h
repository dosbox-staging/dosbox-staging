// SPDX-FileCopyrightText:  2023-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_FRACTION_H
#define DOSBOX_FRACTION_H

#include <cassert>
#include <cstdint>
#include <numeric>

#include <utils/string_utils.h>

// Class to represent simple fractions. The fraction is always simplified
// after construction or after any operation. The sign always is normalised so
// the numerator carries the sign and the denominator is always positive.
class Fraction {
public:
	constexpr Fraction()
	{
		num   = 0;
		denom = 1;
	}

	constexpr Fraction(const int64_t n) : num(n), denom(1) {}

	constexpr Fraction(const int64_t n, const int64_t d) : num(n), denom(d)
	{
		assert(d != 0);

		if (n == 0 || d == 0) {
			assert(num == 0);
			denom = 1;
			return;
		}

		// Simplify fraction
		const auto gcd = std::gcd(num, denom);
		num /= gcd;
		denom /= gcd;

		// Normalise sign so the denominator is always positive
		if (denom < 0) {
			num   = -num;
			denom = -denom;
		}
	}

	constexpr int64_t Num() const
	{
		return num;
	}

	constexpr int64_t Denom() const
	{
		return denom;
	}

	constexpr Fraction Inverse() const
	{
		return Fraction(denom, num);
	}

	constexpr double ToDouble() const
	{
		return static_cast<double>(num) / static_cast<double>(denom);
	}

	constexpr float ToFloat() const
	{
		return static_cast<float>(num) / static_cast<float>(denom);
	}

	// Equality
	constexpr bool operator==(const Fraction& that) const
	{
		return (num == that.num) && (denom == that.denom);
	}

	// Addition
	constexpr Fraction& operator+=(const Fraction& that)
	{
		*this = *this + that;
		return *this;
	}

	constexpr Fraction operator+(const Fraction& that) const
	{
		return {num * that.denom + that.num * denom, denom * that.denom};
	}

	// Subtraction
	constexpr Fraction operator-=(const Fraction& that)
	{
		*this = *this - that;
		return *this;
	}

	constexpr Fraction operator-(const Fraction& that) const
	{
		return {num * that.denom - that.num * denom, denom * that.denom};
	}

	// Multiplication
	constexpr Fraction& operator*=(const int64_t s)
	{
		*this = *this * s;
		return *this;
	}

	constexpr Fraction operator*(const int64_t s) const
	{
		return {num * s, denom};
	}

	constexpr Fraction& operator*=(const Fraction& that)
	{
		*this = *this * that;
		return *this;
	}

	constexpr Fraction operator*(const Fraction& that) const
	{
		return {num * that.num, denom * that.denom};
	}

	// Division
	constexpr Fraction operator/=(const int64_t s)
	{
		*this = *this / s;
		return *this;
	}

	constexpr Fraction operator/(const int64_t s) const
	{
		return {num, denom * s};
	}

	constexpr Fraction operator/=(const Fraction& that)
	{
		*this = *this / that;
		return *this;
	}

	constexpr Fraction operator/(const Fraction& that) const
	{
		return {num * that.denom, denom * that.num};
	}

	// Return a string representation of the fraction (e.g. `4:3 (1.333333)`)
	std::string ToString() const
	{
		return format_str("%d:%d (%g)", num, denom, ToFloat());
	}

private:
	int64_t num   = 0;
	int64_t denom = 1;
};

#endif
