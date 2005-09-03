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

#include "fpu.h"
#define DYN_FPU_ESC(code) {					\
	dyn_get_modrm(); \
	if (decode.modrm.val >= 0xc0) { \
		gen_call_function((void*)&FPU_ESC ## code ## _Normal,"%Id",decode.modrm.val); \
	} else { \
		dyn_fill_ea(); \
		gen_call_function((void*)&FPU_ESC ## code ## _EA,"%Id%Dd",decode.modrm.val,DREG(EA)); \
		gen_releasereg(DREG(EA)); \
	} \
}

enum REP_Type {
	REP_NONE=0,REP_NZ,REP_Z
};

static struct DynDecode {
	PhysPt code;
	PhysPt code_start;
	PhysPt op_start;
	bool big_op;
	bool big_addr;
	REP_Type rep;
	Bitu cycles;
	CacheBlock * block;
	CacheBlock * active_block;
	struct {
		CodePageHandler * code;	
		Bitu index;
		Bit8u * wmap;
		Bitu first;
	} page;
	struct {
		Bitu val;
		Bitu mod;
		Bitu rm;
		Bitu reg;
	} modrm;
	DynReg * segprefix;
} decode;

static Bit8u decode_fetchb(void) {
	if (decode.page.index>=4096) {
        /* Advance to the next page */
		decode.active_block->page.end=4095;
		decode.page.code=MakeCodePage(++decode.page.first);
		CacheBlock * newblock=cache_getblock();
		decode.active_block->crossblock=newblock;
		newblock->crossblock=decode.active_block;
		decode.active_block=newblock;
		decode.active_block->page.start=0;
		decode.page.code->AddCrossBlock(decode.active_block);
		decode.page.wmap=decode.page.code->write_map;
		decode.page.index=0;
	}
	decode.page.wmap[decode.page.index]+=0x01;
	decode.page.index++;
	decode.code+=1;
	return mem_readb(decode.code-1);
}
static Bit16u decode_fetchw(void) {
	if (decode.page.index>=4095) {
   		Bit16u val=decode_fetchb();
		val|=decode_fetchb() << 8;
		return val;
	}
	*(Bit16u *)&decode.page.wmap[decode.page.index]+=0x0101;
	decode.code+=2;decode.page.index+=2;
	return mem_readw(decode.code-2);
}
static Bit32u decode_fetchd(void) {
	if (decode.page.index>=4093) {
   		Bit32u val=decode_fetchb();
		val|=decode_fetchb() << 8;
		val|=decode_fetchb() << 16;
		val|=decode_fetchb() << 24;
		return val;
        /* Advance to the next page */
	}
	*(Bit32u *)&decode.page.wmap[decode.page.index]+=0x01010101;
	decode.code+=4;decode.page.index+=4;
	return mem_readd(decode.code-4);
}

static void dyn_read_byte(DynReg * addr,DynReg * dst,Bitu high) {
	if (high) gen_call_function((void *)&mem_readb,"%Dd%Rh",addr,dst);
	else gen_call_function((void *)&mem_readb,"%Dd%Rl",addr,dst);
}
static void dyn_write_byte(DynReg * addr,DynReg * val,Bitu high) {
	if (high) gen_call_function((void *)&mem_writeb,"%Dd%Dh",addr,val);
	else gen_call_function((void *)&mem_writeb,"%Dd%Dd",addr,val);
}
static void dyn_read_word(DynReg * addr,DynReg * dst,bool dword) {
	if (dword) gen_call_function((void *)&mem_readd_dyncorex86,"%Dd%Rd",addr,dst);
	else gen_call_function((void *)&mem_readw_dyncorex86,"%Dd%Rw",addr,dst);
}
static void dyn_write_word(DynReg * addr,DynReg * val,bool dword) {
	if (dword) gen_call_function((void *)&mem_writed_dyncorex86,"%Dd%Dd",addr,val);
	else gen_call_function((void *)&mem_writew_dyncorex86,"%Dd%Dd",addr,val);
}


static void dyn_read_byte_release(DynReg * addr,DynReg * dst,Bitu high) {
	if (high) gen_call_function((void *)&mem_readb,"%Drd%Rh",addr,dst);
	else gen_call_function((void *)&mem_readb,"%Drd%Rl",addr,dst);
}
static void dyn_write_byte_release(DynReg * addr,DynReg * val,Bitu high) {
	if (high) gen_call_function((void *)&mem_writeb,"%Drd%Dh",addr,val);
	else gen_call_function((void *)&mem_writeb,"%Drd%Dd",addr,val);
}
static void dyn_read_word_release(DynReg * addr,DynReg * dst,bool dword) {
	if (dword) gen_call_function((void *)&mem_readd_dyncorex86,"%Drd%Rd",addr,dst);
	else gen_call_function((void *)&mem_readw_dyncorex86,"%Drd%Rw",addr,dst);
}
static void dyn_write_word_release(DynReg * addr,DynReg * val,bool dword) {
	if (dword) gen_call_function((void *)&mem_writed_dyncorex86,"%Drd%Dd",addr,val);
	else gen_call_function((void *)&mem_writew_dyncorex86,"%Drd%Dd",addr,val);
}


static void dyn_reduce_cycles(void) {
	gen_protectflags();
	if (!decode.cycles) decode.cycles++;
	gen_dop_word_imm(DOP_SUB,true,DREG(CYCLES),decode.cycles);
}

static void dyn_save_noncritical_regs(void) {
	gen_releasereg(DREG(EAX));
	gen_releasereg(DREG(ECX));
	gen_releasereg(DREG(EDX));
	gen_releasereg(DREG(EBX));
	gen_releasereg(DREG(ESP));
	gen_releasereg(DREG(EBP));
	gen_releasereg(DREG(ESI));
	gen_releasereg(DREG(EDI));
}

static void dyn_save_critical_regs(void) {
	dyn_save_noncritical_regs();
	gen_releasereg(DREG(FLAGS));
	gen_releasereg(DREG(EIP));
	gen_releasereg(DREG(CYCLES));
}

static void dyn_set_eip_last_end(DynReg * endreg) {
	gen_protectflags();
	gen_lea(endreg,DREG(EIP),0,0,decode.code-decode.code_start);
	gen_dop_word_imm(DOP_ADD,decode.big_op,DREG(EIP),decode.op_start-decode.code_start);
}

 static INLINE void dyn_set_eip_end(void) {
 	gen_protectflags();
	gen_dop_word_imm(DOP_ADD,cpu.code.big,DREG(EIP),decode.code-decode.code_start);
 }
 
 static INLINE void dyn_set_eip_last(void) {
 	gen_protectflags();
	gen_dop_word_imm(DOP_ADD,cpu.code.big,DREG(EIP),decode.op_start-decode.code_start);
}

static void dyn_push(DynReg * dynreg) {
	gen_protectflags();
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
}

static void dyn_pop(DynReg * dynreg) {
	gen_protectflags();
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
}

static void INLINE dyn_get_modrm(void) {
	decode.modrm.val=decode_fetchb();
	decode.modrm.mod=(decode.modrm.val >> 6) & 3;
	decode.modrm.reg=(decode.modrm.val >> 3) & 7;
	decode.modrm.rm=(decode.modrm.val & 7);
}

static void dyn_fill_ea(bool addseg=true, DynReg * reg_ea=DREG(EA)) {
	DynReg * segbase;
	if (!decode.big_addr) {
		Bits imm;
		switch (decode.modrm.mod) {
		case 0:imm=0;break;
		case 1:imm=(Bit8s)decode_fetchb();break;
		case 2:imm=(Bit16s)decode_fetchw();break;
		}
		DynReg * extend_src=reg_ea;
		switch (decode.modrm.rm) {
		case 0:/* BX+SI */
			gen_lea(reg_ea,DREG(EBX),DREG(ESI),0,imm);
			segbase=DREG(DS);
			break;
		case 1:/* BX+DI */
			gen_lea(reg_ea,DREG(EBX),DREG(EDI),0,imm);
			segbase=DREG(DS);
			break;
		case 2:/* BP+SI */
			gen_lea(reg_ea,DREG(EBP),DREG(ESI),0,imm);
			segbase=DREG(SS);
			break;
		case 3:/* BP+DI */
			gen_lea(reg_ea,DREG(EBP),DREG(EDI),0,imm);
			segbase=DREG(SS);
			break;
		case 4:/* SI */
			if (imm) gen_lea(reg_ea,DREG(ESI),0,0,imm);
			else extend_src=DREG(ESI);
			segbase=DREG(DS);
			break;
		case 5:/* DI */
			if (imm) gen_lea(reg_ea,DREG(EDI),0,0,imm);
			else extend_src=DREG(EDI);
			segbase=DREG(DS);
			break;
		case 6:/* imm/BP */
			if (!decode.modrm.mod) {
				imm=decode_fetchw();
                gen_dop_word_imm(DOP_MOV,true,reg_ea,imm);
				segbase=DREG(DS);
				goto skip_extend_word;
			} else {
				gen_lea(reg_ea,DREG(EBP),0,0,imm);
				segbase=DREG(SS);
			}
			break;
		case 7: /* BX */
			if (imm) gen_lea(reg_ea,DREG(EBX),0,0,imm);
			else extend_src=DREG(EBX);
			segbase=DREG(DS);
			break;
		}
		gen_extend_word(false,reg_ea,extend_src);
skip_extend_word:
		if (addseg) {
			gen_lea(reg_ea,reg_ea,decode.segprefix ? decode.segprefix : segbase,0,0);
		}
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
		if (!addseg) {
			gen_lea(reg_ea,base,scaled,scale,imm);
		} else {
			DynReg** seg = decode.segprefix ? &decode.segprefix : &segbase;
			if (!base) gen_lea(reg_ea,*seg,scaled,scale,imm);
			else if (!scaled) gen_lea(reg_ea,base,*seg,0,imm);
			else {
				gen_lea(reg_ea,base,scaled,scale,imm);
				gen_lea(reg_ea,reg_ea,decode.segprefix ? decode.segprefix : segbase,0,0);
			}
		}
	}
}

#include "helpers.h"
#include "string.h"

static void DynRunException(void) {
	CPU_Exception(cpu.exception.which,cpu.exception.error);
}

static void dyn_check_bool_exception(DynReg * check) {
	Bit8u * branch;DynState state;
	gen_dop_byte(DOP_OR,check,0,check,0);
	branch=gen_create_branch(BR_Z);
	dyn_savestate(&state);
	dyn_flags_gen_to_host();
	dyn_reduce_cycles();
	dyn_set_eip_last();
	dyn_save_critical_regs();
	gen_call_function((void *)&DynRunException,"");
	dyn_flags_host_to_gen();
	gen_return(BR_Normal);
	dyn_loadstate(&state);
	gen_fill_branch(branch);
}


static void dyn_dop_ebgb(DualOps op) {
	dyn_get_modrm();DynReg * rm_reg=&DynRegs[decode.modrm.reg&3];
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		if ((op<=DOP_TEST) && (op!=DOP_ADC && op!=DOP_SBB)) set_skipflags(true);
		dyn_read_byte(DREG(EA),DREG(TMPB),false);
		if (op<=DOP_TEST) {
			if (op==DOP_ADC || op==DOP_SBB) gen_needcarry();
			else set_skipflags(false);
		}
		gen_dop_byte(op,DREG(TMPB),0,rm_reg,decode.modrm.reg&4);
		if (op!=DOP_CMP) dyn_write_byte_release(DREG(EA),DREG(TMPB),false);
		else gen_releasereg(DREG(EA));
		gen_releasereg(DREG(TMPB));
	} else {
		if (op<=DOP_TEST) {
			if (op==DOP_ADC || op==DOP_SBB) gen_needcarry();
			else gen_discardflags();
		}
		gen_dop_byte(op,&DynRegs[decode.modrm.rm&3],decode.modrm.rm&4,rm_reg,decode.modrm.reg&4);
	}
}


