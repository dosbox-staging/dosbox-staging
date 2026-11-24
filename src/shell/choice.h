// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_SHELL_CHOICE_H
#define DOSBOX_SHELL_CHOICE_H

#include <optional>
#include <string>

// The input handling of the CHOICE command, split out from the shell so it
// can be unit tested in isolation. Reading the standard input of the emulated
// machine happens behind the `ChoiceInput` interface; the logic here is free
// of side effects and globals.

class ChoiceInput {
public:
	// Waits for input to become available. Returns false if there's still
	// none after the timeout.
	virtual bool WaitForInput(const int timeout_ms) = 0;

	// Reads the next character, waiting for it if necessary. Returns an
	// empty optional at the end of the input.
	virtual std::optional<char> Read() = 0;

	virtual ~ChoiceInput() = default;
};

struct ChoiceTimeout {
	// Returned as is; the caller must make sure it's one of the choices
	char default_choice = '\0';
	int duration_ms     = 0;
};

struct ChoiceParams {
	// Must be uppercase unless the choices are case-sensitive
	std::string choices = {};

	bool is_case_sensitive = false;

	// If set, the default choice is selected when there's no input for
	// the duration of the timeout
	std::optional<ChoiceTimeout> timeout = {};
};

enum class ChoiceStatus {
	Selected,

	// Ctrl+C was read from the input
	Aborted,

	// The input ran out without a valid choice
	EndOfInput,
};

struct ChoiceResult {
	ChoiceStatus status = ChoiceStatus::Aborted;

	// Only valid if a choice was selected
	char choice = '\0';

	bool operator==(const ChoiceResult&) const = default;
};

// Reads characters from the input until one of them is a valid choice. The
// characters that aren't choices are skipped.
//
// Returns the default choice if there's a timeout and no input arrives for
// its duration.
//
ChoiceResult read_choice(const ChoiceParams& params, ChoiceInput& input);

#endif
