#pragma once

#include "bridge.h"

#include "libs/http/httplib.h"

namespace Webserver {

// Get pointers to interesting data structures, this command is just to prevent
// breakages if these ever change and users hard-code these offsets. It's not
// a place to pull random info that can also be read by the client from these
// addresses directly.
class DosInfoCommand : public DebugCommand {
	uint16_t lol;
	uint16_t sda;
	uint16_t first_shell;

public:
	void Execute() override;
	static void Get(const httplib::Request& req, httplib::Response& res);
};

enum class MemoryArea : int { CONV, UMA };

class AllocMemoryCommand : public DebugCommand {
	MemoryArea area = {};
	uint32_t addr   = 0;
	uint16_t bytes  = 0;

public:
	AllocMemoryCommand(uint16_t bytes, MemoryArea area)
	        : area(area),
	          bytes(bytes)
	{}

	void Execute() override;
	static void Post(const httplib::Request& req, httplib::Response& res);
};

class FreeMemoryCommand : public DebugCommand {
	uint32_t addr = 0;
	bool success  = false;

public:
	FreeMemoryCommand(uint32_t addr) : addr(addr) {}

	void Execute() override;
	static void Post(const httplib::Request& req, httplib::Response& res);
};

} // namespace Webserver
