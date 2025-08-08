// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "cpu/string_ops.h"

#include <cassert>

static void dyn_string(STRING_OP op) {
	DynReg * si_base=decode.segprefix ? decode.segprefix : DREG(DS);
	DynReg * di_base=DREG(ES);
	DynReg * tmp_reg;bool usesi;bool usedi;
	gen_protectflags();
	if (decode.rep) {
		gen_dop_word_imm(DOP_SUB,true,DREG(CYCLES),decode.cycles);
		gen_releasereg(DREG(CYCLES));
		decode.cycles=0;
	}
	/* Check what each string operation will be using */
	switch (op) {
	case R_MOVSB:	case R_MOVSW:	case R_MOVSD:
	case R_CMPSB:	case R_CMPSW:	case R_CMPSD:
		tmp_reg=DREG(TMPB);usesi=true;usedi=true;break;
	case R_LODSB:	case R_LODSW:	case R_LODSD:
		tmp_reg=DREG(EAX);usesi=true;usedi=false;break;
	case R_OUTSB:	case R_OUTSW:	case R_OUTSD:
		tmp_reg=DREG(TMPB);usesi=true;usedi=false;break;
	case R_SCASB:	case R_SCASW:	case R_SCASD:
	case R_STOSB:	case R_STOSW:	case R_STOSD:
		tmp_reg=DREG(EAX);usesi=false;usedi=true;break;
	case R_INSB:	case R_INSW:	case R_INSD:
		tmp_reg=DREG(TMPB);usesi=false;usedi=true;break;
	default:
		IllegalOption("dyn_string op");
	}
	gen_load_host(&cpu.direction,DREG(TMPW),4);
	switch (op & 3) {
	case 0:break;
	case 1:gen_shift_word_imm(SHIFT_SHL,true,DREG(TMPW),1);break;
	case 2:gen_shift_word_imm(SHIFT_SHL,true,DREG(TMPW),2);break;
	default:
		IllegalOption("dyn_string shift");

	}
	if (usesi) {
		gen_preloadreg(DREG(ESI));
		DynRegs[G_ESI].flags|=DYNFLG_CHANGED;
		gen_preloadreg(si_base);
	}
	if (usedi) {
		gen_preloadreg(DREG(EDI));
		DynRegs[G_EDI].flags|=DYNFLG_CHANGED;
		gen_preloadreg(di_base);
	}
	if (decode.rep) {
		gen_preloadreg(DREG(ECX));
		DynRegs[G_ECX].flags|=DYNFLG_CHANGED;
	}
	DynState rep_state;
	dyn_savestate(&rep_state);
	const uint8_t * rep_start=cache.pos;

	/* Check if ECX!=zero */
	if (decode.rep) {
		gen_dop_word(DOP_TEST, decode.big_addr, DREG(ECX), DREG(ECX));
	}
	const uint8_t *rep_ecx_jmp = decode.rep ? gen_create_branch_long(BR_Z)
	                                        : nullptr;
	if (usesi) {
		if (!decode.big_addr) {
			gen_extend_word(false,DREG(EA),DREG(ESI));
			gen_lea(DREG(EA),si_base,DREG(EA),0,0);
		} else {
			gen_lea(DREG(EA),si_base,DREG(ESI),0,0);
		}
		switch (op&3) {
		case 0:dyn_read_byte(DREG(EA),tmp_reg,false);break;
		case 1:dyn_read_word(DREG(EA),tmp_reg,false);break;
		case 2:dyn_read_word(DREG(EA),tmp_reg,true);break;
		}
		switch (op) {
		case R_OUTSB:
			gen_call_function((void*)&IO_WriteB,"%Dw%Dl",DREG(EDX),tmp_reg);break;
		case R_OUTSW:
			gen_call_function((void*)&IO_WriteW,"%Dw%Dw",DREG(EDX),tmp_reg);break;
		case R_OUTSD:
			gen_call_function((void*)&IO_WriteD,"%Dw%Dd",DREG(EDX),tmp_reg);break;
		default:
			break;
		}
	}
	if (usedi) {
		if (!decode.big_addr) {
			gen_extend_word(false,DREG(EA),DREG(EDI));
			gen_lea(DREG(EA),di_base,DREG(EA),0,0);
		} else {
			gen_lea(DREG(EA),di_base,DREG(EDI),0,0);
		}
		/* Maybe something special to be done to fill the value */
		switch (op) {
		case R_INSB:
			gen_call_function((void*)&IO_ReadB,"%Dw%Rl",DREG(EDX),tmp_reg);
			[[fallthrough]];
		case R_MOVSB:
		case R_STOSB:
			dyn_write_byte(DREG(EA),tmp_reg,false);
			break;
		case R_INSW:
			gen_call_function((void*)&IO_ReadW,"%Dw%Rw",DREG(EDX),tmp_reg);
			[[fallthrough]];
		case R_MOVSW:
		case R_STOSW:
			dyn_write_word(DREG(EA),tmp_reg,false);
			break;
		case R_INSD:
			gen_call_function((void*)&IO_ReadD,"%Dw%Rd",DREG(EDX),tmp_reg);
			[[fallthrough]];
		case R_MOVSD:
		case R_STOSD:
			dyn_write_word(DREG(EA),tmp_reg,true);
			break;
		default:
			IllegalOption("dyn_string op");
		}
	}
	gen_releasereg(DREG(EA));gen_releasereg(DREG(TMPB));

	/* update registers */
	if (usesi) gen_dop_word(DOP_ADD,decode.big_addr,DREG(ESI),DREG(TMPW));
	if (usedi) gen_dop_word(DOP_ADD,decode.big_addr,DREG(EDI),DREG(TMPW));

	if (decode.rep) {
		gen_sop_word(SOP_DEC,decode.big_addr,DREG(ECX));
		gen_sop_word(SOP_DEC,true,DREG(CYCLES));
		gen_releasereg(DREG(CYCLES));
		dyn_savestate(&save_info[used_save_info].state);
		save_info[used_save_info].branch_pos=gen_create_branch_long(BR_LE);
		save_info[used_save_info].eip_change=decode.op_start-decode.code_start;
		save_info[used_save_info].type=normal;
		used_save_info++;

		/* Jump back to start of ECX check */
		dyn_synchstate(&rep_state);
		gen_create_jump(rep_start);

		dyn_loadstate(&rep_state);
		assert(rep_ecx_jmp);
		gen_fill_branch_long(rep_ecx_jmp);
	}
	gen_releasereg(DREG(TMPW));
}
