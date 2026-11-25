// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "choice.h"

#include <cctype>

#include "utils/ascii.h"
#include "utils/checks.h"

CHECK_NARROWING();

static char to_upper(const char c)
{
	return static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
}

ChoiceResult read_choice(const ChoiceParams& params, ChoiceInput& input)
{
	// The timeout is only in effect until the first invalid choice is read
	// (see the end of the loop)
	auto timeout = params.timeout;

	// Read the input one character at a time until we get a valid choice.
	// The characters after it are left in the input.
	//
	while (true) {
		// Reading blocks until a character arrives, so the timeout can
		// only be implemented by waiting for the input to become
		// available first. If nothing arrives in time, the default
		// choice is selected without reading anything.
		//
		if (timeout && !input.WaitForInput(timeout->duration_ms)) {
			return {ChoiceStatus::Selected, timeout->default_choice};
		}

		// Only redirected input can end (e.g., `choice < file`); the
		// keyboard just keeps waiting for the next key press.
		//
		const auto maybe_key = input.Read();
		if (!maybe_key) {
			return {ChoiceStatus::EndOfInput};
		}

		// Unless the choices are case-sensitive, they're all in
		// uppercase, so the character must be uppercased to match them
		const auto key = params.is_case_sensitive ? *maybe_key
		                                          : to_upper(*maybe_key);

		// Ctrl+C aborts even if more characters follow it in the input
		if (key == Ascii::CtrlC) {
			return {ChoiceStatus::Aborted};
		}

		// Extended keys (the cursor and function keys, Home, End, etc.)
		// have no ASCII code. Such keys arrive as two characters in two
		// consecutive reads: a zero first, then the scan code of the
		// key. For example, the Up key arrives as 0x00 followed by its
		// scan code 0x48.
		//
		// These two characters don't get any special treatment; each
		// is matched against the choices like the character of a
		// regular key:
		//
		// - The zero can never be a choice, so it's skipped like any
		//   other invalid character.
		//
		// - The scan code then goes through the same check in the next
		//   iteration of the loop. 0x48 happens to be the ASCII code
		//   of 'H', so the Up key selects the 'H' choice if there is
		//   one; otherwise, it's skipped too.
		//
		// This is how MS-DOS behaves, and batch file menus depend on
		// it: they list 'H', 'P', 'K', and 'M' as choices to react to
		// the Up, Down, Left, and Right keys.
		//
		if (params.choices.contains(key)) {
			return {ChoiceStatus::Selected, key};
		}

		// From here on, we wait for a valid choice indefinitely: an
		// invalid one cancels the timeout
		timeout = {};
	}
}
