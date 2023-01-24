/*
 *  Copyright (C) 2002-2021  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef DOSBOX_FPU_H
#define DOSBOX_FPU_H

#ifndef DOSBOX_DOSBOX_H
//So the right config.h gets included for C_DEBUG
#include "dosbox.h"
#endif

#ifndef DOSBOX_MEM_H
#include "mem.h"
#endif

void FPU_ESC0_Normal(Bitu rm);
void FPU_ESC0_EA(Bitu func,PhysPt ea);
void FPU_ESC1_Normal(Bitu rm);
void FPU_ESC1_EA(Bitu func,PhysPt ea);
void FPU_ESC2_Normal(Bitu rm);
void FPU_ESC2_EA(Bitu func,PhysPt ea);
void FPU_ESC3_Normal(Bitu rm);
void FPU_ESC3_EA(Bitu func,PhysPt ea);
void FPU_ESC4_Normal(Bitu rm);
void FPU_ESC4_EA(Bitu func,PhysPt ea);
void FPU_ESC5_Normal(Bitu rm);
void FPU_ESC5_EA(Bitu func,PhysPt ea);
void FPU_ESC6_Normal(Bitu rm);
void FPU_ESC6_EA(Bitu func,PhysPt ea);
void FPU_ESC7_Normal(Bitu rm);
void FPU_ESC7_EA(Bitu func,PhysPt ea);


typedef union {
    double d;
#ifndef WORDS_BIGENDIAN
    struct {
        uint32_t lower;
        int32_t upper;
    } l;
#else
    struct {
        int32_t upper;
        uint32_t lower;
    } l;
#endif
    int64_t ll;
} FPU_Reg;

typedef struct {
    uint32_t m1;
    uint32_t m2;
    uint16_t m3;

    uint16_t d1;
    uint32_t d2;
} FPU_P_Reg;

enum FPU_Tag {
	TAG_Valid = 0,
	TAG_Zero  = 1,
	TAG_Weird = 2,
	TAG_Empty = 3
};

enum FPU_Round {
	ROUND_Nearest = 0,		
	ROUND_Down    = 1,
	ROUND_Up      = 2,	
	ROUND_Chop    = 3
};

typedef struct FPU_rec {
	FPU_Reg		regs[9];
	FPU_P_Reg	p_regs[9];
	FPU_Tag		tags[9];
	uint16_t		cw,cw_mask_all;
	uint16_t		sw;
	uint32_t		top;
	FPU_Round	round;
} FPU_rec;

#define L2E		1.4426950408889634
#define L2T		3.3219280948873623
#define LN2		0.69314718055994531
#define LG2		0.3010299956639812


extern FPU_rec fpu;

#define TOP fpu.top
#define STV(i)  ( (fpu.top+ (i) ) & 7 )


uint16_t FPU_GetTag(void);
void FPU_FLDCW(PhysPt addr);

static inline void FPU_SetTag(uint16_t tag){
	for(Bitu i=0;i<8;i++)
		fpu.tags[i] = static_cast<FPU_Tag>((tag >>(2*i))&3);
}

static inline uint16_t FPU_GetCW()
{
	return fpu.cw;
}

static inline void FPU_SetCW(Bitu word)
{
	fpu.cw = (uint16_t)word;
	fpu.cw_mask_all = (uint16_t)(word | 0x3f);
	fpu.round = (FPU_Round)((word >> 10) & 3);
}

static inline uint16_t FPU_GetSW()
{
	return fpu.sw;
}

static inline void FPU_SetSW(Bitu word)
{
	fpu.sw = (uint16_t)word;
}

static inline Bitu FPU_GET_TOP(void) {
	return (fpu.sw & 0x3800)>>11;
}

static inline void FPU_SET_TOP(Bitu val){
	fpu.sw &= ~0x3800;
	fpu.sw |= (val&7)<<11;
}

void FPU_LoadPRegs(const uint8_t* buffer);
void FPU_SavePRegs(uint8_t* buffer);

static inline void FPU_SET_C0(Bitu C){
	fpu.sw &= ~0x0100;
	if(C) fpu.sw |=  0x0100;
}

static inline void FPU_SET_C1(Bitu C){
	fpu.sw &= ~0x0200;
	if(C) fpu.sw |=  0x0200;
}

static inline void FPU_SET_C2(Bitu C){
	fpu.sw &= ~0x0400;
	if(C) fpu.sw |=  0x0400;
}

static inline void FPU_SET_C3(Bitu C){
	fpu.sw &= ~0x4000;
	if(C) fpu.sw |= 0x4000;
}

static inline void FPU_LOG_WARN(unsigned tree, bool ea, uintptr_t group, uintptr_t sub)
{
	LOG(LOG_FPU, LOG_WARN)("ESC %u%s: Unhandled group %" PRIuPTR " subfunction %" PRIuPTR,
	                       tree, ea ? " EA" : "", group, sub);
}

#define DB_FPU_STACK_CHECK_NONE 0
#define DB_FPU_STACK_CHECK_LOG  1
#define DB_FPU_STACK_CHECK_EXIT 2
//NONE is 0.74 behavior: not care about stack overflow/underflow
//Overflow is always logged/exited on.
//Underflow can be controlled with by this. 
//LOG is giving a message when encountered
//EXIT is to hard exit.
//Currently pop is ignored in release mode and overflow is exit.
//in debug mode: pop will log and overflow is exit. 
#if C_DEBUG
#define DB_FPU_STACK_CHECK_POP DB_FPU_STACK_CHECK_LOG
#define DB_FPU_STACK_CHECK_PUSH DB_FPU_STACK_CHECK_EXIT
#else
#define DB_FPU_STACK_CHECK_POP DB_FPU_STACK_CHECK_NONE
#define DB_FPU_STACK_CHECK_PUSH DB_FPU_STACK_CHECK_EXIT
#endif

#endif
