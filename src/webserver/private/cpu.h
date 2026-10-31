// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_WEBSERVER_CPU_H
#define DOSBOX_WEBSERVER_CPU_H

#include "cpu/cpu_snapshot.h"
#include "webserver/bridge.h"

#include "http/http.h"
#include "json/json.h"

namespace Webserver {

nlohmann::json cpu_registers_to_json(const CpuRegisters& registers);

class CpuRegistersCommand final : public Command {
public:
	void Execute() override;
	static void Get(const httplib::Request&, httplib::Response&);

private:
	CpuRegisters registers = {};
};

} // namespace Webserver

#endif // DOSBOX_WEBSERVER_CPU_H
