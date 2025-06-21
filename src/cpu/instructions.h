// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "flags.h"
#include "lazyflags.h"

/* Jumps */

/* All Byte general instructions */
#define ADDB(op1,op2,load,save)								\
	lf_var1b=load(op1);lf_var2b=op2;					\
	lf_resb=lf_var1b+lf_var2b;					\
	save(op1,lf_resb);								\
	lflags.type=t_ADDb;

#define ADCB(op1,op2,load,save)								\
	lflags.oldcf=get_CF()!=0;								\
	lf_var1b=load(op1);lf_var2b=op2;					\
	lf_resb=lf_var1b+lf_var2b+lflags.oldcf;		\
	save(op1,lf_resb);								\
	lflags.type=t_ADCb;

#define SBBB(op1,op2,load,save)								\
	lflags.oldcf=get_CF()!=0;									\
	lf_var1b=load(op1);lf_var2b=op2;					\
	lf_resb=lf_var1b-(lf_var2b+lflags.oldcf);	\
	save(op1,lf_resb);								\
	lflags.type=t_SBBb;

#define SUBB(op1,op2,load,save)								\
	lf_var1b=load(op1);lf_var2b=op2;					\
	lf_resb=lf_var1b-lf_var2b;					\
	save(op1,lf_resb);								\
	lflags.type=t_SUBb;

#define ORB(op1,op2,load,save)								\
	lf_var1b=load(op1);lf_var2b=op2;					\
	lf_resb=lf_var1b | lf_var2b;				\
	save(op1,lf_resb);								\
	lflags.type=t_ORb;

#define XORB(op1,op2,load,save)								\
	lf_var1b=load(op1);lf_var2b=op2;					\
	lf_resb=lf_var1b ^ lf_var2b;				\
	save(op1,lf_resb);								\
	lflags.type=t_XORb;

#define ANDB(op1,op2,load,save)								\
	lf_var1b=load(op1);lf_var2b=op2;					\
	lf_resb=lf_var1b & lf_var2b;				\
	save(op1,lf_resb);								\
	lflags.type=t_ANDb;

#define CMPB(op1,op2,load,save)								\
	lf_var1b=load(op1);lf_var2b=op2;					\
	lf_resb=lf_var1b-lf_var2b;					\
	lflags.type=t_CMPb;

#define TESTB(op1,op2,load,save)							\
	lf_var1b=load(op1);lf_var2b=op2;					\
	lf_resb=lf_var1b & lf_var2b;				\
	lflags.type=t_TESTb;

/* All Word General instructions */

#define ADDW(op1,op2,load,save)								\
	lf_var1w=load(op1);lf_var2w=op2;					\
	lf_resw=lf_var1w+lf_var2w;					\
	save(op1,lf_resw);								\
	lflags.type=t_ADDw;

#define ADCW(op1,op2,load,save)								\
	lflags.oldcf=get_CF()!=0;									\
	lf_var1w=load(op1);lf_var2w=op2;					\
	lf_resw=lf_var1w+lf_var2w+lflags.oldcf;		\
	save(op1,lf_resw);								\
	lflags.type=t_ADCw;

#define SBBW(op1,op2,load,save)								\
	lflags.oldcf=get_CF()!=0;									\
	lf_var1w=load(op1);lf_var2w=op2;					\
	lf_resw=lf_var1w-(lf_var2w+lflags.oldcf);	\
	save(op1,lf_resw);								\
	lflags.type=t_SBBw;

#define SUBW(op1,op2,load,save)								\
	lf_var1w=load(op1);lf_var2w=op2;					\
	lf_resw=lf_var1w-lf_var2w;					\
	save(op1,lf_resw);								\
	lflags.type=t_SUBw;

#define ORW(op1,op2,load,save)								\
	lf_var1w=load(op1);lf_var2w=op2;					\
	lf_resw=lf_var1w | lf_var2w;				\
	save(op1,lf_resw);								\
	lflags.type=t_ORw;

#define XORW(op1,op2,load,save)								\
	lf_var1w=load(op1);lf_var2w=op2;					\
	lf_resw=lf_var1w ^ lf_var2w;				\
	save(op1,lf_resw);								\
	lflags.type=t_XORw;

#define ANDW(op1,op2,load,save)								\
	lf_var1w=load(op1);lf_var2w=op2;					\
	lf_resw=lf_var1w & lf_var2w;				\
	save(op1,lf_resw);								\
	lflags.type=t_ANDw;

#define CMPW(op1,op2,load,save)								\
	lf_var1w=load(op1);lf_var2w=op2;					\
	lf_resw=lf_var1w-lf_var2w;					\
	lflags.type=t_CMPw;

#define TESTW(op1,op2,load,save)							\
	lf_var1w=load(op1);lf_var2w=op2;					\
	lf_resw=lf_var1w & lf_var2w;				\
	lflags.type=t_TESTw;

/* All DWORD General Instructions */

#define ADDD(op1,op2,load,save)								\
	lf_var1d=load(op1);lf_var2d=op2;					\
	lf_resd=lf_var1d+lf_var2d;					\
	save(op1,lf_resd);								\
	lflags.type=t_ADDd;