static void dyn_dop_gbeb(DualOps op) {
	dyn_get_modrm();DynReg * rm_reg=&DynRegs[decode.modrm.reg&3];
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		if ((op<=DOP_TEST) && (op!=DOP_ADC && op!=DOP_SBB)) set_skipflags(true);
		dyn_read_byte_release(DREG(EA),DREG(TMPB),false);
		if (op<=DOP_TEST) {
			if (op==DOP_ADC || op==DOP_SBB) gen_needcarry();
			else set_skipflags(false);
		}
		gen_dop_byte(op,rm_reg,decode.modrm.reg&4,DREG(TMPB),0);
		gen_releasereg(DREG(TMPB));
	} else {
		if (op<=DOP_TEST) {
			if (op==DOP_ADC || op==DOP_SBB) gen_needcarry();
			else gen_discardflags();
		}
		gen_dop_byte(op,rm_reg,decode.modrm.reg&4,&DynRegs[decode.modrm.rm&3],decode.modrm.rm&4);
	}
}

static void dyn_mov_ebib(void) {
	dyn_get_modrm();
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		gen_call_write(DREG(EA),decode_fetchb(),1);
	} else {
		gen_dop_byte_imm(DOP_MOV,&DynRegs[decode.modrm.rm&3],decode.modrm.rm&4,decode_fetchb());
	}
}

static void dyn_mov_ebgb(void) {
	dyn_get_modrm();
	DynReg * rm_reg=&DynRegs[decode.modrm.reg&3];Bitu rm_regi=decode.modrm.reg&4;
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		dyn_write_byte_release(DREG(EA),rm_reg,rm_regi);
	} else {
		gen_dop_byte(DOP_MOV,&DynRegs[decode.modrm.rm&3],decode.modrm.rm&4,rm_reg,rm_regi);
	}
}

static void dyn_mov_gbeb(void) {
	dyn_get_modrm();
	DynReg * rm_reg=&DynRegs[decode.modrm.reg&3];Bitu rm_regi=decode.modrm.reg&4;
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		dyn_read_byte_release(DREG(EA),rm_reg,rm_regi);
	} else {
		gen_dop_byte(DOP_MOV,rm_reg,rm_regi,&DynRegs[decode.modrm.rm&3],decode.modrm.rm&4);
	}
}

static void dyn_dop_evgv(DualOps op) {
	dyn_get_modrm();
	DynReg * rm_reg=&DynRegs[decode.modrm.reg];
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		if ((op<=DOP_TEST) && (op!=DOP_ADC && op!=DOP_SBB)) set_skipflags(true);
		dyn_read_word(DREG(EA),DREG(TMPW),decode.big_op);
		if (op<=DOP_TEST) {
			if (op==DOP_ADC || op==DOP_SBB) gen_needcarry();
			else set_skipflags(false);
		}
		gen_dop_word(op,decode.big_op,DREG(TMPW),rm_reg);
		if (op!=DOP_CMP) dyn_write_word_release(DREG(EA),DREG(TMPW),decode.big_op);
		else gen_releasereg(DREG(EA));
		gen_releasereg(DREG(TMPW));
	} else {
		if (op<=DOP_TEST) {
			if (op==DOP_ADC || op==DOP_SBB) gen_needcarry();
			else gen_discardflags();
		}
		gen_dop_word(op,decode.big_op,&DynRegs[decode.modrm.rm],rm_reg);
	}
}

static void dyn_imul_gvev(Bitu immsize) {
	dyn_get_modrm();DynReg * src;
	DynReg * rm_reg=&DynRegs[decode.modrm.reg];
	if (decode.modrm.mod<3) {
		dyn_fill_ea();dyn_read_word_release(DREG(EA),DREG(TMPW),decode.big_op);
		src=DREG(TMPW);
	} else {
		src=&DynRegs[decode.modrm.rm];
	}
	gen_needflags();
	switch (immsize) {
	case 0:gen_imul_word(decode.big_op,rm_reg,src);break;
	case 1:gen_imul_word_imm(decode.big_op,rm_reg,src,(Bit8s)decode_fetchb());break;
	case 2:gen_imul_word_imm(decode.big_op,rm_reg,src,(Bit16s)decode_fetchw());break;
	case 4:gen_imul_word_imm(decode.big_op,rm_reg,src,(Bit32s)decode_fetchd());break;
	}
	gen_releasereg(DREG(TMPW));
}

static void dyn_dop_gvev(DualOps op) {
	dyn_get_modrm();
	DynReg * rm_reg=&DynRegs[decode.modrm.reg];
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		if ((op<=DOP_TEST) && (op!=DOP_ADC && op!=DOP_SBB)) set_skipflags(true);
		dyn_read_word_release(DREG(EA),DREG(TMPW),decode.big_op);
		if (op<=DOP_TEST) {
			if (op==DOP_ADC || op==DOP_SBB) gen_needcarry();
			else set_skipflags(false);
		}
		gen_dop_word(op,decode.big_op,rm_reg,DREG(TMPW));
		gen_releasereg(DREG(TMPW));
	} else {
		if (op<=DOP_TEST) {
			if (op==DOP_ADC || op==DOP_SBB) gen_needcarry();
			else gen_discardflags();
		}
		gen_dop_word(op,decode.big_op,rm_reg,&DynRegs[decode.modrm.rm]);
	}
}

