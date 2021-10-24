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

#include <iterator>
#include <string>

#include <gtest/gtest.h>

#include "control.h"
#include "dos_system.h"
#include "shell.h"
#include "string_utils.h"

#include "../src/dos/dos_files.cpp"

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

void assert_DTAExtendName(std::string input,
                          std::string expected_name,
                          std::string expected_ext)
{
	char *const input_str = const_cast<char *const>(&input.c_str()[0]);
	// char * const input_name = &input[0];
	// needs to be minimum length of the input up to the dot + 1 (null)
	char output_filename[DOS_PATHLENGTH];
	char *const filename = &output_filename[0];
	char output_ext[DOS_PATHLENGTH];
	char *const ext = &output_ext[0];

	DTAExtendName(input_str, filename, ext);

	// mutates input up to dot
	EXPECT_EQ(filename, expected_name);
	EXPECT_EQ(ext, expected_ext);
}

void assert_DOS_MakeName(char const *const input,
                         bool exp_result,
                         std::string exp_fullname = "",
                         int exp_drive = 0)
{
	Bit8u drive_result;
	char fullname_result[DOS_PATHLENGTH];
	bool result = DOS_MakeName(input, fullname_result, &drive_result);
	EXPECT_EQ(result, exp_result);
	// if we expected success, also test these
	if (exp_result) {
		EXPECT_EQ(std::string(fullname_result), exp_fullname);
		EXPECT_EQ(drive_result, exp_drive);
	}
}

TEST_F(DOS_FilesTest, DOS_MakeName_Basic_Failures)
{
	// make sure we get failures, not explosions
	assert_DOS_MakeName("\0", false);
	assert_DOS_MakeName(" ", false);
	assert_DOS_MakeName(" NAME", false);
	assert_DOS_MakeName("\1:\\AUTOEXEC.BAT", false);
	assert_DOS_MakeName(nullptr, false);
	assert_DOS_MakeName("B:\\AUTOEXEC.BAT", false);
}

TEST_F(DOS_FilesTest, DOS_MakeName_Z_AUTOEXEC_BAT_exists)
{
	assert_DOS_MakeName("Z:\\AUTOEXEC.BAT", true, "AUTOEXEC.BAT", 25);
}

// This captures a particularity of the DOSBox code where the
// drive index is set even though the path failed. this could have
// ramifications across the codebase if not replicated
TEST_F(DOS_FilesTest, DOS_MakeName_Drive_Index_Set_On_Failure)
{
	Bit8u drive_result;
	char fullname_result[DOS_PATHLENGTH];
	bool result;
	result = DOS_MakeName("A:\r\n", fullname_result, &drive_result);
	EXPECT_EQ(result, false);
	EXPECT_EQ(drive_result, 0);
	result = DOS_MakeName("B:\r\n", fullname_result, &drive_result);
	EXPECT_EQ(drive_result, 1);
	EXPECT_EQ(result, false);
	result = DOS_MakeName("C:\r\n", fullname_result, &drive_result);
	EXPECT_EQ(drive_result, 2);
	EXPECT_EQ(result, false);
	result = DOS_MakeName("Z:\r\n", fullname_result, &drive_result);
	EXPECT_EQ(drive_result, 25);
	EXPECT_EQ(result, false);
}

TEST_F(DOS_FilesTest, DOS_MakeName_Uppercase)
{
	assert_DOS_MakeName("Z:\\autoexec.bat", true, "AUTOEXEC.BAT", 25);
	// lower case
	assert_DOS_MakeName("z:\\AUTOEXEC.BAT", true, "AUTOEXEC.BAT", 25);
	// current dir isn't uppercased if it's not already
	safe_strcpy(Drives[25]->curdir, "Windows\\Folder");
	assert_DOS_MakeName("autoexec.bat", true,
	                    "Windows\\Folder\\AUTOEXEC.BAT", 25);
}

TEST_F(DOS_FilesTest, DOS_MakeName_CONVERTS_FWD_SLASH)
{
	assert_DOS_MakeName("Z:/AUTOEXEC.BAT", true, "AUTOEXEC.BAT", 25);
}

