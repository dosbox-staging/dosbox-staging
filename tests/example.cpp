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

#include "support.h"

#include <gtest/gtest.h>

#include <cstdint>

// Open anonymous namespace (this is Google Test requirement)

namespace {

/* Use the TEST macro to define your tests. You can also create test fixtures
 * using a similar TEST_F macro (see the primer for details).
 *
 * TEST has two parameters: the test case name and the test name.
 * After using the macro, you should define your test logic between a
 * pair of braces.
 *
 * You can use a bunch of macros to indicate the success or failure of a test.
 * EXPECT_TRUE and EXPECT_EQ are examples of such macros, but there are many
 * similar ones, appropriate for various situations.
 */

TEST(DriveIndexTest, BigLetters)
{
	EXPECT_EQ(0, drive_index('A'));
	EXPECT_EQ(25, drive_index('Z'));
}

/* Remember: tests are not code - they should exercise specific edge cases,
 * not re-implement the functionality.
 *
 * When writing *non-trivial* unit tests, it's recommended to follow 
 * "Given-When-Then" style.  It's a style of writing tests, where a single
 * case is split into three sections. Here's a test similar to the previous one,
 * but rewritten using this style:
 */

TEST(DriveIndexTest, SmallLetters)
{
	// "Given" section describes test-prerequisites
	const char input_letter = 'b';

	// "When" section runs the tested code and records the results
	const uint8_t result = drive_index(input_letter);

	// "Then" section describes expectations for the test result
	EXPECT_EQ(1, result);
}

/* Below you'll find an example failing test.
 *
 * Remember to pass code results as second parameter to the macro - this will
 * result in Google Test generating more helpful test output.
 *
 * In this case output will look like this:
 *
 *     [ RUN      ] DriveIndexTest.ExampleFailingTest
 *     example.cpp:110: Failure
 *     Expected equality of these values:
 *       42
 *       drive_index('C')
 *         Which is: '\x2' (2)
 *     [  FAILED  ] DriveIndexTest.ExampleFailingTest (0 ms)
 */

TEST(DriveIndexTest, ExampleFailingTest)
{
	EXPECT_EQ(42, drive_index('C')); // this is clearly incorrect test :)
}

/* Sometimes it might be convenient to disable a test during development for
 * a while. You can accomplish this by prefixing test name with "DISABLED_"
 * prefix.
 *
 * Google Test will print a reminder about disabled tests in test results.
 */

TEST(DriveIndexTest, DISABLED_ExampleFailingTest2)
{
	EXPECT_EQ(42, drive_index('C')); // this is clearly incorrect test :)
}

/* Once the tests are built, run ./tests to see the testing results.
 *
 * You can also use "make test" to build and run tests in one go.
 *
 * See "./tests --help" for additional options (like e.g. filtering which tests
 * should be run or randomizing the tests order).
 */

} // namespace
