// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_WEBSERVER_DOS_H
#define DOSBOX_WEBSERVER_DOS_H
#include "bridge.h"

#include "libs/http/http.h"

namespace Webserver {

// Get pointers to interesting data structures, this command is just to prevent
// breakages if these ever change and users hard-code these offsets. It's not
// a place to pull random info that can also be read by the client from these
// addresses directly.
class DosInfoCommand : public DebugCommand {
	// Usually retrieved with int 21h, ah=0x52
	uint16_t list_of_lists = {};
	// Usually retrieved with int 21h ax=0x5d06
	uint16_t dos_swappable_area = {};
	// Pointer to PSP of first shell, basically start of usable memory
	uint16_t first_shell = {};

public:
	void Execute() override;
	static void Get(const httplib::Request& req, httplib::Response& res);
};

enum class MemoryArea { Conv, Uma, Xms };

enum class AllocStrategy { FirstFit, BestFit, LastFit };

class AllocMemoryCommand : public DebugCommand {
public:
	AllocMemoryCommand(const uint16_t bytes, const MemoryArea area, const AllocStrategy strategy)
	        : area(area),
	          strategy(strategy),
	          bytes(bytes)
	          
	{}

	void Execute() override;
	static void Post(const httplib::Request& req, httplib::Response& res);

private:
	MemoryArea area        = {};
	AllocStrategy strategy = AllocStrategy::BestFit;
	uint32_t addr          = 0;
	uint16_t bytes         = 0;

	void AllocDos();
	void AllocXms();
};

class FreeMemoryCommand : public DebugCommand {
public:
	FreeMemoryCommand(const uint32_t addr) : addr(addr) {}

	void Execute() override;
	static void Post(const httplib::Request& req, httplib::Response& res);

private:
	uint32_t addr = 0;
	bool success  = false;
};

} // namespace Webserver

#endif // DOSBOX_WEBSERVER_DOS_H
