static struct DynDecode {
	PhysPt code;
	PhysPt code_start;
	PhysPt op_start;
	bool big_op;
	bool big_addr;
	bool rep;
	Bitu cycles;
	CacheBlock * block;
	struct {
		Bitu val;
		Bitu mod;
		Bitu rm;
		Bitu reg;
	} modrm;
	DynReg * segprefix;
} decode;

#define FASTCALL __fastcall

#include "helpers.h"

static Bit8u FASTCALL decode_fetchb(void) {
	return mem_readb(decode.code++);
}
static Bit16u FASTCALL decode_fetchw(void) {
	decode.code+=2;
	return mem_readw(decode.code-2);
}
static Bit32u FASTCALL decode_fetchd(void) {
	decode.code+=4;
	return mem_readd(decode.code-4);
}

static void dyn_read_byte(DynReg * addr,DynReg * dst,Bitu high) {
	if (high) gen_call_function((void *)&mem_readb,"%Dd%Rh",addr,dst);
	else gen_call_function((void *)&mem_readb,"%Dd%Rl",addr,dst);
}
static void dyn_write_byte(DynReg * addr,DynReg * val,Bitu high) {
	if (high) gen_call_function((void *)&mem_writeb,"%Dd%Dh",addr,val);
	else gen_call_function((void *)&mem_writeb,"%Dd%Dl",addr,val);
}

static void dyn_read_word(DynReg * addr,DynReg * dst,bool dword) {
	if (dword) gen_call_function((void *)&mem_readd,"%Dd%Rd",addr,dst);
	else gen_call_function((void *)&mem_readw,"%Dd%Rw",addr,dst);
}

static void dyn_write_word(DynReg * addr,DynReg * val,bool dword) {
	if (dword) gen_call_function((void *)&mem_writed,"%Dd%Dd",addr,val);
	else gen_call_function((void *)&mem_writew,"%Dd%Dw",addr,val);
}

static void dyn_reduce_cycles(void) {
	if (!decode.cycles) decode.cycles++;
	gen_lea(DREG(CYCLES),DREG(CYCLES),0,0,-(Bits)decode.cycles);
	gen_releasereg(DREG(CYCLES));
}

static void dyn_push(DynReg * dynreg) {
	gen_storeflags();
	if (decode.big_op) {
		gen_dop_word_imm(DOP_SUB,true,DREG(ESP),4);
	} else {
		gen_dop_word_imm(DOP_SUB,true,DREG(ESP),2);
	}
	gen_dop_word(DOP_MOV,true,DREG(STACK),DREG(ESP));
	gen_dop_word(DOP_AND,true,DREG(STACK),DREG(SMASK));
	gen_dop_word(DOP_ADD,true,DREG(STACK),DREG(SS));
	if (decode.big_op) {
		gen_call_function((void *)&mem_writed,"%Drd%Dd",DREG(STACK),dynreg);
	} else {
		//Can just push the whole 32-bit word as operand
		gen_call_function((void *)&mem_writew,"%Drd%Dd",DREG(STACK),dynreg);
	}
	gen_releasereg(DREG(STACK));
	gen_restoreflags();
}

static void dyn_pop(DynReg * dynreg) {
	gen_storeflags();
	gen_dop_word(DOP_MOV,true,DREG(STACK),DREG(ESP));
	gen_dop_word(DOP_AND,true,DREG(STACK),DREG(SMASK));
	gen_dop_word(DOP_ADD,true,DREG(STACK),DREG(SS));
	if (decode.big_op) {
		gen_call_function((void *)&mem_readd,"%Rd%Drd",dynreg,DREG(STACK));
	} else {
		gen_call_function((void *)&mem_readw,"%Rw%Drd",dynreg,DREG(STACK));
	}
	if (dynreg!=DREG(ESP)) {
		if (decode.big_op) {
			gen_dop_word_imm(DOP_ADD,true,DREG(ESP),4);
		} else {
			gen_dop_word_imm(DOP_ADD,true,DREG(ESP),2);
		}
	}
	gen_releasereg(DREG(STACK));
	gen_restoreflags();
}

static void FASTCALL dyn_get_modrm(void) {
	decode.modrm.val=decode_fetchb();
	decode.modrm.mod=(decode.modrm.val >> 6) & 3;
	decode.modrm.reg=(decode.modrm.val >> 3) & 7;
	decode.modrm.rm=(decode.modrm.val & 7);
}

static void FASTCALL dyn_fill_ea(bool addseg=true) {
	DynReg * segbase;
	if (!decode.big_addr) {
		Bits imm;
		switch (decode.modrm.mod) {
		case 0:imm=0;break;
		case 1:imm=(Bit8s)decode_fetchb();break;
		case 2:imm=(Bit16s)decode_fetchw();break;
		}
		switch (decode.modrm.rm) {
		case 0:/* BX+SI */
			gen_lea(DREG(EA),DREG(EBX),DREG(ESI),0,imm);
			segbase=DREG(DS);
			break;
		case 1:/* BX+DI */
			gen_lea(DREG(EA),DREG(EBX),DREG(EDI),0,imm);
			segbase=DREG(DS);
			break;
		case 2:/* BP+SI */
			gen_lea(DREG(EA),DREG(EBP),DREG(ESI),0,imm);
			segbase=DREG(SS);
			break;
		case 3:/* BP+DI */
			gen_lea(DREG(EA),DREG(EBP),DREG(EDI),0,imm);
			segbase=DREG(SS);
			break;
		case 4:/* SI */
			gen_lea(DREG(EA),DREG(ESI),0,0,imm);
			segbase=DREG(DS);
			break;
		case 5:/* DI */
			gen_lea(DREG(EA),DREG(EDI),0,0,imm);
			segbase=DREG(DS);
			break;
		case 6:/* imm/BP */
			if (!decode.modrm.mod) {
				imm=(Bit16s)decode_fetchw();
                gen_dop_word_imm(DOP_MOV,true,DREG(EA),imm);
				segbase=DREG(DS);
			} else {
				gen_lea(DREG(EA),DREG(EBP),0,0,imm);
				segbase=DREG(SS);
			}
			break;
		case 7: /* BX */
			gen_lea(DREG(EA),DREG(EBX),0,0,imm);
			segbase=DREG(DS);
			break;
		}
		gen_extend_word(false,DREG(EA),DREG(EA));
	} else {
		Bits imm=0;
		DynReg * base=0;DynReg * scaled=0;Bitu scale=0;
		switch (decode.modrm.rm) {
		case 0:base=DREG(EAX);segbase=DREG(DS);break;
		case 1:base=DREG(ECX);segbase=DREG(DS);break;
		case 2:base=DREG(EDX);segbase=DREG(DS);break;
		case 3:base=DREG(EBX);segbase=DREG(DS);break;
		case 4:	/* SIB */
			{
				Bitu sib=decode_fetchb();
				switch (sib & 7) {
				case 0:base=DREG(EAX);segbase=DREG(DS);break;
				case 1:base=DREG(ECX);segbase=DREG(DS);break;
				case 2:base=DREG(EDX);segbase=DREG(DS);break;
				case 3:base=DREG(EBX);segbase=DREG(DS);break;
				case 4:base=DREG(ESP);segbase=DREG(SS);break;
				case 5:
					if (decode.modrm.mod) {
						base=DREG(EBP);segbase=DREG(SS);
					} else {
						imm=(Bit32s)decode_fetchd();segbase=DREG(DS);
					}
					break;
				case 6:base=DREG(ESI);segbase=DREG(DS);break;
				case 7:base=DREG(EDI);segbase=DREG(DS);break;
				}
				static DynReg * scaledtable[8]={
					DREG(EAX),DREG(ECX),DREG(EDX),DREG(EBX),
							0,DREG(EBP),DREG(ESI),DREG(EDI),
				};
				scaled=scaledtable[(sib >> 3) &7];
				scale=(sib >> 6);
			}	
			break;	/* SIB Break */
		case 5:
			if (decode.modrm.mod) {
				base=DREG(EBP);segbase=DREG(SS);
			} else {
				imm=(Bit32s)decode_fetchd();segbase=DREG(DS);
			}
			break;
		case 6:base=DREG(ESI);segbase=DREG(DS);break;
		case 7:base=DREG(EDI);segbase=DREG(DS);break;
		}
		switch (decode.modrm.mod) {
		case 1:imm=(Bit8s)decode_fetchb();break;
		case 2:imm=(Bit32s)decode_fetchd();break;
		}
		gen_lea(DREG(EA),base,scaled,scale,imm);
	}
	if (addseg) {
		gen_lea(DREG(EA),DREG(EA),decode.segprefix ? decode.segprefix : segbase,0,0);
	}
}

