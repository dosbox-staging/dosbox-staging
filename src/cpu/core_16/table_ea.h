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

/* Some variables for EA Loolkup */

typedef EAPoint (*GetEATable[256])(void);
static GetEATable * lookupEATable;

static EAPoint segprefix_base;
static bool segprefix_on=false;

#define SegPrefix(blah)										\
	segprefix_base=SegBase(blah);							\
	segprefix_on=true;										\
	lookupEATable=&GetEA_16_s;								\
	goto restart;											\

#define SegPrefix_66(blah)									\
	segprefix_base=SegBase(blah);							\
	segprefix_on=true;										\
	lookupEATable=&GetEA_16_s;								\
	goto restart_66;										\



#define SegPrefixReset										\
	segprefix_on=false;lookupEATable=&GetEA_16_n;


/* The MOD/RM Decoder for EA for this decoder's addressing modes */
static EAPoint EA_16_00_n(void) { return SegBase(ds)+(Bit16u)(reg_bx+(Bit16s)reg_si); }
static EAPoint EA_16_01_n(void) { return SegBase(ds)+(Bit16u)(reg_bx+(Bit16s)reg_di); }
static EAPoint EA_16_02_n(void) { return SegBase(ss)+(Bit16u)(reg_bp+(Bit16s)reg_si); }
static EAPoint EA_16_03_n(void) { return SegBase(ss)+(Bit16u)(reg_bp+(Bit16s)reg_di); }
static EAPoint EA_16_04_n(void) { return SegBase(ds)+(Bit16u)(reg_si); }
static EAPoint EA_16_05_n(void) { return SegBase(ds)+(Bit16u)(reg_di); }
static EAPoint EA_16_06_n(void) { return SegBase(ds)+(Bit16u)(Fetchw());}
static EAPoint EA_16_07_n(void) { return SegBase(ds)+(Bit16u)(reg_bx); }

static EAPoint EA_16_40_n(void) { return SegBase(ds)+(Bit16u)(reg_bx+(Bit16s)reg_si+Fetchbs()); }
static EAPoint EA_16_41_n(void) { return SegBase(ds)+(Bit16u)(reg_bx+(Bit16s)reg_di+Fetchbs()); }
static EAPoint EA_16_42_n(void) { return SegBase(ss)+(Bit16u)(reg_bp+(Bit16s)reg_si+Fetchbs()); }
static EAPoint EA_16_43_n(void) { return SegBase(ss)+(Bit16u)(reg_bp+(Bit16s)reg_di+Fetchbs()); }
static EAPoint EA_16_44_n(void) { return SegBase(ds)+(Bit16u)(reg_si+Fetchbs()); }
static EAPoint EA_16_45_n(void) { return SegBase(ds)+(Bit16u)(reg_di+Fetchbs()); }
static EAPoint EA_16_46_n(void) { return SegBase(ss)+(Bit16u)(reg_bp+Fetchbs()); }
static EAPoint EA_16_47_n(void) { return SegBase(ds)+(Bit16u)(reg_bx+Fetchbs()); }

static EAPoint EA_16_80_n(void) { return SegBase(ds)+(Bit16u)(reg_bx+(Bit16s)reg_si+Fetchws()); }
static EAPoint EA_16_81_n(void) { return SegBase(ds)+(Bit16u)(reg_bx+(Bit16s)reg_di+Fetchws()); }
static EAPoint EA_16_82_n(void) { return SegBase(ss)+(Bit16u)(reg_bp+(Bit16s)reg_si+Fetchws()); }
static EAPoint EA_16_83_n(void) { return SegBase(ss)+(Bit16u)(reg_bp+(Bit16s)reg_di+Fetchws()); }
static EAPoint EA_16_84_n(void) { return SegBase(ds)+(Bit16u)(reg_si+Fetchws()); }
static EAPoint EA_16_85_n(void) { return SegBase(ds)+(Bit16u)(reg_di+Fetchws()); }
static EAPoint EA_16_86_n(void) { return SegBase(ss)+(Bit16u)(reg_bp+Fetchws()); }
static EAPoint EA_16_87_n(void) { return SegBase(ds)+(Bit16u)(reg_bx+Fetchws()); }

