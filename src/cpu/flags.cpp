// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Lazy flag system was designed after the same kind of system used in Bochs.
	Probably still some bugs left in here.
*/

#include "dosbox.h"
#include "cpu.h"
#include "lazyflags.h"
#include "pic.h"

LazyFlags lflags;

/* CF     Carry Flag -- Set on high-order bit carry or borrow; cleared
          otherwise.
*/
uint32_t get_CF(void) {
	switch (lflags.type) {
	case t_UNKNOWN:
	case t_INCb:
	case t_INCw:
	case t_INCd:
	case t_DECb:
	case t_DECw:
	case t_DECd:
	case t_MUL:
		return GETFLAG(CF);
	case t_ADDb:	
		return (lf_resb<lf_var1b);
	case t_ADDw:	
		return (lf_resw<lf_var1w);
	case t_ADDd:
		return (lf_resd<lf_var1d);
	case t_ADCb:
		return (lf_resb < lf_var1b) || (lflags.oldcf && (lf_resb == lf_var1b));
	case t_ADCw:
		return (lf_resw < lf_var1w) || (lflags.oldcf && (lf_resw == lf_var1w));
	case t_ADCd:
		return (lf_resd < lf_var1d) || (lflags.oldcf && (lf_resd == lf_var1d));
	case t_SBBb:
		return (lf_var1b < lf_resb) || (lflags.oldcf && (lf_var2b==0xff));
	case t_SBBw:
		return (lf_var1w < lf_resw) || (lflags.oldcf && (lf_var2w==0xffff));
	case t_SBBd:
		return (lf_var1d < lf_resd) || (lflags.oldcf && (lf_var2d==0xffffffff));
	case t_SUBb:
	case t_CMPb:
		return (lf_var1b<lf_var2b);
	case t_SUBw:
	case t_CMPw:
		return (lf_var1w<lf_var2w);
	case t_SUBd:
	case t_CMPd:
		return (lf_var1d<lf_var2d);
	case t_SHLb:
		if (lf_var2b>8) return false;
		else return (lf_var1b >> (8-lf_var2b)) & 1;
	case t_SHLw:
		if (lf_var2b>16) return false;
		else return (lf_var1w >> (16-lf_var2b)) & 1;
	case t_SHLd:
	case t_DSHLw:	/* Hmm this is not correct for shift higher than 16 */
	case t_DSHLd:
		return (lf_var1d >> (32 - lf_var2b)) & 1;
	case t_RCRb:
	case t_SHRb: return (lf_var1b >> lf_var2b_minus_one()) & 1;
	case t_RCRw:
	case t_SHRw: return (lf_var1w >> lf_var2b_minus_one()) & 1;
	case t_RCRd:
	case t_SHRd:
	case t_DSHRw:	/* Hmm this is not correct for shift higher than 16 */
	case t_DSHRd: return (lf_var1d >> lf_var2b_minus_one()) & 1;
	case t_SARb: return (((int8_t)lf_var1b) >> lf_var2b_minus_one()) & 1;
	case t_SARw: return (((int16_t)lf_var1w) >> lf_var2b_minus_one()) & 1;
	case t_SARd: return (((int32_t)lf_var1d) >> lf_var2b_minus_one()) & 1;
	case t_NEGb: return lf_var1b;
	case t_NEGw:
		return lf_var1w;
	case t_NEGd:
		return lf_var1d;
	case t_ORb:
	case t_ORw:
	case t_ORd:
	case t_ANDb:
	case t_ANDw:
	case t_ANDd:
	case t_XORb:
	case t_XORw:
	case t_XORd:
	case t_TESTb:
	case t_TESTw:
	case t_TESTd:
		return false;	/* Set to false */
	case t_DIV:
		return false;	/* Unknown */
	default:
		LOG(LOG_CPU,LOG_ERROR)("get_CF Unknown %u",lflags.type);
	}
	return 0;
}

/* AF     Adjust flag -- Set on carry from or borrow to the low order
            four bits of   AL; cleared otherwise. Used for decimal
            arithmetic.
*/
uint32_t get_AF(void) {
	switch (lflags.type) {
	case t_UNKNOWN:
		return GETFLAG(AF);
	case t_ADDb:	
	case t_ADCb:
	case t_SBBb:
	case t_SUBb:
	case t_CMPb:
		return ((lf_var1b ^ lf_var2b) ^ lf_resb) & 0x10;
	case t_ADDw:
	case t_ADCw:
	case t_SBBw:
	case t_SUBw:
	case t_CMPw:
		return ((lf_var1w ^ lf_var2w) ^ lf_resw) & 0x10;
	case t_ADCd:
	case t_ADDd:
	case t_SBBd:
	case t_SUBd:
	case t_CMPd:
		return ((lf_var1d ^ lf_var2d) ^ lf_resd) & 0x10;
	case t_INCb:
		return (lf_resb & 0x0f) == 0;
	case t_INCw:
		return (lf_resw & 0x0f) == 0;
	case t_INCd:
		return (lf_resd & 0x0f) == 0;
	case t_DECb:
		return (lf_resb & 0x0f) == 0x0f;
	case t_DECw:
		return (lf_resw & 0x0f) == 0x0f;
	case t_DECd:
		return (lf_resd & 0x0f) == 0x0f;
	case t_NEGb:
		return lf_var1b & 0x0f;
	case t_NEGw:
		return lf_var1w & 0x0f;
	case t_NEGd:
		return lf_var1d & 0x0f;
	case t_SHLb:
	case t_SHRb:
	case t_SARb:
		return lf_var2b & 0x1f;
	case t_SHLw:
	case t_SHRw:
	case t_SARw:
		return lf_var2w & 0x1f;
	case t_SHLd:
	case t_SHRd:
	case t_SARd:
		return lf_var2d & 0x1f;
	case t_ORb:
	case t_ORw:
	case t_ORd:
	case t_ANDb:
	case t_ANDw:
	case t_ANDd:
	case t_XORb:
	case t_XORw:
	case t_XORd:
	case t_TESTb:
	case t_TESTw:
	case t_TESTd:
	case t_DSHLw:
	case t_DSHLd:
	case t_DSHRw:
	case t_DSHRd:
	case t_DIV:
	case t_MUL:
		return false; /* Unknown */
	default:
		LOG(LOG_CPU,LOG_ERROR)("get_AF Unknown %u",lflags.type);
	}
	return 0;
}

