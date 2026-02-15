// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_WEBSERVER_BRIDGE_H
#define DOSBOX_WEBSERVER_BRIDGE_H

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <vector>

namespace Webserver {

class DebugCommand {
public:
	virtual ~DebugCommand() {}
	virtual void Execute() = 0;

	void WaitForCompletion(const uint32_t timeout_ms = 250);

private:
	friend class DebugBridge;

	bool done = false;
};

class DebugBridge {
public:
	static DebugBridge& Instance();

	// Called by the web server thread
	void ExecuteCommand(DebugCommand& cmd, const uint32_t timeout_ms);

	// Called by the main thread running the CPU emulation
	void ProcessRequests();

private:
	std::mutex mtx                   = {};
	std::condition_variable cv       = {};
	std::vector<DebugCommand*> queue = {};

	DebugBridge(const DebugBridge&)            = delete;
	DebugBridge& operator=(const DebugBridge&) = delete;
	DebugBridge()                              = default;
};

} // namespace Webserver

#endif // DOSBOX_WEBSERVER_BRIDGE_H
