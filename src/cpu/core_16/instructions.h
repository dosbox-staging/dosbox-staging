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

/* 
	Could perhaps do some things with 8 and 16 bit operations like shifts, doing them in 32 bit regs
*/ 

#define JumpSIb(blah) 										\
	if (blah) {												\
		ADDIPFAST(Fetchbs());									\
	} else {												\
		ADDIPFAST(1);											\
	}					

#define JumpSIw(blah) 										\
	if (blah) {												\
		ADDIPFAST(Fetchws());									\
	} else {												\
		ADDIPFAST(2);											\
	}						

#define SETcc(cc)															\
	{																		\
		GetRM;																\
		if (rm >= 0xc0 ) {GetEArb;*earb=(cc) ? 1 : 0;}						\
		else {GetEAa;SaveMb(eaa,(cc) ? 1 : 0);}								\
	}

#define INTERRUPT(blah)										\
	{														\
		Bit8u new_num=blah;									\
		SAVEIP;												\
		Interrupt(new_num);									\
		LOADIP;												\
	}



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

#define NOTDONE												\
	SUBIP(1);E_Exit("CPU:Opcode %2X Unhandled",Fetchb()); 

#define NOTDONE66											\
	SUBIP(1);E_Exit("CPU:Opcode 66:%2X Unhandled",Fetchb()); 


//TODO Maybe make this into a bigger split up because of the rm >=0xc0 this seems make it a bit slower
//TODO set Zero and Sign flag in one run 	
#define ROLB(op1,op2,load,save)								\
	if (!op2) break;									\
	flags.zf=get_ZF();flags.sf=get_SF();flags.af=get_AF();	\
	flags.var2.b=op2&0x07;flags.var1.b=load(op1);			\
	if (!flags.var2.b)	{									\
		flags.result.b=flags.var1.b;						\
	} else {												\
		flags.result.b=(flags.var1.b << flags.var2.b) |		\
				(flags.var1.b >> (8-flags.var2.b));			\
	}														\
	save(op1,flags.result.b);								\
	flags.type=t_ROLb;

#define ROLW(op1,op2,load,save)								\
	if (!op2) break;									\
	flags.zf=get_ZF();flags.sf=get_SF();flags.af=get_AF();	\
	flags.var2.b=op2&0x0F;flags.var1.w=load(op1);			\
	if (!flags.var2.b)	{									\
		flags.result.w=flags.var1.w;						\
	} else {												\
		flags.result.w=(flags.var1.w << flags.var2.b) |		\
				(flags.var1.w >> (16-flags.var2.b));		\
	}														\
	save(op1,flags.result.w);								\
	flags.type=t_ROLw;

#define ROLD(op1,op2,load,save)								\
	if (!op2) break;									\
	flags.zf=get_ZF();flags.sf=get_SF();flags.af=get_AF();	\
	flags.var2.b=op2;flags.var1.d=load(op1);			\
	flags.result.d=(flags.var1.d << flags.var2.b) |			\
				(flags.var1.d >> (32-flags.var2.b));		\
	save(op1,flags.result.d);								\
	flags.type=t_ROLd;


#define RORB(op1,op2,load,save)								\
	if (!op2) break;									\
	flags.zf=get_ZF();flags.sf=get_SF();flags.af=get_AF();	\
	flags.var2.b=op2&0x07;flags.var1.b=load(op1);			\
	if (!flags.var2.b)	{									\
		flags.result.b=flags.var1.b;						\
	} else {												\
		flags.result.b=(flags.var1.b >> flags.var2.b) |		\
				(flags.var1.b << (8-flags.var2.b));			\
	}														\
	save(op1,flags.result.b);								\
	flags.type=t_RORb;

#define RORW(op1,op2,load,save)								\
	if (!op2) break;									\
	flags.zf=get_ZF();flags.sf=get_SF();flags.af=get_AF();	\
	flags.var2.b=op2&0x0F;flags.var1.w=load(op1);			\
	if (!flags.var2.b)	{									\
		flags.result.w=flags.var1.w;						\
	} else {												\
		flags.result.w=(flags.var1.w >> flags.var2.b) |		\
				(flags.var1.w << (16-flags.var2.b));		\
	}														\
	save(op1,flags.result.w);								\
	flags.type=t_RORw;

#define RORD(op1,op2,load,save)								\
	if (!op2) break;									\
	flags.zf=get_ZF();flags.sf=get_SF();flags.af=get_AF();	\
	flags.var2.b=op2;flags.var1.d=load(op1);			\
	flags.result.d=(flags.var1.d >> flags.var2.b) |			\
				(flags.var1.d << (32-flags.var2.b));		\
	save(op1,flags.result.d);								\
	flags.type=t_RORd;


/*	flags.oldcf=get_CF();*/									\