/* ZF     Zero Flag -- Set if result is zero; cleared otherwise.
*/

uint32_t get_ZF(void) {
	switch (lflags.type) {
	case t_UNKNOWN:
		return GETFLAG(ZF);
	case t_ADDb:	
	case t_ORb:
	case t_ADCb:
	case t_SBBb:
	case t_ANDb:
	case t_XORb:
	case t_SUBb:
	case t_CMPb:
	case t_INCb:
	case t_DECb:
	case t_TESTb:
	case t_SHLb:
	case t_SHRb:
	case t_SARb:
	case t_NEGb:
		return (lf_resb==0);
	case t_ADDw:	
	case t_ORw:
	case t_ADCw:
	case t_SBBw:
	case t_ANDw:
	case t_XORw:
	case t_SUBw:
	case t_CMPw:
	case t_INCw:
	case t_DECw:
	case t_TESTw:
	case t_SHLw:
	case t_SHRw:
	case t_SARw:
	case t_DSHLw:
	case t_DSHRw:
	case t_NEGw:
		return (lf_resw==0);
	case t_ADDd:
	case t_ORd:
	case t_ADCd:
	case t_SBBd:
	case t_ANDd:
	case t_XORd:
	case t_SUBd:
	case t_CMPd:
	case t_INCd:
	case t_DECd:
	case t_TESTd:
	case t_SHLd:
	case t_SHRd:
	case t_SARd:
	case t_DSHLd:
	case t_DSHRd:
	case t_NEGd:
		return (lf_resd==0);
	case t_DIV:
	case t_MUL:
		return false; /* Unknown */
	default:
		LOG(LOG_CPU,LOG_ERROR)("get_ZF Unknown %u",lflags.type);
	}
	return false;
}
/* SF     Sign Flag -- Set equal to high-order bit of result (0 is
            positive, 1 if negative).
*/
uint32_t get_SF(void) {
	switch (lflags.type) {
	case t_UNKNOWN:
		return GETFLAG(SF);
	case t_ADDb:
	case t_ORb:
	case t_ADCb:
	case t_SBBb:
	case t_ANDb:
	case t_XORb:
	case t_SUBb:
	case t_CMPb:
	case t_INCb:
	case t_DECb:
	case t_TESTb:
	case t_SHLb:
	case t_SHRb:
	case t_SARb:
	case t_NEGb:
		return	(lf_resb&0x80);
	case t_ADDw:
	case t_ORw:
	case t_ADCw:
	case t_SBBw:
	case t_ANDw:
	case t_XORw:
	case t_SUBw:
	case t_CMPw:
	case t_INCw:
	case t_DECw:
	case t_TESTw:
	case t_SHLw:
	case t_SHRw:
	case t_SARw:
	case t_DSHLw:
	case t_DSHRw:
	case t_NEGw:
		return	(lf_resw&0x8000);
	case t_ADDd:
	case t_ORd:
	case t_ADCd:
	case t_SBBd:
	case t_ANDd:
	case t_XORd:
	case t_SUBd:
	case t_CMPd:
	case t_INCd:
	case t_DECd:
	case t_TESTd:
	case t_SHLd:
	case t_SHRd:
	case t_SARd:
	case t_DSHLd:
	case t_DSHRd:
	case t_NEGd:
		return	(lf_resd&0x80000000);
	case t_DIV:
	case t_MUL:
		return false; /* Unknown */
	default:
		LOG(LOG_CPU,LOG_ERROR)("get_SF Unknown %u",lflags.type);
	}
	return false;

}
uint32_t get_OF(void) {
	switch (lflags.type) {
	case t_UNKNOWN:
	case t_MUL:
		return GETFLAG(OF);
	case t_ADDb:
	case t_ADCb:
		return ((lf_var1b ^ lf_var2b ^ 0x80) & (lf_resb ^ lf_var2b)) & 0x80;
	case t_ADDw:
	case t_ADCw:
		return ((lf_var1w ^ lf_var2w ^ 0x8000) & (lf_resw ^ lf_var2w)) & 0x8000;
	case t_ADDd:
	case t_ADCd:
		return ((lf_var1d ^ lf_var2d ^ 0x80000000) & (lf_resd ^ lf_var2d)) & 0x80000000;
	case t_SBBb:
	case t_SUBb:
	case t_CMPb:
		return ((lf_var1b ^ lf_var2b) & (lf_var1b ^ lf_resb)) & 0x80;
	case t_SBBw:
	case t_SUBw:
	case t_CMPw:
		return ((lf_var1w ^ lf_var2w) & (lf_var1w ^ lf_resw)) & 0x8000;
	case t_SBBd:
	case t_SUBd:
	case t_CMPd:
		return ((lf_var1d ^ lf_var2d) & (lf_var1d ^ lf_resd)) & 0x80000000;
	case t_INCb:
		return (lf_resb == 0x80);
	case t_INCw:
		return (lf_resw == 0x8000);
	case t_INCd:
		return (lf_resd == 0x80000000);
	case t_DECb:
		return (lf_resb == 0x7f);
	case t_DECw:
		return (lf_resw == 0x7fff);
	case t_DECd:
		return (lf_resd == 0x7fffffff);
	case t_NEGb:
		return (lf_var1b == 0x80);
	case t_NEGw:
		return (lf_var1w == 0x8000);
	case t_NEGd:
		return (lf_var1d == 0x80000000);
	case t_SHLb:
		return (lf_resb ^ lf_var1b) & 0x80;
	case t_SHLw:
	case t_DSHRw:
	case t_DSHLw:
		return (lf_resw ^ lf_var1w) & 0x8000;
	case t_SHLd:
	case t_DSHRd:
	case t_DSHLd:
		return (lf_resd ^ lf_var1d) & 0x80000000;
	case t_SHRb:
		if ((lf_var2b&0x1f)==1) return (lf_var1b > 0x80);
		else return false;
	case t_SHRw:
		if ((lf_var2b&0x1f)==1) return (lf_var1w > 0x8000);
		else return false;
	case t_SHRd:
		if ((lf_var2b&0x1f)==1) return (lf_var1d > 0x80000000);
		else return false;
	case t_ORb:
	case t_ORw:
	case t_ORd:
	case t_ANDb:
	case t_ANDw:
	case t_ANDd:
	case t_XORb:
	case t_XORw:
	case t_XORd:
	case t_TESTb:
	case t_TESTw:
	case t_TESTd:
	case t_SARb:
	case t_SARw:
	case t_SARd:
		return false;			/* Return false */
	case t_DIV:
		return false; /* Unknown */
	default:
		LOG(LOG_CPU,LOG_ERROR)("get_OF Unknown %u",lflags.type);
	}
	return false;
}

