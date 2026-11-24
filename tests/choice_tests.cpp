// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "shell/choice.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "dos/dos.h"
#include "dos/drives.h"
#include "dosbox_test_fixture.h"
#include "hardware/pic.h"
#include "ints/bios.h"
#include "misc/support.h"
#include "shell/shell.h"

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

// The tests below run the CHOICE command against the emulated machine. They
// check that the command is wired up correctly to the standard input and the
// keyboard; the input handling logic itself is covered by the tests above.

// BIOS key codes; the scan code is in the high byte and the ASCII code in the
// low byte
constexpr uint16_t KeyY  = 0x1579;
constexpr uint16_t KeyN  = 0x316e;
constexpr uint16_t KeyQ  = 0x1071;
constexpr uint16_t KeyF7 = 0x4100;

// The dedicated cursor keys of enhanced keyboards
constexpr uint16_t KeyUp    = 0x48e0;
constexpr uint16_t KeyLeft  = 0x4be0;
constexpr uint16_t KeyRight = 0x4de0;
constexpr uint16_t KeyDown  = 0x50e0;

// The cursor keys of the numeric keypad
constexpr uint16_t KeyKeypadUp = 0x4800;

constexpr uint8_t ChoiceFailed = 255;

class ShellWithCapturedOutput final : public DosShell {
public:
	void WriteToConsole(const std::string& str) override
	{
		output += str;
	}

	// Returns the console output written since the last call
	std::string GetOutput()
	{
		return std::exchange(output, {});
	}

private:
	std::string output = {};
};

class ChoiceCommandTest : public DOSBoxTestFixture {
public:
	void SetUp() override
	{
		DOSBoxTestFixture::SetUp();

		// The shell must be created after the emulated machine has
		// been set up
		shell = std::make_unique<ShellWithCapturedOutput>();
	}

	void TearDown() override
	{
		if (input) {
			VFILE_Remove(InputFileName);
		}
		shell = {};

		DOSBoxTestFixture::TearDown();
	}

protected:
	// Puts the key into the BIOS keyboard buffer, where it waits to be
	// read
	static void PressKey(const uint16_t key_code)
	{
		ASSERT_TRUE(BIOS_AddKeyToBuffer(key_code));
	}

	// Redirects the standard input to a file with the given contents, the
	// same way the shell does it for `choice < file`
	void RedirectInput(const std::string& contents)
	{
		input = std::vector<uint8_t>(contents.begin(), contents.end());
		VFILE_Register(InputFileName,
		               input->data(),
		               check_cast<uint32_t>(input->size()),
		               "");

		uint16_t handle = 0;
		ASSERT_TRUE(DOS_OpenFile(InputFilePath, OPEN_READ, &handle));

		DOS_PSP psp(dos.psp());
		psp.SetFileHandle(STDIN, psp.GetFileHandle(handle));
	}

	// Returns the errorlevel
	uint8_t Choice(std::string args)
	{
		shell->CMD_CHOICE(args.data());
		return dos.return_code;
	}

	std::unique_ptr<ShellWithCapturedOutput> shell = {};

private:
	static constexpr auto InputFileName = "INPUT.TXT";
	static constexpr auto InputFilePath = "Z:\\INPUT.TXT";

	std::optional<std::vector<uint8_t>> input = {};
};

// The errorlevel is the position of the selected choice in the list of
// choices, starting from one. The output is the prompt followed by the
// selected choice.
//
TEST_F(ChoiceCommandTest, ReturnsIndexOfPressedKey)
{
	PressKey(KeyN);
	EXPECT_EQ(Choice(""), 2);
	EXPECT_EQ(shell->GetOutput(), "[Y,N]?N\n");

	PressKey(KeyY);
	EXPECT_EQ(Choice("/c:nqy"), 3);
	EXPECT_EQ(shell->GetOutput(), "[N,Q,Y]?Y\n");
}

// In these tests, a valid regular key follows the extended key. That's to get
// a failure instead of a test waiting forever for a key press, should CHOICE
// ignore the extended key. The 'q' fallback is the sixth choice.
//
constexpr auto ExtendedKeyChoices = "/s /c:HKPMAq";

struct ExtendedKey {
	uint16_t key_code   = 0;
	int expected_choice = 0;
};

