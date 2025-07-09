// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "fs_utils.h"

#include <gtest/gtest.h>

#include <cerrno>
#include <string>

namespace {

TEST(PathExists, DirExists)
{
	EXPECT_TRUE(path_exists("tests"));
}

TEST(PathExists, FileExists)
{
	EXPECT_TRUE(path_exists("tests/fs_utils_tests.cpp"));
}

TEST(PathExists, MissingPath)
{
	EXPECT_FALSE(path_exists("foobar"));
}

TEST(PathExists, ExistingPathAsString)
{
	const std::string path = "tests/fs_utils_tests.cpp";
	EXPECT_TRUE(path_exists(path));
}

TEST(PathExists, MissingPathAsString)
{
	const std::string path = "barbaz";
	EXPECT_FALSE(path_exists("foobar"));
}

TEST(PathConversion, SimpleTest)
{
	constexpr auto expected_result = "tests/files/paths/empty.txt";
	constexpr auto input = "tests\\files\\PATHS\\EMPTY.TXT";
	ASSERT_TRUE(path_exists(expected_result));
	EXPECT_TRUE(path_exists(to_native_path(input)));
#if !defined(WIN32)
#if defined(MACOSX)
    // macOS file systems are case-insensitive but case-preserving
    EXPECT_NE(expected_result, to_native_path(input));
#else
	EXPECT_EQ(expected_result, to_native_path(input));
#endif // MACOSX
#endif // WIN32
}

TEST(PathConversion, MissingFile)
{
	constexpr auto nonexistent_file = "tests/files/paths/missing.txt";
	ASSERT_FALSE(path_exists(nonexistent_file));
	EXPECT_FALSE(path_exists(to_native_path(nonexistent_file)));
}

constexpr char TEST_DIR[] = "tests/files/no_path";

struct CreateDirTest : public testing::Test {
	~CreateDirTest() {
		if (path_exists(TEST_DIR))
			remove_dir(TEST_DIR);
	}
};

TEST(SimplifyPath, Nominal)
{
	const std_fs::path original = "tests/files/paths";
	const auto simplified       = simplify_path(original);
	EXPECT_EQ(original, simplified);
}

TEST(SimplifyPath, CanBeSimplifiedEasy)
{
	const std_fs::path original = "tests/files/paths/../../";
	const auto simplified       = simplify_path(original);
	EXPECT_TRUE(simplified.string().length() < original.string().length());
}

TEST(SimplifyPath, CanBeSimplifiedComplex)
{
	const std_fs::path original = "./tests123/../valid/tests456///1/..//2/../3/../..";
	const std_fs::path expected = "valid/";
	EXPECT_EQ(simplify_path(original), expected);
}

TEST_F(CreateDirTest, CreateDir)
{
	ASSERT_FALSE(path_exists(TEST_DIR));
	EXPECT_EQ(create_dir(TEST_DIR, 0700), 0);
	EXPECT_TRUE(path_exists(TEST_DIR));
	EXPECT_EQ(create_dir(TEST_DIR, 0700), -1);
	EXPECT_EQ(errno, EEXIST);
}

TEST_F(CreateDirTest, CreateDirWithoutFail)
{
	ASSERT_FALSE(path_exists(TEST_DIR));
	EXPECT_EQ(create_dir(TEST_DIR, 0700, OK_IF_EXISTS), 0);
	EXPECT_TRUE(path_exists(TEST_DIR));
	EXPECT_EQ(create_dir(TEST_DIR, 0700, OK_IF_EXISTS), 0);
}

TEST_F(CreateDirTest, FailDueToFileExisting)
{
	constexpr char path[] = "tests/files/paths/empty.txt";
	ASSERT_TRUE(path_exists(path));
	EXPECT_EQ(create_dir(path, 0700), -1);
	EXPECT_EQ(errno, EEXIST);
}

TEST_F(CreateDirTest, FailDueToFileExisting2)
{
	constexpr char path[] = "tests/files/paths/empty.txt";
	ASSERT_TRUE(path_exists(path));
	EXPECT_EQ(create_dir(path, 0700, OK_IF_EXISTS), -1);
	EXPECT_EQ(errno, EEXIST);
}

} // namespace
