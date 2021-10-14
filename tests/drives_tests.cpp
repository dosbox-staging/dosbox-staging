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

#include "drives.h"

#include <gtest/gtest.h>

#include <string>

// Open anonymous namespace (this is Google Test requirement)

namespace {

TEST(WildFileCmp, ExactMatch)
{
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

TEST(Set_Label, Daggerfall)
{
    std::string input = "Daggerfall";
    char output[9] = { 0 };
    bool cdrom = false;
    std::cout << "CD-ROM? " << cdrom << " Input: " << input << " Output: " << output << '\n';
    Set_Label(input.c_str(), output, cdrom);
    EXPECT_EQ(0, std::string("DAGGERFA.LL").compare(output));
}
TEST(Set_Label, DaggerfallCD)
{
    std::string input = "Daggerfall";
    char output[9] = { 0 };
    bool cdrom = true;
    Set_Label(input.c_str(), output, cdrom);
    std::cout << "CD-ROM? " << cdrom << " Input: " << input << " Output: " << output << '\n';
    EXPECT_EQ(0, std::string("Daggerfa.ll").compare(output));
}

TEST(Set_Label, LongerThan11)
{
    std::string input = "a123456789AAA";
    char output[9] = { 0 };
    bool cdrom = false;
    Set_Label(input.c_str(), output, cdrom);
    std::cout << "CD-ROM? " << cdrom << " Input: " << input << " Output: " << output << '\n';
    EXPECT_EQ(0, std::string("A1234567.89A").compare(output));
}
TEST(Set_Label, LongerThan11CD)
{
    std::string input = "a123456789AAA";
    char output[9] = { 0 };
    bool cdrom = true;
    Set_Label(input.c_str(), output, cdrom);
    std::cout << "CD-ROM? " << cdrom << " Input: " << input << " Output: " << output << '\n';
    EXPECT_EQ(0, std::string("a1234567.89A").compare(output));
}

TEST(Set_Label, ShorterThan8)
{
    std::string input = "a123456";
    char output[9] = { 0 };
    bool cdrom = false;
    Set_Label(input.c_str(), output, cdrom);
    std::cout << "CD-ROM? " << cdrom << " Input: " << input << " Output: " << output << '\n';
    EXPECT_EQ(0, std::string("A123456").compare(output));
}
TEST(Set_Label, ShorterThan8CD)
{
    std::string input = "a123456";
    char output[9] = { 0 };
    bool cdrom = true;
    Set_Label(input.c_str(), output, cdrom);
    std::cout << "CD-ROM? " << cdrom << " Input: " << input << " Output: " << output << '\n';
    EXPECT_EQ(0, std::string("a123456").compare(output));
}

TEST(Set_Label, EqualTo8)
{
    std::string input = "a1234567";
    char output[9] = { 0 };
    bool cdrom = false;
    Set_Label(input.c_str(), output, cdrom);
    std::cout << "CD-ROM? " << cdrom << " Input: " << input << " Output: " << output << '\n';
    EXPECT_EQ(0, std::string("A1234567").compare(output));
}
TEST(Set_Label, EqualTo8CD)
{
    std::string input = "a1234567";
    char output[9] = { 0 };
    bool cdrom = true;
    Set_Label(input.c_str(), output, cdrom);
    std::cout << "CD-ROM? " << cdrom << " Input: " << input << " Output: " << output << '\n';
    EXPECT_EQ(0, std::string("a1234567.").compare(output));
}

} // namespace
