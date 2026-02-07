#pragma once

#include "bridge.h"
#include "cpu.h"

#include <limits>

#include "libs/http/httplib.h"

namespace Webserver {

enum class Segment : int { NONE, CS, SS, DS, ES, FS, GS };

class ReadMemCommand : public DebugCommand {
	Segment base                = {};
	uint32_t offset             = {};
	uint32_t len                = {};
	std::vector<uint8_t> memory = {};
	uint32_t effective_addr     = {};
	Registers regs              = {};

public:
	ReadMemCommand(Segment base, uint32_t offset, uint32_t len)
	        : base(base),
	          offset(offset),
	          len(len)
	{}

	void Execute() override;
	static void Get(const httplib::Request& req, httplib::Response& res);
};

class WriteMemCommand : public DebugCommand {
	Segment base            = {};
	uint32_t offset         = {};
	std::string data        = {};
	uint32_t effective_addr = {};

public:
	WriteMemCommand(Segment base, uint32_t offset, std::string data)
	        : base(base),
	          offset(offset),
	          data(data)
	{}

	void Execute() override;
	static void Put(const httplib::Request& req, httplib::Response& res);
};

} // namespace Webserver