#define ADCD(op1,op2,load,save)								\
	lflags.oldcf=get_CF()!=0;									\
	lf_var1d=load(op1);lf_var2d=op2;					\
	lf_resd=lf_var1d+lf_var2d+lflags.oldcf;		\
	save(op1,lf_resd);								\
	lflags.type=t_ADCd;

#define SBBD(op1,op2,load,save)								\
	lflags.oldcf=get_CF()!=0;									\
	lf_var1d=load(op1);lf_var2d=op2;					\
	lf_resd=lf_var1d-(lf_var2d+lflags.oldcf);	\
	save(op1,lf_resd);								\
	lflags.type=t_SBBd;

#define SUBD(op1,op2,load,save)								\
	lf_var1d=load(op1);lf_var2d=op2;					\
	lf_resd=lf_var1d-lf_var2d;					\
	save(op1,lf_resd);								\
	lflags.type=t_SUBd;

#define ORD(op1,op2,load,save)								\
	lf_var1d=load(op1);lf_var2d=op2;					\
	lf_resd=lf_var1d | lf_var2d;				\
	save(op1,lf_resd);								\
	lflags.type=t_ORd;

#define XORD(op1,op2,load,save)								\
	lf_var1d=load(op1);lf_var2d=op2;					\
	lf_resd=lf_var1d ^ lf_var2d;				\
	save(op1,lf_resd);								\
	lflags.type=t_XORd;

#define ANDD(op1,op2,load,save)								\
	lf_var1d=load(op1);lf_var2d=op2;					\
	lf_resd=lf_var1d & lf_var2d;				\
	save(op1,lf_resd);								\
	lflags.type=t_ANDd;

#define CMPD(op1,op2,load,save)								\
	lf_var1d=load(op1);lf_var2d=op2;					\
	lf_resd=lf_var1d-lf_var2d;					\
	lflags.type=t_CMPd;


#define TESTD(op1,op2,load,save)							\
	lf_var1d=load(op1);lf_var2d=op2;					\
	lf_resd=lf_var1d & lf_var2d;				\
	lflags.type=t_TESTd;




#define INCB(op1,load,save)									\
	LoadCF;lf_var1b=load(op1);								\
	lf_resb=lf_var1b+1;										\
	save(op1,lf_resb);										\
	lflags.type=t_INCb;										\

#define INCW(op1,load,save)									\
	LoadCF;lf_var1w=load(op1);								\
	lf_resw=lf_var1w+1;										\
	save(op1,lf_resw);										\
	lflags.type=t_INCw;

#define INCD(op1,load,save)									\
	LoadCF;lf_var1d=load(op1);								\
	lf_resd=lf_var1d+1;										\
	save(op1,lf_resd);										\
	lflags.type=t_INCd;

#define DECB(op1,load,save)									\
	LoadCF;lf_var1b=load(op1);								\
	lf_resb=lf_var1b-1;										\
	save(op1,lf_resb);										\
	lflags.type=t_DECb;

#define DECW(op1,load,save)									\
	LoadCF;lf_var1w=load(op1);								\
	lf_resw=lf_var1w-1;										\
	save(op1,lf_resw);										\
	lflags.type=t_DECw;

#define DECD(op1,load,save)									\
	LoadCF;lf_var1d=load(op1);								\
	lf_resd=lf_var1d-1;										\
	save(op1,lf_resd);										\
	lflags.type=t_DECd;



#define ROLB(op1,op2,load,save)						\
	FillFlagsNoCFOF();								\
	lf_var1b=load(op1);								\
	lf_var2b=op2&0x07;								\
	lf_resb=(lf_var1b << lf_var2b) |				\
			(lf_var1b >> (8-lf_var2b));				\
	save(op1,lf_resb);								\
	SETFLAGBIT(CF,lf_resb & 1);						\
	SETFLAGBIT(OF,(lf_resb & 1) ^ (lf_resb >> 7));

#define ROLW(op1,op2,load,save)						\
	FillFlagsNoCFOF();								\
	lf_var1w=load(op1);								\
	lf_var2b=op2&0xf;								\
	lf_resw=(lf_var1w << lf_var2b) |				\
			(lf_var1w >> (16-lf_var2b));			\
	save(op1,lf_resw);								\
	SETFLAGBIT(CF,lf_resw & 1);						\
	SETFLAGBIT(OF,(lf_resw & 1) ^ (lf_resw >> 15));

#define ROLD(op1,op2,load,save)						\
	FillFlagsNoCFOF();								\
	lf_var1d=load(op1);								\
	lf_var2b=op2;									\
	lf_resd=(lf_var1d << lf_var2b) |				\
			(lf_var1d >> (32-lf_var2b));			\
	save(op1,lf_resd);								\
	SETFLAGBIT(CF,lf_resd & 1);						\
	SETFLAGBIT(OF,(lf_resd & 1) ^ (lf_resd >> 31));


#define RORB(op1,op2,load,save)						\
	FillFlagsNoCFOF();								\
	lf_var1b=load(op1);								\
	lf_var2b=op2&0x07;								\
	lf_resb=(lf_var1b >> lf_var2b) |				\
			(lf_var1b << (8-lf_var2b));				\
	save(op1,lf_resb);								\
	SETFLAGBIT(CF,lf_resb & 0x80);					\
	SETFLAGBIT(OF,(lf_resb ^ (lf_resb<<1)) & 0x80);