static void dyn_dop_ebgb(DualOps op) {
	dyn_get_modrm();DynReg * rm_reg=&DynRegs[decode.modrm.reg&3];
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		dyn_read_byte(DREG(EA),DREG(TMPB),false);
		gen_dop_byte(op,DREG(TMPB),0,rm_reg,decode.modrm.reg&4);
		dyn_write_byte(DREG(EA),DREG(TMPB),false);
		gen_releasereg(DREG(EA));gen_releasereg(DREG(TMPB));
	} else {
		gen_dop_byte(op,&DynRegs[decode.modrm.rm&3],decode.modrm.rm&4,rm_reg,decode.modrm.reg&4);
	}
}


static void dyn_dop_gbeb(DualOps op) {
	dyn_get_modrm();DynReg * rm_reg=&DynRegs[decode.modrm.reg&3];
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		dyn_read_byte(DREG(EA),DREG(TMPB),false);
		gen_dop_byte(op,rm_reg,decode.modrm.reg&4,DREG(TMPB),0);
		gen_releasereg(DREG(EA));gen_releasereg(DREG(TMPB));
	} else {
		gen_dop_byte(op,rm_reg,decode.modrm.reg&4,&DynRegs[decode.modrm.rm&3],decode.modrm.rm&4);
	}
}

static void dyn_mov_ebib(void) {
	dyn_get_modrm();
	if (decode.modrm.mod<3) {
		//TODO Maybe not use a temp register here and call mem_writeb directly?
		dyn_fill_ea();
		gen_dop_byte_imm(DOP_MOV,DREG(TMPB),0,decode_fetchb());
		dyn_write_byte(DREG(EA),DREG(TMPB),false);
		gen_releasereg(DREG(EA));gen_releasereg(DREG(TMPB));
	} else {
		gen_dop_byte_imm(DOP_MOV,&DynRegs[decode.modrm.rm&3],decode.modrm.rm&4,decode_fetchb());
	}
}

static void dyn_mov_ebgb(void) {
	dyn_get_modrm();
	DynReg * rm_reg=&DynRegs[decode.modrm.reg&3];Bitu rm_regi=decode.modrm.reg&4;
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		dyn_write_byte(DREG(EA),rm_reg,rm_regi);
		gen_releasereg(DREG(EA));
	} else {
		gen_dop_byte(DOP_MOV,&DynRegs[decode.modrm.rm&3],decode.modrm.rm&4,rm_reg,rm_regi);
	}
}

static void dyn_mov_gbeb(void) {
	dyn_get_modrm();
	DynReg * rm_reg=&DynRegs[decode.modrm.reg&3];Bitu rm_regi=decode.modrm.reg&4;
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		dyn_read_byte(DREG(EA),rm_reg,rm_regi);
		gen_releasereg(DREG(EA));
	} else {
		gen_dop_byte(DOP_MOV,rm_reg,rm_regi,&DynRegs[decode.modrm.rm&3],decode.modrm.rm&4);
	}
}

static void dyn_dop_evgv(DualOps op) {
	dyn_get_modrm();
	DynReg * rm_reg=&DynRegs[decode.modrm.reg];
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		dyn_read_word(DREG(EA),DREG(TMPW),decode.big_op);
		gen_dop_word(op,decode.big_op,DREG(TMPW),rm_reg);
		dyn_write_word(DREG(EA),DREG(TMPW),decode.big_op);
		gen_releasereg(DREG(EA));gen_releasereg(DREG(TMPW));
	} else {
		gen_dop_word(op,decode.big_op,&DynRegs[decode.modrm.rm],rm_reg);
	}
}

static void dyn_dop_gvev(DualOps op) {
	dyn_get_modrm();
	DynReg * rm_reg=&DynRegs[decode.modrm.reg];
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		dyn_read_word(DREG(EA),DREG(TMPW),decode.big_op);
		gen_dop_word(op,decode.big_op,rm_reg,DREG(TMPW));
		gen_releasereg(DREG(EA));gen_releasereg(DREG(TMPW));
	} else {
		gen_dop_word(op,decode.big_op,rm_reg,&DynRegs[decode.modrm.rm]);
	}
}

static void dyn_mov_evgv(void) {
	dyn_get_modrm();
	DynReg * rm_reg=&DynRegs[decode.modrm.reg];
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		dyn_write_word(DREG(EA),rm_reg,decode.big_op);
		gen_releasereg(DREG(EA));
	} else {
		gen_dop_word(DOP_MOV,decode.big_op,&DynRegs[decode.modrm.rm],rm_reg);
	}
}

static void dyn_mov_gvev(void) {
	dyn_get_modrm();
	DynReg * rm_reg=&DynRegs[decode.modrm.reg];
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		dyn_read_word(DREG(EA),rm_reg,decode.big_op);
		gen_releasereg(DREG(EA));
	} else {
		gen_dop_word(DOP_MOV,decode.big_op,rm_reg,&DynRegs[decode.modrm.rm]);
	}
}
static void dyn_mov_eviv(void) {
	dyn_get_modrm();
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		gen_dop_word_imm(DOP_MOV,decode.big_op,DREG(TMPW),decode.big_op ? decode_fetchd() : decode_fetchw());
		dyn_write_word(DREG(EA),DREG(TMPW),decode.big_op);
		gen_releasereg(DREG(EA));gen_releasereg(DREG(TMPW));
	} else {
		gen_dop_word_imm(DOP_MOV,decode.big_op,&DynRegs[decode.modrm.rm],decode.big_op ? decode_fetchd() : decode_fetchw());
	}
}

static void dyn_dshift_ev_gv(bool left,bool immediate) {
	dyn_get_modrm();
	DynReg * rm_reg=&DynRegs[decode.modrm.reg];
	DynReg * ea_reg;
	if (decode.modrm.mod<3) {
		dyn_fill_ea();ea_reg=DREG(TMPW);
		dyn_read_word(DREG(EA),DREG(TMPW),decode.big_op);
	} else ea_reg=&DynRegs[decode.modrm.rm];
	if (immediate) gen_dshift_imm(decode.big_op,left,ea_reg,rm_reg,decode_fetchb());
	else gen_dshift_cl(decode.big_op,left,ea_reg,rm_reg,DREG(ECX));
	if (decode.modrm.mod<3) {
		dyn_write_word(DREG(EA),DREG(TMPW),decode.big_op);
		gen_releasereg(DREG(EA));gen_releasereg(DREG(TMPW));
	}
}