// spaces get stripped out before processing (\t, \r, etc, are illegal chars,
// not whitespace)
TEST_F(DOS_FilesTest, DOS_MakeName_STRIP_SPACE)
{
	assert_DOS_MakeName("Z:\\   A U T  OE X   EC     .BAT", true,
	                    "AUTOEXEC.BAT", 25);
	assert_DOS_MakeName("Z: \\   A U T  OE X   EC     .BAT", true,
	                    "AUTOEXEC.BAT", 25);
	assert_DOS_MakeName("12345   678.123", true, "12345678.123", 25);
	// except here, whitespace isn't stripped & causes failure
	assert_DOS_MakeName("Z :\\AUTOEXEC.BAT", false);
}

TEST_F(DOS_FilesTest, DOS_MakeName_Dir_Handling)
{
	assert_DOS_MakeName("Z:\\CODE\\", true, "CODE", 25);
	assert_DOS_MakeName("Z:\\CODE\\AUTOEXEC.BAT", true, "CODE\\AUTOEXEC.BAT", 25);
	assert_DOS_MakeName("Z:\\DIR\\UNTERM", true, "DIR\\UNTERM", 25);
	// trailing gets trimmed
	assert_DOS_MakeName("Z:\\CODE\\TERM\\", true, "CODE\\TERM", 25);
}

TEST_F(DOS_FilesTest, DOS_MakeName_Assumes_Current_Drive_And_Dir)
{
	// when passed only a filename, assume default drive and current dir
	assert_DOS_MakeName("AUTOEXEC.BAT", true, "AUTOEXEC.BAT", 25);
	// artificially change directory
	safe_strcpy(Drives[25]->curdir, "CODE");
	assert_DOS_MakeName("AUTOEXEC.BAT", true, "CODE\\AUTOEXEC.BAT", 25);
	// artificially change directory
	safe_strcpy(Drives[25]->curdir, "CODE\\BIN");
	assert_DOS_MakeName("AUTOEXEC.BAT", true, "CODE\\BIN\\AUTOEXEC.BAT", 25);
	// ignores current dir and goes to root
	assert_DOS_MakeName("\\AUTOEXEC.BAT", true, "AUTOEXEC.BAT", 25);
	safe_strcpy(Drives[25]->curdir, "");
	assert_DOS_MakeName("Z:\\CODE\\BIN", true, "CODE\\BIN", 25);
	assert_DOS_MakeName("Z:", true, "", 25);
	assert_DOS_MakeName("Z:\\", true, "", 25);
	// This is a bug but we need to capture this functionality
	safe_strcpy(Drives[25]->curdir, "CODE\\BIN\\");
	assert_DOS_MakeName("AUTOEXEC.BAT", true, "CODE\\BIN\\\\AUTOEXEC.BAT", 25);
	safe_strcpy(Drives[25]->curdir, "CODE\\BIN\\\\");
	assert_DOS_MakeName("AUTOEXEC.BAT", true, "CODE\\BIN\\\\\\AUTOEXEC.BAT", 25);
}

// This tests that illegal char matching happens AFTER 8.3 trimming
TEST_F(DOS_FilesTest, DOS_MakeName_Illegal_Chars_After_8_3)
{
	safe_strcpy(Drives[25]->curdir, "BIN");
	assert_DOS_MakeName("\n2345678AAAAABBB.BAT", false);
	assert_DOS_MakeName("12345678.\n23BBBBBAAA", false);
	assert_DOS_MakeName("12345678AAAAABB\n.BAT", true, "BIN\\12345678.BAT", 25);
	assert_DOS_MakeName("12345678.123BBBBBAAA\n", true, "BIN\\12345678.123", 25);
}

TEST_F(DOS_FilesTest, DOS_MakeName_DOS_PATHLENGTH_checks)
{
	// Right on the line ...
	safe_strcpy(Drives[25]->curdir,
	            "aaaaaaaaa\\aaaaaaaaa\\aaaaaaaaa\\"
	            "aaaaaaaaa\\aaaaaaaaa\\aaaaaaaaa\\aaaaaaaaaa");
	assert_DOS_MakeName("BBBBB.BB", true,
	                    "aaaaaaaaa\\aaaaaaaaa\\aaaaaaaaa\\aaaaaaaaa\\"
	                    "aaaaaaaaa\\aaaaaaaaa\\aaaaaaaaaa\\BBBBB.BB",
	                    25);
	// Equal to...
	assert_DOS_MakeName("BBBBBB.BB", false);
	// Over...
	assert_DOS_MakeName("BBBBBBB.BB", false);
}

