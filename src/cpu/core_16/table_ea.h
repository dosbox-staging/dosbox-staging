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
typedef EAPoint (*EA_LookupHandler)(void);

static GetEATable * lookupEATable;

#define PREFIX_NONE		0x0
#define PREFIX_SEG		0x1
#define PREFIX_ADDR		0x2
#define PREFIX_SEG_ADDR	0x3

static struct {
	Bitu mark;
	Bitu count;
	EAPoint segbase;
} prefix;

/* Gets initialized at the bottem, can't seem to declare forward references */
static GetEATable * EAPrefixTable[4];


#define SegPrefix(blah)										\
	prefix.segbase=SegBase(blah);							\
	prefix.mark|=PREFIX_SEG;								\
	prefix.count++;											\
  	lookupEATable=EAPrefixTable[prefix.mark];				\
	goto restart;

#define SegPrefix_66(blah)									\
	prefix.segbase=SegBase(blah);							\
	prefix.mark|=PREFIX_SEG;								\
	prefix.count++;											\
  	lookupEATable=EAPrefixTable[prefix.mark];				\
	goto restart_66;


#define PrefixReset											\
	prefix.mark=PREFIX_NONE;								\
	prefix.count=0;											\
	lookupEATable=EAPrefixTable[PREFIX_NONE];


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


#define segprefixed(val) EAPoint ret=prefix.segbase+val;PrefixReset;return ret;

static EAPoint EA_16_00_s(void) { segprefixed((Bit16u)(reg_bx+(Bit16s)reg_si)) }
static EAPoint EA_16_01_s(void) { segprefixed((Bit16u)(reg_bx+(Bit16s)reg_di)) }
static EAPoint EA_16_02_s(void) { segprefixed((Bit16u)(reg_bp+(Bit16s)reg_si)) }
static EAPoint EA_16_03_s(void) { segprefixed((Bit16u)(reg_bp+(Bit16s)reg_di)) }
static EAPoint EA_16_04_s(void) { segprefixed((Bit16u)(reg_si)) }
static EAPoint EA_16_05_s(void) { segprefixed((Bit16u)(reg_di)) }
static EAPoint EA_16_06_s(void) { segprefixed((Bit16u)(Fetchw()))  }
static EAPoint EA_16_07_s(void) { segprefixed((Bit16u)(reg_bx)) }

static EAPoint EA_16_40_s(void) { segprefixed((Bit16u)(reg_bx+(Bit16s)reg_si+Fetchbs())) }
static EAPoint EA_16_41_s(void) { segprefixed((Bit16u)(reg_bx+(Bit16s)reg_di+Fetchbs())) }
static EAPoint EA_16_42_s(void) { segprefixed((Bit16u)(reg_bp+(Bit16s)reg_si+Fetchbs())) }
static EAPoint EA_16_43_s(void) { segprefixed((Bit16u)(reg_bp+(Bit16s)reg_di+Fetchbs())) }
static EAPoint EA_16_44_s(void) { segprefixed((Bit16u)(reg_si+Fetchbs())) }
static EAPoint EA_16_45_s(void) { segprefixed((Bit16u)(reg_di+Fetchbs())) }
static EAPoint EA_16_46_s(void) { segprefixed((Bit16u)(reg_bp+Fetchbs())) }
static EAPoint EA_16_47_s(void) { segprefixed((Bit16u)(reg_bx+Fetchbs())) }

static EAPoint EA_16_80_s(void) { segprefixed((Bit16u)(reg_bx+(Bit16s)reg_si+Fetchws())) }
static EAPoint EA_16_81_s(void) { segprefixed((Bit16u)(reg_bx+(Bit16s)reg_di+Fetchws())) }
static EAPoint EA_16_82_s(void) { segprefixed((Bit16u)(reg_bp+(Bit16s)reg_si+Fetchws())) }
static EAPoint EA_16_83_s(void) { segprefixed((Bit16u)(reg_bp+(Bit16s)reg_di+Fetchws())) }
static EAPoint EA_16_84_s(void) { segprefixed((Bit16u)(reg_si+Fetchws())) }
static EAPoint EA_16_85_s(void) { segprefixed((Bit16u)(reg_di+Fetchws())) }
static EAPoint EA_16_86_s(void) { segprefixed((Bit16u)(reg_bp+Fetchws())) }
static EAPoint EA_16_87_s(void) { segprefixed((Bit16u)(reg_bx+Fetchws())) }

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

