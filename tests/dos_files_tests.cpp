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

#include "dos/dos.h"

#include <iterator>
#include <string>

#include <gtest/gtest.h>

#include "config/config.h"
#include "dos/dos_system.h"
#include "dos/drives.h"
#include "shell/shell.h"
#include "utils/string_utils.h"

#include "dosbox_test_fixture.h"
#include "dos/dos_files.cpp"

namespace {

class DOS_FilesTest : public DOSBoxTestFixture {};

void assert_DTAExtendName(const std::string& input_fullname,
                          const std::string_view expected_name,
                          const std::string_view expected_ext)
{
	const auto [output_name, output_ext] = DTAExtendName(input_fullname.c_str());

	// mutates input up to dot
	EXPECT_EQ(output_name, expected_name);
	EXPECT_EQ(output_ext, expected_ext);
}

void assert_DOS_MakeName(const char* const input, bool exp_result,
                         std::string exp_fullname = "", int exp_drive = 0)
{
	uint8_t drive_result;
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
	uint8_t drive_result;
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
	safe_strcpy(Drives.at(25)->curdir, "Windows\\Folder");
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
	safe_strcpy(Drives.at(25)->curdir, "CODE");
	assert_DOS_MakeName("AUTOEXEC.BAT", true, "CODE\\AUTOEXEC.BAT", 25);
	// artificially change directory
	safe_strcpy(Drives.at(25)->curdir, "CODE\\BIN");
	assert_DOS_MakeName("AUTOEXEC.BAT", true, "CODE\\BIN\\AUTOEXEC.BAT", 25);
	// ignores current dir and goes to root
	assert_DOS_MakeName("\\AUTOEXEC.BAT", true, "AUTOEXEC.BAT", 25);
	safe_strcpy(Drives.at(25)->curdir, "");
	assert_DOS_MakeName("Z:\\CODE\\BIN", true, "CODE\\BIN", 25);
	assert_DOS_MakeName("Z:", true, "", 25);
	assert_DOS_MakeName("Z:\\", true, "", 25);
	// This is a bug but we need to capture this functionality
	safe_strcpy(Drives.at(25)->curdir, "CODE\\BIN\\");
	assert_DOS_MakeName("AUTOEXEC.BAT", true, "CODE\\BIN\\\\AUTOEXEC.BAT", 25);
	safe_strcpy(Drives.at(25)->curdir, "CODE\\BIN\\\\");
	assert_DOS_MakeName("AUTOEXEC.BAT", true, "CODE\\BIN\\\\\\AUTOEXEC.BAT", 25);
}

// This tests that illegal char matching happens AFTER 8.3 trimming
TEST_F(DOS_FilesTest, DOS_MakeName_Illegal_Chars_After_8_3)
{
	safe_strcpy(Drives.at(25)->curdir, "BIN");
	assert_DOS_MakeName("\n2345678AAAAABBB.BAT", false);
	assert_DOS_MakeName("12345678.\n23BBBBBAAA", false);
	assert_DOS_MakeName("12345678AAAAABB\n.BAT", true, "BIN\\12345678.BAT", 25);
	assert_DOS_MakeName("12345678.123BBBBBAAA\n", true, "BIN\\12345678.123", 25);
}

TEST_F(DOS_FilesTest, DOS_MakeName_DOS_PATHLENGTH_checks)
{
	// Right on the line ...
	safe_strcpy(Drives.at(25)->curdir,
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
	safe_strcpy(Drives.at(25)->curdir, "BIN");
	assert_DOS_MakeName("12345678AAAAABBBB.BAT", true, "BIN\\12345678.BAT", 25);
	assert_DOS_MakeName("12345678.123BBBBBAAAA", true, "BIN\\12345678.123", 25);
}

TEST_F(DOS_FilesTest, DOS_MakeName_Dot_Handling)
{
	safe_strcpy(Drives.at(25)->curdir, "WINDOWS\\CONFIG");
	assert_DOS_MakeName(".", true, "WINDOWS\\CONFIG", 25);
	assert_DOS_MakeName("..", true, "WINDOWS", 25);
	assert_DOS_MakeName("...", true, "", 25);
	assert_DOS_MakeName(".\\AUTOEXEC.BAT", true,
	                    "WINDOWS\\CONFIG\\AUTOEXEC.BAT", 25);
	assert_DOS_MakeName("..\\AUTOEXEC.BAT", true, "WINDOWS\\AUTOEXEC.BAT", 25);
	assert_DOS_MakeName("...\\AUTOEXEC.BAT", true, "AUTOEXEC.BAT", 25);
	safe_strcpy(Drives.at(25)->curdir, "WINDOWS\\CONFIG\\FOLDER");
	assert_DOS_MakeName("...\\AUTOEXEC.BAT", true, "WINDOWS\\AUTOEXEC.BAT", 25);
	assert_DOS_MakeName("....\\AUTOEXEC.BAT", true, "AUTOEXEC.BAT", 25);
	safe_strcpy(Drives.at(25)->curdir, "WINDOWS\\CONFIG\\FOLDER\\DEEP");
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
	// `dos` comes from dos.h
	dos.errorcode = DOSERR_NONE;
	EXPECT_FALSE(DOS_FindFirst("Z:\\DARK\\LFD\\", FatAttributeFlags::Volume, false));
	EXPECT_EQ(dos.errorcode, DOSERR_NO_MORE_FILES);

	dos.errorcode = DOSERR_NONE;
	EXPECT_FALSE(DOS_FindFirst("Z:\\DARK\\", FatAttributeFlags::Volume, false));
	EXPECT_EQ(dos.errorcode, DOSERR_NO_MORE_FILES);

	// volume names alone don't trigger the failure
	dos.errorcode = DOSERR_NONE;
	EXPECT_TRUE(DOS_FindFirst("Z:\\", FatAttributeFlags::Volume, false));
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
	EXPECT_TRUE(DOS_FindFirst("Z", FatAttributeFlags::Volume, false));
	EXPECT_EQ(dos.errorcode, DOSERR_NONE);
}

TEST_F(DOS_FilesTest, DOS_FindFirst_FindDevice)
{
	dos.errorcode = DOSERR_NONE;
	EXPECT_TRUE(DOS_FindFirst("COM1", FatAttributeFlags::Device, false));
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

TEST_F(DOS_FilesTest, DOS_DTAExtendName_Space_Pads)
{
	assert_DTAExtendName("1234.E  ", "1234    ", "E  ");
}

TEST_F(DOS_FilesTest, DOS_DTAExtendName_Enforces_8_3)
{
	assert_DTAExtendName("12345678ABCDEF.123ABCDE", "12345678", "123");
}

TEST_F(DOS_FilesTest, VFILE_Register)
{
	VFILE_Register("TEST", nullptr, 0, "/");
	EXPECT_FALSE(DOS_FindFirst("Z:\\TEST\\FILENA~1.TXT", 0, false));
	VFILE_Register("filename_1.txt", nullptr, 0, "/TEST/");
	EXPECT_TRUE(DOS_FindFirst("Z:\\TEST\\FILENA~1.TXT", 0, false));
	EXPECT_FALSE(DOS_FindFirst("Z:\\TEST\\FILENA~2.TXT", 0, false));
	VFILE_Register("filename_2.txt", nullptr, 0, "/TEST/");
	EXPECT_TRUE(DOS_FindFirst("Z:\\TEST\\FILENA~2.TXT", 0, false));
	EXPECT_FALSE(DOS_FindFirst("Z:\\TEST\\FILENA~3.TXT", 0, false));
	VFILE_Register("filename_3.txt", nullptr, 0, "/TEST/");
	EXPECT_TRUE(DOS_FindFirst("Z:\\TEST\\FILENA~3.TXT", 0, false));
}

} // namespace
