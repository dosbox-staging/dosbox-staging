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


#define SUBIP(a)	core_16.ip_lookup-=a
#define SETIP(a)	core_16.ip_lookup=SegBase(cs)+a
#define GETIP		(Bit16u)(core_16.ip_lookup-SegBase(cs)) 
#define SAVEIP		reg_ip=GETIP
#define LOADIP		core_16.ip_lookup=SegBase(cs)+reg_ip

#define LEAVECORE						\
	SAVEIP;								\
	FILLFLAGS;

static INLINE void ADDIP(Bit16u add) {
	core_16.ip_lookup=SegBase(cs)+((Bit16u)(((Bit16u)(core_16.ip_lookup-SegBase(cs)))+(Bit16u)add));
}

static INLINE void ADDIPFAST(Bit16s blah) {
	core_16.ip_lookup+=blah;
}

#define CheckTF()											\
	if (GETFLAG(TF)) {										\
		cpudecoder=CPU_Real_16_Slow_Decode_Trap;			\
		goto decode_end;									\
	}


#define EXCEPTION(blah)											\
	{															\
		Bit8u new_num=blah;										\
		core_16.ip_lookup=core_16.ip_start;						\
		LEAVECORE;												\
		if (Interrupt(new_num)) {								\
			if (GETFLAG(TF)) {									\
				cpudecoder=CPU_Real_16_Slow_Decode_Trap;		\
				return CBRET_NONE;								\
			}													\
			goto decode_start;									\
		} else return CBRET_NONE;								\
	}

static INLINE Bit8u Fetchb() {
	Bit8u temp=LoadMb(core_16.ip_lookup);
	core_16.ip_lookup+=1;
	return temp;
}

static INLINE Bit16u Fetchw() {
	Bit16u temp=LoadMw(core_16.ip_lookup);
	core_16.ip_lookup+=2;
	return temp;
}
static INLINE Bit32u Fetchd() {
	Bit32u temp=LoadMd(core_16.ip_lookup);
	core_16.ip_lookup+=4;
	return temp;
}

static INLINE Bit8s Fetchbs() {
	return Fetchb();
}
static INLINE Bit16s Fetchws() {
	return Fetchw();
}

static INLINE Bit32s Fetchds() {
	return Fetchd();
}

static INLINE void Push_16(Bit16u blah)	{
	reg_sp-=2;
	SaveMw(SegBase(ss)+reg_sp,blah);
};

static INLINE void Push_32(Bit32u blah)	{
	reg_sp-=4;
	SaveMd(SegBase(ss)+reg_sp,blah);
};

static INLINE Bit16u Pop_16() {
	Bit16u temp=LoadMw(SegBase(ss)+reg_sp);
	reg_sp+=2;
	return temp;
};

static INLINE Bit32u Pop_32() {
	Bit32u temp=LoadMd(SegBase(ss)+reg_sp);
	reg_sp+=4;
	return temp;
};

#define JumpSIb(blah) 										\
	if (blah) {												\
		ADDIPFAST(Fetchbs());								\
	} else {												\
		ADDIPFAST(1);										\
	}					

#define JumpSIw(blah) 										\
	if (blah) {												\
		ADDIPFAST(Fetchws());								\
	} else {												\
		ADDIPFAST(2);										\
	}						

#define SETcc(cc)											\
	{														\
		GetRM;												\
		if (rm >= 0xc0 ) {GetEArb;*earb=(cc) ? 1 : 0;}		\
		else {GetEAa;SaveMb(eaa,(cc) ? 1 : 0);}				\
	}

#define NOTDONE												\
	SUBIP(1);E_Exit("CPU:Opcode %2X Unhandled",Fetchb()); 

#define NOTDONE66											\
	SUBIP(1);E_Exit("CPU:Opcode 66:%2X Unhandled",Fetchb()); 


#define stringDI											\
	PhysPt to;												\
	to=SegBase(es)+reg_di							

#define stringSI											\
	PhysPt from;											\
	if (core_16.prefixes & PREFIX_SEG) {					\
		from=(core_16.segbase+reg_si);						\
	} else {												\
		from=SegBase(ds)+reg_si;							\
	}

#include "helpers.h"
#include "table_ea.h"
#include "../modrm.h"

