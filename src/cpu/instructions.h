/*
 *  Copyright (C) 2002  The DOSBox Team
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

/* Jumps */

/* All Byte genereal instructions */
#define ADDB(op1,op2,load,save)								\
	flags.var1.b=load(op1);flags.var2.b=op2;					\
	flags.result.b=flags.var1.b+flags.var2.b;					\
	save(op1,flags.result.b);								\
	flags.type=t_ADDb;

#define ADCB(op1,op2,load,save)								\
	flags.oldcf=get_CF();									\
	flags.var1.b=load(op1);flags.var2.b=op2;					\
	flags.result.b=flags.var1.b+flags.var2.b+flags.oldcf;		\
	save(op1,flags.result.b);								\
	flags.type=t_ADCb;

#define SBBB(op1,op2,load,save)								\
	flags.oldcf=get_CF();									\
	flags.var1.b=load(op1);flags.var2.b=op2;					\
	flags.result.b=flags.var1.b-(flags.var2.b+flags.oldcf);	\
	save(op1,flags.result.b);								\
	flags.type=t_SBBb;

#define SUBB(op1,op2,load,save)								\
	flags.var1.b=load(op1);flags.var2.b=op2;					\
	flags.result.b=flags.var1.b-flags.var2.b;					\
	save(op1,flags.result.b);								\
	flags.type=t_SUBb;

#define ORB(op1,op2,load,save)								\
	flags.var1.b=load(op1);flags.var2.b=op2;					\
	flags.result.b=flags.var1.b | flags.var2.b;				\
	save(op1,flags.result.b);								\
	flags.type=t_ORb;

#define XORB(op1,op2,load,save)								\
	flags.var1.b=load(op1);flags.var2.b=op2;					\
	flags.result.b=flags.var1.b ^ flags.var2.b;				\
	save(op1,flags.result.b);								\
	flags.type=t_XORb;

#define ANDB(op1,op2,load,save)								\
	flags.var1.b=load(op1);flags.var2.b=op2;					\
	flags.result.b=flags.var1.b & flags.var2.b;				\
	save(op1,flags.result.b);								\
	flags.type=t_ANDb;

#define CMPB(op1,op2,load,save)								\
	flags.var1.b=load(op1);flags.var2.b=op2;					\
	flags.result.b=flags.var1.b-flags.var2.b;					\
	flags.type=t_CMPb;

#define TESTB(op1,op2,load,save)							\
	flags.var1.b=load(op1);flags.var2.b=op2;					\
	flags.result.b=flags.var1.b & flags.var2.b;				\
	flags.type=t_TESTb;

/* All Word General instructions */

#define ADDW(op1,op2,load,save)								\
	flags.var1.w=load(op1);flags.var2.w=op2;					\
	flags.result.w=flags.var1.w+flags.var2.w;					\
	save(op1,flags.result.w);								\
	flags.type=t_ADDw;

#define ADCW(op1,op2,load,save)								\
	flags.oldcf=get_CF();									\
	flags.var1.w=load(op1);flags.var2.w=op2;					\
	flags.result.w=flags.var1.w+flags.var2.w+flags.oldcf;		\
	save(op1,flags.result.w);								\
	flags.type=t_ADCw;

#define SBBW(op1,op2,load,save)								\
	flags.oldcf=get_CF();									\
	flags.var1.w=load(op1);flags.var2.w=op2;					\
	flags.result.w=flags.var1.w-(flags.var2.w+flags.oldcf);	\
	save(op1,flags.result.w);								\
	flags.type=t_SBBw;

#define SUBW(op1,op2,load,save)								\
	flags.var1.w=load(op1);flags.var2.w=op2;					\
	flags.result.w=flags.var1.w-flags.var2.w;					\
	save(op1,flags.result.w);								\
	flags.type=t_SUBw;

#define ORW(op1,op2,load,save)								\
	flags.var1.w=load(op1);flags.var2.w=op2;					\
	flags.result.w=flags.var1.w | flags.var2.w;				\
	save(op1,flags.result.w);								\
	flags.type=t_ORw;

#define XORW(op1,op2,load,save)								\
	flags.var1.w=load(op1);flags.var2.w=op2;					\
	flags.result.w=flags.var1.w ^ flags.var2.w;				\
	save(op1,flags.result.w);								\
	flags.type=t_XORw;

#define ANDW(op1,op2,load,save)								\
	flags.var1.w=load(op1);flags.var2.w=op2;					\
	flags.result.w=flags.var1.w & flags.var2.w;				\
	save(op1,flags.result.w);								\
	flags.type=t_ANDw;