static DualOps grp1_table[8]={DOP_ADD,DOP_OR,DOP_ADC,DOP_SBB,DOP_AND,DOP_SUB,DOP_XOR,DOP_CMP};
static void dyn_grp1_eb_ib(void) {
	dyn_get_modrm();
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		dyn_read_byte(DREG(EA),DREG(TMPB),false);
		gen_dop_byte_imm(grp1_table[decode.modrm.reg],DREG(TMPB),0,decode_fetchb());
		if (grp1_table[decode.modrm.reg]!=DOP_CMP) dyn_write_byte(DREG(EA),DREG(TMPB),false);
		gen_releasereg(DREG(EA));gen_releasereg(DREG(TMPB));
	} else {
		gen_dop_byte_imm(grp1_table[decode.modrm.reg],&DynRegs[decode.modrm.rm&3],decode.modrm.rm&4,decode_fetchb());
	}
}

static void dyn_grp1_ev_ivx(bool withbyte) {
	dyn_get_modrm();
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		dyn_read_word(DREG(EA),DREG(TMPW),decode.big_op);
		Bits imm=withbyte ? (Bit8s)decode_fetchb() : (decode.big_op ? decode_fetchd(): decode_fetchw()); 
		gen_dop_word_imm(grp1_table[decode.modrm.reg],decode.big_op,DREG(TMPW),imm);
		dyn_write_word(DREG(EA),DREG(TMPW),decode.big_op);
		gen_releasereg(DREG(EA));gen_releasereg(DREG(TMPW));
	} else {
		Bits imm=withbyte ? (Bit8s)decode_fetchb() : (decode.big_op ? decode_fetchd(): decode_fetchw()); 
		gen_dop_word_imm(grp1_table[decode.modrm.reg],decode.big_op,&DynRegs[decode.modrm.rm],imm);
	}
}


static ShiftOps grp2_table[8]={
	SHIFT_ROL,SHIFT_ROR,SHIFT_RCL,SHIFT_RCR,
	SHIFT_SHL,SHIFT_SHR,SHIFT_SHL,SHIFT_SAR
};

enum grp2_types {
	grp2_1,grp2_imm,grp2_cl,
};

static void dyn_grp2_eb(grp2_types type) {
	dyn_get_modrm();
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		dyn_read_byte(DREG(EA),DREG(TMPB),false);
		DynReg * shift;
		switch (type) {
		case grp2_cl:shift=DREG(ECX);break;
		case grp2_1:shift=DREG(SHIFT);gen_dop_byte_imm(DOP_MOV,DREG(SHIFT),0,1);break;
		case grp2_imm:shift=DREG(SHIFT);gen_dop_byte_imm(DOP_MOV,DREG(SHIFT),0,decode_fetchb());break;
		}
		gen_shift_byte(grp2_table[decode.modrm.reg],shift,DREG(TMPB),0);
		dyn_write_byte(DREG(EA),DREG(TMPB),false);
		gen_releasereg(DREG(EA));gen_releasereg(DREG(TMPB));gen_releasereg(DREG(SHIFT));
	} else {
		DynReg * shift;
		switch (type) {
		case grp2_cl:shift=DREG(ECX);break;
		case grp2_1:shift=DREG(SHIFT);gen_dop_byte_imm(DOP_MOV,DREG(SHIFT),0,1);break;
		case grp2_imm:shift=DREG(SHIFT);gen_dop_byte_imm(DOP_MOV,DREG(SHIFT),0,decode_fetchb());break;
		}
		gen_shift_byte(grp2_table[decode.modrm.reg],shift,&DynRegs[decode.modrm.rm&3],decode.modrm.rm&4);
		gen_releasereg(DREG(SHIFT));
	}
}

static void dyn_grp2_ev(grp2_types type) {
	dyn_get_modrm();
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		dyn_read_word(DREG(EA),DREG(TMPW),decode.big_op);
		DynReg * shift;
		switch (type) {
		case grp2_cl:shift=DREG(ECX);break;
		case grp2_1:shift=DREG(SHIFT);gen_dop_byte_imm(DOP_MOV,DREG(SHIFT),0,1);break;
		case grp2_imm:shift=DREG(SHIFT);gen_dop_byte_imm(DOP_MOV,DREG(SHIFT),0,decode_fetchb());break;
		}
		gen_shift_word(grp2_table[decode.modrm.reg],shift,decode.big_op,DREG(TMPW));
		dyn_write_word(DREG(EA),DREG(TMPW),decode.big_op);
		gen_releasereg(DREG(EA));gen_releasereg(DREG(TMPW));gen_releasereg(DREG(SHIFT));
	} else {
		DynReg * shift;
		switch (type) {
		case grp2_cl:shift=DREG(ECX);break;
		case grp2_1:shift=DREG(SHIFT);gen_dop_byte_imm(DOP_MOV,DREG(SHIFT),0,1);break;
		case grp2_imm:shift=DREG(SHIFT);gen_dop_byte_imm(DOP_MOV,DREG(SHIFT),0,decode_fetchb());break;
		}
		gen_shift_word(grp2_table[decode.modrm.reg],shift,decode.big_op,&DynRegs[decode.modrm.rm]);
		gen_releasereg(DREG(SHIFT));
	}
}

static void dyn_grp3_eb(void) {
	DynState state;Bit8u * branch;
	dyn_get_modrm();DynReg * src;Bit8u src_i;
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		dyn_read_byte(DREG(EA),DREG(TMPB),false);
		src=DREG(TMPB);src_i=0;
	} else {
		src=&DynRegs[decode.modrm.rm&3];
		src_i=decode.modrm.rm&4;
	}
	switch (decode.modrm.reg) {
	case 0x0:	/* test eb,ib */
		gen_dop_byte_imm(DOP_TEST,src,src_i,decode_fetchb());
		goto skipsave;
	case 0x2:	/* NOT Eb */
		gen_sop_byte(SOP_NOT,src,src_i);
		break;
	case 0x3:	/* NEG Eb */
		gen_sop_byte(SOP_NEG,src,src_i);
		break;
	case 0x4:	/* mul Eb */
		gen_mul_byte(false,DREG(EAX),src,src_i);
		goto skipsave;
	case 0x5:	/* imul Eb */
		gen_mul_byte(true,DREG(EAX),src,src_i);
		goto skipsave;
	case 0x6:	/* div Eb */
	case 0x7:	/* idiv Eb */
		/* EAX could be used, so precache it */
		if (decode.modrm.mod==3)
			gen_dop_byte(DOP_MOV,DREG(TMPB),0,&DynRegs[decode.modrm.rm&3],decode.modrm.rm&4);
		gen_storeflags();gen_releasereg(DREG(EAX));
		gen_call_function((decode.modrm.reg==6) ? (void *)&dyn_helper_divb : (void *)&dyn_helper_idivb,
			"%Rd%Drl",DREG(TMPB),DREG(TMPB));
		gen_dop_word(DOP_OR,true,DREG(TMPB),DREG(TMPB));
		branch=gen_create_branch(BR_Z);
		dyn_savestate(&state);
		dyn_reduce_cycles();	
		gen_lea(DREG(EIP),DREG(EIP),0,0,decode.op_start-decode.code_start);
		dyn_save_flags(true);
		dyn_releaseregs();
		gen_call_function((void *)&CPU_Exception,"%Id%Id",0,0);
		dyn_load_flags();
		gen_return(BR_Normal);
		dyn_loadstate(&state);
		gen_fill_branch(branch);
		gen_restoreflags();
		goto skipsave;
	}
	/* Save the result if memory op */
	if (decode.modrm.mod<3) dyn_write_byte(DREG(EA),DREG(TMPB),false);
skipsave:
	gen_releasereg(DREG(TMPB));gen_releasereg(DREG(EA));
}

