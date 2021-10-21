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

#include "dos_inc.h"

#include <iostream>
#include <iterator>
#include <string>

#include <gtest/gtest.h>

#include "control.h"
#include "dos_system.h"

// Open anonymous namespace (this is Google Test requirement)
namespace {

class DOS_FilesTest : public ::testing::Test {
public:
	DOS_FilesTest()
	        : arg_c_str("-conf tests/files/dosbox-staging-tests.conf\0"),
	          argv{arg_c_str},
	          com_line(1, argv),
	          config(Config(&com_line))
	{
		control = &config;
	}

	void SetUp() override
	{
		// This will register all the init functions, but won't run them
		DOSBOX_Init();

		for (auto section_name : sections) {
			_sec = control->GetSection(section_name);
			// NOTE: Some of the sections will return null pointers,
			// if you add a section below, make sure to test for
			// nullptr before executing early init.
			_sec->ExecuteEarlyInit();
		}

		for (auto section_name : sections) {
			_sec = control->GetSection(section_name);
			_sec->ExecuteInit();
		}
	}

	void TearDown() override
	{
		std::vector<std::string>::reverse_iterator r = sections.rbegin();
		for (; r != sections.rend(); ++r)
			control->GetSection(*r)->ExecuteDestroy();
	}

private:
	char const *arg_c_str;
	const char *argv[1];
	CommandLine com_line;
	Config config;
	Section *_sec;
	// Only init these sections for our tests
	std::vector<std::string> sections{"dosbox", "cpu",      "mixer",
	                                  "midi",   "sblaster", "speaker",
	                                  "serial", "dos",      "autoexec"};
};

TEST_F(DOS_FilesTest, DOS_MakeName_FailOnNull)
{
	Bit8u drive;
	char fulldir[DOS_PATHLENGTH];
	bool result = DOS_MakeName("\0", fulldir, &drive);
	EXPECT_EQ(false, result);
}

TEST_F(DOS_FilesTest, DOS_MakeName_DriveNotFound)
{
	Bit8u drive;
	char fulldir[DOS_PATHLENGTH];
	EXPECT_EQ(false, DOS_MakeName("B:\\AUTOEXEC.BAT", fulldir, &drive));
}

TEST_F(DOS_FilesTest, DOS_MakeName_Z_AUTOEXEC_BAT_exists)
{
	Bit8u drive;
	char fulldir[DOS_PATHLENGTH];
	EXPECT_EQ(true, DOS_MakeName("Z:\\AUTOEXEC.BAT", fulldir, &drive));
}

} // namespace