#define CMPW(op1,op2,load,save)								\
	flags.var1.w=load(op1);flags.var2.w=op2;					\
	flags.result.w=flags.var1.w-flags.var2.w;					\
	flags.type=t_CMPw;

#define TESTW(op1,op2,load,save)							\
	flags.var1.w=load(op1);flags.var2.w=op2;					\
	flags.result.w=flags.var1.w & flags.var2.w;				\
	flags.type=t_TESTw;

/* All DWORD General Instructions */

#define ADDD(op1,op2,load,save)								\
	flags.var1.d=load(op1);flags.var2.d=op2;					\
	flags.result.d=flags.var1.d+flags.var2.d;					\
	save(op1,flags.result.d);								\
	flags.type=t_ADDd;

#define ADCD(op1,op2,load,save)								\
	flags.oldcf=get_CF();									\
	flags.var1.d=load(op1);flags.var2.d=op2;					\
	flags.result.d=flags.var1.d+flags.var2.d+flags.oldcf;		\
	save(op1,flags.result.d);								\
	flags.type=t_ADCd;

#define SBBD(op1,op2,load,save)								\
	flags.oldcf=get_CF();									\
	flags.var1.d=load(op1);flags.var2.d=op2;					\
	flags.result.d=flags.var1.d-(flags.var2.d+flags.oldcf);	\
	save(op1,flags.result.d);								\
	flags.type=t_SBBd;

#define SUBD(op1,op2,load,save)								\
	flags.var1.d=load(op1);flags.var2.d=op2;					\
	flags.result.d=flags.var1.d-flags.var2.d;					\
	save(op1,flags.result.d);								\
	flags.type=t_SUBd;

#define ORD(op1,op2,load,save)								\
	flags.var1.d=load(op1);flags.var2.d=op2;					\
	flags.result.d=flags.var1.d | flags.var2.d;				\
	save(op1,flags.result.d);								\
	flags.type=t_ORd;

#define XORD(op1,op2,load,save)								\
	flags.var1.d=load(op1);flags.var2.d=op2;					\
	flags.result.d=flags.var1.d ^ flags.var2.d;				\
	save(op1,flags.result.d);								\
	flags.type=t_XORd;

#define ANDD(op1,op2,load,save)								\
	flags.var1.d=load(op1);flags.var2.d=op2;					\
	flags.result.d=flags.var1.d & flags.var2.d;				\
	save(op1,flags.result.d);								\
	flags.type=t_ANDd;

#define CMPD(op1,op2,load,save)								\
	flags.var1.d=load(op1);flags.var2.d=op2;					\
	flags.result.d=flags.var1.d-flags.var2.d;					\
	flags.type=t_CMPd;


#define TESTD(op1,op2,load,save)							\
	flags.var1.d=load(op1);flags.var2.d=op2;					\
	flags.result.d=flags.var1.d & flags.var2.d;				\
	flags.type=t_TESTd;




#define INCB(op1,load,save)									\
	LoadCF;flags.result.b=load(op1)+1;						\
	save(op1,flags.result.b);								\
	flags.type=t_INCb;										\

#define INCW(op1,load,save)									\
	LoadCF;flags.result.w=load(op1)+1;						\
	save(op1,flags.result.w);								\
	flags.type=t_INCw;

#define INCD(op1,load,save)									\
	LoadCF;flags.result.d=load(op1)+1;						\
	save(op1,flags.result.d);								\
	flags.type=t_INCd;

#define DECB(op1,load,save)									\
	LoadCF;flags.result.b=load(op1)-1;						\
	save(op1,flags.result.b);								\
	flags.type=t_DECb;

#define DECW(op1,load,save)									\
	LoadCF;flags.result.w=load(op1)-1;						\
	save(op1,flags.result.w);								\
	flags.type=t_DECw;

#define DECD(op1,load,save)									\
	LoadCF;flags.result.d=load(op1)-1;						\
	save(op1,flags.result.d);								\
	flags.type=t_DECd;

#define ROLB(op1,op2,load,save)								\
	if (op2&0x07)	{										\
		LoadZF;LoadSF;LoadAF;								\
		flags.var1.b=load(op1);								\
		flags.var2.b=op2&0x07;								\
		flags.result.b=(flags.var1.b << flags.var2.b) |		\
				(flags.var1.b >> (8-flags.var2.b));			\
		save(op1,flags.result.b);							\
		flags.type=t_ROLb;									\
	}

