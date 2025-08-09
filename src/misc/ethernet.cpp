// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "misc/ethernet.h"

#include <cstring>

#include "config/config.h"
#include "ethernet_slirp.h"

EthernetConnection* ETHERNET_OpenConnection([[maybe_unused]] const std::string& backend)
{
	EthernetConnection* conn = nullptr;

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
