/*
 *  Copyright (C) 2002-2003  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
	Lazy flag system was designed after the same kind of system used in Bochs.
	Probably still some bugs left in here.
*/

#include "dosbox.h"
#include "cpu.h"
#include "lazyflags.h"
#include "pic.h"


/* CF     Carry Flag -- Set on high-order bit carry or borrow; cleared
          otherwise.
*/
Bitu get_CF(void) {

	switch (flags.type) {
	case t_UNKNOWN:
	case t_CF:
	case t_INCb:
	case t_INCw:
	case t_INCd:
	case t_DECb:
	case t_DECw:
	case t_DECd:
	case t_MUL:
	case t_RCLb:
	case t_RCLw:
	case t_RCLd:
		return GETFLAG(CF);
		break;
	case t_ADDb:	
		return (flags.result.b<flags.var1.b);
	case t_ADDw:	
		return (flags.result.w<flags.var1.w);
	case t_ADDd:
		return (flags.result.d<flags.var1.d);
	case t_ADCb:
		return (flags.result.b < flags.var1.b) || (flags.oldcf && (flags.result.b == flags.var1.b));
	case t_ADCw:
		return (flags.result.w < flags.var1.w) || (flags.oldcf && (flags.result.w == flags.var1.w));
	case t_ADCd:
		return (flags.result.d < flags.var1.d) || (flags.oldcf && (flags.result.d == flags.var1.d));
	case t_SBBb:
		return (flags.var1.b < flags.result.b) || (flags.oldcf && (flags.var2.b==0xff));
	case t_SBBw:
		return (flags.var1.w < flags.result.w) || (flags.oldcf && (flags.var2.w==0xffff));
	case t_SBBd:
		return (flags.var1.d < flags.result.d) || (flags.oldcf && (flags.var2.d==0xffffffff));
	case t_SUBb:
	case t_CMPb:
		return (flags.var1.b<flags.var2.b);
	case t_SUBw:
	case t_CMPw:
		return (flags.var1.w<flags.var2.w);
	case t_SUBd:
	case t_CMPd:
		return (flags.var1.d<flags.var2.d);
	case t_SHLb:
		if (flags.var2.b>8) return false;
		else return (flags.var1.b >> (8-flags.var2.b)) & 1;
	case t_SHLw:
		if (flags.var2.b>16) return false;
		else return (flags.var1.w >> (16-flags.var2.b)) & 1;
	case t_SHLd:
	case t_DSHLw:	/* Hmm this is not correct for shift higher than 16 */
	case t_DSHLd:
		return (flags.var1.d >> (32 - flags.var2.b)) & 1;
	case t_RCRb:
	case t_SHRb:
		return (flags.var1.b >> (flags.var2.b - 1)) & 1;
	case t_RCRw:
	case t_SHRw:
		return (flags.var1.w >> (flags.var2.b - 1)) & 1;
	case t_RCRd:
	case t_SHRd:
	case t_DSHRw:	/* Hmm this is not correct for shift higher than 16 */
	case t_DSHRd:
		return (flags.var1.d >> (flags.var2.b - 1)) & 1;
	case t_SARb:
		return (((Bit8s) flags.var1.b) >> (flags.var2.b - 1)) & 1;
	case t_SARw:
		return (((Bit16s) flags.var1.w) >> (flags.var2.b - 1)) & 1;
	case t_SARd:
		return (((Bit32s) flags.var1.d) >> (flags.var2.b - 1)) & 1;
	case t_NEGb:
		return (flags.var1.b!=0);
	case t_NEGw:
		return (flags.var1.w!=0);
	case t_NEGd:
		return (flags.var1.d!=0);
	case t_ROLb:
		return flags.result.b & 1;
	case t_ROLw:
		return flags.result.w & 1;
	case t_ROLd:
		return flags.result.d & 1;
	case t_RORb:
		return (flags.result.b & 0x80)>0;
	case t_RORw:
		return (flags.result.w & 0x8000)>0;
	case t_RORd:
		return (flags.result.d & 0x80000000)>0;
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
		return false;	/* Unkown */
	default:
		LOG(LOG_CPU,LOG_ERROR)("get_CF Unknown %d",flags.type);
	}
	return 0;
}

