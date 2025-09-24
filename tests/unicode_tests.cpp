// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "misc/private/unicode_encodings.h"
#include "misc/unicode.h"

#include <cstdint>
#include <gtest/gtest.h>

#include <string>
#include <sys/types.h>

namespace {

TEST(Unicode, UTF_8_EmptyStrings)
{
	EXPECT_TRUE(utf8_to_wide({}).empty());
	EXPECT_TRUE(wide_to_utf8({}).empty());
}

TEST(Unicode, UTF_8_Valid)
{
	auto test_utf8 = [&](const char32_t code_point, const std::string& utf8) {
		// Encoding test
		EXPECT_STREQ(wide_to_utf8({code_point}).c_str(), utf8.c_str());

		// Decoding test
		const auto decoded = utf8_to_wide(utf8);
		EXPECT_EQ(decoded.size(), 1u);
		EXPECT_EQ(decoded[0], code_point);
	};

	// 1-byte encoding
	test_utf8(0x000000, std::string().insert(0, 1, '\0'));
	test_utf8(0x000030, "\x30");
	test_utf8(0x00007f, "\x7f");

	// 2-byte encoding
	test_utf8(0x000080, "\xc2\x80");
	test_utf8(0x000312, "\xcc\x92");
	test_utf8(0x0007ff, "\xdf\xbf");

	// 3-byte encoding (range #1)
	test_utf8(0x000800, "\xe0\xa0\x80");
	test_utf8(0x003123, "\xe3\x84\xa3");
	test_utf8(0x00d7ff, "\xed\x9f\xbf");

	// 3-byte encoding (range #2)
	test_utf8(0x00e000, "\xee\x80\x80");
	test_utf8(0x00f123, "\xef\x84\xa3");
	test_utf8(0x00ffff, "\xef\xbf\xbf");

	// 4-byte encoding
	test_utf8(0x010000, "\xf0\x90\x80\x80");
	test_utf8(0x031234, "\xf0\xb1\x88\xb4");
	test_utf8(0x10ffff, "\xf4\x8f\xbf\xbf");
}

TEST(Unicode, UTF_8_Invalid)
{
	auto test_invalid_utf8 = [&](const std::string& utf8,
	                             const std::string& expected = "?") {
		const auto decoded = utf8_to_wide(utf8);
		EXPECT_EQ(decoded.size(), expected.size());

		const auto limit = std::min(decoded.size(), expected.size());
		for (size_t i = 0; i < limit; ++i) {
			EXPECT_EQ(decoded[i], expected[i]);
		}
	};

	// 5-byte encoding - unsupported
	test_invalid_utf8("\xf8\x81\x81\x81\x81");

	// 6-byte encoding - unsupported
	test_invalid_utf8("\xfc\x81\x81\x81\x81\x81");

	// Sequences prematurely terminated by the end of string
	test_invalid_utf8("\xcc");
	test_invalid_utf8("\xe3");
	test_invalid_utf8("\xe3\x84");
	test_invalid_utf8("\xf0");
	test_invalid_utf8("\xf0\xb1");
	test_invalid_utf8("\xf0\xb1\x88");

	// Sequences prematurely terminated by a regular character
	test_invalid_utf8("\xcc"         "A", "?A");
	test_invalid_utf8("\xe3"         "B", "?B");
	test_invalid_utf8("\xe3\x84"     "C", "?C");
	test_invalid_utf8("\xf0"         "D", "?D");
	test_invalid_utf8("\xf0\xb1"     "E", "?E");
	test_invalid_utf8("\xf0\xb1\x88" "F", "?F");
}

TEST(Unicode, UTF_16_Empty_Strings)
{
	EXPECT_TRUE(utf16_to_wide({}).empty());
	EXPECT_TRUE(wide_to_utf16({}).empty());
}

TEST(Unicode, UTF_16_Valid)
{
	auto test_utf16_1 = [&](const char32_t code_point, const char16_t utf16) {
		// Encoding test
		const auto encoded = wide_to_utf16({code_point});
		EXPECT_EQ(encoded.size(), 1u);
		EXPECT_EQ(encoded[0], utf16);

		// Decoding test
		const auto decoded = utf16_to_wide({utf16});
		EXPECT_EQ(decoded.size(), 1u);
		EXPECT_EQ(decoded[0], code_point);
	};

	auto test_utf16_2 = [&](const char32_t code_point,
	                        const char16_t utf16_1,
	                        const char16_t utf16_2) {
		// Encoding test
		const auto encoded = wide_to_utf16({code_point});
		EXPECT_EQ(encoded.size(), 2u);
		EXPECT_EQ(encoded[0], utf16_1);
		EXPECT_EQ(encoded[1], utf16_2);

		// Decoding test
		const auto decoded = utf16_to_wide({utf16_1, utf16_2});
		EXPECT_EQ(decoded.size(), 1u);
		EXPECT_EQ(decoded[0], code_point);
	};

	// 1-value encoding (range #1)
	test_utf16_1(0x000000, 0x0000);
	test_utf16_1(0x001234, 0x1234);
	test_utf16_1(0x00d7ff, 0xd7ff);

	// 1-value encoding (range #2)
	test_utf16_1(0x00e000, 0xe000);
	test_utf16_1(0x00e567, 0xe567);
	test_utf16_1(0x00ffff, 0xffff);

	// 2-value encoding
	test_utf16_2(0x010000, 0xd800, 0xdc00);
	test_utf16_2(0x101234, 0xdbc4, 0xde34);
	test_utf16_2(0x10ffff, 0xdbff, 0xdfff);
}

TEST(Unicode, UTF_16_Invalid)
{
	auto test_invalid_utf16 = [&](const std::u16string& utf16,
	                              const std::string& expected = "?") {
		const auto decoded = utf16_to_wide(utf16);
		EXPECT_EQ(decoded.size(), expected.size());

		const auto limit = std::min(decoded.size(), expected.size());
		for (size_t i = 0; i < limit; ++i) {
			EXPECT_EQ(decoded[i], expected[i]);
		}
	};

	// Invalid surrogate pairs
	test_invalid_utf16({0xd800, 'a'},    "?a");
	test_invalid_utf16({0xdf00, 'b'},    "?b");
	test_invalid_utf16({0xd800, 0xd800}, "??");
}

TEST(Unicode, UTF_16_ByteOrderMark)
{
	auto test_utf16_bom = [&](const std::u16string& utf16,
	                          const std::string& expected) {
		// Encoding test
		const auto decoded = utf16_to_wide(utf16);
		;
		EXPECT_EQ(decoded.size(), expected.size());

		const auto limit = std::min(decoded.size(), expected.size());
		for (size_t i = 0; i < limit; ++i) {
			EXPECT_EQ(decoded[i], expected[i]);
		}
	};

	// Byte Order Mark (regular and reversed)
	test_utf16_bom({0xfeff, 'a', 'b'},           "ab");
	test_utf16_bom({0xfffe, 'a' << 8, 'b' << 8}, "ab");
}

TEST(Unicode, UCS_2_Empty_Strings)
{
	EXPECT_TRUE(ucs2_to_wide({}).empty());
	EXPECT_TRUE(wide_to_ucs2({}).empty());
}

TEST(Unicode, UCS_2_Valid)
{
	auto test_ucs2_valid = [&](const char16_t code_point) {
		// Encoding test
		const auto encoded = wide_to_ucs2({code_point});
		EXPECT_EQ(encoded.size(), 1u);
		EXPECT_EQ(encoded[0], code_point);

		// Decoding test
		const auto decoded = ucs2_to_wide({code_point});
		EXPECT_EQ(decoded.size(), 1u);
		EXPECT_EQ(decoded[0], static_cast<char32_t>(code_point));
	};

	// Test valid code points (range #1)
	test_ucs2_valid(0x0000);
	test_ucs2_valid(0x5678);
	test_ucs2_valid(0xd7ff);

	// Test valid code points (range #2)
	test_ucs2_valid(0xe000);
	test_ucs2_valid(0xfabc);
	test_ucs2_valid(0xffff);
}

TEST(Unicode, UCS_2_Invalid)
{
	auto test_ucs2_invalid = [&](const char16_t code_point) {
		// Encoding test
		const auto decoded = ucs2_to_wide({code_point});
		EXPECT_EQ(decoded.size(), 1u);
		EXPECT_EQ(decoded[0], static_cast<char16_t>('?'));
	};

	// Test invalid code points (range #1)
	test_ucs2_invalid(0xd800);
	test_ucs2_invalid(0xd999);
	test_ucs2_invalid(0xdbff);

	// Test invalid code points (range #2)
	test_ucs2_invalid(0xdc00);
	test_ucs2_invalid(0xdddd);
	test_ucs2_invalid(0xdfff);
}

TEST(Unicode, Multilingual_Strings)
{
	const std::string TestString_Generic =
	        "Lorem ipsum dolor sit amet,\n"
	        "consectetur adipiscing elit,\n"
	        "sed do eiusmod tempor incididunt\n"
	        "ut labore et dolore magna aliqua.\n";

	// Kometa, by Jaromír Nohavica
	const std::string TestString_Czech =
	        "Na hvězdném nádraží cinkají vagóny,\n"
	        "pan Kepler rozepsal nebeské zákony,\n"
	        "hledal, až nalezl v hvězdářských triedrech\n"
	        "tajemství, která teď neseme na bedrech.\n";

	// The Iliad, by Homer
	const std::string TestString_Greek =
	        "Μῆνιν ἄειδε θεὰ Πηληϊάδεω Ἀχιλῆος\n"
	        "ὐλομένην, ἣ μυρίʼ Ἀχαιοῖς ἄλγεʼ ἔθηκε,\n"
	        "πολλὰς δʼ ἰφθίμους ψυχὰς Ἄϊδι προΐαψεν\n"
	        "ἡρώων, αὐτοὺς δὲ ἑλώρια τεῦχε κύνεσσιν\n";

	EXPECT_STREQ(wide_to_utf8(utf8_to_wide(TestString_Czech)).c_str(),
	             TestString_Czech.c_str());
	EXPECT_STREQ(utf16_to_utf8(utf8_to_utf16(TestString_Czech)).c_str(),
	             TestString_Czech.c_str());
	EXPECT_STREQ(ucs2_to_utf8(utf8_to_ucs2(TestString_Czech)).c_str(),
	             TestString_Czech.c_str());

	EXPECT_STREQ(wide_to_utf8(utf8_to_wide(TestString_Generic)).c_str(),
	             TestString_Generic.c_str());
	EXPECT_STREQ(utf16_to_utf8(utf8_to_utf16(TestString_Generic)).c_str(),
	             TestString_Generic.c_str());
	EXPECT_STREQ(ucs2_to_utf8(utf8_to_ucs2(TestString_Generic)).c_str(),
	             TestString_Generic.c_str());

	EXPECT_STREQ(wide_to_utf8(utf8_to_wide(TestString_Greek)).c_str(),
	             TestString_Greek.c_str());
	EXPECT_STREQ(utf16_to_utf8(utf8_to_utf16(TestString_Greek)).c_str(),
	             TestString_Greek.c_str());
	EXPECT_STREQ(ucs2_to_utf8(utf8_to_ucs2(TestString_Greek)).c_str(),
	             TestString_Greek.c_str());
}

} // namespace
