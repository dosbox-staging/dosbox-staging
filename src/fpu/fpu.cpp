// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later


#include "dosbox.h"
#if C_FPU

#include "cpu.h"
#include "cross.h"
#include "fpu.h"
#include "mem.h"
#include <cmath>

FPU_rec fpu = {};

void FPU_FLDCW(PhysPt addr){
	uint16_t temp = mem_readw(addr);
	FPU_SetCW(temp);
}

uint16_t FPU_GetTag()
{
	uint16_t tag = 0;
	for (int i = 0; i < 8; i++) {
		tag |= ((fpu.tags[i] & 3) << (2 * i));
	}
	return tag;
}

void FPU_SetPRegsFrom(const uint8_t dyn_regs[8][10])
{
	for (uint8_t i = 0; i < 8; ++i) {
		auto& norm_p_reg        = fpu.p_regs[STV(i)];
		const auto dyn_reg_addr = dyn_regs[i];

		norm_p_reg.m1 = read_unaligned_uint32_at(dyn_reg_addr, 0);
		norm_p_reg.m2 = read_unaligned_uint32_at(dyn_reg_addr, 1);
		norm_p_reg.m3 = read_unaligned_uint16_at(dyn_reg_addr, 4);
	}
}

void FPU_GetPRegsTo(uint8_t dyn_regs[8][10])
{
	for (uint8_t i = 0; i < 8; ++i) {
		auto dyn_reg_addr      = dyn_regs[i];
		const auto& norm_p_reg = fpu.p_regs[STV(i)];

		write_unaligned_uint32_at(dyn_reg_addr, 0, norm_p_reg.m1);
		write_unaligned_uint32_at(dyn_reg_addr, 1, norm_p_reg.m2);
		write_unaligned_uint16_at(dyn_reg_addr, 4, norm_p_reg.m3);
	}
}

#if C_FPU_X86
#include "fpu_instructions_x86.h"
#else
#include "fpu_instructions.h"
#endif

/* WATCHIT : ALWAYS UPDATE REGISTERS BEFORE AND AFTER USING THEM 
			STATUS WORD =>	FPU_SET_TOP(TOP) BEFORE a read
			TOP=FPU_GET_TOP() after a write;
			*/

static void EATREE(Bitu _rm){
	const uint8_t group = (_rm >> 3) & 7;
	switch(group){
		case 0x00:	/* FADD */
			FPU_FADD_EA(TOP);
			break;
		case 0x01:	/* FMUL  */
			FPU_FMUL_EA(TOP);
			break;
		case 0x02:	/* FCOM */
			FPU_FCOM_EA(TOP);
			break;
		case 0x03:	/* FCOMP */
			FPU_FCOM_EA(TOP);
			FPU_FPOP();
			break;
		case 0x04:	/* FSUB */
			FPU_FSUB_EA(TOP);
			break;
		case 0x05:	/* FSUBR */
			FPU_FSUBR_EA(TOP);
			break;
		case 0x06:	/* FDIV */
			FPU_FDIV_EA(TOP);
			break;
		case 0x07:	/* FDIVR */
			FPU_FDIVR_EA(TOP);
			break;
		default:
			break;
	}
}

void FPU_ESC0_EA(Bitu rm,PhysPt addr) {
	/* REGULAR TREE WITH 32 BITS REALS */
	FPU_FLD_F32_EA(addr);
	EATREE(rm);
}

void FPU_ESC0_Normal(Bitu rm) {
	const uint8_t group = (rm >> 3) & 7;
	const uint8_t sub   = (rm & 7);
	switch (group) {
	case 0x00: /* FADD ST,STi */ FPU_FADD(TOP, STV(sub)); break;
	case 0x01: /* FMUL  ST,STi */ FPU_FMUL(TOP, STV(sub)); break;
	case 0x02:		/* FCOM  STi */
		FPU_FCOM(TOP,STV(sub));
		break;
	case 0x03:		/* FCOMP STi */
		FPU_FCOM(TOP,STV(sub));
		FPU_FPOP();
		break;
	case 0x04:		/* FSUB  ST,STi */
		FPU_FSUB(TOP,STV(sub));
		break;	
	case 0x05:		/* FSUBR ST,STi */
		FPU_FSUBR(TOP,STV(sub));
		break;
	case 0x06:		/* FDIV  ST,STi */
		FPU_FDIV(TOP,STV(sub));
		break;
	case 0x07:		/* FDIVR ST,STi */
		FPU_FDIVR(TOP,STV(sub));
		break;
	default:
		break;
	}
}

