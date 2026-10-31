// SPDX-FileCopyrightText: 2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later
#include "cpu/cpu.h"
#include "dosbox.h"
#include "fpu.h"
#include "misc/cross.h"
#include "utils/math_utils.h"
#include <bit>
#include <cmath>

namespace fast_fpu {
#include "fpu_instructions_fast.h"
}
namespace accurate_fpu {
#include "fpu_instructions.h"
}

namespace {
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
}

bool FPU_IsAccurate()
{
	return using_accurate_fpu;
}
