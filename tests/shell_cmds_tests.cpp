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

#include "shell.h"

#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "dosbox_test_fixture.h"
#include "../src/shell/shell_cmds.cpp"

namespace {

using namespace testing;

class DOS_Shell_CMDSTest : public DOSBoxTestFixture {};

class MockDOS_Shell : public DOS_Shell {
	public:
		/*
		 * NOTE: If we need to call the actual object, we use this
		MockDOS_Shell() {
			// delegate call to the real object.
			ON_CALL(*this, execute_shell_cmd).WillByDefault([this](char *name, char *arguments) {
				std::cout << "CALLING REAL execute_shell_cmd!\n";
				return real_.execute_shell_cmd(name, arguments);
			});
		}
		*/
		MOCK_METHOD(bool, execute_shell_cmd, (char *name, char *arguments), (override));

	private:
		DOS_Shell real_;  // Keeps an instance of the real in the mock.
};

TEST_F(DOS_Shell_CMDSTest, DoCommand_Basic_Rejections)
{
	MockDOS_Shell shell;
	std::string line = "\t";
	char * line_c_str = const_cast<char *>(line.c_str());
	EXPECT_NO_THROW({
		shell.DoCommand(line_c_str);
	});
	EXPECT_CALL(shell, execute_shell_cmd(_, _)).Times(0);
}

TEST_F(DOS_Shell_CMDSTest, DoCommand_Splits_Cmd_and_Args)
{
	std::string input = "DIR *.*";
	char * input_c_str = const_cast<char *>(input.c_str());

	MockDOS_Shell shell;
	EXPECT_CALL(shell, execute_shell_cmd(StrEq("DIR"), StrEq(" *.*"))).Times(1);
	EXPECT_NO_THROW({
		shell.DoCommand(input_c_str);
	});
}

TEST_F(DOS_Shell_CMDSTest, DoCommand_All_Cmds_Do_Valid_Execute)
{
	MockDOS_Shell shell;
	for (std::pair<std::string, SHELL_Cmd> cmd : shell_cmds) {
		std::string input = cmd.first;
		char * input_c_str = const_cast<char *>(input.c_str());

		MockDOS_Shell shell;
		EXPECT_CALL(shell, execute_shell_cmd(StrEq(input), StrEq(""))).Times(1);
		EXPECT_NO_THROW({
			shell.DoCommand(input_c_str);
		});
	}
}

} // namespace