static Bit32u SIBZero=0;
static Bit32u * SIBIndex[8]= { &reg_eax,&reg_ecx,&reg_edx,&reg_ebx,&SIBZero,&reg_ebp,&reg_esi,&reg_edi };

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
		base=SegBase(ss)+reg_esp;break;
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
	base+=*SIBIndex[(sib >> 3) &7] << (sib >> 6);
	return base;
};


static EAPoint EA_32_00_n(void) { PrefixReset;return SegBase(ds)+reg_eax; }
static EAPoint EA_32_01_n(void) { PrefixReset;return SegBase(ds)+reg_ecx; }
static EAPoint EA_32_02_n(void) { PrefixReset;return SegBase(ds)+reg_edx; }
static EAPoint EA_32_03_n(void) { PrefixReset;return SegBase(ds)+reg_ebx; }
static EAPoint EA_32_04_n(void) { PrefixReset;return Sib(0);}
static EAPoint EA_32_05_n(void) { PrefixReset;return SegBase(ds)+Fetchd(); }
static EAPoint EA_32_06_n(void) { PrefixReset;return SegBase(ds)+reg_esi; }
static EAPoint EA_32_07_n(void) { PrefixReset;return SegBase(ds)+reg_edi; }

static EAPoint EA_32_40_n(void) { PrefixReset;return SegBase(ds)+reg_eax+Fetchbs(); }
static EAPoint EA_32_41_n(void) { PrefixReset;return SegBase(ds)+reg_ecx+Fetchbs(); }
static EAPoint EA_32_42_n(void) { PrefixReset;return SegBase(ds)+reg_edx+Fetchbs(); }
static EAPoint EA_32_43_n(void) { PrefixReset;return SegBase(ds)+reg_ebx+Fetchbs(); }
static EAPoint EA_32_44_n(void) { PrefixReset;return Sib(1)+Fetchbs();}
static EAPoint EA_32_45_n(void) { PrefixReset;return SegBase(ss)+reg_ebp+Fetchbs(); }
static EAPoint EA_32_46_n(void) { PrefixReset;return SegBase(ds)+reg_esi+Fetchbs(); }
static EAPoint EA_32_47_n(void) { PrefixReset;return SegBase(ds)+reg_edi+Fetchbs(); }

static EAPoint EA_32_80_n(void) { PrefixReset;return SegBase(ds)+reg_eax+Fetchds(); }
static EAPoint EA_32_81_n(void) { PrefixReset;return SegBase(ds)+reg_ecx+Fetchds(); }
static EAPoint EA_32_82_n(void) { PrefixReset;return SegBase(ds)+reg_edx+Fetchds(); }
static EAPoint EA_32_83_n(void) { PrefixReset;return SegBase(ds)+reg_ebx+Fetchds(); }
static EAPoint EA_32_84_n(void) { PrefixReset;return Sib(2)+Fetchds();}
static EAPoint EA_32_85_n(void) { PrefixReset;return SegBase(ss)+reg_ebp+Fetchds(); }
static EAPoint EA_32_86_n(void) { PrefixReset;return SegBase(ds)+reg_esi+Fetchds(); }
static EAPoint EA_32_87_n(void) { PrefixReset;return SegBase(ds)+reg_edi+Fetchds(); }

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

INLINE EAPoint Sib_s(Bitu mode) {
	Bit8u sib=Fetchb();
	EAPoint base;
	switch (sib&7) {
	case 0:	/* EAX Base */
		base=prefix.segbase+reg_eax;break;
	case 1:	/* ECX Base */
		base=prefix.segbase+reg_ecx;break;
	case 2:	/* EDX Base */
		base=prefix.segbase+reg_edx;break;
	case 3:	/* EBX Base */
		base=prefix.segbase+reg_ebx;break;
	case 4:	/* ESP Base */
		base=prefix.segbase+reg_esp;break;
	case 5:	/* #1 Base */
		if (!mode) {
			base=prefix.segbase+Fetchd();break;
		} else {
			base=prefix.segbase+reg_ebp;break;
		}
	case 6:	/* ESI Base */
		base=prefix.segbase+reg_esi;break;
	case 7:	/* EDI Base */
		base=prefix.segbase+reg_edi;break;
	}
	base+=*SIBIndex[(sib >> 3) &7] << (sib >> 6);
	PrefixReset;
	return base;
};

