/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2021  The DOSBox Staging Team
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

#include "ethernet.h"

#include <cstring>

#include "control.h"
#include "ethernet_slirp.h"

EthernetConnection *ETHERNET_OpenConnection([[maybe_unused]] const std::string &backend)
{
	EthernetConnection *conn = nullptr;
	// Currently only slirp is supported
	if (backend == "slirp") {
		conn = new SlirpEthernetConnection;
		assert(control);
		const auto settings = control->GetSection("ethernet");
		if (!conn->Initialize(settings)) {
			delete conn;
			conn = nullptr;
		}
	}

	return conn;
}
