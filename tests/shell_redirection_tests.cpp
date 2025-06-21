// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

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

#include <gtest/gtest.h>

using RedirectionResults = DOS_Shell::RedirectionResults;

static void test_redirection(const char* line, const auto& expected_results)
{
	const auto test_results = DOS_Shell::GetRedirection(line);
	assert(test_results);

	EXPECT_EQ(test_results->processed_line, expected_results.processed_line);
	EXPECT_EQ(test_results->in_file, expected_results.in_file);
	EXPECT_EQ(test_results->out_file, expected_results.out_file);
	EXPECT_EQ(test_results->pipe_target, expected_results.pipe_target);
	EXPECT_EQ(test_results->is_appending, expected_results.is_appending);
}

TEST(DOS_Shell_GetRedirection, BasicCommand)
{
	constexpr auto line = "echo hello!";

	const auto expected_results = RedirectionResults{.processed_line = line};

	// verified in MS-DOS 6.22
	test_redirection(line, expected_results);
}

TEST(DOS_Shell_GetRedirection, BasicCommandFrontPadding)
{
	constexpr auto line = "  echo hello!";

	const auto expected_results = RedirectionResults{.processed_line = line};

	// verified in MS-DOS 6.22
	test_redirection(line, expected_results);
}

TEST(DOS_Shell_GetRedirection, BasicCommandBackPadding)
{
	constexpr auto line = "echo hello!  ";

	const auto expected_results = RedirectionResults{.processed_line = line};

	// verified in MS-DOS 6.22
	test_redirection(line, expected_results);
}

TEST(DOS_Shell_GetRedirection, BasicCommandFrontAndBackPadding)
{
	constexpr auto line = "  echo hello!  ";

	const auto expected_results = RedirectionResults{.processed_line = line};

	// verified in MS-DOS 6.22
	test_redirection(line, expected_results);
}

TEST(DOS_Shell_GetRedirection, OutputNoPadding)
{
	constexpr auto line = "echo test>test.txt";

	const auto expected_results = RedirectionResults{.processed_line = "echo test",
	                                                 .out_file = "test.txt"};

	// verified in MS-DOS 6.22 (test.txt is 6 bytes)
	test_redirection(line, expected_results);
}

TEST(DOS_Shell_GetRedirection, OutputNoPaddingWithColons)
{
	constexpr auto line = "echo test>test.txt:>test.txt:";

	const auto expected_results = RedirectionResults{.processed_line = "echo test ",
	                                                 .out_file = "test.txt"};

	// verified in MS-DOS 6.22 (test.txt is 7 bytes)
	test_redirection(line, expected_results);
}

TEST(DOS_Shell_GetRedirection, OutputFrontPadding1)
{
	constexpr auto line = "echo test >test.txt";

	const auto expected_results = RedirectionResults{.processed_line = "echo test ",
	                                                 .out_file = "test.txt"};

	// verified in MS-DOS 6.22 (test.txt is 6 bytes)
	test_redirection(line, expected_results);
}

TEST(DOS_Shell_GetRedirection, OutputFrontPadding2)
{
	constexpr auto line = "echo test> test.txt";

	const auto expected_results = RedirectionResults{.processed_line = "echo test",
	                                                 .out_file = "test.txt"};

	// verified in MS-DOS 6.22 (test.txt is 6 bytes)
	test_redirection(line, expected_results);
}

TEST(DOS_Shell_GetRedirection, OutputFrontPadding3)
{
	constexpr auto line = "echo test > test.txt";

	const auto expected_results = RedirectionResults{.processed_line = "echo test ",
	                                                 .out_file = "test.txt"};

	// verified in MS-DOS 6.22 (test.txt is 7 bytes)
	test_redirection(line, expected_results);
}

TEST(DOS_Shell_GetRedirection, OutputBackPadding)
{
	constexpr auto line = "echo test>test.txt  ";

	const auto expected_results = RedirectionResults{.processed_line = "echo test  ",
	                                                 .out_file = "test.txt"};

	// verified in MS-DOS 6.22 (test.txt is 8 bytes)
	test_redirection(line, expected_results);
}
TEST(DOS_Shell_GetRedirection, OutputFrontAndBackPadding)
{
	constexpr auto line = "echo test > test.txt ";

	const auto expected_results = RedirectionResults{.processed_line = "echo test  ",
	                                                 .out_file = "test.txt"};

	// verified in MS-DOS 6.22 (test.txt is 8 bytes)
	test_redirection(line, expected_results);
}

TEST(DOS_Shell_GetRedirection, Input)
{
	constexpr auto line = "sort<test.txt";

	const auto expected_results = RedirectionResults{.processed_line = "sort",
	                                                 .in_file = "test.txt"};

	// verified in MS-DOS 6.22
	test_redirection(line, expected_results);
}

