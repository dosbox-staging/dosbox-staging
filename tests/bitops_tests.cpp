// SPDX-FileCopyrightText:  2021-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "utils/bitops.h"

#include <gtest/gtest.h>

namespace {

TEST(bitops, enum_vals)
{
	using namespace bit::literals;

	// check against bit-shifts
	EXPECT_FALSE(b0 == 0 << 0);
	EXPECT_TRUE(b0 == 1 << 0); // this is why
	EXPECT_FALSE(b0 == 1 << 1);

	EXPECT_FALSE(b5 == 1 << 4);
	EXPECT_TRUE(b5 == 1 << 5); // industry prefers
	EXPECT_FALSE(b5 == 1 << 6);

	EXPECT_FALSE(b12 == 1 << 11);
	EXPECT_TRUE(b12 == 1 << 12); // zero-based bit names
	EXPECT_FALSE(b12 == 1 << 13);

	EXPECT_FALSE(b22 == 1 << 21);
	EXPECT_TRUE(b22 == 1 << 22); // and not one-based
	EXPECT_FALSE(b22 == 1 << 23);

	EXPECT_FALSE(b31 == 1 << 30);
	EXPECT_TRUE(b31 == 0b1u << 31);

	// check against bit literals
	EXPECT_TRUE(b0 == 0b1);
	EXPECT_TRUE(b5 == 0b100000);
	EXPECT_TRUE(b12 == 0b1000000000000);
	EXPECT_TRUE(b22 == 0b10000000000000000000000);
	EXPECT_TRUE(static_cast<uint32_t>(b14 | b22 | b31) ==
	            0b10000000010000000100000000000000);

	// check against magic numbers
	EXPECT_TRUE(b0 == 1);
	EXPECT_TRUE(b5 == 32);
	EXPECT_TRUE(b12 == 4096);
	EXPECT_TRUE(b22 == 4194304);
	EXPECT_TRUE(static_cast<uint32_t>(b14 | b22 | b31) == 2151694336);

	// check some combos
	EXPECT_FALSE((b4 | b5) == 0b1'1000);
	EXPECT_TRUE((b4 | b5) == 0b11'0000);
	EXPECT_FALSE((b4 | b5) == 0b110'0000);
}

TEST(bitops, nominal_byte)
{
	using namespace bit::literals;

	const auto even_bits = (b0 | b2 | b4 | b6);
	const auto odd_bits = (b1 | b3 | b5 | b7);

	uint8_t reg = 0;
	bit::set(reg, odd_bits);
	EXPECT_EQ(reg, 0b10101010);

	bit::set(reg, even_bits);
	EXPECT_EQ(reg, 0b11111111);

	EXPECT_TRUE(bit::is(reg, b0));
	EXPECT_TRUE(bit::is(reg, b3));
	EXPECT_TRUE(bit::is(reg, b7));
	EXPECT_TRUE(bit::is(reg, even_bits));
	EXPECT_TRUE(bit::is(reg, odd_bits));

	EXPECT_FALSE(bit::cleared(reg, b0));
	EXPECT_FALSE(bit::cleared(reg, b3));
	EXPECT_FALSE(bit::cleared(reg, b7));
	EXPECT_FALSE(bit::cleared(reg, even_bits));
	EXPECT_FALSE(bit::cleared(reg, odd_bits));

	bit::clear(reg, odd_bits); // odd-off, even-on
	EXPECT_EQ(reg, even_bits);
	EXPECT_TRUE(bit::is(reg, even_bits));
	EXPECT_TRUE(bit::cleared(reg, odd_bits));

	bit::flip(reg, odd_bits); // both is on
	EXPECT_TRUE(bit::is(reg, odd_bits | even_bits));
	EXPECT_FALSE(bit::cleared(reg, odd_bits | even_bits));

	bit::flip(reg, even_bits); // odd-on, even-off
	EXPECT_TRUE(bit::is(reg, odd_bits));
	EXPECT_TRUE(bit::cleared(reg, even_bits));

	bit::flip(reg, even_bits | odd_bits); // odd-off, even-on
	EXPECT_EQ(reg, even_bits);

	// set all bits
	bit::set_all(reg);
	EXPECT_EQ(reg, 0b1111'1111);
	EXPECT_TRUE(bit::is(reg, even_bits | odd_bits));
	EXPECT_TRUE(bit::any(reg, even_bits | odd_bits));
	EXPECT_FALSE(bit::cleared(reg, odd_bits | even_bits));

	// flip all bits
	bit::flip_all(reg);
	EXPECT_EQ(reg, 0b0000'0000);
	EXPECT_FALSE(bit::is(reg, even_bits | odd_bits));
	EXPECT_FALSE(bit::any(reg, even_bits | odd_bits));
	EXPECT_TRUE(bit::cleared(reg, odd_bits | even_bits));

	// set bits to specific bool state
	reg = {};
	bit::set_to(reg, b7 | b0, true);
	EXPECT_EQ(reg, 0b1000'0001);

	bit::set_to(reg, b3, true);
	EXPECT_EQ(reg, 0b1000'1001);

	bit::set_to(reg, b3, false);
	EXPECT_EQ(reg, 0b1000'0001);

	bit::set_to(reg, b7 | b0, false);
	EXPECT_EQ(reg, 0b0000'0000);
}

TEST(bitops, nominal_word)
{
	using namespace bit::literals;

	const auto even_bits = (b8 | b10 | b12 | b14);
	const auto odd_bits = (b9 | b11 | b13 | b15);

	uint16_t reg = 0;
	bit::set(reg, odd_bits);
	EXPECT_EQ(reg, 0b10101010'00000000);

	bit::set(reg, even_bits);
	EXPECT_EQ(reg, 0b11111111'00000000);

	EXPECT_FALSE(bit::cleared(reg, b8));
	EXPECT_FALSE(bit::cleared(reg, b12));
	EXPECT_FALSE(bit::cleared(reg, b15));
	EXPECT_FALSE(bit::cleared(reg, even_bits));
	EXPECT_FALSE(bit::cleared(reg, odd_bits));

	bit::clear(reg, odd_bits); // odd-off, even-on
	EXPECT_EQ(reg, even_bits);
	EXPECT_TRUE(bit::is(reg, even_bits));
	EXPECT_TRUE(bit::any(reg, even_bits | odd_bits));
	EXPECT_TRUE(bit::cleared(reg, odd_bits));

	bit::flip(reg, odd_bits); // both is on
	EXPECT_TRUE(bit::is(reg, odd_bits | even_bits));
	EXPECT_FALSE(bit::cleared(reg, odd_bits | even_bits));

	bit::flip(reg, even_bits); // odd-on, even-off
	EXPECT_TRUE(bit::is(reg, odd_bits));
	EXPECT_TRUE(bit::cleared(reg, even_bits));

	bit::flip(reg, even_bits | odd_bits); // odd-off, even-on
	EXPECT_EQ(reg, even_bits);

	// set all bits
	bit::set_all(reg);
	EXPECT_EQ(reg, 0b11111111'11111111);
	EXPECT_TRUE(bit::is(reg, even_bits | odd_bits));
	EXPECT_TRUE(bit::any(reg, even_bits | odd_bits));
	EXPECT_FALSE(bit::cleared(reg, odd_bits | even_bits));

	// flip all bits
	bit::flip_all(reg);
	EXPECT_EQ(reg, 0b00000000'00000000);
	EXPECT_FALSE(bit::is(reg, even_bits | odd_bits));
	EXPECT_FALSE(bit::any(reg, even_bits | odd_bits));
	EXPECT_TRUE(bit::cleared(reg, odd_bits | even_bits));
}

TEST(bitops, nominal_dword)
{
	using namespace bit::literals;

	constexpr uint32_t even_bits = {b16 | b18 | b20 | b22 | b24 | b26 | b28 | b30};
	constexpr uint32_t odd_bits = {b17 | b19 | b21 | b23 | b25 | b27 | b29 | b31};

	uint32_t reg = 0;

	bit::set(reg, even_bits);
	EXPECT_EQ(reg, 0b01010101'01010101'00000000'00000000);

	bit::set(reg, odd_bits);
	EXPECT_EQ(reg, 0b11111111'11111111'00000000'00000000);

	EXPECT_FALSE(bit::cleared(reg, b16));
	EXPECT_FALSE(bit::cleared(reg, b24));
	EXPECT_FALSE(bit::cleared(reg, b31));
	EXPECT_FALSE(bit::cleared(reg, even_bits));
	EXPECT_FALSE(bit::cleared(reg, odd_bits));

	bit::clear(reg, odd_bits); // odd-off, even-on
	EXPECT_EQ(reg, even_bits);
	EXPECT_TRUE(bit::is(reg, even_bits));
	EXPECT_TRUE(bit::cleared(reg, odd_bits));

	bit::flip(reg, odd_bits); // both is on
	EXPECT_TRUE(bit::is(reg, odd_bits | even_bits));
	EXPECT_FALSE(bit::cleared(reg, odd_bits | even_bits));

	bit::flip(reg, even_bits); // odd-on, even-off
	EXPECT_TRUE(bit::is(reg, odd_bits));
	EXPECT_TRUE(bit::cleared(reg, even_bits));

	bit::flip(reg, even_bits | odd_bits); // odd-off, even-on
	EXPECT_EQ(reg, even_bits);

	// set all bits
	bit::set_all(reg);
	EXPECT_EQ(reg, 0b11111111'11111111'11111111'11111111);
	EXPECT_TRUE(bit::is(reg, even_bits | odd_bits));
	EXPECT_TRUE(bit::any(reg, even_bits | odd_bits));
	EXPECT_FALSE(bit::cleared(reg, odd_bits | even_bits));

	// flip all bits
	bit::flip_all(reg);
	EXPECT_EQ(reg, 0b00000000'00000000'00000000'00000000);
	EXPECT_FALSE(bit::is(reg, even_bits | odd_bits));
	EXPECT_FALSE(bit::any(reg, even_bits | odd_bits));
	EXPECT_TRUE(bit::cleared(reg, odd_bits | even_bits));
}

TEST(bitops, bits_too_wide_for_byte)
{
	using namespace bit::literals;

	uint8_t reg = 0;
	bit::set(reg, b7);
	EXPECT_TRUE(bit::is(reg, b7));
	EXPECT_DEBUG_DEATH({ bit::set(reg, b8); }, "");
	EXPECT_DEBUG_DEATH({ bit::is(reg, b8); }, "");

	bit::clear(reg, b7);
	EXPECT_TRUE(bit::cleared(reg, b7));
	EXPECT_DEBUG_DEATH({ bit::clear(reg, b8); }, "");
	EXPECT_DEBUG_DEATH({ bit::cleared(reg, b8); }, "");

	bit::flip(reg, b7);
	EXPECT_TRUE(bit::is(reg, b7));
	EXPECT_DEBUG_DEATH({ bit::flip(reg, b8); }, "");
}

TEST(bitops, bits_too_wide_for_word)
{
	using namespace bit::literals;

	uint16_t reg = 0;
	bit::set(reg, b8);
	EXPECT_TRUE(bit::is(reg, b8));
	EXPECT_DEBUG_DEATH({ bit::set(reg, b16); }, "");
	EXPECT_DEBUG_DEATH({ bit::is(reg, b16); }, "");

	bit::clear(reg, b8);
	EXPECT_TRUE(bit::cleared(reg, b8));
	EXPECT_DEBUG_DEATH({ bit::clear(reg, b16); }, "");
	EXPECT_DEBUG_DEATH({ bit::cleared(reg, b16); }, "");

	bit::flip(reg, b8);
	EXPECT_TRUE(bit::is(reg, b8));
	EXPECT_DEBUG_DEATH({ bit::flip(reg, b16); }, "");
}

TEST(bitops, bits_not_too_wide_for_dword)
{
	using namespace bit::literals;

	uint32_t reg = 0;
	bit::set(reg, b8);
	bit::set(reg, b24);
	bit::set(reg, b31);
	EXPECT_TRUE(bit::is(reg, b8 | b24 | b31));

	bit::clear(reg, b8);
	bit::clear(reg, b24);
	bit::clear(reg, b31);
	EXPECT_TRUE(bit::cleared(reg, b8 | b24 | b31));

	bit::flip(reg, b8);
	bit::flip(reg, b24);
	bit::flip(reg, b31);
	EXPECT_TRUE(bit::is(reg, b8 | b24 | b31));
}

// Retain operations
TEST(bitops, retain)
{
	using namespace bit::literals;

	// Retain a positive bit, with surrounding other bits
	uint8_t reg = (b0 | b1 | b2);
	bit::retain(reg, b1);
	EXPECT_EQ(reg, b1);

	// Retain a negative bit, with surrounding other bits
	reg = (b0 | b1 | b2 | b3 /*| b4*/ | b5 | b6 | b7);
	bit::retain(reg, b4);
	EXPECT_EQ(reg, 0);
}

// Masking operations
TEST(bitops, masking)
{
	using namespace bit::literals;

	// Prepopulated register with bit 4 cleared:
	constexpr uint8_t reg = (b0 | b1 | b2 | b3 /*| b4*/ | b5 | b6 | b7);

	EXPECT_TRUE(bit::is(bit::mask_on(reg, b4), b4));
	EXPECT_TRUE(bit::cleared(bit::mask_off(reg, b4), b4));
	EXPECT_TRUE(bit::any(bit::mask_to(reg, b4, true), b4));
	EXPECT_TRUE(bit::cleared(bit::mask_to(reg, b4, false), b4));
	EXPECT_TRUE(bit::any(bit::mask_flip(reg, b4), b4));
	EXPECT_TRUE(bit::is(bit::mask_flip_all(reg), b4));
}

} // namespace