#define ROLW(op1,op2,load,save)								\
	if (op2&0x0F) {											\
		LoadZF;LoadSF;LoadAF;								\
		flags.var1.w=load(op1);								\
		flags.var2.b=op2&0x0F;								\
		flags.result.w=(flags.var1.w << flags.var2.b) |		\
				(flags.var1.w >> (16-flags.var2.b));		\
		save(op1,flags.result.w);							\
		flags.type=t_ROLw;									\
	}

#define ROLD(op1,op2,load,save)								\
	if (op2) {												\
		LoadZF;LoadSF;LoadAF;								\
		flags.var1.d=load(op1);								\
		flags.var2.b=op2;									\
		flags.result.d=(flags.var1.d << flags.var2.b) |		\
				(flags.var1.d >> (32-flags.var2.b));		\
		save(op1,flags.result.d);							\
		flags.type=t_ROLd;									\
	}

#define RORB(op1,op2,load,save)								\
	if (op2&0x07)	{										\
		LoadZF;LoadSF;LoadAF;								\
		flags.var1.b=load(op1);								\
		flags.var2.b=op2&0x07;								\
		flags.result.b=(flags.var1.b >> flags.var2.b) |		\
				(flags.var1.b << (8-flags.var2.b));			\
		save(op1,flags.result.b);							\
		flags.type=t_RORb;									\
	}														\

#define RORW(op1,op2,load,save)								\
	if (op2&0x0F) {											\
		LoadZF;LoadSF;LoadAF;								\
		flags.var1.w=load(op1);								\
		flags.var2.b=op2&0x0F;								\
		flags.result.w=(flags.var1.w >> flags.var2.b) |		\
				(flags.var1.w << (16-flags.var2.b));		\
		save(op1,flags.result.w);							\
		flags.type=t_RORw;									\
	}

#define RORD(op1,op2,load,save)								\
	if (op2) {												\
		LoadZF;LoadSF;LoadAF;								\
		flags.var1.d=load(op1);								\
		flags.var2.b=op2;									\
		flags.result.d=(flags.var1.d >> flags.var2.b) |		\
				(flags.var1.d << (32-flags.var2.b));		\
		save(op1,flags.result.d);							\
		flags.type=t_RORd;									\
	}


#define RCLB(op1,op2,load,save)								\
	if (op2%9) {											\
		LoadZF;LoadSF;LoadAF;								\
		Bit8u cf=get_CF();									\
		flags.var1.b=load(op1);								\
		flags.var2.b=op2%9;									\
		flags.type=t_RCLb;									\
		flags.result.b=(flags.var1.b << flags.var2.b) |		\
				(cf << (flags.var2.b-1)) |					\
				(flags.var1.b >> (9-flags.var2.b));			\
		SETFLAGBIT(CF,((flags.var1.b >> (8-flags.var2.b)) & 1));			\
		SETFLAGBIT(OF,(flags.var1.b ^ flags.result.b) & 0x80);				\
 		save(op1,flags.result.b);							\
	}

#define RCLW(op1,op2,load,save)								\
	if (op2%17) {											\
		LoadZF;LoadSF;LoadAF;								\
		Bit16u cf=get_CF();									\
		flags.var1.w=load(op1);								\
		flags.var2.b=op2%17;								\
		flags.type=t_RCLw;									\
		flags.result.w=(flags.var1.w << flags.var2.b) |		\
				(cf << (flags.var2.b-1)) |					\
				(flags.var1.w >> (17-flags.var2.b));		\
		SETFLAGBIT(CF,((flags.var1.w >> (16-flags.var2.b)) & 1));				\
		SETFLAGBIT(OF,(flags.var1.w ^ flags.result.w) & 0x8000);				\
		save(op1,flags.result.w);							\
	}

#define RCLD(op1,op2,load,save)								\
	if (op2) {												\
		LoadZF;LoadSF;LoadAF;								\
		Bit32u cf=get_CF();									\
		flags.var1.d=load(op1);								\
		flags.var2.b=op2;									\
		flags.type=t_RCLd;									\
		if (flags.var2.b==1)	{							\
			flags.result.d=(flags.var1.d << 1) | cf;		\
		} else 	{											\
			flags.result.d=(flags.var1.d << flags.var2.b) |	\
			(cf << (flags.var2.b-1)) |						\
			(flags.var1.d >> (33-flags.var2.b));			\
		}													\
		SETFLAGBIT(CF,((flags.var1.d >> (32-flags.var2.b)) & 1));				\
		SETFLAGBIT(OF,(flags.var1.d ^ flags.result.d) & 0x80000000);			\
		save(op1,flags.result.d);							\
	}

