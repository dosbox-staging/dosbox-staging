/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2022-2022  The DOSBox Staging Team
 *
 *  Thanks to Evan Teran for his public domain BitField class, on which
 *  this is based.
 *
 *  This program is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free Software
 *  Foundation; either version 2 of the License, or (at your option) any later
 *  version.
 *
 *  This program is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with
 *  this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
 *  Street, Fifth Floor, Boston, MA 02110-1301, USA.

bit_view
~~~~~~~~

The bit_view class is a wrapper around a data value, typically a union member:

union Register {
        uint8_t data = 0;
        bit_view<0, 1> first_bit;     // value is 0 or 1
        bit_view<1, 6> middle_6_bits; // value is 0 to 2^6-1
        bit_view<7, 1> last_bit;      // value is 0 or 1
};

It provides a view into a subset of its bits allowing them to be
read, written, assigned, flipped, cleared, and tested, without the need to for
the usual twiddling operations.

Constructing a bit_view is similar to C bitfields, however unlike C bitfields,
bit_views are free from undefined behavior and have been proven using GCC's
and Clang's undefined behavior sanitizers.

This gives us the benefits of bitfields without their specification downsides:
- they're succinct and clear to use
- they're just as fast as bitwise operators (maybe more so, being constexpr)
- they're self-documenting using their bit positions, sizes, and field names

Example usage
~~~~~~~~~~~~~

Assume we have a 16-bit audio control register that holds the card's:
- left volume at bit 1 using 6 bits
- right volume at bit 7, also 6 bits
- speaker on/off state at bit 13 with just one bit

The bit_view makes the register's logical elements self-documenting:

union AudioReg {
        uint16_t data = 0;
        bit_view<1, 6> left_volume;
        bit_view<7, 6> right_volume;
        bit_view<13, 1> speaker_on;
};

AudioReg reg = {data};

if (reg.speaker_on)
        enable_speaker();
else
        disable_speaker();

const auto left_percent = reg.left_volume / 63.0;
const auto right_percent = reg.right_volume / 63.0;
set_volume(left_percent, right_percent);
*/

#ifndef BIT_VIEW_H_
#define BIT_VIEW_H_

#include <cassert>
#include <type_traits>

#include "bitops.h"
#include "support.h"

template <int view_index, int view_width>
class bit_view {
private:
	// ensure the data type is large enough to hold the view
	using data_type = nearest_uint_t<view_index + view_width>;

	// compile-time assert that the view is valid
	static_assert(std::is_unsigned<data_type>::value,
	              "the bit_view's data type needs to be unsigned");

	static_assert(view_index >= 0,
	              "the bit_view's index needs to be zero or greater");

	static_assert(view_width > 0,
	              "the bit_view's width needs to span at least one bit");

	static_assert(view_index + view_width <= std::numeric_limits<data_type>::digits,
	              "the bit_view's extents need to fit in the data type");

	// ensure the right-hand-side is an integer and fits in the data type
	template <typename rhs_type>
	constexpr void check_rhs([[maybe_unused]] const rhs_type rhs_value) noexcept
	{
		// detect attempts to assign from non-integral types
		static_assert(std::is_integral<rhs_type>::value,
		              "the bit_view's value can only accept integral types");

		// detect use of bool, in which case the data is 1 bit and safe
		if (std::is_same<rhs_type, bool>::value)
			return;

		// detect assignments of negative values
		if (std::is_signed<rhs_type>::value)
			assert(rhs_value >= 0);

		// detect assignment of values that are too large
		[[maybe_unused]] constexpr uint64_t max_data_value = {0b1u << view_width};
		assert(static_cast<uint64_t>(rhs_value) < max_data_value);
	}

	// hold the view's masks using an enum to avoid using class storage
	enum mask : data_type {
		unshifted = (1u << view_width) - 1u,
		shifted   = unshifted << view_index,
	};

	// leave the member uninitialised; let union peer(s) initialize the data
	data_type data;

public:
	constexpr bit_view() = default;

	// trivial copy constructor
	bit_view(const bit_view &other) noexcept = default;

	// construct the view from a right-hand-side value
	template <class rhs_type>
	constexpr bit_view(const rhs_type rhs_value) noexcept
	{
		// use the = operator to save the value into the view
		*this = rhs_value;
	}

	// assign from right-hand-side boolean
	constexpr bit_view &operator=(const bool b) noexcept
	{
		constexpr uint8_t bool_to_val[2] = {0, 1};

		// use the = operator to save the value into the view
		return *this = bool_to_val[static_cast<size_t>(b)];
	}

	// assign from right-hand-side value
	template <class rhs_type>
	constexpr bit_view &operator=(const rhs_type rhs_value) noexcept
	{
		check_rhs(rhs_value);
		const auto outer = data & ~mask::shifted;
		const auto inner = (rhs_value & mask::unshifted) << view_index;

		data = static_cast<data_type>(outer | inner);
		return *this;
	}

	// assign the view from another bit_view if the same type
	constexpr bit_view &operator=(const bit_view &other) noexcept
	{
		// get the other view's value using the data_type() operator
		const data_type d = other;

		// use the = operator to save the value into the view
		return *this = d;
	}

	// read the view's value
	constexpr operator data_type() const noexcept
	{
		return (data & mask::shifted) >> view_index;
	}

	// pre-increment the view's value
	constexpr bit_view &operator++() noexcept
	{
		// use the = operator to save the value into the view
		return *this = *this + 1;
	}

	// post-increment the view's value
	constexpr bit_view operator++(int) noexcept
	{
		const auto b = *this;
		++*this;
		return b;
	}

	// increment the view's value by the right-hand side
	template <class rhs_type>
	constexpr bit_view &operator+=(const rhs_type rhs_value) noexcept
	{
		check_rhs(rhs_value);
		// use the = operator to save the value into the view
		return *this = *this + rhs_value;
	}

	// pre-decrement the view's value
	constexpr bit_view &operator--() noexcept
	{
		// use the = operator to save the value into the view
		return *this = *this - 1;
	}

	// post-decrement the view's value
	constexpr bit_view operator--(int) noexcept
	{
		const auto b = *this;
		--*this;
		return b;
	}

	// decrement the view's value by the right-hand side
	template <class rhs_type>
	constexpr bit_view &operator-=(const rhs_type rhs_value) noexcept
	{
		check_rhs(rhs_value);
		return *this = *this - rhs_value;
	}

	// check if all the view's bits are set
	constexpr bool all() const noexcept
	{
		return bit::is(data, mask::shifted);
	}

	// check if any of the view's bits are set
	constexpr bool any() const noexcept
	{
		return bit::any(data, mask::shifted);
	}

	// check if none of the view's bits are set
	constexpr bool none() const noexcept
	{
		return bit::cleared(data, mask::shifted);
	}

	// expose the view's underlying data
	constexpr data_type get_data() const noexcept
	{
		return data & mask::shifted;
	}

	// flip the view's bits
	constexpr void flip() noexcept
	{
		bit::flip(data, mask::shifted);
	}

	// clear the view's bits
	constexpr void clear() noexcept
	{
		bit::clear(data, mask::shifted);
	}

	// get the numeric value of the view's bits
	constexpr data_type val() const noexcept
	{
		// use the view's value-cast operator to get the shifted and masked bits
		return static_cast<data_type>(*this);
	}
};

#endif