TEST_F(DOS_FilesTest, DOS_MakeName_Enforce_8_3)
{
	safe_strcpy(Drives[25]->curdir, "BIN");
	assert_DOS_MakeName("12345678AAAAABBBB.BAT", true, "BIN\\12345678.BAT", 25);
	assert_DOS_MakeName("12345678.123BBBBBAAAA", true, "BIN\\12345678.123", 25);
}

TEST_F(DOS_FilesTest, DOS_MakeName_Dot_Handling)
{
	safe_strcpy(Drives[25]->curdir, "WINDOWS\\CONFIG");
	assert_DOS_MakeName(".", true, "WINDOWS\\CONFIG", 25);
	assert_DOS_MakeName("..", true, "WINDOWS", 25);
	assert_DOS_MakeName("...", true, "", 25);
	assert_DOS_MakeName(".\\AUTOEXEC.BAT", true,
	                    "WINDOWS\\CONFIG\\AUTOEXEC.BAT", 25);
	assert_DOS_MakeName("..\\AUTOEXEC.BAT", true, "WINDOWS\\AUTOEXEC.BAT", 25);
	assert_DOS_MakeName("...\\AUTOEXEC.BAT", true, "AUTOEXEC.BAT", 25);
	safe_strcpy(Drives[25]->curdir, "WINDOWS\\CONFIG\\FOLDER");
	assert_DOS_MakeName("...\\AUTOEXEC.BAT", true, "WINDOWS\\AUTOEXEC.BAT", 25);
	assert_DOS_MakeName("....\\AUTOEXEC.BAT", true, "AUTOEXEC.BAT", 25);
	safe_strcpy(Drives[25]->curdir, "WINDOWS\\CONFIG\\FOLDER\\DEEP");
	assert_DOS_MakeName("....\\AUTOEXEC.BAT", true, "WINDOWS\\AUTOEXEC.BAT", 25);
	assert_DOS_MakeName(".....\\AUTOEXEC.BAT", true, "AUTOEXEC.BAT", 25);
	// make sure we can exceed the depth
	assert_DOS_MakeName("......\\AUTOEXEC.BAT", true, "AUTOEXEC.BAT", 25);
	assert_DOS_MakeName("...........\\AUTOEXEC.BAT", true, "AUTOEXEC.BAT", 25);
	// make sure we have arbitrary expansion
	assert_DOS_MakeName("...\\FOLDER\\...\\AUTOEXEC.BAT", true,
	                    "WINDOWS\\AUTOEXEC.BAT", 25);
	assert_DOS_MakeName("...\\FOLDER\\....\\.\\AUTOEXEC.BAT", true,
	                    "AUTOEXEC.BAT", 25);
}

TEST_F(DOS_FilesTest, DOS_MakeName_No_SlashSlash)
{
	assert_DOS_MakeName("Z:..\\tmp.txt", true, "TMP.TXT", 25);
}

// Exhaustive test of all good chars
TEST_F(DOS_FilesTest, DOS_MakeName_GoodChars)
{
	unsigned char start_letter = 'A';
	unsigned char start_number = '0';
	std::vector<unsigned char> symbols{'$', '#',  '@',  '(',  ')', '!', '%',
	                                   '{', '}',  '`',  '~',  '_', '-', '.',
	                                   '*', '?',  '&',  '\'', '+', '^', 246,
	                                   255, 0xa0, 0xe5, 0xbd, 0x9d};
	for (unsigned char li = 0; li < 26; li++) {
		for (unsigned char ni = 0; ni < 10; ni++) {
			for (auto &c : symbols) {
				unsigned char input_array[3] = {
				        static_cast<unsigned char>(start_letter + li),
				        static_cast<unsigned char>(start_number + ni),
				        c,
				};
				std::string test_input(reinterpret_cast<char *>(
				                               input_array),
				                       3);
				assert_DOS_MakeName(test_input.c_str(), true,
				                    test_input, 25);
			}
		}
	}
}

TEST_F(DOS_FilesTest, DOS_MakeName_Colon_Illegal_Paths)
{
	assert_DOS_MakeName(":..\\tmp.txt", false);
	assert_DOS_MakeName(" :..\\tmp.txt", false);
	assert_DOS_MakeName(": \\tmp.txt", false);
	assert_DOS_MakeName(":", false);
}

