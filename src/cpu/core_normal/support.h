// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later


#define LoadMbs(off) (int8_t)(LoadMb(off))
#define LoadMws(off) (int16_t)(LoadMw(off))
#define LoadMds(off) (int32_t)(LoadMd(off))

#define LoadRb(reg) reg
#define LoadRw(reg) reg
#define LoadRd(reg) reg

#define SaveRb(reg,val)	reg=val
#define SaveRw(reg,val)	reg=val
#define SaveRd(reg,val)	reg=val

static inline int8_t Fetchbs()
{
	return static_cast<int8_t>(Fetchb());
}
static inline int16_t Fetchws()
{
	return static_cast<int16_t>(Fetchw());
}

static inline int32_t Fetchds()
{
	return static_cast<int32_t>(Fetchd());
}

#define RUNEXCEPTION() {										\
	CPU_Exception(cpu.exception.which,cpu.exception.error);		\
	continue;													\
}

#define EXCEPTION(blah)										\
	{														\
		CPU_Exception(blah);								\
		continue;											\
	}

//TODO Could probably make all byte operands fast?
#define JumpCond16_b(COND) {						\
	SAVEIP;											\
	if (COND) reg_ip+=Fetchbs();					\
	reg_ip+=1;										\
	continue;										\
}

#define JumpCond16_w(COND) {						\
	SAVEIP;											\
	if (COND) reg_ip+=Fetchws();					\
	reg_ip+=2;										\
	continue;										\
}

#define JumpCond32_b(COND) {						\
	SAVEIP;											\
	if (COND) reg_eip+=Fetchbs();					\
	reg_eip+=1;										\
	continue;										\
}

#define JumpCond32_d(COND) {						\
	SAVEIP;											\
	if (COND) reg_eip+=Fetchds();					\
	reg_eip+=4;										\
	continue;										\
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