static GetEATable GetEA_16_n={
/* 00 */
	EA_16_00_n,EA_16_01_n,EA_16_02_n,EA_16_03_n,EA_16_04_n,EA_16_05_n,EA_16_06_n,EA_16_07_n,
	EA_16_00_n,EA_16_01_n,EA_16_02_n,EA_16_03_n,EA_16_04_n,EA_16_05_n,EA_16_06_n,EA_16_07_n,
	EA_16_00_n,EA_16_01_n,EA_16_02_n,EA_16_03_n,EA_16_04_n,EA_16_05_n,EA_16_06_n,EA_16_07_n,
	EA_16_00_n,EA_16_01_n,EA_16_02_n,EA_16_03_n,EA_16_04_n,EA_16_05_n,EA_16_06_n,EA_16_07_n,
	EA_16_00_n,EA_16_01_n,EA_16_02_n,EA_16_03_n,EA_16_04_n,EA_16_05_n,EA_16_06_n,EA_16_07_n,
	EA_16_00_n,EA_16_01_n,EA_16_02_n,EA_16_03_n,EA_16_04_n,EA_16_05_n,EA_16_06_n,EA_16_07_n,
	EA_16_00_n,EA_16_01_n,EA_16_02_n,EA_16_03_n,EA_16_04_n,EA_16_05_n,EA_16_06_n,EA_16_07_n,
	EA_16_00_n,EA_16_01_n,EA_16_02_n,EA_16_03_n,EA_16_04_n,EA_16_05_n,EA_16_06_n,EA_16_07_n,
/* 01 */
	EA_16_40_n,EA_16_41_n,EA_16_42_n,EA_16_43_n,EA_16_44_n,EA_16_45_n,EA_16_46_n,EA_16_47_n,
	EA_16_40_n,EA_16_41_n,EA_16_42_n,EA_16_43_n,EA_16_44_n,EA_16_45_n,EA_16_46_n,EA_16_47_n,
	EA_16_40_n,EA_16_41_n,EA_16_42_n,EA_16_43_n,EA_16_44_n,EA_16_45_n,EA_16_46_n,EA_16_47_n,
	EA_16_40_n,EA_16_41_n,EA_16_42_n,EA_16_43_n,EA_16_44_n,EA_16_45_n,EA_16_46_n,EA_16_47_n,
	EA_16_40_n,EA_16_41_n,EA_16_42_n,EA_16_43_n,EA_16_44_n,EA_16_45_n,EA_16_46_n,EA_16_47_n,
	EA_16_40_n,EA_16_41_n,EA_16_42_n,EA_16_43_n,EA_16_44_n,EA_16_45_n,EA_16_46_n,EA_16_47_n,
	EA_16_40_n,EA_16_41_n,EA_16_42_n,EA_16_43_n,EA_16_44_n,EA_16_45_n,EA_16_46_n,EA_16_47_n,
	EA_16_40_n,EA_16_41_n,EA_16_42_n,EA_16_43_n,EA_16_44_n,EA_16_45_n,EA_16_46_n,EA_16_47_n,
/* 10 */
	EA_16_80_n,EA_16_81_n,EA_16_82_n,EA_16_83_n,EA_16_84_n,EA_16_85_n,EA_16_86_n,EA_16_87_n,
	EA_16_80_n,EA_16_81_n,EA_16_82_n,EA_16_83_n,EA_16_84_n,EA_16_85_n,EA_16_86_n,EA_16_87_n,
	EA_16_80_n,EA_16_81_n,EA_16_82_n,EA_16_83_n,EA_16_84_n,EA_16_85_n,EA_16_86_n,EA_16_87_n,
	EA_16_80_n,EA_16_81_n,EA_16_82_n,EA_16_83_n,EA_16_84_n,EA_16_85_n,EA_16_86_n,EA_16_87_n,
	EA_16_80_n,EA_16_81_n,EA_16_82_n,EA_16_83_n,EA_16_84_n,EA_16_85_n,EA_16_86_n,EA_16_87_n,
	EA_16_80_n,EA_16_81_n,EA_16_82_n,EA_16_83_n,EA_16_84_n,EA_16_85_n,EA_16_86_n,EA_16_87_n,
	EA_16_80_n,EA_16_81_n,EA_16_82_n,EA_16_83_n,EA_16_84_n,EA_16_85_n,EA_16_86_n,EA_16_87_n,
	EA_16_80_n,EA_16_81_n,EA_16_82_n,EA_16_83_n,EA_16_84_n,EA_16_85_n,EA_16_86_n,EA_16_87_n,
/* 11 These are illegal so make em 0 */
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0
};


