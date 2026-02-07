#include "bridge.h"

#include <chrono>
#include <mutex>
#include <vector>

namespace Webserver {

DebugBridge& DebugBridge::Instance()
{
	static DebugBridge instance;
	return instance;
}

void DebugCommand::WaitForCompletion(uint32_t timeout_ms)
{
	DebugBridge::Instance().ExecuteCommand(*this, timeout_ms);
}

void DebugBridge::ExecuteCommand(DebugCommand& cmd, uint32_t timeout_ms)
{
	std::unique_lock<std::mutex> lock(mtx);
	cmd.done = false;
	queue.push_back(&cmd);
	bool success = cv.wait_for(lock,
	                           std::chrono::milliseconds(timeout_ms),
	                           [&] { return cmd.done; });
	if (!success) {
		auto it = std::find(queue.begin(), queue.end(), &cmd);
		if (it != queue.end()) {
			queue.erase(it);
		}
		throw std::runtime_error("Failed to execute command: timeout");
	}
}

void DebugBridge::ProcessRequests()
{
	std::lock_guard<std::mutex> lock(mtx);
	if (queue.empty()) {
		return;
	}
	for (auto* cmd : queue) {
		cmd->Execute();
		cmd->done = true;
	}
	queue.clear();
	cv.notify_all();
}

} // namespace Webserver
