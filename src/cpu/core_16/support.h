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
	if (segprefix_on) {										\
		from=(segprefix_base+reg_si);						\
		SegPrefixReset;										\
	} else {												\
		from=SegBase(ds)+reg_si;							\
	}


#include "helpers.h"
#include "table_ea.h"
#include "../modrm.h"
#include "instructions.h"


static INLINE void Rep_66(Bit16s direct,EAPoint from,EAPoint to) {
	bool again;
	do {
		again=false;
		Bit8u repcode=Fetchb();
		switch (repcode) {
		case 0x26:			/* ES Prefix */
			again=true;
			from=SegBase(es);
			break;
		case 0x2e:			/* CS Prefix */
			again=true;
			from=SegBase(cs);
			break;
		case 0x36:			/* SS Prefix */
			again=true;
			from=SegBase(ss);
			break;
		case 0x3e:			/* DS Prefix */
			again=true;
			from=SegBase(ds);
			break;
		case 0xa5:			/* REP MOVSD */
			for (;reg_cx>0;reg_cx--) {
				SaveMd(to+reg_di,LoadMd(from+reg_si));
				reg_di+=direct*4;
				reg_si+=direct*4;
			}	
			break;
		case 0xab:			/* REP STOSW */
			for (;reg_cx>0;reg_cx--) {
				SaveMd(to+reg_di,reg_eax);
				reg_di+=direct*4;
			}	
			break;
		default:
			E_Exit("CPU:Opcode 66:Illegal REP prefix %2X",repcode);
		}
	} while (again);
}

//flags.io and nt shouldn't be compiled for 386 
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
	if (flags.tf) E_Exit("CPU:Trap Flag not supported");			\
}


