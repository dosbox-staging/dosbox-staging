#include "webserver/memory.h"
#include "webserver/bridge.h"
#include "webserver/webserver.h"

#include "libs/base64/base64.h"
#include "libs/http/httplib.h"
#include "libs/json/json.h"

#include "cpu/registers.h"
#include "dos/dos_memory.h"

using json = nlohmann::json;
using httplib::Request, httplib::Response;

namespace Webserver {

static auto upper(std::string_view str)
{
	auto v = str | std::views::transform([](unsigned char c) {
		         return static_cast<char>(std::toupper(c));
	         });
	return std::string(v.begin(), v.end());
}

static Segment str_to_base_segment(const std::string_view& str)
{
	auto val = upper(str);
	static const std::unordered_map<std::string_view, Segment> lookup = {
	        {"CS", Segment::CS},
	        {"SS", Segment::SS},
	        {"DS", Segment::DS},
	        {"ES", Segment::ES},
	        {"FS", Segment::FS},
	        {"GS", Segment::GS},
	};

	if (auto it = lookup.find(val); it != lookup.end()) {
		return it->second;
	} else {
		return Segment::NONE;
	}
}

static uint32_t base_segment_to_offset(Segment segment)
{
	uint32_t addr;
	switch (segment) {
	case Segment::CS: {
		addr = SegPhys(SegNames::cs);
		break;
	}
	case Segment::SS: {
		addr = SegPhys(SegNames::ss);
		break;
	}
	case Segment::DS: {
		addr = SegPhys(SegNames::ds);
		break;
	}
	case Segment::ES: {
		addr = SegPhys(SegNames::es);
		break;
	}
	case Segment::FS: {
		addr = SegPhys(SegNames::fs);
		break;
	}
	case Segment::GS: {
		addr = SegPhys(SegNames::gs);
		break;
	}
	default: {
		addr = 0;
		break;
	}
	}
	return addr;
}

static void parse_mem_addr(const httplib::Request& req, Segment& segment,
                           uint32_t& offset)
{
	offset  = num_param<uint32_t>(req, Source::Path, "offset");
	segment = Segment::NONE;
	if (req.path_params.find("segment") != req.path_params.end()) {
		auto& segment_param = req.path_params.at("segment");
		segment             = str_to_base_segment(segment_param);
		if (segment == Segment::NONE) {
			offset += num_param<uint16_t>(req, Source::Path, "segment") *
			          16;
		}
	}
}

void ReadMemCommand::Execute()
{
	regs.load();
	effective_addr = base_segment_to_offset(base) + offset;
	LOG_DEBUG("API: WriteMemCommand(0x%06x, %d)", effective_addr, len);
	memory.resize(len);
	MEM_BlockRead(effective_addr, memory.data(), len);
}

void ReadMemCommand::Get(const Request& req, Response& res)
{
	auto len = num_param<uint32_t>(req, Source::Path, "len", 1, 128 * 1024 * 1024);
	Segment segment;
	uint32_t offset;
	parse_mem_addr(req, segment, offset);
	ReadMemCommand cmd(segment, offset, len);
	cmd.WaitForCompletion();
	auto accept = req.get_header_value("accept");
	if (accept.starts_with("application/json")) {
		json j;
		j["registers"]      = cmd.regs;
		j["memory"]["addr"] = cmd.effective_addr;
		j["memory"]["data"] = base64::to_base64(std::string_view(
		        reinterpret_cast<const char*>(cmd.memory.data()),
		        cmd.memory.size()));
		res.set_content(j.dump(2), "text/json");
	} else {
		res.set_header("Content-Disposition",
		               "attachment; filename =\"memory.bin\"");
		res.set_content(reinterpret_cast<const char*>(cmd.memory.data()),
		                cmd.memory.size(),
		                "application/octet-stream");
	}
}

void WriteMemCommand::Execute()
{
	effective_addr = base_segment_to_offset(base) + offset;
	LOG_DEBUG("API: WriteMemCommand(0x%06x, %d)", effective_addr, data.size());
	MEM_BlockWrite(effective_addr, data.data(), data.size());
}

void WriteMemCommand::Put(const httplib::Request& req, httplib::Response&)
{
	Segment segment;
	uint32_t offset;
	parse_mem_addr(req, segment, offset);
	std::string data;
	if (req.get_header_value("Content-Type") == "application/json") {
		auto j = json::parse(req.body);
		data   = base64::from_base64(j["data"]);
	} else if (req.get_header_value("Content-Type") == "application/octet-stream") {
		data = req.body;
	} else {
		throw std::invalid_argument(
		        "Content-Type must be either application/json or application/octet-stream");
	}
	WriteMemCommand cmd(segment, offset, data);
	cmd.WaitForCompletion();
}

} // namespace Webserver
