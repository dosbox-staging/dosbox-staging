/*
 *  Copyright (C) 2002-2005  The DOSBox Team
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

enum STRING_OP {
	STR_OUTSB=0,STR_OUTSW,STR_OUTSD,
	STR_INSB=4,STR_INSW,STR_INSD,
	STR_MOVSB=8,STR_MOVSW,STR_MOVSD,
	STR_LODSB=12,STR_LODSW,STR_LODSD,
	STR_STOSB=16,STR_STOSW,STR_STOSD,
	STR_SCASB=20,STR_SCASW,STR_SCASD,
	STR_CMPSB=24,STR_CMPSW,STR_CMPSD,
};

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
	case STR_MOVSB:	case STR_MOVSW:	case STR_MOVSD:
	case STR_CMPSB:	case STR_CMPSW:	case STR_CMPSD:
		tmp_reg=DREG(TMPB);usesi=true;usedi=true;break;
	case STR_LODSB:	case STR_LODSW:	case STR_LODSD:
		tmp_reg=DREG(EAX);usesi=true;usedi=false;break;
	case STR_OUTSB:	case STR_OUTSW:	case STR_OUTSD:
		tmp_reg=DREG(TMPB);usesi=true;usedi=false;break;
	case STR_SCASB:	case STR_SCASW:	case STR_SCASD:
	case STR_STOSB:	case STR_STOSW:	case STR_STOSD:
		tmp_reg=DREG(EAX);usesi=false;usedi=true;break;
	case STR_INSB:	case STR_INSW:	case STR_INSD:
		tmp_reg=DREG(TMPB);usesi=false;usedi=true;break;
	default:
		IllegalOption();
	}
	gen_load_host(&cpu.direction,DREG(TMPW),4);
	switch (op & 3) {
	case 0:break;
	case 1:gen_shift_word_imm(SHIFT_SHL,true,DREG(TMPW),1);break;
	case 2:gen_shift_word_imm(SHIFT_SHL,true,DREG(TMPW),2);break;
	default:
		IllegalOption();

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
	Bit8u * rep_start=cache.pos;
	Bit8u * rep_ecx_jmp;
	/* Check if ECX!=zero and decrease it */
	if (decode.rep) {
		gen_dop_word(DOP_OR,decode.big_addr,DREG(ECX),DREG(ECX));
		Bit8u * branch_ecx=gen_create_branch(BR_NZ);
		rep_ecx_jmp=gen_create_jump();
		gen_fill_branch(branch_ecx);
		gen_sop_word(SOP_DEC,decode.big_addr,DREG(ECX));
	}
	if (usesi) {
		if (!decode.big_addr) {
			gen_extend_word(false,DREG(EA),DREG(ESI));
			gen_lea(DREG(EA),si_base,DREG(EA),0,0);
		} else {
			gen_lea(DREG(EA),si_base,DREG(ESI),0,0);
		}
		gen_dop_word(DOP_ADD,decode.big_addr,DREG(ESI),DREG(TMPW));
		switch (op&3) {
		case 0:dyn_read_byte(DREG(EA),tmp_reg,false);break;
		case 1:dyn_read_word(DREG(EA),tmp_reg,false);break;
		case 2:dyn_read_word(DREG(EA),tmp_reg,true);break;
		}
		switch (op) {
		case STR_OUTSB:
			gen_call_function((void*)&IO_WriteB,"%Id%Dl",DREG(EDX),tmp_reg);break;
		case STR_OUTSW:
			gen_call_function((void*)&IO_WriteW,"%Id%Dw",DREG(EDX),tmp_reg);break;
		case STR_OUTSD:
			gen_call_function((void*)&IO_WriteD,"%Id%Dd",DREG(EDX),tmp_reg);break;
		}
	}
	if (usedi) {
		if (!decode.big_addr) {
			gen_extend_word(false,DREG(EA),DREG(EDI));
			gen_lea(DREG(EA),di_base,DREG(EA),0,0);
		} else {
			gen_lea(DREG(EA),di_base,DREG(EDI),0,0);
		}
		gen_dop_word(DOP_ADD,decode.big_addr,DREG(EDI),DREG(TMPW));
		/* Maybe something special to be done to fill the value */
		switch (op) {
		case STR_INSB:
			gen_call_function((void*)&IO_ReadB,"%Dw%Rl",DREG(EDX),tmp_reg);
		case STR_MOVSB:
		case STR_STOSB:
			dyn_write_byte(DREG(EA),tmp_reg,false);
			break;
		case STR_INSW:
			gen_call_function((void*)&IO_ReadW,"%Dw%Rw",DREG(EDX),tmp_reg);
		case STR_MOVSW:
		case STR_STOSW:
			dyn_write_word(DREG(EA),tmp_reg,false);
			break;
		case STR_INSD:
			gen_call_function((void*)&IO_ReadD,"%Dw%Rd",DREG(EDX),tmp_reg);
		case STR_MOVSD:
		case STR_STOSD:
			dyn_write_word(DREG(EA),tmp_reg,true);
			break;
		default:
			IllegalOption();
		}
	}
	gen_releasereg(DREG(EA));gen_releasereg(DREG(TMPB));
	if (decode.rep) {
		DynState cycle_state;
		gen_sop_word(SOP_DEC,true,DREG(CYCLES));
		gen_releasereg(DREG(CYCLES));
		dyn_savestate(&cycle_state);
		Bit8u * cycle_branch=gen_create_branch(BR_NLE);
		gen_dop_word_imm(DOP_ADD,decode.big_op,DREG(EIP),decode.op_start-decode.code_start);
		dyn_save_critical_regs();
		gen_return(BR_Cycles);
		gen_fill_branch(cycle_branch);
		dyn_loadstate(&cycle_state);
		dyn_synchstate(&rep_state);
		/* Jump back to start of ECX check */
		gen_create_jump(rep_start);
		gen_fill_jump(rep_ecx_jmp);
	}
	gen_releasereg(DREG(TMPW));
}
