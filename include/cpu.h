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

#ifndef __CPU_H
#define __CPU_H

#include "dosbox.h" 
#include "regs.h"
#include "mem.h"

/* CPU Cycle Timing */
extern Bits CPU_Cycles;
extern Bits CPU_CycleLeft;
extern Bits CPU_CycleMax;

/* Some common Defines */
/* A CPU Handler */
typedef Bitu (CPU_Decoder)(void);
extern CPU_Decoder * cpudecoder;


//CPU Stuff
void SetCPU16bit( );

//Types of Flag changing instructions
enum {
	t_UNKNOWN=0,
	t_ADDb,t_ADDw,t_ADDd, 
	t_ORb,t_ORw,t_ORd, 
	t_ADCb,t_ADCw,t_ADCd,
	t_SBBb,t_SBBw,t_SBBd,
	t_ANDb,t_ANDw,t_ANDd,
	t_SUBb,t_SUBw,t_SUBd,
	t_XORb,t_XORw,t_XORd,
	t_CMPb,t_CMPw,t_CMPd,
	t_INCb,t_INCw,t_INCd,
	t_DECb,t_DECw,t_DECd,
	t_TESTb,t_TESTw,t_TESTd,
	t_SHLb,t_SHLw,t_SHLd,
	t_SHRb,t_SHRw,t_SHRd,
	t_SARb,t_SARw,t_SARd,
	t_ROLb,t_ROLw,t_ROLd,
	t_RORb,t_RORw,t_RORd,
	t_RCLb,t_RCLw,t_RCLd,
	t_RCRb,t_RCRw,t_RCRd,
	t_NEGb,t_NEGw,t_NEGd,
	t_CF,t_ZF,

	t_DSHLw,t_DSHLd,
	t_DSHRw,t_DSHRd,
	t_MUL,t_DIV,
	t_NOTDONE,
	t_LASTFLAG
};

void Interrupt(Bit8u num);

//Flag Handling
bool get_CF(void);
bool get_AF(void);
bool get_ZF(void);
bool get_SF(void);
bool get_OF(void);
bool get_PF(void);


#define FLAG_CF 0x0001
#define FLAG_PF 0x0004
#define FLAG_AF 0x0010
#define FLAG_ZF 0x0040
#define FLAG_SF 0x0080
#define FLAG_TF 0x0100
#define FLAG_IF 0x0200
#define FLAG_DF 0x0400
#define FLAG_OF 0x0800



#define LoadCF flags.cf=get_CF();
#define LoadZF flags.zf=get_ZF();
#define LoadSF flags.sf=get_SF();
#define LoadOF flags.of=get_OF();


// *********************************************************************
// Descriptor
// *********************************************************************

#define CR0_PROTECTION		0x00000001
#define CR0_FPUENABLED		0x00000002
#define CR0_FPUMONITOR		0x00000004
#define CR0_TASKSWITCH		0x00000008
#define CR0_FPUPRESENT		0x00000010
#define CR0_PAGING			0x80000000


#define DESC_INVALID				0x0
#define DESC_286_TSS_A				0x1
#define DESC_LDT					0x2
#define DESC_286_TSS_B				0x3
#define DESC_286_CALL_GATE			0x4
#define DESC_TASK_GATE				0x5
#define DESC_286_INT_GATE			0x6
#define DESC_286_TRAP_GATE			0x7

#define DESC_386_TSS_A				0x9
#define DESC_386_TSS_B				0xb
#define DESC_386_CALL_GATE			0xc
#define DESC_386_INT_GATE			0xe
#define DESC_386_TRAP_GATE			0xf


class Descriptor
{
public:
	Descriptor() { saved.fill[0]=saved.fill[1]=0; }

