#ifndef DOSBOX_CPU_CONFIG_PARAM_H
#define DOSBOX_CPU_CONFIG_PARAM_H

#include <string>
#include <iostream>

#include <gtest/gtest.h>

namespace {

typedef Bits (*CpuRunner)(void);

struct CpuConfig {
	CpuRunner runner;
	std::string test_name;
	std::string config_cpu;
	std::string config_cpu_type = "auto";
};

std::ostream& operator<<(std::ostream& os, const CpuConfig& cfg)
{
	return os << cfg.test_name;
}

const auto AllCpuConfigs = testing::Values(
#if (C_DYNREC)
        CpuConfig{CPU_Core_Dynrec_Run, "Dynrec", "dynamic"},
#endif
#if (C_DYNAMIC_X86)
        CpuConfig{CPU_Core_Dyn_X86_Run, "Dyn_X86", "dynamic"},
#endif
        CpuConfig{CPU_Core_Normal_Run, "Normal", "normal"},
        CpuConfig{CPU_Core_Simple_Run, "Simple", "simple"},
        CpuConfig{CPU_Core_Full_Run, "Full", "full"},
        CpuConfig{CPU_Core_Prefetch_Run, "Prefetch", "normal", "386_prefetch"});

} // namespace tests

#endif
