// SPDX-FileCopyrightText: 2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later
#include "cpu/cpu.h"
#include "dosbox.h"
#include "fpu/fpu.h"
#include "misc/cross.h"
#include "misc/logging.h"
#include "utils/math_utils.h"
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdlib>

namespace fast_fpu {
#include "fpu_instructions_fast.h"
}
namespace accurate_fpu {
#include "fpu_instructions.h"
}

namespace {
// Opt-in sampled profiling. Time approximately one in 1024 calls; count all calls.
struct FpuProfileEntry {
	const char* name;
	uint64_t calls = 0, samples = 0, ns = 0;
};
FpuProfileEntry fpu_profile[] = {
#define FPU_OPERATION(name, args) {#name},
#include "fpu_operations.h"
#undef FPU_OPERATION
};
enum {
#define FPU_OPERATION(name, args) profile_##name,
#include "fpu_operations.h"
#undef FPU_OPERATION
};
using ProfileClock = std::chrono::steady_clock;
auto profile_last  = ProfileClock::now();
void dump_fpu_profile()
{
	LOG_INFO("FPU PROFILE: cumulative counts; 1/1024 sampled timings (ns include clock overhead)");
	for (const auto& e : fpu_profile) {
		if (e.calls) {
			LOG_INFO("FPU PROFILE: %s calls=%llu samples=%llu ns=%llu",
			         e.name,
			         static_cast<unsigned long long>(e.calls),
			         static_cast<unsigned long long>(e.samples),
			         static_cast<unsigned long long>(e.ns));
		}
	}
}
template <auto Function, int Index>
struct Profiled;
template <int Index, typename... Args, void (*Function)(Args...)>
struct Profiled<Function, Index> {
	static void call(Args... args)
	{
		auto& e = fpu_profile[Index];
		// Spread samples across recurring guest loops rather than
		// selecting the same call every 1024 iterations.
		if (((++e.calls * UINT64_C(11400714819323198485)) >> 54) != 0) {
			Function(args...);
			return;
		}
		const auto start = ProfileClock::now();
		Function(args...);
		const auto end = ProfileClock::now();
		e.ns += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
		                .count();
		++e.samples;
		if (end - profile_last > std::chrono::seconds(5)) {
			profile_last = end;
			dump_fpu_profile();
		}
	}
};
const FPU_Operations profiled_fast_operations = {
#define FPU_OPERATION(name, args) \
	Profiled<fast_fpu::name, profile_##name>::call,
#include "fpu_operations.h"
#undef FPU_OPERATION
};
const FPU_Operations profiled_accurate_operations = {
#define FPU_OPERATION(name, args) \
	Profiled<accurate_fpu::name, profile_##name>::call,
#include "fpu_operations.h"
#undef FPU_OPERATION
};
const FPU_Operations fast_operations = {
#define FPU_OPERATION(name, args) fast_fpu::name,
#include "fpu_operations.h"
#undef FPU_OPERATION
};
const FPU_Operations accurate_operations = {
#define FPU_OPERATION(name, args) accurate_fpu::name,
#include "fpu_operations.h"
#undef FPU_OPERATION
};
bool using_accurate_fpu = false;
} // namespace

FPU_Operations fpu_operations = fast_operations;

void FPU_SelectBackend(const bool accurate)
{
	// Startup only: CPU caches have been reset before FPU_Init is called.
	using_accurate_fpu = accurate;
	fpu_operations     = accurate ? accurate_operations : fast_operations;
	const auto profile = std::getenv("DOSBOX_FPU_PROFILE");
	if (profile && profile[0] == '1' && profile[1] == '\0') {
		fpu_operations = accurate ? profiled_accurate_operations
		                          : profiled_fast_operations;
		static bool exit_handler_registered = false;
		if (!exit_handler_registered) {
			std::atexit(dump_fpu_profile);
			exit_handler_registered = true;
		}
	}
}

bool FPU_IsAccurate()
{
	return using_accurate_fpu;
}