#define RCRB(op1,op2,load,save)								\
	if (op2%9) {											\
		LoadZF;LoadSF;LoadAF;								\
		Bit8u cf=get_CF();									\
		flags.var1.b=load(op1);								\
		flags.var2.b=op2%9;									\
		flags.type=t_RCRb;									\
	 	flags.result.b=(flags.var1.b >> flags.var2.b) |		\
				(cf << (8-flags.var2.b)) |					\
				(flags.var1.b << (9-flags.var2.b));			\
		save(op1,flags.result.b);							\
	}

#define RCRW(op1,op2,load,save)								\
	if (op2%17) {											\
		LoadZF;LoadSF;LoadAF;								\
		Bit16u cf=get_CF();									\
		flags.var1.w=load(op1);								\
		flags.var2.b=op2%17;								\
		flags.type=t_RCRw;									\
	 	flags.result.w=(flags.var1.w >> flags.var2.b) |		\
				(cf << (16-flags.var2.b)) |					\
				(flags.var1.w << (17-flags.var2.b));		\
		save(op1,flags.result.w);							\
	}

#define RCRD(op1,op2,load,save)								\
	if (op2) {												\
		LoadZF;LoadSF;LoadAF;								\
		Bit32u cf=get_CF();									\
		flags.var1.d=load(op1);								\
		flags.var2.b=op2;									\
		flags.type=t_RCRd;									\
		if (flags.var2.b==1) {								\
			flags.result.d=flags.var1.d >> 1 | cf << 31;	\
		} else {											\
 			flags.result.d=(flags.var1.d >> flags.var2.b) |	\
				(cf << (32-flags.var2.b)) |					\
				(flags.var1.d << (33-flags.var2.b));		\
		}													\
		save(op1,flags.result.d);							\
	}


#define SHLB(op1,op2,load,save)								\
	if (!op2) break;								\
	flags.var1.b=load(op1);flags.var2.b=op2;			\
	flags.result.b=flags.var1.b << flags.var2.b;			\
	save(op1,flags.result.b);								\
	flags.type=t_SHLb;

#define SHLW(op1,op2,load,save)								\
	if (!op2) break;								\
	flags.var1.w=load(op1);flags.var2.b=op2 ;			\
	flags.result.w=flags.var1.w << flags.var2.b;			\
	save(op1,flags.result.w);								\
	flags.type=t_SHLw;

#define SHLD(op1,op2,load,save)								\
	if (!op2) break;								\
	flags.var1.d=load(op1);flags.var2.b=op2;			\
	flags.result.d=flags.var1.d << flags.var2.b;			\
	save(op1,flags.result.d);								\
	flags.type=t_SHLd;


#define SHRB(op1,op2,load,save)								\
	if (!op2) break;								\
	flags.var1.b=load(op1);flags.var2.b=op2;			\
	flags.result.b=flags.var1.b >> flags.var2.b;			\
	save(op1,flags.result.b);								\
	flags.type=t_SHRb;

#define SHRW(op1,op2,load,save)								\
	if (!op2) break;								\
	flags.var1.w=load(op1);flags.var2.b=op2;			\
	flags.result.w=flags.var1.w >> flags.var2.b;			\
	save(op1,flags.result.w);								\
	flags.type=t_SHRw;

#define SHRD(op1,op2,load,save)								\
	if (!op2) break;								\
	flags.var1.d=load(op1);flags.var2.b=op2;			\
	flags.result.d=flags.var1.d >> flags.var2.b;			\
	save(op1,flags.result.d);								\
	flags.type=t_SHRd;


#define SARB(op1,op2,load,save)								\
	if (!op2) break;								\
	flags.var1.b=load(op1);flags.var2.b=op2;			\
	if (flags.var2.b>8) flags.var2.b=8;						\
    if (flags.var1.b & 0x80) {								\
		flags.result.b=(flags.var1.b >> flags.var2.b)|		\
		(0xff << (8 - flags.var2.b));						\
	} else {												\
		flags.result.b=flags.var1.b >> flags.var2.b;		\
    }														\
	save(op1,flags.result.b);								\
	flags.type=t_SARb;

#define SARW(op1,op2,load,save)								\
	if (!op2) break;								\
	flags.var1.w=load(op1);flags.var2.b=op2;			\
	if (flags.var2.b>16) flags.var2.b=16;					\
	if (flags.var1.w & 0x8000) {							\
		flags.result.w=(flags.var1.w >> flags.var2.b)|		\
		(0xffff << (16 - flags.var2.b));					\
	} else {												\
		flags.result.w=flags.var1.w >> flags.var2.b;		\
    }														\
	save(op1,flags.result.w);								\
	flags.type=t_SARw;

