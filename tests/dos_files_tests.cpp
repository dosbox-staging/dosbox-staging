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

// Helpers for the device-reservation tests. We don't pin a specific
// device index because the index depends on registration order;
// instead we just check "is some device" or "same device".
// SCOPED_TRACE adds the failing name to gtest output without iostreams
// (the project forbids them); format_str is the project's printf-style
// formatter from utils/string_utils.h.
void assert_DOS_FindDevice_matches(const char* const name)
{
	SCOPED_TRACE(format_str("name: %s", name));
	EXPECT_NE(DOS_FindDevice(name), DOS_DEVICES);
}

void assert_DOS_FindDevice_no_match(const char* const name)
{
	SCOPED_TRACE(format_str("name: %s", name));
	EXPECT_EQ(DOS_FindDevice(name), DOS_DEVICES);
}

void assert_DOS_FindDevice_same(const char* const a, const char* const b)
{
	SCOPED_TRACE(format_str("a: %s  b: %s", a, b));
	const auto idx_a = DOS_FindDevice(a);
	const auto idx_b = DOS_FindDevice(b);
	EXPECT_NE(idx_a, DOS_DEVICES);
	EXPECT_EQ(idx_a, idx_b);
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

// ---------------------------------------------------------------------------
// DOS name semantics — the three name shapes
//
// DOS accepts filenames, drive specs ("A:"), and device names ("LPT1",
// "LPT1:"). The optional trailing colon on device names is the same
// punctuation that follows a drive letter; real MS-DOS treats
// `LPT1` and `LPT1:` as identical references to the LPT1 device.
//
// The tests below capture how each shape is parsed, including the
// less-obvious cases (single-letter filenames vs drive letters,
// device-name reservation across all paths, what's still illegal).
// See docs/dos-name-handling.md for the full narrative.
// ---------------------------------------------------------------------------

// Group 1 — plain filenames: regression guard for the normal-path
// flow. These should pass on current code and stay passing after the
// trailing-colon fix.
TEST_F(DOS_FilesTest, DOS_MakeName_PlainFilenames)
{
	safe_strcpy(Drives.at(25)->curdir, "");
	assert_DOS_MakeName("AUTOEXEC.BAT", true, "AUTOEXEC.BAT", 25);
	assert_DOS_MakeName("autoexec.bat", true, "AUTOEXEC.BAT", 25);
	assert_DOS_MakeName("subdir\\foo.txt", true, "SUBDIR\\FOO.TXT", 25);
	assert_DOS_MakeName("Z:\\AUTOEXEC.BAT", true, "AUTOEXEC.BAT", 25);
	assert_DOS_MakeName("foo", true, "FOO", 25);
	// Single-letter name WITHOUT a colon is a filename, not a drive
	// ref. The drive-prefix branch fires only when name[1] == ':'.
	assert_DOS_MakeName("a", true, "A", 25);
}

// Group 2 — drive specs: regression guard. The 2-char drive prefix
// must keep working before and after the trailing-colon fix. Drive
// specs are consumed at the very top of DOS_MakeName (name_int += 2);
// the trailing-colon-strip we add later operates on the remaining
// path component, never on the drive prefix.
//
// Only Z: is mounted in the test fixture, so we only verify mounted
// drive specs. The unmounted-drive failure path is already covered
// by DOS_MakeName_Basic_Failures.
TEST_F(DOS_FilesTest, DOS_MakeName_DriveSpecs)
{
	safe_strcpy(Drives.at(25)->curdir, "");
	assert_DOS_MakeName("Z:", true, "", 25);
	assert_DOS_MakeName("Z:foo.txt", true, "FOO.TXT", 25);
	assert_DOS_MakeName("z:foo.txt", true, "FOO.TXT", 25);
}

// Group 3 — trailing colon on device names. THIS IS THE BUG.
// On current code these all FAIL because the per-char validation
// loop in DOS_MakeName rejects ':'. After the fix they parse to the
// same fullname as the no-colon form.
TEST_F(DOS_FilesTest, DOS_MakeName_DeviceNamesTrailingColon)
{
	safe_strcpy(Drives.at(25)->curdir, "");
	assert_DOS_MakeName("LPT1:", true, "LPT1", 25);
	assert_DOS_MakeName("CON:", true, "CON", 25);
	assert_DOS_MakeName("PRN:", true, "PRN", 25);
	assert_DOS_MakeName("NUL:", true, "NUL", 25);
	assert_DOS_MakeName("AUX:", true, "AUX", 25);
	assert_DOS_MakeName("COM1:", true, "COM1", 25);
	// Case-normalised the same way as plain filenames.
	assert_DOS_MakeName("lpt1:", true, "LPT1", 25);
	assert_DOS_MakeName("Con:", true, "CON", 25);
}

// Group 4 — drive prefix combined with a device name that also has
// the trailing colon. The drive prefix is consumed first; the
// trailing colon strip handles the rest.
TEST_F(DOS_FilesTest, DOS_MakeName_DriveAndDeviceTrailingColon)
{
	safe_strcpy(Drives.at(25)->curdir, "");
	assert_DOS_MakeName("Z:LPT1:", true, "LPT1", 25);
	assert_DOS_MakeName("Z:CON:", true, "CON", 25);
	assert_DOS_MakeName("z:lpt1:", true, "LPT1", 25);
}

// Group 5 — device-name resolution and the trailing-colon equivalence.
//
// Note on staging's path-prefix behaviour: DOS_FindDevice calls
// `Drives[drive]->TestDir(...)` on the parent directory before
// matching against the device table, so a path-prefixed device name
// like `subdir\LPT1` only resolves to the device if `subdir` actually
// exists on that drive. This is stricter than real MS-DOS (which
// always shadows devices) and is an existing staging behaviour
// independent of our trailing-colon fix. We don't test the
// path-prefixed reservation here to avoid coupling this test to the
// test fixture's directory layout.
//
// What we DO test: the basic device-name match works, extension is
// stripped, and after the trailing-colon fix the colon form
// resolves to the SAME device as the no-colon form.
TEST_F(DOS_FilesTest, DOS_FindDevice_TrailingColonEquivalence)
{
	safe_strcpy(Drives.at(25)->curdir, "");

	// The basics — these pass today.
	assert_DOS_FindDevice_matches("LPT1");
	assert_DOS_FindDevice_matches("CON");
	assert_DOS_FindDevice_matches("NUL");
	// Extensions are stripped by DOS_FindDevice before matching.
	assert_DOS_FindDevice_matches("LPT1.txt");
	assert_DOS_FindDevice_matches("CON.dat");

	// Trailing-colon forms (THE BUG): these fail today, pass after fix.
	assert_DOS_FindDevice_matches("LPT1:");
	assert_DOS_FindDevice_matches("CON:");
	assert_DOS_FindDevice_matches("NUL:");

	// And the trailing-colon form resolves to the same device as the
	// no-colon form.
	assert_DOS_FindDevice_same("LPT1", "LPT1:");
	assert_DOS_FindDevice_same("CON", "CON:");
	assert_DOS_FindDevice_same("NUL", "NUL:");
}

// Group 6 — single letters are NOT device names. The drive-prefix
// branch fires only when name[1] == ':'; without the colon, a
// single-letter input is just a one-char filename.
TEST_F(DOS_FilesTest, DOS_FindDevice_SingleLettersNotReserved)
{
	safe_strcpy(Drives.at(25)->curdir, "");
	assert_DOS_FindDevice_no_match("A");
	assert_DOS_FindDevice_no_match("Z");
	assert_DOS_FindDevice_no_match("X");
}

// Group 7 — what's still illegal after the fix. We strip only a
// FINAL colon; internal ones still hit the per-char validation loop
// and fail. Multiple trailing colons strip one and the second still
// fails. This is the guardrail keeping us from accepting genuinely
// malformed names.
TEST_F(DOS_FilesTest, DOS_MakeName_StillIllegal_InternalColons)
{
	safe_strcpy(Drives.at(25)->curdir, "");
	// Internal colon: not a drive prefix (drive prefix is 2 chars),
	// not a trailing device colon either.
	assert_DOS_MakeName("foo:bar", false);
	assert_DOS_MakeName("Z:\\foo:bar", false);
	// Double trailing colon: we strip one, the other fails.
	assert_DOS_MakeName("LPT1::", false);
	assert_DOS_MakeName("foo::", false);
	// Colon-as-path-separator (e.g. "LPT1:\\foo") — the colon is
	// internal once you account for the path continuation.
	assert_DOS_MakeName("LPT1:\\foo", false);
}

// Group 8 — the documented lenience. A trailing colon on a non-device
// name silently becomes the same name without the colon. This matches
// MS-DOS's permissive parser and what shell.cpp's redirect strip
// already does for `> foo:` redirections. We bake the choice in so
// it's intentional rather than an accident.
//
// Caveat: an unmounted-drive-letter input like "X:" (with X not in
// Drives[]) currently errors at the drive-prefix branch. After the
// fix it bypasses that branch (because name[1]==':' but Drives[X]
// is null), then strips the colon, and becomes the filename "X" on
// the default drive. Real-world impact: nil; nobody types unmounted
// drive letters intentionally.
TEST_F(DOS_FilesTest, DOS_MakeName_TrailingColonLenience)
{
	safe_strcpy(Drives.at(25)->curdir, "");
	// Non-device name with trailing colon: strips to the bare name.
	assert_DOS_MakeName("foo:", true, "FOO", 25);
	assert_DOS_MakeName("HELLO:", true, "HELLO", 25);
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

// Use shared_ptr instead of unique_ptr as the time cache depends on
// std::enable_shared_from_this which will matter for certains tests
static std::shared_ptr<localDrive> create_local_drive(const char* path)
{
	// I was not able to actually run MOUNT.COM inside the test enviornment.
	// These are the default parameters as set by the mount command.
	// They were retrieved from a debugger by setting a breakpoint inside the constructor.
	constexpr uint16_t BytesSector = 512;
	constexpr uint8_t SectorsCluster = 32;
	constexpr uint16_t TotalClusters = 32765;
	constexpr uint16_t FreeClusters = 16000;
	constexpr uint8_t MediaId = 248;
	constexpr bool ReadOnly = false;
	constexpr bool AlwaysOpenRoFiles = true;

	return std::make_shared<localDrive>(
		path,
		BytesSector,
		SectorsCluster,
		TotalClusters,
		FreeClusters,
		MediaId,
		ReadOnly,
		AlwaysOpenRoFiles
	);
}

TEST_F(DOS_FilesTest, SetDate_LocalDrive)
{
	const auto temp_handle = create_native_file("tests/files/paths/date.txt", {});
	ASSERT_NE(temp_handle, InvalidNativeFileHandle);
	close_native_file(temp_handle);

	auto local_drive = create_local_drive("tests/files/paths/");

	// Open read-only and test that we can still set the date.
	auto local_file = local_drive->FileOpen("date.txt", OPEN_READ);
	ASSERT_NE(local_file, nullptr);

	const auto time = DOS_PackTime(4, 20, 0);
	const auto date = DOS_PackDate(1995, 1, 1);

	// This simulates DOS_SetFileDate()
	// We can't actually call that function inside the test enviornment
	// because it relies on PSP enviornment variables.
	local_file->time = time;
	local_file->date = date;
	local_file->flush_time_on_close = FlushTimeOnClose::ManuallySet;

	local_file->Close();
	local_file.reset();
	local_drive.reset();

	// On close, the new time/date should be written out to the host filesystem.
	const auto native_handle = open_native_file("tests/files/paths/date.txt", false);
	ASSERT_NE(native_handle, InvalidNativeFileHandle);
	const auto date_time = get_dos_file_time(native_handle);
	ASSERT_EQ(date_time.date, date);
	ASSERT_EQ(date_time.time, time);
	ASSERT_TRUE(delete_native_file("tests/files/paths/date.txt"));
}

} // namespace
