// SPDX-FileCopyrightText:  2022-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

/*

bit_view
~~~~~~~~

The bit_view class is a wrapper around an 8-bit unsigned integer union member
that offers named access to one or more spans of bits.

For example:

union Register {
        uint8_t data = 0;
        bit_view<0, 1> first_bit;     // value is 0 or 1
        bit_view<1, 6> middle_6_bits; // value is 0 to 2^6-1
        bit_view<7, 1> last_bit;      // value is 0 or 1
};

It provides a view into a subset of its bits allowing them to be read,
written, assigned, flipped, cleared, and tested, without the need for
bit-twidling operations (such as shifting, masking, anding, or or'ing).

Constructing a bit_view is similar to C bitfields, however unlike C
bitfields, bit_views are free from undefined behavior and have been proven
using GCC's and Clang's undefined behavior sanitizers.

This gives us the benefits of bitfields without their specification downsides:
- they're succinct and clear to use
- they're just as fast as bitwise operators (maybe more so, being constexpr)
- they're self-documenting using their bit positions, sizes, and field names

Example usage
~~~~~~~~~~~~~

Assume we have a 8-bit audio control register that holds the card's:
- speaker-on state (off/on) starting at bit 0, using one bit
- stereo-output state (off/on) starting at bit 1, using one bit
- left channel pan starting at bit 2, using 3 bits
- right channel pan starting at bit 5, using 3 bits

bit_view's can make this register's elements self-documenting:

union AudioReg {
  uint8_t data = 0;
  bit_view<0, 1> speaker_on;
  bit_view<1, 1> stereo_output;
  bit_view<2, 3> left_pan;
  bit_view<5, 3> right_pan;
};

AudioReg reg = {data};

if (reg.speaker_on)
  enable_speaker();
else
  disable_speaker();

if (reg.stereo_output)
  stereo_pan(reg.left_pan, reg.right_pan)

8-bit limitation
~~~~~~~~~~~~~~~~

bit_views are endian-safe as they're limited wrapping 8-bit registers,
which are not affected by the byte-ordering of larger multi-byte type.
(compile-type assertions guarantee this as well).

To use bit_views with larger types or registers, the type should be
accessible in it's explicit 8-bit parts (for example, a uint32_t can be
represented as an array of four uin8_t), each element of which can be
assigned to a bit_view-based union.

This is deliberate because the byte-ordering of the bit-view's data cannot
be assumed. For example, the byte ordering of a 16-bit register populated
within a DOS emulator will be little-endian, even when running on a
big-endian host, where as native data types on big-endian hardware will be
big-endian.
*/

#ifndef BIT_VIEW_H_
#define BIT_VIEW_H_

#include <cassert>
#include <type_traits>

#include "utils/bitops.h"
#include "misc/support.h"

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

	static_assert(view_index + view_width <= std::numeric_limits<uint8_t>::digits,
	              "the bit_view's extents need to fit within an uint8_t data type");

	// ensure the right-hand-side fits in the data type
	template <typename rhs_type>
	constexpr void check_rhs([[maybe_unused]] const rhs_type rhs_value) noexcept
	{
		// detect assignment of negative values
		if constexpr (std::is_signed_v<rhs_type>) {
			assert(rhs_value >= 0);
		}
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

	// all-purpose assignment
	template <class rhs_type>
	constexpr bit_view& Assign(const rhs_type& rhs_value) noexcept
	{
		if constexpr (std::is_integral_v<rhs_type>) {
			check_rhs(rhs_value);
		}
		const auto outer = data & ~mask::shifted;
		const auto inner = (rhs_value & mask::unshifted) << view_index;

		data = static_cast<data_type>(outer | inner);
		return *this;
	}

public:
	// trivial constructors
	constexpr bit_view() = default;
	constexpr bit_view(const bit_view&) = default;

	// construct from any T
	template <class rhs_type>
	constexpr bit_view(const rhs_type& rhs_value) noexcept
	{
		(void)Assign(rhs_value);
	}
	// assign from bit_view (equal layout)
	constexpr bit_view& operator=(const bit_view& other) noexcept
	{
		const data_type value = other;
		return Assign(value);
	}

	// assign from bit_view (different layout)
	template <int other_index, int other_width>
	constexpr bit_view& operator=(const bit_view<other_index, other_width>& other) noexcept
	{
		static_assert(view_width >= other_width,
		              "this bit_view has too few bits to accomodate the assignment");
		const data_type value = other;
		return Assign(value);
	}

	// assign from bool
	constexpr bit_view& operator=(const bool b) noexcept
	{
		static_assert(view_width == 1,
		              "Only 1-bit-wide bit_views can be unambiguously assigned from bool");

		constexpr uint8_t bool_to_val[2] = {0, 1};
		return Assign(bool_to_val[static_cast<size_t>(b)]);
	}

	// assign from any T
	template <class rhs_type>
	constexpr bit_view& operator=(const rhs_type& rhs_value) noexcept
	{
		return Assign(rhs_value);
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
		return Assign(*this + 1);
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
		return Assign(*this + rhs_value);
	}

	// pre-decrement the view's value
	constexpr bit_view &operator--() noexcept
	{
		return Assign(*this - 1);
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
		return Assign(*this - rhs_value);
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