TEST(DOS_Shell_GetRedirection, InputAndOutput)
{
	constexpr auto line = "less<in.txt>out.txt";

	const auto expected_results = RedirectionResults{.processed_line = "less",
	                                                 .in_file  = "in.txt",
	                                                 .out_file = "out.txt"};

	// verified in MS-DOS 6.22
	test_redirection(line, expected_results);
}

TEST(DOS_Shell_GetRedirection, InputAndNullOutput)
{
	constexpr auto line = "less<in.txt>NUL";

	const auto expected_results = RedirectionResults{.processed_line = "less",
	                                                 .in_file  = "in.txt",
	                                                 .out_file = "NUL"};

	// verified in MS-DOS 6.22
	test_redirection(line, expected_results);
}

TEST(DOS_Shell_GetRedirection, InputAndNullOutputWithColon)
{
	constexpr auto line = "less<in.txt>NUL:";

	const auto expected_results = RedirectionResults{.processed_line = "less",
	                                                 .in_file  = "in.txt",
	                                                 .out_file = "NUL"};

	// verified in MS-DOS 6.22
	test_redirection(line, expected_results);
}

TEST(DOS_Shell_GetRedirection, InputAndNullOutputWithColonAndWhitespace)
{
	constexpr auto line = "less < in.txt > NUL:";

	const auto expected_results = RedirectionResults{.processed_line = "less ",
	                                                 .in_file  = "in.txt",
	                                                 .out_file = "NUL"};

	// verified in MS-DOS 6.22
	test_redirection(line, expected_results);
}

TEST(DOS_Shell_GetRedirection, InputAndOutputWithColonAndWhitespace)
{
	constexpr auto line = "less < in.txt > OUT:";

	const auto expected_results = RedirectionResults{.processed_line = "less ",
	                                                 .in_file  = "in.txt",
	                                                 .out_file = "OUT"};

	// verified in MS-DOS 6.22
	test_redirection(line, expected_results);
}

TEST(DOS_Shell_GetRedirection, InputAndPipe)
{
	constexpr auto line = "more<file.txt|sort";

	const auto expected_results = RedirectionResults{.processed_line = "more",
	                                                 .in_file = "file.txt",
	                                                 .pipe_target = "sort"};

	// verified in MS-DOS 6.22
	test_redirection(line, expected_results);
}

TEST(DOS_Shell_GetRedirection, InputAndOutputLongLine)
{
	constexpr auto line = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa<in.txt>>out.txt";

	const auto expected_results = RedirectionResults{
	        .processed_line = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
	        .in_file      = "in.txt",
	        .out_file     = "out.txt",
	        .is_appending = true};

	// verified in MS-DOS 6.22
	test_redirection(line, expected_results);
}

TEST(DOS_Shell_GetRedirection, EmptyLine)
{
	constexpr auto line = "";

	const RedirectionResults expected_results = {};

	test_redirection(line, expected_results);
}

TEST(DOS_Shell_GetRedirection, InputAndOutputExtraLineSpacing)
{
	constexpr auto line = " echo  test < in.txt > out.txt ";

	const auto expected_results = RedirectionResults{.processed_line = " echo  test  ",
	                                                 .in_file  = "in.txt",
	                                                 .out_file = "out.txt"};

	// verified in MS-DOS 6.22
	test_redirection(line, expected_results);
}

TEST(DOS_Shell_GetRedirection, InvalidSyntaxPipeWithExtraPipe)
{
	constexpr auto line = "dir || more";

	const auto test_results = DOS_Shell::GetRedirection(line);

	// Syntax error in MS-DOS 6.22
	EXPECT_FALSE(test_results);
}

TEST(DOS_Shell_GetRedirection, InvalidSyntaxOutputWithExtraPipe)
{
	constexpr auto line = "dir> |out.txt";

	const auto test_results = DOS_Shell::GetRedirection(line);

	// Syntax error in MS-DOS 6.22
	EXPECT_FALSE(test_results);
}

TEST(DOS_Shell_GetRedirection, InvalidSyntaxInputWithExtraPipe)
{
	constexpr auto line = "dir <| in.txt";

	const auto test_results = DOS_Shell::GetRedirection(line);

	// Syntax error in MS-DOS 6.22
	EXPECT_FALSE(test_results);
}

TEST(DOS_Shell_GetRedirection, InvalidSyntaxPipeWithExtraInput)
{
	constexpr auto line = "dir| < more";

	const auto test_results = DOS_Shell::GetRedirection(line);

	// Syntax error in MS-DOS 6.22
	EXPECT_FALSE(test_results);
}

TEST(DOS_Shell_GetRedirection, InvalidSyntaxPipeWithExtraOutput)
{
	constexpr auto line = "dir|>more";

	const auto test_results = DOS_Shell::GetRedirection(line);

	// Syntax error in MS-DOS 6.22
	EXPECT_FALSE(test_results);
}

