/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2023-2023  The DOSBox Staging Team
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

#include <gtest/gtest.h>
#include <unordered_map>

#include "shell.h"

class MockReader final : public ByteReader {
public:
	void Reset() override
	{
		index = 0;
	}
	std::optional<char> Read() override
	{
		if (index >= contents.size()) {
			return std::nullopt;
		}

		const auto data = contents[index];
		++index;
		return data;
	}

	explicit MockReader(std::string&& str) : contents(std::move(str)) {}

	MockReader(const MockReader&)            = delete;
	MockReader& operator=(const MockReader&) = delete;
	MockReader(MockReader&&)                 = delete;
	MockReader& operator=(MockReader&&)      = delete;
	~MockReader() override                   = default;

private:
	std::string contents;
	decltype(contents)::size_type index = 0;
};

class MockShell final : public HostShell {
public:
	bool GetEnvStr(const char* entry, std::string& result) const override
	{
		if (env.find(entry) == std::end(env)) {
			return false;
		}
		result = std::string(entry) + '=' + env.at(entry);
		return true;
	}

	explicit MockShell(std::unordered_map<std::string, std::string>&& map)
	        : env(std::move(map))
	{}

	MockShell(const MockShell&)            = delete;
	MockShell& operator=(const MockShell&) = delete;
	MockShell(MockShell&&)                 = delete;
	MockShell& operator=(MockShell&&)      = delete;
	~MockShell() override                  = default;

private:
	std::unordered_map<std::string, std::string> env;
};

TEST(BatchFileRead, StopAtEndNoNewline)
{
	const auto shell = MockShell({});
	auto batchfile   = BatchFile(
                shell, std::make_unique<MockReader>("contents"), "", "", true);

	char line[CMD_MAXLINE];

	batchfile.ReadLine(line);
	ASSERT_STREQ(line, "contents");

	const auto read = batchfile.ReadLine(line);
	ASSERT_FALSE(read);
}

TEST(BatchFileRead, StopAtEndWithNewline)
{
	const auto shell = MockShell({});
	auto batchfile   = BatchFile(
                shell, std::make_unique<MockReader>("contents\n"), "", "", true);

	char line[CMD_MAXLINE];

	batchfile.ReadLine(line);
	ASSERT_STREQ(line, "contents");

	const auto read = batchfile.ReadLine(line);
	ASSERT_FALSE(read);
}

TEST(BatchFileParse, EmptySubstitutionsWhenNotFound)
{
	const auto shell = MockShell({});
	auto batchfile   = BatchFile(shell,
                                   std::make_unique<MockReader>("%0%1%NONEXISTENTVAR%%"),
                                   "",
                                   "",
                                   true);

	char line[CMD_MAXLINE];

	batchfile.ReadLine(line);
	ASSERT_STREQ(line, "");
}

TEST(BatchFileParse, SubstituteFilename)
{
	const auto shell = MockShell({});
	auto batchfile   = BatchFile(shell,
                                   std::make_unique<MockReader>("%0"),
                                   "filename.bat",
                                   "",
                                   true);

	char line[CMD_MAXLINE];

	batchfile.ReadLine(line);
	ASSERT_STREQ(line, "filename.bat");
}

TEST(BatchFileParse, SubstituteArgs)
{
	const auto shell = MockShell({});
	auto batchfile   = BatchFile(shell,
                                   std::make_unique<MockReader>("%1%2%3%4"),
                                   "",
                                   "one two three",
                                   true);

	char line[CMD_MAXLINE];

	batchfile.ReadLine(line);
	ASSERT_STREQ(line, "onetwothree");
}

TEST(BatchFileParse, SubstituteEnvironmentVariable)
{
	const auto shell = MockShell(std::unordered_map<std::string, std::string>(
	        {{"variable", "value"}}));
	auto batchfile = BatchFile(
	        shell, std::make_unique<MockReader>("%variable%"), "", "", true);

	char line[CMD_MAXLINE];

	batchfile.ReadLine(line);
	ASSERT_STREQ(line, "value");
}

TEST(BatchFileGoto, FindLabel)
{
	const auto shell = MockShell({});
	auto batchfile   = BatchFile(
                shell, std::make_unique<MockReader>(":label"), "", "", true);

	const auto found_label = batchfile.Goto("label");
	ASSERT_TRUE(found_label);
}

TEST(BatchFileGoto, LabelNotFound)
{
	const auto shell = MockShell({});
	auto batchfile   = BatchFile(
                shell, std::make_unique<MockReader>(":label"), "", "", true);

	const auto found_label = batchfile.Goto("nolabel");
	ASSERT_FALSE(found_label);
}

TEST(BatchFileGoto, LabelOnPreviousLine)
{
	const auto shell = MockShell({});
	auto batchfile   = BatchFile(
                shell, std::make_unique<MockReader>(":label\nline"), "", "", true);
	char line[CMD_MAXLINE];

	batchfile.ReadLine(line);
	ASSERT_STREQ(line, "line");

	const auto found_label = batchfile.Goto("label");
	ASSERT_TRUE(found_label);
}

TEST(BatchFileGoto, SkipLines)
{
	const auto shell = MockShell({});
	auto batchfile   = BatchFile(shell,
                                   std::make_unique<MockReader>("before\n:label\nafter"),
                                   "",
                                   "",
                                   true);
	char line[CMD_MAXLINE];

	const auto found_label = batchfile.Goto("label");
	ASSERT_TRUE(found_label);

	batchfile.ReadLine(line);
	ASSERT_STREQ(line, "after");
}