static void dyn_grp3_ev(void) {
	DynState state;Bit8u * branch;
	dyn_get_modrm();DynReg * src;
	if (decode.modrm.mod<3) {
		dyn_fill_ea();src=DREG(TMPW);
		dyn_read_word(DREG(EA),DREG(TMPW),decode.big_op);
	} else src=&DynRegs[decode.modrm.rm];
	switch (decode.modrm.reg) {
	case 0x0:	/* test ev,iv */
		gen_dop_word_imm(DOP_TEST,decode.big_op,src,decode.big_op ? decode_fetchd() : decode_fetchw());
		goto skipsave;
	case 0x2:	/* NOT Ev */
		gen_sop_word(SOP_NOT,decode.big_op,src);
		break;
	case 0x3:	/* NEG Eb */
		gen_sop_word(SOP_NEG,decode.big_op,src);
		break;
	case 0x4:	/* mul Eb */
		gen_mul_word(false,DREG(EAX),DREG(EDX),decode.big_op,src);
		goto skipsave;
	case 0x5:	/* imul Eb */
		gen_mul_word(true,DREG(EAX),DREG(EDX),decode.big_op,src);
		goto skipsave;
	case 0x6:	/* div Eb */
	case 0x7:	/* idiv Eb */
		/* EAX could be used, so precache it */
		if (decode.modrm.mod==3)
			gen_dop_word(DOP_MOV,decode.big_op,DREG(TMPW),&DynRegs[decode.modrm.rm]);
		gen_storeflags();gen_releasereg(DREG(EAX));gen_releasereg(DREG(EDX));
		void * func=(decode.modrm.reg==6) ?
			(decode.big_op ? (void *)&dyn_helper_divd : (void *)&dyn_helper_divw) :
			(decode.big_op ? (void *)&dyn_helper_idivd : (void *)&dyn_helper_idivw);
		gen_call_function(func,"%Rd%Drd",DREG(TMPW),DREG(TMPW));
		gen_dop_word(DOP_OR,true,DREG(TMPW),DREG(TMPW));
		branch=gen_create_branch(BR_Z);
		dyn_savestate(&state);
		dyn_reduce_cycles();	
		gen_lea(DREG(EIP),DREG(EIP),0,0,decode.op_start-decode.code_start);
		dyn_save_flags(true);
		dyn_releaseregs();
		gen_call_function((void *)&CPU_Exception,"%Id%Id",0,0);
		dyn_load_flags();
		gen_return(BR_Normal);
		dyn_loadstate(&state);
		gen_fill_branch(branch);
		gen_restoreflags();
		goto skipsave;
	}
	/* Save the result if memory op */
	if (decode.modrm.mod<3) dyn_write_word(DREG(EA),DREG(TMPW),decode.big_op);
skipsave:
	gen_releasereg(DREG(TMPW));gen_releasereg(DREG(EA));
}


static void dyn_mov_ev_seg(void) {
	dyn_get_modrm();
	gen_load_host(&Segs.val[(SegNames) decode.modrm.reg],DREG(TMPW),2);
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		dyn_write_word(DREG(EA),DREG(TMPW),decode.big_op);
		gen_releasereg(DREG(EA));
	} else {
		gen_dop_word(DOP_MOV,decode.big_op,&DynRegs[decode.modrm.rm],DREG(TMPW));
	}
	gen_releasereg(DREG(TMPW));
}

static void dyn_mov_seg_ev(void) {
	dyn_get_modrm();
	SegNames seg=(SegNames)decode.modrm.reg;
	if (seg==cs) IllegalOption();
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		dyn_read_word(DREG(EA),DREG(EA),decode.big_op);
		gen_call_function((void *)&CPU_SetSegGeneral,"%Id%Drw",seg,DREG(EA));
	} else {
		gen_call_function((void *)&CPU_SetSegGeneral,"%Id%Dw",seg,&DynRegs[decode.modrm.rm]);
	}
	gen_releasereg(&DynRegs[G_ES+seg]);
}


static void dyn_push_seg(SegNames seg) {
	gen_load_host(&Segs.val[seg],DREG(TMPW),2);
	dyn_push(DREG(TMPW));
	gen_releasereg(DREG(TMPW));
}

static void dyn_pop_seg(SegNames seg) {
	gen_storeflags();
	dyn_pop(DREG(TMPW));
	gen_call_function((void*)&CPU_SetSegGeneral,"%Id%Drw",seg,DREG(TMPW));
	gen_releasereg(&DynRegs[G_ES+seg]);
	gen_restoreflags();
}

static void dyn_pop_ev(void) {
	gen_storeflags();
	dyn_pop(DREG(TMPW));
	dyn_get_modrm();
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		dyn_write_word(DREG(EA),DREG(TMPW),decode.big_op);
		gen_releasereg(DREG(EA));
	} else {
		gen_dop_word(DOP_MOV,decode.big_op,&DynRegs[decode.modrm.rm],DREG(TMPW));
	}
	gen_releasereg(DREG(TMPW));
	gen_restoreflags();
}

static void dyn_leave(void) {
	gen_storeflags();
	gen_dop_word(DOP_MOV,true,DREG(TMPW),DREG(SMASK));
	gen_sop_word(SOP_NOT,true,DREG(TMPW));
	gen_dop_word(DOP_AND,true,DREG(ESP),DREG(TMPW));
	gen_dop_word(DOP_MOV,true,DREG(TMPW),DREG(EBP));
	gen_dop_word(DOP_AND,true,DREG(TMPW),DREG(SMASK));
	gen_dop_word(DOP_OR,true,DREG(ESP),DREG(TMPW));
	dyn_pop(DREG(EBP));
	gen_releasereg(DREG(TMPW));
	gen_restoreflags();
}

static void dyn_segprefix(SegNames seg) {
	if (decode.segprefix) IllegalOption();
	decode.segprefix=&DynRegs[G_ES+seg];
}

static void dyn_closeblock(BlockType type) {
	//Shouldn't create empty block normally but let's do it like this
	if (decode.code>decode.code_start) decode.code--;
	Bitu start_page=decode.code_start >> 12;
	Bitu end_page=decode.code>>12;
	decode.block->page.start=(Bit16s)decode.code_start & 4095;
	decode.block->page.end=(Bit16s)((end_page-start_page)*4096+(decode.code&4095));
	cache_closeblock(type);
}

static void dyn_normal_exit(BlockReturn code) {
	gen_lea(DREG(EIP),DREG(EIP),0,0,decode.code-decode.code_start);
	dyn_reduce_cycles();
	dyn_releaseregs();
	gen_return(code);
	dyn_closeblock(BT_Normal);
}

static void dyn_exit_link(bool dword,Bits eip_change) {
	gen_lea(DREG(EIP),DREG(EIP),0,0,(decode.code-decode.code_start)+eip_change);
	if (!dword) gen_extend_word(false,DREG(EIP),DREG(EIP));
	dyn_reduce_cycles();
	dyn_releaseregs();
//	gen_return(BR_Normal);
	gen_jmp_ptr(&decode.block->link.to[0],offsetof(CacheBlock,cache.start));
	dyn_closeblock(BT_SingleLink);
}

static void dyn_branched_exit(BranchTypes btype,Bit32s eip_add) {
	dyn_reduce_cycles();
	dyn_releaseregs();
	Bitu eip_base=decode.code-decode.code_start;
	Bit8u * data=gen_create_branch(btype);
	/* Branch not taken */
	gen_lea(DREG(EIP),DREG(EIP),0,0,eip_base);
	gen_releasereg(DREG(EIP));
//	gen_return(BR_Normal);
	gen_jmp_ptr(&decode.block->link.to[0],offsetof(CacheBlock,cache.start));
	gen_fill_branch(data);
	/* Branch taken */
	gen_lea(DREG(EIP),DREG(EIP),0,0,eip_base+eip_add);
	gen_releasereg(DREG(EIP));
//	gen_return(BR_Normal);
	gen_jmp_ptr(&decode.block->link.to[1],offsetof(CacheBlock,cache.start));
	dyn_closeblock(BT_DualLink);
}

