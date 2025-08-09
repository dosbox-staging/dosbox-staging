// SPDX-FileCopyrightText:  2020-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "program_autotype.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <sstream>

#include "dosbox.h"
#include "gui/mapper.h"
#include "util/math_utils.h"
#include "program_more_output.h"
#include "programs.h"

// Prints the key-names for the mapper's currently-bound events.
void AUTOTYPE::PrintKeys()
{
	const std::vector<std::string> names = MAPPER_GetEventNames("key_");

	// Keep track of the longest key name
	size_t max_length = 0;
	for (const auto &name : names)
		max_length = std::max(name.length(), max_length);

	// Sanity check to avoid dividing by 0
	if (!max_length) {
		WriteOut_NoParsing(
		        "AUTOTYPE: The mapper has no key bindings\n");
		return;
	}

	// Setup our rows and columns
	const size_t wrap_width = 72; // confortable columns not pushed to the edge
	const size_t columns = wrap_width / max_length;
	const size_t rows = ceil_udivide(names.size(), columns);

	// Build the string output by rows and columns
	auto name = names.begin();
	for (size_t row = 0; row < rows; ++row) {
		for (size_t i = row; i < names.size(); i += rows)
			WriteOut("  %-*s", static_cast<int>(max_length), name[i].c_str());
		WriteOut_NoParsing("\n");
	}
}

/*
 *  Reads a floating point argument from command line, where:
 *  - name is a human description for the flag, ie: DELAY
 *  - flag is the command-line flag, ie: -d or -delay
 *  - default is the default value if the flag doesn't exist
 *  - value will be populated with the default or provided value
 *
 *  Returns:
 *    true if 'value' is set to the default or read from the arg.
 *    false if the argument was used but could not be parsed.
 */
bool AUTOTYPE::ReadDoubleArg(const std::string &name,
                             const char *flag,
                             const double def_value,
                             const double min_value,
                             const double max_value,
                             double &value)
{
	bool result = false;
	std::string str_value;

	// Is the user trying to set this flag?
	if (cmd->FindString(flag, str_value, true)) {
		// Can the user's value be parsed?
		const double user_value = to_finite<double>(str_value);
		if (std::isfinite(user_value)) {
			result = true;

			// Clamp the user's value if needed
			value = clamp(user_value, min_value, max_value);

			// Inform them if we had to clamp their value
			if (std::fabs(user_value - value) >
			    std::numeric_limits<double>::epsilon())
				WriteOut("AUTOTYPE: bounding %s value of %.2f "
				         "to %.2f\n",
				         name.c_str(), user_value, value);

		} else { // Otherwise we couldn't parse their value
			WriteOut("AUTOTYPE: %s value '%s' is not a valid "
			         "floating point number\n",
			         name.c_str(), str_value.c_str());
		}
	} else { // Otherwise thay haven't passed this flag
		value = def_value;
		result = true;
	}
	return result;
}

void AUTOTYPE::Run()
{
	// Hack To allow long commandlines
	ChangeToLongCmd();

	// Usage
	if (!cmd->GetCount() || HelpRequested()) {
		MoreOutputStrings output(*this);
		output.AddString(MSG_Get("PROGRAM_AUTOTYPE_HELP_LONG"));
		output.Display();
		return;
	}

	// Print available keys
	if (cmd->FindExist("-list", false)) {
		PrintKeys();
		return;
	}

	// Get the wait delay in milliseconds
	double wait_s;
	constexpr double def_wait_s = 2.0;
	constexpr double min_wait_s = 0.0;
	constexpr double max_wait_s = 30.0;
	if (!ReadDoubleArg("WAIT", "-w", def_wait_s, min_wait_s, max_wait_s, wait_s))
		return;
	const auto wait_ms = static_cast<uint32_t>(wait_s * 1000);

	// Get the inter-key pacing in milliseconds
	double pace_s;
	constexpr double def_pace_s = 0.5;
	constexpr double min_pace_s = 0.0;
	constexpr double max_pace_s = 10.0;
	if (!ReadDoubleArg("PACE", "-p", def_pace_s, min_pace_s, max_pace_s, pace_s))
		return;
	const auto pace_ms = static_cast<uint32_t>(pace_s * 1000);

	// Get the button sequence
	auto sequence = cmd->GetArguments();

	if (sequence.empty()) {
		WriteOut_NoParsing("AUTOTYPE: button sequence is empty\n");
		return;
	}
	MAPPER_AutoType(sequence, wait_ms, pace_ms);
}

void AUTOTYPE::AddMessages() {
	MSG_Add("PROGRAM_AUTOTYPE_HELP_LONG",
	        "Perform scripted keyboard entry into a running DOS game.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=light-green]autotype[reset] -list\n"
	        "  [color=light-green]autotype[reset] [-w [color=white]WAIT[reset]] [-p [color=white]PACE[reset]] [color=light-cyan]BUTTONS[reset]\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=white]WAIT[reset]     number of seconds to wait before typing begins (max of 30)\n"
	        "  [color=white]PACE[reset]     number of seconds before each keystroke (max of 10)\n"
	        "  [color=light-cyan]BUTTONS[reset]  one or more space-separated buttons\n"
	        "\n"
	        "Notes:\n"
	        "  The [color=light-cyan]BUTTONS[reset] supplied in the command will be autotyped into running DOS games\n"
	        "  after they start. Autotyping begins after [color=light-cyan]WAIT[reset] seconds, and each button is\n"
	        "  entered every [color=white]PACE[reset] seconds. The [color=light-cyan],[reset] character inserts an extra [color=white]PACE[reset] delay.\n"
	        "  [color=white]WAIT[reset] and [color=white]PACE[reset] default to 2 and 0.5 seconds respectively if not specified.\n"
	        "  A list of all available button names can be obtained using the -list option.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]autotype[reset] -list\n"
	        "  [color=light-green]autotype[reset] -w [color=white]1[reset] -p [color=white]0.3[reset] [color=light-cyan]up enter , right enter[reset]\n"
	        "  [color=light-green]autotype[reset] -p [color=white]0.2[reset] [color=light-cyan]f1 kp_8 , , enter[reset]\n"
	        "  [color=light-green]autotype[reset] -w [color=white]1.3[reset] [color=light-cyan]esc enter , p l a y e r enter[reset]\n");
}
