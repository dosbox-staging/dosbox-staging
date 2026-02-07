#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <vector>

namespace Webserver {

class DebugCommand {
public:
	bool done = false;
	virtual ~DebugCommand() {}
	virtual void Execute() = 0;

	void WaitForCompletion(uint32_t timeout_ms = 100);
};

class DebugBridge {
	std::mutex mtx                   = {};
	std::condition_variable cv       = {};
	std::vector<DebugCommand*> queue = {};

	DebugBridge(const DebugBridge&)            = delete;
	DebugBridge& operator=(const DebugBridge&) = delete;
	DebugBridge()                              = default;

public:
	static DebugBridge& Instance();

	// Called by the web server thread
	void ExecuteCommand(DebugCommand& cmd, uint32_t timeout_ms);

	// Called by the main thread running the CPU emulation
	void ProcessRequests();
};

} // namespace Webserver
