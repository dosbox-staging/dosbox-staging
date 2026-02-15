// SPDX-FileCopyrightText:  2026-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_WEBSERVER_CPU_H
#define DOSBOX_WEBSERVER_CPU_H

#include "bridge.h"

#include "libs/http/http.h"
#include "libs/json/json.h"

namespace Webserver {

struct Registers {
	uint32_t eax   = 0;
	uint32_t ebx   = 0;
	uint32_t ecx   = 0;
	uint32_t edx   = 0;
	uint32_t esi   = 0;
	uint32_t edi   = 0;
	uint32_t esp   = 0;
	uint32_t ebp   = 0;
	uint32_t eip   = 0;
	uint32_t flags = 0;
	uint16_t cs    = 0;
	uint16_t ds    = 0;
	uint16_t es    = 0;
	uint16_t ss    = 0;
	uint16_t fs    = 0;
	uint16_t gs    = 0;

	void load();
};
// This could be done with reflection, but msvc (as of version 19.50.35723)
// just keeps running into random internal errors and crashes...
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Registers, eax, ebx, ecx, edx, esi, edi, esp,
                                   ebp, eip, flags, cs, ds, es, ss, fs, gs)

class CpuInfoCommand : public DebugCommand {
public:
	void Execute() override;
	static void Get(const httplib::Request&, httplib::Response&);

private:
	Registers regs = {};
};

} // namespace Webserver

#endif // DOSBOX_WEBSERVER_CPU_H