/* AF     Adjust flag -- Set on carry from or borrow to the low order
            four bits of   AL; cleared otherwise. Used for decimal
            arithmetic.
*/
Bitu get_AF(void) {
	Bitu type=flags.type;
again:	
	switch (type) {
	case t_UNKNOWN:
	case t_ROLb:
	case t_RORb:
	case t_RCLb:
	case t_RCRb:
	case t_ROLw:
	case t_RORw:
	case t_RCLw:
	case t_RCRw:
	case t_ROLd:
	case t_RORd:
	case t_RCLd:
	case t_RCRd:
		return GETFLAG(AF);
	case t_CF:
		type=flags.prev_type;
		goto again;
	case t_ADDb:	
	case t_ADCb:
	case t_SBBb:
	case t_SUBb:
	case t_CMPb:
		return (((flags.var1.b ^ flags.var2.b) ^ flags.result.b) & 0x10)>0;
	case t_ADDw:
	case t_ADCw:
	case t_SBBw:
	case t_SUBw:
	case t_CMPw:
		return (((flags.var1.w ^ flags.var2.w) ^ flags.result.w) & 0x10)>0;
	case t_ADCd:
	case t_ADDd:
	case t_SBBd:
	case t_SUBd:
	case t_CMPd:
		return (((flags.var1.d ^ flags.var2.d) ^ flags.result.d) & 0x10)>0;
	case t_INCb:
		return (flags.result.b & 0x0f) == 0;
	case t_INCw:
		return (flags.result.w & 0x0f) == 0;
	case t_INCd:
		return (flags.result.d & 0x0f) == 0;
	case t_DECb:
		return (flags.result.b & 0x0f) == 0x0f;
	case t_DECw:
		return (flags.result.w & 0x0f) == 0x0f;
	case t_DECd:
		return (flags.result.d & 0x0f) == 0x0f;
	case t_NEGb:
		return (flags.var1.b & 0x0f) > 0;
	case t_NEGw:
		return (flags.var1.w & 0x0f) > 0;
	case t_NEGd:
		return (flags.var1.d & 0x0f) > 0;
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
	case t_SHLb:
	case t_SHLw:
	case t_SHLd:
	case t_SHRb:
	case t_SHRw:
	case t_SHRd:
	case t_SARb:
	case t_SARw:
	case t_SARd:
	case t_DSHLw:
	case t_DSHLd:
	case t_DSHRw:
	case t_DSHRd:
	case t_DIV:
	case t_MUL:
		return false;			          /* Unkown */
	default:
		LOG(LOG_CPU,LOG_ERROR)("get_AF Unknown %d",flags.type);
	}
	return 0;
}

/* ZF     Zero Flag -- Set if result is zero; cleared otherwise.
*/