static void dyn_mov_evgv(void) {
	dyn_get_modrm();
	DynReg * rm_reg=&DynRegs[decode.modrm.reg];
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		dyn_write_word_release(DREG(EA),rm_reg,decode.big_op);
	} else {
		gen_dop_word(DOP_MOV,decode.big_op,&DynRegs[decode.modrm.rm],rm_reg);
	}
}

static void dyn_mov_gvev(void) {
	dyn_get_modrm();
	DynReg * rm_reg=&DynRegs[decode.modrm.reg];
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		dyn_read_word_release(DREG(EA),rm_reg,decode.big_op);
	} else {
		gen_dop_word(DOP_MOV,decode.big_op,rm_reg,&DynRegs[decode.modrm.rm]);
	}
}
static void dyn_mov_eviv(void) {
	dyn_get_modrm();
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		gen_call_write(DREG(EA),decode.big_op ? decode_fetchd() : decode_fetchw(),decode.big_op?4:2);
	} else {
		gen_dop_word_imm(DOP_MOV,decode.big_op,&DynRegs[decode.modrm.rm],decode.big_op ? decode_fetchd() : decode_fetchw());
	}
}

static void dyn_mov_ev_gb(bool sign) {
	dyn_get_modrm();DynReg * rm_reg=&DynRegs[decode.modrm.reg];
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		dyn_read_byte_release(DREG(EA),DREG(TMPB),false);
		gen_extend_byte(sign,decode.big_op,rm_reg,DREG(TMPB),0);
		gen_releasereg(DREG(TMPB));
	} else {
		gen_extend_byte(sign,decode.big_op,rm_reg,&DynRegs[decode.modrm.rm&3],decode.modrm.rm&4);
	}
}

static void dyn_mov_ev_gw(bool sign) {
	if (!decode.big_op) {
		dyn_mov_gvev();
		return;
	}
	dyn_get_modrm();DynReg * rm_reg=&DynRegs[decode.modrm.reg];
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		dyn_read_word_release(DREG(EA),DREG(TMPW),false);
		gen_extend_word(sign,rm_reg,DREG(TMPW));
		gen_releasereg(DREG(TMPW));
	} else {
		gen_extend_word(sign,rm_reg,&DynRegs[decode.modrm.rm]);
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
	gen_needflags();
	if (immediate) gen_dshift_imm(decode.big_op,left,ea_reg,rm_reg,decode_fetchb());
	else gen_dshift_cl(decode.big_op,left,ea_reg,rm_reg,DREG(ECX));
	if (decode.modrm.mod<3) {
		dyn_write_word_release(DREG(EA),DREG(TMPW),decode.big_op);
		gen_releasereg(DREG(TMPW));
	}
}


static DualOps grp1_table[8]={DOP_ADD,DOP_OR,DOP_ADC,DOP_SBB,DOP_AND,DOP_SUB,DOP_XOR,DOP_CMP};
static void dyn_grp1_eb_ib(void) {
	dyn_get_modrm();
	DualOps op=grp1_table[decode.modrm.reg];
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		if ((op<=DOP_TEST) && (op!=DOP_ADC && op!=DOP_SBB)) set_skipflags(true);
		dyn_read_byte(DREG(EA),DREG(TMPB),false);
		if (op<=DOP_TEST) {
			if (op==DOP_ADC || op==DOP_SBB) gen_needcarry();
			else set_skipflags(false);
		}
		gen_dop_byte_imm(grp1_table[decode.modrm.reg],DREG(TMPB),0,decode_fetchb());
		if (op!=DOP_CMP) dyn_write_byte_release(DREG(EA),DREG(TMPB),false);
		else gen_releasereg(DREG(EA));
		gen_releasereg(DREG(TMPB));
	} else {
		if (op<=DOP_TEST) {
			if (op==DOP_ADC || op==DOP_SBB) gen_needcarry();
			else gen_discardflags();
		}
		gen_dop_byte_imm(grp1_table[decode.modrm.reg],&DynRegs[decode.modrm.rm&3],decode.modrm.rm&4,decode_fetchb());
	}
}

static void dyn_grp1_ev_ivx(bool withbyte) {
	dyn_get_modrm();
	DualOps op=grp1_table[decode.modrm.reg];
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		if ((op<=DOP_TEST) && (op!=DOP_ADC && op!=DOP_SBB)) set_skipflags(true);
		dyn_read_word(DREG(EA),DREG(TMPW),decode.big_op);
		Bits imm=withbyte ? (Bit8s)decode_fetchb() : (decode.big_op ? decode_fetchd(): decode_fetchw()); 
		if (op<=DOP_TEST) {
			if (op==DOP_ADC || op==DOP_SBB) gen_needcarry();
			else set_skipflags(false);
		}
		gen_dop_word_imm(grp1_table[decode.modrm.reg],decode.big_op,DREG(TMPW),imm);
		if (op!=DOP_CMP) dyn_write_word_release(DREG(EA),DREG(TMPW),decode.big_op);
		else gen_releasereg(DREG(EA));
		gen_releasereg(DREG(TMPW));
	} else {
		Bits imm=withbyte ? (Bit8s)decode_fetchb() : (decode.big_op ? decode_fetchd(): decode_fetchw()); 
		if (op<=DOP_TEST) {
			if (op==DOP_ADC || op==DOP_SBB) gen_needcarry();
			else gen_discardflags();
		}
		gen_dop_word_imm(grp1_table[decode.modrm.reg],decode.big_op,&DynRegs[decode.modrm.rm],imm);
	}
}

enum grp2_types {
	grp2_1,grp2_imm,grp2_cl,
};

static void dyn_grp2_eb(grp2_types type) {
	dyn_get_modrm();DynReg * src;Bit8u src_i;
	if (decode.modrm.mod<3) {
		dyn_fill_ea();dyn_read_byte(DREG(EA),DREG(TMPB),false);
		src=DREG(TMPB);
		src_i=0;
	} else {
		src=&DynRegs[decode.modrm.rm&3];
		src_i=decode.modrm.rm&4;
	}
	switch (type) {
	case grp2_1:
		/* rotates (first 4 ops) alter cf/of only; shifts (last 4 ops) alter all flags */
		if (decode.modrm.reg < 4) gen_needflags();
		else gen_discardflags();
		gen_shift_byte_imm(decode.modrm.reg,src,src_i,1);
		break;
	case grp2_imm: {
		Bit8u imm=decode_fetchb();
		if (imm) {
			/* rotates (first 4 ops) alter cf/of only; shifts (last 4 ops) alter all flags */
			if (decode.modrm.reg < 4) gen_needflags();
			else gen_discardflags();
			gen_shift_byte_imm(decode.modrm.reg,src,src_i,imm);
		} else return;
		}
		break;
	case grp2_cl:
		gen_needflags();	/* flags must not be changed on ecx==0 */
		gen_shift_byte_cl (decode.modrm.reg,src,src_i,DREG(ECX));
		break;
	}
	if (decode.modrm.mod<3) {
		dyn_write_byte_release(DREG(EA),src,false);
		gen_releasereg(src);
	}
}

static void dyn_grp2_ev(grp2_types type) {
	dyn_get_modrm();DynReg * src;
	if (decode.modrm.mod<3) {
		dyn_fill_ea();dyn_read_word(DREG(EA),DREG(TMPW),decode.big_op);
		src=DREG(TMPW);
	} else {
		src=&DynRegs[decode.modrm.rm];
	}
	switch (type) {
	case grp2_1:
		/* rotates (first 4 ops) alter cf/of only; shifts (last 4 ops) alter all flags */
		if (decode.modrm.reg < 4) gen_needflags();
		else gen_discardflags();
		gen_shift_word_imm(decode.modrm.reg,decode.big_op,src,1);
		break;
	case grp2_imm: {
		Bit8u imm=decode_fetchb();
		if (imm) {
			/* rotates (first 4 ops) alter cf/of only; shifts (last 4 ops) alter all flags */
			if (decode.modrm.reg < 4) gen_needflags();
			else gen_discardflags();
			gen_shift_word_imm(decode.modrm.reg,decode.big_op,src,imm);
		} else return;
		}
		break;
	case grp2_cl:
		gen_needflags();	/* flags must not be changed on ecx==0 */
		gen_shift_word_cl (decode.modrm.reg,decode.big_op,src,DREG(ECX));
		break;
	}
	if (decode.modrm.mod<3) {
		dyn_write_word_release(DREG(EA),src,decode.big_op);
		gen_releasereg(src);
	}
}

