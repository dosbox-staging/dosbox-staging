// SPDX-FileCopyrightText:  2022-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

// This is a stubbed out implementation replacement for the actual CommandLine
// class used by the unit test system for two reasons:
//
//  1. To avoid pulling in the DOS shell dependencies, which in turn
//     depend on the greater DOS system.
//
//  2. Unit tests that aren't testing the CommandLine class itself
//     don't need command line parsing functionality as their inputs
//     are defined in the unit test function bodies.

#include "shell/command_line.h"

bool CommandLine::HasDirectory() const
{
	return false;
}

bool CommandLine::HasExecutableName() const
{
	return false;
}

bool CommandLine::FindRemoveBoolArgument(const std::string&, char)
{
	return false;
}

std::string CommandLine::FindRemoveStringArgument(const std::string&)
{
	return {};
}

std::optional<int> CommandLine::FindRemoveIntArgument(const std::string&)
{
	return {};
}

std::vector<std::string> CommandLine::FindRemoveVectorArgument(const std::string&)
{
	return {};
}

std::optional<std::vector<std::string>> CommandLine::FindRemoveOptionalArgument(
        const std::string&)
{
	return {};
}
