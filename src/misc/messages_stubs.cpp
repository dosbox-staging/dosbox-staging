/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2022-2022  The DOSBox Staging Team
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

void MSG_Add(const char *, const char *) {}

const char *MSG_Get(char const *)
{
	return "";
}

const char *MSG_GetRaw(char const *)
{
	return "";
}

bool MSG_Exists(const char *)
{
	return true;
}

bool MSG_Write(const char *)
{
	return true;
}

class Section_prop;
void MSG_Init(Section_prop *) {}