#define prefixed(val) EAPoint ret=segprefix_base+val;SegPrefixReset;return ret;

static EAPoint EA_16_00_s(void) { prefixed((Bit16u)(reg_bx+(Bit16s)reg_si)) }
static EAPoint EA_16_01_s(void) { prefixed((Bit16u)(reg_bx+(Bit16s)reg_di)) }
static EAPoint EA_16_02_s(void) { prefixed((Bit16u)(reg_bp+(Bit16s)reg_si)) }
static EAPoint EA_16_03_s(void) { prefixed((Bit16u)(reg_bp+(Bit16s)reg_di)) }
static EAPoint EA_16_04_s(void) { prefixed((Bit16u)(reg_si)) }
static EAPoint EA_16_05_s(void) { prefixed((Bit16u)(reg_di)) }
static EAPoint EA_16_06_s(void) { prefixed((Bit16u)(Fetchw()))  }
static EAPoint EA_16_07_s(void) { prefixed((Bit16u)(reg_bx)) }

static EAPoint EA_16_40_s(void) { prefixed((Bit16u)(reg_bx+(Bit16s)reg_si+Fetchbs())) }
static EAPoint EA_16_41_s(void) { prefixed((Bit16u)(reg_bx+(Bit16s)reg_di+Fetchbs())) }
static EAPoint EA_16_42_s(void) { prefixed((Bit16u)(reg_bp+(Bit16s)reg_si+Fetchbs())) }
static EAPoint EA_16_43_s(void) { prefixed((Bit16u)(reg_bp+(Bit16s)reg_di+Fetchbs())) }
static EAPoint EA_16_44_s(void) { prefixed((Bit16u)(reg_si+Fetchbs())) }
static EAPoint EA_16_45_s(void) { prefixed((Bit16u)(reg_di+Fetchbs())) }
static EAPoint EA_16_46_s(void) { prefixed((Bit16u)(reg_bp+Fetchbs())) }
static EAPoint EA_16_47_s(void) { prefixed((Bit16u)(reg_bx+Fetchbs())) }

static EAPoint EA_16_80_s(void) { prefixed((Bit16u)(reg_bx+(Bit16s)reg_si+Fetchws())) }
static EAPoint EA_16_81_s(void) { prefixed((Bit16u)(reg_bx+(Bit16s)reg_di+Fetchws())) }
static EAPoint EA_16_82_s(void) { prefixed((Bit16u)(reg_bp+(Bit16s)reg_si+Fetchws())) }
static EAPoint EA_16_83_s(void) { prefixed((Bit16u)(reg_bp+(Bit16s)reg_di+Fetchws())) }
static EAPoint EA_16_84_s(void) { prefixed((Bit16u)(reg_si+Fetchws())) }
static EAPoint EA_16_85_s(void) { prefixed((Bit16u)(reg_di+Fetchws())) }
static EAPoint EA_16_86_s(void) { prefixed((Bit16u)(reg_bp+Fetchws())) }
static EAPoint EA_16_87_s(void) { prefixed((Bit16u)(reg_bx+Fetchws())) }

