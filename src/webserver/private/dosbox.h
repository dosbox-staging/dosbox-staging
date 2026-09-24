// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_WEBSERVER_DOSBOX_H
#define DOSBOX_WEBSERVER_DOSBOX_H

#include "webserver/bridge.h"

#include "http/http.h"

namespace Webserver {

class ShutdownCommand : public Command {
public:
	void Execute() override;
	static void Post(const httplib::Request& req, httplib::Response& res);
};

class PauseCommand : public Command {
public:
	explicit PauseCommand(const bool pause) : pause(pause) {}
	void Execute() override;
	static void PostPause(const httplib::Request& req, httplib::Response& res);
	static void PostResume(const httplib::Request& req, httplib::Response& res);

private:
	bool pause = false;
};

class ReloadSignaturesCommand : public Command {
public:
	void Execute() override;
	static void Post(const httplib::Request& req, httplib::Response& res);

private:
	int count = 0;
};

} // namespace Webserver

#endif // DOSBOX_WEBSERVER_DOSBOX_H
