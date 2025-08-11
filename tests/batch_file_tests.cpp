// SPDX-FileCopyrightText:  2023-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <unordered_map>

#include "shell/shell.h"
#include "utils/string_utils.h"

class FakeReader final : public LineReader {
public:
	void Reset() override
	{
		index = 0;
	}
	std::optional<std::string> Read() override
	{
		if (index >= contents.size()) {
			return {};
		}

		const auto data = contents[index];
		++index;
		return data;
	}

	explicit FakeReader(std::string&& str) : contents(split(std::move(str))) {}

	FakeReader(const FakeReader&)            = delete;
	FakeReader& operator=(const FakeReader&) = delete;
	FakeReader(FakeReader&&)                 = delete;
	FakeReader& operator=(FakeReader&&)      = delete;
	~FakeReader() override                   = default;

private:
	std::vector<std::string> contents;
	decltype(contents)::size_type index = 0;
};

class FakeShell final : public Environment {
public:
	std::optional<std::string> GetEnvironmentValue(std::string_view entry) const override
	{
		auto environment_variable = env.find(std::string(entry));
		if (environment_variable == std::end(env)) {
			return {};
		}
		return environment_variable->second;
	}

	explicit FakeShell(std::unordered_map<std::string, std::string>&& map)
	        : env(std::move(map))
	{}

	FakeShell(const FakeShell&)            = delete;
	FakeShell& operator=(const FakeShell&) = delete;
	FakeShell(FakeShell&&)                 = delete;
	FakeShell& operator=(FakeShell&&)      = delete;
	~FakeShell() override                  = default;

private:
	std::unordered_map<std::string, std::string> env;
};

TEST(BatchFileRead, StopAtEndNoNewline)
{
	const auto shell = FakeShell({});
	auto batchfile   = BatchFile(
                shell, std::make_unique<FakeReader>("contents"), "", "", true);

	char line[CMD_MAXLINE];

	batchfile.ReadLine(line);
	ASSERT_STREQ(line, "contents");

	const auto read = batchfile.ReadLine(line);
	ASSERT_FALSE(read);
}

TEST(BatchFileRead, StopAtEndWithNewline)
{
	const auto shell = FakeShell({});
	auto batchfile   = BatchFile(
                shell, std::make_unique<FakeReader>("contents\n"), "", "", true);

	char line[CMD_MAXLINE];

	batchfile.ReadLine(line);
	ASSERT_STREQ(line, "contents");

	const auto read = batchfile.ReadLine(line);
	ASSERT_FALSE(read);
}

TEST(BatchFileParse, EmptySubstitutionsWhenNotFound)
{
	const auto shell = FakeShell({});
	auto batchfile   = BatchFile(shell,
                                   std::make_unique<FakeReader>("%0%1%NONEXISTENTVAR%%"),
                                   "",
                                   "",
                                   true);

	char line[CMD_MAXLINE];

	batchfile.ReadLine(line);
	ASSERT_STREQ(line, "");
}

TEST(BatchFileParse, SubstituteFilename)
{
	const auto shell = FakeShell({});
	auto batchfile   = BatchFile(shell,
                                   std::make_unique<FakeReader>("%0"),
                                   "filename.bat",
                                   "",
                                   true);

	char line[CMD_MAXLINE];

	batchfile.ReadLine(line);
	ASSERT_STREQ(line, "filename.bat");
}

TEST(BatchFileParse, SubstituteArgs)
{
	const auto shell = FakeShell({});
	auto batchfile   = BatchFile(shell,
                                   std::make_unique<FakeReader>("%1%2%3%4"),
                                   "",
                                   "one two three",
                                   true);

	char line[CMD_MAXLINE];

	batchfile.ReadLine(line);
	ASSERT_STREQ(line, "onetwothree");
}

TEST(BatchFileParse, SubstituteEnvironmentVariable)
{
	const auto shell = FakeShell(std::unordered_map<std::string, std::string>(
	        {{"variable", "value"}}));
	auto batchfile = BatchFile(
	        shell, std::make_unique<FakeReader>("%variable%"), "", "", true);

	char line[CMD_MAXLINE];

	batchfile.ReadLine(line);
	ASSERT_STREQ(line, "value");
}

TEST(BatchFileGoto, FindLabel)
{
	const auto shell = FakeShell({});
	auto batchfile   = BatchFile(
                shell, std::make_unique<FakeReader>(":label"), "", "", true);

	const auto found_label = batchfile.Goto("label");
	ASSERT_TRUE(found_label);
}

TEST(BatchFileGoto, LabelNotFound)
{
	const auto shell = FakeShell({});
	auto batchfile   = BatchFile(
                shell, std::make_unique<FakeReader>(":label"), "", "", true);

	const auto found_label = batchfile.Goto("nolabel");
	ASSERT_FALSE(found_label);
}

TEST(BatchFileGoto, LabelOnPreviousLine)
{
	const auto shell = FakeShell({});
	auto batchfile   = BatchFile(
                shell, std::make_unique<FakeReader>(":label\nline"), "", "", true);
	char line[CMD_MAXLINE];

	batchfile.ReadLine(line);
	ASSERT_STREQ(line, "line");

	const auto found_label = batchfile.Goto("label");
	ASSERT_TRUE(found_label);
}

TEST(BatchFileGoto, SkipLines)
{
	const auto shell = FakeShell({});
	auto batchfile   = BatchFile(shell,
                                   std::make_unique<FakeReader>("before\n:label\nafter"),
                                   "",
                                   "",
                                   true);
	char line[CMD_MAXLINE];

	const auto found_label = batchfile.Goto("label");
	ASSERT_TRUE(found_label);

	batchfile.ReadLine(line);
	ASSERT_STREQ(line, "after");
}
