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

#include "fs_utils.h"
#include "programs.h"
#include "string_utils.h"

bool CommandLine::FindExist(const char* name, bool remove)
{
	cmd_it it;
	if (!(FindEntry(name, it, false))) {
		return false;
	}
	if (remove) {
		cmds.erase(it);
	}
	return true;
}

// Checks if any of the command line arguments are found in the pre_args *and*
// exist prior to any of the post_args. If none of the command line arguments
// are found in the pre_args then false is returned.
bool CommandLine::ExistsPriorTo(const std::list<std::string_view>& pre_args,
                                const std::list<std::string_view>& post_args) const
{
	auto any_of_iequals = [](const auto& haystack, const auto& needle) {
		return std::any_of(haystack.begin(),
		                   haystack.end(),
		                   [&needle](const auto& hay) {
			                   return iequals(hay, needle);
		                   });
	};

	for (const auto& cli_arg : cmds) {
		// If any of the pre-args are insensitive-equal-to the CLI
		// argument
		if (any_of_iequals(pre_args, cli_arg)) {
			return true;
		}
		if (any_of_iequals(post_args, cli_arg)) {
			return false;
		}
	}
	return false;
}

bool CommandLine::FindInt(const char* name, int& value, bool remove)
{
	cmd_it it, it_next;

	if (!(FindEntry(name, it, true))) {
		return false;
	}

	it_next = it;
	++it_next;
	value = atoi((*it_next).c_str());

	if (remove) {
		cmds.erase(it, ++it_next);
	}
	return true;
}

bool CommandLine::FindString(const char* name, std::string& value, bool remove)
{
	cmd_it it, it_next;

	if (!(FindEntry(name, it, true))) {
		return false;
	}

	it_next = it;
	++it_next;
	value = *it_next;

	if (remove) {
		cmds.erase(it, ++it_next);
	}
	return true;
}

bool CommandLine::FindCommand(unsigned int which, std::string& value) const
{
	if (which < 1) {
		return false;
	}
	if (which > cmds.size()) {
		return false;
	}
	auto it = cmds.begin();
	for (; which > 1; --which) {
		++it;
	}
	value = (*it);
	return true;
}

// Was a directory provided on the command line?
bool CommandLine::HasDirectory() const
{
	return std::any_of(cmds.begin(), cmds.end(), is_directory);
}

// Was an executable filename provided on the command line?
bool CommandLine::HasExecutableName() const
{
	for (const auto& arg : cmds) {
		if (is_executable_filename(arg)) {
			return true;
		}
	}
	return false;
}

bool CommandLine::FindEntry(const char* name, cmd_it& it, bool neednext)
{
	for (it = cmds.begin(); it != cmds.end(); ++it) {
		if (!strcasecmp((*it).c_str(), name)) {
			cmd_it itnext = it;
			++itnext;
			if (neednext && (itnext == cmds.end())) {
				return false;
			}
			return true;
		}
	}
	return false;
}

bool CommandLine::FindStringBegin(const char* const begin, std::string& value,
                                  bool remove)
{
	size_t len = strlen(begin);
	for (cmd_it it = cmds.begin(); it != cmds.end(); ++it) {
		if (strncmp(begin, (*it).c_str(), len) == 0) {
			value = ((*it).c_str() + len);
			if (remove) {
				cmds.erase(it);
			}
			return true;
		}
	}
	return false;
}

bool CommandLine::FindStringRemain(const char* name, std::string& value)
{
	cmd_it it;
	value.clear();
	if (!FindEntry(name, it)) {
		return false;
	}
	++it;
	for (; it != cmds.end(); ++it) {
		value += " ";
		value += (*it);
	}
	return true;
}

// Only used for parsing command.com /C
// Allowing /C dir and /Cdir
// Restoring quotes back into the commands so command /C mount d "/tmp/a b"
// works as intended
bool CommandLine::FindStringRemainBegin(const char* name, std::string& value)
{
	cmd_it it;
	value.clear();

	if (!FindEntry(name, it)) {
		size_t len = strlen(name);

		for (it = cmds.begin(); it != cmds.end(); ++it) {
			if (strncasecmp(name, (*it).c_str(), len) == 0) {
				std::string temp = ((*it).c_str() + len);
				// Restore quotes for correct parsing in later
				// stages
				if (temp.find(' ') != std::string::npos) {
					value = std::string("\"") + temp +
					        std::string("\"");
				} else {
					value = temp;
				}
				break;
			}
		}
		if (it == cmds.end()) {
			return false;
		}
	}

	++it;

	for (; it != cmds.end(); ++it) {
		value += " ";
		std::string temp = (*it);
		if (temp.find(' ') != std::string::npos) {
			value += std::string("\"") + temp + std::string("\"");
		} else {
			value += temp;
		}
	}
	return true;
}

bool CommandLine::GetStringRemain(std::string& value)
{
	if (!cmds.size()) {
		return false;
	}

	cmd_it it = cmds.begin();
	value     = (*it++);

	for (; it != cmds.end(); ++it) {
		value += " ";
		value += (*it);
	}
	return true;
}

unsigned int CommandLine::GetCount(void)
{
	return (unsigned int)cmds.size();
}

