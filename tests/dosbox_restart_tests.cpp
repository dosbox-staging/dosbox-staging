// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dosbox.h"

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#ifdef WIN32
#include <windows.h>

// Must come after windows.h
#include <shellapi.h>

#include "misc/unicode.h"
#endif

namespace {

#ifdef WIN32

// Parses the command line with the Windows API, the same way a restarted
// DOSBox instance receives it.
std::optional<std::vector<std::string>> parse_arguments(const std::string& command_line)
{
	// The first token is always parsed as the program name
	const auto utf16 = utf8_to_utf16("dosbox.exe " + command_line);
	const auto wide_command_line = std::wstring(utf16.begin(), utf16.end());

	auto num_args = 0;

	const auto argv = std::unique_ptr<LPWSTR, decltype(&LocalFree)>(
	        CommandLineToArgvW(wide_command_line.c_str(), &num_args), LocalFree);

	if (!argv) {
		return {};
	}

	std::vector<std::string> args = {};

	for (auto i = 1; i < num_args; ++i) {
		const std::wstring_view arg = argv.get()[i];
		args.push_back(utf16_to_utf8(std::u16string(arg.begin(), arg.end())));
	}
	return args;
}

std::string join_quoted(const std::vector<std::string>& args)
{
	std::string command_line = {};

	for (const auto& arg : args) {
		if (!command_line.empty()) {
			command_line.push_back(' ');
		}
		command_line.append(quote_windows_argument(arg));
	}
	return command_line;
}

void expect_round_trip(const std::vector<std::string>& args)
{
	const auto command_line = join_quoted(args);

	EXPECT_EQ(parse_arguments(command_line), args)
	        << "Command line: " << command_line;
}

#endif // WIN32

} // namespace

TEST(QuoteWindowsArgument, PlainArgument)
{
	EXPECT_EQ(quote_windows_argument("abc"), "\"abc\"");
}

TEST(QuoteWindowsArgument, ArgumentWithSpaces)
{
	EXPECT_EQ(quote_windows_argument("name value"), "\"name value\"");
}

TEST(QuoteWindowsArgument, EmptyArgument)
{
	EXPECT_EQ(quote_windows_argument(""), "\"\"");
}

TEST(QuoteWindowsArgument, BackslashesInTheMiddleAreLiteral)
{
	EXPECT_EQ(quote_windows_argument(R"(C:\dir\file)"), R"("C:\dir\file")");
}

TEST(QuoteWindowsArgument, TrailingBackslashIsDoubled)
{
	EXPECT_EQ(quote_windows_argument(R"(C:\Program Files\)"),
	          R"("C:\Program Files\\")");

	EXPECT_EQ(quote_windows_argument(R"(C:\dir\\)"), R"("C:\dir\\\\")");
}

TEST(QuoteWindowsArgument, EmbeddedQuoteIsEscaped)
{
	EXPECT_EQ(quote_windows_argument(R"(a"b)"), R"("a\"b")");
	EXPECT_EQ(quote_windows_argument(R"(")"), R"("\"")");
}

TEST(QuoteWindowsArgument, BackslashesBeforeEmbeddedQuoteAreDoubled)
{
	EXPECT_EQ(quote_windows_argument(R"(a\"b)"), R"("a\\\"b")");
	EXPECT_EQ(quote_windows_argument(R"(a\\"b)"), R"("a\\\\\"b")");
}

TEST(QuoteWindowsArgument, AlreadyQuotedArgumentIsQuotedAgain)
{
	EXPECT_EQ(quote_windows_argument(R"("name value")"), R"("\"name value\"")");
}

#ifdef WIN32

TEST(QuoteWindowsArgument, RoundTripsKnownTrickyArguments)
{
	expect_round_trip({R"(C:\Program Files\)"});
	expect_round_trip({R"(C:\Games\Foo\)", R"(C:\Games\Foo Bar\\)"});
	expect_round_trip({"", " ", "\t", "\"", "\\\"", "\\\\\"", "x\\\\"});
	expect_round_trip({R"("name value")", "--set", "section.key=a b"});
}

TEST(QuoteWindowsArgument, RoundTripsRandomArguments)
{
	// Biased towards the characters that are special to the parser
	constexpr std::string_view Alphabet = "\\\\\\\"\" \tab";

	std::mt19937 rng(42);

	const auto random_int = [&](const int max_inclusive) {
		return std::uniform_int_distribution<int>(0, max_inclusive)(rng);
	};

	for (auto i = 0; i < 20'000; ++i) {
		std::vector<std::string> args = {};

		const auto num_args = 1 + random_int(3);
		for (auto j = 0; j < num_args; ++j) {
			std::string arg = {};

			const auto length = random_int(8);
			for (auto k = 0; k < length; ++k) {
				arg.push_back(
				        Alphabet[random_int(Alphabet.size() - 1)]);
			}
			args.push_back(std::move(arg));
		}
		expect_round_trip(args);

		if (HasFailure()) {
			break;
		}
	}
}

#endif // WIN32