void FPU_ESC1_EA(Bitu rm,PhysPt addr) {
	const uint8_t group = (rm >> 3) & 7;
	const uint8_t sub   = (rm & 7);
	switch (group) {
	case 0x00: /* FLD float*/
		FPU_PREP_PUSH();
		FPU_FLD_F32(addr, TOP);
		break;
	case 0x01: /* UNKNOWN */ FPU_LOG_WARN(1, true, group, sub); break;
	case 0x02: /* FST float*/ FPU_FST_F32(addr); break;
	case 0x03: /* FSTP float*/
		FPU_FST_F32(addr);
		FPU_FPOP();
		break;
	case 0x04: /* FLDENV */ FPU_FLDENV(addr); break;
	case 0x05: /* FLDCW */ FPU_FLDCW(addr); break;
	case 0x06: /* FSTENV */ FPU_FSTENV(addr); break;
	case 0x07: /* FNSTCW*/ mem_writew(addr, fpu.cw); break;
	default: FPU_LOG_WARN(1, true, group, sub); break;
	}
}

void FPU_ESC1_Normal(Bitu rm) {
	const uint8_t group = (rm >> 3) & 7;
	const uint8_t sub   = (rm & 7);
	switch (group) {
	case 0x00: /* FLD STi */
	{
		Bitu reg_from = STV(sub);
		FPU_PREP_PUSH();
		FPU_FST(reg_from, TOP);
		break;
	}
	case 0x01: /* FXCH STi */ FPU_FXCH(TOP, STV(sub)); break;
	case 0x02: /* FNOP */
		FPU_FNOP();
		break;
	case 0x03: /* FSTP STi */
		FPU_FST(TOP,STV(sub));
		FPU_FPOP();
		break;   
	case 0x04:
		switch(sub){
		case 0x00:       /* FCHS */
			FPU_FCHS();
			break;
		case 0x01:       /* FABS */
			FPU_FABS();
			break;
		case 0x02:       /* UNKNOWN */
		case 0x03:       /* ILLEGAL */
			FPU_LOG_WARN(1,false,group,sub);
			break;
		case 0x04:       /* FTST */
			FPU_FTST();
			break;
		case 0x05:       /* FXAM */
			FPU_FXAM();
			break;
		case 0x06:       /* FTSTP (cyrix)*/
		case 0x07:       /* UNKNOWN */
			FPU_LOG_WARN(1,false,group,sub);
			break;
		}
		break;
	case 0x05:
		switch(sub){	
		case 0x00:       /* FLD1 */
			FPU_FLD1();
			break;
		case 0x01:       /* FLDL2T */
			FPU_FLDL2T();
			break;
		case 0x02:       /* FLDL2E */
			FPU_FLDL2E();
			break;
		case 0x03:       /* FLDPI */
			FPU_FLDPI();
			break;
		case 0x04:       /* FLDLG2 */
			FPU_FLDLG2();
			break;
		case 0x05:       /* FLDLN2 */
			FPU_FLDLN2();
			break;
		case 0x06:       /* FLDZ*/
			FPU_FLDZ();
			break;
		case 0x07:       /* ILLEGAL */
			FPU_LOG_WARN(1,false,group,sub);
			break;
		}
		break;
	case 0x06:
		switch(sub){
		case 0x00:	/* F2XM1 */
			FPU_F2XM1();
			break;
		case 0x01:	/* FYL2X */
			FPU_FYL2X();
			break;
		case 0x02:	/* FPTAN  */
			FPU_FPTAN();
			break;
		case 0x03:	/* FPATAN */
			FPU_FPATAN();
			break;
		case 0x04:	/* FXTRACT */
			FPU_FXTRACT();
			break;
		case 0x05:	/* FPREM1 */
			FPU_FPREM1();
			break;
		case 0x06:	/* FDECSTP */
			TOP = (TOP - 1) & 7;
			break;
		case 0x07:	/* FINCSTP */
			TOP = (TOP + 1) & 7;
			break;
		default:
			FPU_LOG_WARN(1,false,group,sub);
			break;
		}
		break;
	case 0x07:
		switch(sub){
		case 0x00:		/* FPREM */
			FPU_FPREM();
			break;
		case 0x01:		/* FYL2XP1 */
			FPU_FYL2XP1();
			break;
		case 0x02:		/* FSQRT */
			FPU_FSQRT();
			break;
		case 0x03:		/* FSINCOS */
			FPU_FSINCOS();
			break;
		case 0x04:		/* FRNDINT */
			FPU_FRNDINT();
			break;
		case 0x05:		/* FSCALE */
			FPU_FSCALE();
			break;
		case 0x06:		/* FSIN */
			FPU_FSIN();
			break;
		case 0x07:		/* FCOS */
			FPU_FCOS();
			break;
		default:
			FPU_LOG_WARN(1,false,group,sub);
			break;
		}
		break;
		default:
			FPU_LOG_WARN(1,false,group,sub);
			break;
	        }
}