#define segprefixed_32(val) EAPoint ret=prefix.segbase+(Bit32u)(val);PrefixReset;return ret;

static EAPoint EA_32_00_s(void) { segprefixed_32(reg_eax); }
static EAPoint EA_32_01_s(void) { segprefixed_32(reg_ecx); }
static EAPoint EA_32_02_s(void) { segprefixed_32(reg_edx); }
static EAPoint EA_32_03_s(void) { segprefixed_32(reg_ebx); }
static EAPoint EA_32_04_s(void) { return Sib_s(0);}
static EAPoint EA_32_05_s(void) { segprefixed_32(Fetchd()); }
static EAPoint EA_32_06_s(void) { segprefixed_32(reg_esi); }
static EAPoint EA_32_07_s(void) { segprefixed_32(reg_edi); }

static EAPoint EA_32_40_s(void) { segprefixed_32(reg_eax+Fetchbs()); }
static EAPoint EA_32_41_s(void) { segprefixed_32(reg_ecx+Fetchbs()); }
static EAPoint EA_32_42_s(void) { segprefixed_32(reg_edx+Fetchbs()); }
static EAPoint EA_32_43_s(void) { segprefixed_32(reg_ebx+Fetchbs()); }
static EAPoint EA_32_44_s(void) { return Sib_s(1)+Fetchbs();}
static EAPoint EA_32_45_s(void) { segprefixed_32(reg_ebp+Fetchbs()); }
static EAPoint EA_32_46_s(void) { segprefixed_32(reg_esi+Fetchbs()); }
static EAPoint EA_32_47_s(void) { segprefixed_32(reg_edi+Fetchbs()); }

static EAPoint EA_32_80_s(void) { segprefixed_32(reg_eax+Fetchds()); }
static EAPoint EA_32_81_s(void) { segprefixed_32(reg_ecx+Fetchds()); }
static EAPoint EA_32_82_s(void) { segprefixed_32(reg_edx+Fetchds()); }
static EAPoint EA_32_83_s(void) { segprefixed_32(reg_ebx+Fetchds()); }
static EAPoint EA_32_84_s(void) { return Sib_s(2)+Fetchds();}
static EAPoint EA_32_85_s(void) { segprefixed_32(reg_ebp+Fetchds()); }
static EAPoint EA_32_86_s(void) { segprefixed_32(reg_esi+Fetchds()); }
static EAPoint EA_32_87_s(void) { segprefixed_32(reg_edi+Fetchds()); }


