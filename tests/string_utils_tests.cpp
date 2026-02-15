// SPDX-FileCopyrightText:  2020-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "utils/string_utils.h"

#include <gtest/gtest.h>

#include <string>

#include "misc/support.h"

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

TEST(SafeStrlen, FixedSize)
{
	constexpr size_t N = 5;
	char buffer[N] = "1234";
	EXPECT_EQ(N - 1, safe_strlen(buffer));
}

TEST(Split_delim, NoBoundingDelims)
{
	const std::vector<std::string> expected({"a", "/b", "/c/d", "/e/f/"});
	EXPECT_EQ(split_with_empties("a:/b:/c/d:/e/f/", ':'), expected);
	EXPECT_EQ(split_with_empties("a /b /c/d /e/f/", ' '), expected);
	EXPECT_EQ(split_with_empties("abc", 'x'), std::vector<std::string>{"abc"});
}

TEST(Split_delim, DelimAtStartNotEnd)
{
	const std::vector<std::string> expected({"", "a", "/b", "/c/d", "/e/f/"});
	EXPECT_EQ(split_with_empties(":a:/b:/c/d:/e/f/", ':'), expected);
	EXPECT_EQ(split_with_empties(" a /b /c/d /e/f/", ' '), expected);
}

TEST(Split_delim, DelimAtEndNotStart)
{
	const std::vector<std::string> expected({"a", "/b", "/c/d", "/e/f/", ""});
	EXPECT_EQ(split_with_empties("a:/b:/c/d:/e/f/:", ':'), expected);
	EXPECT_EQ(split_with_empties("a /b /c/d /e/f/ ", ' '), expected);
}

TEST(Split_delim, DelimsAtBoth)
{
	const std::vector<std::string> expected({"", "a", "/b", "/c/d", "/e/f/", ""});
	EXPECT_EQ(split_with_empties(":a:/b:/c/d:/e/f/:", ':'), expected);
	EXPECT_EQ(split_with_empties(" a /b /c/d /e/f/ ", ' '), expected);
}

TEST(Split_delim, MultiInternalDelims)
{
	const std::vector<std::string> expected(
	        {"a", "/b", "", "/c/d", "", "", "/e/f/"});
	EXPECT_EQ(split_with_empties("a:/b::/c/d:::/e/f/", ':'), expected);
	EXPECT_EQ(split_with_empties("a /b  /c/d   /e/f/", ' '), expected);
}

TEST(Split_delim, MultiBoundingDelims)
{
	const std::vector<std::string> expected(
	        {"", "", "a", "/b", "/c/d", "/e/f/", "", "", ""});
	EXPECT_EQ(split_with_empties("::a:/b:/c/d:/e/f/:::", ':'), expected);
	EXPECT_EQ(split_with_empties("  a /b /c/d /e/f/   ", ' '), expected);
}

TEST(Split_delim, MixedDelims)
{
	const std::vector<std::string> expected(
	        {"", "", "a", "/b", "", "/c/d", "/e/f/"});
	EXPECT_EQ(split_with_empties("::a:/b::/c/d:/e/f/", ':'), expected);
	EXPECT_EQ(split_with_empties("  a /b  /c/d /e/f/", ' '), expected);
}

