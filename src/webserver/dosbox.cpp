// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "private/dosbox.h"
#include "signatures.h"
#include "webserver.h"

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

void PauseCommand::Execute()
{
	if (pause) {
		DOSBOX_RequestUserPause();
	} else {
		DOSBOX_RequestUserResume();
	}
}

void PauseCommand::PostPause(const httplib::Request&, httplib::Response&)
{
	PauseCommand cmd(true);
	cmd.WaitForCompletion();
}

void PauseCommand::PostResume(const httplib::Request&, httplib::Response&)
{
	PauseCommand cmd(false);
	cmd.WaitForCompletion();
}

void ReloadSignaturesCommand::Execute()
{
	count = Signatures::Reload();
}

void ReloadSignaturesCommand::Post(const httplib::Request&, httplib::Response& res)
{
	ReloadSignaturesCommand cmd;
	cmd.WaitForCompletion();

	nlohmann::json j;
	j["signatures"] = cmd.count;
	send_json(res, j);
}

} // namespace Webserver