static GetEATable GetEA_16_s={
/* 00 */
	EA_16_00_s,EA_16_01_s,EA_16_02_s,EA_16_03_s,EA_16_04_s,EA_16_05_s,EA_16_06_s,EA_16_07_s,
	EA_16_00_s,EA_16_01_s,EA_16_02_s,EA_16_03_s,EA_16_04_s,EA_16_05_s,EA_16_06_s,EA_16_07_s,
	EA_16_00_s,EA_16_01_s,EA_16_02_s,EA_16_03_s,EA_16_04_s,EA_16_05_s,EA_16_06_s,EA_16_07_s,
	EA_16_00_s,EA_16_01_s,EA_16_02_s,EA_16_03_s,EA_16_04_s,EA_16_05_s,EA_16_06_s,EA_16_07_s,
	EA_16_00_s,EA_16_01_s,EA_16_02_s,EA_16_03_s,EA_16_04_s,EA_16_05_s,EA_16_06_s,EA_16_07_s,
	EA_16_00_s,EA_16_01_s,EA_16_02_s,EA_16_03_s,EA_16_04_s,EA_16_05_s,EA_16_06_s,EA_16_07_s,
	EA_16_00_s,EA_16_01_s,EA_16_02_s,EA_16_03_s,EA_16_04_s,EA_16_05_s,EA_16_06_s,EA_16_07_s,
	EA_16_00_s,EA_16_01_s,EA_16_02_s,EA_16_03_s,EA_16_04_s,EA_16_05_s,EA_16_06_s,EA_16_07_s,
/* 01 */
	EA_16_40_s,EA_16_41_s,EA_16_42_s,EA_16_43_s,EA_16_44_s,EA_16_45_s,EA_16_46_s,EA_16_47_s,
	EA_16_40_s,EA_16_41_s,EA_16_42_s,EA_16_43_s,EA_16_44_s,EA_16_45_s,EA_16_46_s,EA_16_47_s,
	EA_16_40_s,EA_16_41_s,EA_16_42_s,EA_16_43_s,EA_16_44_s,EA_16_45_s,EA_16_46_s,EA_16_47_s,
	EA_16_40_s,EA_16_41_s,EA_16_42_s,EA_16_43_s,EA_16_44_s,EA_16_45_s,EA_16_46_s,EA_16_47_s,
	EA_16_40_s,EA_16_41_s,EA_16_42_s,EA_16_43_s,EA_16_44_s,EA_16_45_s,EA_16_46_s,EA_16_47_s,
	EA_16_40_s,EA_16_41_s,EA_16_42_s,EA_16_43_s,EA_16_44_s,EA_16_45_s,EA_16_46_s,EA_16_47_s,
	EA_16_40_s,EA_16_41_s,EA_16_42_s,EA_16_43_s,EA_16_44_s,EA_16_45_s,EA_16_46_s,EA_16_47_s,
	EA_16_40_s,EA_16_41_s,EA_16_42_s,EA_16_43_s,EA_16_44_s,EA_16_45_s,EA_16_46_s,EA_16_47_s,
/* 10 */
	EA_16_80_s,EA_16_81_s,EA_16_82_s,EA_16_83_s,EA_16_84_s,EA_16_85_s,EA_16_86_s,EA_16_87_s,
	EA_16_80_s,EA_16_81_s,EA_16_82_s,EA_16_83_s,EA_16_84_s,EA_16_85_s,EA_16_86_s,EA_16_87_s,
	EA_16_80_s,EA_16_81_s,EA_16_82_s,EA_16_83_s,EA_16_84_s,EA_16_85_s,EA_16_86_s,EA_16_87_s,
	EA_16_80_s,EA_16_81_s,EA_16_82_s,EA_16_83_s,EA_16_84_s,EA_16_85_s,EA_16_86_s,EA_16_87_s,
	EA_16_80_s,EA_16_81_s,EA_16_82_s,EA_16_83_s,EA_16_84_s,EA_16_85_s,EA_16_86_s,EA_16_87_s,
	EA_16_80_s,EA_16_81_s,EA_16_82_s,EA_16_83_s,EA_16_84_s,EA_16_85_s,EA_16_86_s,EA_16_87_s,
	EA_16_80_s,EA_16_81_s,EA_16_82_s,EA_16_83_s,EA_16_84_s,EA_16_85_s,EA_16_86_s,EA_16_87_s,
	EA_16_80_s,EA_16_81_s,EA_16_82_s,EA_16_83_s,EA_16_84_s,EA_16_85_s,EA_16_86_s,EA_16_87_s,
/* 11 These are illegal so make em 0 */
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0
};


