// SPDX-FileCopyrightText:  2023-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

typedef PhysPt (*EA_LookupHandler)(void);

/* The MOD/RM Decoder for EA for this decoder's addressing modes */
static PhysPt EA_16_00_n(void) { return BaseDS+(uint16_t)(reg_bx+(int16_t)reg_si); }
static PhysPt EA_16_01_n(void) { return BaseDS+(uint16_t)(reg_bx+(int16_t)reg_di); }
static PhysPt EA_16_02_n(void) { return BaseSS+(uint16_t)(reg_bp+(int16_t)reg_si); }
static PhysPt EA_16_03_n(void) { return BaseSS+(uint16_t)(reg_bp+(int16_t)reg_di); }
static PhysPt EA_16_04_n() { return BaseDS + reg_si; }
static PhysPt EA_16_05_n() { return BaseDS + reg_di; }
static PhysPt EA_16_06_n() { return BaseDS + Fetchw(); }
static PhysPt EA_16_07_n() { return BaseDS + reg_bx; }

static PhysPt EA_16_40_n(void) { return BaseDS+(uint16_t)(reg_bx+(int16_t)reg_si+Fetchbs()); }
static PhysPt EA_16_41_n(void) { return BaseDS+(uint16_t)(reg_bx+(int16_t)reg_di+Fetchbs()); }
static PhysPt EA_16_42_n(void) { return BaseSS+(uint16_t)(reg_bp+(int16_t)reg_si+Fetchbs()); }
static PhysPt EA_16_43_n(void) { return BaseSS+(uint16_t)(reg_bp+(int16_t)reg_di+Fetchbs()); }
static PhysPt EA_16_44_n(void) { return BaseDS+(uint16_t)(reg_si+Fetchbs()); }
static PhysPt EA_16_45_n(void) { return BaseDS+(uint16_t)(reg_di+Fetchbs()); }
static PhysPt EA_16_46_n(void) { return BaseSS+(uint16_t)(reg_bp+Fetchbs()); }
static PhysPt EA_16_47_n(void) { return BaseDS+(uint16_t)(reg_bx+Fetchbs()); }

static PhysPt EA_16_80_n(void) { return BaseDS+(uint16_t)(reg_bx+(int16_t)reg_si+Fetchws()); }
static PhysPt EA_16_81_n(void) { return BaseDS+(uint16_t)(reg_bx+(int16_t)reg_di+Fetchws()); }
static PhysPt EA_16_82_n(void) { return BaseSS+(uint16_t)(reg_bp+(int16_t)reg_si+Fetchws()); }
static PhysPt EA_16_83_n(void) { return BaseSS+(uint16_t)(reg_bp+(int16_t)reg_di+Fetchws()); }
static PhysPt EA_16_84_n(void) { return BaseDS+(uint16_t)(reg_si+Fetchws()); }
static PhysPt EA_16_85_n(void) { return BaseDS+(uint16_t)(reg_di+Fetchws()); }
static PhysPt EA_16_86_n(void) { return BaseSS+(uint16_t)(reg_bp+Fetchws()); }
static PhysPt EA_16_87_n(void) { return BaseDS+(uint16_t)(reg_bx+Fetchws()); }

static uint32_t SIBZero=0;
static uint32_t * SIBIndex[8]= { &reg_eax,&reg_ecx,&reg_edx,&reg_ebx,&SIBZero,&reg_ebp,&reg_esi,&reg_edi };

static inline PhysPt Sib(Bitu mode) {
	uint8_t sib=Fetchb();
	PhysPt base;
	switch (sib&7) {
	case 0:	/* EAX Base */
		base=BaseDS+reg_eax;break;
	case 1:	/* ECX Base */
		base=BaseDS+reg_ecx;break;
	case 2:	/* EDX Base */
		base=BaseDS+reg_edx;break;
	case 3:	/* EBX Base */
		base=BaseDS+reg_ebx;break;
	case 4:	/* ESP Base */
		base=BaseSS+reg_esp;break;
	case 5:	/* #1 Base */
		if (!mode) {
			base=BaseDS+Fetchd();break;
		} else {
			base=BaseSS+reg_ebp;break;
		}
	case 6:	/* ESI Base */
		base=BaseDS+reg_esi;break;
	case 7:	/* EDI Base */
		base=BaseDS+reg_edi;break;
	}
	base+=*SIBIndex[(sib >> 3) &7] << (sib >> 6);
	return base;
}

static PhysPt EA_32_00_n(void) { return BaseDS+reg_eax; }
static PhysPt EA_32_01_n(void) { return BaseDS+reg_ecx; }
static PhysPt EA_32_02_n(void) { return BaseDS+reg_edx; }
static PhysPt EA_32_03_n(void) { return BaseDS+reg_ebx; }
static PhysPt EA_32_04_n(void) { return Sib(0);}
static PhysPt EA_32_05_n(void) { return BaseDS+Fetchd(); }
static PhysPt EA_32_06_n(void) { return BaseDS+reg_esi; }
static PhysPt EA_32_07_n(void) { return BaseDS+reg_edi; }

static PhysPt EA_32_40_n(void) { return BaseDS+reg_eax+Fetchbs(); }
static PhysPt EA_32_41_n(void) { return BaseDS+reg_ecx+Fetchbs(); }
static PhysPt EA_32_42_n(void) { return BaseDS+reg_edx+Fetchbs(); }
static PhysPt EA_32_43_n(void) { return BaseDS+reg_ebx+Fetchbs(); }
static PhysPt EA_32_44_n(void) { PhysPt temp=Sib(1);return temp+Fetchbs();}
//static PhysPt EA_32_44_n(void) { return Sib(1)+Fetchbs();}
static PhysPt EA_32_45_n(void) { return BaseSS+reg_ebp+Fetchbs(); }
static PhysPt EA_32_46_n(void) { return BaseDS+reg_esi+Fetchbs(); }
static PhysPt EA_32_47_n(void) { return BaseDS+reg_edi+Fetchbs(); }

static PhysPt EA_32_80_n(void) { return BaseDS+reg_eax+Fetchds(); }
static PhysPt EA_32_81_n(void) { return BaseDS+reg_ecx+Fetchds(); }
static PhysPt EA_32_82_n(void) { return BaseDS+reg_edx+Fetchds(); }
static PhysPt EA_32_83_n(void) { return BaseDS+reg_ebx+Fetchds(); }
static PhysPt EA_32_84_n(void) { PhysPt temp=Sib(2);return temp+Fetchds();}
//static PhysPt EA_32_84_n(void) { return Sib(2)+Fetchds();}
static PhysPt EA_32_85_n(void) { return BaseSS+reg_ebp+Fetchds(); }
static PhysPt EA_32_86_n(void) { return BaseDS+reg_esi+Fetchds(); }
static PhysPt EA_32_87_n(void) { return BaseDS+reg_edi+Fetchds(); }

// clang-format off
static GetEAHandler EATable[512]={
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
/* 11 These are illegal so make em nullptr */
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
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
/* 11 These are illegal so make em nullptr */
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
};
// clang-format on

#define GetEADirect							\
	PhysPt eaa;								\
	if (TEST_PREFIX_ADDR) {					\
		eaa=BaseDS+Fetchd();				\
	} else {								\
		eaa=BaseDS+Fetchw();				\
	}										\


