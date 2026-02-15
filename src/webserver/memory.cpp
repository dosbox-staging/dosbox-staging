// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "webserver/memory.h"
#include "webserver/bridge.h"
#include "webserver/webserver.h"

#include "libs/base64/base64.h"
#include "libs/http/http.h"
#include "libs/json/json.h"
#include "utils/string_utils.h"

#include "cpu/registers.h"
#include "dos/dos_memory.h"

using json = nlohmann::json;
using httplib::Request, httplib::Response;

namespace Webserver {

static Segment str_to_base_segment(const std::string_view str)
{
	auto val = upcase(str);
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
		return Segment::None;
	}
}

static uint32_t base_segment_to_offset(const Segment segment)
{
	switch (segment) {
	case Segment::CS:
		return SegPhys(SegNames::cs);
	case Segment::SS:
		return SegPhys(SegNames::ss);
	case Segment::DS:
		return SegPhys(SegNames::ds);
	case Segment::ES:
		return SegPhys(SegNames::es);
	case Segment::FS:
		return SegPhys(SegNames::fs);
	case Segment::GS:
		return SegPhys(SegNames::gs);
	case Segment::None:
		return 0;
	default:
		return 0;
	}
}

static void parse_mem_addr(const httplib::Request& req, Segment& segment,
                           uint32_t& offset)
{
	offset  = num_param<uint32_t>(req, Source::Path, "offset");
	segment = Segment::None;
	if (req.path_params.find("segment") != req.path_params.end()) {
		auto& segment_param = req.path_params.at("segment");
		segment             = str_to_base_segment(segment_param);
		// Segment can either be a register to resolve later or an
		// address which we can already resolve here.
		if (segment == Segment::None) {
			const auto seg_addr = PhysicalMake(
			        num_param<uint16_t>(req, Source::Path, "segment"), 0);
			offset += seg_addr;
		}
	}
}

void ReadMemCommand::Execute()
{
	regs.load();
	effective_addr = base_segment_to_offset(base) + offset;
	LOG_DEBUG("API: ReadMemCommand(0x%06x, %d)", effective_addr, len);

	memory.resize(len);
	MEM_BlockRead(effective_addr, memory.data(), len);
}

void ReadMemCommand::Get(const Request& req, Response& res)
{
	// 128 MiB per request ought to be enough for everyone.
	// This limit just prevents bad things when accidentally requesting an
	// unreasonably large size.
	auto num_bytes = num_param<uint32_t>(req, Source::Path, "len", 1, 128 * 1024 * 1024);
	Segment segment;
	uint32_t offset;
	parse_mem_addr(req, segment, offset);

	ReadMemCommand cmd(segment, offset, num_bytes);
	cmd.WaitForCompletion();

	if (req.get_header_value("accept").starts_with(TypeJson)) {
		json j;
		j["registers"]      = cmd.regs;
		j["memory"]["addr"] = cmd.effective_addr;
		j["memory"]["data"] = base64::to_base64(cmd.memory);
		send_json(res, j);
	} else {
		// Only do base64 if explicitly requested, binary/download by
		// default.
		res.set_header("Content-Disposition",
		               "attachment; filename =\"memory.bin\"");
		res.set_content(cmd.memory, TypeBinary);
	}
}

void WriteMemCommand::Execute()
{
	effective_addr = base_segment_to_offset(base) + offset;
	LOG_DEBUG("API: WriteMemCommand(0x%06x, %d)", effective_addr, data.size());

	if (!expected_data.empty()) {
		conflict_data.resize(expected_data.size());
		MEM_BlockRead(effective_addr,
		              conflict_data.data(),
		              conflict_data.size());
		if (expected_data != conflict_data) {
			return;
		}
		conflict_data.clear();
	}
	MEM_BlockWrite(effective_addr, data.data(), data.size());
}

void WriteMemCommand::Put(const httplib::Request& req, httplib::Response& res)
{
	Segment segment;
	uint32_t offset;
	parse_mem_addr(req, segment, offset);

	std::string data;
	if (req.get_header_value("Content-Type") == TypeJson) {
		auto j = json::parse(req.body);
		data   = base64::from_base64(j.at("data").get<std::string>());
	} else if (req.get_header_value("Content-Type") == TypeBinary) {
		data = req.body;
	} else {
		throw std::invalid_argument("Content-Type must be either " +
		                            std::string(TypeJson) + " or " +
		                            std::string(TypeBinary));
	}

	std::string expected_data;
	if (req.has_header("If-Match")) {
		// The standard requires ETags in this and ETags are quoted
		// but we accept unquoted because no one is gonna bother
		std::string etag_hdr  = req.get_header_value("If-Match");
		std::string_view etag = etag_hdr;
		if (etag.starts_with('"') && etag.ends_with('"')) {
			etag.remove_prefix(1);
			etag.remove_suffix(1);
		}
		expected_data = base64::from_base64(etag);
	}

	WriteMemCommand cmd(segment, offset, std::move(data), std::move(expected_data));
	cmd.WaitForCompletion();

	json j;
	j["memory"]["addr"] = cmd.effective_addr;
	if (!cmd.conflict_data.empty()) {
		res.status = httplib::StatusCode::PreconditionFailed_412;
		j["memory"]["data"] = base64::to_base64(cmd.conflict_data);
	}
	send_json(res, j);
}

} // namespace Webserver
