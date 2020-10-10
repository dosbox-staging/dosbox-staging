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

#include "fs_utils.h"

#include <gtest/gtest.h>

#include <string>

namespace {

TEST(PathExists, DirExists)
{
	EXPECT_TRUE(path_exists("tests"));
}

TEST(PathExists, FileExists)
{
	EXPECT_TRUE(path_exists("tests/fs_utils.cpp"));
}

TEST(PathExists, MissingPath)
{
	EXPECT_FALSE(path_exists("foobar"));
}

TEST(PathExists, ExistingPathAsString)
{
	const std::string path = "tests/fs_utils.cpp";
	EXPECT_TRUE(path_exists(path));
}

TEST(PathExists, MissingPathAsString)
{
	const std::string path = "barbaz";
	EXPECT_FALSE(path_exists("foobar"));
}

TEST(PathConversion, SimpleTest)
{
	constexpr auto expected_result = "tests/files/paths/empty.txt";
	constexpr auto input = "tests\\files\\PATHS\\EMPTY.TXT";
	ASSERT_TRUE(path_exists(expected_result));
	EXPECT_TRUE(path_exists(to_native_path(input)));
#if !defined(WIN32)
	EXPECT_EQ(expected_result, to_native_path(input));
#endif
}

TEST(PathConversion, MissingFile)
{
	constexpr auto nonexistent_file = "tests/files/paths/missing.txt";
	ASSERT_FALSE(path_exists(nonexistent_file));
	EXPECT_FALSE(path_exists(to_native_path(nonexistent_file)));
}

} // namespace