#define SARD(op1,op2,load,save)								\
	if (!op2) break;								\
	flags.var2.b=op2;flags.var1.d=load(op1);			\
	if (flags.var1.d & 0x80000000) {						\
		flags.result.d=(flags.var1.d >> flags.var2.b)|		\
		(0xffffffff << (32 - flags.var2.b));				\
	} else {												\
		flags.result.d=flags.var1.d >> flags.var2.b;		\
    }														\
	save(op1,flags.result.d);								\
	flags.type=t_SARd;



#define DAA()												\
	if (((reg_al & 0x0F)>0x09) || get_AF()) {				\
		reg_al+=0x06;										\
		SETFLAGBIT(AF,true);								\
	} else {												\
		SETFLAGBIT(AF,false);								\
	}														\
	if ((reg_al > 0x9F) || get_CF()) {						\
		reg_al+=0x60;										\
		SETFLAGBIT(CF,true);								\
	} else {												\
		SETFLAGBIT(CF,false);								\
	}														\
	SETFLAGBIT(SF,(reg_al&0x80));							\
	SETFLAGBIT(ZF,(reg_al==0));								\
	flags.type=t_UNKNOWN;


#define DAS()												\
	if (((reg_al & 0x0f) > 9) || get_AF()) {				\
		reg_al-=6;											\
		SETFLAGBIT(AF,true);								\
	} else {												\
		SETFLAGBIT(AF,false);								\
	}														\
	if ((reg_al>0x9f) || get_CF()) {						\
		reg_al-=0x60;										\
		SETFLAGBIT(CF,true);								\
	} else {												\
		SETFLAGBIT(CF,false);								\
	}														\
	flags.type=t_UNKNOWN;


#define AAA()												\
	if (get_AF() || ((reg_al & 0xf) > 9))					\
	{														\
		reg_al += 6;										\
		reg_ah += 1;										\
		SETFLAGBIT(AF,true);								\
		SETFLAGBIT(CF,true);								\
	} else {												\
		SETFLAGBIT(AF,false);								\
		SETFLAGBIT(CF,false);								\
	}														\
	reg_al &= 0x0F;											\
	flags.type=t_UNKNOWN;

#define AAS()												\
	if (((reg_al & 0x0f)>9) || get_AF()) {					\
		reg_ah--;											\
		if (reg_al < 6) reg_ah--;							\
		reg_al=(reg_al-6) & 0xF;							\
		SETFLAGBIT(AF,true);								\
		SETFLAGBIT(CF,true);								\
	} else {												\
		SETFLAGBIT(AF,false);								\
		SETFLAGBIT(CF,false);								\
	}														\
	reg_al &= 0x0F;											\
	flags.type=t_UNKNOWN;

#define AAM(op1)											\
	{														\
		Bit8u BLAH=op1;										\
		reg_ah=reg_al / BLAH;								\
		reg_al=reg_al % BLAH;								\
		flags.type=t_UNKNOWN;								\
		SETFLAGBIT(SF,(reg_ah & 0x80));						\
		SETFLAGBIT(ZF,(reg_ax == 0));						\
		SETFLAGBIT(PF,false);								\
	}


#define AAD(op1)											\
	{														\
		Bit8u BLAH=op1;										\
		reg_al=reg_ah*BLAH+reg_al;							\
		reg_ah=0;											\
		SETFLAGBIT(CF,(reg_al>=0x80));						\
		SETFLAGBIT(ZF,(reg_al==0));							\
		SETFLAGBIT(PF,false);								\
		flags.type=t_UNKNOWN;								\
	}



#define MULB(op1,load,save)									\
	flags.type=t_MUL;										\
	reg_ax=reg_al*load(op1);								\
	if (reg_ax & 0xff00) {									\
		SETFLAGBIT(CF,true);SETFLAGBIT(OF,true);			\
	} else {												\
		SETFLAGBIT(CF,false);SETFLAGBIT(OF,false);			\
	}

#define MULW(op1,load,save)									\
{															\
	Bitu tempu=(Bitu)reg_ax*(Bitu)(load(op1));				\
	reg_ax=(Bit16u)(tempu);									\
	reg_dx=(Bit16u)(tempu >> 16);							\
	flags.type=t_MUL;										\
	if (reg_dx) {											\
		SETFLAGBIT(CF,true);SETFLAGBIT(OF,true);			\
	} else {												\
		SETFLAGBIT(CF,false);SETFLAGBIT(OF,false);			\
	}														\
}

