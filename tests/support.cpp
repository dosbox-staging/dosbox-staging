/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2021  The DOSBox Staging Team
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
 
TEST(Support_split, NoBoundingWhitespace)
{
	const std::vector<std::string> expected({"a", "/b", "/c/d", "/e/f/"});
	EXPECT_EQ(split("a /b /c/d /e/f/"), expected);
	EXPECT_EQ(split("abc"), std::vector<std::string>{"abc"});
}
TEST(Support_split, WhitespaceAtStartNotEnd)
{
	const std::vector<std::string> expected({"a", "/b", "/c/d", "/e/f/"});
	EXPECT_EQ(split(" a /b /c/d /e/f/"), expected);
}

TEST(Support_split, WhitespaceAtEndNotStart)
{
	const std::vector<std::string> expected({"a", "/b", "/c/d", "/e/f/"});
	EXPECT_EQ(split("a /b /c/d /e/f/ "), expected);
}

TEST(Support_split, WhitespaceAtBoth)
{
	const std::vector<std::string> expected({"a", "/b", "/c/d", "/e/f/"});
	EXPECT_EQ(split(" a /b /c/d /e/f/ "), expected);
}

TEST(Support_split, MultiInternalWhitespace)
{
	const std::vector<std::string> expected({"a", "/b", "/c/d", "/e/f/"});
	EXPECT_EQ(split("a /b  /c/d   /e/f/"), expected);
}

TEST(Support_split, MultiBoundingWhitespace)
{
	const std::vector<std::string> expected({"a", "/b", "/c/d", "/e/f/"});
	EXPECT_EQ(split("  a /b /c/d /e/f/   "), expected);
}

TEST(Support_split, MixedWhitespace)
{
	const std::vector<std::string> expected({"a", "b", "c"});
	EXPECT_EQ(split("\t\na\f\vb\rc"), expected);
	EXPECT_EQ(split("a\tb\f\vc"), expected);
	EXPECT_EQ(split(" a \n \v \r b \f \r c "), expected);
}

TEST(Support_split, Empty)
{
	const std::vector<std::string> empty;
	EXPECT_EQ(split(""), empty);
	EXPECT_EQ(split(" "), empty);
	EXPECT_EQ(split("   "), empty);
}

} // namespace
