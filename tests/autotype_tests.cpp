// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dos/programs/autotype.h"

#include <iterator>
#include <string>
#include <utility>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "cpu/callback.h"
#include "dos/dos.h"
#include "dosbox_test_fixture.h"
#include "hardware/pic.h"
#include "shell/command_line.h"
#include "shell/shell.h"

namespace {

using testing::HasSubstr;

class AutoTypeWithCapturedOutput final : public AUTOTYPE {
public:
	// Programs get their arguments from the PSP of the current process;
	// this replaces them with the given ones
	explicit AutoTypeWithCapturedOutput(const std::string& args)
	{
		delete cmd;
		cmd = new CommandLine("AUTOTYPE", args);
	}

	std::string output = {};

protected:
	void WriteToConsole(const std::string& str) override
	{
		output += str;
	}
};

// The AUTOTYPE command types by triggering the mapper's key events, so the
// keys travel through the emulated keyboard, its controller, the keyboard
// interrupt, and the BIOS. The tests read the typed keys from the standard
// input, where DOS programs get them from.
//
// All timings are measured on the clock of the emulated machine, so they don't
// depend on the speed of the host.
//
class AutoTypeTest : public DOSBoxTestFixture {
public:
	void SetUp() override
	{
		DOSBoxTestFixture::SetUp();
		start_ms = PIC_FullIndex();
	}

protected:
	// Runs the AUTOTYPE command with the given arguments and returns its
	// console output
	static std::string AutoType(const std::string& args)
	{
		AutoTypeWithCapturedOutput autotype(args);
		autotype.Run();

		return autotype.output;
	}

	// Reads the given number of characters from the standard input while
	// running the emulated machine. Returns fewer characters if the rest
	// doesn't arrive in time, so a key going missing fails the test
	// instead of making it wait forever.
	//
	static std::string ReadInput(const int num_chars)
	{
		constexpr auto timeout_ms = 1000.0;

		const auto read_start_ms = PIC_FullIndex();
		std::string chars        = {};

		while (std::ssize(chars) < num_chars &&
		       PIC_FullIndex() - read_start_ms < timeout_ms) {

			const auto is_input_available = DOS_GetSTDINStatus();
			if (!is_input_available) {
				const auto is_shutdown_requested = CALLBACK_Idle();
				if (is_shutdown_requested) {
					break;
				}
				continue;
			}

			uint8_t data        = 0;
			uint16_t bytes_read = 1;

			if (!DOS_ReadFile(STDIN, &data, &bytes_read) ||
			    bytes_read == 0) {
				break;
			}
			chars += static_cast<char>(data);
		}
		return chars;
	}

	static void RunMachineFor(const double duration_ms)
	{
		const auto run_start_ms = PIC_FullIndex();

		while (PIC_FullIndex() - run_start_ms < duration_ms) {
			const auto is_shutdown_requested = CALLBACK_Idle();
			ASSERT_FALSE(is_shutdown_requested);
		}
	}

	// Returns true if nothing arrives on the standard input for the given
	// time
	static bool NoInputFor(const double duration_ms)
	{
		RunMachineFor(duration_ms);
		return !DOS_GetSTDINStatus();
	}

	// The time since the start of the test
	double ElapsedMs() const
	{
		return PIC_FullIndex() - start_ms;
	}

	// Runs the CHOICE command and returns the errorlevel
	static uint8_t Choice(std::string args)
	{
		DosShell shell = {};
		shell.CMD_CHOICE(args.data());

		return dos.return_code;
	}

	// A button is held down for this long
	static constexpr auto HoldMs = 50.0;