uint16_t parity_lookup[256] = {
  FLAG_PF, 0, 0, FLAG_PF, 0, FLAG_PF, FLAG_PF, 0, 0, FLAG_PF, FLAG_PF, 0, FLAG_PF, 0, 0, FLAG_PF,
  0, FLAG_PF, FLAG_PF, 0, FLAG_PF, 0, 0, FLAG_PF, FLAG_PF, 0, 0, FLAG_PF, 0, FLAG_PF, FLAG_PF, 0,
  0, FLAG_PF, FLAG_PF, 0, FLAG_PF, 0, 0, FLAG_PF, FLAG_PF, 0, 0, FLAG_PF, 0, FLAG_PF, FLAG_PF, 0,
  FLAG_PF, 0, 0, FLAG_PF, 0, FLAG_PF, FLAG_PF, 0, 0, FLAG_PF, FLAG_PF, 0, FLAG_PF, 0, 0, FLAG_PF,
  0, FLAG_PF, FLAG_PF, 0, FLAG_PF, 0, 0, FLAG_PF, FLAG_PF, 0, 0, FLAG_PF, 0, FLAG_PF, FLAG_PF, 0,
  FLAG_PF, 0, 0, FLAG_PF, 0, FLAG_PF, FLAG_PF, 0, 0, FLAG_PF, FLAG_PF, 0, FLAG_PF, 0, 0, FLAG_PF,
  FLAG_PF, 0, 0, FLAG_PF, 0, FLAG_PF, FLAG_PF, 0, 0, FLAG_PF, FLAG_PF, 0, FLAG_PF, 0, 0, FLAG_PF,
  0, FLAG_PF, FLAG_PF, 0, FLAG_PF, 0, 0, FLAG_PF, FLAG_PF, 0, 0, FLAG_PF, 0, FLAG_PF, FLAG_PF, 0,
  0, FLAG_PF, FLAG_PF, 0, FLAG_PF, 0, 0, FLAG_PF, FLAG_PF, 0, 0, FLAG_PF, 0, FLAG_PF, FLAG_PF, 0,
  FLAG_PF, 0, 0, FLAG_PF, 0, FLAG_PF, FLAG_PF, 0, 0, FLAG_PF, FLAG_PF, 0, FLAG_PF, 0, 0, FLAG_PF,
  FLAG_PF, 0, 0, FLAG_PF, 0, FLAG_PF, FLAG_PF, 0, 0, FLAG_PF, FLAG_PF, 0, FLAG_PF, 0, 0, FLAG_PF,
  0, FLAG_PF, FLAG_PF, 0, FLAG_PF, 0, 0, FLAG_PF, FLAG_PF, 0, 0, FLAG_PF, 0, FLAG_PF, FLAG_PF, 0,
  FLAG_PF, 0, 0, FLAG_PF, 0, FLAG_PF, FLAG_PF, 0, 0, FLAG_PF, FLAG_PF, 0, FLAG_PF, 0, 0, FLAG_PF,
  0, FLAG_PF, FLAG_PF, 0, FLAG_PF, 0, 0, FLAG_PF, FLAG_PF, 0, 0, FLAG_PF, 0, FLAG_PF, FLAG_PF, 0,
  0, FLAG_PF, FLAG_PF, 0, FLAG_PF, 0, 0, FLAG_PF, FLAG_PF, 0, 0, FLAG_PF, 0, FLAG_PF, FLAG_PF, 0,
  FLAG_PF, 0, 0, FLAG_PF, 0, FLAG_PF, FLAG_PF, 0, 0, FLAG_PF, FLAG_PF, 0, FLAG_PF, 0, 0, FLAG_PF
  };

