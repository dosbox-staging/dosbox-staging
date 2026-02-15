// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_WEBSERVER_MEMORY_H
#define DOSBOX_WEBSERVER_MEMORY_H

#include "bridge.h"
#include "cpu.h"

#include <limits>

#include "libs/http/http.h"

namespace Webserver {

enum class Segment { None, CS, SS, DS, ES, FS, GS };

class ReadMemCommand : public DebugCommand {
public:
	ReadMemCommand(const Segment base, const uint32_t offset, const uint32_t len)
	        : base(base),
	          offset(offset),
	          len(len)
	{}

	void Execute() override;
	static void Get(const httplib::Request& req, httplib::Response& res);

private:
	// Request
	Segment base    = {};
	uint32_t offset = {};
	uint32_t len    = {};

	// Response
	// Memory is std::string to avoid ugly casts for httplib
	std::string memory      = {};
	uint32_t effective_addr = {};
	Registers regs          = {};
};

class WriteMemCommand : public DebugCommand {
public:
	WriteMemCommand(const Segment base, const uint32_t offset,
	                std::string data, std::string expected_data)
	        : base(base),
	          offset(offset),
	          data(std::move(data)),
	          expected_data(std::move(expected_data))
	{}

	void Execute() override;
	static void Put(const httplib::Request& req, httplib::Response& res);

private:
	// Request
	Segment base     = {};
	uint32_t offset  = {};
	std::string data = {};
	// Only write the data if the current data at the address exactly
	// matches this. Usable as an atomic CAS to implement a mutex.
	std::string expected_data = {};

	// Response
	uint32_t effective_addr = {};
	// Only filled if expected_data was set and didn't match.
	std::string conflict_data = {};
};

} // namespace Webserver

#endif //DOSBOX_WEBSERVER_MEMORY_H
