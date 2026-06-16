// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "utils/indexed_filenames.h"

#include <fstream>
#include <string>
#include <system_error>

#include <gtest/gtest.h>

#include "misc/std_filesystem.h"

namespace {

class IndexedFilenamesTest : public ::testing::Test {
protected:
	std_fs::path dir = {};

	void SetUp() override
	{
		const auto* test_info =
		        ::testing::UnitTest::GetInstance()->current_test_info();
		dir = std_fs::temp_directory_path() /
		      ("dosbox_indexed_filenames_test_" +
		       std::string(test_info->name()));
		std::error_code ec;
		std_fs::remove_all(dir, ec);
		std_fs::create_directories(dir, ec);
	}

	void TearDown() override
	{
		std::error_code ec;
		std_fs::remove_all(dir, ec);
	}

	void touch(const std::string& filename) const
	{
		std::ofstream{dir / filename}.put('x');
	}
};

TEST(IndexedFilenamesFormat, PadsToFourDigits)
{
	EXPECT_EQ(format_indexed_filename("doc", 1, ".ps"), "doc0001.ps");
	EXPECT_EQ(format_indexed_filename("doc", 42, ".ps"), "doc0042.ps");
	EXPECT_EQ(format_indexed_filename("doc", 9999, ".ps"), "doc9999.ps");
}

TEST(IndexedFilenamesFormat, PaddingIsMinimumNotMaximum)
{
	EXPECT_EQ(format_indexed_filename("x", 12345, ".y"), "x12345.y");
}

TEST(IndexedFilenamesFormat, SupportsRichSuffix)
{
	EXPECT_EQ(format_indexed_filename("image", 1, "-raw.png"),
	          "image0001-raw.png");
	EXPECT_EQ(format_indexed_filename("image", 99, "-rendered.png"),
	          "image0099-rendered.png");
}

TEST(IndexedFilenamesFormat, EmptyBasenameOrSuffix)
{
	EXPECT_EQ(format_indexed_filename("", 7, ".bin"), "0007.bin");
	EXPECT_EQ(format_indexed_filename("doc", 7, ""), "doc0007");
}

TEST_F(IndexedFilenamesTest, EmptyDirectoryReturnsZero)
{
	EXPECT_EQ(find_highest_file_index(dir, "doc", ".ps"), 0);
}

TEST_F(IndexedFilenamesTest, NonexistentDirectoryReturnsZero)
{
	const auto bogus = dir / "does_not_exist";
	EXPECT_EQ(find_highest_file_index(bogus, "doc", ".ps"), 0);
}

TEST_F(IndexedFilenamesTest, SingleMatch)
{
	touch("doc0007.ps");
	EXPECT_EQ(find_highest_file_index(dir, "doc", ".ps"), 7);
}

TEST_F(IndexedFilenamesTest, GapsInSequenceReturnHighest)
{
	touch("doc0001.ps");
	touch("doc0005.ps");
	touch("doc0012.ps");
	EXPECT_EQ(find_highest_file_index(dir, "doc", ".ps"), 12);
}

TEST_F(IndexedFilenamesTest, NonMatchingBasenameIgnored)
{
	touch("other0099.ps");
	touch("doc0003.ps");
	EXPECT_EQ(find_highest_file_index(dir, "doc", ".ps"), 3);
}

TEST_F(IndexedFilenamesTest, NonMatchingSuffixIgnored)
{
	touch("doc0099.txt");
	touch("doc0003.ps");
	EXPECT_EQ(find_highest_file_index(dir, "doc", ".ps"), 3);
}

TEST_F(IndexedFilenamesTest, CaseInsensitiveMatching)
{
	touch("DOC0042.PS");
	EXPECT_EQ(find_highest_file_index(dir, "doc", ".ps"), 42);
}

TEST_F(IndexedFilenamesTest, NonDigitMiddleIgnored)
{
	touch("doc0001-junk.ps");
	touch("doc0003.ps");
	EXPECT_EQ(find_highest_file_index(dir, "doc", ".ps"), 3);
}

TEST_F(IndexedFilenamesTest, EmptyMiddleIgnored)
{
	touch("doc.ps");
	EXPECT_EQ(find_highest_file_index(dir, "doc", ".ps"), 0);
}

TEST_F(IndexedFilenamesTest, UnpaddedDigitsStillMatch)
{
	touch("doc5.ps");
	touch("doc12.ps");
	EXPECT_EQ(find_highest_file_index(dir, "doc", ".ps"), 12);
}

TEST_F(IndexedFilenamesTest, RichSuffixMatching)
{
	touch("image0001-raw.png");
	touch("image0007-raw.png");
	touch("image0005-rendered.png");
	EXPECT_EQ(find_highest_file_index(dir, "image", "-raw.png"), 7);
	EXPECT_EQ(find_highest_file_index(dir, "image", "-rendered.png"), 5);
}

} // namespace
