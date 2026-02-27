// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_LAZYFLAGS_H
#define DOSBOX_LAZYFLAGS_H

#include "cpu/cpu.h"

#include <cassert>

// Flag Handling
uint32_t get_CF();
uint32_t get_AF();
uint32_t get_ZF();
uint32_t get_SF();
uint32_t get_OF();
uint32_t get_PF();

uint32_t FillFlags(void);
void FillFlagsNoCFOF(void);
void DestroyConditionFlags(void);

#ifndef DOSBOX_REGS_H
#include "cpu/registers.h"
#endif

struct LazyFlags {
	GenReg32 var1 = {};
	GenReg32 var2 = {};
	GenReg32 res = {};
	uint_fast8_t type  = 0;
	uint_fast8_t oldcf = 0;
};

extern LazyFlags lflags;

inline constexpr uint8_t &lf_var1b = lflags.var1.byte[BL_INDEX];
inline constexpr uint8_t &lf_var2b = lflags.var2.byte[BL_INDEX];
inline constexpr uint8_t &lf_resb = lflags.res.byte[BL_INDEX];

inline constexpr uint16_t &lf_var1w = lflags.var1.word[W_INDEX];
inline constexpr uint16_t &lf_var2w = lflags.var2.word[W_INDEX];
inline constexpr uint16_t &lf_resw = lflags.res.word[W_INDEX];

inline constexpr uint32_t &lf_var1d = lflags.var1.dword[DW_INDEX];
inline constexpr uint32_t &lf_var2d = lflags.var2.dword[DW_INDEX];
inline constexpr uint32_t &lf_resd = lflags.res.dword[DW_INDEX];

// many places in the code want to shift by "lf_var2b - 1", so this wrapper
// catches potential negative shifts.
inline uint8_t lf_var2b_minus_one()
{
	assert(lf_var2b > 0);
	return lf_var2b - 1;
}

// Types of Flag changing instructions
enum {
	t_UNKNOWN=0,
	t_ADDb,t_ADDw,t_ADDd, 
	t_ORb,t_ORw,t_ORd, 
	t_ADCb,t_ADCw,t_ADCd,
	t_SBBb,t_SBBw,t_SBBd,
	t_ANDb,t_ANDw,t_ANDd,
	t_SUBb,t_SUBw,t_SUBd,
	t_XORb,t_XORw,t_XORd,
	t_CMPb,t_CMPw,t_CMPd,
	t_INCb,t_INCw,t_INCd,
	t_DECb,t_DECw,t_DECd,
	t_TESTb,t_TESTw,t_TESTd,
	t_SHLb,t_SHLw,t_SHLd,
	t_SHRb,t_SHRw,t_SHRd,
	t_SARb,t_SARw,t_SARd,
	t_ROLb,t_ROLw,t_ROLd,
	t_RORb,t_RORw,t_RORd,
	t_RCLb,t_RCLw,t_RCLd,
	t_RCRb,t_RCRw,t_RCRd,
	t_NEGb,t_NEGw,t_NEGd,
	
	t_DSHLw,t_DSHLd,
	t_DSHRw,t_DSHRd,
	t_MUL,t_DIV,
	t_NOTDONE,
	t_LASTFLAG
};

inline void SETFLAGSb(const uint8_t FLAGB)
{
	SETFLAGBIT(OF, get_OF());
	lflags.type = t_UNKNOWN;
	CPU_SetFlags(FLAGB, FMASK_NORMAL & 0xff);
}

inline void SETFLAGSw(const uint16_t FLAGW)
{
	lflags.type = t_UNKNOWN;
	CPU_SetFlagsw(FLAGW);
}

inline void SETFLAGSd(const uint32_t FLAGD)
{
	lflags.type = t_UNKNOWN;
	CPU_SetFlagsd(FLAGD);
}

#define LoadCF SETFLAGBIT(CF, get_CF());
#define LoadZF SETFLAGBIT(ZF, get_ZF());
#define LoadSF SETFLAGBIT(SF, get_SF());
#define LoadOF SETFLAGBIT(OF, get_OF());
#define LoadAF SETFLAGBIT(AF, get_AF());

#define TFLG_O   (get_OF())
#define TFLG_NO  (!get_OF())
#define TFLG_B   (get_CF())
#define TFLG_NB  (!get_CF())
#define TFLG_Z   (get_ZF())
#define TFLG_NZ  (!get_ZF())
#define TFLG_BE  (get_CF() || get_ZF())
#define TFLG_NBE (!get_CF() && !get_ZF())
#define TFLG_S   (get_SF())
#define TFLG_NS  (!get_SF())
#define TFLG_P   (get_PF())
#define TFLG_NP  (!get_PF())
#define TFLG_L   ((get_SF() != 0) != (get_OF() != 0))
#define TFLG_NL  ((get_SF() != 0) == (get_OF() != 0))
#define TFLG_LE  (get_ZF() || ((get_SF() != 0) != (get_OF() != 0)))
#define TFLG_NLE (!get_ZF() && ((get_SF() != 0) == (get_OF() != 0)))

// -----------------------------------------------------------------------
// Inline definitions for lazy flag evaluation functions.
//
// These were previously in flags.cpp (a separate translation unit), which
// prevented the compiler from inlining them into callers in other TUs
// such as the dynrec's operators.h (dynrec_get_sf, dynrec_get_nsf, etc.).
//
// On PPC64LE the double function-call overhead (JIT -> dynrec_get_nsf ->
// get_SF) was a significant hotspot (~14% CPU). Making these inline
// eliminates one entire call layer.
// -----------------------------------------------------------------------

// Parity lookup table: FLAG_PF for even parity, 0 for odd parity.
// Used by get_PF() and FillFlags/FillFlagsNoCFOF.
inline uint16_t parity_lookup[256] = {
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

/* CF     Carry Flag -- Set on high-order bit carry or borrow; cleared
          otherwise.
*/
inline uint32_t get_CF(void) {
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
inline uint32_t get_AF(void) {
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
inline uint32_t get_ZF(void) {
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
inline uint32_t get_SF(void) {
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

inline uint32_t get_OF(void) {
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

inline uint32_t get_PF(void) {
	switch (lflags.type) {
	case t_UNKNOWN:
		return GETFLAG(PF);
	default:
		return	(parity_lookup[lf_resb]);
	};
	return 0;
}

#endif