void FPU_ESC2_EA(Bitu rm,PhysPt addr) {
	/* 32 bits integer operants */
	FPU_FLD_I32_EA(addr);
	EATREE(rm);
}

void FPU_ESC2_Normal(Bitu rm) {
	const uint8_t group = (rm >> 3) & 7;
	const uint8_t sub   = (rm & 7);
	switch (group) {
	case 0x05:
		switch(sub){
		case 0x01:		/* FUCOMPP */
			FPU_FUCOM(TOP,STV(1));
			FPU_FPOP();
			FPU_FPOP();
			break;
		default:
			FPU_LOG_WARN(2,false,group,sub);
			break;
		}
		break;
	default:
		FPU_LOG_WARN(2,false,group,sub);
		break;
	}
}


void FPU_ESC3_EA(Bitu rm,PhysPt addr) {
	const uint8_t group = (rm >> 3) & 7;
	const uint8_t sub   = (rm & 7);
	switch (group) {
	case 0x00: /* FILD */
		FPU_PREP_PUSH();
		FPU_FLD_I32(addr,TOP);
		break;
	case 0x01:	/* FISTTP */
		FPU_LOG_WARN(3,true,group,sub);
		break;
	case 0x02:	/* FIST */
		FPU_FST_I32(addr);
		break;
	case 0x03:	/* FISTP */
		FPU_FST_I32(addr);
		FPU_FPOP();
		break;
	case 0x05:	/* FLD 80 Bits Real */
		FPU_PREP_PUSH();
		FPU_FLD_F80(addr);
		break;
	case 0x07:	/* FSTP 80 Bits Real */
		FPU_FST_F80(addr);
		FPU_FPOP();
		break;
	default:
		FPU_LOG_WARN(3,true,group,sub);
		break;
	}
}

void FPU_ESC3_Normal(Bitu rm) {
	const uint8_t group = (rm >> 3) & 7;
	const uint8_t sub   = (rm & 7);
	switch (group) {
	case 0x04:
		switch (sub) {
		case 0x00:				//FNENI
		case 0x01:				//FNDIS
			LOG(LOG_FPU,LOG_ERROR)("8087 only fpu code used esc 3: group 4: subfunction: %u", sub);
			break;
		case 0x02:				//FNCLEX FCLEX
			FPU_FCLEX();
			break;
		case 0x03:				//FNINIT FINIT
			FPU_FINIT();
			break;
		case 0x04:				//FNSETPM
		case 0x05:				//FRSTPM
			// LOG(LOG_FPU,LOG_ERROR)("80267 protected mode (un)set. Nothing done");
			FPU_FNOP();
			break;
		default:
			E_Exit("ESC 3: ILLEGAL OPCODE group %u subfunction %u", group, sub);
		}
		break;
	default:
		FPU_LOG_WARN(3, false, group, sub);
		break;
	}
}

