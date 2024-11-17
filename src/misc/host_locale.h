/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2024-2024  The DOSBox Staging Team
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

#ifndef DOSBOX_HOST_LOCALE_H
#define DOSBOX_HOST_LOCALE_H

#include <optional>
#include <string>

struct HostLanguage {
	// If the host OS support code cannot determine the UI language, leave
	// it as default
	std::string language_file = {}; // translation (messages)
	// If detection was successful, always provide info for the log output,
	// telling which host OS property/value was used to determine the
	// language. Feel free to provide it even if detection failed.
	std::string log_info = {};
};

const HostLanguage& GetHostLanguage();

#endif