#define RORW(op1,op2,load,save)					\
	FillFlagsNoCFOF();							\
	lf_var1w=load(op1);							\
	lf_var2b=op2&0xf;							\
	lf_resw=(lf_var1w >> lf_var2b) |			\
			(lf_var1w << (16-lf_var2b));		\
	save(op1,lf_resw);							\
	SETFLAGBIT(CF,lf_resw & 0x8000);			\
	SETFLAGBIT(OF,(lf_resw ^ (lf_resw<<1)) & 0x8000);

#define RORD(op1,op2,load,save)					\
	FillFlagsNoCFOF();							\
	lf_var1d=load(op1);							\
	lf_var2b=op2;								\
	lf_resd=(lf_var1d >> lf_var2b) |			\
			(lf_var1d << (32-lf_var2b));		\
	save(op1,lf_resd);							\
	SETFLAGBIT(CF,lf_resd & 0x80000000);		\
	SETFLAGBIT(OF,(lf_resd ^ (lf_resd<<1)) & 0x80000000);


#define RCLB(op1,op2,load,save)							\
	if (!(op2%9)) break;								\
{	uint8_t cf=(uint8_t)FillFlags()&0x1;					\
	lf_var1b=load(op1);									\
	lf_var2b=op2%9;										\
	lf_resb=(lf_var1b << lf_var2b) |					\
			(cf << (lf_var2b-1)) |						\
			(lf_var1b >> (9-lf_var2b));					\
 	save(op1,lf_resb);									\
	SETFLAGBIT(CF,((lf_var1b >> (8-lf_var2b)) & 1));	\
	SETFLAGBIT(OF,(reg_flags & 1) ^ (lf_resb >> 7));	\
}

#define RCLW(op1,op2,load,save)							\
	if (!(op2%17)) break;								\
{	uint16_t cf=(uint16_t)FillFlags()&0x1;					\
	lf_var1w=load(op1);									\
	lf_var2b=op2%17;									\
	lf_resw=(lf_var1w << lf_var2b) |					\
			(cf << (lf_var2b-1)) |						\
			(lf_var1w >> (17-lf_var2b));				\
	save(op1,lf_resw);									\
	SETFLAGBIT(CF,((lf_var1w >> (16-lf_var2b)) & 1));	\
	SETFLAGBIT(OF,(reg_flags & 1) ^ (lf_resw >> 15));	\
}

#define RCLD(op1, op2, load, save) \
	if (!op2) \
		break; \
	{ \
		const uint32_t cf = FillFlags() & 0x1; \
		lf_var1d = load(op1); \
		lf_var2b = op2; \
		if (lf_var2b == 1) { \
			lf_resd = (lf_var1d << 1) | cf; \
		} else { \
			lf_resd = (lf_var1d << lf_var2b) | \
			          (cf << lf_var2b_minus_one()) | \
			          (lf_var1d >> (33 - lf_var2b)); \
		} \
		save(op1, lf_resd); \
		SETFLAGBIT(CF, ((lf_var1d >> (32 - lf_var2b)) & 1)); \
		SETFLAGBIT(OF, (reg_flags & 1) ^ (lf_resd >> 31)); \
	}

#define RCRB(op1, op2, load, save) \
	if (op2 % 9) { \
		uint8_t cf = (uint8_t)FillFlags() & 0x1; \
		lf_var1b = load(op1); \
		lf_var2b = op2 % 9; \
		lf_resb = (lf_var1b >> lf_var2b) | (cf << (8 - lf_var2b)) | \
		          (lf_var1b << (9 - lf_var2b)); \
		save(op1, lf_resb); \
		SETFLAGBIT(CF, (lf_var1b >> lf_var2b_minus_one()) & 1); \
		SETFLAGBIT(OF, (lf_resb ^ (lf_resb << 1)) & 0x80); \
	}

#define RCRW(op1, op2, load, save) \
	if (op2 % 17) { \
		uint16_t cf = (uint16_t)FillFlags() & 0x1; \
		lf_var1w = load(op1); \
		lf_var2b = op2 % 17; \
		lf_resw = (lf_var1w >> lf_var2b) | (cf << (16 - lf_var2b)) | \
		          (lf_var1w << (17 - lf_var2b)); \
		save(op1, lf_resw); \
		SETFLAGBIT(CF, (lf_var1w >> lf_var2b_minus_one()) & 1); \
		SETFLAGBIT(OF, (lf_resw ^ (lf_resw << 1)) & 0x8000); \
	}

#define RCRD(op1, op2, load, save) \
	if (op2) { \
		const uint32_t cf = FillFlags() & 0x1; \
		lf_var1d = load(op1); \
		lf_var2b = op2; \
		if (lf_var2b == 1) { \
			lf_resd = lf_var1d >> 1 | cf << 31; \
		} else { \
			lf_resd = (lf_var1d >> lf_var2b) | \
			          (cf << (32 - lf_var2b)) | \
			          (lf_var1d << (33 - lf_var2b)); \
		} \
		save(op1, lf_resd); \
		SETFLAGBIT(CF, (lf_var1d >> lf_var2b_minus_one()) & 1); \
		SETFLAGBIT(OF, (lf_resd ^ (lf_resd << 1)) & 0x80000000); \
	}