void FPU_ESC4_EA(Bitu rm,PhysPt addr) {
	/* REGULAR TREE WITH 64 BITS REALS */
	FPU_FLD_F64_EA(addr);
	EATREE(rm);
}

void FPU_ESC4_Normal(Bitu rm) {
	/* LOOKS LIKE number 6 without popping */
	const uint8_t group = (rm >> 3) & 7;
	const uint8_t sub   = (rm & 7);
	switch (group) {
	case 0x00: /* FADD STi,ST*/ FPU_FADD(STV(sub), TOP); break;
	case 0x01: /* FMUL STi,ST*/ FPU_FMUL(STV(sub), TOP); break;
	case 0x02:  /* FCOM*/
		FPU_FCOM(TOP,STV(sub));
		break;
	case 0x03:  /* FCOMP*/
		FPU_FCOM(TOP,STV(sub));
		FPU_FPOP();
		break;
	case 0x04:  /* FSUBR STi,ST*/
		FPU_FSUBR(STV(sub),TOP);
		break;
	case 0x05:  /* FSUB  STi,ST*/
		FPU_FSUB(STV(sub),TOP);
		break;
	case 0x06:  /* FDIVR STi,ST*/
		FPU_FDIVR(STV(sub),TOP);
		break;
	case 0x07:  /* FDIV STi,ST*/
		FPU_FDIV(STV(sub),TOP);
		break;
	default:
		break;
	}
}

void FPU_ESC5_EA(Bitu rm,PhysPt addr) {
	const uint8_t group = (rm >> 3) & 7;
	const uint8_t sub   = (rm & 7);
	switch (group) {
	case 0x00: /* FLD double real*/
		FPU_PREP_PUSH();
		FPU_FLD_F64(addr,TOP);
		break;
	case 0x01:  /* FISTTP longint*/
		FPU_LOG_WARN(5,true,group,sub);
		break;
	case 0x02:   /* FST double real*/
		FPU_FST_F64(addr);
		break;
	case 0x03:	/* FSTP double real*/
		FPU_FST_F64(addr);
		FPU_FPOP();
		break;
	case 0x04:	/* FRSTOR */
		FPU_FRSTOR(addr);
		break;
	case 0x06:	/* FSAVE */
		FPU_FSAVE(addr);
		break;
	case 0x07:   /*FNSTSW    NG DISAGREES ON THIS*/
		FPU_SET_TOP(TOP);
		mem_writew(addr,fpu.sw);
		//seems to break all dos4gw games :)
		break;
	default:
		FPU_LOG_WARN(5,true,group,sub);
		break;
	}
}

void FPU_ESC5_Normal(Bitu rm) {
	const uint8_t group = (rm >> 3) & 7;
	const uint8_t sub   = (rm & 7);
	switch (group) {
	case 0x00: /* FFREE STi */ fpu.tags[STV(sub)] = TAG_Empty; break;
	case 0x01: /* FXCH STi*/
		FPU_FXCH(TOP,STV(sub));
		break;
	case 0x02: /* FST STi */
		FPU_FST(TOP,STV(sub));
		break;
	case 0x03:  /* FSTP STi*/
		FPU_FST(TOP,STV(sub));
		FPU_FPOP();
		break;
	case 0x04:	/* FUCOM STi */
		FPU_FUCOM(TOP,STV(sub));
		break;
	case 0x05:	/*FUCOMP STi */
		FPU_FUCOM(TOP,STV(sub));
		FPU_FPOP();
		break;
	default:
		FPU_LOG_WARN(5,false,group,sub);
		break;
	}
}

void FPU_ESC6_EA(Bitu rm,PhysPt addr) {
	/* 16 bit (word integer) operants */
	FPU_FLD_I16_EA(addr);
	EATREE(rm);
}

