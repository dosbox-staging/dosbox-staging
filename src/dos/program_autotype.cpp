/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2022  The DOSBox Staging Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "program_autotype.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <sstream>

#include "mapper.h"
#include "math_utils.h"
#include "dosbox.h"
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
                             const double &def_value,
                             const double &min_value,
                             const double &max_value,
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
		WriteOut(MSG_Get("SHELL_CMD_AUTOTYPE_HELP_LONG"));
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
	std::vector<std::string> sequence;
	cmd->FillVector(sequence);
	if (sequence.empty()) {
		WriteOut_NoParsing("AUTOTYPE: button sequence is empty\n");
		return;
	}
	MAPPER_AutoType(sequence, wait_ms, pace_ms);
}

void AUTOTYPE::AddMessages() {
	MSG_Add("SHELL_CMD_AUTOTYPE_HELP_LONG",
	        "Performs scripted keyboard entry into a running DOS game.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=green]autotype[reset] -list\n"
	        "  [color=green]autotype[reset] [-w [color=white]WAIT[reset]] [-p [color=white]PACE[reset]] [color=cyan]BUTTONS[reset]\n"
	        "\n"
	        "Where:\n"
	        "  [color=white]WAIT[reset]    is the number of seconds to wait before typing begins (max of 30).\n"
	        "  [color=white]PACE[reset]    is the number of seconds before each keystroke (max of 10).\n"
	        "  [color=cyan]BUTTONS[reset] is one or more space-separated buttons.\n"
	        "\n"
	        "Notes:\n"
	        "  The [color=cyan]BUTTONS[reset] supplied in the command will be autotyped into running DOS games\n"
	        "  after they start. Autotyping begins after [color=cyan]WAIT[reset] seconds, and each button is\n"
	        "  entered every [color=white]PACE[reset] seconds. The [color=cyan],[reset] character inserts an extra [color=white]PACE[reset] delay.\n"
	        "  [color=white]WAIT[reset] and [color=white]PACE[reset] default to 2 and 0.5 seconds respectively if not specified.\n"
	        "  A list of all available button names can be obtained using the -list option.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=green]autotype[reset] -list\n"
	        "  [color=green]autotype[reset] -w [color=white]1[reset] -p [color=white]0.3[reset] [color=cyan]up enter , right enter[reset]\n"
	        "  [color=green]autotype[reset] -p [color=white]0.2[reset] [color=cyan]f1 kp_8 , , enter[reset]\n"
	        "  [color=green]autotype[reset] -w [color=white]1.3[reset] [color=cyan]esc enter , p l a y e r enter\n[reset]");
}