	void Load(PhysPt address) {
		Bit32u* data = (Bit32u*)&saved;
		*data	  = mem_readd(address);
		*(data+1) = mem_readd(address+4);
	}
	void Save(PhysPt address) {
		Bit32u* data = (Bit32u*)&saved;
		mem_writed(address,*data);
		mem_writed(address+4,*(data+1));
	}
	PhysPt GetBase (void) { 
		return (saved.seg.base_24_31<<24) | (saved.seg.base_16_23<<16) | saved.seg.base_0_15; 
	}
	Bitu GetLimit (void) {
		Bitu limit = (saved.seg.limit_16_19<<16) | saved.seg.limit_0_15;
		if (saved.seg.g)	return (limit<<12) | 0xFFF;
		return limit;
	}
	Bitu GetType(void) {
		return saved.seg.type;
	}
	Bitu GetOffset(void) {
		return (saved.gate.offset_16_31 << 16) | saved.gate.offset_0_15;
	}
	Bitu Conforming(void) {
		return saved.seg.type & 8;
	}
	Bitu GetDPL(void) {
		return saved.seg.dpl;
	}
public:
#pragma pack(1)
	struct S_Descriptor {
		Bit32u limit_0_15	:16;
		Bit32u base_0_15	:16;
		Bit32u base_16_23	:8;
		Bit32u type			:4;
		Bit32u s			:1;
		Bit32u dpl			:2;
		Bit32u p			:1;
		Bit32u limit_16_19	:4;
		Bit32u avl			:1;
		Bit32u r			:1;
		Bit32u big			:1;
		Bit32u g			:1;
		Bit32u base_24_31	:8;
	};
	struct G_Descriptor {
		Bit32u offset_0_15	:16;
		Bit32u selector		:16;
		Bit32u paramcount	:5;
		Bit32u reserved		:3;
		Bit32u type			:4;
		Bit32u s			:1;
		Bit32u dpl			:2;
		Bit32u p			:1;
		Bit32u offset_16_31	:16;
	};
#pragma pack()

	union {
		S_Descriptor seg;
		G_Descriptor gate;
		Bit32u fill[2];
	} saved;
};

class DescriptorTable {
public:
	PhysPt	GetBase			(void)			{ return table_base;	}
	Bitu	GetLimit		(void)			{ return table_limit;	}
	void	SetBase			(PhysPt _base)	{ table_base = _base;	}
	void	SetLimit		(Bitu _limit)	{ table_limit= _limit;	}

	bool GetDescriptor	(Bitu selector, Descriptor& desc) {
		selector&=~7;
		if (selector>=table_limit) return false;
		desc.Load(table_base+(selector));
		return true;
	}
protected:
	PhysPt table_base;
	Bitu table_limit;
};

class GDTDescriptorTable : public DescriptorTable {
public:
	bool GetDescriptor	(Bitu selector, Descriptor& desc) {
		Bitu address=selector&=~7;
		if (selector & 4) {
			if (address>=ldt_limit) return false;
			desc.Load(ldt_base+address);
			return true;
		} else {
			if (address>=table_limit) return false;
			desc.Load(table_base+address);
			return true;
		}
	}

	Bitu SLDT(void)	{
		return ldt_value;
	}
	bool LLDT(Bitu value)	{
//TODO checking
		Descriptor desc;
		GetDescriptor(value,desc);
		ldt_base=desc.GetBase();
		ldt_limit=desc.GetLimit();
		ldt_value=value;
		return true;
	}
private:
	PhysPt ldt_base;
	Bitu ldt_limit;
	Bitu ldt_value;
};

struct CPUBlock {
	Bitu protmode;						/* Are we in protected mode */
	Bitu cpl;							/* Current Privilege */
	Bitu conforming;					/* Current descriptor is conforming */
	Bitu big;							/* Current descriptor is USE32 */
	Bitu state;
	Bitu cr0;
	GDTDescriptorTable gdt;
	DescriptorTable idt;
	struct {
		Bitu prefix,entry;
	} full;
};

extern CPUBlock cpu;

#endif