#define SHLB(op1,op2,load,save)								\
	lf_var1b=load(op1);lf_var2b=op2;				\
	lf_resb=lf_var1b << lf_var2b;			\
	save(op1,lf_resb);								\
	lflags.type=t_SHLb;

#define SHLW(op1,op2,load,save)								\
	lf_var1w=load(op1);lf_var2b=op2 ;				\
	lf_resw=lf_var1w << lf_var2b;			\
	save(op1,lf_resw);								\
	lflags.type=t_SHLw;

#define SHLD(op1,op2,load,save)								\
	lf_var1d=load(op1);lf_var2b=op2;				\
	lf_resd=lf_var1d << lf_var2b;			\
	save(op1,lf_resd);								\
	lflags.type=t_SHLd;


#define SHRB(op1,op2,load,save)								\
	lf_var1b=load(op1);lf_var2b=op2;				\
	lf_resb=lf_var1b >> lf_var2b;			\
	save(op1,lf_resb);								\
	lflags.type=t_SHRb;

#define SHRW(op1,op2,load,save)								\
	lf_var1w=load(op1);lf_var2b=op2;				\
	lf_resw=lf_var1w >> lf_var2b;			\
	save(op1,lf_resw);								\
	lflags.type=t_SHRw;

#define SHRD(op1,op2,load,save)								\
	lf_var1d=load(op1);lf_var2b=op2;				\
	lf_resd=lf_var1d >> lf_var2b;			\
	save(op1,lf_resd);								\
	lflags.type=t_SHRd;


#define SARB(op1,op2,load,save)								\
	lf_var1b=load(op1);lf_var2b=op2;				\
	if (lf_var2b>8) lf_var2b=8;						\
    if (lf_var1b & 0x80) {								\
		lf_resb=(lf_var1b >> lf_var2b)|		\
		(0xff << (8 - lf_var2b));						\
	} else {												\
		lf_resb=lf_var1b >> lf_var2b;		\
    }														\
	save(op1,lf_resb);								\
	lflags.type=t_SARb;

#define SARW(op1,op2,load,save)								\
	lf_var1w=load(op1);lf_var2b=op2;			\
	if (lf_var2b>16) lf_var2b=16;					\
	if (lf_var1w & 0x8000) {							\
		lf_resw=(lf_var1w >> lf_var2b)|		\
		(0xffff << (16 - lf_var2b));					\
	} else {												\
		lf_resw=lf_var1w >> lf_var2b;		\
    }														\
	save(op1,lf_resw);								\
	lflags.type=t_SARw;

#define SARD(op1,op2,load,save)								\
	lf_var2b=op2;lf_var1d=load(op1);			\
	if (lf_var1d & 0x80000000) {						\
		lf_resd=(lf_var1d >> lf_var2b)|		\
		(0xffffffff << (32 - lf_var2b));				\
	} else {												\
		lf_resd=lf_var1d >> lf_var2b;		\
    }														\
	save(op1,lf_resd);								\
	lflags.type=t_SARd;



#define DAA()												\
	if (((reg_al & 0x0F)>0x09) || get_AF()) {				\
		if ((reg_al > 0x99) || get_CF()) {					\
			reg_al+=0x60;									\
			SETFLAGBIT(CF,true);							\
		} else {											\
			SETFLAGBIT(CF,false);							\
		}													\
		reg_al+=0x06;										\
		SETFLAGBIT(AF,true);								\
	} else {												\
		if ((reg_al > 0x99) || get_CF()) {					\
			reg_al+=0x60;									\
			SETFLAGBIT(CF,true);							\
		} else {											\
			SETFLAGBIT(CF,false);							\
		}													\
		SETFLAGBIT(AF,false);								\
	}														\
	SETFLAGBIT(SF,(reg_al&0x80));							\
	SETFLAGBIT(ZF,(reg_al==0));								\
	SETFLAGBIT(PF,parity_lookup[reg_al]);					\
	lflags.type=t_UNKNOWN;


#define DAS()												\
{															\
	uint8_t osigned=reg_al & 0x80;							\
	if (((reg_al & 0x0f) > 9) || get_AF()) {				\
		if ((reg_al>0x99) || get_CF()) {					\
			reg_al-=0x60;									\
			SETFLAGBIT(CF,true);							\
		} else {											\
			SETFLAGBIT(CF,(reg_al<=0x05));					\
		}													\
		reg_al-=6;											\
		SETFLAGBIT(AF,true);								\
	} else {												\
		if ((reg_al>0x99) || get_CF()) {					\
			reg_al-=0x60;									\
			SETFLAGBIT(CF,true);							\
		} else {											\
			SETFLAGBIT(CF,false);							\
		}													\
		SETFLAGBIT(AF,false);								\
	}														\
	SETFLAGBIT(OF,osigned && ((reg_al&0x80)==0));			\
	SETFLAGBIT(SF,(reg_al&0x80));							\
	SETFLAGBIT(ZF,(reg_al==0));								\
	SETFLAGBIT(PF,parity_lookup[reg_al]);					\
	lflags.type=t_UNKNOWN;									\
}