static void Repeat_Normal(bool testz,bool prefix_66) {
	
	PhysPt base_si,base_di;

	Bit16s direct;
	if (GETFLAG(DF)) direct=-1;
	else direct=1;
	base_di=SegBase(es);
	if (core_16.prefixes & PREFIX_ADDR) E_Exit("Unhandled 0x67 prefixed string op");
rep_again:	
	if (core_16.prefixes & PREFIX_SEG) {
		base_si=(core_16.segbase);
	} else {
		base_si=SegBase(ds);
	}
	switch (Fetchb()) {
	case 0x26:			/* ES Prefix */
		core_16.segbase=SegBase(es);
		core_16.prefixes|=PREFIX_SEG;
		goto rep_again;
	case 0x2e:			/* CS Prefix */
		core_16.segbase=SegBase(cs);
		core_16.prefixes|=PREFIX_SEG;
		goto rep_again;
	case 0x36:			/* SS Prefix */
		core_16.segbase=SegBase(ss);
		core_16.prefixes|=PREFIX_SEG;
		goto rep_again;
	case 0x3e:			/* DS Prefix */
		core_16.segbase=SegBase(ds);
		core_16.prefixes|=PREFIX_SEG;
		goto rep_again;
	case 0x64:			/* FS Prefix */
		core_16.segbase=SegBase(fs);
		core_16.prefixes|=PREFIX_SEG;
		goto rep_again;
	case 0x65:			/* GS Prefix */
		core_16.segbase=SegBase(gs);
		core_16.prefixes|=PREFIX_SEG;
		goto rep_again;
	case 0x66:			/* Size Prefix */
		prefix_66=!prefix_66;
		goto rep_again;
	case 0x6c:			/* REP INSB */
		for (;CPU_Cycles>0;CPU_Cycles--) {
			if (!reg_cx) goto normalexit; reg_cx--;
			SaveMb(base_di+reg_di,IO_Read(reg_dx));
			reg_di+=direct;
		}
		break;
	case 0x6d:			/* REP INSW/D */
		if (prefix_66) {
			direct*=4;
			for (;CPU_Cycles>0;CPU_Cycles--) {
				if (!reg_cx) goto normalexit; reg_cx--;
				SaveMb(base_di+reg_di+0,IO_Read(reg_dx+0));
				SaveMb(base_di+reg_di+1,IO_Read(reg_dx+1));
				SaveMb(base_di+reg_di+2,IO_Read(reg_dx+2));
				SaveMb(base_di+reg_di+3,IO_Read(reg_dx+3));
				reg_di+=direct;
			}
		} else {
			direct*=2;
			for (;CPU_Cycles>0;CPU_Cycles--) {
				if (!reg_cx) goto normalexit; reg_cx--;
				SaveMb(base_di+reg_di+0,IO_Read(reg_dx+0));
				SaveMb(base_di+reg_di+1,IO_Read(reg_dx+1));
				reg_di+=direct;
			}
		}
		break;
	case 0x6e:			/* REP OUTSB */
		for (;CPU_Cycles>0;CPU_Cycles--) {
			if (!reg_cx) goto normalexit; reg_cx--;
			IO_Write(reg_dx,LoadMb(base_si+reg_si));
			reg_si+=direct;
		}	
		break;
	case 0x6f:			/* REP OUTSW/D */
		if (prefix_66) {
			direct*=4;
			for (;CPU_Cycles>0;CPU_Cycles--) {
				if (!reg_cx) goto normalexit; reg_cx--;
				IO_Write(reg_dx+0,LoadMb(base_si+reg_si+0));
				IO_Write(reg_dx+1,LoadMb(base_si+reg_si+1));
				IO_Write(reg_dx+2,LoadMb(base_si+reg_si+2));
				IO_Write(reg_dx+3,LoadMb(base_si+reg_si+3));
				reg_si+=direct;
			}
		} else {
			direct*=2;
			for (;CPU_Cycles>0;CPU_Cycles--) {
				if (!reg_cx) goto normalexit; reg_cx--;
				IO_Write(reg_dx+0,LoadMb(base_si+reg_si+0));
				IO_Write(reg_dx+1,LoadMb(base_si+reg_si+1));
				reg_si+=direct;
			}
		}
		break;
	case 0xa4:			/* REP MOVSB */
		for (;CPU_Cycles>0;CPU_Cycles--) {
			if (!reg_cx) goto normalexit; reg_cx--;
			SaveMb(base_di+reg_di,LoadMb(base_si+reg_si));
			reg_si+=direct;reg_di+=direct;
		}
		break;
	case 0xa5:			/* REP MOVSW/D */
		if (prefix_66) {
			direct*=4;
			for (;CPU_Cycles>0;CPU_Cycles--) {
				if (!reg_cx) goto normalexit; reg_cx--;
				SaveMd(base_di+reg_di,LoadMd(base_si+reg_si));
				reg_si+=direct;reg_di+=direct;
			} 
		} else {
			direct*=2;
			for (;CPU_Cycles>0;CPU_Cycles--) {
				if (!reg_cx) goto normalexit; reg_cx--;
				SaveMw(base_di+reg_di,LoadMw(base_si+reg_si));
				reg_si+=direct;reg_di+=direct;
			}
		}
		break;
	case 0xa6:			/* REP CMPSB */
		{	
			Bit8u op1,op2;
			if (!reg_cx) { CPU_Cycles--;goto normalexit; }
			for (;CPU_Cycles>0;CPU_Cycles--) {
				op1=LoadMb(base_si+reg_si);op2=LoadMb(base_di+reg_di);
				reg_cx--;reg_si+=direct;reg_di+=direct;
				if ((op1==op2)!=testz || !reg_cx) { CMPB(op1,op2,LoadRb,0);goto normalexit; }
			}
			CMPB(op1,op2,LoadRb,0);
		}
		break;
	case 0xa7:			/* REP CMPSW */
		{	
			if (!reg_cx) { CPU_Cycles--;goto normalexit; }
			if (prefix_66) {
				direct*=4;Bit32u op1,op2;
				for (;CPU_Cycles>0;CPU_Cycles--) {
					op1=LoadMd(base_si+reg_si);op2=LoadMd(base_di+reg_di);
					reg_cx--;reg_si+=direct;reg_di+=direct;
					if ((op1==op2)!=testz || !reg_cx) { CMPD(op1,op2,LoadRd,0);goto normalexit; }
				}
				CMPD(op1,op2,LoadRd,0);
			} else {
				direct*=2;Bit16u op1,op2;
				for (;CPU_Cycles>0;CPU_Cycles--) {
					op1=LoadMw(base_si+reg_si);op2=LoadMw(base_di+reg_di);
					reg_cx--,reg_si+=direct;reg_di+=direct;
					if ((op1==op2)!=testz || !reg_cx) { CMPW(op1,op2,LoadRw,0);goto normalexit; }
				}
				CMPW(op1,op2,LoadRw,0);
			}
		}
		break;
	case 0xaa:			/* REP STOSB */
		for (;CPU_Cycles>0;CPU_Cycles--) {
			if (!reg_cx) goto normalexit; reg_cx--;
			SaveMb(base_di+reg_di,reg_al);
			reg_di+=direct;
		}
		break;
	case 0xab:			/* REP STOSW */
		if (prefix_66) {
			direct*=4;	
			for (;CPU_Cycles>0;CPU_Cycles--) {
				if (!reg_cx) goto normalexit; reg_cx--;
				SaveMd(base_di+reg_di,reg_eax);
				reg_di+=direct;
			}
		} else {
			direct*=2;	
			for (;CPU_Cycles>0;CPU_Cycles--) {
				if (!reg_cx) goto normalexit; reg_cx--;
				SaveMw(base_di+reg_di,reg_ax);
				reg_di+=direct;
			}
		}
		break;
	case 0xac:			/* REP LODSB */
		for (;CPU_Cycles>0;CPU_Cycles--) {
			if (!reg_cx) goto normalexit; reg_cx--;
			reg_al=LoadMb(base_si+reg_si);
			reg_si+=direct;
		}	
		break;
	case 0xad:			/* REP LODSW */
		if (prefix_66) {
			direct*=4;
			for (;CPU_Cycles>0;CPU_Cycles--) {
				if (!reg_cx) goto normalexit; reg_cx--;
				reg_eax=LoadMd(base_si+reg_si);
				reg_si+=direct;
			}	
		} else {
			direct*=2;
			for (;CPU_Cycles>0;CPU_Cycles--) {
				if (!reg_cx) goto normalexit; reg_cx--;
				reg_ax=LoadMw(base_si+reg_si);
				reg_si+=direct;
			}	
		}
		break;
	case 0xae:			/* REP SCASB */
		{	
			Bit8u op2;
			if (!reg_cx) { CPU_Cycles--;goto normalexit; }
			for (;CPU_Cycles>0;CPU_Cycles--) {
				op2=LoadMb(base_di+reg_di);
				reg_cx--;reg_di+=direct;
				if ((reg_al==op2)!=testz || !reg_cx) { CMPB(reg_al,op2,LoadRb,0);goto normalexit; }
			}
			CMPB(reg_al,op2,LoadRb,0);
		}
		break;
	case 0xaf:			/* REP SCASW */
		{	
			if (!reg_cx) { CPU_Cycles--;goto normalexit; }
			if (prefix_66) {
				direct*=4;Bit32u op2;
				for (;CPU_Cycles>0;CPU_Cycles--) {
					op2=LoadMd(base_di+reg_di);
					reg_cx--;reg_di+=direct;
					if ((reg_eax==op2)!=testz || !reg_cx) { CMPD(reg_eax,op2,LoadRd,0);goto normalexit; }
				}
				CMPD(reg_eax,op2,LoadRd,0);
			} else {
				direct*=2;Bit16u op2;
				for (;CPU_Cycles>0;CPU_Cycles--) {
					op2=LoadMw(base_di+reg_di);
					reg_cx--;reg_di+=direct;
					if ((reg_ax==op2)!=testz || !reg_cx) { CMPW(reg_ax,op2,LoadRw,0);goto normalexit; }
				}
				CMPW(reg_ax,op2,LoadRw,0);
			}
		}
		break;
	default:
		core_16.ip_lookup--;
		LOG(LOG_CPU,LOG_ERROR)("Unhandled REP Prefix %X",Fetchb());
		goto normalexit;
	}
	/* If we end up here it's because the CPU_Cycles counter is 0, so restart instruction */
	core_16.ip_lookup=core_16.ip_start;
normalexit:;
}

