// SPDX-FileCopyrightText: 2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later
// Aliases used by the interpreter and dynamic recompiler. Generated blocks
// capture the selected function pointer directly, without a runtime mode branch.
#define FPU_OPERATION(name, args) static auto& name = fpu_operations.name;
#include "fpu_operations.h"
#undef FPU_OPERATION