#define AAA()												\
	SETFLAGBIT(SF,((reg_al>=0x7a) && (reg_al<=0xf9)));		\
	if ((reg_al & 0xf) > 9) {								\
		SETFLAGBIT(OF,(reg_al&0xf0)==0x70);					\
		reg_ax += 0x106;									\
		SETFLAGBIT(CF,true);								\
		SETFLAGBIT(ZF,(reg_al == 0));						\
		SETFLAGBIT(AF,true);								\
	} else if (get_AF()) {									\
		reg_ax += 0x106;									\
		SETFLAGBIT(OF,false);								\
		SETFLAGBIT(CF,true);								\
		SETFLAGBIT(ZF,false);								\
		SETFLAGBIT(AF,true);								\
	} else {												\
		SETFLAGBIT(OF,false);								\
		SETFLAGBIT(CF,false);								\
		SETFLAGBIT(ZF,(reg_al == 0));						\
		SETFLAGBIT(AF,false);								\
	}														\
	SETFLAGBIT(PF,parity_lookup[reg_al]);					\
	reg_al &= 0x0F;											\
	lflags.type=t_UNKNOWN;

#define AAS()												\
	if ((reg_al & 0x0f)>9) {								\
		SETFLAGBIT(SF,(reg_al>0x85));						\
		reg_ax -= 0x106;									\
		SETFLAGBIT(OF,false);								\
		SETFLAGBIT(CF,true);								\
		SETFLAGBIT(AF,true);								\
	} else if (get_AF()) {									\
		SETFLAGBIT(OF,((reg_al>=0x80) && (reg_al<=0x85)));	\
		SETFLAGBIT(SF,(reg_al<0x06) || (reg_al>0x85));		\
		reg_ax -= 0x106;									\
		SETFLAGBIT(CF,true);								\
		SETFLAGBIT(AF,true);								\
	} else {												\
		SETFLAGBIT(SF,(reg_al>=0x80));						\
		SETFLAGBIT(OF,false);								\
		SETFLAGBIT(CF,false);								\
		SETFLAGBIT(AF,false);								\
	}														\
	SETFLAGBIT(ZF,(reg_al == 0));							\
	SETFLAGBIT(PF,parity_lookup[reg_al]);					\
	reg_al &= 0x0F;											\
	lflags.type=t_UNKNOWN;

#define AAM(op1)											\
{															\
	uint8_t dv=op1;											\
	if (dv!=0) {											\
		reg_ah=reg_al / dv;									\
		reg_al=reg_al % dv;									\
		SETFLAGBIT(SF,(reg_al & 0x80));						\
		SETFLAGBIT(ZF,(reg_al == 0));						\
		SETFLAGBIT(PF,parity_lookup[reg_al]);				\
		SETFLAGBIT(CF,false);								\
		SETFLAGBIT(OF,false);								\
		SETFLAGBIT(AF,false);								\
		lflags.type=t_UNKNOWN;								\
	} else EXCEPTION(0);									\
}


//Took this from bochs, i seriously hate these weird bcd opcodes
#define AAD(op1)											\
	{														\
		uint16_t ax1 = reg_ah * op1;							\
		uint16_t ax2 = ax1 + reg_al;							\
		reg_al = (uint8_t) ax2;								\
		reg_ah = 0;											\
		SETFLAGBIT(CF,false);								\
		SETFLAGBIT(OF,false);								\
		SETFLAGBIT(AF,false);								\
		SETFLAGBIT(SF,reg_al >= 0x80);						\
		SETFLAGBIT(ZF,reg_al == 0);							\
		SETFLAGBIT(PF,parity_lookup[reg_al]);				\
		lflags.type=t_UNKNOWN;								\
	}

#define MULB(op1,load,save)									\
	reg_ax=reg_al*load(op1);								\
	FillFlagsNoCFOF();										\
	SETFLAGBIT(ZF,reg_al == 0);								\
	if (reg_ax & 0xff00) {									\
		SETFLAGBIT(CF,true);SETFLAGBIT(OF,true);			\
	} else {												\
		SETFLAGBIT(CF,false);SETFLAGBIT(OF,false);			\
	}

#define MULW(op1, load, save) \
	{ \
		const uint32_t res = static_cast<uint32_t>(reg_ax) * \
		                     static_cast<uint32_t>(load(op1)); \
		reg_ax = static_cast<uint16_t>(res); \
		reg_dx = static_cast<uint16_t>(res >> 16); \
		FillFlagsNoCFOF(); \
		SETFLAGBIT(ZF, reg_ax == 0); \
		if (reg_dx) { \
			SETFLAGBIT(CF, true); \
			SETFLAGBIT(OF, true); \
		} else { \
			SETFLAGBIT(CF, false); \
			SETFLAGBIT(OF, false); \
		} \
	}

