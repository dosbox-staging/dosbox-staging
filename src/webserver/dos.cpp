// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dos.h"
#include "bridge.h"
#include "webserver.h"

#include "libs/http/http.h"
#include "libs/json/json.h"

#include "cpu/paging.h"
#include "cpu/registers.h"
#include "dos/dos.h"
#include "dos/dos_memory.h"
#include "utils/string_utils.h"

using json = nlohmann::json;

namespace Webserver {

constexpr int DosBlockSize = 16;

void DosInfoCommand::Execute()
{
	list_of_lists      = RealToPhysical(dos_infoblock.GetPointer());
	dos_swappable_area = PhysicalMake(DOS_SDA_SEG, DOS_SDA_OFS);
	first_shell        = PhysicalMake(DOS_FIRST_SHELL, 0);
	LOG_DEBUG("API: DosInfoCommand()");
}

void DosInfoCommand::Get(const httplib::Request&, httplib::Response& res)
{
	DosInfoCommand cmd;
	cmd.WaitForCompletion();

	json j;
	j["listOfLists"]      = cmd.list_of_lists;
	j["dosSwappableArea"] = cmd.dos_swappable_area;
	j["firstShell"]       = cmd.first_shell;
	send_json(res, j);
}

void AllocMemoryCommand::AllocDos()
{
	uint16_t blocks  = (bytes + DosBlockSize - 1) / DosBlockSize;
	auto old_strat   = DOS_GetMemAllocStrategy();
	uint16_t segment = 0;

	DOS_SetMemAllocStrategy(area == MemoryArea::Conv
	                                ? DosMemAllocStrategy::BestFit
	                                : DosMemAllocStrategy::UmbMemoryBestFit);

	auto ok = DOS_AllocateMemory(&segment, &blocks);
	addr    = PhysicalMake(segment, 0);
	LOG_DEBUG("API: AllocMemoryCommand(%d): result=%d, %d bytes at %p (DOS allocator)",
	          bytes,
	          ok,
	          blocks * DosBlockSize,
	          addr);
	DOS_SetMemAllocStrategy(old_strat);

	if (!ok) {
		addr = 0;
	} else if (blocks * DosBlockSize < bytes) {
		DOS_FreeMemory(segment);
		addr = 0;
	}
}

void AllocMemoryCommand::AllocXms()
{
	auto num_pages = (bytes + MEM_PAGE_SIZE - 1) / MEM_PAGE_SIZE;
	auto handle    = MEM_AllocatePages(num_pages, true);

	// Returns 0 on error or out of memory, nullptr is handled as error below.
	addr = handle * MEM_PAGE_SIZE;
	LOG_DEBUG("API: AllocMemoryCommand(%d), handle=%d: %d bytes at %p (XMS/page allocator)",
	          bytes,
	          handle,
	          num_pages * MEM_PAGE_SIZE,
	          addr);
}

void AllocMemoryCommand::Execute()
{
	if (area < MemoryArea::Xms) {
		AllocDos();
	} else {
		AllocXms();
	}
}

void AllocMemoryCommand::Post(const httplib::Request& req, httplib::Response& res)
{
	auto j        = json::parse(req.body);
	uint32_t size = j.at("size");
	auto area     = MemoryArea::Conv;
	if (j.contains("area")) {
		std::string req_area = j["area"];
		upcase(req_area);
		if (req_area == "CONV") {
			area = MemoryArea::Conv;
		} else if (req_area == "UMA") {
			area = MemoryArea::Uma;
		} else if (req_area == "XMS") {
			area = MemoryArea::Xms;
		} else {
			throw std::invalid_argument("Invalid memory area: " +
			                            j["area"].dump());
		}
	}

	AllocMemoryCommand cmd(size, area);
	cmd.WaitForCompletion();

	if (cmd.addr) {
		json j;
		j["addr"] = cmd.addr;
		send_json(res, j);
	} else {
		res.status = httplib::StatusCode::ServiceUnavailable_503;
	}
}

void FreeMemoryCommand::Execute()
{
	if (addr < XMS_START * MEM_PAGE_SIZE) {
		success = DOS_FreeMemory(addr / DosBlockSize);
		LOG_DEBUG("API: FreeMemoryCommand(%p): success=%d (DOS allocator)",
		          addr,
		          success);
	} else {
		auto free_before = MEM_FreeTotal();
		MEM_ReleasePages(addr / MEM_PAGE_SIZE);
		auto released = static_cast<int64_t>(MEM_FreeTotal()) - free_before;
		success = released > 0;
		LOG_DEBUG("API: FreeMemoryCommand(%p): released=%d (page allocator)",
		          addr,
		          released);
	}
}

void FreeMemoryCommand::Post(const httplib::Request& req, httplib::Response& res)
{
	auto j        = json::parse(req.body);
	uint32_t addr = j.at("addr");

	FreeMemoryCommand cmd(addr);
	cmd.WaitForCompletion();

	if (!cmd.success) {
		res.status = httplib::StatusCode::BadRequest_400;
	}
}

} // namespace Webserver
