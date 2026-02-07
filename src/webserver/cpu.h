#pragma once

#include "bridge.h"

#include "libs/http/httplib.h"
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
// This could be done with reflection, but with msvc (as of version 19.50.35723)
// just keeps running into random internal errors and crashes...
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Registers, eax, ebx, ecx, edx, esi, edi, esp,
                                   ebp, eip, flags, cs, ds, es, ss, fs, gs)

class CpuInfoCommand : public DebugCommand {
	Registers regs = {};

public:
	void Execute() override;
	static void Get(const httplib::Request&, httplib::Response&);
};

} // namespace Webserver
