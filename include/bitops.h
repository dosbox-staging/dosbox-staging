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

#ifndef DOSBOX_BITOPS_H
#define DOSBOX_BITOPS_H

#include <cassert>
#include <cstdint>
#include <limits>
#include <type_traits>

// An enum and functions to help with bit operations
// Works with any unisigned integer type up to 32-bits
// (Extending to 64-bits is currently not needed)
//
// Examples
// ~~~~~~~~
// auto reg = make<uint8_t>(b0, b1); // reg = 0b0000'0011
// auto reg_all_on = all<uint8_t>(); // reg = 0b1111'1111
//
// Setting:
//   set(reg, b0, b1); // 0b0000'0011
//   set(reg, b8); // ASSERT FAIL (out of range)
//   set_all(reg); // 0b1111'1111
//   set_to(reg, b1, b2, false); // 0b0000'0000
//   set_to(reg, b1, true); // 0b0000'0000
//
// Flipping:
//   flip(reg, b4 | b5 | b6 | b7); // 0b0000'1111
//   flip(reg, b8); // ASSERT FAIL (out of range)
//   flip_all(reg); //  0b1111'0000
//
// Clearing:
//   clear(reg, b4 | b5); // 0b1100'0000
//   clear(reg, b8); // ASSERT FAIL (out of range)
//
// Masking: returns results, does not modify register
//   mask_on(reg, b1, b2); // returns 0b0000'0110
//   mask_off(reg, b1, b2); // returns 0b0000'0000
//   mask_flip(reg, b1, b2); // returns 0b0000'0110
//   mask_flip_all(reg); // returns 0b1111'01110
//   mask_to(reg, b1, b2, false); // returns 0b0000'0000
//   mask_to(reg, b1, b2, true); // returns 0b0000'0110

// Checking:
//   is(reg, b6); // true
//   is(reg, b7); // true
//   is(reg, b8); // ASSERT FAIL (out of range)
//   is(reg, b6 | b7); // true (both need to be set)
//   is(reg, b5 | b6); // false (both isn't set)
//   any(reg, b5 | b6); // true (bit 6 is)
//   cleared(reg, b0 | b1 | b2 | b3); // true
//