// ensures a fix for dark forces installer
TEST_F(DOS_FilesTest, DOS_FindFirst_Ending_Slash)
{
	// `dos` comes from dos_inc.h
	dos.errorcode = DOSERR_NONE;
	EXPECT_FALSE(DOS_FindFirst("Z:\\DARK\\LFD\\", DOS_ATTR_VOLUME, false));
	EXPECT_EQ(dos.errorcode, DOSERR_NO_MORE_FILES);

	dos.errorcode = DOSERR_NONE;
	EXPECT_FALSE(DOS_FindFirst("Z:\\DARK\\", DOS_ATTR_VOLUME, false));
	EXPECT_EQ(dos.errorcode, DOSERR_NO_MORE_FILES);

	// volume names alone don't trigger the failure
	dos.errorcode = DOSERR_NONE;
	EXPECT_TRUE(DOS_FindFirst("Z:\\", DOS_ATTR_VOLUME, false));
	EXPECT_NE(dos.errorcode, DOSERR_NO_MORE_FILES);

	// volume attr NOT required
	dos.errorcode = DOSERR_NONE;
	EXPECT_FALSE(DOS_FindFirst("Z:\\NOMATCH\\", 0, false));
	EXPECT_EQ(dos.errorcode, DOSERR_NO_MORE_FILES);
}

TEST_F(DOS_FilesTest, DOS_FindFirst_Rejects_Invalid_Names)
{
	// triggers failures via DOS_FindFirst
	EXPECT_FALSE(DOS_FindFirst("Z:\\BAD\nDIR\\HI.TXT", 0, false));
	EXPECT_EQ(dos.errorcode, DOSERR_PATH_NOT_FOUND);
}

TEST_F(DOS_FilesTest, DOS_FindFirst_FindVolume)
{
	dos.errorcode = DOSERR_NONE;
	EXPECT_TRUE(DOS_FindFirst("Z", DOS_ATTR_VOLUME, false));
	EXPECT_EQ(dos.errorcode, DOSERR_NONE);
}

TEST_F(DOS_FilesTest, DOS_FindFirst_FindDevice)
{
	dos.errorcode = DOSERR_NONE;
	EXPECT_TRUE(DOS_FindFirst("COM1", DOS_ATTR_DEVICE, false));
	EXPECT_EQ(dos.errorcode, DOSERR_NONE);
}

TEST_F(DOS_FilesTest, DOS_FindFirst_FindFile)
{
	dos.errorcode = DOSERR_NONE;
	EXPECT_TRUE(DOS_FindFirst("Z:\\AUTOEXEC.BAT", 0, false));
	EXPECT_EQ(dos.errorcode, DOSERR_NONE);
}

TEST_F(DOS_FilesTest, DOS_FindFirst_FindFile_Nonexistant)
{
	dos.errorcode = DOSERR_NONE;
	EXPECT_FALSE(DOS_FindFirst("Z:\\AUTOEXEC.NO", 0, false));
	EXPECT_EQ(dos.errorcode, DOSERR_NO_MORE_FILES);
}

// this probably isn't a desirable quality, but figure that out later
TEST_F(DOS_FilesTest, DOS_DTAExtendName_Mutates_Input)
{
	char input_str[] = "123456789AAAA.EXT\0";
	int initial_input_name = strlen(input_str);
	char *const input_name = &input_str[0];
	// needs to be minimum length of the input up to the dot + 1 (null)
	char output_filename[14];
	char *const filename = &output_filename[0];
	char output_ext[4];
	char *const ext = &output_ext[0];

	DTAExtendName(input_name, filename, ext);

	EXPECT_EQ(strlen(input_name), 13);
	EXPECT_NE(initial_input_name, strlen(input_str));
}

TEST_F(DOS_FilesTest, DOS_DTAExtendName_Space_Pads)
{
	assert_DTAExtendName("1234.E  ", "1234    ", "E  ");
}

TEST_F(DOS_FilesTest, DOS_DTAExtendName_Enforces_8_3)
{
	assert_DTAExtendName("12345678ABCDEF.123ABCDE", "12345678", "123");
}

} // namespace