#define MULD(op1,load,save)									\
{															\
	Bit64u tempu=(Bit64u)reg_eax*(Bit64u)(load(op1));		\
	reg_eax=(Bit32u)(tempu);								\
	reg_edx=(Bit32u)(tempu >> 32);							\
	flags.type=t_MUL;										\
	if (reg_edx) {											\
		SETFLAGBIT(CF,true);SETFLAGBIT(OF,true);			\
	} else {												\
		SETFLAGBIT(CF,false);SETFLAGBIT(OF,false);			\
	}														\
}

#define DIVB(op1,load,save)									\
{															\
	Bitu val=load(op1);										\
	if (val==0)	EXCEPTION(0);								\
	Bitu quo=reg_ax / val;									\
	reg_ah=(Bit8u)(reg_ax % val);							\
	reg_al=(Bit8u)quo;										\
	if (quo>0xff) EXCEPTION(0);								\
}


#define DIVW(op1,load,save)									\
{															\
	Bitu val=load(op1);										\
	if (val==0)	EXCEPTION(0);								\
	Bitu num=(reg_dx<<16)|reg_ax;							\
	Bitu quo=num/val;										\
	reg_dx=(Bit16u)(num % val);								\
	reg_ax=(Bit16u)quo;										\
	if (quo>0xffff) EXCEPTION(0);							\
}

#define DIVD(op1,load,save)									\
{															\
	Bitu val=load(op1);										\
	if (!val) EXCEPTION(0);									\
	Bit64u num=(((Bit64u)reg_edx)<<32)|reg_eax;				\
	Bit64u quo=num/val;										\
	reg_edx=(Bit32u)(num % val);							\
	reg_eax=(Bit32u)quo;									\
	if (quo!=(Bit64u)reg_eax) EXCEPTION(0);					\
}


#define IDIVB(op1,load,save)								\
{															\
	Bits val=(Bit8s)(load(op1));							\
	if (val==0)	EXCEPTION(0);								\
	Bits quo=((Bit16s)reg_ax) / val;						\
	reg_ah=(Bit8s)(((Bit16s)reg_ax) % val);					\
	reg_al=(Bit8s)quo;										\
	if (quo!=(Bit8s)reg_al) EXCEPTION(0);					\
}


#define IDIVW(op1,load,save)								\
{															\
	Bits val=(Bit16s)(load(op1));							\
	if (!val) EXCEPTION(0);									\
	Bits num=(Bit32s)((reg_dx<<16)|reg_ax);					\
	Bits quo=num/val;										\
	reg_dx=(Bit16u)(num % val);								\
	reg_ax=(Bit16s)quo;										\
	if (quo!=(Bit16s)reg_ax) EXCEPTION(0);					\
}

#define IDIVD(op1,load,save)								\
{															\
	Bits val=(Bit32s)(load(op1));							\
	if (!val) EXCEPTION(0);									\
	Bit64s num=(((Bit64u)reg_edx)<<32)|reg_eax;				\
	Bit64s quo=num/val;										\
	reg_edx=(Bit32s)(num % val);							\
	reg_eax=(Bit32s)(quo);									\
	if (quo!=(Bit64s)((Bit32s)reg_eax)) EXCEPTION(0);		\
}

#define IMULB(op1,load,save)								\
{															\
	flags.type=t_MUL;										\
	reg_ax=((Bit8s)reg_al) * ((Bit8s)(load(op1)));			\
	if ((reg_ax & 0xff80)==0xff80 ||						\
		(reg_ax & 0xff80)==0x0000) {						\
		SETFLAGBIT(CF,false);SETFLAGBIT(OF,false);			\
	} else {												\
		SETFLAGBIT(CF,true);SETFLAGBIT(OF,true);			\
	}														\
}
	

#define IMULW(op1,load,save)								\
{															\
	Bits temps=((Bit16s)reg_ax)*((Bit16s)(load(op1)));		\
	reg_ax=(Bit16s)(temps);									\
	reg_dx=(Bit16s)(temps >> 16);							\
	flags.type=t_MUL;										\
	if (((temps & 0xffff8000)==0xffff8000 ||				\
		(temps & 0xffff8000)==0x0000)) {					\
		SETFLAGBIT(CF,false);SETFLAGBIT(OF,false);			\
	} else {												\
		SETFLAGBIT(CF,true);SETFLAGBIT(OF,true);			\
	}														\
}

