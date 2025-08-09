// SPDX-FileCopyrightText:  2024-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dosbox.h"

#include "cpu.h"
#include "fpu.h"
#include "hardware/memory.h"
#include "mmx.h"

MMX_reg* reg_mmx[8] = {
        &fpu.mmx_regs[0],
        &fpu.mmx_regs[1],
        &fpu.mmx_regs[2],
        &fpu.mmx_regs[3],
        &fpu.mmx_regs[4],
        &fpu.mmx_regs[5],
        &fpu.mmx_regs[6],
        &fpu.mmx_regs[7],
};

MMX_reg* lookupRMregMM[256] = {
        reg_mmx[0], reg_mmx[0], reg_mmx[0], reg_mmx[0],
        reg_mmx[0], reg_mmx[0], reg_mmx[0], reg_mmx[0],
        reg_mmx[1], reg_mmx[1], reg_mmx[1], reg_mmx[1],
        reg_mmx[1], reg_mmx[1], reg_mmx[1], reg_mmx[1],
        reg_mmx[2], reg_mmx[2], reg_mmx[2], reg_mmx[2],
        reg_mmx[2], reg_mmx[2], reg_mmx[2], reg_mmx[2],
        reg_mmx[3], reg_mmx[3], reg_mmx[3], reg_mmx[3],
        reg_mmx[3], reg_mmx[3], reg_mmx[3], reg_mmx[3],
        reg_mmx[4], reg_mmx[4], reg_mmx[4], reg_mmx[4],
        reg_mmx[4], reg_mmx[4], reg_mmx[4], reg_mmx[4],
        reg_mmx[5], reg_mmx[5], reg_mmx[5], reg_mmx[5],
        reg_mmx[5], reg_mmx[5], reg_mmx[5], reg_mmx[5],
        reg_mmx[6], reg_mmx[6], reg_mmx[6], reg_mmx[6],
        reg_mmx[6], reg_mmx[6], reg_mmx[6], reg_mmx[6],
        reg_mmx[7], reg_mmx[7], reg_mmx[7], reg_mmx[7],
        reg_mmx[7], reg_mmx[7], reg_mmx[7], reg_mmx[7],

        reg_mmx[0], reg_mmx[0], reg_mmx[0], reg_mmx[0],
        reg_mmx[0], reg_mmx[0], reg_mmx[0], reg_mmx[0],
        reg_mmx[1], reg_mmx[1], reg_mmx[1], reg_mmx[1],
        reg_mmx[1], reg_mmx[1], reg_mmx[1], reg_mmx[1],
        reg_mmx[2], reg_mmx[2], reg_mmx[2], reg_mmx[2],
        reg_mmx[2], reg_mmx[2], reg_mmx[2], reg_mmx[2],
        reg_mmx[3], reg_mmx[3], reg_mmx[3], reg_mmx[3],
        reg_mmx[3], reg_mmx[3], reg_mmx[3], reg_mmx[3],
        reg_mmx[4], reg_mmx[4], reg_mmx[4], reg_mmx[4],
        reg_mmx[4], reg_mmx[4], reg_mmx[4], reg_mmx[4],
        reg_mmx[5], reg_mmx[5], reg_mmx[5], reg_mmx[5],
        reg_mmx[5], reg_mmx[5], reg_mmx[5], reg_mmx[5],
        reg_mmx[6], reg_mmx[6], reg_mmx[6], reg_mmx[6],
        reg_mmx[6], reg_mmx[6], reg_mmx[6], reg_mmx[6],
        reg_mmx[7], reg_mmx[7], reg_mmx[7], reg_mmx[7],
        reg_mmx[7], reg_mmx[7], reg_mmx[7], reg_mmx[7],

        reg_mmx[0], reg_mmx[0], reg_mmx[0], reg_mmx[0],
        reg_mmx[0], reg_mmx[0], reg_mmx[0], reg_mmx[0],
        reg_mmx[1], reg_mmx[1], reg_mmx[1], reg_mmx[1],
        reg_mmx[1], reg_mmx[1], reg_mmx[1], reg_mmx[1],
        reg_mmx[2], reg_mmx[2], reg_mmx[2], reg_mmx[2],
        reg_mmx[2], reg_mmx[2], reg_mmx[2], reg_mmx[2],
        reg_mmx[3], reg_mmx[3], reg_mmx[3], reg_mmx[3],
        reg_mmx[3], reg_mmx[3], reg_mmx[3], reg_mmx[3],
        reg_mmx[4], reg_mmx[4], reg_mmx[4], reg_mmx[4],
        reg_mmx[4], reg_mmx[4], reg_mmx[4], reg_mmx[4],
        reg_mmx[5], reg_mmx[5], reg_mmx[5], reg_mmx[5],
        reg_mmx[5], reg_mmx[5], reg_mmx[5], reg_mmx[5],
        reg_mmx[6], reg_mmx[6], reg_mmx[6], reg_mmx[6],
        reg_mmx[6], reg_mmx[6], reg_mmx[6], reg_mmx[6],
        reg_mmx[7], reg_mmx[7], reg_mmx[7], reg_mmx[7],
        reg_mmx[7], reg_mmx[7], reg_mmx[7], reg_mmx[7],

        reg_mmx[0], reg_mmx[0], reg_mmx[0], reg_mmx[0],
        reg_mmx[0], reg_mmx[0], reg_mmx[0], reg_mmx[0],
        reg_mmx[1], reg_mmx[1], reg_mmx[1], reg_mmx[1],
        reg_mmx[1], reg_mmx[1], reg_mmx[1], reg_mmx[1],
        reg_mmx[2], reg_mmx[2], reg_mmx[2], reg_mmx[2],
        reg_mmx[2], reg_mmx[2], reg_mmx[2], reg_mmx[2],
        reg_mmx[3], reg_mmx[3], reg_mmx[3], reg_mmx[3],
        reg_mmx[3], reg_mmx[3], reg_mmx[3], reg_mmx[3],
        reg_mmx[4], reg_mmx[4], reg_mmx[4], reg_mmx[4],
        reg_mmx[4], reg_mmx[4], reg_mmx[4], reg_mmx[4],
        reg_mmx[5], reg_mmx[5], reg_mmx[5], reg_mmx[5],
        reg_mmx[5], reg_mmx[5], reg_mmx[5], reg_mmx[5],
        reg_mmx[6], reg_mmx[6], reg_mmx[6], reg_mmx[6],
        reg_mmx[6], reg_mmx[6], reg_mmx[6], reg_mmx[6],
        reg_mmx[7], reg_mmx[7], reg_mmx[7], reg_mmx[7],
        reg_mmx[7], reg_mmx[7], reg_mmx[7], reg_mmx[7],
};

void setFPUTagEmpty()
{
	fpu.tags[0] = TAG_Empty;
	fpu.tags[1] = TAG_Empty;
	fpu.tags[2] = TAG_Empty;
	fpu.tags[3] = TAG_Empty;
	fpu.tags[4] = TAG_Empty;
	fpu.tags[5] = TAG_Empty;
	fpu.tags[6] = TAG_Empty;
	fpu.tags[7] = TAG_Empty;
	fpu.tags[8] = TAG_Valid; // is only used by us
}