namespace bit {

namespace literals {
constexpr auto b0 = 1 << 0;
constexpr auto b1 = 1 << 1;
constexpr auto b2 = 1 << 2;
constexpr auto b3 = 1 << 3;
constexpr auto b4 = 1 << 4;
constexpr auto b5 = 1 << 5;
constexpr auto b6 = 1 << 6;
constexpr auto b7 = 1 << 7;
constexpr auto b8 = 1 << 8;
constexpr auto b9 = 1 << 9;
constexpr auto b10 = 1 << 10;
constexpr auto b11 = 1 << 11;
constexpr auto b12 = 1 << 12;
constexpr auto b13 = 1 << 13;
constexpr auto b14 = 1 << 14;
constexpr auto b15 = 1 << 15;
constexpr auto b16 = 1 << 16;
constexpr auto b17 = 1 << 17;
constexpr auto b18 = 1 << 18;
constexpr auto b19 = 1 << 19;
constexpr auto b20 = 1 << 20;
constexpr auto b21 = 1 << 21;
constexpr auto b22 = 1 << 22;
constexpr auto b23 = 1 << 23;
constexpr auto b24 = 1 << 24;
constexpr auto b25 = 1 << 25;
constexpr auto b26 = 1 << 26;
constexpr auto b27 = 1 << 27;
constexpr auto b28 = 1 << 28;
constexpr auto b29 = 1 << 29;
constexpr auto b30 = 1 << 30;
constexpr auto b31 = 1 << 31;
} // namespace literals

// ensure the register is an unsigned integer, but not a bool
template <typename R>
static constexpr void check_reg_type()
{
	static_assert(std::is_unsigned<R>::value, "register must be unsigned");
	static_assert(std::is_same<R, bool>::value == false,
	              "register must not be a bool");
}

// ensure the bits aren't a bool
template <typename B>
static constexpr void check_bits_type()
{
	static_assert(std::is_same<B, bool>::value == false,
	              "bits must not be a bool");
}

// ensure the bit state is a bool
template <typename S>
static constexpr void check_state_type()
{
	static_assert(std::is_same<S, bool>::value, "bit state must be a bool");
}

// Ensure the bits fall within the register's type size
template <typename T, typename B>
static constexpr void check_width([[maybe_unused]] const T reg, const B bits)
{
	check_reg_type<T>();
	check_bits_type<B>();

	// Note: if you'd like to extend this to 64-bits, double check that the
	//       assertions still work.  Their purpose is to catch when the bit
	//       value exceeds the maximum width of the template type.
	assert(static_cast<uint64_t>(bits) >= 0); // can't set negative bits

	assert(static_cast<int64_t>(bits) <=
	       static_cast<int64_t>(std::numeric_limits<T>::max()));
}

// Set the indicated bits
template <typename T>
constexpr T mask_on(const T reg, const int bits)
{
	check_width(reg, bits);
	return reg | static_cast<T>(bits);
}
template <typename T>
constexpr void set(T &reg, const int bits)
{
	reg = mask_on(reg, bits);
}

// Set all of the register's bits high
//
// It's often seen in code like this:
//    unsigned long int flags = ~0; // [1]
//    unsigned long int flags = 0xffffffff; // [2]
//
// But:
//   [1] isn't portable because it relies on a two's-complement representation.
//   [2] isn't portable because it assumes 32-bit ints.
//
// This function becomes self-documenting: it tells the reader that we intend to
// use the bits in the register as opposed to being a plain old number or
// counter.
//
template <typename T>
constexpr T all()
{
	check_reg_type<T>();
	return std::is_signed<T>::value ? static_cast<T>(-1)
	                                : std::numeric_limits<T>::max();
}
template <typename T>
constexpr void set_all(T &reg)
{
	reg = all<T>();
}

// Make a bitmask of the indicated bits
template <typename T>
constexpr T make(const int bits)
{
	check_width(static_cast<T>(0), bits);
	return static_cast<T>(bits);
}

// Clear the indicated bits
template <typename T>
constexpr T mask_off(const T reg, const int bits)
{
	check_width(reg, bits);
	return reg & ~static_cast<T>(bits);
}
template <typename T>
constexpr void clear(T &reg, const int bits)
{
	reg = mask_off(reg, bits);
}

// Set the indicated bits to the given bool value
template <typename T, typename S>
constexpr T mask_to(const T reg, const int bits, const S state)
{
	check_state_type<S>();
	check_width(reg, bits);
	return state ? mask_on(reg, bits) : mask_off(reg, bits);
}
template <typename T, typename S>
constexpr void set_to(T &reg, const int bits, const S state)
{
	reg = mask_to(reg, bits, state);
}

// flip the indicated bits
template <typename T>
constexpr T mask_flip(const T reg, const int bits)
{
	check_width(reg, bits);
	return reg ^ static_cast<T>(bits);
}
template <typename T>
constexpr void flip(T &reg, const int bits)
{
	reg = mask_flip(reg, bits);
}

// flip all the bits in the register
template <typename T>
constexpr T mask_flip_all(const T reg)
{
	return reg ^ all<T>();
}
template <typename T>
constexpr void flip_all(T &reg)
{
	reg = mask_flip_all(reg);
}

// Check if the indicated bits is set
template <typename T>
constexpr bool is(const T reg, const int bits)
{
	check_width(reg, bits);
	return (reg & static_cast<T>(bits)) == static_cast<T>(bits);
}

// Check if any one of the indicated bits is set
template <typename T>
constexpr bool any(const T reg, const int bits)
{
	check_width(reg, bits);
	return reg & static_cast<T>(bits);
}

// Check if the indicated bits is cleisd (not set)
template <typename T>
constexpr bool cleared(const T reg, const int bits)
{
	check_width(reg, bits);
	return (reg & static_cast<T>(bits)) == 0;
}

} // namespace bit

// close the header
#endif