#define IMULD(op1,load,save)								\
{															\
	Bit64s temps=((Bit64s)((Bit32s)reg_eax))*				\
				 ((Bit64s)((Bit32s)(load(op1))));			\
	reg_eax=(Bit32u)(temps);								\
	reg_edx=(Bit32u)(temps >> 32);							\
	flags.type=t_MUL;										\
	if ((reg_edx==0xffffffff) &&							\
		(reg_eax & 0x80000000) ) {							\
		SETFLAGBIT(CF,false);SETFLAGBIT(OF,false);			\
	} else if ( (reg_edx==0x00000000) &&					\
				(reg_eax< 0x80000000) ) {					\
		SETFLAGBIT(CF,false);SETFLAGBIT(OF,false);			\
	} else {												\
		SETFLAGBIT(CF,true);SETFLAGBIT(OF,true);			\
	}														\
}

#define DIMULW(op1,op2,op3,load,save)						\
{															\
	Bits res;												\
	res=((Bit16s)op2) * ((Bit16s)op3);						\
	save(op1,res & 0xffff);									\
	flags.type=t_MUL;										\
	if ((res> -32768)  && (res<32767)) {					\
		SETFLAGBIT(CF,false);SETFLAGBIT(OF,false);			\
	} else {												\
		SETFLAGBIT(CF,true);SETFLAGBIT(OF,true);			\
	}														\
}

#define DIMULD(op1,op2,op3,load,save)						\
{															\
	Bit64s res=((Bit64s)((Bit32s)op2))*((Bit64s)((Bit32s)op3));	\
	save(op1,(Bit32s)res);									\
	flags.type=t_MUL;										\
	if ((res>-((Bit64s)(2147483647)+1)) &&					\
		(res<(Bit64s)2147483647)) {							\
		SETFLAGBIT(CF,false);SETFLAGBIT(OF,false);			\
	} else {												\
		SETFLAGBIT(CF,true);SETFLAGBIT(OF,true);			\
	}														\
}

#define GRP2B(blah)											\
{															\
	GetRM;													\
	if (rm >= 0xc0) {										\
		GetEArb;											\
		Bit8u val=blah & 0x1f;								\
		switch (rm&0x38)	{								\
		case 0x00:ROLB(*earb,val,LoadRb,SaveRb);break;		\
		case 0x08:RORB(*earb,val,LoadRb,SaveRb);break;		\
		case 0x10:RCLB(*earb,val,LoadRb,SaveRb);break;		\
		case 0x18:RCRB(*earb,val,LoadRb,SaveRb);break;		\
		case 0x20:/* SHL and SAL are the same */			\
		case 0x30:SHLB(*earb,val,LoadRb,SaveRb);break;		\
		case 0x28:SHRB(*earb,val,LoadRb,SaveRb);break;		\
		case 0x38:SARB(*earb,val,LoadRb,SaveRb);break;		\
		}													\
	} else {												\
		GetEAa;												\
		Bit8u val=blah & 0x1f;								\
		switch (rm & 0x38) {								\
		case 0x00:ROLB(eaa,val,LoadMb,SaveMb);break;		\
		case 0x08:RORB(eaa,val,LoadMb,SaveMb);break;		\
		case 0x10:RCLB(eaa,val,LoadMb,SaveMb);break;		\
		case 0x18:RCRB(eaa,val,LoadMb,SaveMb);break;		\
		case 0x20:/* SHL and SAL are the same */			\
		case 0x30:SHLB(eaa,val,LoadMb,SaveMb);break;		\
		case 0x28:SHRB(eaa,val,LoadMb,SaveMb);break;		\
		case 0x38:SARB(eaa,val,LoadMb,SaveMb);break;		\
		}													\
	}														\
}



#define GRP2W(blah)											\
{															\
	GetRM;													\
	if (rm >= 0xc0) {										\
		GetEArw;											\
		Bit8u val=blah & 0x1f;								\
		switch (rm&0x38)	{								\
		case 0x00:ROLW(*earw,val,LoadRw,SaveRw);break;		\
		case 0x08:RORW(*earw,val,LoadRw,SaveRw);break;		\
		case 0x10:RCLW(*earw,val,LoadRw,SaveRw);break;		\
		case 0x18:RCRW(*earw,val,LoadRw,SaveRw);break;		\
		case 0x20:/* SHL and SAL are the same */			\
		case 0x30:SHLW(*earw,val,LoadRw,SaveRw);break;		\
		case 0x28:SHRW(*earw,val,LoadRw,SaveRw);break;		\
		case 0x38:SARW(*earw,val,LoadRw,SaveRw);break;		\
		}													\
	} else {												\
		GetEAa;												\
		Bit8u val=blah & 0x1f;								\
		switch (rm & 0x38) {								\
		case 0x00:ROLW(eaa,val,LoadMw,SaveMw);break;		\
		case 0x08:RORW(eaa,val,LoadMw,SaveMw);break;		\
		case 0x10:RCLW(eaa,val,LoadMw,SaveMw);break;		\
		case 0x18:RCRW(eaa,val,LoadMw,SaveMw);break;		\
		case 0x20:/* SHL and SAL are the same */			\
		case 0x30:SHLW(eaa,val,LoadMw,SaveMw);break;		\
		case 0x28:SHRW(eaa,val,LoadMw,SaveMw);break;		\
		case 0x38:SARW(eaa,val,LoadMw,SaveMw);break;		\
		}													\
	}														\
}


