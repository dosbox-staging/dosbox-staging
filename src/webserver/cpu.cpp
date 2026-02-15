// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "cpu.h"
#include "bridge.h"
#include "webserver.h"

#include "libs/base64/base64.h"
#include "libs/http/http.h"
#include "libs/json/json.h"

#include "cpu/registers.h"

using json = nlohmann::json;

namespace Webserver {

void Registers::load()
{
	this->eax   = reg_eax;
	this->ebx   = reg_ebx;
	this->ecx   = reg_ecx;
	this->edx   = reg_edx;
	this->esi   = reg_esi;
	this->edi   = reg_edi;
	this->esp   = reg_esp;
	this->ebp   = reg_ebp;
	this->eip   = reg_eip;
	this->flags = reg_flags;
	this->cs    = SegValue(SegNames::cs);
	this->ds    = SegValue(SegNames::ds);
	this->es    = SegValue(SegNames::es);
	this->ss    = SegValue(SegNames::ss);
	this->fs    = SegValue(SegNames::fs);
	this->gs    = SegValue(SegNames::gs);
}

void CpuInfoCommand::Execute()
{
	regs.load();
	LOG_DEBUG("API: CpuInfoCommand()");
}

void CpuInfoCommand::Get(const httplib::Request&, httplib::Response& res)
{
	CpuInfoCommand cmd;
	cmd.WaitForCompletion();

	json j;
	j["registers"] = cmd.regs;
	send_json(res, j);
}

} // namespace Webserver