#define MULD(op1, load, save) \
	{ \
		const uint64_t res = static_cast<uint64_t>(reg_eax) * \
		                     static_cast<uint64_t>(load(op1)); \
		reg_eax = static_cast<uint32_t>(res); \
		reg_edx = static_cast<uint32_t>(res >> 32); \
		FillFlagsNoCFOF(); \
		SETFLAGBIT(ZF, reg_eax == 0); \
		if (reg_edx) { \
			SETFLAGBIT(CF, true); \
			SETFLAGBIT(OF, true); \
		} else { \
			SETFLAGBIT(CF, false); \
			SETFLAGBIT(OF, false); \
		} \
	}

#define DIVB(op1,load,save)									\
{															\
	Bitu val=load(op1);										\
	if (val==0)	EXCEPTION(0);								\
	Bitu quo=reg_ax / val;									\
	uint8_t rem=(uint8_t)(reg_ax % val);						\
	uint8_t quo8=(uint8_t)(quo&0xff);							\
	if (quo>0xff) EXCEPTION(0);								\
	reg_ah=rem;												\
	reg_al=quo8;											\
	set_cpu_test_flags_for_division(quo8); \
}

#define DIVW(op1,load,save)									\
{															\
	Bitu val=load(op1);										\
	if (val==0)	EXCEPTION(0);								\
	Bitu num=((uint32_t)reg_dx<<16)|reg_ax;							\
	Bitu quo=num/val;										\
	uint16_t rem=(uint16_t)(num % val);							\
	uint16_t quo16=(uint16_t)(quo&0xffff);						\
	if (quo!=(uint32_t)quo16) EXCEPTION(0);					\
	reg_dx=rem;												\
	reg_ax=quo16;											\
	set_cpu_test_flags_for_division(quo16); \
}

#define DIVD(op1,load,save)									\
{															\
	Bitu val=load(op1);										\
	if (val==0) EXCEPTION(0);									\
	uint64_t num=(((uint64_t)reg_edx)<<32)|reg_eax;				\
	uint64_t quo=num/val;										\
	uint32_t rem=(uint32_t)(num % val);							\
	uint32_t quo32=(uint32_t)(quo&0xffffffff);					\
	if (quo!=(uint64_t)quo32) EXCEPTION(0);					\
	reg_edx=rem;											\
	reg_eax=quo32;											\
	set_cpu_test_flags_for_division(quo32); \
}


#define IDIVB(op1,load,save)								\
{															\
	Bits val=(int8_t)(load(op1));							\
	if (val==0)	EXCEPTION(0);								\
	Bits quo=((int16_t)reg_ax) / val;						\
	int8_t rem=(int8_t)((int16_t)reg_ax % val);				\
	int8_t quo8s=(int8_t)(quo&0xff);							\
	if (quo!=(int16_t)quo8s) EXCEPTION(0);					\
	reg_ah=rem;												\
	reg_al=quo8s;											\
	set_cpu_test_flags_for_division(quo8s); \
}

#define IDIVW(op1, load, save) \
	{ \
		const auto val = static_cast<int16_t>(load(op1)); \
		if (val == 0) \
			EXCEPTION(0); \
		const auto num = (reg_dx << 16) | reg_ax; \
		const auto quo = num / val; \
		const auto rem = num % val; \
		const auto quo16s = static_cast<int16_t>(quo); \
		if (quo != quo16s) \
			EXCEPTION(0); \
		reg_ax = quo16s; \
		reg_dx = static_cast<int16_t>(rem); \
		set_cpu_test_flags_for_division(quo16s); \
	}

#define IDIVD(op1,load,save)								\
{															\
	Bits val=(int32_t)(load(op1));							\
	if (val==0) EXCEPTION(0);									\
	int64_t num=(((uint64_t)reg_edx)<<32)|reg_eax;				\
	int64_t quo=num/val;										\
	int32_t rem=(int32_t)(num % val);							\
	int32_t quo32s=(int32_t)(quo&0xffffffff);					\
	if (quo!=(int64_t)quo32s) EXCEPTION(0);					\
	reg_edx=rem;											\
	reg_eax=quo32s;											\
	set_cpu_test_flags_for_division(quo32s); \
}
#define IMULB(op1,load,save)								\
{															\
	reg_ax=((int8_t)reg_al) * ((int8_t)(load(op1)));			\
	FillFlagsNoCFOF();										\
	SETFLAGBIT(ZF,reg_al == 0);								\
	SETFLAGBIT(SF,reg_al & 0x80);							\
	if ((reg_ax & 0xff80)==0xff80 ||						\
		(reg_ax & 0xff80)==0x0000) {						\
		SETFLAGBIT(CF,false);SETFLAGBIT(OF,false);			\
	} else {												\
		SETFLAGBIT(CF,true);SETFLAGBIT(OF,true);			\
	}														\
}

#define IMULW(op1, load, save) \
	{ \
		const auto res = static_cast<int16_t>(reg_ax) * \
		                 static_cast<int16_t>(load(op1)); \
		reg_ax = static_cast<int16_t>(res); \
		reg_dx = static_cast<int16_t>(res >> 16); \
		FillFlagsNoCFOF(); \
		SETFLAGBIT(ZF, reg_ax == 0); \
		SETFLAGBIT(SF, reg_ax & 0x8000); \
		if (((res & 0xffff8000) == 0xffff8000 || \
		     (res & 0xffff8000) == 0x0000)) { \
			SETFLAGBIT(CF, false); \
			SETFLAGBIT(OF, false); \
		} else { \
			SETFLAGBIT(CF, true); \
			SETFLAGBIT(OF, true); \
		} \
	}