#define GRP2D(blah)											\
{															\
	GetRM;													\
	if (rm >= 0xc0) {										\
		GetEArd;											\
		Bit8u val=blah & 0x1f;								\
		switch (rm&0x38)	{								\
		case 0x00:ROLD(*eard,val,LoadRd,SaveRd);break;		\
		case 0x08:RORD(*eard,val,LoadRd,SaveRd);break;		\
		case 0x10:RCLD(*eard,val,LoadRd,SaveRd);break;		\
		case 0x18:RCRD(*eard,val,LoadRd,SaveRd);break;		\
		case 0x20:/* SHL and SAL are the same */			\
		case 0x30:SHLD(*eard,val,LoadRd,SaveRd);break;		\
		case 0x28:SHRD(*eard,val,LoadRd,SaveRd);break;		\
		case 0x38:SARD(*eard,val,LoadRd,SaveRd);break;		\
		}													\
	} else {												\
		GetEAa;												\
		Bit8u val=blah & 0x1f;								\
		switch (rm & 0x38) {								\
		case 0x00:ROLD(eaa,val,LoadMd,SaveMd);break;		\
		case 0x08:RORD(eaa,val,LoadMd,SaveMd);break;		\
		case 0x10:RCLD(eaa,val,LoadMd,SaveMd);break;		\
		case 0x18:RCRD(eaa,val,LoadMd,SaveMd);break;		\
		case 0x20:/* SHL and SAL are the same */			\
		case 0x30:SHLD(eaa,val,LoadMd,SaveMd);break;		\
		case 0x28:SHRD(eaa,val,LoadMd,SaveMd);break;		\
		case 0x38:SARD(eaa,val,LoadMd,SaveMd);break;		\
		}													\
	}														\
}

/* let's hope bochs has it correct with the higher than 16 shifts */
/* double-precision shift left has low bits in second argument */
#define DSHLW(op1,op2,op3,load,save)									\
	Bit8u val=op3 & 0x1F;												\
	if (!val) break;													\
	flags.var2.b=val;flags.var1.d=(load(op1)<<16)|op2;					\
	Bit32u tempd=flags.var1.d << flags.var2.b;							\
  	if (flags.var2.b>16) tempd |= (op2 << (flags.var2.b - 16));			\
	flags.result.w=(Bit16u)(tempd >> 16);								\
	save(op1,flags.result.w);											\
	flags.type=t_DSHLw;

#define DSHLD(op1,op2,op3,load,save)									\
	Bit8u val=op3 & 0x1F;												\
	if (!val) break;													\
	flags.var2.b=val;flags.var1.d=load(op1);							\
	flags.result.d=(flags.var1.d << flags.var2.b) | (op2 >> (32-flags.var2.b));	\
	save(op1,flags.result.d);											\
	flags.type=t_DSHLd;

/* double-precision shift right has high bits in second argument */
#define DSHRW(op1,op2,op3,load,save)									\
	Bit8u val=op3 & 0x1F;												\
	if (!val) break;													\
	flags.var2.b=val;flags.var1.d=(op2<<16)|load(op1);					\
	Bit32u tempd=flags.var1.d >> flags.var2.b;							\
  	if (flags.var2.b>16) tempd |= (op2 << (32-flags.var2.b ));			\
	flags.result.w=(Bit16u)(tempd);										\
	save(op1,flags.result.w);											\
	flags.type=t_DSHRw;

#define DSHRD(op1,op2,op3,load,save)									\
	Bit8u val=op3 & 0x1F;												\
	if (!val) break;													\
	flags.var2.b=val;flags.var1.d=load(op1);							\
	flags.result.d=(flags.var1.d >> flags.var2.b) | (op2 << (32-flags.var2.b));	\
	save(op1,flags.result.d);											\
	flags.type=t_DSHRd;

#define BSWAP(op1)														\
	op1 = (op1>>24)|((op1>>8)&0xFF00)|((op1<<8)&0xFF0000)|((op1<<24)&0xFF000000);
