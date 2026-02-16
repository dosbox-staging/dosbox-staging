// SPDX-FileCopyrightText:  2022-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "hardware/video/vga.h"

#include <gtest/gtest.h>

#include <array>
#include <optional>
#include <string>
#include <vector>

// declarations of private functions to test
std::optional<Rgb888> parse_color_token(const std::string& token,
                                       const uint8_t color_index);

std::optional<cga_colors_t> parse_cga_colors(const std::string& cga_colors_prefs);

namespace {

constexpr auto dummy_color_index = 0;

// HEX3 - VALID

TEST(parse_color_token, hex3_valid)
{
	const auto expected = Rgb888(0x11, 0xaa, 0xee);
	const auto result   = parse_color_token("#1ae", dummy_color_index);
	EXPECT_TRUE(result.has_value());
	EXPECT_EQ(expected, result);
}

TEST(parse_color_token, hex3_valid_min)
{
	const auto expected = Rgb888(0x00, 0x00, 0x00);
	const auto result   = parse_color_token("#000", dummy_color_index);
	EXPECT_TRUE(result.has_value());
	EXPECT_EQ(expected, result);
}

TEST(parse_color_token, hex3_valid_max)
{
	const auto expected = Rgb888(0xff, 0xff, 0xff);
	const auto result   = parse_color_token("#fff", dummy_color_index);
	EXPECT_TRUE(result.has_value());
	EXPECT_EQ(expected, result);
}

// HEX

TEST(parse_color_token, hex3_invalid_only_prefix)
{
	const auto result = parse_color_token("#", dummy_color_index);
	EXPECT_FALSE(result.has_value());
}

// HEX3 - INVALID

TEST(parse_color_token, hex3_invalid_too_short)
{
	const auto result = parse_color_token("#12", dummy_color_index);
	EXPECT_FALSE(result.has_value());
}

TEST(parse_color_token, hex3_invalid_too_long)
{
	const auto result = parse_color_token("#1234", dummy_color_index);
	EXPECT_FALSE(result.has_value());
}

TEST(parse_color_token, hex3_invalid_no_leading_hashmark)
{
	const auto result = parse_color_token("1ae", dummy_color_index);
	EXPECT_FALSE(result.has_value());
}

TEST(parse_color_token, hex3_invalid_character)
{
	const auto result = parse_color_token("#1ag", dummy_color_index);
	EXPECT_FALSE(result.has_value());
}

// HEX6 - VALID

TEST(parse_color_token, hex6_valid)
{
	const auto expected = Rgb888(0x12, 0xab, 0xef);
	const auto result   = parse_color_token("#12abef", dummy_color_index);
	EXPECT_TRUE(result.has_value());
	EXPECT_EQ(expected, result);
}

TEST(parse_color_token, hex6_valid_min)
{
	const auto expected = Rgb888(0x00, 0x00, 0x00);
	const auto result   = parse_color_token("#000000", dummy_color_index);
	EXPECT_TRUE(result.has_value());
	EXPECT_EQ(expected, result);
}

TEST(parse_color_token, hex6_valid_max)
{
	const auto expected = Rgb888(0xff, 0xff, 0xff);
	const auto result   = parse_color_token("#ffffff", dummy_color_index);
	EXPECT_TRUE(result.has_value());
	EXPECT_EQ(expected, result);
}

// HEX6 - INVALID

TEST(parse_color_token, hex6_invalid_no_leading_hashmark)
{
	const auto result = parse_color_token("aabbee", dummy_color_index);
	EXPECT_FALSE(result.has_value());
}

TEST(parse_color_token, hex6_invalid_too_short)
{
	const auto result = parse_color_token("#12345", dummy_color_index);
	EXPECT_FALSE(result.has_value());
}

TEST(parse_color_token, hex6_invalid_too_long)
{
	const auto result = parse_color_token("#1234567", dummy_color_index);
	EXPECT_FALSE(result.has_value());
}

// RGB triplet - VALID

TEST(parse_color_token, rgb_triplet_valid_no_whitespaces)
{
	const auto expected = Rgb888(7, 42, 231);
	const auto result   = parse_color_token("(7,42,231)", dummy_color_index);
	EXPECT_TRUE(result.has_value());
	EXPECT_EQ(expected, result);
}

TEST(parse_color_token, rgb_triplet_valid_single_whitespaces)
{
	const auto expected = Rgb888(7, 42, 231);
	const auto result   = parse_color_token("(7, 42, 231)", dummy_color_index);
	EXPECT_TRUE(result.has_value());
	EXPECT_EQ(expected, result);
}

TEST(parse_color_token, rgb_triplet_valid_multiple_whitespaces)
{
	const auto expected = Rgb888(7, 42, 231);
	const auto result   = parse_color_token("( 7 ,  42  ,   231  )",
                                                dummy_color_index);
	EXPECT_TRUE(result.has_value());
	EXPECT_EQ(expected, result);
}

// RGB triplet - INVALID

TEST(parse_color_token, rgb_triplet_invalid_empty)
{
	const auto result = parse_color_token("()", dummy_color_index);
	EXPECT_FALSE(result.has_value());
}

TEST(parse_color_token, rgb_triplet_only_commas)
{
	const auto result = parse_color_token("(,,)", dummy_color_index);
	EXPECT_FALSE(result.has_value());
}

TEST(parse_color_token, rgb_triplet_invalid_one_component)
{
	const auto result = parse_color_token("(1)", dummy_color_index);
	EXPECT_FALSE(result.has_value());
}

TEST(parse_color_token, rgb_triplet_invalid_four_components)
{
	const auto result = parse_color_token("(1,2,3,4)", dummy_color_index);
	EXPECT_FALSE(result.has_value());
}

TEST(parse_color_token, rgb_triplet_invalid_red_too_big)
{
	const auto result = parse_color_token("(256, 2, 3)", dummy_color_index);
	EXPECT_FALSE(result.has_value());
}

TEST(parse_color_token, rgb_triplet_invalid_green_too_big)
{
	const auto result = parse_color_token("(1, 256, 3)", dummy_color_index);
	EXPECT_FALSE(result.has_value());
}

TEST(parse_color_token, rgb_triplet_invalid_blue_too_big)
{
	const auto result = parse_color_token("(1, 2, 256)", dummy_color_index);
	EXPECT_FALSE(result.has_value());
}

TEST(parse_color_token, rgb_triplet_invalid_red_negative)
{
	const auto result = parse_color_token("(-1, 2, 3)", dummy_color_index);
	EXPECT_FALSE(result.has_value());
}

TEST(parse_color_token, rgb_triplet_invalid_green_negative)
{
	const auto result = parse_color_token("(1, -2, 3)", dummy_color_index);
	EXPECT_FALSE(result.has_value());
}

TEST(parse_color_token, rgb_triplet_invalid_blue_negative)
{
	const auto result = parse_color_token("(1, 2, -3)", dummy_color_index);
	EXPECT_FALSE(result.has_value());
}

TEST(parse_color_token, rgb_triplet_invalid_red_invalid)
{
	const auto result = parse_color_token("(1x, 2, 3)", dummy_color_index);
	EXPECT_FALSE(result.has_value());
}

TEST(parse_color_token, rgb_triplet_invalid_green_invalid)
{
	const auto result = parse_color_token("(1, 2x, 3)", dummy_color_index);
	EXPECT_FALSE(result.has_value());
}

TEST(parse_color_token, rgb_triplet_invalid_blue_invalid)
{
	const auto result = parse_color_token("(1, 2, 3x)", dummy_color_index);
	EXPECT_FALSE(result.has_value());
}

/////////////////////////////////////////////////////////////////////////////

TEST(parse_cga_colors, valid_hex3)
{
	const auto cga_colors_prefs = "  #012  #123, #234, #345 #456 #567 #678 #789 "
	                              "#89a #9ab #abc , #bcd #cde, #def #eff, #fff";

	const auto maybe_result = parse_cga_colors(cga_colors_prefs);
	EXPECT_TRUE(maybe_result.has_value());

	const auto result = maybe_result.value();

	EXPECT_EQ(result[0], (Rgb666(0x00, 0x04, 0x08)));
	EXPECT_EQ(result[1], (Rgb666(0x04, 0x08, 0x0c)));
	EXPECT_EQ(result[2], (Rgb666(0x08, 0x0c, 0x11)));
	EXPECT_EQ(result[3], (Rgb666(0x0c, 0x11, 0x15)));
	EXPECT_EQ(result[4], (Rgb666(0x11, 0x15, 0x19)));
	EXPECT_EQ(result[5], (Rgb666(0x15, 0x19, 0x1d)));
	EXPECT_EQ(result[6], (Rgb666(0x19, 0x1d, 0x22)));
	EXPECT_EQ(result[7], (Rgb666(0x1d, 0x22, 0x26)));
	EXPECT_EQ(result[8], (Rgb666(0x22, 0x26, 0x2a)));
	EXPECT_EQ(result[9], (Rgb666(0x26, 0x2a, 0x2e)));
	EXPECT_EQ(result[10], (Rgb666(0x2a, 0x2e, 0x33)));
	EXPECT_EQ(result[11], (Rgb666(0x2e, 0x33, 0x37)));
	EXPECT_EQ(result[12], (Rgb666(0x33, 0x37, 0x3b)));
	EXPECT_EQ(result[13], (Rgb666(0x37, 0x3b, 0x3f)));
	EXPECT_EQ(result[14], (Rgb666(0x3b, 0x3f, 0x3f)));
	EXPECT_EQ(result[15], (Rgb666(0x3f, 0x3f, 0x3f)));
}

TEST(parse_cga_colors, valid_hex6)
{
	const auto cga_colors_prefs = "#012345 #123456 #234567  #345678 "
	                              "#456789 #56789a #6789ab , #789abc "
	                              "#89abcd #9abcde ,#abcdef #bcdeff "
	                              "#cdefff, #deffff #efffff #ffffff  ";

	const auto maybe_result = parse_cga_colors(cga_colors_prefs);
	EXPECT_TRUE(maybe_result.has_value());

	const auto result = maybe_result.value();

	EXPECT_EQ(result[0], (Rgb666(0x00, 0x08, 0x11)));
	EXPECT_EQ(result[1], (Rgb666(0x04, 0x0d, 0x15)));
	EXPECT_EQ(result[2], (Rgb666(0x08, 0x11, 0x19)));
	EXPECT_EQ(result[3], (Rgb666(0x0d, 0x15, 0x1e)));
	EXPECT_EQ(result[4], (Rgb666(0x11, 0x19, 0x22)));
	EXPECT_EQ(result[5], (Rgb666(0x15, 0x1e, 0x26)));
	EXPECT_EQ(result[6], (Rgb666(0x19, 0x22, 0x2a)));
	EXPECT_EQ(result[7], (Rgb666(0x1e, 0x26, 0x2f)));
	EXPECT_EQ(result[8], (Rgb666(0x22, 0x2a, 0x33)));
	EXPECT_EQ(result[9], (Rgb666(0x26, 0x2f, 0x37)));
	EXPECT_EQ(result[10], (Rgb666(0x2a, 0x33, 0x3b)));
	EXPECT_EQ(result[11], (Rgb666(0x2f, 0x37, 0x3f)));
	EXPECT_EQ(result[12], (Rgb666(0x33, 0x3b, 0x3f)));
	EXPECT_EQ(result[13], (Rgb666(0x37, 0x3f, 0x3f)));
	EXPECT_EQ(result[14], (Rgb666(0x3b, 0x3f, 0x3f)));
	EXPECT_EQ(result[15], (Rgb666(0x3f, 0x3f, 0x3f)));
}

TEST(parse_cga_colors, valid_rgb_triplet)
{
	const auto cga_colors_prefs =
	        "(1,35,69), ( 18 , 52,86), ( 35,  69,103 ) , ( 52 ,86 ,120), "
	        "( 69, 103, 137), ( 86, 120, 154), (103, 137, 171), (120, 154, 188) "
	        "(137,171,205) (154,188,222) (171,205,239) (188,222,255) "
	        "(205, 239, 255)  ,  (222, 255, 255) (239, 255, 255) (255,255,255) ";

	const auto maybe_result = parse_cga_colors(cga_colors_prefs);
	EXPECT_TRUE(maybe_result.has_value());

	const auto result = maybe_result.value();

	EXPECT_EQ(result[0], (Rgb666(0x00, 0x08, 0x11)));
	EXPECT_EQ(result[1], (Rgb666(0x04, 0x0d, 0x15)));
	EXPECT_EQ(result[2], (Rgb666(0x08, 0x11, 0x19)));
	EXPECT_EQ(result[3], (Rgb666(0x0d, 0x15, 0x1e)));
	EXPECT_EQ(result[4], (Rgb666(0x11, 0x19, 0x22)));
	EXPECT_EQ(result[5], (Rgb666(0x15, 0x1e, 0x26)));
	EXPECT_EQ(result[6], (Rgb666(0x19, 0x22, 0x2a)));
	EXPECT_EQ(result[7], (Rgb666(0x1e, 0x26, 0x2f)));
	EXPECT_EQ(result[8], (Rgb666(0x22, 0x2a, 0x33)));
	EXPECT_EQ(result[9], (Rgb666(0x26, 0x2f, 0x37)));
	EXPECT_EQ(result[10], (Rgb666(0x2a, 0x33, 0x3b)));
	EXPECT_EQ(result[11], (Rgb666(0x2f, 0x37, 0x3f)));
	EXPECT_EQ(result[12], (Rgb666(0x33, 0x3b, 0x3f)));
	EXPECT_EQ(result[13], (Rgb666(0x37, 0x3f, 0x3f)));
	EXPECT_EQ(result[14], (Rgb666(0x3b, 0x3f, 0x3f)));
	EXPECT_EQ(result[15], (Rgb666(0x3f, 0x3f, 0x3f)));
}

TEST(parse_cga_colors, valid_mixed)
{
	const auto cga_colors_prefs =
	        "  #012  #123, #234, #345 "
	        "( 69, 103, 137), ( 86, 120, 154), (103, 137, 171), (120, 154, 188) "
	        "#89abcd #9abcde ,#abcdef #bcdeff "
	        "(205, 239, 255)  ,  (222, 255, 255) (239, 255, 255) (255,255,255) ";

	const auto maybe_result = parse_cga_colors(cga_colors_prefs);
	EXPECT_TRUE(maybe_result.has_value());

	const auto result = maybe_result.value();

	EXPECT_EQ(result[0], (Rgb666(0x00, 0x04, 0x08)));
	EXPECT_EQ(result[1], (Rgb666(0x04, 0x08, 0x0c)));
	EXPECT_EQ(result[2], (Rgb666(0x08, 0x0c, 0x11)));
	EXPECT_EQ(result[3], (Rgb666(0x0c, 0x11, 0x15)));
	EXPECT_EQ(result[4], (Rgb666(0x11, 0x19, 0x22)));
	EXPECT_EQ(result[5], (Rgb666(0x15, 0x1e, 0x26)));
	EXPECT_EQ(result[6], (Rgb666(0x19, 0x22, 0x2a)));
	EXPECT_EQ(result[7], (Rgb666(0x1e, 0x26, 0x2f)));
	EXPECT_EQ(result[8], (Rgb666(0x22, 0x2a, 0x33)));
	EXPECT_EQ(result[9], (Rgb666(0x26, 0x2f, 0x37)));
	EXPECT_EQ(result[10], (Rgb666(0x2a, 0x33, 0x3b)));
	EXPECT_EQ(result[11], (Rgb666(0x2f, 0x37, 0x3f)));
	EXPECT_EQ(result[12], (Rgb666(0x33, 0x3b, 0x3f)));
	EXPECT_EQ(result[13], (Rgb666(0x37, 0x3f, 0x3f)));
	EXPECT_EQ(result[14], (Rgb666(0x3b, 0x3f, 0x3f)));
	EXPECT_EQ(result[15], (Rgb666(0x3f, 0x3f, 0x3f)));
}

TEST(parse_cga_colors, invalid_too_few_colors)
{
	const auto cga_colors_prefs = "#012 #123 #234 #345 #456 #567 #678 #789 "
	                              "#89a #9ab #abc #bcd #cde #def #eff";

	const auto maybe_result = parse_cga_colors(cga_colors_prefs);
	EXPECT_FALSE(maybe_result.has_value());
}

TEST(parse_cga_colors, invalid_too_many_colors)
{
	const auto cga_colors_prefs = "#012 #123 #234 #345 #456 #567 #678 #789 "
	                              "#89a #9ab #abc #bcd #cde #def #eff #fff #abc";

	const auto maybe_result = parse_cga_colors(cga_colors_prefs);
	EXPECT_FALSE(maybe_result.has_value());
}

TEST(parse_cga_colors, invalid_empty_string)
{
	const auto maybe_result = parse_cga_colors("  ");
	EXPECT_FALSE(maybe_result.has_value());
}

} // namespace
