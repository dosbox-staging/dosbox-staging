// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "shell/choice.h"

#include <optional>
#include <string>

#include <gtest/gtest.h>

namespace {

constexpr char CtrlC = 0x03;

// The first character of an extended key (e.g., a cursor or function key);
// the second one is the scan code of the key.
constexpr char ExtendedKeyPrefix = '\0';

constexpr ChoiceResult Aborted    = {ChoiceStatus::Aborted};
constexpr ChoiceResult EndOfInput = {ChoiceStatus::EndOfInput};

constexpr ChoiceResult selected(const char choice)
{
	return {ChoiceStatus::Selected, choice};
}

class FakeInput final : public ChoiceInput {
public:
	// Simulates a keyboard or a redirected file with the given contents
	explicit FakeInput(std::string contents) : contents(std::move(contents))
	{}

	bool WaitForInput(const int timeout_ms) override
	{
		++num_waits;
		last_timeout_ms = timeout_ms;

		return !is_timing_out;
	}

	std::optional<char> Read() override
	{
		if (index >= contents.size()) {
			return {};
		}
		return contents[index++];
	}

	// Simulates no input arriving until the timeout
	bool is_timing_out = false;

	int num_waits       = 0;
	int last_timeout_ms = 0;

private:
	std::string contents = {};
	size_t index         = 0;
};

ChoiceResult read_choice_from(const ChoiceParams& params,
                              const std::string& input_contents)
{
	FakeInput input{input_contents};
	return read_choice(params, input);
}

const ChoiceParams YesNo = {.choices = "YN"};

const ChoiceParams YesNoWithTimeout = {
        .choices = "YN",
        .timeout = ChoiceTimeout{.default_choice = 'N', .duration_ms = 5000},
};

TEST(ReadChoice, SelectsValidChoice)
{
	EXPECT_EQ(read_choice_from(YesNo, "Y"), selected('Y'));
	EXPECT_EQ(read_choice_from(YesNo, "N"), selected('N'));
}

// The characters after the selected choice must not be consumed
TEST(ReadChoice, StopsReadingAtFirstValidChoice)
{
	FakeInput input{"NY"};

	EXPECT_EQ(read_choice(YesNo, input), selected('N'));
	EXPECT_EQ(input.Read(), 'Y');
}

// Anything that isn't a choice is skipped, including the line endings of a
// redirected file
TEST(ReadChoice, IgnoresInvalidChoices)
{
	EXPECT_EQ(read_choice_from(YesNo, "qx1 \r\nN"), selected('N'));
}

TEST(ReadChoice, IsCaseInsensitiveByDefault)
{
	EXPECT_EQ(read_choice_from(YesNo, "n"), selected('N'));
}

TEST(ReadChoice, CaseSensitiveChoices)
{
	const ChoiceParams params = {.choices = "yNn", .is_case_sensitive = true};

	EXPECT_EQ(read_choice_from(params, "n"), selected('n'));
	EXPECT_EQ(read_choice_from(params, "N"), selected('N'));

	// 'Y' is not a valid choice, only 'y' is
	EXPECT_EQ(read_choice_from(params, "Yn"), selected('n'));
}

// Extended keys are returned by the keyboard as a zero followed by the scan
// code of the key. MS-DOS matches the scan code against the choices, so the
// up, left, down, and right cursor keys select 'H', 'K', 'P', and 'M'. Batch
// file menus rely on this.
//
TEST(ReadChoice, ExtendedKeysSelectByScanCode)
{
	const ChoiceParams params = {.choices = "HKPM", .is_case_sensitive = true};

	for (const auto scan_code : params.choices) {
		const std::string key = {ExtendedKeyPrefix, scan_code};

		EXPECT_EQ(read_choice_from(params, key), selected(scan_code));
	}
}

// F1 has the scan code 0x3b, which is the ';' character. Neither the zero
// prefix nor a scan code that isn't a choice must select or abort anything.
TEST(ReadChoice, ExtendedKeysWithoutMatchingChoiceAreIgnored)
{
	const std::string f1_key = {ExtendedKeyPrefix, ';'};

	EXPECT_EQ(read_choice_from(YesNo, f1_key + "Y"), selected('Y'));
}

TEST(ReadChoice, CtrlCAborts)
{
	EXPECT_EQ(read_choice_from(YesNo, {CtrlC}), Aborted);

	// Even if a valid choice follows
	EXPECT_EQ(read_choice_from(YesNo, {'q', CtrlC, 'Y'}), Aborted);
}

// This is what happens with redirected input, e.g., `choice < file` or
// `echo x | choice`
TEST(ReadChoice, FailsIfInputEndsWithoutValidChoice)
{
	EXPECT_EQ(read_choice_from(YesNo, ""), EndOfInput);
	EXPECT_EQ(read_choice_from(YesNo, "q\r\n"), EndOfInput);
}

// Waiting for input is only needed to implement the timeout. Without one, the
// blocking read does all the waiting.
TEST(ReadChoice, DoesNotWaitForInputWithoutTimeout)
{
	FakeInput input{"qY"};

	EXPECT_EQ(read_choice(YesNo, input), selected('Y'));
	EXPECT_EQ(input.num_waits, 0);
}

// No input arrives for the whole duration of the timeout, so the default is
// selected without reading anything
TEST(ReadChoice, SelectsDefaultChoiceAfterTimeout)
{
	FakeInput input{""};
	input.is_timing_out = true;

	EXPECT_EQ(read_choice(YesNoWithTimeout, input), selected('N'));
	EXPECT_EQ(input.num_waits, 1);
	EXPECT_EQ(input.last_timeout_ms, 5000);
}

TEST(ReadChoice, InputBeforeTimeoutWins)
{
	FakeInput input{"Y"};

	EXPECT_EQ(read_choice(YesNoWithTimeout, input), selected('Y'));
	EXPECT_EQ(input.num_waits, 1);
}

// After an invalid choice, CHOICE waits for a valid one indefinitely. So only
// the first read is preceded by a wait with the timeout; once 'q' is read,
// there must be no more of those.
//
TEST(ReadChoice, InvalidChoiceCancelsTimeout)
{
	FakeInput input{"qxY"};

	EXPECT_EQ(read_choice(YesNoWithTimeout, input), selected('Y'));
	EXPECT_EQ(input.num_waits, 1);
}

// An empty redirected file is ready to be read straight away, so the result
// is the end of input failure rather than the default choice
TEST(ReadChoice, FailsIfInputEndsBeforeTimeout)
{
	EXPECT_EQ(read_choice_from(YesNoWithTimeout, ""), EndOfInput);
}

} // namespace
