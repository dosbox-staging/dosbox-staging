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

typedef PhysPt (*GetEATable[256])(void);
typedef PhysPt (*EA_LookupHandler)(void);


/* The MOD/RM Decoder for EA for this decoder's addressing modes */
static PhysPt EA_16_00_n(void) { return SegBase(ds)+(Bit16u)(reg_bx+(Bit16s)reg_si); }
static PhysPt EA_16_01_n(void) { return SegBase(ds)+(Bit16u)(reg_bx+(Bit16s)reg_di); }
static PhysPt EA_16_02_n(void) { return SegBase(ss)+(Bit16u)(reg_bp+(Bit16s)reg_si); }
static PhysPt EA_16_03_n(void) { return SegBase(ss)+(Bit16u)(reg_bp+(Bit16s)reg_di); }
static PhysPt EA_16_04_n(void) { return SegBase(ds)+(Bit16u)(reg_si); }
static PhysPt EA_16_05_n(void) { return SegBase(ds)+(Bit16u)(reg_di); }
static PhysPt EA_16_06_n(void) { return SegBase(ds)+(Bit16u)(Fetchw());}
static PhysPt EA_16_07_n(void) { return SegBase(ds)+(Bit16u)(reg_bx); }

static PhysPt EA_16_40_n(void) { return SegBase(ds)+(Bit16u)(reg_bx+(Bit16s)reg_si+Fetchbs()); }
static PhysPt EA_16_41_n(void) { return SegBase(ds)+(Bit16u)(reg_bx+(Bit16s)reg_di+Fetchbs()); }
static PhysPt EA_16_42_n(void) { return SegBase(ss)+(Bit16u)(reg_bp+(Bit16s)reg_si+Fetchbs()); }
static PhysPt EA_16_43_n(void) { return SegBase(ss)+(Bit16u)(reg_bp+(Bit16s)reg_di+Fetchbs()); }
static PhysPt EA_16_44_n(void) { return SegBase(ds)+(Bit16u)(reg_si+Fetchbs()); }
static PhysPt EA_16_45_n(void) { return SegBase(ds)+(Bit16u)(reg_di+Fetchbs()); }
static PhysPt EA_16_46_n(void) { return SegBase(ss)+(Bit16u)(reg_bp+Fetchbs()); }
static PhysPt EA_16_47_n(void) { return SegBase(ds)+(Bit16u)(reg_bx+Fetchbs()); }

static PhysPt EA_16_80_n(void) { return SegBase(ds)+(Bit16u)(reg_bx+(Bit16s)reg_si+Fetchws()); }
static PhysPt EA_16_81_n(void) { return SegBase(ds)+(Bit16u)(reg_bx+(Bit16s)reg_di+Fetchws()); }
static PhysPt EA_16_82_n(void) { return SegBase(ss)+(Bit16u)(reg_bp+(Bit16s)reg_si+Fetchws()); }
static PhysPt EA_16_83_n(void) { return SegBase(ss)+(Bit16u)(reg_bp+(Bit16s)reg_di+Fetchws()); }
static PhysPt EA_16_84_n(void) { return SegBase(ds)+(Bit16u)(reg_si+Fetchws()); }
static PhysPt EA_16_85_n(void) { return SegBase(ds)+(Bit16u)(reg_di+Fetchws()); }
static PhysPt EA_16_86_n(void) { return SegBase(ss)+(Bit16u)(reg_bp+Fetchws()); }
static PhysPt EA_16_87_n(void) { return SegBase(ds)+(Bit16u)(reg_bx+Fetchws()); }

