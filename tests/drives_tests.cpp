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

#include "dos/drives.h"

#include <gtest/gtest.h>

#include <string>

std::string run_Set_Label(char const * const input, bool cdrom) {
    char output[32] = { 0 };
    Set_Label(input, output, cdrom);
    std::cout << "Set_Label " << "CD-ROM? " << (cdrom ? 'y' : 'n') << \
        " Input: " << input << " Output: " << output << '\n';
    return std::string(output);
}

// Open anonymous namespace (this is Google Test requirement)

namespace {

TEST(WildFileCmp, wild_match)
{
    EXPECT_EQ(true, wild_match("TEST", "*"));
    EXPECT_EQ(true, wild_match("TEST", "T*"));
    EXPECT_EQ(true, wild_match("TEST", "T*T"));
    EXPECT_EQ(true, wild_match("TEST", "TES?"));
    EXPECT_EQ(false, wild_match("TEST LONG NAME", "TEST*long*"));
    EXPECT_EQ(false, wild_match("TEST LONG NAME", "*NONE*"));
    EXPECT_EQ(true, wild_match("TEST LONG LONG NAME", "*LONG?NAME"));
    EXPECT_EQ(true, wild_match("TEST LONG LONG NAME", "*LONG*LONG*"));
    EXPECT_EQ(false, wild_match("TEST LONG LONG NAME", "*LONGLONG*"));
    EXPECT_EQ(false, wild_match("TEST", "Z*"));
}

TEST(WildFileCmp, ExactMatch)
{
    EXPECT_EQ(true, WildFileCmp("", ""));
    EXPECT_EQ(true, WildFileCmp("TEST.EXE", "TEST.EXE"));
    EXPECT_EQ(true, WildFileCmp("TEST", "TEST"));
    EXPECT_EQ(false, WildFileCmp("TEST.EXE", ".EXE"));
    EXPECT_EQ(true, WildFileCmp(".EXE", ".EXE"));
}

TEST(WildFileCmp, WildDotWild)
{
    EXPECT_EQ(true, WildFileCmp("TEST.EXE", "*.*"));
    EXPECT_EQ(true, WildFileCmp("TEST", "*.*"));
    EXPECT_EQ(true, WildFileCmp(".EXE", "*.*"));
}

TEST(WildFileCmp, WildcardNoExt)
{
    EXPECT_EQ(false, WildFileCmp("TEST.EXE", "*"));
    EXPECT_EQ(false, WildFileCmp(".EXE", "*"));
    EXPECT_EQ(true, WildFileCmp("TEST", "*"));
    EXPECT_EQ(true, WildFileCmp("TEST", "T*"));
    EXPECT_EQ(true, WildFileCmp("TEST", "*Y*"));
    EXPECT_EQ(false, WildFileCmp("TEST", "Z*"));
}

TEST(WildFileCmp, QuestionMark)
{
    EXPECT_EQ(true, WildFileCmp("TEST.EXE", "?EST.EXE"));
    EXPECT_EQ(true, WildFileCmp("TEST", "?EST"));
    EXPECT_EQ(false, WildFileCmp("TEST", "???Z"));
    EXPECT_EQ(true, WildFileCmp("TEST.EXE", "TEST.???"));
    EXPECT_EQ(true, WildFileCmp("TEST.EXE", "TEST.?XE"));
    EXPECT_EQ(true, WildFileCmp("TEST.EXE", "???T.EXE"));
    EXPECT_EQ(true, WildFileCmp("TEST", "???T.???"));
}

TEST(WildFileCmp, LongCompare)
{
    EXPECT_EQ(false, WildFileCmp("TEST", "", true));
    EXPECT_EQ(true, WildFileCmp("TEST.EXE", "*", true));
    EXPECT_EQ(true, WildFileCmp("TEST", "?EST", true));
    EXPECT_EQ(false, WildFileCmp("TEST", "???Z", true));
    EXPECT_EQ(true, WildFileCmp("TEST.EXE", "T*T.*", true));
    EXPECT_EQ(true, WildFileCmp("TEST.EXE", "T*T.?X?", true));
    EXPECT_EQ(true, WildFileCmp("TEST.EXE", "T??T.E*E", true));
    EXPECT_EQ(true, WildFileCmp("Test.exe", "*ST.E*", true));
    EXPECT_EQ(true, WildFileCmp("Test long name", "*NAME", true));
    EXPECT_EQ(true, WildFileCmp("Test long name", "*T*L*M*", true));
    EXPECT_EQ(true, WildFileCmp("Test long name.txt", "T*long*.T??", true));
    EXPECT_EQ(true, WildFileCmp("Test long name.txt", "??st*name.*t", true));
    EXPECT_EQ(true, WildFileCmp("Test long name.txt", "Test?long?????.*t", true));
    EXPECT_EQ(true, WildFileCmp("Test long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long.txt", "Test*long.???", true));
    EXPECT_EQ(true, WildFileCmp("Test long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long.txt", "Test long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long.txt", true));
    EXPECT_EQ(false, WildFileCmp("Test long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long.txt", "Test long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long long.txt", true));
    EXPECT_EQ(false, WildFileCmp("TEST", "Z*", true));
    EXPECT_EQ(false, WildFileCmp("TEST FILE NAME", "*Y*", true));
    EXPECT_EQ(false, WildFileCmp("TEST FILE NAME", "*F*X*", true));
}

TEST(generate_8x3, SFNTest)
{
	EXPECT_EQ("TESTLO~1", generate_8x3("test long name...", 1, true));
	EXPECT_EQ("TESTLO~2.TXT", generate_8x3("test long name.txt", 2, true));
	EXPECT_EQ("TESTL~20.TEX", generate_8x3("test long name.txt.text", 20, true));
	EXPECT_EQ("TEST__~2.TEX", generate_8x3("test[ ]long name.text", 2, true));
	EXPECT_EQ("TEST_~20", generate_8x3("... test[ ]long name ...", 20, true));
	EXPECT_EQ("TEST~200.TT", generate_8x3("test long name.txt.tt", 200, true));
	EXPECT_EQ("TES~2000.TXT", generate_8x3("test[]long name.txt", 2000, true));
	EXPECT_EQ("TE~20000.EXT", generate_8x3("test long long name.txt..ext..", 20000, true));
	EXPECT_EQ("T~200000.TXT", generate_8x3("test long long name.txt", 200000, true));
	EXPECT_EQ("", generate_8x3("test long long name.txt", 2000000, true));
}

TEST(filename_not_8x3, NameTest)
{
	EXPECT_FALSE(filename_not_8x3("testfile.txt"));
	EXPECT_TRUE(filename_not_8x3("test_file.txt"));
	EXPECT_FALSE(filename_not_8x3("myfile.t"));
	EXPECT_TRUE(filename_not_8x3("my file.txt"));
	EXPECT_TRUE(filename_not_8x3("my+file.txt"));
	EXPECT_TRUE(filename_not_8x3("myfile.text"));
	EXPECT_TRUE(filename_not_8x3("myfile..txt"));
}

/**
 * Set_Labels tests. These test the conversion of a FAT/CD-ROM volume
 * label to an MS-DOS 8.3 label with a variety of edge cases & oddities.
 */
TEST(Set_Label, Daggerfall)
{
    std::string output = run_Set_Label("Daggerfall", false);
    EXPECT_EQ("DAGGERFA.LL", output);
}
TEST(Set_Label, DaggerfallCD)
{
    std::string output = run_Set_Label("Daggerfall", true);
    EXPECT_EQ("Daggerfa.ll", output);
}

TEST(Set_Label, LongerThan11)
{
    std::string output = run_Set_Label("a123456789AAA", false);
    EXPECT_EQ("A1234567.89A", output);
}
TEST(Set_Label, LongerThan11CD)
{
    std::string output = run_Set_Label("a123456789AAA", true);
    EXPECT_EQ("a1234567.89A", output);
}

TEST(Set_Label, ShorterThan8)
{
    std::string output = run_Set_Label("a123456", false);
    EXPECT_EQ("A123456", output);
}
TEST(Set_Label, ShorterThan8CD)
{
    std::string output = run_Set_Label("a123456", true);
    EXPECT_EQ("a123456", output);
}

// Tests that the CD-ROM version adds a trailing dot when
// input is 8 chars plus one dot (9 chars total)
TEST(Set_Label, EqualTo8)
{
    std::string output = run_Set_Label("a1234567", false);
    EXPECT_EQ("A1234567", output);
}
TEST(Set_Label, EqualTo8CD)
{
    std::string output = run_Set_Label("a1234567", true);
    EXPECT_EQ("a1234567.", output);
}

// A test to ensure non-CD-ROM function strips trailing dot
TEST(Set_Label, StripEndingDot)
{
    std::string output = run_Set_Label("a1234567.", false);
    EXPECT_EQ("A1234567", output);
}
TEST(Set_Label, NoStripEndingDotCD)
{
    std::string output = run_Set_Label("a1234567.", true);
    EXPECT_EQ("a1234567.", output);
}

// Just to make sure this function doesn't clean invalid DOS labels
TEST(Set_Label, InvalidCharsEndingDot)
{
    std::string output = run_Set_Label("?*':&@(..", false);
    EXPECT_EQ("?*':&@(.", output);
}
TEST(Set_Label, InvalidCharsEndingDotCD)
{
    std::string output = run_Set_Label("?*':&@(..", true);
    EXPECT_EQ("?*':&@(..", output);
}

} // namespace
