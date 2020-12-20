/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2020  The dosbox-staging team
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

#include "support.h"

#include <gtest/gtest.h>

#include <string>

namespace {

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

TEST(DriveIndex, DriveA)
{
	EXPECT_EQ(0, drive_index('a'));
	EXPECT_EQ(0, drive_index('A'));
}

TEST(DriveIndex, DriveZ)
{
	EXPECT_EQ(25, drive_index('z'));
	EXPECT_EQ(25, drive_index('Z'));
}

TEST(Support_split_delim, NoBoundingDelims)
{
	const std::vector<std::string> expected({"a", "/b", "/c/d", "/e/f/"});
	EXPECT_EQ(split("a:/b:/c/d:/e/f/", ':'), expected);
	EXPECT_EQ(split("a /b /c/d /e/f/", ' '), expected);
	EXPECT_EQ(split("abc", 'x'), std::vector<std::string>{"abc"});
}

TEST(Support_split_delim, DelimAtStartNotEnd)
{
	const std::vector<std::string> expected({"", "a", "/b", "/c/d", "/e/f/"});
	EXPECT_EQ(split(":a:/b:/c/d:/e/f/", ':'), expected);
	EXPECT_EQ(split(" a /b /c/d /e/f/", ' '), expected);
}

TEST(Support_split_delim, DelimAtEndNotStart)
{
	const std::vector<std::string> expected({"a", "/b", "/c/d", "/e/f/", ""});
	EXPECT_EQ(split("a:/b:/c/d:/e/f/:", ':'), expected);
	EXPECT_EQ(split("a /b /c/d /e/f/ ", ' '), expected);
}

TEST(Support_split_delim, DelimsAtBoth)
{
	const std::vector<std::string> expected({"", "a", "/b", "/c/d", "/e/f/", ""});
	EXPECT_EQ(split(":a:/b:/c/d:/e/f/:", ':'), expected);
	EXPECT_EQ(split(" a /b /c/d /e/f/ ", ' '), expected);
}

TEST(Support_split_delim, MultiInternalDelims)
{
	const std::vector<std::string> expected(
	        {"a", "/b", "", "/c/d", "", "", "/e/f/"});
	EXPECT_EQ(split("a:/b::/c/d:::/e/f/", ':'), expected);
	EXPECT_EQ(split("a /b  /c/d   /e/f/", ' '), expected);
}

TEST(Support_split_delim, MultiBoundingDelims)
{
	const std::vector<std::string> expected(
	        {"", "", "a", "/b", "/c/d", "/e/f/", "", "", ""});
	EXPECT_EQ(split("::a:/b:/c/d:/e/f/:::", ':'), expected);
	EXPECT_EQ(split("  a /b /c/d /e/f/   ", ' '), expected);
}

TEST(Support_split_delim, MixedDelims)
{
	const std::vector<std::string> expected(
	        {"", "", "a", "/b", "", "/c/d", "/e/f/"});
	EXPECT_EQ(split("::a:/b::/c/d:/e/f/", ':'), expected);
	EXPECT_EQ(split("  a /b  /c/d /e/f/", ' '), expected);
}

TEST(Support_split_delim, Empty)
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
{
	const std::vector<std::string> expected({"a", "/b", "/c/d", "/e/f/"});
	EXPECT_EQ(split("::::a:/b::::/c/d:/e/f/", ':'), expected);
	EXPECT_EQ(split("a /b  /c/d   /e/f/    ", ' '), expected);
}

TEST(Support_split, Empty)
{
	const std::vector<std::string> expected;
	EXPECT_EQ(split("", ':'), expected);
	EXPECT_EQ(split(":", ':'), expected);
	EXPECT_EQ(split("::", ':'), expected);
	EXPECT_EQ(split(" ", ' '), expected);
	EXPECT_EQ(split("  ", ' '), expected);
	EXPECT_EQ(split("   ", ' '), expected);
}

} // namespace