static GetEATable GetEA_32_s={
/* 00 */
	EA_32_00_s,EA_32_01_s,EA_32_02_s,EA_32_03_s,EA_32_04_s,EA_32_05_s,EA_32_06_s,EA_32_07_s,
	EA_32_00_s,EA_32_01_s,EA_32_02_s,EA_32_03_s,EA_32_04_s,EA_32_05_s,EA_32_06_s,EA_32_07_s,
	EA_32_00_s,EA_32_01_s,EA_32_02_s,EA_32_03_s,EA_32_04_s,EA_32_05_s,EA_32_06_s,EA_32_07_s,
	EA_32_00_s,EA_32_01_s,EA_32_02_s,EA_32_03_s,EA_32_04_s,EA_32_05_s,EA_32_06_s,EA_32_07_s,
	EA_32_00_s,EA_32_01_s,EA_32_02_s,EA_32_03_s,EA_32_04_s,EA_32_05_s,EA_32_06_s,EA_32_07_s,
	EA_32_00_s,EA_32_01_s,EA_32_02_s,EA_32_03_s,EA_32_04_s,EA_32_05_s,EA_32_06_s,EA_32_07_s,
	EA_32_00_s,EA_32_01_s,EA_32_02_s,EA_32_03_s,EA_32_04_s,EA_32_05_s,EA_32_06_s,EA_32_07_s,
	EA_32_00_s,EA_32_01_s,EA_32_02_s,EA_32_03_s,EA_32_04_s,EA_32_05_s,EA_32_06_s,EA_32_07_s,
/* 01 */
	EA_32_40_s,EA_32_41_s,EA_32_42_s,EA_32_43_s,EA_32_44_s,EA_32_45_s,EA_32_46_s,EA_32_47_s,
	EA_32_40_s,EA_32_41_s,EA_32_42_s,EA_32_43_s,EA_32_44_s,EA_32_45_s,EA_32_46_s,EA_32_47_s,
	EA_32_40_s,EA_32_41_s,EA_32_42_s,EA_32_43_s,EA_32_44_s,EA_32_45_s,EA_32_46_s,EA_32_47_s,
	EA_32_40_s,EA_32_41_s,EA_32_42_s,EA_32_43_s,EA_32_44_s,EA_32_45_s,EA_32_46_s,EA_32_47_s,
	EA_32_40_s,EA_32_41_s,EA_32_42_s,EA_32_43_s,EA_32_44_s,EA_32_45_s,EA_32_46_s,EA_32_47_s,
	EA_32_40_s,EA_32_41_s,EA_32_42_s,EA_32_43_s,EA_32_44_s,EA_32_45_s,EA_32_46_s,EA_32_47_s,
	EA_32_40_s,EA_32_41_s,EA_32_42_s,EA_32_43_s,EA_32_44_s,EA_32_45_s,EA_32_46_s,EA_32_47_s,
	EA_32_40_s,EA_32_41_s,EA_32_42_s,EA_32_43_s,EA_32_44_s,EA_32_45_s,EA_32_46_s,EA_32_47_s,
/* 10 */
	EA_32_80_s,EA_32_81_s,EA_32_82_s,EA_32_83_s,EA_32_84_s,EA_32_85_s,EA_32_86_s,EA_32_87_s,
	EA_32_80_s,EA_32_81_s,EA_32_82_s,EA_32_83_s,EA_32_84_s,EA_32_85_s,EA_32_86_s,EA_32_87_s,
	EA_32_80_s,EA_32_81_s,EA_32_82_s,EA_32_83_s,EA_32_84_s,EA_32_85_s,EA_32_86_s,EA_32_87_s,
	EA_32_80_s,EA_32_81_s,EA_32_82_s,EA_32_83_s,EA_32_84_s,EA_32_85_s,EA_32_86_s,EA_32_87_s,
	EA_32_80_s,EA_32_81_s,EA_32_82_s,EA_32_83_s,EA_32_84_s,EA_32_85_s,EA_32_86_s,EA_32_87_s,
	EA_32_80_s,EA_32_81_s,EA_32_82_s,EA_32_83_s,EA_32_84_s,EA_32_85_s,EA_32_86_s,EA_32_87_s,
	EA_32_80_s,EA_32_81_s,EA_32_82_s,EA_32_83_s,EA_32_84_s,EA_32_85_s,EA_32_86_s,EA_32_87_s,
	EA_32_80_s,EA_32_81_s,EA_32_82_s,EA_32_83_s,EA_32_84_s,EA_32_85_s,EA_32_86_s,EA_32_87_s,
/* 11 These are illegal so make em 0 */
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,
	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0,	0
};

static EAPoint GetEADirect_NONE(void) {
	EAPoint result=SegBase(ds)+Fetchw();
	return result;
}
static EAPoint GetEADirect_SEG(void) {
	EAPoint result=prefix.segbase+Fetchw();PrefixReset;
	return result;
}
static EAPoint GetEADirect_ADDR(void) {
	EAPoint result=SegBase(ds)+Fetchd();PrefixReset;
	return result;
}
static EAPoint GetEADirect_SEG_ADDR(void) {
	EAPoint result=prefix.segbase+Fetchd();PrefixReset;
	return result;
}

static EA_LookupHandler GetEADirect[4]={GetEADirect_NONE,GetEADirect_SEG,GetEADirect_ADDR,GetEADirect_SEG_ADDR};