static void dyn_grp3_eb(void) {
	dyn_get_modrm();DynReg * src;Bit8u src_i;
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		if ((decode.modrm.reg==0) || (decode.modrm.reg==3)) set_skipflags(true);
		dyn_read_byte(DREG(EA),DREG(TMPB),false);
		src=DREG(TMPB);src_i=0;
	} else {
		src=&DynRegs[decode.modrm.rm&3];
		src_i=decode.modrm.rm&4;
	}
	switch (decode.modrm.reg) {
	case 0x0:	/* test eb,ib */
		set_skipflags(false);gen_dop_byte_imm(DOP_TEST,src,src_i,decode_fetchb());
		goto skipsave;
	case 0x2:	/* NOT Eb */
		gen_sop_byte(SOP_NOT,src,src_i);
		break;
	case 0x3:	/* NEG Eb */
		set_skipflags(false);gen_sop_byte(SOP_NEG,src,src_i);
		break;
	case 0x4:	/* mul Eb */
		gen_needflags();gen_mul_byte(false,DREG(EAX),src,src_i);
		goto skipsave;
	case 0x5:	/* imul Eb */
		gen_needflags();gen_mul_byte(true,DREG(EAX),src,src_i);
		goto skipsave;
	case 0x6:	/* div Eb */
	case 0x7:	/* idiv Eb */
		/* EAX could be used, so precache it */
		if (decode.modrm.mod==3)
			gen_dop_byte(DOP_MOV,src,0,&DynRegs[decode.modrm.rm&3],decode.modrm.rm&4);
		gen_releasereg(DREG(EAX));
		gen_call_function((decode.modrm.reg==6) ? (void *)&dyn_helper_divb : (void *)&dyn_helper_idivb,
			"%Rd%Dd",DREG(TMPB),src);
		dyn_check_bool_exception(DREG(TMPB));
		goto skipsave;
	}
	/* Save the result if memory op */
	if (decode.modrm.mod<3) dyn_write_byte_release(DREG(EA),src,false);
skipsave:
	gen_releasereg(DREG(TMPB));gen_releasereg(DREG(EA));
}

static void dyn_grp3_ev(void) {
	dyn_get_modrm();DynReg * src;
	if (decode.modrm.mod<3) {
		dyn_fill_ea();src=DREG(TMPW);
		if ((decode.modrm.reg==0) || (decode.modrm.reg==3)) set_skipflags(true);
		dyn_read_word(DREG(EA),DREG(TMPW),decode.big_op);
	} else src=&DynRegs[decode.modrm.rm];
	switch (decode.modrm.reg) {
	case 0x0:	/* test ev,iv */
		set_skipflags(false);gen_dop_word_imm(DOP_TEST,decode.big_op,src,decode.big_op ? decode_fetchd() : decode_fetchw());
		goto skipsave;
	case 0x2:	/* NOT Ev */
		gen_sop_word(SOP_NOT,decode.big_op,src);
		break;
	case 0x3:	/* NEG Eb */
		set_skipflags(false);gen_sop_word(SOP_NEG,decode.big_op,src);
		break;
	case 0x4:	/* mul Eb */
		gen_needflags();gen_mul_word(false,DREG(EAX),DREG(EDX),decode.big_op,src);
		goto skipsave;
	case 0x5:	/* imul Eb */
		gen_needflags();gen_mul_word(true,DREG(EAX),DREG(EDX),decode.big_op,src);
		goto skipsave;
	case 0x6:	/* div Eb */
	case 0x7:	/* idiv Eb */
		/* EAX could be used, so precache it */
		if (decode.modrm.mod==3)
			gen_dop_word(DOP_MOV,decode.big_op,src,&DynRegs[decode.modrm.rm]);
		gen_releasereg(DREG(EAX));gen_releasereg(DREG(EDX));
		void * func=(decode.modrm.reg==6) ?
			(decode.big_op ? (void *)&dyn_helper_divd : (void *)&dyn_helper_divw) :
			(decode.big_op ? (void *)&dyn_helper_idivd : (void *)&dyn_helper_idivw);
		gen_call_function(func,"%Rd%Dd",DREG(TMPB),src);
		dyn_check_bool_exception(DREG(TMPB));
		gen_releasereg(DREG(TMPB));
		goto skipsave;
	}
	/* Save the result if memory op */
	if (decode.modrm.mod<3) dyn_write_word_release(DREG(EA),src,decode.big_op);
skipsave:
	gen_releasereg(DREG(TMPW));gen_releasereg(DREG(EA));
}

static void dyn_mov_ev_seg(void) {
	dyn_get_modrm();
	gen_load_host(&Segs.val[decode.modrm.reg],DREG(TMPW),2);
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		dyn_write_word_release(DREG(EA),DREG(TMPW),false);
	} else {
		gen_dop_word(DOP_MOV,decode.big_op,&DynRegs[decode.modrm.rm],DREG(TMPW));
	}
	gen_releasereg(DREG(TMPW));
}

static void dyn_load_seg(SegNames seg,DynReg * src) {
	if (cpu.pmode) {
		gen_call_function((void *)&CPU_SetSegGeneral,"%Rd%Id%Drw",DREG(TMPB),seg,src);
		dyn_check_bool_exception(DREG(TMPB));
		gen_releasereg(DREG(TMPB));
	} else gen_call_function((void *)CPU_SetSegGeneral,"%Id%Drw",seg,src);
	gen_releasereg(&DynRegs[G_ES+seg]);
	if (seg==ss) gen_releasereg(DREG(SMASK));
}

static void dyn_load_seg_off_ea(SegNames seg) {
	dyn_get_modrm();
	if (GCC_UNLIKELY(decode.modrm.mod<3)) {
		dyn_fill_ea();
		gen_lea(DREG(TMPW),DREG(EA),0,0,decode.big_op ? 4:2);
		dyn_read_word(DREG(TMPW),DREG(TMPW),false);
		dyn_load_seg(seg,DREG(TMPW));gen_releasereg(DREG(TMPW));
		dyn_read_word_release(DREG(EA),&DynRegs[decode.modrm.reg],decode.big_op);
	} else {
		IllegalOption();
	}
}

static void dyn_mov_seg_ev(void) {
	dyn_get_modrm();
	SegNames seg=(SegNames)decode.modrm.reg;
	if (GCC_UNLIKELY(seg==cs)) IllegalOption();
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		dyn_read_word(DREG(EA),DREG(EA),false);
		dyn_load_seg(seg,DREG(EA));
		gen_releasereg(DREG(EA));
	} else {
		dyn_load_seg(seg,&DynRegs[decode.modrm.rm]);
	}
}

static void dyn_push_seg(SegNames seg) {
	gen_load_host(&Segs.val[seg],DREG(TMPW),2);
	dyn_push(DREG(TMPW));
	gen_releasereg(DREG(TMPW));
}

static void dyn_pop_seg(SegNames seg) {
	if (!cpu.pmode) {
		dyn_pop(DREG(TMPW));
		dyn_load_seg(seg,DREG(TMPW));
		gen_releasereg(DREG(TMPW));
	} else {
		gen_releasereg(DREG(ESP));
		gen_call_function((void *)&CPU_PopSeg,"%Rd%Id%Id",DREG(TMPB),seg,decode.big_op);
		dyn_check_bool_exception(DREG(TMPB));
		gen_releasereg(DREG(TMPB));
		gen_releasereg(&DynRegs[G_ES+seg]);
		gen_releasereg(DREG(ESP));
		if (seg==ss) gen_releasereg(DREG(SMASK));
	}
}

static void dyn_pop_ev(void) {
	dyn_pop(DREG(TMPW));
	dyn_get_modrm();
	if (decode.modrm.mod<3) {
		dyn_fill_ea();
		dyn_write_word_release(DREG(EA),DREG(TMPW),decode.big_op);
	} else {
		gen_dop_word(DOP_MOV,decode.big_op,&DynRegs[decode.modrm.rm],DREG(TMPW));
	}
	gen_releasereg(DREG(TMPW));
}