void FPU_ESC6_Normal(Bitu rm) {
	/* all P variants working only on registers */
	/* get top before switch and pop afterwards */
	const uint8_t group = (rm >> 3) & 7;
	const uint8_t sub   = (rm & 7);
	switch (group) {
	case 0x00: /*FADDP STi,ST*/ FPU_FADD(STV(sub), TOP); break;
	case 0x01: /* FMULP STi,ST*/ FPU_FMUL(STV(sub), TOP); break;
	case 0x02:  /* FCOMP5*/
		FPU_FCOM(TOP,STV(sub));
		break;	/* TODO IS THIS ALLRIGHT ????????? */
	case 0x03:  /*FCOMPP*/
		if(sub != 1) {
			FPU_LOG_WARN(6,false,group,sub);
			return;
		}
		FPU_FCOM(TOP,STV(1));
		FPU_FPOP(); /* extra pop at the bottom*/
		break;
	case 0x04:  /* FSUBRP STi,ST*/
		FPU_FSUBR(STV(sub),TOP);
		break;
	case 0x05:  /* FSUBP  STi,ST*/
		FPU_FSUB(STV(sub),TOP);
		break;
	case 0x06:	/* FDIVRP STi,ST*/
		FPU_FDIVR(STV(sub),TOP);
		break;
	case 0x07:  /* FDIVP STi,ST*/
		FPU_FDIV(STV(sub),TOP);
		break;
	default:
		break;
	}
	FPU_FPOP();		
}


void FPU_ESC7_EA(Bitu rm,PhysPt addr) {
	const uint8_t group = (rm >> 3) & 7;
	const uint8_t sub   = (rm & 7);
	switch (group) {
	case 0x00: /* FILD int16_t */
		FPU_PREP_PUSH();
		FPU_FLD_I16(addr,TOP);
		break;
	case 0x01:
		FPU_LOG_WARN(7,true,group,sub);
		break;
	case 0x02:   /* FIST int16_t */
		FPU_FST_I16(addr);
		break;
	case 0x03:	/* FISTP int16_t */
		FPU_FST_I16(addr);
		FPU_FPOP();
		break;
	case 0x04:   /* FBLD packed BCD */
		FPU_PREP_PUSH();
		FPU_FBLD(addr,TOP);
		break;
	case 0x05:  /* FILD int64_t */
		FPU_PREP_PUSH();
		FPU_FLD_I64(addr,TOP);
		break;
	case 0x06:	/* FBSTP packed BCD */
		FPU_FBST(addr);
		FPU_FPOP();
		break;
	case 0x07:  /* FISTP int64_t */
		FPU_FST_I64(addr);
		FPU_FPOP();
		break;
	default:
		FPU_LOG_WARN(7,true,group,sub);
		break;
	}
}

void FPU_ESC7_Normal(Bitu rm) {
	const uint8_t group = (rm >> 3) & 7;
	const uint8_t sub   = (rm & 7);
	switch (group) {
	case 0x00: /* FFREEP STi*/
		fpu.tags[STV(sub)] = TAG_Empty;
		FPU_FPOP();
		break;
	case 0x01: /* FXCH STi*/
		FPU_FXCH(TOP,STV(sub));
		break;
	case 0x02:  /* FSTP STi*/
	case 0x03:  /* FSTP STi*/
		FPU_FST(TOP,STV(sub));
		FPU_FPOP();
		break;
	case 0x04:
		switch(sub){
			case 0x00:     /* FNSTSW AX*/
				FPU_SET_TOP(TOP);
				reg_ax = fpu.sw;
				break;
			default:
				FPU_LOG_WARN(7,false,group,sub);
				break;
		}
		break;
	default:
		FPU_LOG_WARN(7,false,group,sub);
		break;
	}
}


void FPU_Init(Section*) {
#if !C_FPU_X86
	LOG_WARNING("FPU: Using reduced-precision floating-point emulation");
#endif
	FPU_FINIT();
}

#endif
