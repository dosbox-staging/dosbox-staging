#include "dos.h"
#include "bridge.h"
#include "webserver.h"

#include "libs/http/httplib.h"
#include "libs/json/json.h"

#include "cpu/registers.h"
#include "dos/dos.h"
#include "dos/dos_memory.h"

using json = nlohmann::json;

namespace Webserver {

void DosInfoCommand::Execute()
{
	lol         = RealToPhysical(dos_infoblock.GetPointer());
	sda         = PhysicalMake(DOS_SDA_SEG, DOS_SDA_OFS);
	first_shell = PhysicalMake(DOS_FIRST_SHELL, 0);
	LOG_DEBUG("API: DosInfoCommand()");
}

void DosInfoCommand::Get(const httplib::Request&, httplib::Response& res)
{
	DosInfoCommand cmd;
	cmd.WaitForCompletion();
	json j;
	j["lol"]        = cmd.lol;
	j["sda"]        = cmd.sda;
	j["firstShell"] = cmd.first_shell;
	j["version"]    = DOSBOX_GetDetailedVersion();
	res.set_content(j.dump(2), "text/json");
}

void AllocMemoryCommand::Execute()
{
	uint16_t blocks = bytes / 16;
	auto old_strat  = DOS_GetMemAllocStrategy();
	DOS_SetMemAllocStrategy(area == MemoryArea::CONV ? 0x00 : 0x40);
	uint16_t segment = 0;
	auto ok          = DOS_AllocateMemory(&segment, &blocks);
	LOG_DEBUG("API: AllocMemoryCommand(%d) = %d: %d bytes at %p",
	          bytes,
	          ok,
	          blocks * 16,
	          segment * 16);
	DOS_SetMemAllocStrategy(old_strat);
	if (!ok) {
		segment = 0;
	} else if (blocks < bytes / 16) {
		DOS_FreeMemory(segment);
		segment = 0;
	} else {
		addr = segment * 16;
	}
}

void AllocMemoryCommand::Post(const httplib::Request& req, httplib::Response& res)
{
	auto j        = json::parse(req.body);
	uint32_t size = j["size"];
	auto area     = MemoryArea::CONV;
	if (j.contains("area")) {
		if (j["area"] == "UMA") {
			area = MemoryArea::UMA;
		} else if (j["area"] != "conv") {
			throw std::invalid_argument("Invalid memory area: " +
			                            j["area"].dump());
		}
	}
	AllocMemoryCommand cmd(size, area);
	cmd.WaitForCompletion();
	if (cmd.addr) {
		json j;
		j["addr"] = cmd.addr;
		res.set_content(j.dump(2), "text/json");
	} else {
		res.status = httplib::StatusCode::ServiceUnavailable_503;
	}
}

void FreeMemoryCommand::Execute()
{
	success = DOS_FreeMemory(addr / 16);
	LOG_DEBUG("API: FreeMemoryCommand(%p) = %d", addr, success);
}

void FreeMemoryCommand::Post(const httplib::Request& req, httplib::Response& res)
{
	auto j        = json::parse(req.body);
	uint32_t addr = j["addr"];
	FreeMemoryCommand cmd(addr);
	cmd.WaitForCompletion();
	if (!cmd.success) {
		res.status = httplib::StatusCode::BadRequest_400;
	}
}

} // namespace Webserver