#define RCLB(op1,op2,load,save)								\
	if (!op2) break;								\
	flags.zf=get_ZF();flags.sf=get_SF();flags.af=get_AF();	\
	flags.cf=get_CF();flags.type=t_RCLb;					\
	flags.var2.b=op2%9;flags.var1.b=load(op1);		\
	if (!flags.var2.b)	{									\
		flags.result.b=flags.var1.b;						\
	} else {												\
		flags.result.b=(flags.var1.b << flags.var2.b) |		\
				(flags.cf << (flags.var2.b-1)) |			\
				(flags.var1.b >> (9-flags.var2.b));			\
		flags.cf=((flags.var1.b >> (8-flags.var2.b)) & 1);	\
	}														\
	flags.of=((flags.result.b & 0x80) ^ (flags.cf ? 0x80 : 0)) != 0;	\
 	save(op1,flags.result.b);

#define RCLW(op1,op2,load,save)								\
	if (!op2) break;								\
	flags.zf=get_ZF();flags.sf=get_SF();flags.af=get_AF();	\
	flags.cf=get_CF();flags.type=t_RCLw;					\
	flags.var2.b=op2%17;flags.var1.w=load(op1);	\
	if (!flags.var2.b)	{									\
		flags.result.w=flags.var1.w;						\
	} else {												\
		flags.result.w=(flags.var1.w << flags.var2.b) |		\
				(flags.cf << (flags.var2.b-1)) |			\
				(flags.var1.w >> (17-flags.var2.b));		\
		flags.cf=((flags.var1.w >> (16-flags.var2.b)) & 1);	\
	}														\
	flags.of=((flags.result.w & 0x8000) ^ (flags.cf ? 0x8000 : 0)) != 0;	\
	save(op1,flags.result.w);

#define RCLD(op1,op2,load,save)								\
	if (!op2) break;								\
	flags.zf=get_ZF();flags.sf=get_SF();flags.af=get_AF();	\
	flags.cf=get_CF();flags.type=t_RCLd;					\
	flags.var2.b=op2;flags.var1.d=load(op1);			\
	if (flags.var2.b==1)	{								\
	flags.result.d=(flags.var1.d << 1) | flags.cf;			\
	} else 	{												\
		flags.result.d=(flags.var1.d << flags.var2.b) |		\
				(flags.cf  << (flags.var2.b-1)) |			\
				(flags.var1.d >> (33-flags.var2.b));		\
	}														\
	flags.cf=((flags.var1.d >> (32-flags.var2.b)) & 1);	\
	flags.of=((flags.result.d & 0x80000000) ^ (flags.cf ? 0x80000000 : 0)) != 0;	\
	save(op1,flags.result.d);

#define RCRB(op1,op2,load,save)								\
	if (!op2) break; 								\
	flags.zf=get_ZF();flags.sf=get_SF();flags.af=get_AF();	\
	flags.cf=get_CF();flags.type=t_RCRb;					\
	flags.var2.b=op2%9;flags.var1.b=load(op1);		\
	if (!flags.var2.b)	{									\
		flags.result.b=flags.var1.b;						\
	} else {												\
	 	flags.result.b=(flags.var1.b >> flags.var2.b) |		\
				(flags.cf << (8-flags.var2.b)) |			\
				(flags.var1.b << (9-flags.var2.b));			\
	}														\
	save(op1,flags.result.b);

#define RCRW(op1,op2,load,save)								\
	if (!op2) break;								\
	flags.zf=get_ZF();flags.sf=get_SF();flags.af=get_AF();	\
	flags.cf=get_CF();flags.type=t_RCRw;					\
	flags.var2.b=op2%17;flags.var1.w=load(op1);	\
	if (!flags.var2.b)	{									\
		flags.result.w=flags.var1.w;						\
	} else {												\
	 	flags.result.w=(flags.var1.w >> flags.var2.b) |		\
				(flags.cf << (16-flags.var2.b)) |			\
				(flags.var1.w << (17-flags.var2.b));		\
	}														\
	save(op1,flags.result.w);

#define RCRD(op1,op2,load,save)								\
	if (!op2) break;								\
	flags.zf=get_ZF();flags.sf=get_SF();flags.af=get_AF();	\
	flags.cf=get_CF();flags.type=t_RCRd;					\
	flags.var2.b=op2;flags.var1.d=load(op1);			\
	if (flags.var2.b==1) {									\
		flags.result.d=flags.var1.d >> 1 | flags.cf << 31;	\
	} else {												\
 		flags.result.d=(flags.var1.d >> flags.var2.b) |		\
				(flags.cf << (32-flags.var2.b)) |			\
				(flags.var1.d << (33-flags.var2.b));		\
	}														\
	save(op1,flags.result.d);


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
