/*
 *  Copyright (C) 2024-2024  The DOSBox Staging Team
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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "mmx.h"

extern uint32_t * lookupRMEAregd[256];

#define LoadMd(off) mem_readd_inline(off)
#define LoadMq(off) ((uint64_t)((uint64_t)mem_readd_inline(off+4)<<32 | (uint64_t)mem_readd_inline(off)))
#define SaveMd(off,val)	mem_writed_inline(off,val)
#define SaveMq(off,val) {mem_writed_inline(off,val&0xffffffff);mem_writed_inline(off+4,(val>>32)&0xffffffff);}

#define CASE_0F_MMX(opcode) case(opcode):
#define GetRM 
#define GetEAa
#define Fetchb() imm
#define GetEArd	uint32_t * eard=lookupRMEAregd[rm];

static void gen_mmx_op(Bitu op, Bitu rm, Bitu imm = 0, PhysPt eaa = 0) {
	switch (op)
	{
#include "../core_normal/prefix_0f_mmx.h"
	default:
		break;
	}
illegal_opcode:
	return;
}

static void dyn_mmx_op(Bitu op) {
	Bitu imm = 0;
	dyn_get_modrm();

	if (decode.modrm.mod < 3) {
		dyn_fill_ea();
		gen_call_function((void*)&gen_mmx_op, "%I%I%I%Ddr", op, decode.modrm.val, imm, DREG(EA));
	}
	else {
		if ((op == 0x71) || (op == 0x72) || (op == 0x73)) 
			decode_fetchb_imm(imm);
		gen_call_function((void*)&gen_mmx_op, "%I%I%I", op, decode.modrm.val, imm);
	}
}

static void dyn_mmx_emms() {
	gen_call_function((void*)&setFPUTagEmpty, "");
}
