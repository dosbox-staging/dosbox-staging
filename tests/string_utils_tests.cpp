/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2023  The DOSBox Staging Team
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

#include "string_utils.h"

#include <gtest/gtest.h>

#include <string>

#include "support.h"

namespace {

TEST(CaseInsensitiveCompare, Chars)
{
	constexpr const char a[] = "123";
	constexpr const char not_a[] = "321";

	EXPECT_TRUE(iequals(a, a));
	EXPECT_FALSE(iequals(a, not_a));
}

TEST(CaseInsensitiveCompare, StringViews)
{
	constexpr std::string_view a = "123";
	constexpr std::string_view not_a = "321";

	EXPECT_TRUE(iequals(a, a));
	EXPECT_FALSE(iequals(a, not_a));
}

TEST(CaseInsensitiveCompare, Strings)
{
	const std::string a = "123";
	const std::string not_a = "321";

	EXPECT_TRUE(iequals(a, a));
	EXPECT_FALSE(iequals(a, not_a));
}


TEST(CaseInsensitiveCompare, MixedTypes)
{
	constexpr const char a_sz[] = "123";

	constexpr std::string_view a_sv = "123";
	constexpr std::string_view not_a_sv = "321";

	const std::string a_string = "123";
	const std::string not_a_string = "321";

	// char and string_view
	EXPECT_TRUE(iequals(a_sz, a_sv));
	EXPECT_FALSE(iequals(a_sz, not_a_sv));

	// char and string
	EXPECT_TRUE(iequals(a_sz, a_string));
	EXPECT_FALSE(iequals(a_sz, not_a_string));

	// string_view and string
	EXPECT_TRUE(iequals(a_sv, a_string));
	EXPECT_FALSE(iequals(a_sv, not_a_string));
}


TEST(NaturalCompare, AtStartChar)
{
	EXPECT_FALSE(natural_compare("", ""));

	EXPECT_TRUE(natural_compare(" ", "  "));
	EXPECT_TRUE(natural_compare("a", "Aa"));
	EXPECT_TRUE(natural_compare("aA", "Ba"));
	EXPECT_TRUE(natural_compare("Aa", "ba"));
}
TEST(NaturalCompare, AtStartNum)
{

	EXPECT_TRUE(natural_compare("1", "1a"));
	EXPECT_TRUE(natural_compare("1", "2a"));
	EXPECT_TRUE(natural_compare("999", "1000a"));
}

TEST(NaturalCompare, InMiddleChar)
{
	EXPECT_TRUE(natural_compare("aac", "ABC"));
	EXPECT_TRUE(natural_compare("aAc", "aBc"));
	EXPECT_TRUE(natural_compare("AAC", "abc"));
}
TEST(NaturalCompare, InMiddleNum)
{

	EXPECT_TRUE(natural_compare("a1a", "a1aa"));
	EXPECT_TRUE(natural_compare("A1A", "a2a"));
	EXPECT_TRUE(natural_compare("A999b", "a1000a"));
}
TEST(NaturalCompare, AtEndChar)
{
	EXPECT_TRUE(natural_compare("abc", "ABCd"));
	EXPECT_TRUE(natural_compare("abcD", "abcE"));
	EXPECT_TRUE(natural_compare("ABCD", "abce"));

}
TEST(NaturalCompare, AtEndNum)
{

	EXPECT_TRUE(natural_compare("a1", "a1 "));
	EXPECT_TRUE(natural_compare("A10", "b2"));
	EXPECT_TRUE(natural_compare("A10", "a20"));
	EXPECT_TRUE(natural_compare("Ab999", "aB1000"));
}


TEST(StartsWith, Prefix)
{
	EXPECT_TRUE(starts_with("abcd", "ab"));
	EXPECT_TRUE(starts_with(std::string{"abcd"}, "ab"));
}

TEST(StartsWith, NotPrefix)
{
	EXPECT_FALSE(starts_with("abcd", "xy"));
	EXPECT_FALSE(starts_with(std::string{"abcd"}, "xy"));
}

TEST(StartsWith, TooLongPrefix)
{
	EXPECT_FALSE(starts_with("ab", "abcd"));
	EXPECT_FALSE(starts_with(std::string{"ab"}, "abcd"));
}

TEST(StartsWith, EmptyPrefix)
{
	EXPECT_TRUE(starts_with("abcd", ""));
	EXPECT_TRUE(starts_with(std::string{"abcd"}, ""));
}

TEST(StartsWith, EmptyString)
{
	EXPECT_FALSE(starts_with("", "ab"));
	EXPECT_FALSE(starts_with(std::string{""}, "ab"));
}

TEST(SafeSprintF, PreventOverflow)
{
	char buf[3];
	const int full_msg_len = safe_sprintf(buf, "%d", 98765);
	EXPECT_EQ(buf[0], '9');
	EXPECT_EQ(buf[1], '8');
	EXPECT_EQ(buf[2], '\0');
	EXPECT_EQ(full_msg_len, 5);
}

TEST(SafeSprintF, PreventUnderflow)
{
	char buf[10];
	const int full_msg_len = safe_sprintf(buf, "%d", 987);
	EXPECT_STREQ(buf, "987");
	EXPECT_EQ(full_msg_len, 3);
}

TEST(SafeStrcpy, SimpleCopy)
{
	char buffer[10] = "";
	char *ret_value = safe_strcpy(buffer, "abc");
	EXPECT_EQ(ret_value, &buffer[0]);
	EXPECT_STREQ("abc", buffer);
}

TEST(SafeStrcpy, CopyFromNonArray)
{
	char buffer[10] = "";
	std::string str = "abc";
	EXPECT_STREQ("abc", safe_strcpy(buffer, str.c_str()));
}

TEST(SafeStrcpy, EmptyStringOverwrites)
{
	char buffer[4] = "abc";
	EXPECT_STREQ("", safe_strcpy(buffer, ""));
}

TEST(SafeStrcpy, StringLongerThanBuffer)
{
	char buffer[5] = "";
	char long_input[] = "1234567890";
	ASSERT_LT(ARRAY_LEN(buffer), strlen(long_input));
	EXPECT_STREQ("1234", safe_strcpy(buffer, long_input));
}

TEST(SafeStrcpyDeathTest, PassNull)
{
	char buf[] = "12345678";
	EXPECT_DEBUG_DEATH({ safe_strcpy(buf, nullptr); }, "");
}

TEST(SafeStrcpyDeathTest, ProtectFromCopyingOverlappingString)
{
	char buf[] = "12345678";
	char *overlapping = &buf[2];
	ASSERT_LE(buf, overlapping);
	ASSERT_LE(overlapping, buf + ARRAY_LEN(buf));
	EXPECT_DEBUG_DEATH({ safe_strcpy(buf, overlapping); }, "");
}

TEST(SafeStrlen, Simple)
{
	char buffer[] = "1234";
	EXPECT_EQ(4, safe_strlen(buffer));
}

TEST(SafeStrlen, EmptyString)
{
	char buffer[] = "";
	EXPECT_EQ(0, safe_strlen(buffer));
}

TEST(SafeStrlen, Uninitialized)
{
	char buffer[4];
	EXPECT_GT(sizeof(buffer), safe_strlen(buffer));
}

TEST(SafeStrlen, FixedSize)
{
	constexpr size_t N = 5;
	char buffer[N] = "1234";
	EXPECT_EQ(N - 1, safe_strlen(buffer));
}

TEST(Split_delit, NoBoundingDelims)
{
	const std::vector<std::string> expected({"a", "/b", "/c/d", "/e/f/"});
	EXPECT_EQ(split("a:/b:/c/d:/e/f/", ':'), expected);
	EXPECT_EQ(split("a /b /c/d /e/f/", ' '), expected);
	EXPECT_EQ(split("abc", 'x'), std::vector<std::string>{"abc"});
}

TEST(Split_delim, DelimAtStartNotEnd)
{
	const std::vector<std::string> expected({"", "a", "/b", "/c/d", "/e/f/"});
	EXPECT_EQ(split(":a:/b:/c/d:/e/f/", ':'), expected);
	EXPECT_EQ(split(" a /b /c/d /e/f/", ' '), expected);
}

TEST(Split_delim, DelimAtEndNotStart)
{
	const std::vector<std::string> expected({"a", "/b", "/c/d", "/e/f/", ""});
	EXPECT_EQ(split("a:/b:/c/d:/e/f/:", ':'), expected);
	EXPECT_EQ(split("a /b /c/d /e/f/ ", ' '), expected);
}

TEST(Split_delim, DelimsAtBoth)
{
	const std::vector<std::string> expected({"", "a", "/b", "/c/d", "/e/f/", ""});
	EXPECT_EQ(split(":a:/b:/c/d:/e/f/:", ':'), expected);
	EXPECT_EQ(split(" a /b /c/d /e/f/ ", ' '), expected);
}

TEST(Split_delim, MultiInternalDelims)
{
	const std::vector<std::string> expected(
	        {"a", "/b", "", "/c/d", "", "", "/e/f/"});
	EXPECT_EQ(split("a:/b::/c/d:::/e/f/", ':'), expected);
	EXPECT_EQ(split("a /b  /c/d   /e/f/", ' '), expected);
}

TEST(Split_delim, MultiBoundingDelims)
{
	const std::vector<std::string> expected(
	        {"", "", "a", "/b", "/c/d", "/e/f/", "", "", ""});
	EXPECT_EQ(split("::a:/b:/c/d:/e/f/:::", ':'), expected);
	EXPECT_EQ(split("  a /b /c/d /e/f/   ", ' '), expected);
}

TEST(Split_delim, MixedDelims)
{
	const std::vector<std::string> expected(
	        {"", "", "a", "/b", "", "/c/d", "/e/f/"});
	EXPECT_EQ(split("::a:/b::/c/d:/e/f/", ':'), expected);
	EXPECT_EQ(split("  a /b  /c/d /e/f/", ' '), expected);
}

TEST(Split_delim, Empty)
{
	const std::vector<std::string> empty;
	const std::vector<std::string> two({"", ""});
	const std::vector<std::string> three({"", "", ""});

	EXPECT_EQ(split("", ':'), empty);
	EXPECT_EQ(split(":", ':'), two);
	EXPECT_EQ(split("::", ':'), three);
	EXPECT_EQ(split("", ' '), empty);
	EXPECT_EQ(split(" ", ' '), two);
	EXPECT_EQ(split("  ", ' '), three);
}
 
TEST(Split, NoBoundingWhitespace)
{
	const std::vector<std::string> expected({"a", "/b", "/c/d", "/e/f/"});
	EXPECT_EQ(split("a /b /c/d /e/f/"), expected);
	EXPECT_EQ(split("abc"), std::vector<std::string>{"abc"});
}
TEST(Split, WhitespaceAtStartNotEnd)
{
	const std::vector<std::string> expected({"a", "/b", "/c/d", "/e/f/"});
	EXPECT_EQ(split(" a /b /c/d /e/f/"), expected);
}

TEST(Split, WhitespaceAtEndNotStart)
{
	const std::vector<std::string> expected({"a", "/b", "/c/d", "/e/f/"});
	EXPECT_EQ(split("a /b /c/d /e/f/ "), expected);
}

TEST(Split, WhitespaceAtBoth)
{
	const std::vector<std::string> expected({"a", "/b", "/c/d", "/e/f/"});
	EXPECT_EQ(split(" a /b /c/d /e/f/ "), expected);
}

TEST(Split, MultiInternalWhitespace)
{
	const std::vector<std::string> expected({"a", "/b", "/c/d", "/e/f/"});
	EXPECT_EQ(split("a /b  /c/d   /e/f/"), expected);
}

TEST(Split, MultiBoundingWhitespace)
{
	const std::vector<std::string> expected({"a", "/b", "/c/d", "/e/f/"});
	EXPECT_EQ(split("  a /b /c/d /e/f/   "), expected);
}

TEST(Split, MixedWhitespace)
{
	const std::vector<std::string> expected({"a", "b", "c"});
	EXPECT_EQ(split("\t\na\f\vb\rc"), expected);
	EXPECT_EQ(split("a\tb\f\vc"), expected);
	EXPECT_EQ(split(" a \n \v \r b \f \r c "), expected);
}

TEST(Split, Empty)
{
	const std::vector<std::string> empty;
	EXPECT_EQ(split(""), empty);
	EXPECT_EQ(split(" "), empty);
	EXPECT_EQ(split("   "), empty);
}

TEST(ParseFloat, Valid)
{
	// negatives
	EXPECT_EQ(*parse_float("-10000"), -10000.0f);
	EXPECT_EQ(*parse_float("-0.1"), -0.1f);
	EXPECT_EQ(*parse_float("-0.0001"), -0.0001f);
	EXPECT_EQ(*parse_float("-0.0"), 0.0f);
	EXPECT_EQ(*parse_float("-0"), 0.0f);

	// positives
	EXPECT_EQ(*parse_float("10000"), 10000.0f);
	EXPECT_EQ(*parse_float("0.1"), 0.1f);
	EXPECT_EQ(*parse_float("0.0001"), 0.0001f);
	EXPECT_EQ(*parse_float("0.0"), 0.0f);
	EXPECT_EQ(*parse_float("0"), 0.0f);
}

TEST(ParseFloat, Invalid)
{
	std::optional<float> empty = {};
	EXPECT_EQ(parse_float("100a"), empty);
	EXPECT_EQ(parse_float("sfafsd"), empty);
	EXPECT_EQ(parse_float(""), empty);
	EXPECT_EQ(parse_float(" "), empty);
}

TEST(ParseInt, Valid)
{
	// negatives
	EXPECT_EQ(*parse_float("-10000"), -10000);
	EXPECT_EQ(*parse_float("-0"), 0);
	EXPECT_EQ(*parse_float("-1"), -1);

	// positives
	EXPECT_EQ(*parse_int("10000"), 10000);
	EXPECT_EQ(*parse_int("0"), 0);
	EXPECT_EQ(*parse_int("1"), 1);
}

TEST(ParseInt, Invalid)
{
	std::optional<int> empty = {};
	EXPECT_EQ(parse_int("100a"), empty);
	EXPECT_EQ(parse_int("sfafsd"), empty);
	EXPECT_EQ(parse_int(""), empty);
	EXPECT_EQ(parse_int(" "), empty);
}

TEST(ParsePercentage, Valid)
{
	EXPECT_EQ(parse_percentage("0"), 0.0f);
	EXPECT_EQ(parse_percentage("0.0"), 0.0f);
	EXPECT_EQ(parse_percentage("-0.0"), 0.0f);
	EXPECT_EQ(parse_percentage("1"), 1.0f);
	EXPECT_EQ(parse_percentage("5.0"), 5.0f);
	EXPECT_EQ(parse_percentage("0.00001"), 0.00001f);
	EXPECT_EQ(parse_percentage("99.9999"), 99.9999f);
	EXPECT_EQ(parse_percentage("100"), 100.0f);
}

TEST(ParsePercentage, Invalid)
{
	std::optional<int> empty = {};
	EXPECT_EQ(parse_percentage("-1"), empty);
	EXPECT_EQ(parse_percentage("-5.0"), empty);
	EXPECT_EQ(parse_percentage("-0.00001"), empty);
	EXPECT_EQ(parse_percentage("100.0001"), empty);
	EXPECT_EQ(parse_percentage("105"), empty);
	EXPECT_EQ(parse_percentage("asdf"), empty);
	EXPECT_EQ(parse_percentage("5a"), empty);
	EXPECT_EQ(parse_percentage(""), empty);
	EXPECT_EQ(parse_percentage(" "), empty);
}

TEST(FormatString, Valid)
{
	EXPECT_EQ(format_string(""), "");
 	EXPECT_EQ(format_string("abcd"), "abcd");
	EXPECT_EQ(format_string("%d", 42), "42");
	EXPECT_EQ(format_string("%d\0", 42), "42\0");
	EXPECT_EQ(format_string("%s%d%s", "abcd", 42, "xyz"), "abcd42xyz");
}

} // namespace
