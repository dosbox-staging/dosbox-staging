// SPDX-FileCopyrightText:  2023-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "util/fraction.h"

#include <gtest/gtest.h>

TEST(fraction, constructor_integer)
{
	const Fraction a = {2};
	EXPECT_EQ(a.Num(), 2);
	EXPECT_EQ(a.Denom(), 1);
}

TEST(fraction, constructor_zero)
{
	const Fraction a = {0};
	EXPECT_EQ(a.Num(), 0);
	EXPECT_EQ(a.Denom(), 1);
}

TEST(fraction, constructor_positive)
{
	const Fraction a = {2, 3};
	EXPECT_EQ(a.Num(), 2);
	EXPECT_EQ(a.Denom(), 3);
}

TEST(fraction, constructor_negative)
{
	const Fraction a = {-2, -3};
	EXPECT_EQ(a.Num(), 2);
	EXPECT_EQ(a.Denom(), 3);
}

TEST(fraction, constructor_negative_num)
{
	const Fraction a = {-2, 3};
	EXPECT_EQ(a.Num(), -2);
	EXPECT_EQ(a.Denom(), 3);
}

TEST(fraction, constructor_negative_denom)
{
	const Fraction a = {2, -3};
	EXPECT_EQ(a.Num(), -2);
	EXPECT_EQ(a.Denom(), 3);
}

TEST(fraction, inverse)
{
	const Fraction a = {2, 3};
	EXPECT_EQ(a.Inverse(), Fraction(3, 2));
}

TEST(fraction, to_float)
{
	const Fraction a = {2, 3};
	EXPECT_EQ(a.ToFloat(), 2.0f / 3.0f);
}

TEST(fraction, to_double)
{
	const Fraction a = {2, 3};
	EXPECT_EQ(a.ToDouble(), 2.0 / 3.0);
}

TEST(fraction, equality)
{
	const Fraction a = {2, 3};
	const Fraction b = {2, 3};
	const Fraction c = {3, 4};
	EXPECT_TRUE(a == b);
	EXPECT_FALSE(a == c);
}

TEST(fraction, unequality)
{
	const Fraction a = {2, 3};
	const Fraction b = {2, 3};
	const Fraction c = {3, 4};
	EXPECT_FALSE(a != b);
	EXPECT_TRUE(a != c);
}

TEST(fraction, simplification)
{
	const Fraction b = {2 * 9, 3 * 9};
	EXPECT_EQ(b.Num(), 2);
	EXPECT_EQ(b.Denom(), 3);
}

TEST(fraction, addition)
{
	const Fraction a = {3, 5};
	EXPECT_EQ(a + Fraction(2, 3), Fraction(19, 15));
}

TEST(fraction, addition_assignment)
{
	Fraction a = {3, 5};
	a += {2, 3};
	EXPECT_EQ(a, Fraction(19, 15));
}

TEST(fraction, subtraction)
{
	const Fraction a = {3, 5};
	EXPECT_EQ(a - Fraction(2, 3), Fraction(-1, 15));
}

TEST(fraction, subtraction_assignment)
{
	Fraction a = {3, 5};
	a -= {2, 3};
	EXPECT_EQ(a, Fraction(-1, 15));
}

TEST(fraction, multiplication)
{
	const Fraction a = {3, 5};
	EXPECT_EQ(a * Fraction(14, 4), Fraction(21, 10));
}

TEST(fraction, multiplication_assignment)
{
	Fraction a = {3, 5};
	a *= Fraction(14, 4);
	EXPECT_EQ(a, Fraction(21, 10));
}

TEST(fraction, scalar_multiplication)
{
	const Fraction a = {2, 3};
	EXPECT_EQ(a * 6, Fraction(4, 1));
}

TEST(fraction, scalar_multiplication_assignment)
{
	Fraction a = {2, 3};
	a *= 6;
	EXPECT_EQ(a, Fraction(4, 1));
}

TEST(fraction, division)
{
	const Fraction a = {3, 5};
	EXPECT_EQ(a / Fraction(14, 4), Fraction(6, 35));
}

TEST(fraction, division_assignment)
{
	Fraction a = {3, 5};
	a /= Fraction(14, 4);
	EXPECT_EQ(a, Fraction(6, 35));
}

TEST(fraction, scalar_division)
{
	const Fraction a = {3, 5};
	EXPECT_EQ(a / 3, Fraction(1, 5));
}

TEST(fraction, scalar_division_assignment)
{
	Fraction a = {3, 5};
	a /= 3;
	EXPECT_EQ(a, Fraction(1, 5));
}

