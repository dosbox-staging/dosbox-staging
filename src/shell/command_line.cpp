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

bool CommandLine::FindExist(const std::string& arg)
{
	cmd_it it;

	return FindEntry(arg, it, false);
}

bool CommandLine::FindRemoveExist(const std::string& arg)
{
	cmd_it it;

	if (!FindEntry(arg, it, false)) {
		return false;
	}

	while (FindEntry(arg, it, false)) {
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

bool CommandLine::FindInt(const std::string& arg, int& value)
{
	cmd_it it;

	if (!FindEntry(arg, it, true)) {
		return false;
	}

	++it;

	const auto result = parse_int(*it);
	if (result) {
		value = *result;
		return true;
	}

	return false;
}

bool CommandLine::FindRemoveInt(const std::string& arg, int& value)
{
	cmd_it it, it_next;

	if (!FindEntry(arg, it, true)) {
		return false;
	}

	it_next = it;
	++it_next;

	const auto result = parse_int(*it_next);
	if (result) {
		value = *result;
		cmds.erase(it, ++it_next);
		return true;
	}

	return false;
}

bool CommandLine::FindString(const std::string& arg, std::string& value)
{
	cmd_it it;

	if (!FindEntry(arg, it, true)) {
		return false;
	}

	++it;
	value = *it;

	return true;
}

bool CommandLine::FindRemoveString(const std::string& arg, std::string& value)
{
	cmd_it it, it_next;

	if (!FindEntry(arg, it, true)) {
		return false;
	}

	it_next = it;
	++it_next;
	value = *it_next;

	cmds.erase(it, ++it_next);
	return true;
}

bool CommandLine::FindCommand(const size_t which, std::string& value) const
{
	if (which < 1) {
		return false;
	}
	if (which > cmds.size()) {
		return false;
	}
	auto it = cmds.begin();
	for (auto i = which; i > 1; --i) {
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

bool CommandLine::FindEntry(const std::string& arg, cmd_it& it, const bool needs_next)
{
	for (it = cmds.begin(); it != cmds.end(); ++it) {
		if (iequals(*it, arg)) {
			cmd_it itnext = it;
			++itnext;
			if (needs_next && (itnext == cmds.end())) {
				return false;
			}
			return true;
		}
	}
	return false;
}

bool CommandLine::FindStringBegin(const std::string& arg, std::string& value)
{
	const size_t len = arg.size();
	for (cmd_it it = cmds.begin(); it != cmds.end(); ++it) {
		if (strncmp(arg.c_str(), (*it).c_str(), len) == 0) {
			value = it->substr(len);
			return true;
		}
	}
	return false;
}

bool CommandLine::FindRemoveStringBegin(const std::string& arg, std::string& value)
{
	const size_t len = arg.size();
	for (cmd_it it = cmds.begin(); it != cmds.end(); ++it) {
		if (strncmp(arg.c_str(), (*it).c_str(), len) == 0) {
			value = it->substr(len);
			cmds.erase(it);
			return true;
		}
	}
	return false;
}

bool CommandLine::FindStringRemain(const std::string& arg, std::string& value)
{
	cmd_it it;
	value.clear();
	if (!FindEntry(arg, it)) {
		return false;
	}
	++it;
	for (; it != cmds.end(); ++it) {
		value += " ";
		value += (*it);
	}
	return true;
}

bool CommandLine::FindStringRemainBegin(const std::string& arg, std::string& value)
{
	cmd_it it;
	value.clear();

	if (!FindEntry(arg, it)) {
		const size_t len = arg.size();

		for (it = cmds.begin(); it != cmds.end(); ++it) {
			if (strncasecmp(arg.c_str(), (*it).c_str(), len) == 0) {
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

size_t CommandLine::GetCount() const
{
	return cmds.size();
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
	AddArguments(cmdline);
}

void CommandLine::AddEnvArguments(const std::string_view variable)
{
	auto idx = variable.find('=');
	if (idx != std::string::npos) {
		AddArguments(variable.substr(idx + 1));
	}
}

void CommandLine::AddArguments(const std::string_view arguments)
{
	bool in_quotes = false;
	cmds.emplace_back();

	for (const auto& c : arguments) {
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

void CommandLine::Shift(const size_t amount)
{
	auto counter = amount;
	while (counter--) {
		file_name = cmds.size() ? (*(cmds.begin())) : "";
		if (cmds.size()) {
			cmds.erase(cmds.begin());
		}
	}
}

bool CommandLine::FindBoolArgument(const std::string& name, char short_letter)
{
	const std::string double_dash = "--" + name;
	const std::string single_dash = '-' + name;
	const std::string short_form  = std::string("-") + short_letter;

	return FindExist(single_dash, double_dash) ||
	       (short_letter && FindExist(short_form));
}

bool CommandLine::FindRemoveBoolArgument(const std::string& name, char short_letter)
{
	const std::string double_dash = "--" + name;
	const std::string single_dash = '-' + name;
	const std::string short_form  = std::string("-") + short_letter;

	return FindRemoveExist(single_dash, double_dash) ||
	       (short_letter && FindRemoveExist(short_form));
}

std::string CommandLine::FindRemoveSingleString(const std::string& arg)
{
	cmd_it it = {};
	constexpr bool need_next_arg = true;
	while (FindEntry(arg, it, need_next_arg)) {
		cmd_it it_next = it;
		++it_next;
		std::string value = *it_next;
		bool is_valid     = !value.empty() && value[0] != '-';
		if (is_valid) {
			++it_next;
		}
		it = cmds.erase(it, it_next);
		if (is_valid) {
			return value;
		}
	}
	return {};
}

std::string CommandLine::FindRemoveStringArgument(const std::string& name)
{
	const std::string single_dash = '-' + name;
	const std::string double_dash = "--" + name;

	const auto ret = FindRemoveSingleString(double_dash);
	if (!ret.empty()) {
		return ret;
	}
	return FindRemoveSingleString(single_dash);
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
        const std::string& arg)
{
	if (!FindBoolArgument(arg)) {
		return {};
	}
	return FindRemoveVectorArgument(arg);
}

std::optional<int> CommandLine::FindRemoveIntArgument(const std::string& name)
{
	return parse_int(FindRemoveStringArgument(name));
}
