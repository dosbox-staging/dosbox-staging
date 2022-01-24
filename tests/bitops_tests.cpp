/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2021-2021  The DOSBox Staging Team
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

#include "bitops.h"

#include <gtest/gtest.h>

namespace {

TEST(bitops, enum_vals)
{
	// check against bit-shifts
	EXPECT_FALSE(bit_0 == 0 << 0);
	EXPECT_TRUE(bit_0 == 1 << 0); // this is why
	EXPECT_FALSE(bit_0 == 1 << 1);

	EXPECT_FALSE(bit_5 == 1 << 4);
	EXPECT_TRUE(bit_5 == 1 << 5); // industry prefers
	EXPECT_FALSE(bit_5 == 1 << 6);

	EXPECT_FALSE(bit_12 == 1 << 11);
	EXPECT_TRUE(bit_12 == 1 << 12); // zero-based bit names
	EXPECT_FALSE(bit_12 == 1 << 13);

	EXPECT_FALSE(bit_22 == 1 << 21);
	EXPECT_TRUE(bit_22 == 1 << 22); // and not one-based
	EXPECT_FALSE(bit_22 == 1 << 23);

	EXPECT_FALSE(bit_31 == 1 << 30);
	EXPECT_TRUE(bit_31 == 1 << 31);

	// check against bit literals
	EXPECT_TRUE(bit_0 == 0b1);
	EXPECT_TRUE(bit_5 == 0b100000);
	EXPECT_TRUE(bit_12 == 0b1000000000000);
	EXPECT_TRUE(bit_22 == 0b10000000000000000000000);
	EXPECT_TRUE(static_cast<uint32_t>(bit_14 | bit_22 | bit_31) ==
	            0b10000000010000000100000000000000);

	// check against magic numbers
	EXPECT_TRUE(bit_0 == 1);
	EXPECT_TRUE(bit_5 == 32);
	EXPECT_TRUE(bit_12 == 4096);
	EXPECT_TRUE(bit_22 == 4194304);
	EXPECT_TRUE(static_cast<uint32_t>(bit_14 | bit_22 | bit_31) == 2151694336);

	// check some combos
	EXPECT_FALSE((bit_4 | bit_5) == 0b1'1000);
	EXPECT_TRUE((bit_4 | bit_5) == 0b11'0000);
	EXPECT_FALSE((bit_4 | bit_5) == 0b110'0000);
}

TEST(bitops, nominal_byte)
{
	const auto even_bits = bit_0 | bit_2 | bit_4 | bit_6;
	const auto odd_bits = bit_1 | bit_3 | bit_5 | bit_7;

	uint8_t reg = 0;
	set_bits(reg, odd_bits);
	EXPECT_EQ(reg, 0b10101010);

	set_bits(reg, even_bits);
	EXPECT_EQ(reg, 0b11111111);

	EXPECT_TRUE(are_bits_set(reg, bit_0));
	EXPECT_TRUE(are_bits_set(reg, bit_3));
	EXPECT_TRUE(are_bits_set(reg, bit_7));
	EXPECT_TRUE(are_bits_set(reg, even_bits));
	EXPECT_TRUE(are_bits_set(reg, odd_bits));

	EXPECT_FALSE(are_bits_cleared(reg, bit_0));
	EXPECT_FALSE(are_bits_cleared(reg, bit_3));
	EXPECT_FALSE(are_bits_cleared(reg, bit_7));
	EXPECT_FALSE(are_bits_cleared(reg, even_bits));
	EXPECT_FALSE(are_bits_cleared(reg, odd_bits));

	clear_bits(reg, odd_bits); // odd-off, even-on
	EXPECT_EQ(reg, even_bits);
	EXPECT_TRUE(are_bits_set(reg, even_bits));
	EXPECT_TRUE(are_bits_cleared(reg, odd_bits));

	toggle_bits(reg, odd_bits); // both are on
	EXPECT_TRUE(are_bits_set(reg, (odd_bits | even_bits)));
	EXPECT_FALSE(are_bits_cleared(reg, (odd_bits | even_bits)));

	toggle_bits(reg, even_bits); // odd-on, even-off
	EXPECT_TRUE(are_bits_set(reg, odd_bits));
	EXPECT_TRUE(are_bits_cleared(reg, even_bits));

	toggle_bits(reg, even_bits | odd_bits); // odd-off, even-on
	EXPECT_EQ(reg, even_bits);

	// set all bits
	set_all_bits(reg);
	EXPECT_EQ(reg, 0b1111'1111);
	EXPECT_TRUE(are_bits_set(reg, even_bits | odd_bits));
	EXPECT_TRUE(are_any_bits_set(reg, even_bits | odd_bits));
	EXPECT_FALSE(are_bits_cleared(reg, (odd_bits | even_bits)));

	// toggle all bits
	toggle_all_bits(reg);
	EXPECT_EQ(reg, 0b0000'0000);
	EXPECT_FALSE(are_bits_set(reg, even_bits | odd_bits));
	EXPECT_FALSE(are_any_bits_set(reg, even_bits | odd_bits));
	EXPECT_TRUE(are_bits_cleared(reg, (odd_bits | even_bits)));
}

TEST(bitops, nominal_word)
{
	const auto even_bits = bit_8 | bit_10 | bit_12 | bit_14;
	const auto odd_bits = bit_9 | bit_11 | bit_13 | bit_15;

	uint16_t reg = 0;
	set_bits(reg, odd_bits);
	EXPECT_EQ(reg, 0b10101010'00000000);

	set_bits(reg, even_bits);
	EXPECT_EQ(reg, 0b11111111'00000000);

	EXPECT_FALSE(are_bits_cleared(reg, bit_8));
	EXPECT_FALSE(are_bits_cleared(reg, bit_12));
	EXPECT_FALSE(are_bits_cleared(reg, bit_15));
	EXPECT_FALSE(are_bits_cleared(reg, even_bits));
	EXPECT_FALSE(are_bits_cleared(reg, odd_bits));

	clear_bits(reg, odd_bits); // odd-off, even-on
	EXPECT_EQ(reg, even_bits);
	EXPECT_TRUE(are_bits_set(reg, even_bits));
	EXPECT_TRUE(are_any_bits_set(reg, even_bits | odd_bits));
	EXPECT_TRUE(are_bits_cleared(reg, odd_bits));

	toggle_bits(reg, odd_bits); // both are on
	EXPECT_TRUE(are_bits_set(reg, (odd_bits | even_bits)));
	EXPECT_FALSE(are_bits_cleared(reg, (odd_bits | even_bits)));

	toggle_bits(reg, even_bits); // odd-on, even-off
	EXPECT_TRUE(are_bits_set(reg, odd_bits));
	EXPECT_TRUE(are_bits_cleared(reg, even_bits));

	toggle_bits(reg, even_bits | odd_bits); // odd-off, even-on
	EXPECT_EQ(reg, even_bits);

	// set all bits
	set_all_bits(reg);
	EXPECT_EQ(reg, 0b11111111'11111111);
	EXPECT_TRUE(are_bits_set(reg, even_bits | odd_bits));
	EXPECT_TRUE(are_any_bits_set(reg, even_bits | odd_bits));
	EXPECT_FALSE(are_bits_cleared(reg, (odd_bits | even_bits)));

	// toggle all bits
	toggle_all_bits(reg);
	EXPECT_EQ(reg, 0b00000000'00000000);
	EXPECT_FALSE(are_bits_set(reg, even_bits | odd_bits));
	EXPECT_FALSE(are_any_bits_set(reg, even_bits | odd_bits));
	EXPECT_TRUE(are_bits_cleared(reg, (odd_bits | even_bits)));
}

TEST(bitops, nominal_dword)
{
	const auto even_bits = bit_16 | bit_18 | bit_20 | bit_22 | bit_24 |
	                       bit_26 | bit_28 | bit_30;
	const auto odd_bits = bit_17 | bit_19 | bit_21 | bit_23 | bit_25 |
	                      bit_27 | bit_29 | bit_31;

	uint32_t reg = 0;

	set_bits(reg, even_bits);
	EXPECT_EQ(reg, 0b01010101'01010101'00000000'00000000);

	set_bits(reg, odd_bits);
	EXPECT_EQ(reg, 0b11111111'11111111'00000000'00000000);

	EXPECT_FALSE(are_bits_cleared(reg, bit_16));
	EXPECT_FALSE(are_bits_cleared(reg, bit_24));
	EXPECT_FALSE(are_bits_cleared(reg, bit_31));
	EXPECT_FALSE(are_bits_cleared(reg, even_bits));
	EXPECT_FALSE(are_bits_cleared(reg, odd_bits));

	clear_bits(reg, odd_bits); // odd-off, even-on
	EXPECT_EQ(reg, even_bits);
	EXPECT_TRUE(are_bits_set(reg, even_bits));
	EXPECT_TRUE(are_bits_cleared(reg, odd_bits));

	toggle_bits(reg, odd_bits); // both are on
	EXPECT_TRUE(are_bits_set(reg, (odd_bits | even_bits)));
	EXPECT_FALSE(are_bits_cleared(reg, (odd_bits | even_bits)));

	toggle_bits(reg, even_bits); // odd-on, even-off
	EXPECT_TRUE(are_bits_set(reg, odd_bits));
	EXPECT_TRUE(are_bits_cleared(reg, even_bits));

	toggle_bits(reg, even_bits | odd_bits); // odd-off, even-on
	EXPECT_EQ(reg, even_bits);

	// set all bits
	set_all_bits(reg);
	EXPECT_EQ(reg, 0b11111111'11111111'11111111'11111111);
	EXPECT_TRUE(are_bits_set(reg, even_bits | odd_bits));
	EXPECT_TRUE(are_any_bits_set(reg, even_bits | odd_bits));
	EXPECT_FALSE(are_bits_cleared(reg, (odd_bits | even_bits)));

	// toggle all bits
	toggle_all_bits(reg);
	EXPECT_EQ(reg, 0b00000000'00000000'00000000'00000000);
	EXPECT_FALSE(are_bits_set(reg, even_bits | odd_bits));
	EXPECT_FALSE(are_any_bits_set(reg, even_bits | odd_bits));
	EXPECT_TRUE(are_bits_cleared(reg, (odd_bits | even_bits)));
}

TEST(bitops, bits_too_wide_for_byte)
{
	uint8_t reg = 0;
	set_bits(reg, bit_7);
	EXPECT_TRUE(are_bits_set(reg, bit_7));
	EXPECT_DEBUG_DEATH({ set_bits(reg, bit_8); }, "");
	EXPECT_DEBUG_DEATH({ are_bits_set(reg, bit_8); }, "");

	clear_bits(reg, bit_7);
	EXPECT_TRUE(are_bits_cleared(reg, bit_7));
	EXPECT_DEBUG_DEATH({ clear_bits(reg, bit_8); }, "");
	EXPECT_DEBUG_DEATH({ are_bits_cleared(reg, bit_8); }, "");

	toggle_bits(reg, bit_7);
	EXPECT_TRUE(are_bits_set(reg, bit_7));
	EXPECT_DEBUG_DEATH({ toggle_bits(reg, bit_8); }, "");
}

TEST(bitops, bits_too_wide_for_word)
{
	uint16_t reg = 0;
	set_bits(reg, bit_8);
	EXPECT_TRUE(are_bits_set(reg, bit_8));
	EXPECT_DEBUG_DEATH({ set_bits(reg, bit_16); }, "");
	EXPECT_DEBUG_DEATH({ are_bits_set(reg, bit_16); }, "");

	clear_bits(reg, bit_8);
	EXPECT_TRUE(are_bits_cleared(reg, bit_8));
	EXPECT_DEBUG_DEATH({ clear_bits(reg, bit_16); }, "");
	EXPECT_DEBUG_DEATH({ are_bits_cleared(reg, bit_16); }, "");

	toggle_bits(reg, bit_8);
	EXPECT_TRUE(are_bits_set(reg, bit_8));
	EXPECT_DEBUG_DEATH({ toggle_bits(reg, bit_16); }, "");
}

TEST(bitops, bits_not_too_wide_for_dword)
{
	uint32_t reg = 0;
	set_bits(reg, bit_8);
	set_bits(reg, bit_24);
	set_bits(reg, bit_31);
	EXPECT_TRUE(are_bits_set(reg, (bit_8 | bit_24 | bit_31)));

	clear_bits(reg, bit_8);
	clear_bits(reg, bit_24);
	clear_bits(reg, bit_31);
	EXPECT_TRUE(are_bits_cleared(reg, (bit_8 | bit_24 | bit_31)));

	toggle_bits(reg, bit_8);
	toggle_bits(reg, bit_24);
	toggle_bits(reg, bit_31);
	EXPECT_TRUE(are_bits_set(reg, (bit_8 | bit_24 | bit_31)));
}

} // namespace
