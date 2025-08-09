// SPDX-FileCopyrightText:  2022-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

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
constexpr uint8_t b0 = {0b1u << 0};
constexpr uint8_t b1 = {0b1u << 1};
constexpr uint8_t b2 = {0b1u << 2};
constexpr uint8_t b3 = {0b1u << 3};
constexpr uint8_t b4 = {0b1u << 4};
constexpr uint8_t b5 = {0b1u << 5};
constexpr uint8_t b6 = {0b1u << 6};
constexpr uint8_t b7 = {0b1u << 7};
constexpr uint16_t b8 = {0b1u << 8};
constexpr uint16_t b9 = {0b1u << 9};
constexpr uint16_t b10 = {0b1u << 10};
constexpr uint16_t b11 = {0b1u << 11};
constexpr uint16_t b12 = {0b1u << 12};
constexpr uint16_t b13 = {0b1u << 13};
constexpr uint16_t b14 = {0b1u << 14};
constexpr uint16_t b15 = {0b1u << 15};
constexpr uint32_t b16 = {0b1u << 16};
constexpr uint32_t b17 = {0b1u << 17};
constexpr uint32_t b18 = {0b1u << 18};
constexpr uint32_t b19 = {0b1u << 19};
constexpr uint32_t b20 = {0b1u << 20};
constexpr uint32_t b21 = {0b1u << 21};
constexpr uint32_t b22 = {0b1u << 22};
constexpr uint32_t b23 = {0b1u << 23};
constexpr uint32_t b24 = {0b1u << 24};
constexpr uint32_t b25 = {0b1u << 25};
constexpr uint32_t b26 = {0b1u << 26};
constexpr uint32_t b27 = {0b1u << 27};
constexpr uint32_t b28 = {0b1u << 28};
constexpr uint32_t b29 = {0b1u << 29};
constexpr uint32_t b30 = {0b1u << 30};
constexpr uint32_t b31 = {0b1u << 31};
} // namespace literals

// ensure the register is an unsigned integer, but not a bool
template <typename R>
static constexpr void check_reg_type()
{
	static_assert(std::is_unsigned<R>::value, "register must be unsigned");
	static_assert(std::is_same<R, bool>::value == false,
	              "register must not be a bool");
}

// ensure the bit state is a bool
template <typename S>
static constexpr void check_state_type()
{
	static_assert(std::is_same<S, bool>::value, "bit state must be a bool");
}

// Ensure the bits fall within the register's type size
template <typename T>
static constexpr void check_width([[maybe_unused]] const T reg,
                                  [[maybe_unused]] const unsigned int bits)
{
	check_reg_type<T>();
	assert(sizeof(reg) >= sizeof(bits) || bits <= std::numeric_limits<T>::max());
}

// Set the indicated bits
template <typename T>
constexpr T mask_on(const T reg, const unsigned int bits)
{
	check_width(reg, bits);
	return reg | static_cast<T>(bits);
}
template <typename T>
constexpr void set(T &reg, const unsigned int bits)
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
constexpr T make(const unsigned int bits)
{
	check_width(static_cast<T>(0), bits);
	return static_cast<T>(bits);
}

// Clear the indicated bits
template <typename T>
constexpr T mask_off(const T reg, const unsigned int bits)
{
	check_width(reg, bits);
	return reg & ~static_cast<T>(bits);
}
template <typename T>
constexpr void clear(T &reg, const unsigned int bits)
{
	reg = mask_off(reg, bits);
}

// Retain only the indicated bits, clearing the others
template <typename T>
constexpr void retain(T &reg, const unsigned int bits)
{
	check_width(reg, bits);
	reg = reg & static_cast<T>(bits);
}

// Set the indicated bits to the given bool value
template <typename T, typename S>
constexpr T mask_to(const T reg, const unsigned int bits, const S state)
{
	check_state_type<S>();
	check_width(reg, bits);
	return state ? mask_on(reg, bits) : mask_off(reg, bits);
}
template <typename T, typename S>
constexpr void set_to(T &reg, const unsigned int bits, const S state)
{
	reg = mask_to(reg, bits, state);
}

// flip the indicated bits
template <typename T>
constexpr T mask_flip(const T reg, const unsigned int bits)
{
	check_width(reg, bits);
	return reg ^ static_cast<T>(bits);
}
template <typename T>
constexpr void flip(T &reg, const unsigned int bits)
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
constexpr bool is(const T reg, const unsigned int bits)
{
	check_width(reg, bits);
	return (reg & static_cast<T>(bits)) == static_cast<T>(bits);
}

// Check if any one of the indicated bits is set
template <typename T>
constexpr bool any(const T reg, const unsigned int bits)
{
	check_width(reg, bits);
	return reg & static_cast<T>(bits);
}

// Check if the indicated bits is cleared (not set)
template <typename T>
constexpr bool cleared(const T reg, const unsigned int bits)
{
	check_width(reg, bits);
	return (reg & static_cast<T>(bits)) == 0;
}

} // namespace bit

// close the header
#endif
