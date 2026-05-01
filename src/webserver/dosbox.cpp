// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "private/dosbox.h"

#include "dosbox.h"

namespace Webserver {

void ShutdownCommand::Execute()
{
	DOSBOX_RequestShutdown();
}

void ShutdownCommand::Post(const httplib::Request&, httplib::Response&)
{
	ShutdownCommand cmd;
	cmd.WaitForCompletion();
}

} // namespace Webserver