#define IMULD(op1, load, save) \
	{ \
		int64_t temps = ((int64_t)((int32_t)reg_eax)) * \
		               ((int64_t)((int32_t)(load(op1)))); \
		reg_eax = (int32_t)(temps); \
		reg_edx = (int32_t)(temps >> 32); \
		FillFlagsNoCFOF(); \
		SETFLAGBIT(ZF, reg_eax == 0); \
		SETFLAGBIT(SF, reg_eax & 0x80000000); \
		if ((reg_edx == 0xffffffff) && (reg_eax & 0x80000000)) { \
			SETFLAGBIT(CF, false); \
			SETFLAGBIT(OF, false); \
		} else if ((reg_edx == 0x00000000) && (reg_eax < 0x80000000)) { \
			SETFLAGBIT(CF, false); \
			SETFLAGBIT(OF, false); \
		} else { \
			SETFLAGBIT(CF, true); \
			SETFLAGBIT(OF, true); \
		} \
	}

#define DIMULW(op1, op2, op3, load, save) \
	{ \
		const auto res = op2 * op3; \
		save(op1, res & 0xffff); \
		FillFlagsNoCFOF(); \
		if ((res >= -32768) && (res <= 32767)) { \
			SETFLAGBIT(CF, false); \
			SETFLAGBIT(OF, false); \
		} else { \
			SETFLAGBIT(CF, true); \
			SETFLAGBIT(OF, true); \
		} \
	}

#define DIMULD(op1, op2, op3, load, save) \
	{ \
		/* All DIMULD operands are signed 32-bit ints */ \
		const auto op2_i32 = static_cast<int32_t>(op2); \
		const auto op3_i32 = static_cast<int32_t>(op3); \
\
		/* Store the multiplication using 64 bits to detect overflow */ \
		const auto result_i64 = static_cast<int64_t>(op2_i32) * op3_i32; \
\
		/* Save the result as a 32-bit and let it overflow */ \
		const auto result_i32 = static_cast<int32_t>(result_i64); \
		save(op1, result_i32); \
		FillFlagsNoCFOF(); \
\
		/* Set the carry and overflow flags accordingly */ \
		const auto had_overflow = (result_i32 != result_i64); \
		SETFLAGBIT(CF, had_overflow); \
		SETFLAGBIT(OF, had_overflow); \
	}

#define GRP2B(blah)											\
{															\
	GetRM;Bitu which=(rm>>3)&7;								\
	if (rm >= 0xc0) {										\
		GetEArb;											\
		uint8_t val=blah & 0x1f;								\
		if (!val) break;									\
		switch (which)	{									\
		case 0x00:ROLB(*earb,val,LoadRb,SaveRb);break;		\
		case 0x01:RORB(*earb,val,LoadRb,SaveRb);break;		\
		case 0x02:RCLB(*earb,val,LoadRb,SaveRb);break;		\
		case 0x03:RCRB(*earb,val,LoadRb,SaveRb);break;		\
		case 0x04:/* SHL and SAL are the same */			\
		case 0x06:SHLB(*earb,val,LoadRb,SaveRb);break;		\
		case 0x05:SHRB(*earb,val,LoadRb,SaveRb);break;		\
		case 0x07:SARB(*earb,val,LoadRb,SaveRb);break;		\
		}													\
	} else {												\
		GetEAa;												\
		uint8_t val=blah & 0x1f;								\
		if (!val) break;									\
		switch (which) {									\
		case 0x00:ROLB(eaa,val,LoadMb,SaveMb);break;		\
		case 0x01:RORB(eaa,val,LoadMb,SaveMb);break;		\
		case 0x02:RCLB(eaa,val,LoadMb,SaveMb);break;		\
		case 0x03:RCRB(eaa,val,LoadMb,SaveMb);break;		\
		case 0x04:/* SHL and SAL are the same */			\
		case 0x06:SHLB(eaa,val,LoadMb,SaveMb);break;		\
		case 0x05:SHRB(eaa,val,LoadMb,SaveMb);break;		\
		case 0x07:SARB(eaa,val,LoadMb,SaveMb);break;		\
		}													\
	}														\
}



