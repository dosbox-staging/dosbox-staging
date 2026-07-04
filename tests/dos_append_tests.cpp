// SPDX-License-Identifier: GPL-2.0-or-later

#include "dos/dos_append.h"

#include <filesystem>
#include <algorithm>
#include <fstream>
#include <string>

#include <gtest/gtest.h>

#include "dosbox_test_fixture.h"

namespace {

class DOS_AppendTest : public DOSBoxTestFixture {};

TEST_F(DOS_AppendTest, ParseSinglePath)
{
	DOS_Append::SetPaths("C:\\GAMES");
	
	EXPECT_TRUE(DOS_Append::IsActive());
	
	const auto& paths = DOS_Append::GetPaths();
	ASSERT_EQ(paths.size(), 1);
	EXPECT_EQ(paths[0], "C:\\GAMES");
}

TEST_F(DOS_AppendTest, ParseMultiplePathsAndRetainTrailingSlash)
{
	DOS_Append::SetPaths("C:\\GAMES\\;D:\\DATA;E:\\");
	
	EXPECT_TRUE(DOS_Append::IsActive());
	
	const auto paths = DOS_Append::GetPaths();
	ASSERT_EQ(paths.size(), 3);
	EXPECT_EQ(paths[0], "C:\\GAMES\\");
	EXPECT_EQ(paths[1], "D:\\DATA");
	EXPECT_EQ(paths[2], "E:\\");
}

TEST_F(DOS_AppendTest, ParseEmptyPaths)
{
	DOS_Append::SetPaths(";;C:\\GAMES;;;");
	
	EXPECT_TRUE(DOS_Append::IsActive());
	
	const auto& paths = DOS_Append::GetPaths();
	ASSERT_EQ(paths.size(), 1);
	EXPECT_EQ(paths[0], "C:\\GAMES");
}

TEST_F(DOS_AppendTest, ClearPaths)
{
	DOS_Append::SetPaths("C:\\GAMES");
	EXPECT_TRUE(DOS_Append::IsActive());
	
	DOS_Append::SetPaths("");
	EXPECT_FALSE(DOS_Append::IsActive());
	EXPECT_TRUE(DOS_Append::GetPaths().empty());
}

TEST_F(DOS_AppendTest, ExecModeSwitch)
{
	DOS_Append::SetExecutableMode(true);
	EXPECT_TRUE(DOS_Append::IsExecutableMode());
	
	DOS_Append::SetExecutableMode(false);
	EXPECT_FALSE(DOS_Append::IsExecutableMode());
}

TEST_F(DOS_AppendTest, VFSIntegrationFallback)
{
	// 1. Create a dummy file on the host side
	std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "dosbox_append_test";
	std::filesystem::create_directories(temp_dir);
	std::filesystem::path dummy_file = temp_dir / "test.txt";
	
	std::ofstream ofs(dummy_file);
	ofs << "dummy content";
	ofs.close();

	// 2. Set the DOS_Append paths
	DOS_Append::SetPaths(temp_dir.string());
	
	std::string resolved_path = "";
	
	// 3. Simulate the VFS failure hook pattern (attempting to resolve "test.txt")
	bool found = DOS_Append::Resolve("test.txt", [&](const std::string& try_path) {
		// Convert DOS backslashes to host-native slashes for the physical file check
		std::string host_path = try_path;
		std::replace(host_path.begin(), host_path.end(), '\\', '/');
		
		// Mock the physical filesystem lookup that DOS_OpenFile / FindFirst performs
		if (std::filesystem::exists(host_path)) {
			resolved_path = try_path;
			return true;
		}
		return false;
	});

	// 4. Assert that the hook correctly found and verified the physical file
	EXPECT_TRUE(found);
	EXPECT_EQ(resolved_path, temp_dir.string() + "\\test.txt");

	// Cleanup
	std::filesystem::remove_all(temp_dir);
}

TEST_F(DOS_AppendTest, ResolveTemplateIteratesCorrectly)
{
	DOS_Append::SetPaths("C:\\DIR1;C:\\DIR2;C:\\DIR3");
	
	std::vector<std::string> checked_paths;
	
	bool found = DOS_Append::Resolve("TEST.TXT", [&](const std::string& try_path) {
		checked_paths.push_back(try_path);
		// Simulate finding the file only in DIR2
		return try_path == "C:\\DIR2\\TEST.TXT";
	});
	
	EXPECT_TRUE(found);
	ASSERT_EQ(checked_paths.size(), 2);
	EXPECT_EQ(checked_paths[0], "C:\\DIR1\\TEST.TXT");
	EXPECT_EQ(checked_paths[1], "C:\\DIR2\\TEST.TXT");
}

TEST_F(DOS_AppendTest, ResolveTemplateFailsWhenNotFound)
{
	DOS_Append::SetPaths("C:\\DIR1;C:\\DIR2");
	
	int checks = 0;
	bool found = DOS_Append::Resolve("TEST.TXT", [&](const std::string& try_path) {
		(void)try_path;
		checks++;
		return false; // File does not exist anywhere
	});
	
	EXPECT_FALSE(found);
	EXPECT_EQ(checks, 2);
}

TEST_F(DOS_AppendTest, ResolveTemplateFailsWhenEmpty)
{
	DOS_Append::SetPaths("");
	
	int checks = 0;
	bool found = DOS_Append::Resolve("TEST.TXT", [&](const std::string& try_path) {
		(void)try_path;
		checks++;
		return true;
	});
	
	EXPECT_FALSE(found);
	EXPECT_EQ(checks, 0); // Should not iterate at all
}

TEST_F(DOS_AppendTest, ResolveTemplateAbsolutePaths)
{
	// Appending works by blindly concatenating.
	// If the user appended an absolute path, DOS_MakeName handles the normalization.
	DOS_Append::SetPaths("C:\\GAMES");
	
	std::vector<std::string> checked_paths;
	bool found = DOS_Append::Resolve("DATA\\LEVEL1.DAT", [&](const std::string& try_path) {
		checked_paths.push_back(try_path);
		return true;
	});
	
	EXPECT_TRUE(found);
	ASSERT_EQ(checked_paths.size(), 1);
	EXPECT_EQ(checked_paths[0], "C:\\GAMES\\DATA\\LEVEL1.DAT");
}

} // namespace
