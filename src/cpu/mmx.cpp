/*
 *  Copyright (C) 2024-2025  The DOSBox Staging Team
 *  Copyright (C) 2002-2021  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "dosbox.h"

#include "cpu.h"
#include "fpu.h"
#include "mem.h"
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
