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

#ifndef __FPU_H
#define __FPU_H

#include <dosbox.h>
#include <regs.h>
#include <mem.h>

enum { FPUREG_VALID=0, FPUREG_ZERO, FPUREG_PNAN, FPUREG_NNAN, FPUREG_EMPTY };

enum {
	t_FLD=0, t_FLDST, t_FDIV,
	t_FDIVP, t_FCHS, t_FCOMP,

	t_FUNKNOWN,
	t_FNOTDONE,
};

bool FPU_get_C3();
bool FPU_get_C2();
bool FPU_get_C1();
bool FPU_get_C0();
bool FPU_get_IR();
bool FPU_get_SF();
bool FPU_get_PF();
bool FPU_get_UF();
bool FPU_get_OF();
bool FPU_get_ZF();
bool FPU_get_DF();
bool FPU_get_IN();

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


#endif