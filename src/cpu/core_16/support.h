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

EAPoint IPPoint;

#define SUBIP(a)	IPPoint-=a
#define SETIP(a)	IPPoint=SegBase(cs)+a
#define GETIP		(Bit16u)(IPPoint-SegBase(cs)) 
#define SAVEIP		reg_ip=GETIP
#define LOADIP		IPPoint=SegBase(cs)+reg_ip

static INLINE void ADDIP(Bit16u add) {
	IPPoint=SegBase(cs)+((Bit16u)(((Bit16u)(IPPoint-SegBase(cs)))+(Bit16u)add));
}

static INLINE void ADDIPFAST(Bit16s blah) {
	IPPoint+=blah;
}

static INLINE Bit8u Fetchb() {
	Bit8u temp=LoadMb(IPPoint);
	IPPoint+=1;
	return temp;
}

static INLINE Bit16u Fetchw() {
	Bit16u temp=LoadMw(IPPoint);
	IPPoint+=2;
	return temp;
}
static INLINE Bit32u Fetchd() {
	Bit32u temp=LoadMd(IPPoint);
	IPPoint+=4;
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

#define stringDI											\
	EAPoint to;												\
	to=SegBase(es)+reg_di							

#define stringSI											\
	EAPoint from;											\
	if (prefix.mark & PREFIX_SEG) {										\
		from=(prefix.segbase+reg_si);						\
		PrefixReset;										\
	} else {												\
		from=SegBase(ds)+reg_si;							\
	}


#include "helpers.h"
#include "table_ea.h"
#include "../modrm.h"
#include "instructions.h"


static Bits Repeat_Normal(bool testz,bool prefix_66,Bits count_remain) {
	
	Bits count=count_remain;
	PhysPt base_si,base_di;

	Bit16s direct;
	if (flags.df) direct=-1;
	else direct=1;
	base_di=SegBase(es);
rep_again:	
	if (prefix.mark & PREFIX_SEG) {
		base_si=(prefix.segbase);
	} else {
		base_si=SegBase(ds);
	}
	switch (Fetchb()) {
	case 0x26:			/* ES Prefix */
		prefix.segbase=SegBase(es);
		prefix.mark|=PREFIX_SEG;
		prefix.count++;
		goto rep_again;
	case 0x2e:			/* CS Prefix */
		prefix.segbase=SegBase(cs);
		prefix.mark|=PREFIX_SEG;
		prefix.count++;
		goto rep_again;
	case 0x36:			/* SS Prefix */
		prefix.segbase=SegBase(ss);
		prefix.mark|=PREFIX_SEG;
		prefix.count++;
		goto rep_again;
	case 0x3e:			/* DS Prefix */
		prefix.segbase=SegBase(ds);
		prefix.mark|=PREFIX_SEG;
		prefix.count++;
		goto rep_again;
	case 0x66:			/* Size Prefix */
		prefix.count++;
		prefix_66=!prefix_66;
		goto rep_again;
	case 0x6c:			/* REP INSB */
		for (;(reg_cx && count>0);reg_cx--,count--) {
			SaveMb(base_di+reg_di,IO_Read(reg_dx));
			reg_di+=direct;
		}
		if (reg_cx && count<=0) goto countzero;
		break;
	case 0x6d:			/* REP INSW/D */
		if (prefix_66) {
			direct*=4;
			for (;(reg_cx && count>0);reg_cx--,count--) {
				SaveMb(base_di+reg_di+0,IO_Read(reg_dx+0));
				SaveMb(base_di+reg_di+1,IO_Read(reg_dx+1));
				SaveMb(base_di+reg_di+2,IO_Read(reg_dx+2));
				SaveMb(base_di+reg_di+3,IO_Read(reg_dx+3));
				reg_di+=direct;
			}
		} else {
			direct*=2;
			for (;(reg_cx && count>0);reg_cx--,count--) {
				SaveMb(base_di+reg_di+0,IO_Read(reg_dx+0));
				SaveMb(base_di+reg_di+1,IO_Read(reg_dx+1));
				reg_di+=direct;
			}
		}
		if (reg_cx && count<=0) goto countzero;
		break;
	case 0x6e:			/* REP OUTSB */
		for (;(reg_cx && count>0);reg_cx--,count--) {
			IO_Write(reg_dx,LoadMb(base_si+reg_si));
			reg_si+=direct;
		}	
		if (reg_cx && count<=0) goto countzero;
		break;
	case 0x6f:			/* REP OUTSW/D */
		if (prefix_66) {
			direct*=4;
			for (;(reg_cx && count>0);reg_cx--,count--) {
				IO_Write(reg_dx+0,LoadMb(base_si+reg_si+0));
				IO_Write(reg_dx+1,LoadMb(base_si+reg_si+1));
				IO_Write(reg_dx+2,LoadMb(base_si+reg_si+2));
				IO_Write(reg_dx+3,LoadMb(base_si+reg_si+3));
				reg_si+=direct;
			}
		} else {
			direct*=2;
			for (;(reg_cx && count>0);reg_cx--,count--) {
				IO_Write(reg_dx+0,LoadMb(base_si+reg_si+0));
				IO_Write(reg_dx+1,LoadMb(base_si+reg_si+1));
				reg_si+=direct;
			}
		}
		if (reg_cx && count<=0) goto countzero;
		break;
	case 0xa4:			/* REP MOVSB */
		for (;(reg_cx && count>0);reg_cx--,count--) {
			SaveMb(base_di+reg_di,LoadMb(base_si+reg_si));
			reg_si+=direct;reg_di+=direct;
		}
		if (reg_cx && count<=0) goto countzero;
		break;
	case 0xa5:			/* REP MOVSW/D */
		if (prefix_66) {
			direct*=4;
			for (;(reg_cx && count>0);reg_cx--,count--) {
				SaveMd(base_di+reg_di,LoadMd(base_si+reg_si));
				reg_si+=direct;reg_di+=direct;
			} 
		} else {
			direct*=2;
			for (;(reg_cx && count>0);reg_cx--,count--) {
				SaveMw(base_di+reg_di,LoadMw(base_si+reg_si));
				reg_si+=direct;reg_di+=direct;
			}
		}
		if (reg_cx && count<=0) goto countzero;
		break;
	case 0xa6:			/* REP CMPSB */
		{	
			if (!reg_cx) goto stopit;Bit8u op1,op2;
			for (;(reg_cx && count>0);) {
				op1=LoadMb(base_si+reg_si);op2=LoadMb(base_di+reg_di);
				reg_cx--,count--;reg_si+=direct;reg_di+=direct;
				if ((op1==op2)!=testz) break;
			}
			CMPB(op1,op2,LoadRb,0);
			if ((op1==op2)!=testz) goto stopit;
			if (reg_cx && count<=0) goto countzero;
		}
		break;
	case 0xa7:			/* REP CMPSW */
		{	
			if (!reg_cx) goto stopit;
			if (prefix_66) {
				direct*=4;	
				Bit32u op1,op2;
				for (;(reg_cx && count>0);) {
					op1=LoadMd(base_si+reg_si);op2=LoadMd(base_di+reg_di);
					reg_cx--,count--;reg_si+=direct;reg_di+=direct;
					if ((op1==op2)!=testz) break;
				}
				CMPD(op1,op2,LoadRd,0);
				if ((op1==op2)!=testz) goto stopit;
			} else {
				direct*=2;
				Bit16u op1,op2;
				for (;(reg_cx && count>0);) {
					op1=LoadMw(base_si+reg_si);op2=LoadMw(base_di+reg_di);
					reg_cx--,count--;reg_si+=direct;reg_di+=direct;
					if ((op1==op2)!=testz) break;
				}
				CMPW(op1,op2,LoadRw,0);
				if ((op1==op2)!=testz) goto stopit;
			}
			if (reg_cx && count<=0) goto countzero;
		}
		break;
	case 0xaa:			/* REP STOSB */
		for (;(reg_cx && count>0);reg_cx--,count--) {
			SaveMb(base_di+reg_di,reg_al);
			reg_di+=direct;
		}
		if (reg_cx && count<=0) goto countzero;
		break;
	case 0xab:			/* REP STOSW */
		if (prefix_66) {
			direct*=4;	
			for (;(reg_cx && count>0);reg_cx--,count--) {
				SaveMd(base_di+reg_di,reg_eax);
				reg_di+=direct;
			}
		} else {
			direct*=2;	
			for (;(reg_cx && count>0);reg_cx--,count--) {
				SaveMw(base_di+reg_di,reg_ax);
				reg_di+=direct;
			}
		}
		if (reg_cx && count<=0) goto countzero;
		break;
	case 0xac:			/* REP LODSB */
		for (;(reg_cx && count>0);reg_cx--,count--) {
			reg_al=LoadMb(base_si+reg_si);
			reg_si+=direct;
		}	
		if (reg_cx && count<=0) goto countzero;
		break;
	case 0xad:			/* REP LODSW */
		if (prefix_66) {
			direct*=4;
			for (;(reg_cx && count>0);reg_cx--,count--) {
				reg_eax=LoadMd(base_si+reg_si);
				reg_si+=direct;
			}	
		} else {
			direct*=2;
			for (;(reg_cx && count>0);reg_cx--,count--) {
				reg_ax=LoadMw(base_si+reg_si);
				reg_si+=direct;
			}	
		}
		if (reg_cx && count<=0) goto countzero;
		break;
	case 0xae:			/* REP SCASB */
		{	
			if (!reg_cx) goto stopit;Bit8u op2;
			for (;(reg_cx && count>0);) {
				op2=LoadMb(base_di+reg_di);
				reg_cx--,count--;reg_di+=direct;
				if ((reg_al==op2)!=testz) break;
			}
			CMPB(reg_al,op2,LoadRb,0);
			if ((reg_al==op2)!=testz) goto stopit;
			if (reg_cx && count<=0) goto countzero;
		}
		break;
	case 0xaf:			/* REP SCASW */
		{	
			if (!reg_cx) goto stopit;
			if (prefix_66) {
				direct*=4;	
				Bit32u op2;
				for (;(reg_cx && count>0);) {
					op2=LoadMd(base_di+reg_di);
					reg_cx--,count--;reg_di+=direct;
					if ((reg_eax==op2)!=testz) break;
				}
				CMPD(reg_eax,op2,LoadRd,0);
				if ((reg_eax==op2)!=testz) goto stopit;
			} else {
				direct*=2;
				Bit16u op2;
				for (;(reg_cx && count>0);) {
					op2=LoadMw(base_di+reg_di);
					reg_cx--,count--;reg_di+=direct;
					if ((reg_ax==op2)!=testz) break;
				}
				CMPW(reg_ax,op2,LoadRw,0);
				if ((reg_ax==op2)!=testz) goto stopit;
			}
			if (reg_cx && count<=0) goto countzero;
		}
		break;
	default:
		IPPoint--;
		LOG_DEBUG("Unhandled REP Prefix %X",Fetchb());

	}
stopit:
	PrefixReset;
	return count_remain-count;
countzero:
	IPPoint-=(prefix.count+2);		/* Rep instruction and whatever string instruction */
	PrefixReset;
	return count_remain;
}

//flags.io and nt shouldn't be compiled for 386 
#ifdef CPU_386
#define Save_Flagsw(FLAGW)											\
{																	\
	flags.type=t_UNKNOWN;											\
	flags.cf	=(FLAGW & 0x001)>0;flags.pf	=(FLAGW & 0x004)>0;		\
	flags.af	=(FLAGW & 0x010)>0;flags.zf	=(FLAGW & 0x040)>0;		\
	flags.sf	=(FLAGW & 0x080)>0;flags.tf	=(FLAGW & 0x100)>0;		\
	flags.intf	=(FLAGW & 0x200)>0;									\
	flags.df	=(FLAGW & 0x400)>0;flags.of	=(FLAGW & 0x800)>0;		\
	flags.io	=(FLAGW >> 12) & 0x03;								\
	flags.nt	=(FLAGW & 0x4000)>0;								\
	if (flags.intf && PIC_IRQCheck) {								\
		SAVEIP;														\
		PIC_runIRQs();												\
		LOADIP;														\
	}																\
	if (flags.tf)	LOG_DEBUG("CPU:Trap Flag not supported");			\
}

#else 

#define Save_Flagsw(FLAGW)											\
{																	\
	flags.type=t_UNKNOWN;											\
	flags.cf	=(FLAGW & 0x001)>0;flags.pf	=(FLAGW & 0x004)>0;		\
	flags.af	=(FLAGW & 0x010)>0;flags.zf	=(FLAGW & 0x040)>0;		\
	flags.sf	=(FLAGW & 0x080)>0;flags.tf	=(FLAGW & 0x100)>0;		\
	flags.intf	=(FLAGW & 0x200)>0;									\
	flags.df	=(FLAGW & 0x400)>0;flags.of	=(FLAGW & 0x800)>0;		\
	if (flags.intf && PIC_IRQCheck) {								\
		SAVEIP;														\
		PIC_runIRQs();												\
		LOADIP;														\
	}																\
	if (flags.tf)	LOG_DEBUG("CPU:Trap Flag not supported");			\
}

#endif
