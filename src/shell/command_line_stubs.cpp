/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2022-2023  The DOSBox Staging Team
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

// This is a stubbed out implementation replacement for the actual CommandLine
// class used by the unit test system for two reasons:
//
//  1. To avoid pulling in the DOS shell dependencies, which in turn
//     depend on the greater DOS system.
//
//  2. Unit tests that aren't testing the CommandLine class itself
//     don't need command line parsing functionality as their inputs
//     are defined in the unit test function bodies.

#include "programs.h"

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
