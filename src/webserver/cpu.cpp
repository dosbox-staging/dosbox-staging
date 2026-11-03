// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "webserver.h"
#include "private/cpu.h"

using json = nlohmann::json;

namespace Webserver {

json cpu_registers_to_json(const CpuRegisters& registers)
{
	return {
	        {"eax", registers.eax},
	        {"ebx", registers.ebx},
	        {"ecx", registers.ecx},
	        {"edx", registers.edx},
	        {"esi", registers.esi},
	        {"edi", registers.edi},
	        {"ebp", registers.ebp},
	        {"esp", registers.esp},
	        {"eip", registers.eip},
	        {"flags", registers.flags},
	        {"cs", registers.cs},
	        {"ds", registers.ds},
	        {"es", registers.es},
	        {"ss", registers.ss},
	        {"fs", registers.fs},
	        {"gs", registers.gs},
	};
}

void CpuRegistersCommand::Execute()
{
	registers = CPU_CaptureRegisters();
}

void CpuRegistersCommand::Get(const httplib::Request&, httplib::Response& res)
{
	CpuRegistersCommand command = {};
	command.WaitForCompletion();
	send_json(res, json{{"registers", cpu_registers_to_json(command.registers)}});
}

} // namespace Webserver
