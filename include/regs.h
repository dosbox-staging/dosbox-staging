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

#if !defined __REGS_H
#define __REGS_H

#include <mem.h>

struct Flag_Info {
    union {
		Bit8u b;
		Bit16u w;
		Bit32u d;
	} var1,var2,result;
	Bitu type;
	Bitu prev_type;
	bool cf,sf,pf,af,zf,of,df,tf,intf;
	bool nt;
	Bit8u io;
	bool oldcf;
};

struct Segment {
	Bit16u value;
	bool special;							/* Signal for pointing to special memory */
	HostPt host;							/* The address of start in host memory */
	PhysPt	phys;							/* The phyiscal address start in emulated machine */
};

enum { cs=0,ds,es,fs,gs,ss};

struct CPU_Regs {
	union {
		Bit32u d;
		Bit16u w;
		struct {
			Bit8u l,h;
		}b;
	} regs[8],ip;
};

extern Segment Segs[6];
extern Flag_Info flags;
extern CPU_Regs cpu_regs;

void SetSegment_16(Bit32u seg,Bit16u val);


//extern Bit8u & reg_al=cpu_regs.ax.b.l;
enum REG_NUM {
	REG_NUM_AX, REG_NUM_CX, REG_NUM_DX, REG_NUM_BX,
	REG_NUM_SP, REG_NUM_BP, REG_NUM_SI, REG_NUM_DI
};

//macros to convert a 3-bit register index to the correct register
#define reg_8l(reg) (cpu_regs.regs[(reg)].b.l)
#define reg_8h(reg) (cpu_regs.regs[(reg)].b.h)
#define reg_8(reg) ((reg) & 4 ? reg_8h((reg) & 3) : reg_8l((reg) & 3))
#define reg_16(reg) (cpu_regs.regs[(reg)].w)
#define reg_32(reg) (cpu_regs.regs[(reg)].d)

#define reg_al cpu_regs.regs[REG_NUM_AX].b.l
#define reg_ah cpu_regs.regs[REG_NUM_AX].b.h
#define reg_ax cpu_regs.regs[REG_NUM_AX].w
#define reg_eax cpu_regs.regs[REG_NUM_AX].d

#define reg_bl cpu_regs.regs[REG_NUM_BX].b.l
#define reg_bh cpu_regs.regs[REG_NUM_BX].b.h
#define reg_bx cpu_regs.regs[REG_NUM_BX].w
#define reg_ebx cpu_regs.regs[REG_NUM_BX].d

#define reg_cl cpu_regs.regs[REG_NUM_CX].b.l
#define reg_ch cpu_regs.regs[REG_NUM_CX].b.h
#define reg_cx cpu_regs.regs[REG_NUM_CX].w
#define reg_ecx cpu_regs.regs[REG_NUM_CX].d

#define reg_dl cpu_regs.regs[REG_NUM_DX].b.l
#define reg_dh cpu_regs.regs[REG_NUM_DX].b.h
#define reg_dx cpu_regs.regs[REG_NUM_DX].w
#define reg_edx cpu_regs.regs[REG_NUM_DX].d

#define reg_si cpu_regs.regs[REG_NUM_SI].w
#define reg_esi cpu_regs.regs[REG_NUM_SI].d

#define reg_di cpu_regs.regs[REG_NUM_DI].w
#define reg_edi cpu_regs.regs[REG_NUM_DI].d

#define reg_sp cpu_regs.regs[REG_NUM_SP].w
#define reg_esp cpu_regs.regs[REG_NUM_SP].d

#define reg_bp cpu_regs.regs[REG_NUM_BP].w
#define reg_ebp cpu_regs.regs[REG_NUM_BP].d

#define reg_ip cpu_regs.ip.w
#define reg_eip cpu_regs.ip.d

#endif