enum LoopTypes {
	LOOP_NONE,LOOP_NE,LOOP_E,
};

static void dyn_loop(LoopTypes type) {
	Bits eip_add=(Bit8s)decode_fetchb();
	Bitu eip_base=decode.code-decode.code_start;
	gen_storeflags();
	dyn_reduce_cycles();
	Bit8u * branch1;
	Bit8u * branch2=0;
	gen_sop_word(SOP_DEC,decode.big_addr,DREG(ECX));
	dyn_releaseregs();
	branch1=gen_create_branch(BR_Z);
	gen_restoreflags(true);
	switch (type) {
	case LOOP_NONE:
		break;
	case LOOP_E:
		branch2=gen_create_branch(BR_NZ);
		break;
	case LOOP_NE:
		branch2=gen_create_branch(BR_Z);
		break;
	}
	gen_lea(DREG(EIP),DREG(EIP),0,0,eip_base+eip_add);
	gen_releasereg(DREG(EIP));
	gen_jmp_ptr(&decode.block->link.to[0],offsetof(CacheBlock,cache.start));
	gen_fill_branch(branch1);
	if (branch2) gen_fill_branch(branch2);
	/* Branch taken */
	gen_restoreflags();
	gen_lea(DREG(EIP),DREG(EIP),0,0,eip_base);
	gen_releasereg(DREG(EIP));
	gen_jmp_ptr(&decode.block->link.to[1],offsetof(CacheBlock,cache.start));
	dyn_closeblock(BT_DualLink);
}

static void dyn_ret_near(Bitu bytes) {
	dyn_reduce_cycles();
//TODO maybe AND eip 0xffff, but shouldn't be needed
	gen_storeflags();
	dyn_pop(DREG(EIP));
	if (bytes) gen_dop_word_imm(DOP_ADD,true,DREG(ESP),bytes);
	dyn_releaseregs();
	gen_restoreflags();
	gen_return(BR_Normal);
	dyn_closeblock(BT_Normal);
}

static void dyn_ret_far(Bitu bytes) {
	dyn_reduce_cycles();
//TODO maybe AND eip 0xffff, but shouldn't be needed
	gen_lea(DREG(EIP),DREG(EIP),0,0,decode.code-decode.code_start);
	dyn_save_flags();
	dyn_releaseregs();
	gen_call_function((void*)&CPU_RET,"%Id%Id%Id",decode.big_op,bytes,decode.code-decode.op_start);
	dyn_load_flags();
	dyn_releaseregs();;
	gen_return(BR_Normal);
	dyn_closeblock(BT_Normal);
}

static void dyn_call_near_imm(void) {
	Bits imm;
	if (decode.big_op) imm=(Bit32s)decode_fetchd();
	else imm=(Bit16s)decode_fetchw();
	gen_lea(DREG(EIP),DREG(EIP),0,0,decode.code-decode.code_start);
	dyn_push(DREG(EIP));
	gen_lea(DREG(EIP),DREG(EIP),0,0,imm);
	if (!decode.big_op) gen_extend_word(false,DREG(EIP),DREG(EIP));
	dyn_reduce_cycles();
	dyn_releaseregs();
	gen_return(BR_Normal);
//	gen_jmp_ptr(&decode.block->link.to[0],offsetof(CacheBlock,cache.start));
	dyn_closeblock(BT_SingleLink);
}

static void dyn_call_far_imm(void) {
	Bitu sel,off;
	off=decode.big_op ? decode_fetchd() : decode_fetchw();
	sel=decode_fetchw();
	dyn_reduce_cycles();
	gen_lea(DREG(EIP),DREG(EIP),0,0,decode.code-decode.code_start);
	dyn_save_flags();
	dyn_releaseregs();
	gen_call_function((void*)&CPU_CALL,"%Id%Id%Id%Id",decode.big_op,sel,off,decode.code-decode.op_start);
	dyn_load_flags();
	dyn_releaseregs();
	gen_return(BR_Normal);
	dyn_closeblock(BT_Normal);
}

static void dyn_jmp_far_imm(void) {
	Bitu sel,off;
	off=decode.big_op ? decode_fetchd() : decode_fetchw();
	sel=decode_fetchw();
	dyn_reduce_cycles();
	gen_lea(DREG(EIP),DREG(EIP),0,0,decode.code-decode.code_start);
	dyn_save_flags();
	dyn_releaseregs();
	gen_call_function((void*)&CPU_JMP,"%Id%Id%Id%Id",decode.big_op,sel,off,decode.code-decode.op_start);
	dyn_load_flags();
	dyn_releaseregs();
	gen_return(BR_Normal);
	dyn_closeblock(BT_Normal);
}

static void dyn_iret(void) {
	dyn_save_flags();
	dyn_reduce_cycles();
	gen_dop_word_imm(DOP_ADD,true,DREG(EIP),decode.code-decode.code_start);
	dyn_releaseregs();
	gen_call_function((void*)&CPU_IRET,"%Id%Id",decode.big_op,decode.code-decode.op_start);
	dyn_load_flags();
	dyn_releaseregs();
	gen_return(BR_Normal);
	dyn_closeblock(BT_CheckFlags);
}

static void dyn_interrupt(Bitu num) {
	dyn_save_flags();
	dyn_reduce_cycles();
	gen_dop_word_imm(DOP_ADD,true,DREG(EIP),decode.code-decode.code_start);
	dyn_releaseregs();
	gen_call_function((void*)&CPU_Interrupt,"%Id%Id%Id",num,CPU_INT_SOFTWARE,decode.code-decode.op_start);
	dyn_load_flags();
	dyn_releaseregs();
	gen_return(BR_Normal);
	dyn_closeblock(BT_Normal);
}

