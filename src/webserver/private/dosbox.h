// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_WEBSERVER_SESSION_H
#define DOSBOX_WEBSERVER_SESSION_H

#include "webserver/bridge.h"

#include "libs/http/http.h"

namespace Webserver {

class ShutdownCommand : public Command {
public:
	void Execute() override;
	static void Post(const httplib::Request& req, httplib::Response& res);
};

} // namespace Webserver

#endif // DOSBOX_WEBSERVER_SESSION_H
