/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2022  The DOSBox Staging Team
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

/* This sample shows how to write a simple unit test for dosbox-staging using
 * Google C++ testing framework.
 *
 * Read Google Test Primer for reference of most available features, macros,
 * and guidance about writing unit tests:
 *
 * https://github.com/google/googletest/blob/master/googletest/docs/primer.md#googletest-primer
 */

/* Include necessary header files; order of headers should be as follows:
 *
 * 1. Header declaring functions/classes being tested
 * 2. <gtest/gtest.h>, which declares the testing framework
 * 3. Additional system headers (if needed)
 * 4. Additional dosbox-staging headers (if needed)
 */

#include "shell.h"

#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "dosbox_test_fixture.h"

namespace {

using namespace testing;

class DOS_Shell_REDIRTest : public DOSBoxTestFixture {};

class MockDOS_Shell : public DOS_Shell {
public:
	/**
	 * NOTE: If we need to call the actual object, we use this. By
	 * default, the mocked functions return whatever we tell it to
	 * (if given a .WillOnce(Return(...)), or a default value
	 * (false).
	 */

private:
	DOS_Shell real_; // Keeps an instance of the real in the mock.
};

TEST_F(DOS_Shell_REDIRTest, CMD_Redirection)
{
	MockDOS_Shell shell;
	bool append;
	char line[CROSS_LEN];
	std::string in = "", out = "", pipe = "";

	strcpy(line, "echo hello!");
	shell.GetRedirection(line, in, out, pipe, &append);
	EXPECT_STREQ(line, "echo hello!");
	EXPECT_TRUE(in == "");
	EXPECT_TRUE(out == "");
	EXPECT_TRUE(pipe == "");
	EXPECT_EQ(append, false);

	in = out = pipe = "";
	strcpy(line, "echo test>test.txt");
	shell.GetRedirection(line, in, out, pipe, &append);
	EXPECT_STREQ(line, "echo test");
	EXPECT_TRUE(in == "");
	EXPECT_TRUE(out == "test.txt");
	EXPECT_TRUE(pipe == "");
	EXPECT_EQ(append, false);

	in = out = pipe = "";
	strcpy(line, "sort<test.txt");
	shell.GetRedirection(line, in, out, pipe, &append);
	EXPECT_STREQ(line, "sort");
	EXPECT_TRUE(in == "test.txt");
	EXPECT_TRUE(out == "");
	EXPECT_TRUE(pipe == "");
	EXPECT_EQ(append, false);

	in = out = pipe = "";
	strcpy(line, "less<in.txt>out.txt");
	shell.GetRedirection(line, in, out, pipe, &append);
	EXPECT_STREQ(line, "less");
	EXPECT_TRUE(in == "in.txt");
	EXPECT_TRUE(out == "out.txt");
	EXPECT_TRUE(pipe == "");
	EXPECT_EQ(append, false);

	in = out = pipe = "";
	strcpy(line, "less<in.txt>NUL");
	shell.GetRedirection(line, in, out, pipe, &append);
	EXPECT_STREQ(line, "less");
	EXPECT_EQ(in, "in.txt");
	EXPECT_EQ(out, "NUL");
	EXPECT_EQ(pipe, "");
	EXPECT_EQ(append, false);

	in = out = pipe = "";
	strcpy(line, "less<in.txt>NUL:");
	shell.GetRedirection(line, in, out, pipe, &append);
	EXPECT_STREQ(line, "less:");
	EXPECT_EQ(in, "in.txt");
	EXPECT_EQ(out, "NUL");
	EXPECT_EQ(pipe, "");
	EXPECT_EQ(append, false);

	in = out = pipe = "";
	strcpy(line, "more<file.txt|sort");
	shell.GetRedirection(line, in, out, pipe, &append);
	EXPECT_STREQ(line, "more");
	EXPECT_TRUE(in == "file.txt");
	EXPECT_TRUE(out == "");
	EXPECT_TRUE(pipe == "sort");
	EXPECT_EQ(append, false);

	in = out = pipe = "";
	strcpy(line, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa<in.txt>>out.txt");
	shell.GetRedirection(line, in, out, pipe, &append);
	EXPECT_STREQ(line, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
	EXPECT_TRUE(in == "in.txt");
	EXPECT_TRUE(out == "out.txt");
	EXPECT_TRUE(pipe == "");
	EXPECT_EQ(append, true);

	in = out = pipe = "";
	strcpy(line, "");
	shell.GetRedirection(line, in, out, pipe, &append);
	EXPECT_STREQ(line, "");
	EXPECT_TRUE(in == "");
	EXPECT_TRUE(out == "");
	EXPECT_TRUE(pipe == "");
	EXPECT_EQ(append, false);

	in = out = pipe = "";
	strcpy(line, " echo  test < in.txt > out.txt ");
	shell.GetRedirection(line, in, out, pipe, &append);
	EXPECT_STREQ(line, " echo  test   ");
	EXPECT_TRUE(in == "in.txt");
	EXPECT_TRUE(out == "out.txt");
	EXPECT_TRUE(pipe == "");
	EXPECT_EQ(append, false);

	in = out = pipe = "";
	strcpy(line, "dir || more");
	shell.GetRedirection(line, in, out, pipe, &append);
	EXPECT_STREQ(line, "dir ");
	EXPECT_TRUE(in == "");
	EXPECT_TRUE(out == "");
	EXPECT_TRUE(pipe == "| more");
	EXPECT_EQ(append, false);

	in = out = pipe = "";
	strcpy(line, "dir *.bat << in.txt >> out.txt");
	shell.GetRedirection(line, in, out, pipe, &append);
	EXPECT_STREQ(line, "dir *.bat  in.txt ");
	EXPECT_TRUE(in == "<");
	EXPECT_TRUE(out == "out.txt");
	EXPECT_TRUE(pipe == "");
	EXPECT_EQ(append, true);

	in = out = pipe = "";
	strcpy(line, "echo test>out1.txt>>out2.txt");
	shell.GetRedirection(line, in, out, pipe, &append);
	EXPECT_STREQ(line, "echo test");
	EXPECT_TRUE(in == "");
	EXPECT_TRUE(out == "out1.txt>>out2.txt");
	EXPECT_TRUE(pipe == "");
	EXPECT_EQ(append, false);
}

} // namespace