uint32_t get_PF(void) {
	switch (lflags.type) {
	case t_UNKNOWN:
		return GETFLAG(PF);
	default:
		return	(parity_lookup[lf_resb]);
	};
	return 0;
}


#if 0

uint32_t FillFlags(void) {
//	if (lflags.type==t_UNKNOWN) return reg_flags;
	Bitu new_word=(reg_flags & ~FLAG_MASK);
	if (get_CF()) new_word|=FLAG_CF;
	if (get_PF()) new_word|=FLAG_PF;
	if (get_AF()) new_word|=FLAG_AF;
	if (get_ZF()) new_word|=FLAG_ZF;
	if (get_SF()) new_word|=FLAG_SF;
	if (get_OF()) new_word|=FLAG_OF;
	reg_flags=new_word;
	lflags.type=t_UNKNOWN;
	return reg_flags;
}

#else

#define DOFLAG_PF	reg_flags=(reg_flags & ~FLAG_PF) | parity_lookup[lf_resb];

#define DOFLAG_AF	reg_flags=(reg_flags & ~FLAG_AF) | (((lf_var1b ^ lf_var2b) ^ lf_resb) & 0x10);

#define DOFLAG_ZFb	SETFLAGBIT(ZF,lf_resb==0);
#define DOFLAG_ZFw	SETFLAGBIT(ZF,lf_resw==0);
#define DOFLAG_ZFd	SETFLAGBIT(ZF,lf_resd==0);

#define DOFLAG_SFb	reg_flags=(reg_flags & ~FLAG_SF) | ((lf_resb & 0x80) >> 0);
#define DOFLAG_SFw	reg_flags=(reg_flags & ~FLAG_SF) | ((lf_resw & 0x8000) >> 8);
#define DOFLAG_SFd	reg_flags=(reg_flags & ~FLAG_SF) | ((lf_resd & 0x80000000) >> 24);

#define SETCF(NEWBIT) reg_flags=(reg_flags & ~FLAG_CF)|(NEWBIT);

#define SET_FLAG SETFLAGBIT

