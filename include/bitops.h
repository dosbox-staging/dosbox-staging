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
// uint8_t reg = 0;
//
// Setting:
//   set_bits(reg, bit_0 | bit_1); // 0b0000'0011
//   set_bits(reg, bit_8); // ASSERT FAIL (out of range)
//   set_all_bits(reg); // 0b1111'1111
//
// Toggling:
//   toggle_bits(reg, bit_4 | bit_5 | bit_6 | bit_7); // 0b0000'1111
//   toggle_bits(reg, bit_8); // ASSERT FAIL (out of range)
//   toggle_all_bits(reg); //  0b1111'0000
//
// Clearing:
//   clear_bits(reg, bit_4 | bit_5); // 0b1100'0000
//   clear_bits(reg, bit_8); // ASSERT FAIL (out of range)
//
// Checking:
//   are_bits_set(reg, bit_6); // true
//   are_bits_set(reg, bit_7); // true
//   are_bits_set(reg, bit_8); // ASSERT FAIL (out of range)
//   are_bits_set(reg, bit_6 | bit_7); // true (both need to be set)
//   are_bits_set(reg, bit_5 | bit_6); // false (both aren't set)
//   are_any_bits_set(reg, bit_5 | bit_6); // true (bit 6 is)
//   are_bits_cleared(reg, bit_0 | bit_1 | bit_2 | bit_3); // true
//

enum BITS {
	bit_0 = 1 << 0,
	bit_1 = 1 << 1,
	bit_2 = 1 << 2,
	bit_3 = 1 << 3,
	bit_4 = 1 << 4,
	bit_5 = 1 << 5,
	bit_6 = 1 << 6,
	bit_7 = 1 << 7,
	bit_8 = 1 << 8,
	bit_9 = 1 << 9,
	bit_10 = 1 << 10,
	bit_11 = 1 << 11,
	bit_12 = 1 << 12,
	bit_13 = 1 << 13,
	bit_14 = 1 << 14,
	bit_15 = 1 << 15,
	bit_16 = 1 << 16,
	bit_17 = 1 << 17,
	bit_18 = 1 << 18,
	bit_19 = 1 << 19,
	bit_20 = 1 << 20,
	bit_21 = 1 << 21,
	bit_22 = 1 << 22,
	bit_23 = 1 << 23,
	bit_24 = 1 << 24,
	bit_25 = 1 << 25,
	bit_26 = 1 << 26,
	bit_27 = 1 << 27,
	bit_28 = 1 << 28,
	bit_29 = 1 << 29,
	bit_30 = 1 << 30,
	bit_31 = 1 << 31
};

// Is the bitmask within the maximum value of the register?
template <typename T>
static constexpr void validate_bit_width([[maybe_unused]] const T reg, const int bits)
{
	// ensure the register is unsigned
	static_assert(std::is_unsigned<T>::value, "register must be unsigned");

	// Ensure the bits fall within the type size
	assert(static_cast<uint64_t>(bits) > 0);
	assert(static_cast<int64_t>(bits) <=
	       static_cast<int64_t>(std::numeric_limits<T>::max()));
}

// Set the indicated bits
template <typename T>
constexpr T set_bits_const(const T reg, const int bits)
{
	validate_bit_width(reg, bits);
	return reg | static_cast<T>(bits);
}
template <typename T>
constexpr void set_bits(T &reg, const int bits)
{
	reg = set_bits_const(reg, bits);
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
constexpr T all_bits_set()
{
	return std::is_signed<T>::value ? static_cast<T>(-1)
	                                : std::numeric_limits<T>::max();
}
template <typename T>
constexpr void set_all_bits(T &reg)
{
	reg = all_bits_set<T>();
}

// Clear the indicated bits
template <typename T>
constexpr T clear_bits_const(const T reg, const int bits)
{
	validate_bit_width(reg, bits);
	return reg & ~static_cast<T>(bits);
}
template <typename T>
constexpr void clear_bits(T &reg, const int bits)
{
	reg = clear_bits_const(reg, bits);
}

// Set the indicated bits to the given bool value
template <typename T>
constexpr T set_bits_const_to(const T reg, const int bits, const bool value)
{
	validate_bit_width(reg, bits);
	return value ? set_bits_const(reg, bits) : clear_bits_const(reg, bits);
}
template <typename T>
constexpr void set_bits_to(T &reg, const int bits, const bool value)
{
	reg = set_bits_const_to(reg, bits, value);
}

// Toggle the indicated bits
template <typename T>
constexpr T toggle_bits_const(const T reg, const int bits)
{
	validate_bit_width(reg, bits);
	return reg ^ static_cast<T>(bits);
}
template <typename T>
constexpr void toggle_bits(T &reg, const int bits)
{
	reg = toggle_bits_const(reg, bits);
}

// Toggle all the bits in the register
template <typename T>
constexpr T toggle_all_bits_const(const T reg)
{
	return reg ^ all_bits_set<T>();
}
template <typename T>
constexpr void toggle_all_bits(T &reg)
{
	reg = toggle_all_bits_const(reg);
}

// Check if the indicated bits are set
template <typename T>
constexpr bool are_bits_set(const T reg, const int bits)
{
	validate_bit_width(reg, bits);
	return (reg & static_cast<T>(bits)) == static_cast<T>(bits);
}

// Check if any one of the indicated bits are set
template <typename T>
constexpr bool are_any_bits_set(const T reg, const int bits)
{
	validate_bit_width(reg, bits);
	return reg & static_cast<T>(bits);
}

// Check if the indicated bits are cleared (not set)
template <typename T>
constexpr bool are_bits_cleared(const T reg, const int bits)
{
	validate_bit_width(reg, bits);
	return (reg & static_cast<T>(bits)) == 0;
}

// close the header
#endif