static void dyn_enter(void) {
	gen_releasereg(DREG(ESP));
	gen_releasereg(DREG(EBP));
	Bitu bytes=decode_fetchw();
	Bitu level=decode_fetchb();
	gen_call_function((void *)&CPU_ENTER,"%Id%Id%Id",decode.big_op,bytes,level);
}

static void dyn_leave(void) {
	gen_protectflags();
	gen_dop_word(DOP_MOV,true,DREG(TMPW),DREG(SMASK));
	gen_sop_word(SOP_NOT,true,DREG(TMPW));
	gen_dop_word(DOP_AND,true,DREG(ESP),DREG(TMPW));
	gen_dop_word(DOP_MOV,true,DREG(TMPW),DREG(EBP));
	gen_dop_word(DOP_AND,true,DREG(TMPW),DREG(SMASK));
	gen_dop_word(DOP_OR,true,DREG(ESP),DREG(TMPW));
	dyn_pop(DREG(EBP));
	gen_releasereg(DREG(TMPW));
}

static void dyn_segprefix(SegNames seg) {
	if (GCC_UNLIKELY(decode.segprefix)) IllegalOption();
	decode.segprefix=&DynRegs[G_ES+seg];
}

static void dyn_closeblock(void) {
	//Shouldn't create empty block normally but let's do it like this
	gen_protectflags();
	cache_closeblock();
}

static void dyn_normal_exit(BlockReturn code) {
	gen_protectflags();
	dyn_reduce_cycles();
	dyn_set_eip_last();
	dyn_save_critical_regs();
	gen_return(code);
	dyn_closeblock();
}

static void dyn_exit_link(Bits eip_change) {
	gen_protectflags();
	gen_dop_word_imm(DOP_ADD,decode.big_op,DREG(EIP),(decode.code-decode.code_start)+eip_change);
	dyn_reduce_cycles();
	dyn_save_critical_regs();
	gen_jmp_ptr(&decode.block->link[0].to,offsetof(CacheBlock,cache.start));
	dyn_closeblock();
}

static void dyn_branched_exit(BranchTypes btype,Bit32s eip_add) {
	Bitu eip_base=decode.code-decode.code_start;
 	gen_needflags();
 	gen_protectflags();
	dyn_save_noncritical_regs();
	gen_releasereg(DREG(FLAGS));
	gen_releasereg(DREG(EIP));

	gen_preloadreg(DREG(CYCLES));
	gen_preloadreg(DREG(EIP));
	DynReg save_cycles,save_eip;
	dyn_saveregister(DREG(CYCLES),&save_cycles);
	dyn_saveregister(DREG(EIP),&save_eip);
	Bit8u * data=gen_create_branch(btype);

 	/* Branch not taken */
	dyn_reduce_cycles();
 	gen_dop_word_imm(DOP_ADD,decode.big_op,DREG(EIP),eip_base);
	gen_releasereg(DREG(CYCLES));
 	gen_releasereg(DREG(EIP));
 	gen_jmp_ptr(&decode.block->link[0].to,offsetof(CacheBlock,cache.start));
 	gen_fill_branch(data);

 	/* Branch taken */
	dyn_restoreregister(&save_cycles,DREG(CYCLES));
	dyn_restoreregister(&save_eip,DREG(EIP));
	dyn_reduce_cycles();
 	gen_dop_word_imm(DOP_ADD,decode.big_op,DREG(EIP),eip_base+eip_add);
	gen_releasereg(DREG(CYCLES));
 	gen_releasereg(DREG(EIP));
 	gen_jmp_ptr(&decode.block->link[1].to,offsetof(CacheBlock,cache.start));
 	dyn_closeblock();
}

enum LoopTypes {
	LOOP_NONE,LOOP_NE,LOOP_E,LOOP_JCXZ
};

static void dyn_loop(LoopTypes type) {
	dyn_reduce_cycles();
	Bits eip_add=(Bit8s)decode_fetchb();
	Bitu eip_base=decode.code-decode.code_start;
	Bit8u * branch1=0;Bit8u * branch2=0;
	dyn_save_critical_regs();
	switch (type) {
	case LOOP_E:
		gen_needflags();
		branch1=gen_create_branch(BR_NZ);
		break;
	case LOOP_NE:
		gen_needflags();
		branch1=gen_create_branch(BR_Z);
		break;
	}
	gen_protectflags();
	switch (type) {
	case LOOP_E:
	case LOOP_NE:
	case LOOP_NONE:
		gen_sop_word(SOP_DEC,decode.big_addr,DREG(ECX));
		gen_releasereg(DREG(ECX));
		branch2=gen_create_branch(BR_Z);
		break;
	case LOOP_JCXZ:
		gen_dop_word(DOP_OR,decode.big_addr,DREG(ECX),DREG(ECX));
		gen_releasereg(DREG(ECX));
		branch2=gen_create_branch(BR_NZ);
		break;
	}
	gen_lea(DREG(EIP),DREG(EIP),0,0,eip_base+eip_add);
	gen_releasereg(DREG(EIP));
	gen_jmp_ptr(&decode.block->link[0].to,offsetof(CacheBlock,cache.start));
	if (branch1) {
		gen_fill_branch(branch1);
		gen_sop_word(SOP_DEC,decode.big_addr,DREG(ECX));
		gen_releasereg(DREG(ECX));
	}
	/* Branch taken */
	gen_fill_branch(branch2);
	gen_lea(DREG(EIP),DREG(EIP),0,0,eip_base);
	gen_releasereg(DREG(EIP));
	gen_jmp_ptr(&decode.block->link[1].to,offsetof(CacheBlock,cache.start));
	dyn_closeblock();
}

static void dyn_ret_near(Bitu bytes) {
	gen_protectflags();
	dyn_reduce_cycles();
	dyn_pop(DREG(EIP));
	if (bytes) gen_dop_word_imm(DOP_ADD,true,DREG(ESP),bytes);
	dyn_save_critical_regs();
	gen_return(BR_Normal);
	dyn_closeblock();
}

static void dyn_call_near_imm(void) {
	Bits imm;
	if (decode.big_op) imm=(Bit32s)decode_fetchd();
	else imm=(Bit16s)decode_fetchw();
	dyn_set_eip_end();
	dyn_push(DREG(EIP));
	gen_dop_word_imm(DOP_ADD,decode.big_op,DREG(EIP),imm);
	dyn_reduce_cycles();
	dyn_save_critical_regs();
	gen_jmp_ptr(&decode.block->link[0].to,offsetof(CacheBlock,cache.start));
	dyn_closeblock();
}

static void dyn_ret_far(Bitu bytes) {
	gen_protectflags();
	dyn_reduce_cycles();
	dyn_set_eip_last_end(DREG(TMPW));
	dyn_flags_gen_to_host();
	dyn_save_critical_regs();
	gen_call_function((void*)&CPU_RET,"%Id%Id%Drd",decode.big_op,bytes,DREG(TMPW));
	dyn_flags_host_to_gen();
	gen_return(BR_Normal);
	dyn_closeblock();
}

static void dyn_call_far_imm(void) {
	Bitu sel,off;
	off=decode.big_op ? decode_fetchd() : decode_fetchw();
	sel=decode_fetchw();
	dyn_reduce_cycles();
	dyn_set_eip_last_end(DREG(TMPW));
	dyn_flags_gen_to_host();
	dyn_save_critical_regs();
	gen_call_function((void*)&CPU_CALL,"%Id%Id%Id%Drd",decode.big_op,sel,off,DREG(TMPW));
	dyn_flags_host_to_gen();
	gen_return(BR_Normal);
	dyn_closeblock();
}

static void dyn_jmp_far_imm(void) {
	Bitu sel,off;
	gen_protectflags();
	off=decode.big_op ? decode_fetchd() : decode_fetchw();
	sel=decode_fetchw();
	dyn_reduce_cycles();
	dyn_set_eip_last_end(DREG(TMPW));
	dyn_flags_gen_to_host();
	dyn_save_critical_regs();
	gen_call_function((void*)&CPU_JMP,"%Id%Id%Id%Drd",decode.big_op,sel,off,DREG(TMPW));
	dyn_flags_host_to_gen();
	gen_return(BR_Normal);
	dyn_closeblock();
}