	// A key takes a little time to get from the keyboard to the standard
	// input
	static constexpr auto ToleranceMs = 20.0;

private:
	double start_ms = 0.0;
};

TEST_F(AutoTypeTest, TypesButtonsInOrder)
{
	AutoType("-w 0 -p 0 a 1 b");
	EXPECT_EQ(ReadInput(3), "a1b");
}

// An uppercase letter is typed by holding Shift while pressing the key. Shift
// is released with the key, so the next letter is in lowercase again.
//
TEST_F(AutoTypeTest, TypesUppercaseLettersWithShift)
{
	AutoType("-w 0 -p 0 n N n");
	EXPECT_EQ(ReadInput(3), "nNn");
}

TEST_F(AutoTypeTest, TypesNamedKeys)
{
	AutoType("-w 0 -p 0 space enter tab esc bspace");
	EXPECT_EQ(ReadInput(5), " \r\t\x1b\b");
}

// Extended keys have no ASCII code; they're read as a zero followed by the
// scan code of the key. The scan codes of the Up and F1 keys are 0x48 and
// 0x3b, which are the ASCII codes of 'H' and ';'.
//
TEST_F(AutoTypeTest, TypesExtendedKeys)
{
	using namespace std::string_literals;

	AutoType("-w 0 -p 0 up f1");
	EXPECT_EQ(ReadInput(4), "\0H\0;"s);
}

// The button is released, so the key doesn't start repeating
TEST_F(AutoTypeTest, ReleasesButtons)
{
	AutoType("-w 0 -p 0 a");
	EXPECT_EQ(ReadInput(1), "a");

	constexpr auto well_after_typematic_delay_ms = 700.0;
	EXPECT_TRUE(NoInputFor(well_after_typematic_delay_ms));
}

TEST_F(AutoTypeTest, SkipsUnknownButtons)
{
	AutoType("-w 0 -p 0 a nosuchbutton b");
	EXPECT_EQ(ReadInput(2), "ab");
}

TEST_F(AutoTypeTest, StartsTypingAfterWait)
{
	AutoType("-w 0.2 -p 0 a");

	EXPECT_EQ(ReadInput(1), "a");
	EXPECT_GE(ElapsedMs(), 200.0);
	EXPECT_LT(ElapsedMs(), 200.0 + ToleranceMs);
}

// The pace is the time between releasing a button and pressing the next one
TEST_F(AutoTypeTest, WaitsForPaceBetweenButtons)
{
	AutoType("-w 0 -p 0.1 a b");

	EXPECT_EQ(ReadInput(1), "a");
	EXPECT_LT(ElapsedMs(), ToleranceMs);

	constexpr auto second_button_ms = HoldMs + 100.0;

	EXPECT_EQ(ReadInput(1), "b");
	EXPECT_GE(ElapsedMs(), second_button_ms);
	EXPECT_LT(ElapsedMs(), second_button_ms + ToleranceMs);
}

// Each comma delays the next button by one more pace
TEST_F(AutoTypeTest, CommaAddsExtraPace)
{
	AutoType("-w 0 -p 0.1 a , b");

	constexpr auto second_button_ms = HoldMs + 2 * 100.0;

	EXPECT_EQ(ReadInput(2), "ab");
	EXPECT_GE(ElapsedMs(), second_button_ms);
	EXPECT_LT(ElapsedMs(), second_button_ms + ToleranceMs);
}

TEST_F(AutoTypeTest, MultipleCommasAddUp)
{
	AutoType("-w 0 -p 0.1 a , , b");

	constexpr auto second_button_ms = HoldMs + 3 * 100.0;

	EXPECT_EQ(ReadInput(2), "ab");
	EXPECT_GE(ElapsedMs(), second_button_ms);
	EXPECT_LT(ElapsedMs(), second_button_ms + ToleranceMs);
}

TEST_F(AutoTypeTest, NegativeWaitIsBoundedToZero)
{
	const auto output = AutoType("-w -1 -p 0 a");
	EXPECT_THAT(output, HasSubstr("bounding WAIT value of -1.00 to 0.00"));

	EXPECT_EQ(ReadInput(1), "a");
	EXPECT_LT(ElapsedMs(), ToleranceMs);
}

TEST_F(AutoTypeTest, InvalidPaceTypesNothing)
{
	const auto output = AutoType("-w 0 -p abc a");
	EXPECT_THAT(output, HasSubstr("PACE value 'abc' is not a valid"));

	EXPECT_TRUE(NoInputFor(100.0));
}

TEST_F(AutoTypeTest, EmptyButtonSequenceTypesNothing)
{
	const auto output = AutoType("-w 0 -p 0");
	EXPECT_THAT(output, HasSubstr("button sequence is empty"));

	EXPECT_TRUE(NoInputFor(100.0));
}

TEST_F(AutoTypeTest, ListsButtonNames)
{
	const auto output = AutoType("-list");

	EXPECT_THAT(output, HasSubstr(" enter "));
	EXPECT_THAT(output, HasSubstr(" up "));
	EXPECT_THAT(output, HasSubstr(" f12 "));

	EXPECT_TRUE(NoInputFor(100.0));
}

// The tests below are for AUTOTYPE followed by CHOICE, the combination batch
// files use to automate their menus: the keys are typed in the background
// while CHOICE is waiting for input.
//
// CHOICE always gets a one second timeout with a default that differs from
// the expected choice. That's to get a failure instead of a test waiting
// forever, should nothing be typed.

TEST_F(AutoTypeTest, TypesIntoChoice)
{
	AutoType("-w 0.1 -p 0 n");
	EXPECT_EQ(Choice("/c:yn /t:y,1"), 2);
}

// The choices are case-sensitive here; without Shift, 'n' would be selected
TEST_F(AutoTypeTest, TypesUppercaseLettersIntoChoice)
{
	AutoType("-w 0.1 -p 0 N");
	EXPECT_EQ(Choice("/s /c:nN /t:n,1"), 2);
}

// A cursor key selects the choice that matches its scan code; the Down key
// has the scan code 0x50, which is 'P'.
//
// The zero that precedes the scan code is not a valid choice, and an invalid
// choice cancels the timeout. So the 'q' is typed as a second fallback, in
// case the scan code doesn't arrive or doesn't select anything.
//
TEST_F(AutoTypeTest, TypesExtendedKeysIntoChoice)
{
	AutoType("-w 0.1 -p 0 down q");
	EXPECT_EQ(Choice("/s /c:HKPMq /t:q,1"), 3);
}

} // namespace