Bitu get_ZF(void) {
	Bitu type=flags.type;
again:	
	switch (type) {
	case t_UNKNOWN:
	case t_ROLb:
	case t_RORb:
	case t_RCLb:
	case t_RCRb:
	case t_ROLw:
	case t_RORw:
	case t_RCLw:
	case t_RCRw:
	case t_ROLd:
	case t_RORd:
	case t_RCLd:
	case t_RCRd:
		return GETFLAG(ZF);
	case t_CF:
		type=flags.prev_type;
		goto again;
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
		return (flags.result.b==0);
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
		return (flags.result.w==0);
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
		return (flags.result.d==0);
	case t_DIV:
	case t_MUL:
		return false;		/* Unkown */
	default:
		LOG(LOG_CPU,LOG_ERROR)("get_ZF Unknown %d",flags.type);
	}
	return false;
}
/* SF     Sign Flag -- Set equal to high-order bit of result (0 is
            positive, 1 if negative).
*/
Bitu get_SF(void) {
	Bitu type=flags.type;
again:	
	switch (type) {
	case t_UNKNOWN:
	case t_ROLb:
	case t_RORb:
	case t_RCLb:
	case t_RCRb:
	case t_ROLw:
	case t_RORw:
	case t_RCLw:
	case t_RCRw:
	case t_ROLd:
	case t_RORd:
	case t_RCLd:
	case t_RCRd:
		return GETFLAG(SF);
	case t_CF:
		type=flags.prev_type;
		goto again;
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
		return	(flags.result.b>=0x80);
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
		return	(flags.result.w>=0x8000);
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
		return	(flags.result.d>=0x80000000);
	case t_DIV:
	case t_MUL:
		return false;	/* Unkown */
	default:
		LOG(LOG_CPU,LOG_ERROR)("get_SF Unkown %d",flags.type);
	}
	return false;

}
Bitu get_OF(void) {
  Bit8u		var1b7, var2b7, resultb7;
  Bit16u	var1w15, var2w15, resultw15;
  Bit32u	var1d31, var2d31, resultd31;
  
	Bitu type=flags.type;
again:	
	switch (type) {
	case t_UNKNOWN:
	case t_MUL:
	case t_RCLb:
	case t_RCLw:
	case t_RCLd:
	case t_SARb:
	case t_SARw:
	case t_SARd:
		return GETFLAG(OF);
	case t_CF:
		type=flags.prev_type;
		goto again;
	case t_ADDb:
	case t_ADCb:
//		return (((flags.result.b) ^ (flags.var2.b)) & ((flags.result.b) ^ (flags.var1.b)) & 0x80)>0;
		var1b7 = flags.var1.b & 0x80;
		var2b7 = flags.var2.b & 0x80;
		resultb7 = flags.result.b & 0x80;
		return (var1b7 == var2b7) && (resultb7 ^ var2b7);
	case t_ADDw:
	case t_ADCw:
//		return (((flags.result.w) ^ (flags.var2.w)) & ((flags.result.w) ^ (flags.var1.w)) & 0x8000)>0;
		var1w15 = flags.var1.w & 0x8000;
		var2w15 = flags.var2.w & 0x8000;
		resultw15 = flags.result.w & 0x8000;
		return (var1w15 == var2w15) && (resultw15 ^ var2w15);
	case t_ADDd:
	case t_ADCd:
//TODO fix dword Overflow
		var1d31 = flags.var1.d & 0x80000000;
		var2d31 = flags.var2.d & 0x80000000;
		resultd31 = flags.result.d & 0x80000000;
		return (var1d31 == var2d31) && (resultd31 ^ var2d31);	
	case t_SBBb:
	case t_SUBb:
	case t_CMPb:
//		return (((flags.var1.b) ^ (flags.var2.b)) & ((flags.var1.b) ^ (flags.result.b)) & 0x80)>0;
		var1b7 = flags.var1.b & 0x80;
		var2b7 = flags.var2.b & 0x80;
		resultb7 = flags.result.b & 0x80;
		return (var1b7 ^ var2b7) && (var1b7 ^ resultb7);
	case t_SBBw:
	case t_SUBw:
	case t_CMPw:
//		return (((flags.var1.w) ^ (flags.var2.w)) & ((flags.var1.w) ^ (flags.result.w)) & 0x8000)>0;
		var1w15 = flags.var1.w & 0x8000;
		var2w15 = flags.var2.w & 0x8000;
		resultw15 = flags.result.w & 0x8000;
		return (var1w15 ^ var2w15) && (var1w15 ^ resultw15);
	case t_SBBd:
	case t_SUBd:
	case t_CMPd:
		var1d31 = flags.var1.d & 0x80000000;
		var2d31 = flags.var2.d & 0x80000000;
		resultd31 = flags.result.d & 0x80000000;
		return (var1d31 ^ var2d31) && (var1d31 ^ resultd31);
	case t_INCb:
		return (flags.result.b == 0x80);
	case t_INCw:
		return (flags.result.w == 0x8000);
	case t_INCd:
		return (flags.result.d == 0x80000000);
	case t_DECb:
		return (flags.result.b == 0x7f);
	case t_DECw:
		return (flags.result.w == 0x7fff);
	case t_DECd:
		return (flags.result.d == 0x7fffffff);
	case t_NEGb:
		return (flags.var1.b == 0x80);
	case t_NEGw:
		return (flags.var1.w == 0x8000);
	case t_NEGd:
		return (flags.var1.d == 0x80000000);
	case t_ROLb:
		return ((flags.result.b & 0x80) ^ (flags.result.b & 1 ? 0x80 : 0)) != 0;
	case t_ROLw:
		return ((flags.result.w & 0x8000) ^ (flags.result.w & 1 ? 0x8000 : 0)) != 0;
	case t_ROLd:
		return ((flags.result.d & 0x80000000) ^ (flags.result.d & 1 ? 0x80000000 : 0)) != 0;
	case t_SHLb:
		if (flags.var2.b>9) return false;
		return ((flags.result.b & 0x80) ^
			((flags.var1.b << (flags.var2.b - 1)) & 0x80)) != 0;
	case t_SHLw:
		if (flags.var2.b>17) return false;
		return ((flags.result.w & 0x8000) ^
			((flags.var1.w << (flags.var2.b - 1)) & 0x8000)) != 0;
	case t_DSHLw:	/* Hmm this is not correct for shift higher than 16 */
		return ((flags.result.w & 0x8000) ^
			(((flags.var1.d << (flags.var2.b - 1)) >> 16) & 0x8000)) != 0;
	case t_SHLd:
	case t_DSHLd:
		return ((flags.result.d & 0x80000000) ^
			((flags.var1.d << (flags.var2.b - 1)) & 0x80000000)) != 0;
	case t_RORb:
	case t_RCRb:
		return ((flags.result.b ^ (flags.result.b << 1)) & 0x80) > 0;
	case t_RORw:
	case t_RCRw:
	case t_DSHRw:
		return ((flags.result.w ^ (flags.result.w << 1)) & 0x8000) > 0;
	case t_RORd:
	case t_RCRd:
	case t_DSHRd:
		return ((flags.result.d ^ (flags.result.d << 1)) & 0x80000000) > 0;
	case t_SHRb:
		return (flags.result.b >= 0x40);
	case t_SHRw:
		return (flags.result.w >= 0x4000);
	case t_SHRd:
		return (flags.result.d >= 0x40000000);
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
	return false;			/* Return false */
	case t_DIV:
		return false;		/* Unkown */
	default:
		LOG(LOG_CPU,LOG_ERROR)("get_OF Unkown %d",flags.type);
	}
	return false;
}