std::vector<std::string> CommandLine::GetArguments()
{
	std::vector<std::string> args;
	for (const auto& cmd : cmds) {
		args.emplace_back(cmd);
	}
#ifdef WIN32
	// Add back the \" if the parameter contained a space
	for (auto& arg : args) {
		if (arg.find(' ') != std::string::npos) {
			arg = "\"" + arg + "\"";
		}
	}
#endif
	return args;
}

int CommandLine::GetParameterFromList(const char* const params[],
                                      std::vector<std::string>& output)
{
	// Return values: 0 = P_NOMATCH, 1 = P_NOPARAMS
	// TODO return nomoreparams
	int retval = 1;
	output.clear();

	enum { P_START, P_FIRSTNOMATCH, P_FIRSTMATCH } parsestate = P_START;

	cmd_it it = cmds.begin();

	while (it != cmds.end()) {
		bool found = false;

		for (int i = 0; *params[i] != 0; ++i) {
			if (!strcasecmp((*it).c_str(), params[i])) {
				// Found a parameter
				found = true;
				switch (parsestate) {
				case P_START:
					retval     = i + 2;
					parsestate = P_FIRSTMATCH;
					break;
				case P_FIRSTMATCH:
				case P_FIRSTNOMATCH: return retval;
				}
			}
		}

		if (!found) {
			switch (parsestate) {
			case P_START:
				// No match
				retval     = 0;
				parsestate = P_FIRSTNOMATCH;
				output.push_back(*it);
				break;
			case P_FIRSTMATCH:
			case P_FIRSTNOMATCH: output.push_back(*it); break;
			}
		}

		cmd_it itold = it;
		++it;

		cmds.erase(itold);
	}

	return retval;
}

CommandLine::CommandLine(int argc, const char* const argv[])
{
	if (argc > 0) {
		file_name = argv[0];
	}

	int i = 1;

	while (i < argc) {
		cmds.push_back(argv[i]);
		i++;
	}
}

uint16_t CommandLine::Get_arglength()
{
	if (cmds.empty()) {
		return 0;
	}

	size_t total_length = 0;
	for (const auto& cmd : cmds) {
		total_length += cmd.size() + 1;
	}

	if (total_length > UINT16_MAX) {
		LOG_WARNING("SETUP: Command line length too long, truncating");
		total_length = UINT16_MAX;
	}

	return static_cast<uint16_t>(total_length);
}

CommandLine::CommandLine(const std::string_view name, const std::string_view cmdline)
        : file_name(name)
{
	bool in_quotes = false;
	cmds.emplace_back();

	for (const auto& c : cmdline) {
		if (c == ' ' && !in_quotes) {
			if (!cmds.back().empty()) {
				cmds.emplace_back();
			}
		} else if (c == '"') {
			in_quotes = !in_quotes;
		} else {
			cmds.back() += c;
		}
	}

	if (cmds.back().empty()) {
		cmds.pop_back();
	}
}

void CommandLine::Shift(unsigned int amount)
{
	while (amount--) {
		file_name = cmds.size() ? (*(cmds.begin())) : "";
		if (cmds.size()) {
			cmds.erase(cmds.begin());
		}
	}
}

bool CommandLine::FindBoolArgument(const std::string& name, bool remove,
                                   char short_letter)
{
	const std::string double_dash = "--" + name;
	const std::string dash        = '-' + name;
	char short_name[3]            = {};
	short_name[0]                 = '-';
	short_name[1]                 = short_letter;
	return FindExist(double_dash.c_str(), remove) ||
	       FindExist(dash.c_str(), remove) ||
	       (short_letter && FindExist(short_name, remove));
}

bool CommandLine::FindRemoveBoolArgument(const std::string& name, char short_letter)
{
	constexpr bool remove_arg = true;
	return FindBoolArgument(name, remove_arg, short_letter);
}

std::string CommandLine::FindRemoveSingleString(const char* name)
{
	cmd_it it                    = {};
	constexpr bool need_next_arg = true;
	while (FindEntry(name, it, need_next_arg)) {
		cmd_it it_next = it;
		++it_next;
		std::string value = *it_next;
		bool is_valid     = !value.empty() && value[0] != '-';
		if (is_valid) {
			++it_next;
		}
		cmds.erase(it, it_next);
		if (is_valid) {
			return value;
		}
	}
	return {};
}

std::string CommandLine::FindRemoveStringArgument(const std::string& name)
{
	const std::string double_dash = "--" + name;
	const std::string dash        = '-' + name;

	std::string ret = FindRemoveSingleString(double_dash.c_str());
	if (!ret.empty()) {
		return ret;
	}
	return FindRemoveSingleString(dash.c_str());
}

std::vector<std::string> CommandLine::FindRemoveVectorArgument(const std::string& name)
{
	std::vector<std::string> arg = {};
	for (;;) {
		std::string str = FindRemoveStringArgument(name);
		if (str.empty()) {
			break;
		}
		arg.emplace_back(std::move(str));
	}
	return arg;
}

std::optional<std::vector<std::string>> CommandLine::FindRemoveOptionalArgument(
        const std::string& name)
{
	constexpr bool remove_arg = false;
	if (!FindBoolArgument(name, remove_arg)) {
		return {};
	}
	return FindRemoveVectorArgument(name);
}

std::optional<int> CommandLine::FindRemoveIntArgument(const std::string& name)
{
	return parse_int(FindRemoveStringArgument(name));
}
