/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2022-2022  The DOSBox Staging Team
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

#include "bit_view.h"

#include <gtest/gtest.h>
#include <memory>

namespace {

union Register {
	uint8_t data = 0;
	bit_view<0, 2> first_2;  // is bits 0 and 1
	bit_view<2, 3> middle_3; // is bits 2, 3, and 4
	bit_view<5, 3> last_3;   // is bits 5, 6, and 7
};

// function to take in and return a Register by value
Register pass_by_value(const Register in)
{
	// empty register
	Register out = {0};

	// assign just the middle_3
	out.middle_3 = in.middle_3;

	// return by value
	return out;
}

// function to take in a reference of a Register and set its bits
void set_bits(Register &reg, uint8_t first, uint8_t middle, uint8_t last)
{
	reg.first_2  = first;
	reg.middle_3 = middle;
	reg.last_3   = last;
}

// function to take in a reference of a Register and get its bits
void get_bits(const Register &reg, uint8_t &first, uint8_t &middle, uint8_t &last)
{
	first  = reg.first_2;
	middle = reg.middle_3;
	last   = reg.last_3;
}

// function to take in a pointer to a Register and set its bits
void set_bits(Register *reg, uint8_t first, uint8_t middle, uint8_t last)
{
	reg->first_2  = first;
	reg->middle_3 = middle;
	reg->last_3   = last;
}

// function to take in a pointer to a Register and get its bits
void get_bits(const Register *reg, uint8_t &first, uint8_t &middle, uint8_t &last)
{
	first  = reg->first_2;
	middle = reg->middle_3;
	last   = reg->last_3;
}

TEST(bit_view, assign_from_literal)
{
	constexpr Register r = {0b111'000'11};

	EXPECT_EQ(r.first_2, 0b11);
	EXPECT_EQ(r.middle_3, 0b000);
	EXPECT_EQ(r.last_3, 0b111);
}

TEST(bit_view, assign_to_data)
{
	Register r1 = {0b111'000'11};
	Register r2 = {0b000'111'00};

	// storage-to-storage assignment
	r1.data = r2.data;

	EXPECT_EQ(r1.first_2, 0b00);
	EXPECT_EQ(r1.middle_3, 0b111);
	EXPECT_EQ(r1.last_3, 0b000);
}

TEST(bit_view, assign_from_parts)
{
	Register r1 = {0b111'000'11};
	Register r2 = {0b000'111'00};

	r1.middle_3 = r2.middle_3;

	EXPECT_EQ(r1.first_2, 0b11);
	EXPECT_EQ(r1.middle_3, 0b111);
	EXPECT_EQ(r1.last_3, 0b111);
}

TEST(bit_view, flip)
{
	Register r = {0b111'000'11};

	r.middle_3.flip();

	EXPECT_EQ(r.first_2, 0b11);
	EXPECT_EQ(r.middle_3, 0b111);
	EXPECT_EQ(r.last_3, 0b111);
}

TEST(bit_view, val)
{
	Register r = {0b111'000'11};

	EXPECT_EQ(r.first_2.val(), 3);
	EXPECT_EQ(r.middle_3.val(), 0);
	EXPECT_EQ(r.last_3.val(), 7);

	r.middle_3.flip();

	EXPECT_EQ(r.first_2.val(), 3);
	EXPECT_EQ(r.middle_3.val(), 7);
	EXPECT_EQ(r.last_3.val(), 7);
}

TEST(bit_view, increment)
{
	Register r = {0b111'000'00};

	// post-increment
	EXPECT_EQ(r.first_2++, 0b00);
	EXPECT_EQ(r.first_2, 0b01);

	// make sure adjacent bits are not affected
	EXPECT_EQ(r.middle_3, 0b000);
	EXPECT_EQ(r.last_3, 0b111);

	// pre-increment
	EXPECT_EQ(++r.first_2, 0b10);

	// make sure adjacent bits are not affected
	EXPECT_EQ(r.middle_3, 0b000);
	EXPECT_EQ(r.last_3, 0b111);

	// RHS value increment
	r.first_2 += 1;
	EXPECT_EQ(r.first_2, 0b11);

	// make sure adjacent bits are not affected
	EXPECT_EQ(r.middle_3, 0b000);
	EXPECT_EQ(r.last_3, 0b111);

	// overflow is caught with every increment method
	EXPECT_DEBUG_DEATH({ r.first_2++; }, "");
	EXPECT_DEBUG_DEATH({ ++r.first_2; }, "");
	EXPECT_DEBUG_DEATH({ r.first_2 += 1; }, "");

	// make sure adjacent bits are not affected
	EXPECT_EQ(r.middle_3, 0b000);
	EXPECT_EQ(r.last_3, 0b111);
}

TEST(bit_view, decrement)
{
	Register r = {0b111'000'11};

	// post-decrement
	EXPECT_EQ(r.first_2--, 0b11);
	EXPECT_EQ(r.first_2, 0b10);

	// make sure adjacent bits are not affected
	EXPECT_EQ(r.middle_3, 0b000);
	EXPECT_EQ(r.last_3, 0b111);

	// pre-decrement
	EXPECT_EQ(--r.first_2, 0b01);

	// make sure adjacent bits are not affected
	EXPECT_EQ(r.middle_3, 0b000);
	EXPECT_EQ(r.last_3, 0b111);

	// RHS value decrement
	r.first_2 -= 1;
	EXPECT_EQ(r.first_2, 0b00);
	// next decrement will underflow

	// make sure adjacent bits are not affected
	EXPECT_EQ(r.middle_3, 0b000);
	EXPECT_EQ(r.last_3, 0b111);

	// underflow is caught with every decrement method
	EXPECT_DEBUG_DEATH({ r.first_2--; }, "");
	EXPECT_DEBUG_DEATH({ --r.first_2; }, "");
	EXPECT_DEBUG_DEATH({ r.first_2 -= 1; }, "");

	// make sure adjacent bits are not affected
	EXPECT_EQ(r.middle_3, 0b000);
	EXPECT_EQ(r.last_3, 0b111);
}

TEST(bit_view, compare_with_bool)
{
	union RegSingles {
		uint8_t data = 0;
		bit_view<0, 1> bit0;
		bit_view<7, 1> bit7;
	};

	RegSingles reg = {0};

	reg.bit0 = true;
	EXPECT_EQ(reg.data, 0b00000001);
	EXPECT_EQ(reg.bit0, 1);
	EXPECT_TRUE(reg.bit0);
	EXPECT_TRUE(reg.bit0 == true);

	reg.bit7 = true;
	EXPECT_EQ(reg.data, 0b10000001);
	EXPECT_EQ(reg.bit7, 1);
	EXPECT_TRUE(reg.bit7 == true);

	reg.bit0 = false;
	EXPECT_EQ(reg.data, 0b10000000);
	EXPECT_EQ(reg.bit0, 0);

	EXPECT_FALSE(reg.bit0);
	EXPECT_TRUE(reg.bit0 != true);
	EXPECT_TRUE(reg.bit0 == false);
	EXPECT_FALSE(reg.bit0 == reg.bit7);
	EXPECT_TRUE(reg.bit0 != reg.bit7);
}

TEST(bit_view, clear)
{
	Register r = {0b111'111'11};

	r.middle_3.clear();

	EXPECT_EQ(r.first_2, 0b11);
	EXPECT_EQ(r.middle_3, 0b000);
	EXPECT_EQ(r.last_3, 0b111);
}

TEST(bit_view, boolean_checks)
{
	constexpr Register r = {0b111'010'00};

	EXPECT_TRUE(r.first_2.none());
	EXPECT_FALSE(r.first_2.any());
	EXPECT_FALSE(r.first_2.all());

	EXPECT_FALSE(r.middle_3.none());
	EXPECT_TRUE(r.middle_3.any());
	EXPECT_FALSE(r.middle_3.all());

	EXPECT_FALSE(r.last_3.none());
	EXPECT_TRUE(r.last_3.any());
	EXPECT_TRUE(r.last_3.all());
}

TEST(bit_view, equality)
{
	constexpr Register r1 = {0b111'010'00};
	constexpr Register r2 = {0b111'010'11};

	// equality tests
	EXPECT_FALSE(r1.first_2 == r2.first_2);
	EXPECT_TRUE(r1.middle_3 == r2.middle_3);
	EXPECT_TRUE(r1.last_3 == r2.last_3);

	// in-equality tests
	EXPECT_TRUE(r1.first_2 != r2.first_2);
	EXPECT_FALSE(r1.middle_3 != r2.middle_3);
	EXPECT_FALSE(r1.last_3 != r2.last_3);
}

TEST(bit_view, size_safety)
{
	// unions are sized to the largest member, so in this case,
	// it's the second view which is requires to be 16-bit to accomodate the
	// view's bit position and width.
	union RegisterSmallData {
		uint8_t data = 0;
		bit_view<0, 8> first_8;
		bit_view<8, 8> last_8;
	};

	RegisterSmallData r8 = {0b0000'1111};
	EXPECT_TRUE(r8.first_8 == 0b000'1111);

	// write into bits 8 through 15 are legal & safe
	r8.last_8 = 0b1111'0000;
	EXPECT_TRUE(r8.last_8 == 0b1111'0000);

	// make sure adjacent bits are not affected
	EXPECT_TRUE(r8.first_8 == 0b000'1111);
}

TEST(bit_view, illegal_view)
{
	union BadRegister {
		uint32_t data = 0;
		// the following fails to compile because the view is out of
		// range:

		//  bit_view<48, 128> too_large;

		// error: static_assert failed due to requirement 48 + 128 <=
		// std::numeric_limits<unsigned long>::digits' "bit_view cannot
		// exceed the number of bits in the data_type"
	};
}

TEST(bit_view, pass_by_value)
{
	const Register in = {0b111'010'11};

	// The function assigns and returns just the middle_3 from 'in'
	const auto out = pass_by_value(in);

	//  should only have middle_3 set from in
	EXPECT_EQ(out.middle_3, in.middle_3);
	EXPECT_EQ(out.middle_3, 0b010);

	// first_2 and last_3 should still be zeros
	EXPECT_EQ(out.first_2, 0b00);
	EXPECT_EQ(out.last_3, 0b000);
}

TEST(bit_view, writable_via_reference)
{
	// create a register and set it bits using the "set_bits" function
	Register r = {0};

	constexpr auto first_val  = 0b11;
	constexpr auto middle_val = 0b010;
	constexpr auto last_val   = 0b111;

	set_bits(r, first_val, middle_val, last_val);

	EXPECT_EQ(r.first_2, first_val);
	EXPECT_EQ(r.middle_3, middle_val);
	EXPECT_EQ(r.last_3, last_val);
}

TEST(bit_view, readable_via_reference)
{
	// create a register and set it bits using the "set_bits" function
	constexpr Register r = {0b111'010'11};

	uint8_t first_val  = 0;
	uint8_t middle_val = 0;
	uint8_t last_val   = 0;

	get_bits(r, first_val, middle_val, last_val);

	EXPECT_EQ(first_val, 0b11);
	EXPECT_EQ(middle_val, 0b010);
	EXPECT_EQ(last_val, 0b111);
}

TEST(bit_view, writable_via_pointer)
{
	// create a register and set it bits using the "set_bits" function
	Register r = {0};

	constexpr auto first_val  = 0b11;
	constexpr auto middle_val = 0b010;
	constexpr auto last_val   = 0b111;

	set_bits(&r, first_val, middle_val, last_val);

	EXPECT_EQ(r.first_2, first_val);
	EXPECT_EQ(r.middle_3, middle_val);
	EXPECT_EQ(r.last_3, last_val);
}

TEST(bit_view, readable_via_pointer)
{
	// create a register and set it bits using the "set_bits" function
	constexpr Register r = {0b111'010'11};

	uint8_t first_val  = 0;
	uint8_t middle_val = 0;
	uint8_t last_val   = 0;

	get_bits(&r, first_val, middle_val, last_val);

	EXPECT_EQ(first_val, 0b11);
	EXPECT_EQ(middle_val, 0b010);
	EXPECT_EQ(last_val, 0b111);
}

TEST(bit_view, create_with_new)
{
	// create a register and set it bits using the "set_bits" function
	auto r = new Register{0b111'010'11};

	constexpr auto first_val  = 0b11;
	constexpr auto middle_val = 0b010;
	constexpr auto last_val   = 0b111;

	set_bits(r, first_val, middle_val, last_val);

	EXPECT_EQ(r->first_2, first_val);
	EXPECT_EQ(r->middle_3, middle_val);
	EXPECT_EQ(r->last_3, last_val);

	delete r;
	r = nullptr;
}

TEST(bit_view, create_with_make_unique)
{
	// create a register and set it bits using the "set_bits" function
	auto r = std::make_unique<Register>();

	r->data = {0b111'010'11};

	constexpr auto first_val  = 0b11;
	constexpr auto middle_val = 0b010;
	constexpr auto last_val   = 0b111;

	set_bits(r.get(), first_val, middle_val, last_val);

	EXPECT_EQ(r->first_2, first_val);
	EXPECT_EQ(r->middle_3, middle_val);
	EXPECT_EQ(r->last_3, last_val);

	r.reset();
}

TEST(bit_view, use_in_array)
{
	Register regs[2] = {{0b111'010'11}, {0b000'101'00}};

	// test a couple
	EXPECT_EQ(regs[0].first_2, 0b11);
	EXPECT_EQ(regs[1].middle_3, 0b101);

	// flip a couple
	regs[0].first_2.flip();
	regs[1].middle_3.flip();

	EXPECT_EQ(regs[0].first_2, regs[1].first_2);
	EXPECT_EQ(regs[0].middle_3, regs[1].middle_3);
}

TEST(bit_view, bare_initialization)
{
	const bit_view<0, 2> two_bits = {0b10};
	EXPECT_EQ(two_bits, 0b10);
	EXPECT_EQ(two_bits.get_data(), 0b000000'10);

	const bit_view<2, 3> three_bits = {0b101};
	EXPECT_EQ(three_bits, 0b101);
	EXPECT_EQ(three_bits.get_data(), 0b000'101'00);

	const bit_view<4, 4> four_bits = {0b1011};
	EXPECT_EQ(four_bits, 0b1011);
	EXPECT_EQ(four_bits.get_data(), 0b1011'0000);
}

TEST(bit_view, bare_constructor)
{
	const bit_view<0, 2> one_bit = {0b1};
	EXPECT_EQ(one_bit, 0b1);
	EXPECT_EQ(one_bit.get_data(), 0b000000'1);

	const bit_view<0, 2> two_bits(0b11);
	EXPECT_EQ(two_bits, 0b11);
	EXPECT_EQ(two_bits.get_data(), 0b000000'11);

	const bit_view<2, 3> three_bits(0b111);
	EXPECT_EQ(three_bits, 0b111);
	EXPECT_EQ(three_bits.get_data(), 0b000'111'00);

	const bit_view<4, 4> four_bits(0b1111);
	EXPECT_EQ(four_bits, 0b1111);
	EXPECT_EQ(four_bits.get_data(), 0b1111'0000);
}

} // namespace