bool parity_lookup[256] = {
  1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
  0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
  0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
  1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
  0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
  1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
  1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
  0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
  0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
  1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
  1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
  0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
  1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
  0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
  0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
  1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1
  };

Bitu get_PF(void) {
	switch (flags.type) {
		case t_UNKNOWN:
		return GETFLAG(PF);
	default:
		return	(parity_lookup[flags.result.b]);;
	};
	return false;
}


#define SET_FLAG(TYPE,TEST)	if ( TEST ) new_flags|=TYPE
#if 0
#endif

Bitu get_Flags(void) {
	Bitu new_flags=0;
	switch (flags.type) {
	case t_ADDb:	
		SET_FLAG(FLAG_CF,(flags.result.b<flags.var1.b));
		SET_FLAG(FLAG_AF,(((flags.var1.b ^ flags.var2.b) ^ flags.result.b) & 0x10)>0);
		break;
	case t_ADDw:	
		SET_FLAG(FLAG_CF,(flags.result.w<flags.var1.w));
		SET_FLAG(FLAG_AF,(((flags.var1.w ^ flags.var2.w) ^ flags.result.w) & 0x10)>0);
		break;
	case t_ADDd:
		SET_FLAG(FLAG_CF,(flags.result.d<flags.var1.d));
		SET_FLAG(FLAG_AF,(((flags.var1.d ^ flags.var2.d) ^ flags.result.d) & 0x10)>0);
		break;


	case t_ADCb:
		SET_FLAG(FLAG_CF,(flags.result.b < flags.var1.b) || (flags.oldcf && (flags.result.b == flags.var1.b)));
		SET_FLAG(FLAG_AF,(((flags.var1.b ^ flags.var2.b) ^ flags.result.b) & 0x10)>0);
		break;
	case t_ADCw:
		SET_FLAG(FLAG_CF,(flags.result.w < flags.var1.w) || (flags.oldcf && (flags.result.w == flags.var1.w)));
		SET_FLAG(FLAG_AF,(((flags.var1.w ^ flags.var2.w) ^ flags.result.w) & 0x10)>0);
		break;
	case t_ADCd:
		SET_FLAG(FLAG_CF,(flags.result.d < flags.var1.d) || (flags.oldcf && (flags.result.d == flags.var1.d)));
		SET_FLAG(FLAG_AF,(((flags.var1.d ^ flags.var2.d) ^ flags.result.d) & 0x10)>0);
		break;

	
	case t_SBBb:
		SET_FLAG(FLAG_CF,(flags.var1.b < flags.result.b) || (flags.oldcf && (flags.var2.b==0xff)));
		SET_FLAG(FLAG_AF,(((flags.var1.b ^ flags.var2.b) ^ flags.result.b) & 0x10)>0);
		break;
	case t_SBBw:
		SET_FLAG(FLAG_CF,(flags.var1.w < flags.result.w) || (flags.oldcf && (flags.var2.w==0xffff)));
		SET_FLAG(FLAG_AF,(((flags.var1.w ^ flags.var2.w) ^ flags.result.w) & 0x10)>0);
		break;
	case t_SBBd:
		SET_FLAG(FLAG_CF,(flags.var1.d < flags.result.d) || (flags.oldcf && (flags.var2.d==0xffffffff)));
		SET_FLAG(FLAG_AF,(((flags.var1.d ^ flags.var2.d) ^ flags.result.d) & 0x10)>0);
		break;
	

	case t_SUBb:
	case t_CMPb:
		SET_FLAG(FLAG_CF,(flags.var1.b<flags.var2.b));
		SET_FLAG(FLAG_AF,(((flags.var1.b ^ flags.var2.b) ^ flags.result.b) & 0x10)>0);
		break;
	case t_SUBw:
	case t_CMPw:
		SET_FLAG(FLAG_CF,(flags.var1.w<flags.var2.w));
		SET_FLAG(FLAG_AF,(((flags.var1.w ^ flags.var2.w) ^ flags.result.w) & 0x10)>0);
		break;
	case t_SUBd:
	case t_CMPd:
		SET_FLAG(FLAG_CF,(flags.var1.d<flags.var2.d));
		SET_FLAG(FLAG_AF,(((flags.var1.d ^ flags.var2.d) ^ flags.result.d) & 0x10)>0);
		break;

	
	case t_ORb:
		SET_FLAG(FLAG_CF,false);
		break;
	case t_ORw:
		SET_FLAG(FLAG_CF,false);
		break;
	case t_ORd:
		SET_FLAG(FLAG_CF,false);
		break;
	
	
	case t_TESTb:
	case t_ANDb:
		SET_FLAG(FLAG_CF,false);
		break;
	case t_TESTw:
	case t_ANDw:
		SET_FLAG(FLAG_CF,false);
		break;
	case t_TESTd:
	case t_ANDd:
		SET_FLAG(FLAG_CF,false);
		break;

	
	case t_XORb:
		SET_FLAG(FLAG_CF,false);
		break;
	case t_XORw:
		SET_FLAG(FLAG_CF,false);
		break;
	case t_XORd:
		SET_FLAG(FLAG_CF,false);
		break;

		
	case t_SHLb:
		if (flags.var2.b>8) SET_FLAG(FLAG_CF,false);
		else SET_FLAG(FLAG_CF,(flags.var1.b >> (8-flags.var2.b)) & 1);
		break;
	case t_SHLw:
		if (flags.var2.b>16) SET_FLAG(FLAG_CF,false);
		else SET_FLAG(FLAG_CF,(flags.var1.w >> (16-flags.var2.b)) & 1);
		break;
	case t_SHLd:
		SET_FLAG(FLAG_CF,(flags.var1.d >> (32 - flags.var2.b)) & 1);
		break;


	case t_DSHLw:	/* Hmm this is not correct for shift higher than 16 */
		SET_FLAG(FLAG_CF,(flags.var1.d >> (32 - flags.var2.b)) & 1);
		break;
	case t_DSHLd:
		SET_FLAG(FLAG_CF,(flags.var1.d >> (32 - flags.var2.b)) & 1);
		break;


	case t_SHRb:
		SET_FLAG(FLAG_CF,(flags.var1.b >> (flags.var2.b - 1)) & 1);
		break;
	case t_SHRw:
		SET_FLAG(FLAG_CF,(flags.var1.w >> (flags.var2.b - 1)) & 1);
		break;
	case t_SHRd:
		SET_FLAG(FLAG_CF,(flags.var1.d >> (flags.var2.b - 1)) & 1);
		break;

	
	case t_DSHRw:	/* Hmm this is not correct for shift higher than 16 */
		SET_FLAG(FLAG_CF,(flags.var1.d >> (flags.var2.b - 1)) & 1);
		break;
	case t_DSHRd:
		SET_FLAG(FLAG_CF,(flags.var1.d >> (flags.var2.b - 1)) & 1);
		break;


	case t_SARb:
		SET_FLAG(FLAG_CF,(((Bit8s) flags.var1.b) >> (flags.var2.b - 1)) & 1);
		break;
	case t_SARw:
		SET_FLAG(FLAG_CF,(((Bit16s) flags.var1.w) >> (flags.var2.b - 1)) & 1);
		break;
	case t_SARd:
		SET_FLAG(FLAG_CF,(((Bit32s) flags.var1.d) >> (flags.var2.b - 1)) & 1);
		break;


	case t_ROLb:
		SET_FLAG(FLAG_CF,flags.result.b & 1);
		break;
	case t_ROLw:
		SET_FLAG(FLAG_CF,flags.result.w & 1);
		break;
	case t_ROLd:
		SET_FLAG(FLAG_CF,flags.result.d & 1);
		break;


	case t_RORb:
		SET_FLAG(FLAG_CF,(flags.result.b & 0x80)>0);
		break;
	case t_RORw:
		SET_FLAG(FLAG_CF,(flags.result.w & 0x8000)>0);
		break;
	case t_RORd:
		SET_FLAG(FLAG_CF,(flags.result.d & 0x80000000)>0);
		break;

	
	case t_RCRb:
		SET_FLAG(FLAG_CF,(flags.var1.b >> (flags.var2.b - 1)) & 1);
		break;
	case t_RCRw:
		SET_FLAG(FLAG_CF,(flags.var1.w >> (flags.var2.b - 1)) & 1);
		break;
	case t_RCRd:
		SET_FLAG(FLAG_CF,(flags.var1.d >> (flags.var2.b - 1)) & 1);
		break;


	case t_INCb:
		SET_FLAG(FLAG_OF,(flags.result.b == 0x80));
		SET_FLAG(FLAG_AF,((flags.result.b & 0x0f) == 0));
		break;
	case t_INCw:
		SET_FLAG(FLAG_OF,(flags.result.w == 0x8000));
		SET_FLAG(FLAG_AF,((flags.result.w & 0x0f) == 0));
		break;
	case t_INCd:
		SET_FLAG(FLAG_OF,(flags.result.d == 0x80000000));
		SET_FLAG(FLAG_AF,((flags.result.d & 0x0f) == 0));
		break;


	case t_DECb:
		SET_FLAG(FLAG_OF,(flags.result.b == 0x7f));
		break;
	case t_DECw:
		SET_FLAG(FLAG_OF,(flags.result.w == 0x7fff));
		break;
	case t_DECd:
		SET_FLAG(FLAG_OF,(flags.result.d == 0x7fffffff));
		break;


	case t_NEGb:
		SET_FLAG(FLAG_CF,(flags.var1.b!=0));
		break;
	case t_NEGw:
		SET_FLAG(FLAG_CF,(flags.var1.w!=0));
		break;
	case t_NEGd:
		SET_FLAG(FLAG_CF,(flags.var1.d!=0));
		break;

	
	case t_DIV:
		SET_FLAG(FLAG_CF,false);	/* Unkown */
		break;
	default:
		LOG(LOG_CPU,LOG_ERROR)("Unhandled flag type %d",flags.type);
		return 0;
	}
	flags.word=new_flags;
	return 0;
}

Bit8u * blah=(Bit8u *)&get_Flags;
