/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2024-2024  The DOSBox Staging Team
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

#include <gtest/gtest.h>

struct cmd_move_arguments {
	std::vector<std::string> sources = {};
	std::string destination          = {};
	const char* error                = nullptr;
};

cmd_move_arguments cmd_move_parse_arguments(const char* args);

TEST(cmd_move_parse_arguments, multiple_sources)
{
	cmd_move_arguments ret = cmd_move_parse_arguments(
	        "   a.bat   ,   b.bat        ,c.bat,d.bat  destdir");
	EXPECT_EQ(ret.error, nullptr);
	EXPECT_EQ(ret.destination, "destdir");
	ASSERT_EQ(ret.sources.size(), 4);
	EXPECT_EQ(ret.sources[0], "a.bat");
	EXPECT_EQ(ret.sources[1], "b.bat");
	EXPECT_EQ(ret.sources[2], "c.bat");
	EXPECT_EQ(ret.sources[3], "d.bat");
}

TEST(cmd_move_parse_arguments, too_few_arguments)
{
	cmd_move_arguments ret = cmd_move_parse_arguments("a");
	ASSERT_NE(ret.error, nullptr);
	EXPECT_EQ(strcmp(ret.error, "SHELL_MISSING_PARAMETER"), 0);
}

TEST(cmd_move_parse_arguments, too_many_arguments)
{
	cmd_move_arguments ret = cmd_move_parse_arguments("a b c");
	ASSERT_NE(ret.error, nullptr);
	EXPECT_EQ(strcmp(ret.error, "SHELL_TOO_MANY_PARAMETERS"), 0);
}

TEST(cmd_move_parse_arguments, weird_quotes)
{
	// This actually works in MS-DOS 6.22
	cmd_move_arguments ret = cmd_move_parse_arguments(
	        "\"a.bat,\"  b.\"ba\"t  \",\"  c.bat   d\"estdi\"r");
	EXPECT_EQ(ret.error, nullptr);
	EXPECT_EQ(ret.destination, "destdir");
	ASSERT_EQ(ret.sources.size(), 3);
	EXPECT_EQ(ret.sources[0], "a.bat");
	EXPECT_EQ(ret.sources[1], "b.bat");
	EXPECT_EQ(ret.sources[2], "c.bat");
}