INLINE EAPoint Sib(Bitu mode) {
	Bit8u sib=Fetchb();
	EAPoint base;
	switch (sib&7) {
	case 0:	/* EAX Base */
		base=SegBase(ds)+reg_eax;break;
	case 1:	/* ECX Base */
		base=SegBase(ds)+reg_ecx;break;
	case 2:	/* EDX Base */
		base=SegBase(ds)+reg_edx;break;
	case 3:	/* EBX Base */
		base=SegBase(ds)+reg_ebx;break;
	case 4:	/* ESP Base */
		base=SegBase(ds)+reg_esp;break;
	case 5:	/* #1 Base */
		if (!mode) {
			base=SegBase(ds)+Fetchd();break;
		} else {
			base=SegBase(ss)+reg_ebp;break;
		}
	case 6:	/* ESI Base */
		base=SegBase(ds)+reg_esi;break;
	case 7:	/* EDI Base */
		base=SegBase(ds)+reg_edi;break;
	}
	Bitu shift=sib >> 6;
	switch ((sib >>3) &7) {
	case 0:	/* EAX Index */
		base+=(Bit32s)reg_eax<<shift;break;
	case 1:	/* ECX Index */
		base+=(Bit32s)reg_ecx<<shift;break;
	case 2:	/* EDX Index */
		base+=(Bit32s)reg_edx<<shift;break;
	case 3:	/* EBX Index */
		base+=(Bit32s)reg_ebx<<shift;break;
	case 4:	/* None */
		break;
	case 5:	/* EBP Index */
		base+=(Bit32s)reg_ebp<<shift;break;
	case 6:	/* ESI Index */
		base+=(Bit32s)reg_esi<<shift;break;
	case 7:	/* EDI Index */
		base+=(Bit32s)reg_edi<<shift;break;
	};
	return base;
};


static EAPoint EA_32_00_n(void) { return SegBase(ds)+reg_eax; }
static EAPoint EA_32_01_n(void) { return SegBase(ds)+reg_ecx; }
static EAPoint EA_32_02_n(void) { return SegBase(ds)+reg_edx; }
static EAPoint EA_32_03_n(void) { return SegBase(ds)+reg_ebx; }
static EAPoint EA_32_04_n(void) { return Sib(0);}
static EAPoint EA_32_05_n(void) { return SegBase(ds)+Fetchd(); }
static EAPoint EA_32_06_n(void) { return SegBase(ss)+reg_esi; }
static EAPoint EA_32_07_n(void) { return SegBase(ds)+reg_edi; }

static EAPoint EA_32_40_n(void) { return SegBase(ds)+reg_eax+Fetchbs(); }
static EAPoint EA_32_41_n(void) { return SegBase(ds)+reg_ecx+Fetchbs(); }
static EAPoint EA_32_42_n(void) { return SegBase(ds)+reg_edx+Fetchbs(); }
static EAPoint EA_32_43_n(void) { return SegBase(ds)+reg_ebx+Fetchbs(); }
static EAPoint EA_32_44_n(void) { return Sib(1)+Fetchbs();}
static EAPoint EA_32_45_n(void) { return SegBase(ss)+reg_ebp+Fetchbs(); }
static EAPoint EA_32_46_n(void) { return SegBase(ds)+reg_esi+Fetchbs(); }
static EAPoint EA_32_47_n(void) { return SegBase(ds)+reg_edi+Fetchbs(); }

static EAPoint EA_32_80_n(void) { return SegBase(ds)+reg_eax+Fetchds(); }
static EAPoint EA_32_81_n(void) { return SegBase(ds)+reg_ecx+Fetchds(); }
static EAPoint EA_32_82_n(void) { return SegBase(ds)+reg_edx+Fetchds(); }
static EAPoint EA_32_83_n(void) { return SegBase(ds)+reg_ebx+Fetchds(); }
static EAPoint EA_32_84_n(void) { return Sib(2)+Fetchds();}
static EAPoint EA_32_85_n(void) { return SegBase(ss)+reg_ebp+Fetchds(); }
static EAPoint EA_32_86_n(void) { return SegBase(ds)+reg_esi+Fetchds(); }
static EAPoint EA_32_87_n(void) { return SegBase(ds)+reg_edi+Fetchds(); }

