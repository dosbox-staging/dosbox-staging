/*
 *  Copyright (C) 2002-2004  The DOSBox Team
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

#define SETIP(_a_)	(core.ip_lookup=SegBase(cs)+_a_)
#define GETIP		(Bit32u)(core.ip_lookup-SegBase(cs))
#define SAVEIP		{reg_eip=GETIP;}
#define LOADIP		{core.ip_lookup=(SegBase(cs)+reg_eip);}

#define LEAVECORE						\
	SAVEIP;								\
	FillFlags();

static INLINE void ADDIPw(Bits add) {
	SAVEIP;
	reg_eip=(Bit16u)(reg_eip+add);
	LOADIP;
}

static INLINE void ADDIPd(Bits add) {
	SAVEIP;
	reg_eip=(reg_eip+add);
	LOADIP;
}


static INLINE void ADDIPFAST(Bits blah) {
	core.ip_lookup+=blah;
}

#define EXCEPTION(blah)										\
	{														\
		Bit8u new_num=blah;									\
		core.ip_lookup=core.op_start;						\
		LEAVECORE;											\
		CPU_Exception(new_num);								\
		goto decode_start;									\
	}

static INLINE Bit8u Fetchb() {
	Bit8u temp=LoadMb(core.ip_lookup);
	core.ip_lookup+=1;
	return temp;
}

static INLINE Bit16u Fetchw() {
	Bit16u temp=LoadMw(core.ip_lookup);
	core.ip_lookup+=2;
	return temp;
}
static INLINE Bit32u Fetchd() {
	Bit32u temp=LoadMd(core.ip_lookup);
	core.ip_lookup+=4;
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

#if 0

static INLINE void Push_16(Bit16u blah)	{
	reg_esp-=2;
	SaveMw(SegBase(ss)+(reg_esp & cpu.stack.mask),blah);
};

static INLINE void Push_32(Bit32u blah)	{
	reg_esp-=4;
	SaveMd(SegBase(ss)+(reg_esp & cpu.stack.mask),blah);
};

static INLINE Bit16u Pop_16() {
	Bit16u temp=LoadMw(SegBase(ss)+(reg_esp & cpu.stack.mask));
	reg_esp+=2;
	return temp;
};

static INLINE Bit32u Pop_32() {
	Bit32u temp=LoadMd(SegBase(ss)+(reg_esp & cpu.stack.mask));
	reg_esp+=4;
	return temp;
};

#else 

#define Push_16 CPU_Push16
#define Push_32 CPU_Push32
#define Pop_16 CPU_Pop16
#define Pop_32 CPU_Pop32

#endif

#define JumpSIb(blah) 										\
	if (blah) {												\
		ADDIPFAST(Fetchbs());								\
	} else {												\
		ADDIPFAST(1);										\
	}					

#define JumpSIw(blah) 										\
	if (blah) {												\
		ADDIPw(Fetchws());									\
	} else {												\
		ADDIPFAST(2);										\
	}						


#define JumpSId(blah) 										\
	if (blah) {												\
		ADDIPd(Fetchds());									\
	} else {												\
		ADDIPFAST(4);										\
	}	

#define SETcc(cc)											\
	{														\
		GetRM;												\
		if (rm >= 0xc0 ) {GetEArb;*earb=(cc) ? 1 : 0;}		\
		else {GetEAa;SaveMb(eaa,(cc) ? 1 : 0);}				\
	}

#include "helpers.h"
#include "table_ea.h"
#include "../modrm.h"