static void dyn_iret(void) {
	gen_protectflags();
	dyn_flags_gen_to_host();
	dyn_reduce_cycles();
	dyn_set_eip_last_end(DREG(TMPW));
	dyn_save_critical_regs();
	gen_call_function((void*)&CPU_IRET,"%Id%Drd",decode.big_op,DREG(TMPW));
	dyn_flags_host_to_gen();
	gen_return(BR_Normal);
	dyn_closeblock();
}

static void dyn_interrupt(Bitu num) {
	gen_protectflags();
	dyn_flags_gen_to_host();
	dyn_reduce_cycles();
	dyn_set_eip_last_end(DREG(TMPW));
	dyn_save_critical_regs();
	gen_call_function((void*)&CPU_Interrupt,"%Id%Id%Drd",num,CPU_INT_SOFTWARE,DREG(TMPW));
	dyn_flags_host_to_gen();
	gen_return(BR_Normal);
	dyn_closeblock();
}

static CacheBlock * CreateCacheBlock(CodePageHandler * codepage,PhysPt start,Bitu max_opcodes) {
	Bits i;
/* Init a load of variables */
	decode.code_start=start;
	decode.code=start;
	Bitu cycles=0;
	decode.page.code=codepage;
	decode.page.index=start&4095;
	decode.page.wmap=codepage->write_map;
	decode.page.first=start >> 12;
	decode.active_block=decode.block=cache_openblock();
	decode.block->page.start=decode.page.index;
	codepage->AddCacheBlock(decode.block);

	gen_save_host_direct(&cache.block.running,(Bit32u)decode.block);
	for (i=0;i<G_MAX;i++) {
		DynRegs[i].flags&=~(DYNFLG_ACTIVE|DYNFLG_CHANGED);
		DynRegs[i].genreg=0;
	}
	gen_reinit();
	/* Start with the cycles check */
	gen_protectflags();
	gen_dop_word_imm(DOP_CMP,true,DREG(CYCLES),0);
	Bit8u * cyclebranch=gen_create_branch(BR_NLE);
	gen_return(BR_Cycles);
	gen_fill_branch(cyclebranch);
	gen_releasereg(DREG(CYCLES));
	decode.cycles=0;
	while (max_opcodes--) {
/* Init prefixes */
		decode.big_addr=cpu.code.big;
		decode.big_op=cpu.code.big;
		decode.segprefix=0;
		decode.rep=REP_NONE;
		decode.cycles++;
		decode.op_start=decode.code;
restart_prefix:
		Bitu opcode=decode_fetchb();
		switch (opcode) {

		case 0x00:dyn_dop_ebgb(DOP_ADD);break;
		case 0x01:dyn_dop_evgv(DOP_ADD);break;
		case 0x02:dyn_dop_gbeb(DOP_ADD);break;
		case 0x03:dyn_dop_gvev(DOP_ADD);break;
		case 0x04:gen_discardflags();gen_dop_byte_imm(DOP_ADD,DREG(EAX),0,decode_fetchb());break;
		case 0x05:gen_discardflags();gen_dop_word_imm(DOP_ADD,decode.big_op,DREG(EAX),decode.big_op ? decode_fetchd() :  decode_fetchw());break;
		case 0x06:dyn_push_seg(es);break;
		case 0x07:dyn_pop_seg(es);break;

		case 0x08:dyn_dop_ebgb(DOP_OR);break;
		case 0x09:dyn_dop_evgv(DOP_OR);break;
		case 0x0a:dyn_dop_gbeb(DOP_OR);break;
		case 0x0b:dyn_dop_gvev(DOP_OR);break;
		case 0x0c:gen_discardflags();gen_dop_byte_imm(DOP_OR,DREG(EAX),0,decode_fetchb());break;
		case 0x0d:gen_discardflags();gen_dop_word_imm(DOP_OR,decode.big_op,DREG(EAX),decode.big_op ? decode_fetchd() :  decode_fetchw());break;
		case 0x0e:dyn_push_seg(cs);break;
		case 0x0f:
		{
			Bitu dual_code=decode_fetchb();
			switch (dual_code) {
			/* Short conditional jumps */
			case 0x80:case 0x81:case 0x82:case 0x83:case 0x84:case 0x85:case 0x86:case 0x87:	
			case 0x88:case 0x89:case 0x8a:case 0x8b:case 0x8c:case 0x8d:case 0x8e:case 0x8f:	
				dyn_branched_exit((BranchTypes)(dual_code&0xf),
					decode.big_op ? (Bit32s)decode_fetchd() : (Bit16s)decode_fetchw());	
				goto finish_block;
			/* PUSH/POP FS */
			case 0xa0:dyn_push_seg(fs);break;
			case 0xa1:dyn_pop_seg(fs);break;
			/* SHLD Imm/cl*/
			case 0xa4:dyn_dshift_ev_gv(true,true);break;
			case 0xa5:dyn_dshift_ev_gv(true,false);break;
			/* PUSH/POP GS */
			case 0xa8:dyn_push_seg(gs);break;
			case 0xa9:dyn_pop_seg(gs);break;
			/* SHRD Imm/cl*/
			case 0xac:dyn_dshift_ev_gv(false,true);break;
			case 0xad:dyn_dshift_ev_gv(false,false);break;		
			/* Imul Ev,Gv */
			case 0xaf:dyn_imul_gvev(0);break;
			/* LFS,LGS */
			case 0xb4:dyn_load_seg_off_ea(fs);break;
			case 0xb5:dyn_load_seg_off_ea(gs);break;
			/* MOVZX Gv,Eb/Ew */
			case 0xb6:dyn_mov_ev_gb(false);break;
			case 0xb7:dyn_mov_ev_gw(false);break;
			/* MOVSX Gv,Eb/Ew */
			case 0xbe:dyn_mov_ev_gb(true);break;
			case 0xbf:dyn_mov_ev_gw(true);break;

			default:
				DYN_LOG("Unhandled dual opcode 0F%02X",dual_code);
				goto illegalopcode;
			}
		}break;

		case 0x10:dyn_dop_ebgb(DOP_ADC);break;
		case 0x11:dyn_dop_evgv(DOP_ADC);break;
		case 0x12:dyn_dop_gbeb(DOP_ADC);break;
		case 0x13:dyn_dop_gvev(DOP_ADC);break;
		case 0x14:gen_needcarry();gen_dop_byte_imm(DOP_ADC,DREG(EAX),0,decode_fetchb());break;
		case 0x15:gen_needcarry();gen_dop_word_imm(DOP_ADC,decode.big_op,DREG(EAX),decode.big_op ? decode_fetchd() :  decode_fetchw());break;
		case 0x16:dyn_push_seg(ss);break;
		case 0x17:dyn_pop_seg(ss);break;

		case 0x18:dyn_dop_ebgb(DOP_SBB);break;
		case 0x19:dyn_dop_evgv(DOP_SBB);break;
		case 0x1a:dyn_dop_gbeb(DOP_SBB);break;
		case 0x1b:dyn_dop_gvev(DOP_SBB);break;
		case 0x1c:gen_needcarry();gen_dop_byte_imm(DOP_SBB,DREG(EAX),0,decode_fetchb());break;
		case 0x1d:gen_needcarry();gen_dop_word_imm(DOP_SBB,decode.big_op,DREG(EAX),decode.big_op ? decode_fetchd() :  decode_fetchw());break;
		case 0x1e:dyn_push_seg(ds);break;
		case 0x1f:dyn_pop_seg(ds);break;
		case 0x20:dyn_dop_ebgb(DOP_AND);break;
		case 0x21:dyn_dop_evgv(DOP_AND);break;
		case 0x22:dyn_dop_gbeb(DOP_AND);break;
		case 0x23:dyn_dop_gvev(DOP_AND);break;
		case 0x24:gen_discardflags();gen_dop_byte_imm(DOP_AND,DREG(EAX),0,decode_fetchb());break;
		case 0x25:gen_discardflags();gen_dop_word_imm(DOP_AND,decode.big_op,DREG(EAX),decode.big_op ? decode_fetchd() :  decode_fetchw());break;
		case 0x26:dyn_segprefix(es);goto restart_prefix;

		case 0x28:dyn_dop_ebgb(DOP_SUB);break;
		case 0x29:dyn_dop_evgv(DOP_SUB);break;
		case 0x2a:dyn_dop_gbeb(DOP_SUB);break;
		case 0x2b:dyn_dop_gvev(DOP_SUB);break;
		case 0x2c:gen_discardflags();gen_dop_byte_imm(DOP_SUB,DREG(EAX),0,decode_fetchb());break;
		case 0x2d:gen_discardflags();gen_dop_word_imm(DOP_SUB,decode.big_op,DREG(EAX),decode.big_op ? decode_fetchd() :  decode_fetchw());break;
		case 0x2e:dyn_segprefix(cs);goto restart_prefix;

		case 0x30:dyn_dop_ebgb(DOP_XOR);break;
		case 0x31:dyn_dop_evgv(DOP_XOR);break;
		case 0x32:dyn_dop_gbeb(DOP_XOR);break;
		case 0x33:dyn_dop_gvev(DOP_XOR);break;
		case 0x34:gen_discardflags();gen_dop_byte_imm(DOP_XOR,DREG(EAX),0,decode_fetchb());break;
		case 0x35:gen_discardflags();gen_dop_word_imm(DOP_XOR,decode.big_op,DREG(EAX),decode.big_op ? decode_fetchd() :  decode_fetchw());break;
		case 0x36:dyn_segprefix(ss);goto restart_prefix;

		case 0x38:dyn_dop_ebgb(DOP_CMP);break;
		case 0x39:dyn_dop_evgv(DOP_CMP);break;
		case 0x3a:dyn_dop_gbeb(DOP_CMP);break;
		case 0x3b:dyn_dop_gvev(DOP_CMP);break;
		case 0x3c:gen_discardflags();gen_dop_byte_imm(DOP_CMP,DREG(EAX),0,decode_fetchb());break;
		case 0x3d:gen_discardflags();gen_dop_word_imm(DOP_CMP,decode.big_op,DREG(EAX),decode.big_op ? decode_fetchd() :  decode_fetchw());break;
		case 0x3e:dyn_segprefix(ds);goto restart_prefix;

		/* INC/DEC general register */
		case 0x40:case 0x41:case 0x42:case 0x43:case 0x44:case 0x45:case 0x46:case 0x47:	
			gen_needcarry();gen_sop_word(SOP_INC,decode.big_op,&DynRegs[opcode&7]);
			break;
		case 0x48:case 0x49:case 0x4a:case 0x4b:case 0x4c:case 0x4d:case 0x4e:case 0x4f:	
			gen_needcarry();gen_sop_word(SOP_DEC,decode.big_op,&DynRegs[opcode&7]);
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
			gen_dop_word(DOP_MOV,true,DREG(TMPW),DREG(ESP));
			for (i=G_EAX;i<=G_EDI;i++) {
				dyn_push((i!=G_ESP) ? &DynRegs[i] : DREG(TMPW));
			}
			gen_releasereg(DREG(TMPW));
			break;
		case 0x61:		/* POPA */
			for (i=G_EDI;i>=G_EAX;i--) {
				dyn_pop((i!=G_ESP) ? &DynRegs[i] : DREG(TMPW));
			}
			gen_releasereg(DREG(TMPW));
			break;
		//segprefix FS,GS
		case 0x64:dyn_segprefix(fs);goto restart_prefix;
		case 0x65:dyn_segprefix(gs);goto restart_prefix;
		//Push immediates
		//Operand size		
		case 0x66:decode.big_op=!cpu.code.big;;goto restart_prefix;
		//Address size		
		case 0x67:decode.big_addr=!cpu.code.big;goto restart_prefix;
		case 0x68:		/* PUSH Iv */
			gen_dop_word_imm(DOP_MOV,decode.big_op,DREG(TMPW),decode.big_op ? decode_fetchd() : decode_fetchw());
			dyn_push(DREG(TMPW));
			gen_releasereg(DREG(TMPW));
			break;
		/* Imul Ivx */
		case 0x69:dyn_imul_gvev(decode.big_op ? 4 : 2);break;
		case 0x6a:		/* PUSH Ibx */
			gen_dop_word_imm(DOP_MOV,true,DREG(TMPW),(Bit8s)decode_fetchb());
			dyn_push(DREG(TMPW));
			gen_releasereg(DREG(TMPW));
			break;
		/* Imul Ibx */
		case 0x6b:dyn_imul_gvev(1);break;
		/* Short conditional jumps */
		case 0x70:case 0x71:case 0x72:case 0x73:case 0x74:case 0x75:case 0x76:case 0x77:	
		case 0x78:case 0x79:case 0x7a:case 0x7b:case 0x7c:case 0x7d:case 0x7e:case 0x7f:	
			dyn_branched_exit((BranchTypes)(opcode&0xf),(Bit8s)decode_fetchb());	
			goto finish_block;
		/* Group 1 */
		case 0x80:dyn_grp1_eb_ib();break;
		case 0x81:dyn_grp1_ev_ivx(false);break;
		case 0x82:dyn_grp1_eb_ib();break;
		case 0x83:dyn_grp1_ev_ivx(true);break;
		/* TEST Gb,Eb Gv,Ev */
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
			dyn_get_modrm();
			if (decode.big_op) {
				dyn_fill_ea(false,&DynRegs[decode.modrm.reg]);
			} else {
				dyn_fill_ea(false);
				gen_dop_word(DOP_MOV,decode.big_op,&DynRegs[decode.modrm.reg],DREG(EA));
				gen_releasereg(DREG(EA));
			}
			break;
		/* Mov seg,ev */
		case 0x8e:dyn_mov_seg_ev();break;
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
			gen_cbw(decode.big_op,DREG(EAX));
			break;
		/* CWD/CDQ */
		case 0x99:
			gen_cwd(decode.big_op,DREG(EAX),DREG(EDX));
			break;
		/* CALL FAR Ip */
		case 0x9a:dyn_call_far_imm();goto finish_block;
		case 0x9c:		//PUSHF
			gen_protectflags();
			gen_releasereg(DREG(ESP));
			dyn_flags_gen_to_host();
			gen_call_function((void *)&CPU_PUSHF,"%Rd%Id",DREG(TMPB),decode.big_op);
			if (cpu.pmode) dyn_check_bool_exception(DREG(TMPB));
			gen_releasereg(DREG(TMPB));
			break;
		case 0x9d:		//POPF
			gen_releasereg(DREG(ESP));
			gen_releasereg(DREG(FLAGS));
			gen_call_function((void *)&CPU_POPF,"%Rd%Id",DREG(TMPB),decode.big_op);
			if (cpu.pmode) dyn_check_bool_exception(DREG(TMPB));
			dyn_flags_host_to_gen();
			gen_releasereg(DREG(TMPB));
			break;
		/* MOV AL,direct addresses */
		case 0xa0:
			gen_lea(DREG(EA),decode.segprefix ? decode.segprefix : DREG(DS),0,0,
				decode.big_addr ? decode_fetchd() : decode_fetchw());
			dyn_read_byte_release(DREG(EA),DREG(EAX),false);
			break;
		/* MOV AX,direct addresses */
		case 0xa1:
			gen_lea(DREG(EA),decode.segprefix ? decode.segprefix : DREG(DS),0,0,
				decode.big_addr ? decode_fetchd() : decode_fetchw());
			dyn_read_word_release(DREG(EA),DREG(EAX),decode.big_op);
			break;
		/* MOV direct address,AL */
		case 0xa2:
			gen_lea(DREG(EA),decode.segprefix ? decode.segprefix : DREG(DS),0,0,
				decode.big_addr ? decode_fetchd() : decode_fetchw());
			dyn_write_byte_release(DREG(EA),DREG(EAX),false);
			break;
		/* MOV direct addresses,AX */
		case 0xa3:
			gen_lea(DREG(EA),decode.segprefix ? decode.segprefix : DREG(DS),0,0,
				decode.big_addr ? decode_fetchd() : decode_fetchw());
			dyn_write_word_release(DREG(EA),DREG(EAX),decode.big_op);
			break;
		/* MOVSB/W/D*/
		case 0xa4:dyn_string(STR_MOVSB);break;
		case 0xa5:dyn_string(decode.big_op ? STR_MOVSD : STR_MOVSW);break;
		/* TEST AL,AX Imm */
		case 0xa8:gen_discardflags();gen_dop_byte_imm(DOP_TEST,DREG(EAX),0,decode_fetchb());break;
		case 0xa9:gen_discardflags();gen_dop_word_imm(DOP_TEST,decode.big_op,DREG(EAX),decode.big_op ? decode_fetchd() :  decode_fetchw());break;
		/* STOSB/W/D*/
		case 0xaa:dyn_string(STR_STOSB);break;
		case 0xab:dyn_string(decode.big_op ? STR_STOSD : STR_STOSW);break;
		/* LODSB/W/D*/
		case 0xac:dyn_string(STR_LODSB);break;
		case 0xad:dyn_string(decode.big_op ? STR_LODSD : STR_LODSW);break;
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
		case 0xc2:dyn_ret_near(decode_fetchw());goto finish_block;
		case 0xc3:dyn_ret_near(0);goto finish_block;
		//LES/LDS
		case 0xc4:dyn_load_seg_off_ea(es);break;
		case 0xc5:dyn_load_seg_off_ea(ds);break;
		// MOV Eb/Ev,Ib/Iv
		case 0xc6:dyn_mov_ebib();break;
		case 0xc7:dyn_mov_eviv();break;
		//ENTER and LEAVE
		case 0xc8:dyn_enter();break;
		case 0xc9:dyn_leave();break;
		//RET far Iw / Ret
		case 0xca:dyn_ret_far(decode_fetchw());goto finish_block;
		case 0xcb:dyn_ret_far(0);goto finish_block;
		/* Interrupt */
//		case 0xcd:dyn_interrupt(decode_fetchb());goto finish_block;
		/* IRET */
		case 0xcf:dyn_iret();goto finish_block;

		//GRP2 Eb/Ev,1
		case 0xd0:dyn_grp2_eb(grp2_1);break;
		case 0xd1:dyn_grp2_ev(grp2_1);break;
		//GRP2 Eb/Ev,CL
		case 0xd2:dyn_grp2_eb(grp2_cl);break;
		case 0xd3:dyn_grp2_ev(grp2_cl);break;
		//FPU
#ifdef CPU_FPU
		case 0xd8:
			DYN_FPU_ESC(0);
			break;
		case 0xd9:
			DYN_FPU_ESC(1);
			break;
		case 0xda:
			DYN_FPU_ESC(2);
			break;
		case 0xdb:
			DYN_FPU_ESC(3);
			break;
		case 0xdc:
			DYN_FPU_ESC(4);
			break;
		case 0xdd:
			DYN_FPU_ESC(5);
			break;
		case 0xde:
			DYN_FPU_ESC(6);
			break;
		case 0xdf:
			dyn_get_modrm();
			if (decode.modrm.val >= 0xc0) {
				if (decode.modrm.val == 0xe0) gen_releasereg(DREG(EAX)); /* FSTSW */
				gen_call_function((void*)&FPU_ESC7_Normal,"%Id",decode.modrm.val);
			} else {
				dyn_fill_ea();
				gen_call_function((void*)&FPU_ESC7_EA,"%Id%Dd",decode.modrm.val,DREG(EA));
				gen_releasereg(DREG(EA));
			}
			break;
#endif
		//Loop's 
		case 0xe2:dyn_loop(LOOP_NONE);goto finish_block;
		case 0xe3:dyn_loop(LOOP_JCXZ);goto finish_block;
		//IN AL/AX,imm
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
			goto finish_block;
		case 0xe9:		/* Jmp Ivx */
			dyn_exit_link(decode.big_op ? (Bit32s)decode_fetchd() : (Bit16s)decode_fetchw());
			goto finish_block;
		case 0xea:		/* JMP FAR Ip */
			dyn_jmp_far_imm();
			goto finish_block;
			/* Jmp Ibx */
		case 0xeb:dyn_exit_link((Bit8s)decode_fetchb());goto finish_block;
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
		case 0xf2:		//REPNE/NZ
			decode.rep=REP_NZ;
			goto restart_prefix;
		case 0xf3:		//REPE/Z
			decode.rep=REP_Z;
			goto restart_prefix;
		/* Change carry flag */
		case 0xf5:		//CMC
		case 0xf8:		//CLC
		case 0xf9:		//STC
			gen_needflags();
			cache_addb(opcode);break;
		/* GRP 3 Eb/EV */
		case 0xf6:dyn_grp3_eb();break;
		case 0xf7:dyn_grp3_ev();break;
		/* Change interrupt flag */
		case 0xfa:		//CLI
			gen_call_function((void *)&CPU_CLI,"%Rd",DREG(TMPB));
			if (cpu.pmode) dyn_check_bool_exception(DREG(TMPB));
			gen_releasereg(DREG(TMPB));
			break;
		case 0xfb:		//STI
			gen_call_function((void *)&CPU_STI,"%Rd",DREG(TMPB));
			if (cpu.pmode) dyn_check_bool_exception(DREG(TMPB));
			gen_releasereg(DREG(TMPB));
			if (max_opcodes<=0) max_opcodes=1;		//Allow 1 extra opcode
			break;
		case 0xfc:		//CLD
			gen_protectflags();
			gen_dop_word_imm(DOP_AND,true,DREG(FLAGS),~FLAG_DF);
			gen_save_host_direct(&cpu.direction,1);
			break;
		case 0xfd:		//STD
			gen_protectflags();
			gen_dop_word_imm(DOP_OR,true,DREG(FLAGS),FLAG_DF);
			gen_save_host_direct(&cpu.direction,-1);
			break;
		/* GRP 4 Eb and callback's */
		case 0xfe:
            dyn_get_modrm();
			switch (decode.modrm.reg) {
			case 0x0://INC Eb
			case 0x1://DEC Eb
				if (decode.modrm.mod<3) {
					dyn_fill_ea();dyn_read_byte(DREG(EA),DREG(TMPB),false);
					gen_needcarry();
					gen_sop_byte(decode.modrm.reg==0 ? SOP_INC : SOP_DEC,DREG(TMPB),0);
					dyn_write_byte_release(DREG(EA),DREG(TMPB),false);
					gen_releasereg(DREG(TMPB));
				} else {
					gen_needcarry();
					gen_sop_byte(decode.modrm.reg==0 ? SOP_INC : SOP_DEC,
						&DynRegs[decode.modrm.rm&3],decode.modrm.rm&4);
				}
				break;
			case 0x7:		//CALBACK Iw
				gen_save_host_direct(&core_dyn.callback,decode_fetchw());
				dyn_set_eip_end();
				dyn_reduce_cycles();
				dyn_save_critical_regs();
				gen_return(BR_CallBack);
				dyn_closeblock();
				goto finish_block;
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
				gen_needcarry();
				gen_sop_word(decode.modrm.reg==0 ? SOP_INC : SOP_DEC,decode.big_op,src);
				if (decode.modrm.mod<3){
					dyn_write_word_release(DREG(EA),DREG(TMPW),decode.big_op);
					gen_releasereg(DREG(TMPW));
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
				gen_protectflags();
				dyn_flags_gen_to_host();
				gen_lea(DREG(EA),DREG(EA),0,0,decode.big_op ? 4: 2);
				dyn_set_eip_last_end(DREG(TMPB));
				dyn_read_word(DREG(EA),DREG(EA),false);
				dyn_save_critical_regs();
				gen_call_function(
					decode.modrm.reg == 3 ? (void*)&CPU_CALL : (void*)&CPU_JMP,
					decode.big_op ? (char *)"%Id%Drw%Drd%Drd" : (char *)"%Id%Drw%Drw%Drd",
					decode.big_op,DREG(EA),DREG(TMPW),DREG(TMPB));
				dyn_flags_host_to_gen();
				goto core_close_block;
			case 0x6:		/* PUSH Ev */
				gen_releasereg(DREG(EA));
				dyn_push(src);
				break;
			default:
				IllegalOption();
			}}
			break;
		default:
//			DYN_LOG("Dynamic unhandled opcode %X",opcode);
			goto illegalopcode;
		}
	}
	/* Normal exit because of max opcodes reached */
	dyn_set_eip_end();
core_close_block:
	dyn_reduce_cycles();
	dyn_save_critical_regs();
	gen_return(BR_Normal);
	dyn_closeblock();
	goto finish_block;
illegalopcode:
	dyn_set_eip_last();
	dyn_reduce_cycles();
	dyn_save_critical_regs();
	gen_return(BR_Opcode);
	dyn_closeblock();
	goto finish_block;
#if (C_DEBUG)
illegalopcodefull:
	dyn_set_eip_last();
	dyn_reduce_cycles();
	dyn_save_critical_regs();
	gen_return(BR_OpcodeFull);
	dyn_closeblock();
	goto finish_block;
#endif
finish_block:
	/* Setup the correct end-address */
	decode.active_block->page.end=--decode.page.index;
//	LOG_MSG("Created block size %d start %d end %d",decode.block->cache.size,decode.block->page.start,decode.block->page.end);
	return decode.block;
}