static GetEATable GetEA_NONE={
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



static PhysPt EA_16_00_s(void) { return core.seg_prefix_base+(Bit16u)(reg_bx+(Bit16s)reg_si); }
static PhysPt EA_16_01_s(void) { return core.seg_prefix_base+(Bit16u)(reg_bx+(Bit16s)reg_di); }
static PhysPt EA_16_02_s(void) { return core.seg_prefix_base+(Bit16u)(reg_bp+(Bit16s)reg_si); }
static PhysPt EA_16_03_s(void) { return core.seg_prefix_base+(Bit16u)(reg_bp+(Bit16s)reg_di); }
static PhysPt EA_16_04_s(void) { return core.seg_prefix_base+(Bit16u)(reg_si); }
static PhysPt EA_16_05_s(void) { return core.seg_prefix_base+(Bit16u)(reg_di); }
static PhysPt EA_16_06_s(void) { return core.seg_prefix_base+(Bit16u)(Fetchw());  }
static PhysPt EA_16_07_s(void) { return core.seg_prefix_base+(Bit16u)(reg_bx); }

static PhysPt EA_16_40_s(void) { return core.seg_prefix_base+(Bit16u)(reg_bx+(Bit16s)reg_si+Fetchbs()); }
static PhysPt EA_16_41_s(void) { return core.seg_prefix_base+(Bit16u)(reg_bx+(Bit16s)reg_di+Fetchbs()); }
static PhysPt EA_16_42_s(void) { return core.seg_prefix_base+(Bit16u)(reg_bp+(Bit16s)reg_si+Fetchbs()); }
static PhysPt EA_16_43_s(void) { return core.seg_prefix_base+(Bit16u)(reg_bp+(Bit16s)reg_di+Fetchbs()); }
static PhysPt EA_16_44_s(void) { return core.seg_prefix_base+(Bit16u)(reg_si+Fetchbs()); }
static PhysPt EA_16_45_s(void) { return core.seg_prefix_base+(Bit16u)(reg_di+Fetchbs()); }
static PhysPt EA_16_46_s(void) { return core.seg_prefix_base+(Bit16u)(reg_bp+Fetchbs()); }
static PhysPt EA_16_47_s(void) { return core.seg_prefix_base+(Bit16u)(reg_bx+Fetchbs()); }

static PhysPt EA_16_80_s(void) { return core.seg_prefix_base+(Bit16u)(reg_bx+(Bit16s)reg_si+Fetchws()); }
static PhysPt EA_16_81_s(void) { return core.seg_prefix_base+(Bit16u)(reg_bx+(Bit16s)reg_di+Fetchws()); }
static PhysPt EA_16_82_s(void) { return core.seg_prefix_base+(Bit16u)(reg_bp+(Bit16s)reg_si+Fetchws()); }
static PhysPt EA_16_83_s(void) { return core.seg_prefix_base+(Bit16u)(reg_bp+(Bit16s)reg_di+Fetchws()); }
static PhysPt EA_16_84_s(void) { return core.seg_prefix_base+(Bit16u)(reg_si+Fetchws()); }
static PhysPt EA_16_85_s(void) { return core.seg_prefix_base+(Bit16u)(reg_di+Fetchws()); }
static PhysPt EA_16_86_s(void) { return core.seg_prefix_base+(Bit16u)(reg_bp+Fetchws()); }
static PhysPt EA_16_87_s(void) { return core.seg_prefix_base+(Bit16u)(reg_bx+Fetchws()); }

static GetEATable GetEA_SEG={
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

INLINE PhysPt Sib(Bitu mode) {
	Bit8u sib=Fetchb();
	PhysPt base;
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


static PhysPt EA_32_00_n(void) { return SegBase(ds)+reg_eax; }
static PhysPt EA_32_01_n(void) { return SegBase(ds)+reg_ecx; }
static PhysPt EA_32_02_n(void) { return SegBase(ds)+reg_edx; }
static PhysPt EA_32_03_n(void) { return SegBase(ds)+reg_ebx; }
static PhysPt EA_32_04_n(void) { return Sib(0);}
static PhysPt EA_32_05_n(void) { return SegBase(ds)+Fetchd(); }
static PhysPt EA_32_06_n(void) { return SegBase(ds)+reg_esi; }
static PhysPt EA_32_07_n(void) { return SegBase(ds)+reg_edi; }

static PhysPt EA_32_40_n(void) { return SegBase(ds)+reg_eax+Fetchbs(); }
static PhysPt EA_32_41_n(void) { return SegBase(ds)+reg_ecx+Fetchbs(); }
static PhysPt EA_32_42_n(void) { return SegBase(ds)+reg_edx+Fetchbs(); }
static PhysPt EA_32_43_n(void) { return SegBase(ds)+reg_ebx+Fetchbs(); }
static PhysPt EA_32_44_n(void) { PhysPt temp=Sib(1);return temp+Fetchbs();}
//static PhysPt EA_32_44_n(void) { return Sib(1)+Fetchbs();}
static PhysPt EA_32_45_n(void) { return SegBase(ss)+reg_ebp+Fetchbs(); }
static PhysPt EA_32_46_n(void) { return SegBase(ds)+reg_esi+Fetchbs(); }
static PhysPt EA_32_47_n(void) { return SegBase(ds)+reg_edi+Fetchbs(); }

static PhysPt EA_32_80_n(void) { return SegBase(ds)+reg_eax+Fetchds(); }
static PhysPt EA_32_81_n(void) { return SegBase(ds)+reg_ecx+Fetchds(); }
static PhysPt EA_32_82_n(void) { return SegBase(ds)+reg_edx+Fetchds(); }
static PhysPt EA_32_83_n(void) { return SegBase(ds)+reg_ebx+Fetchds(); }
static PhysPt EA_32_84_n(void) { PhysPt temp=Sib(2);return temp+Fetchds();}
//static PhysPt EA_32_84_n(void) { return Sib(2)+Fetchds();}
static PhysPt EA_32_85_n(void) { return SegBase(ss)+reg_ebp+Fetchds(); }
static PhysPt EA_32_86_n(void) { return SegBase(ds)+reg_esi+Fetchds(); }
static PhysPt EA_32_87_n(void) { return SegBase(ds)+reg_edi+Fetchds(); }

static GetEATable GetEA_ADDR={
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

INLINE PhysPt Sib_s(Bitu mode) {
	Bit8u sib=Fetchb();
	PhysPt base;
	switch (sib&7) {
	case 0:	/* EAX Base */
		base=reg_eax;break;
	case 1:	/* ECX Base */
		base=reg_ecx;break;
	case 2:	/* EDX Base */
		base=reg_edx;break;
	case 3:	/* EBX Base */
		base=reg_ebx;break;
	case 4:	/* ESP Base */
		base=reg_esp;break;
	case 5:	/* #1 Base */
		if (!mode) {
			base=Fetchd();break;
		} else {
			base=reg_ebp;break;
		}
	case 6:	/* ESI Base */
		base=reg_esi;break;
	case 7:	/* EDI Base */
		base=reg_edi;break;
	}
	base+=*SIBIndex[(sib >> 3) &7] << (sib >> 6);
	return base;
};


static PhysPt EA_32_00_s(void) { return core.seg_prefix_base+(Bit32u)(reg_eax); }
static PhysPt EA_32_01_s(void) { return core.seg_prefix_base+(Bit32u)(reg_ecx); }
static PhysPt EA_32_02_s(void) { return core.seg_prefix_base+(Bit32u)(reg_edx); }
static PhysPt EA_32_03_s(void) { return core.seg_prefix_base+(Bit32u)(reg_ebx); }
static PhysPt EA_32_04_s(void) { return core.seg_prefix_base+(Bit32u)(Sib_s(0));}
static PhysPt EA_32_05_s(void) { return core.seg_prefix_base+(Bit32u)(Fetchd()); }
static PhysPt EA_32_06_s(void) { return core.seg_prefix_base+(Bit32u)(reg_esi); }
static PhysPt EA_32_07_s(void) { return core.seg_prefix_base+(Bit32u)(reg_edi); }

static PhysPt EA_32_40_s(void) { return core.seg_prefix_base+(Bit32u)(reg_eax+Fetchbs()); }
static PhysPt EA_32_41_s(void) { return core.seg_prefix_base+(Bit32u)(reg_ecx+Fetchbs()); }
static PhysPt EA_32_42_s(void) { return core.seg_prefix_base+(Bit32u)(reg_edx+Fetchbs()); }
static PhysPt EA_32_43_s(void) { return core.seg_prefix_base+(Bit32u)(reg_ebx+Fetchbs()); }
static PhysPt EA_32_44_s(void) { PhysPt temp=Sib_s(1);return core.seg_prefix_base+(Bit32u)(temp+Fetchbs());}
static PhysPt EA_32_45_s(void) { return core.seg_prefix_base+(Bit32u)(reg_ebp+Fetchbs()); }
static PhysPt EA_32_46_s(void) { return core.seg_prefix_base+(Bit32u)(reg_esi+Fetchbs()); }
static PhysPt EA_32_47_s(void) { return core.seg_prefix_base+(Bit32u)(reg_edi+Fetchbs()); }

static PhysPt EA_32_80_s(void) { return core.seg_prefix_base+(Bit32u)(reg_eax+Fetchds()); }
static PhysPt EA_32_81_s(void) { return core.seg_prefix_base+(Bit32u)(reg_ecx+Fetchds()); }
static PhysPt EA_32_82_s(void) { return core.seg_prefix_base+(Bit32u)(reg_edx+Fetchds()); }
static PhysPt EA_32_83_s(void) { return core.seg_prefix_base+(Bit32u)(reg_ebx+Fetchds()); }
static PhysPt EA_32_84_s(void) { PhysPt temp=Sib_s(2);return core.seg_prefix_base+(Bit32u)(temp+Fetchds());}
static PhysPt EA_32_85_s(void) { return core.seg_prefix_base+(Bit32u)(reg_ebp+Fetchds()); }
static PhysPt EA_32_86_s(void) { return core.seg_prefix_base+(Bit32u)(reg_esi+Fetchds()); }
static PhysPt EA_32_87_s(void) { return core.seg_prefix_base+(Bit32u)(reg_edi+Fetchds()); }


static GetEATable GetEA_SEG_ADDR={
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

#define GetEADirect										\
	PhysPt eaa;											\
	if (TEST_PREFIX_SEG) {								\
		if (TEST_PREFIX_ADDR) {							\
			eaa=core.seg_prefix_base+Fetchd();			\
		} else {										\
			eaa=core.seg_prefix_base+Fetchw();			\
		}												\
	} else {											\
		if (TEST_PREFIX_ADDR) {							\
			eaa=SegBase(ds)+Fetchd();					\
		} else {										\
			eaa=SegBase(ds)+Fetchw();					\
		}												\
	}