class ChoiceCommandExtendedKeyTest
        : public ChoiceCommandTest,
          public testing::WithParamInterface<ExtendedKey> {};

// Extended keys are matched by their scan code as in MS-DOS, so the cursor
// keys select 'H', 'K', 'P', and 'M'. Batch file menus rely on this.
TEST_P(ChoiceCommandExtendedKeyTest, SelectsByScanCode)
{
	PressKey(GetParam().key_code);
	PressKey(KeyQ);

	EXPECT_EQ(Choice(ExtendedKeyChoices), GetParam().expected_choice);
}

// The expected choice is the position of the key's scan code in the choices
// "HKPMAq". The cursor keys of the numeric keypad have the same scan codes as
// the dedicated ones, but they're stored with a zero instead of 0xe0 in the low
// byte of the key code. F7 has the scan code 0x41, which is 'A'.
//
INSTANTIATE_TEST_SUITE_P(
        ExtendedKeys, ChoiceCommandExtendedKeyTest,
        testing::Values(ExtendedKey{KeyUp, 1}, ExtendedKey{KeyLeft, 2},
                        ExtendedKey{KeyDown, 3}, ExtendedKey{KeyRight, 4},
                        ExtendedKey{KeyKeypadUp, 1}, ExtendedKey{KeyF7, 5}));

// In the tests using redirected input, the wrong choice is waiting in the
// keyboard buffer. That's to get a failure instead of a test waiting forever
// for a key press, should CHOICE read from the keyboard.

TEST_F(ChoiceCommandTest, ReadsRedirectedInput)
{
	PressKey(KeyY);
	RedirectInput("n\r\n");

	EXPECT_EQ(Choice("/c:yn"), 2);
}

// The 'q' and the line ending after it are skipped
TEST_F(ChoiceCommandTest, IgnoresInvalidCharactersInRedirectedInput)
{
	PressKey(KeyY);
	RedirectInput("q\r\nn\r\n");

	EXPECT_EQ(Choice("/c:yn"), 2);
}

// Running out of redirected input is an error; CHOICE must not fall back to
// the keyboard
TEST_F(ChoiceCommandTest, FailsOnEmptyRedirectedInput)
{
	PressKey(KeyY);
	RedirectInput("");

	EXPECT_EQ(Choice("/c:yn"), ChoiceFailed);
}

TEST_F(ChoiceCommandTest, FailsIfRedirectedInputHasNoValidChoice)
{
	PressKey(KeyY);
	RedirectInput("q\r\n");

	EXPECT_EQ(Choice("/c:yn"), ChoiceFailed);
}

// With a timeout, CHOICE checks whether the standard input has something to
// read before reading it. A file must always count as ready; otherwise, the
// timeout elapses and the default 'n' gets selected.
//
// The input file is on the read-only Z: drive on purpose. Files on read-only
// drives set a flag that means "no input available" for devices, so checking
// that flag without checking for a device first gets this case wrong.
//
TEST_F(ChoiceCommandTest, ReadsRedirectedInputWithoutWaitingForTimeout)
{
	RedirectInput("y\r\n");

	EXPECT_EQ(Choice("/c:yn /t:n,1"), 1);
}

// No key is pressed, so this takes the whole timeout of one second, the
// shortest CHOICE accepts. The time is measured on the clock of the emulated
// machine, not on the host's.
//
TEST_F(ChoiceCommandTest, ReturnsDefaultChoiceAfterTimeout)
{
	const auto start_ms = PIC_FullIndex();

	EXPECT_EQ(Choice("/c:yn /t:n,1"), 2);
	EXPECT_EQ(shell->GetOutput(), "[Y,N]?N\n");

	constexpr auto timeout_ms   = 1000.0;
	constexpr auto tolerance_ms = 100.0;

	const auto elapsed_ms = PIC_FullIndex() - start_ms;

	EXPECT_GE(elapsed_ms, timeout_ms);
	EXPECT_LT(elapsed_ms, timeout_ms + tolerance_ms);
}

// The key is already waiting in the keyboard buffer, so it's returned without
// waiting for the timeout
TEST_F(ChoiceCommandTest, KeyPressedBeforeTimeoutWins)
{
	PressKey(KeyY);
	EXPECT_EQ(Choice("/c:yn /t:n,1"), 1);
}

} // namespace
