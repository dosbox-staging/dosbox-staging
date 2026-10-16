// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_WEBSERVER_DEBUG_H
#define DOSBOX_WEBSERVER_DEBUG_H

#include "debugger/debugger_api.h"
#include "http/http.h"
#include "webserver/bridge.h"

namespace Webserver {

class DebugStatusCommand final : public Command {
public:
	void Execute() override;
	static void Get(const httplib::Request& req, httplib::Response& res);
	const DebugStopState& GetStopState() const
	{
		return stop_state;
	}

private:
	DebugStopState stop_state = {};
};

class DebugPauseCommand final : public Command {
public:
	explicit DebugPauseCommand(const DebugPauseMode pause_mode)
	        : pause_mode(pause_mode)
	{}

	void Execute() override;
	static void Post(const httplib::Request& req, httplib::Response& res);

private:
	DebugPauseMode pause_mode = DebugPauseMode::Headless;
	DebugResult result        = {};
};

class DebugContinueCommand final : public Command {
public:
	void Execute() override;
	static void Post(const httplib::Request& req, httplib::Response& res);

private:
	DebugResult result = {};
};

class DebugStepCommand final : public Command {
public:
	explicit DebugStepCommand(const DebugStepMode step_mode)
	        : step_mode(step_mode)
	{}

	void Execute() override;
	static void Post(const httplib::Request& req, httplib::Response& res);

private:
	DebugStepMode step_mode = DebugStepMode::Into;
	DebugResult result      = {};
};

} // namespace Webserver

#endif // DOSBOX_WEBSERVER_DEBUG_H
