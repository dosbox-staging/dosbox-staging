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

#include "dosbox.h"
#include "fpu.h"
#include "pic.h"
#include "fpu_types.h"
extern FPU_Flag_Info fpu_flags;

bool FPU_get_C3() {
	switch(fpu_flags.type) {
	case t_FLD:
	case t_FLDST:
	case t_FDIV:
	case t_FDIVP:
	case t_FCHS:
	case t_FUNKNOWN:
	case t_FNOTDONE:
		return fpu_flags.sw.c3;
	case t_FCOMP:
		return (fpu_flags.result.tag==FPUREG_EMPTY||fpu_flags.result.tag==FPUREG_ZERO);
	default:
		E_Exit("FPU_get_C3 Unknown %d",fpu_flags.type);
	}
	return 0;
}

bool FPU_get_C2() {
	switch(fpu_flags.type) {
	case t_FLD:
	case t_FLDST:
	case t_FDIV:
	case t_FDIVP:
	case t_FCHS:
	case t_FUNKNOWN:
	case t_FNOTDONE:
		return fpu_flags.sw.c2;
	case t_FCOMP:
		return (fpu_flags.result.tag==FPUREG_EMPTY);
	default:
		E_Exit("FPU_get_C2 Unknown %d",fpu_flags.type);
	}
	return 0;
}

bool FPU_get_C1() {
	switch(fpu_flags.type) {
	case t_FLD:
	case t_FLDST:
	case t_FDIV:
	case t_FDIVP:
	case t_FCHS:
	case t_FUNKNOWN:
	case t_FNOTDONE:
		return fpu_flags.sw.c1;
	case t_FCOMP:
		return false;										/* FIXME */
	default:
		E_Exit("FPU_get_C1 Unknown %d",fpu_flags.type);
	}
	return 0;
}

bool FPU_get_C0() {
	switch(fpu_flags.type) {
	case t_FLD:
	case t_FLDST:
	case t_FDIV:
	case t_FDIVP:
	case t_FCHS:
	case t_FUNKNOWN:
	case t_FNOTDONE:
		return fpu_flags.sw.c0;
	case t_FCOMP:
		return (fpu_flags.result.tag!=FPUREG_ZERO&&fpu_flags.result.tag!=FPUREG_PNAN);
	default:
		E_Exit("FPU_get_C0 Unknown %d",fpu_flags.type);
	}
	return 0;
}

bool FPU_get_IR() {
	switch(fpu_flags.type) {
	case t_FLD:
	case t_FLDST:
	case t_FDIV:
	case t_FDIVP:
	case t_FCHS:
	case t_FUNKNOWN:
	case t_FNOTDONE:
	case t_FCOMP:
		return fpu_flags.sw.ir;
	default:
		E_Exit("FPU_get_IR Unknown %d",fpu_flags.type);
	}
	return 0;
}

bool FPU_get_SF() {
	switch(fpu_flags.type) {
	case t_FLD:
	case t_FLDST:
	case t_FDIV:
	case t_FDIVP:
	case t_FCHS:
	case t_FUNKNOWN:
	case t_FNOTDONE:
		return fpu_flags.sw.sf;
	case t_FCOMP:
		return false;										/* FIXME */
	default:
		E_Exit("FPU_get_SF Unknown %d",fpu_flags.type);
	}
	return 0;
}

bool FPU_get_PF() {
	switch(fpu_flags.type) {
	case t_FLD:
	case t_FLDST:
	case t_FDIV:
	case t_FDIVP:
	case t_FCHS:
	case t_FUNKNOWN:
	case t_FNOTDONE:
	case t_FCOMP:
		return fpu_flags.sw.pf;
	default:
		E_Exit("FPU_get_PF Unknown %d",fpu_flags.type);
	}
	return 0;
}

bool FPU_get_UF() {
	switch(fpu_flags.type) {
	case t_FLD:
	case t_FLDST:
	case t_FDIV:
	case t_FDIVP:
	case t_FCHS:
	case t_FUNKNOWN:
	case t_FNOTDONE:
	case t_FCOMP:
		return fpu_flags.sw.uf;
	default:
		E_Exit("FPU_get_UF Unknown %d",fpu_flags.type);
	}
	return 0;
}

bool FPU_get_OF() {
	switch(fpu_flags.type) {
	case t_FLD:
	case t_FLDST:
	case t_FDIV:
	case t_FDIVP:
	case t_FCHS:
	case t_FUNKNOWN:
	case t_FNOTDONE:
	case t_FCOMP:
		return fpu_flags.sw.of;
	default:
		E_Exit("FPU_get_OF Unknown %d",fpu_flags.type);
	}
	return 0;
}

bool FPU_get_ZF() {
	switch(fpu_flags.type) {
	case t_FLD:
	case t_FLDST:
	case t_FCHS:
	case t_FUNKNOWN:
	case t_FNOTDONE:
	case t_FCOMP:
		return fpu_flags.sw.zf;
	case t_FDIV:
	case t_FDIVP:
		return (fpu_flags.result.tag==FPUREG_PNAN||fpu_flags.result.tag==FPUREG_NNAN);
	default:
		E_Exit("FPU_get_ZF Unknown %d",fpu_flags.type);
	}
	return 0;
}

bool FPU_get_DF() {
	switch(fpu_flags.type) {
	case t_FLD:
	case t_FLDST:
	case t_FDIV:
	case t_FDIVP:
	case t_FCHS:
	case t_FUNKNOWN:
	case t_FNOTDONE:
		return fpu_flags.sw.df;
	case t_FCOMP:
		return false;										/* FIXME */
	default:
		E_Exit("FPU_get_DF Unknown %d",fpu_flags.type);
	}
	return 0;
}

bool FPU_get_IN(){
	switch(fpu_flags.type) {
	case t_FLD:
	case t_FLDST:
	case t_FDIV:
	case t_FDIVP:
	case t_FCHS:
	case t_FUNKNOWN:
	case t_FNOTDONE:
		return fpu_flags.sw.in;
	case t_FCOMP:
		return (fpu_flags.result.tag==FPUREG_EMPTY);		/* FIXME */
	default:
		E_Exit("FPU_get_IN Unknown %d",fpu_flags.type);
	}
	return 0;
}