static GetEATable GetEA_32_n={
/* 00 */
	EA_32_00_n,EA_32_01_n,EA_32_02_n,EA_32_03_n,EA_32_04_n,EA_32_05_n,EA_32_06_n,EA_32_07_n,
	EA_32_00_n,EA_32_01_n,EA_32_02_n,EA_32_03_n,EA_32_04_n,EA_32_05_n,EA_32_06_n,EA_32_07_n,
	EA_32_00_n,EA_32_01_n,EA_32_02_n,EA_32_03_n,EA_32_04_n,EA_32_05_n,EA_32_06_n,EA_32_07_n,
	EA_32_00_n,EA_32_01_n,EA_32_02_n,EA_32_03_n,EA_32_04_n,EA_32_05_n,EA_32_06_n,EA_32_07_n,
	EA_32_00_n,EA_32_01_n,EA_32_02_n,EA_32_03_n,EA_32_04_n,EA_32_05_n,EA_32_06_n,EA_32_07_n,
	EA_32_00_n,EA_32_01_n,EA_32_02_n,EA_32_03_n,EA_32_04_n,EA_32_05_n,EA_32_06_n,EA_32_07_n,
	EA_32_00_n,EA_32_01_n,EA_32_02_n,EA_32_03_n,EA_32_04_n,EA_32_05_n,EA_32_06_n,EA_32_07_n,
	EA_32_00_n,EA_32_01_n,EA_32_02_n,EA_32_03_n,EA_32_04_n,EA_32_05_n,EA_32_06_n,EA_32_07_n,
/* 01 */
	EA_32_40_n,EA_32_41_n,EA_32_42_n,EA_32_43_n,EA_32_44_n,EA_32_45_n,EA_32_46_n,EA_32_47_n,
	EA_32_40_n,EA_32_41_n,EA_32_42_n,EA_32_43_n,EA_32_44_n,EA_32_45_n,EA_32_46_n,EA_32_47_n,
	EA_32_40_n,EA_32_41_n,EA_32_42_n,EA_32_43_n,EA_32_44_n,EA_32_45_n,EA_32_46_n,EA_32_47_n,
	EA_32_40_n,EA_32_41_n,EA_32_42_n,EA_32_43_n,EA_32_44_n,EA_32_45_n,EA_32_46_n,EA_32_47_n,
	EA_32_40_n,EA_32_41_n,EA_32_42_n,EA_32_43_n,EA_32_44_n,EA_32_45_n,EA_32_46_n,EA_32_47_n,
	EA_32_40_n,EA_32_41_n,EA_32_42_n,EA_32_43_n,EA_32_44_n,EA_32_45_n,EA_32_46_n,EA_32_47_n,
	EA_32_40_n,EA_32_41_n,EA_32_42_n,EA_32_43_n,EA_32_44_n,EA_32_45_n,EA_32_46_n,EA_32_47_n,
	EA_32_40_n,EA_32_41_n,EA_32_42_n,EA_32_43_n,EA_32_44_n,EA_32_45_n,EA_32_46_n,EA_32_47_n,
/* 10 */
	EA_32_80_n,EA_32_81_n,EA_32_82_n,EA_32_83_n,EA_32_84_n,EA_32_85_n,EA_32_86_n,EA_32_87_n,
	EA_32_80_n,EA_32_81_n,EA_32_82_n,EA_32_83_n,EA_32_84_n,EA_32_85_n,EA_32_86_n,EA_32_87_n,
	EA_32_80_n,EA_32_81_n,EA_32_82_n,EA_32_83_n,EA_32_84_n,EA_32_85_n,EA_32_86_n,EA_32_87_n,
	EA_32_80_n,EA_32_81_n,EA_32_82_n,EA_32_83_n,EA_32_84_n,EA_32_85_n,EA_32_86_n,EA_32_87_n,
	EA_32_80_n,EA_32_81_n,EA_32_82_n,EA_32_83_n,EA_32_84_n,EA_32_85_n,EA_32_86_n,EA_32_87_n,
	EA_32_80_n,EA_32_81_n,EA_32_82_n,EA_32_83_n,EA_32_84_n,EA_32_85_n,EA_32_86_n,EA_32_87_n,
	EA_32_80_n,EA_32_81_n,EA_32_82_n,EA_32_83_n,EA_32_84_n,EA_32_85_n,EA_32_86_n,EA_32_87_n,
	EA_32_80_n,EA_32_81_n,EA_32_82_n,EA_32_83_n,EA_32_84_n,EA_32_85_n,EA_32_86_n,EA_32_87_n,
/* 11 These are illegal so make em 0 */
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0
};

