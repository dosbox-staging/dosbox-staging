/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2022-2024  The DOSBox Staging Team
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

// Empty functions to suppress output in unit tests as well as a to eliminate
// dependency-sprawl

#include "messages.h"

void MSG_Add(const std::string &, const std::string &) {}

const char *MSG_Get(const std::string &)
{
	return "";
}

const char *MSG_GetRaw(const std::string &)
{
	return "";
}

bool MSG_Exists(const std::string &)
{
	return true;
}

bool MSG_WriteToFile(const std::string &)
{
	return true;
}

class Section_prop;
void MSG_Init(Section_prop *) {}