static CacheBlock * CreateCacheBlock(PhysPt start,bool big,Bitu max_opcodes) {
	Bits i;
	decode.code_start=start;
	decode.code=start;
	Bitu cycles=0;
	decode.block=cache_openblock();
	gen_save_host_direct(&core_dyn.lastblock,(Bit32u)decode.block);
	for (i=0;i<G_MAX;i++) {
		DynRegs[i].flags&=~(DYNFLG_LOADONCE|DYNFLG_CHANGED);
		DynRegs[i].genreg=0;
	}
	gen_reinit();
	/* Start with the cycles check */
	gen_storeflags();
	gen_dop_word_imm(DOP_CMP,true,DREG(CYCLES),0);
	Bit8u * cyclebranch=gen_create_branch(BR_NLE);
	gen_restoreflags(true);
	gen_return(BR_Cycles);
	gen_fill_branch(cyclebranch);
	gen_releasereg(DREG(CYCLES));
	gen_restoreflags();
	decode.cycles=0;
	while (max_opcodes--) {
/* Init prefixes */
		decode.big_addr=big;
		decode.big_op=big;
		decode.segprefix=0;
		decode.rep=false;
		decode.cycles++;
		decode.op_start=decode.code;
restart_prefix:
		Bitu opcode=decode_fetchb();
		switch (opcode) {
			//Add
		case 0x00:dyn_dop_ebgb(DOP_ADD);break;
		case 0x01:dyn_dop_evgv(DOP_ADD);break;
		case 0x02:dyn_dop_gbeb(DOP_ADD);break;
		case 0x03:dyn_dop_gvev(DOP_ADD);break;
		case 0x04:gen_dop_byte_imm(DOP_ADD,DREG(EAX),0,decode_fetchb());break;
		case 0x05:gen_dop_word_imm(DOP_ADD,decode.big_op,DREG(EAX),decode.big_op ? decode_fetchd() :  decode_fetchw());break;
		case 0x06:dyn_push_seg(es);break;
		case 0x07:dyn_pop_seg(es);break;

		case 0x08:dyn_dop_ebgb(DOP_OR);break;
		case 0x09:dyn_dop_evgv(DOP_OR);break;
		case 0x0a:dyn_dop_gbeb(DOP_OR);break;
		case 0x0b:dyn_dop_gvev(DOP_OR);break;
		case 0x0c:gen_dop_byte_imm(DOP_OR,DREG(EAX),0,decode_fetchb());break;
		case 0x0d:gen_dop_word_imm(DOP_OR,decode.big_op,DREG(EAX),decode.big_op ? decode_fetchd() :  decode_fetchw());break;
		case 0x0e:dyn_push_seg(cs);break;
//TODO opcode 0x0f prefixed
		case 0x0f:
		{
			Bitu dual_code=decode_fetchb();
			switch (dual_code) {
			/* Short conditional jumps */
			case 0x80:case 0x81:case 0x82:case 0x83:case 0x84:case 0x85:case 0x86:case 0x87:	
			case 0x88:case 0x89:case 0x8a:case 0x8b:case 0x8c:case 0x8d:case 0x8e:case 0x8f:	
				dyn_branched_exit((BranchTypes)(dual_code&0xf),
					decode.big_op ? (Bit32s)decode_fetchd() : (Bit16s)decode_fetchw());	
				return decode.block;
			default:
				DYN_LOG("Unhandled dual opcode 0F%02X",dual_code);
				goto illegalopcode;
			}
		}break;
		case 0x10:dyn_dop_ebgb(DOP_ADC);break;
		case 0x11:dyn_dop_evgv(DOP_ADC);break;
		case 0x12:dyn_dop_gbeb(DOP_ADC);break;
		case 0x13:dyn_dop_gvev(DOP_ADC);break;
		case 0x14:gen_dop_byte_imm(DOP_ADC,DREG(EAX),0,decode_fetchb());break;
		case 0x15:gen_dop_word_imm(DOP_ADC,decode.big_op,DREG(EAX),decode.big_op ? decode_fetchd() :  decode_fetchw());break;
		case 0x16:dyn_push_seg(ss);break;
		case 0x17:dyn_pop_seg(ss);break;
		case 0x18:dyn_dop_ebgb(DOP_SBB);break;
		case 0x19:dyn_dop_evgv(DOP_SBB);break;
		case 0x1a:dyn_dop_gbeb(DOP_SBB);break;
		case 0x1b:dyn_dop_gvev(DOP_SBB);break;
		case 0x1c:gen_dop_byte_imm(DOP_SBB,DREG(EAX),0,decode_fetchb());break;
		case 0x1d:gen_dop_word_imm(DOP_SBB,decode.big_op,DREG(EAX),decode.big_op ? decode_fetchd() :  decode_fetchw());break;
		case 0x1e:dyn_push_seg(ds);break;
		case 0x1f:dyn_pop_seg(ds);break;

		case 0x20:dyn_dop_ebgb(DOP_AND);break;
		case 0x21:dyn_dop_evgv(DOP_AND);break;
		case 0x22:dyn_dop_gbeb(DOP_AND);break;
		case 0x23:dyn_dop_gvev(DOP_AND);break;
		case 0x24:gen_dop_byte_imm(DOP_AND,DREG(EAX),0,decode_fetchb());break;
		case 0x25:gen_dop_word_imm(DOP_AND,decode.big_op,DREG(EAX),decode.big_op ? decode_fetchd() :  decode_fetchw());break;
		case 0x26:dyn_segprefix(es);goto restart_prefix;

		case 0x28:dyn_dop_ebgb(DOP_SUB);break;
		case 0x29:dyn_dop_evgv(DOP_SUB);break;
		case 0x2a:dyn_dop_gbeb(DOP_SUB);break;
		case 0x2b:dyn_dop_gvev(DOP_SUB);break;
		case 0x2c:gen_dop_byte_imm(DOP_SUB,DREG(EAX),0,decode_fetchb());break;
		case 0x2d:gen_dop_word_imm(DOP_SUB,decode.big_op,DREG(EAX),decode.big_op ? decode_fetchd() :  decode_fetchw());break;
		case 0x2e:dyn_segprefix(cs);goto restart_prefix;

		case 0x30:dyn_dop_ebgb(DOP_XOR);break;
		case 0x31:dyn_dop_evgv(DOP_XOR);break;
		case 0x32:dyn_dop_gbeb(DOP_XOR);break;
		case 0x33:dyn_dop_gvev(DOP_XOR);break;
		case 0x34:gen_dop_byte_imm(DOP_XOR,DREG(EAX),0,decode_fetchb());break;
		case 0x35:gen_dop_word_imm(DOP_XOR,decode.big_op,DREG(EAX),decode.big_op ? decode_fetchd() :  decode_fetchw());break;
		case 0x36:dyn_segprefix(ss);goto restart_prefix;

		case 0x38:dyn_dop_ebgb(DOP_CMP);break;
		case 0x39:dyn_dop_evgv(DOP_CMP);break;
		case 0x3a:dyn_dop_gbeb(DOP_CMP);break;
		case 0x3b:dyn_dop_gvev(DOP_CMP);break;
		case 0x3c:gen_dop_byte_imm(DOP_CMP,DREG(EAX),0,decode_fetchb());break;
		case 0x3d:gen_dop_word_imm(DOP_CMP,decode.big_op,DREG(EAX),decode.big_op ? decode_fetchd() :  decode_fetchw());break;
		case 0x3e:dyn_segprefix(ds);goto restart_prefix;
			/* INC/DEC general register */

		case 0x40:case 0x41:case 0x42:case 0x43:case 0x44:case 0x45:case 0x46:case 0x47:	
			gen_sop_word(SOP_INC,decode.big_op,&DynRegs[opcode&7]);
			break;
		case 0x48:case 0x49:case 0x4a:case 0x4b:case 0x4c:case 0x4d:case 0x4e:case 0x4f:	
			gen_sop_word(SOP_DEC,decode.big_op,&DynRegs[opcode&7]);
			break;

		/* PUSH/POP General register */
		case 0x50:case 0x51:case 0x52:case 0x53:case 0x55:case 0x56:case 0x57:	
			dyn_push(&DynRegs[opcode&7]);
			break;
		case 0x54:		/* PUSH SP is special */
			gen_dop_word(DOP_MOV,true,DREG(TMPW),DREG(ESP));
			dyn_push(DREG(TMPW));
			gen_releasereg(DREG(TMPW));
			break;
		case 0x58:case 0x59:case 0x5a:case 0x5b:case 0x5c:case 0x5d:case 0x5e:case 0x5f:	
			dyn_pop(&DynRegs[opcode&7]);
			break;
		case 0x60:		/* PUSHA */
			gen_storeflags();
			gen_dop_word(DOP_MOV,true,DREG(TMPW),DREG(ESP));
			for (i=G_EAX;i<=G_EDI;i++) {
				dyn_push((i!=G_ESP) ? &DynRegs[i] : DREG(TMPW));
			}
			gen_restoreflags();
			gen_releasereg(DREG(TMPW));
			break;
		case 0x61:		/* POPA */
			gen_storeflags();
			for (i=G_EDI;i>=G_EAX;i--) {
				dyn_pop((i!=G_ESP) ? &DynRegs[i] : DREG(TMPW));
			}
			gen_restoreflags();gen_releasereg(DREG(TMPW));
			break;
		//segprefix FS,GS
		case 0x64:dyn_segprefix(fs);goto restart_prefix;
		case 0x65:dyn_segprefix(gs);goto restart_prefix;
		//Push immediates
		//Operand size		
		case 0x66:decode.big_op=!big;goto restart_prefix;
		//Address size		
		case 0x67:decode.big_addr=!big;goto restart_prefix;
		case 0x68:		/* PUSH Iv */
			gen_dop_word_imm(DOP_MOV,decode.big_op,DREG(TMPW),decode.big_op ? decode_fetchd() : decode_fetchw());
			dyn_push(DREG(TMPW));
			gen_releasereg(DREG(TMPW));
			break;
		case 0x6a:		/* PUSH Ibx */
			gen_dop_word_imm(DOP_MOV,true,DREG(TMPW),(Bit8s)decode_fetchb());
			dyn_push(DREG(TMPW));
			gen_releasereg(DREG(TMPW));
			break;
		/* Short conditional jumps */
		case 0x70:case 0x71:case 0x72:case 0x73:case 0x74:case 0x75:case 0x76:case 0x77:	
		case 0x78:case 0x79:case 0x7a:case 0x7b:case 0x7c:case 0x7d:case 0x7e:case 0x7f:	
			dyn_branched_exit((BranchTypes)(opcode&0xf),(Bit8s)decode_fetchb());	
			return decode.block;
			/* Group 1 */

		case 0x80:dyn_grp1_eb_ib();break;
		case 0x81:dyn_grp1_ev_ivx(false);break;
		case 0x82:dyn_grp1_eb_ib();break;
		case 0x83:dyn_grp1_ev_ivx(true);break;
			/* TEST Gb,Eb Gv,Ev */			//Can use G,E since results don't get saved
		case 0x84:dyn_dop_gbeb(DOP_TEST);break;
		case 0x85:dyn_dop_gvev(DOP_TEST);break;
		/* XCHG Eb,Gb Ev,Gv */
		case 0x86:dyn_dop_ebgb(DOP_XCHG);break;
		case 0x87:dyn_dop_evgv(DOP_XCHG);break;
		/* MOV e,g and g,e */
		case 0x88:dyn_mov_ebgb();break;
		case 0x89:dyn_mov_evgv();break;
		case 0x8a:dyn_mov_gbeb();break;
		case 0x8b:dyn_mov_gvev();break;
		/* MOV ev,seg */
		case 0x8c:dyn_mov_ev_seg();break;
		/* LEA Gv */
		case 0x8d:
			dyn_get_modrm();dyn_fill_ea(false);
			gen_dop_word(DOP_MOV,decode.big_op,&DynRegs[decode.modrm.reg],DREG(EA));
			gen_releasereg(DREG(EA));
			break;
		/* Mov seg,ev */
//		case 0x8e:dyn_mov_seg_ev();break;
		/* POP Ev */
		case 0x8f:dyn_pop_ev();break;
		//NOP
		case 0x90:
			break;
		//XCHG ax,reg
		case 0x91:case 0x92:case 0x93:case 0x94:case 0x95:case 0x96:case 0x97:	
			gen_dop_word(DOP_XCHG,decode.big_op,DREG(EAX),&DynRegs[opcode&07]);
			break;
		/* CBW/CWDE */
		case 0x98:
			if (decode.big_op) gen_extend_word(true,DREG(EAX),DREG(EAX));
			else gen_extend_byte(true,false,DREG(EAX),DREG(EAX),0);
			break;
		/* CALL FAR Ip */
		case 0x9a:dyn_call_far_imm();return decode.block;
			/* MOV AL,direct addresses */
		case 0xa0:
			gen_lea(DREG(EA),decode.segprefix ? decode.segprefix : DREG(DS),0,0,
				decode.big_addr ? decode_fetchd() : decode_fetchw());
			dyn_read_byte(DREG(EA),DREG(EAX),false);
			gen_releasereg(DREG(EA));
			break;
		/* MOV AX,direct addresses */
		case 0xa1:
			gen_lea(DREG(EA),decode.segprefix ? decode.segprefix : DREG(DS),0,0,
				decode.big_addr ? decode_fetchd() : decode_fetchw());
			dyn_read_word(DREG(EA),DREG(EAX),decode.big_op);
			gen_releasereg(DREG(EA));
			break;
		/* MOV direct address,AL */
		case 0xa2:
			gen_lea(DREG(EA),decode.segprefix ? decode.segprefix : DREG(DS),0,0,
				decode.big_addr ? decode_fetchd() : decode_fetchw());
			dyn_write_byte(DREG(EA),DREG(EAX),false);
			gen_releasereg(DREG(EA));
			break;
		/* MOV direct addresses,AX */
		case 0xa3:
			gen_lea(DREG(EA),decode.segprefix ? decode.segprefix : DREG(DS),0,0,
				decode.big_addr ? decode_fetchd() : decode_fetchw());
			dyn_write_word(DREG(EA),DREG(EAX),decode.big_op);
			gen_releasereg(DREG(EA));
			break;
		/* TEST AL,AX Imm */
		case 0xa8:gen_dop_byte_imm(DOP_TEST,DREG(EAX),0,decode_fetchb());break;
		case 0xa9:gen_dop_word_imm(DOP_TEST,decode.big_op,DREG(EAX),decode.big_op ? decode_fetchd() :  decode_fetchw());break;

		//Mov Byte reg,Imm byte
		case 0xb0:case 0xb1:case 0xb2:case 0xb3:case 0xb4:case 0xb5:case 0xb6:case 0xb7:	
			gen_dop_byte_imm(DOP_MOV,&DynRegs[opcode&3],opcode&4,decode_fetchb());
			break;
		//Mov word reg imm byte,word,
		case 0xb8:case 0xb9:case 0xba:case 0xbb:case 0xbc:case 0xbd:case 0xbe:case 0xbf:	
			gen_dop_word_imm(DOP_MOV,decode.big_op,&DynRegs[opcode&7],decode.big_op ? decode_fetchd() :  decode_fetchw());break;
			break;
		//GRP2 Eb/Ev,Ib
		case 0xc0:dyn_grp2_eb(grp2_imm);break;
		case 0xc1:dyn_grp2_ev(grp2_imm);break;
		//RET near Iw / Ret
		case 0xc2:dyn_ret_near(decode_fetchw());return decode.block;			
		case 0xc3:dyn_ret_near(0);return decode.block;
		// MOV Eb/Ev,Ib/Iv
		case 0xc6:dyn_mov_ebib();break;
		case 0xc7:dyn_mov_eviv();break;
		// LEAVE 
		case 0xc9:dyn_leave();break;
		//RET far Iw / Ret
		case 0xca:dyn_ret_far(decode_fetchw());return decode.block;			
		case 0xcb:dyn_ret_far(0);return decode.block;
		/* Interrupt */
		case 0xcd:dyn_interrupt(decode_fetchb());return decode.block;
		/* IRET */
		case 0xcF:dyn_iret();return decode.block;
		//GRP2 Eb/Ev,1
		case 0xd0:dyn_grp2_eb(grp2_1);break;
		case 0xd1:dyn_grp2_ev(grp2_1);break;
		//GRP2 Eb/Ev,CL
		case 0xd2:dyn_grp2_eb(grp2_cl);break;
		case 0xd3:dyn_grp2_ev(grp2_cl);break;
		//IN AL/AX,imm
		case 0xe2:dyn_loop(LOOP_NONE);return decode.block;
		case 0xe4:gen_call_function((void*)&IO_ReadB,"%Id%Rl",decode_fetchb(),DREG(EAX));break;
		case 0xe5:
			if (decode.big_op) {
                gen_call_function((void*)&IO_ReadD,"%Id%Rd",decode_fetchb(),DREG(EAX));
			} else {
				gen_call_function((void*)&IO_ReadW,"%Id%Rw",decode_fetchb(),DREG(EAX));
			}
			break;
		//OUT imm,AL
		case 0xe6:gen_call_function((void*)&IO_WriteB,"%Id%Dl",decode_fetchb(),DREG(EAX));break;
		case 0xe7:
			if (decode.big_op) {
                gen_call_function((void*)&IO_WriteD,"%Id%Dd",decode_fetchb(),DREG(EAX));
			} else {
				gen_call_function((void*)&IO_WriteW,"%Id%Dw",decode_fetchb(),DREG(EAX));
			}
			break;
		case 0xe8:		/* CALL Ivx */
			dyn_call_near_imm();
			return decode.block;
		case 0xe9:		/* Jmp Ivx */
			dyn_exit_link(decode.big_op,decode.big_op ? (Bit32s)decode_fetchd() : (Bit16s)decode_fetchw());
			return decode.block;
		/* CALL FAR Ip */
		case 0xea:dyn_jmp_far_imm();return decode.block;
		/* Jmp Ibx */
		case 0xeb:dyn_exit_link(decode.big_op,(Bit8s)decode_fetchb());return decode.block;
		/* IN AL/AX,DX*/
		case 0xec:gen_call_function((void*)&IO_ReadB,"%Dw%Rl",DREG(EDX),DREG(EAX));break;
		case 0xed:
			if (decode.big_op) {
                gen_call_function((void*)&IO_ReadD,"%Dw%Rd",DREG(EDX),DREG(EAX));
			} else {
				gen_call_function((void*)&IO_ReadW,"%Dw%Rw",DREG(EDX),DREG(EAX));
			}
			break;
		/* OUT DX,AL/AX */
		case 0xee:gen_call_function((void*)&IO_WriteB,"%Dw%Dl",DREG(EDX),DREG(EAX));break;
		case 0xef:
			if (decode.big_op) {
                gen_call_function((void*)&IO_WriteD,"%Dw%Dd",DREG(EDX),DREG(EAX));
			} else {
				gen_call_function((void*)&IO_WriteW,"%Dw%Dw",DREG(EDX),DREG(EAX));
			}
			break;
		/* Change carry flag */
		case 0xf5:		//CMC
		case 0xf8:		//CLC
		case 0xf9:		//STC
			cache_addb(opcode);break;
		/* GRP 3 Eb/EV */
		case 0xf6:dyn_grp3_eb();break;
		case 0xf7:dyn_grp3_ev();break;
		/* Change interrupt flag */
		case 0xfa:		//CLI
			gen_storeflags();
			gen_dop_word_imm(DOP_AND,true,DREG(FLAGS),~FLAG_IF);
			gen_restoreflags();
			break;
		case 0xfb:		//STI
			gen_storeflags();
			gen_dop_word_imm(DOP_OR,true,DREG(FLAGS),FLAG_IF);
			gen_restoreflags();
			if (max_opcodes<=0) max_opcodes=1;		//Allow 1 extra opcode
			break;
		/* GRP 4 Eb and callback's */
		case 0xfe:
            dyn_get_modrm();
			switch (decode.modrm.reg) {
			case 0x0://INC Eb
			case 0x1://DEC Eb
				if (decode.modrm.mod<3) {
					dyn_fill_ea();dyn_read_byte(DREG(EA),DREG(TMPB),false);
					gen_sop_byte(decode.modrm.reg==0 ? SOP_INC : SOP_DEC,DREG(TMPB),0);
					dyn_write_byte(DREG(EA),DREG(TMPB),false);
					gen_releasereg(DREG(EA));gen_releasereg(DREG(TMPB));
				} else {
					gen_sop_byte(decode.modrm.reg==0 ? SOP_INC : SOP_DEC,
						&DynRegs[decode.modrm.rm&3],decode.modrm.rm&4);
				}
				break;
			case 0x7:		//CALBACK Iw
				gen_save_host_direct(&core_dyn.callback,decode_fetchw());
				gen_lea(DREG(EIP),DREG(EIP),0,0,decode.code-decode.code_start);
				dyn_reduce_cycles();
				dyn_releaseregs();
				gen_return(BR_CallBack);
				dyn_closeblock(BT_Normal);
				return decode.block;
			}
			break;
		case 0xff: 
			{
            dyn_get_modrm();DynReg * src;
			if (decode.modrm.mod<3) {
					dyn_fill_ea();
					dyn_read_word(DREG(EA),DREG(TMPW),decode.big_op);
					src=DREG(TMPW);
			} else src=&DynRegs[decode.modrm.rm];
			switch (decode.modrm.reg) {
			case 0x0://INC Ev
			case 0x1://DEC Ev
				gen_sop_word(decode.modrm.reg==0 ? SOP_INC : SOP_DEC,decode.big_op,src);
				if (decode.modrm.mod<3){
					dyn_write_word(DREG(EA),DREG(TMPW),decode.big_op);
					gen_releasereg(DREG(EA));gen_releasereg(DREG(TMPW));
				}
				break;
			case 0x2:	/* CALL Ev */
				gen_lea(DREG(EIP),DREG(EIP),0,0,decode.code-decode.code_start);
				dyn_push(DREG(EIP));
				gen_dop_word(DOP_MOV,decode.big_op,DREG(EIP),src);
				goto core_close_block;
			case 0x4:	/* JMP Ev */
				gen_dop_word(DOP_MOV,decode.big_op,DREG(EIP),src);
				goto core_close_block;
			case 0x3:	/* CALL Ep */
			case 0x5:	/* JMP Ep */
				dyn_save_flags();
				gen_lea(DREG(EA),DREG(EA),0,0,decode.big_op ? 4: 2);
				gen_lea(DREG(EIP),DREG(EIP),0,0,decode.code-decode.code_start);
				dyn_read_word(DREG(EA),DREG(EA),false);
				for (Bitu i=0;i<G_MAX;i++) if (i!= G_EA && i!=G_TMPW) gen_releasereg(&DynRegs[i]);
				gen_call_function(
					decode.modrm.reg == 3 ? (void*)&CPU_CALL : (void*)&CPU_JMP,
					decode.big_op ? (char *)"%Id%Drw%Drd%Id" : (char *)"%Id%Drw%Drw%Id",
					decode.big_op,DREG(EA),DREG(TMPW),decode.code-decode.op_start);
				dyn_load_flags();
				goto core_close_block;
			case 0x6:		/* PUSH Ev */
				dyn_push(src);
				break;
			default:
				IllegalOption();
			}}
			break;
		default:
			DYN_LOG("Dynamic unhandled opcode %X",opcode);
			goto illegalopcode;
		}
	}
	/* Normal exit because of max opcodes reached */
	gen_lea(DREG(EIP),DREG(EIP),0,0,decode.code-decode.code_start);
core_close_block:
	dyn_reduce_cycles();
	dyn_releaseregs();
	gen_return(BR_Normal);
	dyn_closeblock(BT_Normal);
	return decode.block;
illegalopcode:
	decode.code=decode.op_start;
	gen_lea(DREG(EIP),DREG(EIP),0,0,decode.code-decode.code_start);
	dyn_reduce_cycles();
	dyn_releaseregs();
	gen_return(BR_Opcode);
	dyn_closeblock(BT_Normal);
	return decode.block;
}