TEST(Split_delim, Empty)
{
	const std::vector<std::string> empty;
	const std::vector<std::string> two({"", ""});
	const std::vector<std::string> three({"", "", ""});

	EXPECT_EQ(split_with_empties("", ':'), empty);
	EXPECT_EQ(split_with_empties(":", ':'), two);
	EXPECT_EQ(split_with_empties("::", ':'), three);
	EXPECT_EQ(split_with_empties("", ' '), empty);
	EXPECT_EQ(split_with_empties(" ", ' '), two);
	EXPECT_EQ(split_with_empties("  ", ' '), three);
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

TEST(ParsePercentageWithOptionalPercentSign, Valid)
{
	EXPECT_EQ(*parse_percentage_with_optional_percent_sign("1%"), 1.0f);
	EXPECT_EQ(*parse_percentage_with_optional_percent_sign("1"), 1.0f);

	EXPECT_EQ(*parse_percentage_with_optional_percent_sign("100%"), 100.0f);
	EXPECT_EQ(*parse_percentage_with_optional_percent_sign("100"), 100.0f);

	EXPECT_EQ(*parse_percentage_with_optional_percent_sign("150%"), 150.0f);
	EXPECT_EQ(*parse_percentage_with_optional_percent_sign("150"), 150.0f);

	EXPECT_EQ(*parse_percentage_with_optional_percent_sign("-5%"), -5.0f);
	EXPECT_EQ(*parse_percentage_with_optional_percent_sign("-5"), -5.0f);

	EXPECT_EQ(*parse_percentage_with_optional_percent_sign("0%"), 0.0f);
	EXPECT_EQ(*parse_percentage_with_optional_percent_sign("0"), 0.0f);

	EXPECT_EQ(*parse_percentage_with_optional_percent_sign("-110.5%"), -110.5f);
	EXPECT_EQ(*parse_percentage_with_optional_percent_sign("-110.5"), -110.5f);
}

TEST(ParsePercentageWithPercentSign, Valid)
{
	EXPECT_EQ(*parse_percentage_with_percent_sign("1%"), 1.0f);
	EXPECT_EQ(*parse_percentage_with_percent_sign("100%"), 100.0f);
	EXPECT_EQ(*parse_percentage_with_percent_sign("150%"), 150.0f);
	EXPECT_EQ(*parse_percentage_with_percent_sign("-5%"), -5.0f);
	EXPECT_EQ(*parse_percentage_with_percent_sign("0%"), 0.0f);
	EXPECT_EQ(*parse_percentage_with_percent_sign("-110.5%"), -110.5f);
}

TEST(ParsePercentageWithPercentSign, Invalid)
{
	std::optional<float> empty = {};

	EXPECT_EQ(parse_percentage_with_percent_sign("100"), empty);
	EXPECT_EQ(parse_percentage_with_percent_sign("0"), empty);
	EXPECT_EQ(parse_percentage_with_percent_sign("-1"), empty);
	EXPECT_EQ(parse_percentage_with_percent_sign("100a"), empty);
	EXPECT_EQ(parse_percentage_with_percent_sign("sfafsd"), empty);
	EXPECT_EQ(parse_percentage_with_percent_sign(""), empty);
	EXPECT_EQ(parse_percentage_with_percent_sign(" "), empty);
}

TEST(ParsePercentageWithOptionalPercentSign, Invalid)
{
	std::optional<float> empty = {};

	EXPECT_EQ(parse_percentage_with_optional_percent_sign("100a"), empty);
	EXPECT_EQ(parse_percentage_with_optional_percent_sign("sfafsd"), empty);
	EXPECT_EQ(parse_percentage_with_optional_percent_sign(""), empty);
	EXPECT_EQ(parse_percentage_with_optional_percent_sign(" "), empty);
}

TEST(FormatString, Valid)
{
	EXPECT_EQ(format_str(""), "");
 	EXPECT_EQ(format_str("abcd"), "abcd");
	EXPECT_EQ(format_str("%d", 42), "42");
	EXPECT_EQ(format_str("%d\0", 42), "42\0");
	EXPECT_EQ(format_str("%s%d%s", "abcd", 42, "xyz"), "abcd42xyz");
}

TEST(IsHexDigits, Valid)
{
	EXPECT_TRUE(is_hex_digits("0123456789ABCDEF"));
	EXPECT_TRUE(is_hex_digits("0123456789abcdef"));
	EXPECT_TRUE(is_hex_digits(""));
}

TEST(IsHexDigits, Invalid)
{
	EXPECT_FALSE(is_hex_digits("0123456789ABCDEFG"));
	EXPECT_FALSE(is_hex_digits("0123456789abcdefg"));
}

TEST(IsDigits, Valid)
{
	EXPECT_TRUE(is_digits("0123456789"));
	EXPECT_TRUE(is_digits(""));
}

TEST(IsDigits, Invalid)
{
	EXPECT_FALSE(is_digits("01234567890ABCDEFG"));
	EXPECT_FALSE(is_digits("01234567890abcdefg"));
}

TEST(LTrim, Valid) 
{
	auto perform_ltrim = [](const std::string& input) {
		std::string output = input;
		ltrim(output);
		return output;
	};
	EXPECT_EQ(perform_ltrim("  ABC"), "ABC");
	EXPECT_EQ(perform_ltrim("ABC"), "ABC");
	EXPECT_EQ(perform_ltrim("ABC   "), "ABC   ");
}

TEST(Upcase, Valid)
{
	auto perform_upcase = [](const std::string& input,
	                         const std::string& expected) {
		std::string test_str = input;
		upcase(test_str);
		EXPECT_EQ(test_str, expected);

		std::string test_cstr = input;
		char* result          = upcase(test_cstr.data());
		EXPECT_STREQ(result, expected.c_str());
		EXPECT_EQ(test_cstr, expected);

		EXPECT_EQ(upcase(std::string_view(input)), expected);
	};
	perform_upcase("abc", "ABC");
	perform_upcase("ABC", "ABC");
	perform_upcase("aBc", "ABC");
}

TEST(Lowcase, Valid)
{
	auto perform_lowcase = [](const std::string& input,
	                          const std::string& expected) {
		std::string test_str = input;
		lowcase(test_str);
		EXPECT_EQ(test_str, expected);

		std::string test_cstr = input;
		char* result          = lowcase(test_cstr.data());
		EXPECT_STREQ(result, expected.c_str());
		EXPECT_EQ(test_cstr, expected);

		EXPECT_EQ(lowcase(std::string_view(input)), expected);
	};
	perform_lowcase("abc", "abc");
	perform_lowcase("ABC", "abc");
	perform_lowcase("aBc", "abc");
}

TEST(Replace, Valid)
{
	EXPECT_EQ(replace("abc", 'c', 'D'), "abD");
	EXPECT_EQ(replace("abc", 'd', 'D'), "abc");
	EXPECT_EQ(replace("", 'd', 'D'), "");
}

TEST(ReplaceAll, Valid)
{
	const auto s1 = "%% foo%%bar quz%baz%";
	EXPECT_EQ(replace_all(s1, "%%", "%"), "% foo%bar quz%baz%");

	const auto s2 = "\nthe quick brown fox jumps\nover the\nlazy dog";
	EXPECT_EQ(replace_all(s2, "the", "a"), "\na quick brown fox jumps\nover a\nlazy dog");
}

TEST(ReplaceEol, Valid)
{
	const auto s1 = "\n foo \n\r bar \r\n baz \r";

	EXPECT_EQ(replace_eol(s1, "\n"), "\n foo \n bar \n baz \n");
	EXPECT_EQ(replace_eol(s1, "\n\r"), "\n\r foo \n\r bar \n\r baz \n\r");
	EXPECT_EQ(replace_eol(s1, "\r\n"), "\r\n foo \r\n bar \r\n baz \r\n");

	const auto s2 = "Foo\n\nBar\r\r";

	EXPECT_EQ(replace_eol(s2, "\n"), "Foo\n\nBar\n\n");
	EXPECT_EQ(replace_eol(s2, "\n\r"), "Foo\n\r\n\rBar\n\r\n\r");
	EXPECT_EQ(replace_eol(s2, "\r\n"), "Foo\r\n\r\nBar\r\n\r\n");
}

TEST(IsTextEqual, Valid)
{
	// Base text, different eol marks
	const auto s1_posix =
	        "Lorem ipsum dolor sit amet,\n"
	        "consectetur adipiscing elit,\n"
	        "sed do eiusmod tempor incididunt\n"
	        "ut labore et dolore magna aliqua.\n";
	const auto s1_win32 =
	        "Lorem ipsum dolor sit amet,\r\n"
	        "consectetur adipiscing elit,\r\n"
	        "sed do eiusmod tempor incididunt\r\n"
	        "ut labore et dolore magna aliqua.\r\n";
	const auto s1_mixed =
	        "Lorem ipsum dolor sit amet,\r\n"
	        "consectetur adipiscing elit,\n"
	        "sed do eiusmod tempor incididunt\n\r"
	        "ut labore et dolore magna aliqua.\r";

	// Similar to s1_*, but each line starts with uppercase
	const auto s2_posix =
	        "Lorem ipsum dolor sit amet,\n"
	        "Consectetur adipiscing elit,\n"
	        "Sed do eiusmod tempor incididunt\n"
	        "Ut labore et dolore magna aliqua.\n";
	const auto s2_win32 =
	        "Lorem ipsum dolor sit amet,\r\n"
	        "Consectetur adipiscing elit,\r\n"
	        "Sed do eiusmod tempor incididunt\r\n"
	        "Ut labore et dolore magna aliqua.\r\n";

	// Similar to s1_*, but each end-of-line is doubled
	const auto s3_posix =
	        "Lorem ipsum dolor sit amet,\n\n"
	        "consectetur adipiscing elit,\n\n"
	        "sed do eiusmod tempor incididunt\n\n"
	        "ut labore et dolore magna aliqua.\n\n";

	const auto s3_win32 =
	        "Lorem ipsum dolor sit amet,\r\n\r\n"
	        "consectetur adipiscing elit,\r\n\r\n"
	        "sed do eiusmod tempor incididunt\r\n\r\n"
	        "ut labore et dolore magna aliqua.\r\n\r\n";

	// Compare with same text
	EXPECT_TRUE(is_text_equal(s1_posix, s1_posix));
	EXPECT_TRUE(is_text_equal(s1_win32, s1_win32));
	EXPECT_TRUE(is_text_equal(s1_mixed, s1_mixed));
	EXPECT_TRUE(is_text_equal(s2_posix, s2_posix));
	EXPECT_TRUE(is_text_equal(s2_win32, s2_win32));
	EXPECT_TRUE(is_text_equal(s3_posix, s3_posix));
	EXPECT_TRUE(is_text_equal(s3_win32, s3_win32));

	// Compare s1_* with s1_*
	EXPECT_TRUE(is_text_equal(s1_posix, s1_win32));
	EXPECT_TRUE(is_text_equal(s1_win32, s1_posix));
	EXPECT_TRUE(is_text_equal(s1_win32, s1_mixed));
	EXPECT_TRUE(is_text_equal(s1_mixed, s1_win32));
	EXPECT_TRUE(is_text_equal(s1_mixed, s1_posix));
	EXPECT_TRUE(is_text_equal(s1_posix, s1_mixed));

	// Compare s2_* with s2_*
	EXPECT_TRUE(is_text_equal(s2_posix, s2_win32));
	EXPECT_TRUE(is_text_equal(s2_win32, s2_posix));

	// Compare s3_* with s3_*
	EXPECT_TRUE(is_text_equal(s3_posix, s3_win32));
	EXPECT_TRUE(is_text_equal(s3_win32, s3_posix));

	// Compare s1_posix with s2_*
	EXPECT_FALSE(is_text_equal(s1_posix, s2_posix));
	EXPECT_FALSE(is_text_equal(s1_posix, s2_win32));
	EXPECT_FALSE(is_text_equal(s1_posix, s3_posix));
	EXPECT_FALSE(is_text_equal(s1_posix, s3_win32));

	// Compare s2_* with s1_posix
	EXPECT_FALSE(is_text_equal(s2_posix, s1_posix));
	EXPECT_FALSE(is_text_equal(s2_win32, s1_posix));
	EXPECT_FALSE(is_text_equal(s3_posix, s1_posix));
	EXPECT_FALSE(is_text_equal(s3_win32, s1_posix));

	// Compare s1_win32 with s2_*
	EXPECT_FALSE(is_text_equal(s1_win32, s2_posix));
	EXPECT_FALSE(is_text_equal(s1_win32, s2_win32));
	EXPECT_FALSE(is_text_equal(s1_win32, s3_posix));
	EXPECT_FALSE(is_text_equal(s1_win32, s3_win32));

	// Compare s2_* with s1_win32
	EXPECT_FALSE(is_text_equal(s2_posix, s1_win32));
	EXPECT_FALSE(is_text_equal(s2_win32, s1_win32));
	EXPECT_FALSE(is_text_equal(s3_posix, s1_win32));
	EXPECT_FALSE(is_text_equal(s3_win32, s1_win32));

	// Compare s1_mixed with s2_*
	EXPECT_FALSE(is_text_equal(s1_mixed, s2_posix));
	EXPECT_FALSE(is_text_equal(s1_mixed, s2_win32));
	EXPECT_FALSE(is_text_equal(s1_mixed, s3_posix));
	EXPECT_FALSE(is_text_equal(s1_mixed, s3_win32));

	// Compare s2_* with s1_mixed
	EXPECT_FALSE(is_text_equal(s2_posix, s1_mixed));
	EXPECT_FALSE(is_text_equal(s2_win32, s1_mixed));
	EXPECT_FALSE(is_text_equal(s3_posix, s1_mixed));
	EXPECT_FALSE(is_text_equal(s3_win32, s1_mixed));

	// Compare strings without newline characters
	EXPECT_TRUE(is_text_equal("FooBar", "FooBar"));
	EXPECT_FALSE(is_text_equal("FooBar", "BarFoo"));
	EXPECT_FALSE(is_text_equal("FooBar", "FooBarBaz"));
	EXPECT_FALSE(is_text_equal("FooBarBaz", "FooBar"));
}

TEST(WritePaddedStringTest, PadsWithSpaces)
{
	constexpr int length               = 8;
	std::string input                  = "abc";
	auto result                        = right_pad(input, length, ' ');
	std::string result_str(reinterpret_cast<char*>(result.data()), length);
	EXPECT_EQ(result_str, "abc     ");
}

TEST(WritePaddedStringTest, PadsWithCustomChar)
{
	constexpr int length               = 6;
	std::string input                  = "hi";
	auto result                        = right_pad(input, length, '-');
	std::string result_str(reinterpret_cast<char*>(result.data()), length);
	EXPECT_EQ(result_str, "hi----");
}

TEST(WritePaddedStringTest, TruncatesIfLonger)
{
	constexpr int length               = 4;
	std::string input                  = "toolong";
	auto result                        = right_pad(input, length, ' ');
	std::string result_str(reinterpret_cast<char*>(result.data()), length);
	EXPECT_EQ(result_str, "tool");
}

TEST(WritePaddedStringTest, ExactLengthNoPad)
{
	constexpr int length               = 5;
	std::string input                  = "hello";
	auto result                        = right_pad(input, length, 'x');
	std::string result_str(reinterpret_cast<char*>(result.data()), length);
	EXPECT_EQ(result_str, "hello");
}

TEST(WritePaddedStringTest, EmptyStringAllPad)
{
	constexpr int length               = 3;
	std::string input                  = "";
	auto result                        = right_pad(input, length, '*');
	std::string result_str(reinterpret_cast<char*>(result.data()), length);
	EXPECT_EQ(result_str, "***");
}

} // namespace