uint32_t FillFlags(void) {
	switch (lflags.type) {
	case t_UNKNOWN:
		break;
	case t_ADDb:	
		SET_FLAG(CF,(lf_resb<lf_var1b));
		DOFLAG_AF;
		DOFLAG_ZFb;
		DOFLAG_SFb;
		SET_FLAG(OF,((lf_var1b ^ lf_var2b ^ 0x80) & (lf_resb ^ lf_var1b)) & 0x80);
		DOFLAG_PF;
		break;
	case t_ADDw:	
		SET_FLAG(CF,(lf_resw<lf_var1w));
		DOFLAG_AF;
		DOFLAG_ZFw;
		DOFLAG_SFw;
		SET_FLAG(OF,((lf_var1w ^ lf_var2w ^ 0x8000) & (lf_resw ^ lf_var1w)) & 0x8000);
		DOFLAG_PF;
		break;
	case t_ADDd:
		SET_FLAG(CF,(lf_resd<lf_var1d));
		DOFLAG_AF;
		DOFLAG_ZFd;
		DOFLAG_SFd;
		SET_FLAG(OF,((lf_var1d ^ lf_var2d ^ 0x80000000) & (lf_resd ^ lf_var1d)) & 0x80000000);
		DOFLAG_PF;
		break;
	case t_ADCb:
		SET_FLAG(CF,(lf_resb < lf_var1b) || (lflags.oldcf && (lf_resb == lf_var1b)));
		DOFLAG_AF;
		DOFLAG_ZFb;
		DOFLAG_SFb;
		SET_FLAG(OF,((lf_var1b ^ lf_var2b ^ 0x80) & (lf_resb ^ lf_var1b)) & 0x80);
		DOFLAG_PF;
		break;
	case t_ADCw:
		SET_FLAG(CF,(lf_resw < lf_var1w) || (lflags.oldcf && (lf_resw == lf_var1w)));
		DOFLAG_AF;
		DOFLAG_ZFw;
		DOFLAG_SFw;
		SET_FLAG(OF,((lf_var1w ^ lf_var2w ^ 0x8000) & (lf_resw ^ lf_var1w)) & 0x8000);
		DOFLAG_PF;
		break;
	case t_ADCd:
		SET_FLAG(CF,(lf_resd < lf_var1d) || (lflags.oldcf && (lf_resd == lf_var1d)));
		DOFLAG_AF;
		DOFLAG_ZFd;
		DOFLAG_SFd;
		SET_FLAG(OF,((lf_var1d ^ lf_var2d ^ 0x80000000) & (lf_resd ^ lf_var1d)) & 0x80000000);
		DOFLAG_PF;
		break;


	case t_SBBb:
		SET_FLAG(CF,(lf_var1b < lf_resb) || (lflags.oldcf && (lf_var2b==0xff)));
		DOFLAG_AF;
		DOFLAG_ZFb;
		DOFLAG_SFb;
		SET_FLAG(OF,(lf_var1b ^ lf_var2b) & (lf_var1b ^ lf_resb) & 0x80);
		DOFLAG_PF;
		break;
	case t_SBBw:
		SET_FLAG(CF,(lf_var1w < lf_resw) || (lflags.oldcf && (lf_var2w==0xffff)));
		DOFLAG_AF;
		DOFLAG_ZFw;
		DOFLAG_SFw;
		SET_FLAG(OF,(lf_var1w ^ lf_var2w) & (lf_var1w ^ lf_resw) & 0x8000);
		DOFLAG_PF;
		break;
	case t_SBBd:
		SET_FLAG(CF,(lf_var1d < lf_resd) || (lflags.oldcf && (lf_var2d==0xffffffff)));
		DOFLAG_AF;
		DOFLAG_ZFd;
		DOFLAG_SFd;
		SET_FLAG(OF,(lf_var1d ^ lf_var2d) & (lf_var1d ^ lf_resd) & 0x80000000);
		DOFLAG_PF;
		break;
	

	case t_SUBb:
	case t_CMPb:
		SET_FLAG(CF,(lf_var1b<lf_var2b));
		DOFLAG_AF;
		DOFLAG_ZFb;
		DOFLAG_SFb;
		SET_FLAG(OF,(lf_var1b ^ lf_var2b) & (lf_var1b ^ lf_resb) & 0x80);
		DOFLAG_PF;
		break;
	case t_SUBw:
	case t_CMPw:
		SET_FLAG(CF,(lf_var1w<lf_var2w));
		DOFLAG_AF;
		DOFLAG_ZFw;
		DOFLAG_SFw;
		SET_FLAG(OF,(lf_var1w ^ lf_var2w) & (lf_var1w ^ lf_resw) & 0x8000);
		DOFLAG_PF;
		break;
	case t_SUBd:
	case t_CMPd:
		SET_FLAG(CF,(lf_var1d<lf_var2d));
		DOFLAG_AF;
		DOFLAG_ZFd;
		DOFLAG_SFd;
		SET_FLAG(OF,(lf_var1d ^ lf_var2d) & (lf_var1d ^ lf_resd) & 0x80000000);
		DOFLAG_PF;
		break;
	
	case t_ORb:
	case t_TESTb:
	case t_ANDb:
	case t_XORb:
		SET_FLAG(CF,false);
		SET_FLAG(AF,false);
		DOFLAG_ZFb;
		DOFLAG_SFb;
		SET_FLAG(OF,false);
		DOFLAG_PF;
		break;

	case t_ORw:
	case t_TESTw:
	case t_ANDw:
	case t_XORw:
		SET_FLAG(CF,false);
		SET_FLAG(AF,false);
		DOFLAG_ZFw;
		DOFLAG_SFw;
		SET_FLAG(OF,false);
		DOFLAG_PF;
		break;

	case t_ORd:
	case t_TESTd:
	case t_ANDd:
	case t_XORd:
		SET_FLAG(CF,false);
		SET_FLAG(AF,false);
		DOFLAG_ZFd;
		DOFLAG_SFd;
		SET_FLAG(OF,false);
		DOFLAG_PF;
		break;

	case t_SHLb:
		if (lf_var2b>8) SET_FLAG(CF,false);
		else SET_FLAG(CF,(lf_var1b >> (8-lf_var2b)) & 1);
		DOFLAG_ZFb;
		DOFLAG_SFb;
		SET_FLAG(OF,(lf_resb ^ lf_var1b) & 0x80);
		DOFLAG_PF;
		SET_FLAG(AF,(lf_var2b&0x1f));
		break;
	case t_SHLw:
		if (lf_var2b>16) SET_FLAG(CF,false);
		else SET_FLAG(CF,(lf_var1w >> (16-lf_var2b)) & 1);
		DOFLAG_ZFw;
		DOFLAG_SFw;
		SET_FLAG(OF,(lf_resw ^ lf_var1w) & 0x8000);
		DOFLAG_PF;
		SET_FLAG(AF,(lf_var2w&0x1f));
		break;
	case t_SHLd:
		SET_FLAG(CF,(lf_var1d >> (32 - lf_var2b)) & 1);
		DOFLAG_ZFd;
		DOFLAG_SFd;
		SET_FLAG(OF,(lf_resd ^ lf_var1d) & 0x80000000);
		DOFLAG_PF;
		SET_FLAG(AF,(lf_var2d&0x1f));
		break;


	case t_DSHLw:
		SET_FLAG(CF,(lf_var1d >> (32 - lf_var2b)) & 1);
		DOFLAG_ZFw;
		DOFLAG_SFw;
		SET_FLAG(OF,(lf_resw ^ lf_var1w) & 0x8000);
		DOFLAG_PF;
		break;
	case t_DSHLd:
		SET_FLAG(CF,(lf_var1d >> (32 - lf_var2b)) & 1);
		DOFLAG_ZFd;
		DOFLAG_SFd;
		SET_FLAG(OF,(lf_resd ^ lf_var1d) & 0x80000000);
		DOFLAG_PF;
		break;


	case t_SHRb:
		SET_FLAG(CF, (lf_var1b >> lf_var2b_minus_one()) & 1);
		DOFLAG_ZFb;
		DOFLAG_SFb;
		if ((lf_var2b&0x1f)==1) SET_FLAG(OF,(lf_var1b >= 0x80));
		else SET_FLAG(OF,false);
		DOFLAG_PF;
		SET_FLAG(AF,(lf_var2b&0x1f));
		break;
	case t_SHRw:
		SET_FLAG(CF, (lf_var1w >> lf_var2b_minus_one()) & 1);
		DOFLAG_ZFw;
		DOFLAG_SFw;
		if ((lf_var2w&0x1f)==1) SET_FLAG(OF,(lf_var1w >= 0x8000));
		else SET_FLAG(OF,false);
		DOFLAG_PF;
		SET_FLAG(AF,(lf_var2w&0x1f));
		break;
	case t_SHRd:
		SET_FLAG(CF, (lf_var1d >> lf_var2b_minus_one()) & 1);
		DOFLAG_ZFd;
		DOFLAG_SFd;
		if ((lf_var2d&0x1f)==1) SET_FLAG(OF,(lf_var1d >= 0x80000000));
		else SET_FLAG(OF,false);
		DOFLAG_PF;
		SET_FLAG(AF,(lf_var2d&0x1f));
		break;

	
	case t_DSHRw:	/* Hmm this is not correct for shift higher than 16 */
		SET_FLAG(CF, (lf_var1d >> lf_var2b_minus_one()) & 1);
		DOFLAG_ZFw;
		DOFLAG_SFw;
		SET_FLAG(OF,(lf_resw ^ lf_var1w) & 0x8000);
		DOFLAG_PF;
		break;
	case t_DSHRd:
		SET_FLAG(CF, (lf_var1d >> lf_var2b_minus_one()) & 1);
		DOFLAG_ZFd;
		DOFLAG_SFd;
		SET_FLAG(OF,(lf_resd ^ lf_var1d) & 0x80000000);
		DOFLAG_PF;
		break;


	case t_SARb:
		SET_FLAG(CF, (((int8_t)lf_var1b) >> lf_var2b_minus_one()) & 1);
		DOFLAG_ZFb;
		DOFLAG_SFb;
		SET_FLAG(OF,false);
		DOFLAG_PF;
		SET_FLAG(AF,(lf_var2b&0x1f));
		break;
	case t_SARw:
		SET_FLAG(CF, (((int16_t)lf_var1w) >> lf_var2b_minus_one()) & 1);
		DOFLAG_ZFw;
		DOFLAG_SFw;
		SET_FLAG(OF,false);
		DOFLAG_PF;
		SET_FLAG(AF,(lf_var2w&0x1f));
		break;
	case t_SARd:
		SET_FLAG(CF, (((int32_t)lf_var1d) >> lf_var2b_minus_one()) & 1);
		DOFLAG_ZFd;
		DOFLAG_SFd;
		SET_FLAG(OF,false);
		DOFLAG_PF;
		SET_FLAG(AF,(lf_var2d&0x1f));
		break;

	case t_INCb:
		SET_FLAG(AF,(lf_resb & 0x0f) == 0);
		DOFLAG_ZFb;
		DOFLAG_SFb;
		SET_FLAG(OF,(lf_resb == 0x80));
		DOFLAG_PF;
		break;
	case t_INCw:
		SET_FLAG(AF,(lf_resw & 0x0f) == 0);
		DOFLAG_ZFw;
		DOFLAG_SFw;
		SET_FLAG(OF,(lf_resw == 0x8000));
		DOFLAG_PF;
		break;
	case t_INCd:
		SET_FLAG(AF,(lf_resd & 0x0f) == 0);
		DOFLAG_ZFd;
		DOFLAG_SFd;
		SET_FLAG(OF,(lf_resd == 0x80000000));
		DOFLAG_PF;
		break;

	case t_DECb:
		SET_FLAG(AF,(lf_resb & 0x0f) == 0x0f);
		DOFLAG_ZFb;
		DOFLAG_SFb;
		SET_FLAG(OF,(lf_resb == 0x7f));
		DOFLAG_PF;
		break;
	case t_DECw:
		SET_FLAG(AF,(lf_resw & 0x0f) == 0x0f);
		DOFLAG_ZFw;
		DOFLAG_SFw;
		SET_FLAG(OF,(lf_resw == 0x7fff));
		DOFLAG_PF;
		break;
	case t_DECd:
		SET_FLAG(AF,(lf_resd & 0x0f) == 0x0f);
		DOFLAG_ZFd;
		DOFLAG_SFd;
		SET_FLAG(OF,(lf_resd == 0x7fffffff));
		DOFLAG_PF;
		break;

	case t_NEGb:
		SET_FLAG(CF,(lf_var1b!=0));
		SET_FLAG(AF,(lf_resb & 0x0f) != 0);
		DOFLAG_ZFb;
		DOFLAG_SFb;
		SET_FLAG(OF,(lf_var1b == 0x80));
		DOFLAG_PF;
		break;
	case t_NEGw:
		SET_FLAG(CF,(lf_var1w!=0));
		SET_FLAG(AF,(lf_resw & 0x0f) != 0);
		DOFLAG_ZFw;
		DOFLAG_SFw;
		SET_FLAG(OF,(lf_var1w == 0x8000));
		DOFLAG_PF;
		break;
	case t_NEGd:
		SET_FLAG(CF,(lf_var1d!=0));
		SET_FLAG(AF,(lf_resd & 0x0f) != 0);
		DOFLAG_ZFd;
		DOFLAG_SFd;
		SET_FLAG(OF,(lf_var1d == 0x80000000));
		DOFLAG_PF;
		break;

	
	case t_DIV:
	case t_MUL:
		break;

	default:
		LOG(LOG_CPU,LOG_ERROR)("Unhandled flag type %u",lflags.type);
		return 0;
	}
	lflags.type=t_UNKNOWN;
	return reg_flags;
}

