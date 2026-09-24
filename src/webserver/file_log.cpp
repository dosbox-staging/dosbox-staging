// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "file_log.h"
#include "private/timestamp.h"

#include <cstdio>
#include <string>

#include "json/json.h"

#include "cpu/cpu.h"
#include "cpu/registers.h"
#include "dos/dos.h"
#include "dos/dos_system.h"
#include "hardware/memory.h"
#include "misc/logging.h"
#include "misc/support.h"
#include "utils/checks.h"

CHECK_NARROWING();

namespace FileLog {

namespace {

enum Function : uint8_t {
	Create       = 0x3c,
	Open         = 0x3d,
	Close        = 0x3e,
	Read         = 0x3f,
	Write        = 0x40,
	Seek         = 0x42,
	ExtendedOpen = 0x6c,
};

FILE_unique_ptr log_file = {};
uint64_t sequence        = 0;

const char* op_name(const uint8_t function)
{
	switch (function) {
	case Create: return "create";
	case Open: return "open";
	case Close: return "close";
	case Read: return "read";
	case Write: return "write";
	case Seek: return "seek";
	case ExtendedOpen: return "open_ext";
	default: return "other";
	}
}

std::string name_at(const PhysPt address)
{
	// Same limit as the INT 21h handler's DOSNAMEBUF
	constexpr auto MaxName = 256;
	char name[MaxName + 1] = {};
	MEM_StrCopy(address, name, MaxName);
	return name;
}

DOS_File* file_of(const uint16_t handle)
{
	const auto real_handle = RealHandle(handle);
	if (real_handle >= DOS_FILES) {
		return nullptr;
	}
	return Files[real_handle].get();
}

} // namespace

void Init(const std::string& path)
{
	if (path.empty()) {
		return;
	}
	log_file = make_fopen(path.c_str(), "a");
	if (log_file) {
		LOG_MSG("WEBSERVER: Logging DOS file calls to '%s'", path.c_str());
	} else {
		LOG_WARNING("WEBSERVER: Can't open DOS file call log '%s'",
		            path.c_str());
	}
}

std::optional<Call> Begin()
{
	if (!log_file) {
		return {};
	}
	const auto function = reg_ah;
	switch (function) {
	case Create:
	case Open:
	case Close:
	case Read:
	case Write:
	case Seek:
	case ExtendedOpen: break;
	default: return {};
	}

	Call call        = {};
	call.function    = function;
	call.subfunction = reg_al;
	call.handle      = reg_bx;
	call.length      = reg_cx;
	call.buffer      = SegPhys(SegNames::ds) + reg_dx;

	if (function == Create || function == Open) {
		call.file = name_at(SegPhys(SegNames::ds) + reg_dx);
	} else if (function == ExtendedOpen) {
		call.file = name_at(SegPhys(SegNames::ds) + reg_si);
	} else if (auto file = file_of(reg_bx)) {
		call.file                     = file->GetName();
		constexpr uint16_t DeviceFlag = 0x80;
		const auto is_device = (file->GetInformation() & DeviceFlag) != 0;
		if (!is_device && (function == Read || function == Write)) {
			uint32_t position = 0;
			if (file->Seek(&position, DOS_SEEK_CUR)) {
				call.position = position;
			}
		}
	}
	return call;
}

void End(const std::optional<Call>& call)
{
	if (!call || !log_file) {
		return;
	}
	// DOS calls report failure by setting CF in the flags that IRET will
	// restore, which CALLBACK_SCF() keeps on the stack at SS:SP+4.
	const auto flags = real_readw(SegValue(SegNames::ss), reg_sp + 4);
	const auto ok    = (flags & FLAG_CF) == 0;

	nlohmann::json j;
	j["seq"]  = sequence++;
	j["t"]    = Webserver::UtcTimestamp();
	j["op"]   = op_name(call->function);
	j["file"] = call->file;
	j["ok"]   = ok;

	switch (call->function) {
	case Create:
	case Open:
	case ExtendedOpen:
		if (call->function == Create) {
			j["attributes"] = call->length; // CX
		} else {
			j["mode"] = call->subfunction; // AL
		}
		if (ok) {
			j["handle"] = reg_ax;
		}
		break;
	case Close: j["handle"] = call->handle; break;
	case Read:
	case Write:
		j["handle"]    = call->handle;
		j["buffer"]    = call->buffer;
		j["requested"] = call->length;
		if (call->position) {
			j["position"] = *call->position;
		}
		if (ok) {
			j["done"] = reg_ax;
		}
		break;
	case Seek:
		j["handle"] = call->handle;
		j["whence"] = call->subfunction;
		if (ok) {
			j["position"] = (static_cast<uint32_t>(reg_dx) << 16) | reg_ax;
		}
		break;
	default: break;
	}
	if (!ok) {
		j["error"] = reg_ax;
	}

	const auto line = j.dump() + "\n";
	fwrite(line.data(), 1, line.size(), log_file.get());
	fflush(log_file.get());
}

} // namespace FileLog
