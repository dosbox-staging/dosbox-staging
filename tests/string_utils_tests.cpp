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

#include "string_utils.h"

#include <gtest/gtest.h>

#include <string>

#include "support.h"

namespace {

TEST(StartsWith, Prefix)
{
	EXPECT_TRUE(starts_with("ab", "abcd"));
	EXPECT_TRUE(starts_with("ab", std::string{"abcd"}));
}

TEST(StartsWith, NotPrefix)
{
	EXPECT_FALSE(starts_with("xy", "abcd"));
	EXPECT_FALSE(starts_with("xy", std::string{"abcd"}));
}

TEST(StartsWith, TooLongPrefix)
{
	EXPECT_FALSE(starts_with("abcd", "ab"));
	EXPECT_FALSE(starts_with("abcd", std::string{"ab"}));
}

TEST(StartsWith, EmptyPrefix)
{
	EXPECT_TRUE(starts_with("", "abcd"));
	EXPECT_TRUE(starts_with("", std::string{"abcd"}));
}

TEST(StartsWith, EmptyString)
{
	EXPECT_FALSE(starts_with("ab", ""));
	EXPECT_FALSE(starts_with("ab", std::string{""}));
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

} // namespace