void FillFlagsNoCFOF(void) {
	switch (lflags.type) {
	case t_UNKNOWN:
		return;
	case t_ADDb:	
		DOFLAG_AF;
		DOFLAG_ZFb;
		DOFLAG_SFb;
		DOFLAG_PF;
		break;
	case t_ADDw:	
		DOFLAG_AF;
		DOFLAG_ZFw;
		DOFLAG_SFw;
		DOFLAG_PF;
		break;
	case t_ADDd:
		DOFLAG_AF;
		DOFLAG_ZFd;
		DOFLAG_SFd;
		DOFLAG_PF;
		break;
	case t_ADCb:
		DOFLAG_AF;
		DOFLAG_ZFb;
		DOFLAG_SFb;
		DOFLAG_PF;
		break;
	case t_ADCw:
		DOFLAG_AF;
		DOFLAG_ZFw;
		DOFLAG_SFw;
		DOFLAG_PF;
		break;
	case t_ADCd:
		DOFLAG_AF;
		DOFLAG_ZFd;
		DOFLAG_SFd;
		DOFLAG_PF;
		break;


	case t_SBBb:
		DOFLAG_AF;
		DOFLAG_ZFb;
		DOFLAG_SFb;
		DOFLAG_PF;
		break;
	case t_SBBw:
		DOFLAG_AF;
		DOFLAG_ZFw;
		DOFLAG_SFw;
		DOFLAG_PF;
		break;
	case t_SBBd:
		DOFLAG_AF;
		DOFLAG_ZFd;
		DOFLAG_SFd;
		DOFLAG_PF;
		break;
	

	case t_SUBb:
	case t_CMPb:
		DOFLAG_AF;
		DOFLAG_ZFb;
		DOFLAG_SFb;
		DOFLAG_PF;
		break;
	case t_SUBw:
	case t_CMPw:
		DOFLAG_AF;
		DOFLAG_ZFw;
		DOFLAG_SFw;
		DOFLAG_PF;
		break;
	case t_SUBd:
	case t_CMPd:
		DOFLAG_AF;
		DOFLAG_ZFd;
		DOFLAG_SFd;
		DOFLAG_PF;
		break;
	
	case t_ORb:
	case t_TESTb:
	case t_ANDb:
	case t_XORb:
		SET_FLAG(AF,false);
		DOFLAG_ZFb;
		DOFLAG_SFb;
		DOFLAG_PF;
		break;

	case t_ORw:
	case t_TESTw:
	case t_ANDw:
	case t_XORw:
		SET_FLAG(AF,false);
		DOFLAG_ZFw;
		DOFLAG_SFw;
		DOFLAG_PF;
		break;

	case t_ORd:
	case t_TESTd:
	case t_ANDd:
	case t_XORd:
		SET_FLAG(AF,false);
		DOFLAG_ZFd;
		DOFLAG_SFd;
		DOFLAG_PF;
		break;
	
	case t_DSHLw:
	case t_DSHRw:	/* Hmm this is not correct for shift higher than 16 */
		DOFLAG_ZFw;
		DOFLAG_SFw;
		DOFLAG_PF;
		break;

	case t_DSHLd:
	case t_DSHRd:
		DOFLAG_ZFd;
		DOFLAG_SFd;
		DOFLAG_PF;
		break;

	case t_SHLb:
	case t_SHRb:
	case t_SARb:
		DOFLAG_ZFb;
		DOFLAG_SFb;
		DOFLAG_PF;
		SET_FLAG(AF,(lf_var2b&0x1f));
		break;

	case t_SHLw:
	case t_SHRw:
	case t_SARw:
		DOFLAG_ZFw;
		DOFLAG_SFw;
		DOFLAG_PF;
		SET_FLAG(AF,(lf_var2w&0x1f));
		break;

	case t_SHLd:
	case t_SHRd:
	case t_SARd:
		DOFLAG_ZFd;
		DOFLAG_SFd;
		DOFLAG_PF;
		SET_FLAG(AF,(lf_var2d&0x1f));
		break;

	case t_INCb:
		SET_FLAG(AF,(lf_resb & 0x0f) == 0);
		DOFLAG_ZFb;
		DOFLAG_SFb;
		DOFLAG_PF;
		break;
	case t_INCw:
		SET_FLAG(AF,(lf_resw & 0x0f) == 0);
		DOFLAG_ZFw;
		DOFLAG_SFw;
		DOFLAG_PF;
		break;
	case t_INCd:
		SET_FLAG(AF,(lf_resd & 0x0f) == 0);
		DOFLAG_ZFd;
		DOFLAG_SFd;
		DOFLAG_PF;
		break;

	case t_DECb:
		SET_FLAG(AF,(lf_resb & 0x0f) == 0x0f);
		DOFLAG_ZFb;
		DOFLAG_SFb;
		DOFLAG_PF;
		break;
	case t_DECw:
		SET_FLAG(AF,(lf_resw & 0x0f) == 0x0f);
		DOFLAG_ZFw;
		DOFLAG_SFw;
		DOFLAG_PF;
		break;
	case t_DECd:
		SET_FLAG(AF,(lf_resd & 0x0f) == 0x0f);
		DOFLAG_ZFd;
		DOFLAG_SFd;
		DOFLAG_PF;
		break;

	case t_NEGb:
		SET_FLAG(AF,(lf_resb & 0x0f) != 0);
		DOFLAG_ZFb;
		DOFLAG_SFb;
		DOFLAG_PF;
		break;
	case t_NEGw:
		SET_FLAG(AF,(lf_resw & 0x0f) != 0);
		DOFLAG_ZFw;
		DOFLAG_SFw;
		DOFLAG_PF;
		break;
	case t_NEGd:
		SET_FLAG(AF,(lf_resd & 0x0f) != 0);
		DOFLAG_ZFd;
		DOFLAG_SFd;
		DOFLAG_PF;
		break;

	
	case t_DIV:
	case t_MUL:
		break;

	default:
		LOG(LOG_CPU,LOG_ERROR)("Unhandled flag type %u",lflags.type);
		break;
	}
	lflags.type=t_UNKNOWN;
}

