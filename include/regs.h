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

extern Segment Segs[6];
extern Flag_Info flags;
//extern Regs regs;

void SetSegment_16(Bit32u seg,Bit16u val);


struct CPU_Regs {
	union {
		Bit32u d;
		Bit16u w;
		struct {
			Bit8u l,h;
		}b;
	} ax,bx,cx,dx,si,di,sp,bp,ip;
};

extern CPU_Regs cpu_regs;

#define reg_al cpu_regs.ax.b.l

//extern Bit8u & reg_al=cpu_regs.ax.b.l;

#define reg_ah cpu_regs.ax.b.h
#define reg_ax cpu_regs.ax.w
#define reg_eax cpu_regs.ax.d

#define reg_bl cpu_regs.bx.b.l
#define reg_bh cpu_regs.bx.b.h
#define reg_bx cpu_regs.bx.w
#define reg_ebx cpu_regs.bx.d

#define reg_cl cpu_regs.cx.b.l
#define reg_ch cpu_regs.cx.b.h
#define reg_cx cpu_regs.cx.w
#define reg_ecx cpu_regs.cx.d

#define reg_dl cpu_regs.dx.b.l
#define reg_dh cpu_regs.dx.b.h
#define reg_dx cpu_regs.dx.w
#define reg_edx cpu_regs.dx.d

#define reg_si cpu_regs.si.w
#define reg_esi cpu_regs.si.d

#define reg_di cpu_regs.di.w
#define reg_edi cpu_regs.di.d

#define reg_sp cpu_regs.sp.w
#define reg_esp cpu_regs.sp.d

#define reg_bp cpu_regs.bp.w
#define reg_ebp cpu_regs.bp.d

#define reg_ip cpu_regs.ip.w
#define reg_eip cpu_regs.ip.d





#endif