TEST(DOS_Shell_GetRedirection, InvalidSyntaxOutputWithExtraOutput)
{
	constexpr auto line = "dir > >out.txt";

	const auto test_results = DOS_Shell::GetRedirection(line);

	// Syntax error in MS-DOS 6.22
	EXPECT_FALSE(test_results);
}

TEST(DOS_Shell_GetRedirection, InvalidSyntaxOutputWithExtraInput)
{
	constexpr auto line = "more >< in.txt";

	const auto test_results = DOS_Shell::GetRedirection(line);

	// Syntax error in MS-DOS 6.22
	EXPECT_FALSE(test_results);
}

TEST(DOS_Shell_GetRedirection, InvalidSyntaxInputWithExtraInput)
{
	constexpr auto line = "more< <in.txt";

	const auto test_results = DOS_Shell::GetRedirection(line);

	// Syntax error in MS-DOS 6.22
	EXPECT_FALSE(test_results);
}

TEST(DOS_Shell_GetRedirection, InvalidSyntaxInputWithExtraOutput)
{
	constexpr auto line = "dir < > in.txt";

	const auto test_results = DOS_Shell::GetRedirection(line);

	// Syntax error in MS-DOS 6.22
	EXPECT_FALSE(test_results);
}

TEST(DOS_Shell_GetRedirection, InvalidSyntaxMany)
{
	constexpr auto line = "dir < || > in.txt";

	const auto test_results = DOS_Shell::GetRedirection(line);

	// Syntax error in MS-DOS 6.22
	EXPECT_FALSE(test_results);
}

TEST(DOS_Shell_GetRedirection, InvalidSyntaxMany2)
{
	constexpr auto line = "dir<<<|||||in.txt";

	const auto test_results = DOS_Shell::GetRedirection(line);

	// Syntax error in MS-DOS 6.22
	EXPECT_FALSE(test_results);
}

TEST(DOS_Shell_GetRedirection, DoubleInputOperator)
{
	constexpr auto line = "dir *.bat << in1.txt << in2.txt";

	const auto expected_results = RedirectionResults{.processed_line = "dir *.bat ",
	                                                 .in_file = "in2.txt",
	                                                 .is_appending = true};

	// verified in MS-DOS 6.22
	test_redirection(line, expected_results);
}

TEST(DOS_Shell_GetRedirection, TwoInputs)
{
	constexpr auto line = "more < in1.txt < in2.txt";

	const auto expected_results = RedirectionResults{.processed_line = "more ",
	                                                 .in_file = "in2.txt"};

	// verified in MS-DOS 6.22 (last input wins)
	test_redirection(line, expected_results);
}

TEST(DOS_Shell_GetRedirection, TwoInputsNoWhitespace)
{
	constexpr auto line = "more<in1.txt<in2.txt";

	const auto expected_results = RedirectionResults{.processed_line = "more",
	                                                 .in_file = "in2.txt"};

	// verified in MS-DOS 6.22 (last input wins)
	test_redirection(line, expected_results);
}

TEST(DOS_Shell_GetRedirection, TwoOutputsNoTrailingWhitespace)
{
	constexpr auto line = "echo test>out1.txt>out2.txt";

	const auto expected_results = RedirectionResults{.processed_line = "echo test ",
	                                                 .out_file = "out2.txt"};

	// verified in MS-DOS 6.22 (out2.txt is 7 bytes)
	test_redirection(line, expected_results);
}

TEST(DOS_Shell_GetRedirection, TwoOutputsNoTrailingWhitespace2)
{
	constexpr auto line = "echo test>    out1.txt>     out2.txt";

	const auto expected_results = RedirectionResults{.processed_line = "echo test ",
	                                                 .out_file = "out2.txt"};

	// verified in MS-DOS 6.22
	test_redirection(line, expected_results);
}

TEST(DOS_Shell_GetRedirection, TwoOutputsFirstTrailingSpace)
{
	constexpr auto line = "echo test>out1.txt >out2.txt";

	const auto expected_results = RedirectionResults{.processed_line = "echo test ",
	                                                 .out_file = "out2.txt"};

	// verified in MS-DOS 6.22
	test_redirection(line, expected_results);
}

TEST(DOS_Shell_GetRedirection, TwoOutputsFirstTrailingTwoSpaces)
{
	constexpr auto line = "echo test>out1.txt  >out2.txt";

	const auto expected_results = RedirectionResults{.processed_line = "echo test  ",
	                                                 .out_file = "out2.txt"};

	// verified in MS-DOS 6.22
	test_redirection(line, expected_results);
}

TEST(DOS_Shell_GetRedirection, TwoOutputsFirstBothTrailing)
{
	constexpr auto line = "echo test>out1.txt >out2.txt ";

	const auto expected_results = RedirectionResults{.processed_line = "echo test  ",
	                                                 .out_file = "out2.txt"};

	// verified in MS-DOS 6.22
	test_redirection(line, expected_results);
}