// Helper function to assess a value for the parity flag, which indicates
// whether the modulo 2 sum of the low-order eight bits of the result is even
// (PF=O) or odd (PF=1).
template <typename T>
constexpr bool lower_8_has_odd_parity(const T value)
{
	return parity_lookup[value & 0xff] != 0;
}

template <typename T>
constexpr bool is_negative(const T value)
{
	return std::is_signed_v<T> && value < 0;
}

// Set CPU test flags for all IDIV and DIV quotients
template <typename T>
void set_cpu_test_flags_for_division(const T quotient) noexcept
{
	// Reset the flag type being used
	lflags.type = {};

	// Create a new test flags object with the flags we'll be settings
	CpuTestFlags div_test_flags(FLAG_CF | FLAG_OF | FLAG_PF | FLAG_SF | FLAG_ZF);

	// After performing IDIV, Intel's micro-operation clears the carry and
	// overflow flags. Curiously, the 8086 documentation declares that the
	// status flags are undefined following IDIV, but the microcode
	// explicitly clears the carry and overflow flags. Signed DIV re-uses
	// the above IDIV routine, so this is true for both signed and unsigned
	// division.
	//
	//_https://www.righto.com/2023/04/reverse-engineering-8086-divide-microcode.html
	//
	div_test_flags.has_carry        = false;
	div_test_flags.has_overflow     = false;
	div_test_flags.has_odd_parity   = lower_8_has_odd_parity(quotient);
	div_test_flags.is_sign_negative = is_negative(quotient);
	div_test_flags.is_zero          = (quotient == 0);

	cpu_regs.ApplyTestFlags(div_test_flags);
}

// Explicit instantiations (replaces 18 instances)
template void set_cpu_test_flags_for_division<uint8_t>(const uint8_t) noexcept;
template void set_cpu_test_flags_for_division<uint16_t>(const uint16_t) noexcept;
template void set_cpu_test_flags_for_division<uint32_t>(const uint32_t) noexcept;

template void set_cpu_test_flags_for_division<int8_t>(const int8_t) noexcept;
template void set_cpu_test_flags_for_division<int16_t>(const int16_t) noexcept;
template void set_cpu_test_flags_for_division<int32_t>(const int32_t) noexcept;

void DestroyConditionFlags(void) {
	lflags.type=t_UNKNOWN;
}

#endif