#define GRP2W(blah)											\
{															\
	GetRM;Bitu which=(rm>>3)&7;								\
	if (rm >= 0xc0) {										\
		GetEArw;											\
		uint8_t val=blah & 0x1f;								\
		if (!val) break;									\
		switch (which)	{									\
		case 0x00:ROLW(*earw,val,LoadRw,SaveRw);break;		\
		case 0x01:RORW(*earw,val,LoadRw,SaveRw);break;		\
		case 0x02:RCLW(*earw,val,LoadRw,SaveRw);break;		\
		case 0x03:RCRW(*earw,val,LoadRw,SaveRw);break;		\
		case 0x04:/* SHL and SAL are the same */			\
		case 0x06:SHLW(*earw,val,LoadRw,SaveRw);break;		\
		case 0x05:SHRW(*earw,val,LoadRw,SaveRw);break;		\
		case 0x07:SARW(*earw,val,LoadRw,SaveRw);break;		\
		}													\
	} else {												\
		GetEAa;												\
		uint8_t val=blah & 0x1f;								\
		if (!val) break;									\
		switch (which) {									\
		case 0x00:ROLW(eaa,val,LoadMw,SaveMw);break;		\
		case 0x01:RORW(eaa,val,LoadMw,SaveMw);break;		\
		case 0x02:RCLW(eaa,val,LoadMw,SaveMw);break;		\
		case 0x03:RCRW(eaa,val,LoadMw,SaveMw);break;		\
		case 0x04:/* SHL and SAL are the same */			\
		case 0x06:SHLW(eaa,val,LoadMw,SaveMw);break;		\
		case 0x05:SHRW(eaa,val,LoadMw,SaveMw);break;		\
		case 0x07:SARW(eaa,val,LoadMw,SaveMw);break;		\
		}													\
	}														\
}


#define GRP2D(blah)											\
{															\
	GetRM;Bitu which=(rm>>3)&7;								\
	if (rm >= 0xc0) {										\
		GetEArd;											\
		uint8_t val=blah & 0x1f;								\
		if (!val) break;									\
		switch (which)	{									\
		case 0x00:ROLD(*eard,val,LoadRd,SaveRd);break;		\
		case 0x01:RORD(*eard,val,LoadRd,SaveRd);break;		\
		case 0x02:RCLD(*eard,val,LoadRd,SaveRd);break;		\
		case 0x03:RCRD(*eard,val,LoadRd,SaveRd);break;		\
		case 0x04:/* SHL and SAL are the same */			\
		case 0x06:SHLD(*eard,val,LoadRd,SaveRd);break;		\
		case 0x05:SHRD(*eard,val,LoadRd,SaveRd);break;		\
		case 0x07:SARD(*eard,val,LoadRd,SaveRd);break;		\
		}													\
	} else {												\
		GetEAa;												\
		uint8_t val=blah & 0x1f;								\
		if (!val) break;									\
		switch (which) {									\
		case 0x00:ROLD(eaa,val,LoadMd,SaveMd);break;		\
		case 0x01:RORD(eaa,val,LoadMd,SaveMd);break;		\
		case 0x02:RCLD(eaa,val,LoadMd,SaveMd);break;		\
		case 0x03:RCRD(eaa,val,LoadMd,SaveMd);break;		\
		case 0x04:/* SHL and SAL are the same */			\
		case 0x06:SHLD(eaa,val,LoadMd,SaveMd);break;		\
		case 0x05:SHRD(eaa,val,LoadMd,SaveMd);break;		\
		case 0x07:SARD(eaa,val,LoadMd,SaveMd);break;		\
		}													\
	}														\
}

/* let's hope bochs has it correct with the higher than 16 shifts */
/* double-precision shift left has low bits in second argument */
#define DSHLW(op1,op2,op3,load,save)									\
	uint8_t val=op3 & 0x1F;												\
	if (!val) break;													\
	lf_var2b=val;lf_var1d=(load(op1)<<16)|op2;					\
	uint32_t tempd=lf_var1d << lf_var2b;							\
  	if (lf_var2b>16) tempd |= (op2 << (lf_var2b - 16));			\
	lf_resw=(uint16_t)(tempd >> 16);								\
	save(op1,lf_resw);											\
	lflags.type=t_DSHLw;

#define DSHLD(op1,op2,op3,load,save)									\
	uint8_t val=op3 & 0x1F;												\
	if (!val) break;													\
	lf_var2b=val;lf_var1d=load(op1);							\
	lf_resd=(lf_var1d << lf_var2b) | (op2 >> (32-lf_var2b));	\
	save(op1,lf_resd);											\
	lflags.type=t_DSHLd;

/* double-precision shift right has high bits in second argument */
#define DSHRW(op1,op2,op3,load,save)									\
	uint8_t val=op3 & 0x1F;												\
	if (!val) break;													\
	lf_var2b=val;lf_var1d=(op2<<16)|load(op1);					\
	uint32_t tempd=lf_var1d >> lf_var2b;							\
  	if (lf_var2b>16) tempd |= (op2 << (32-lf_var2b ));			\
	lf_resw=(uint16_t)(tempd);										\
	save(op1,lf_resw);											\
	lflags.type=t_DSHRw;

#define DSHRD(op1,op2,op3,load,save)									\
	uint8_t val=op3 & 0x1F;												\
	if (!val) break;													\
	lf_var2b=val;lf_var1d=load(op1);							\
	lf_resd=(lf_var1d >> lf_var2b) | (op2 << (32-lf_var2b));	\
	save(op1,lf_resd);											\
	lflags.type=t_DSHRd;

#define BSWAPW(op1)														\
	op1 = 0;

#define BSWAPD(op1)														\
	op1 = (op1>>24)|((op1>>8)&0xFF00)|((op1<<8)&0xFF0000)|((op1<<24)&0xFF000000);
