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

extern bool parity_lookup[256];

void CPU_LLDT(Bitu selector);
void CPU_LTR(Bitu selector);
void CPU_LIDT(Bitu limit,Bitu base);
void CPU_LGDT(Bitu limit,Bitu base);

void CPU_STR(Bitu & selector);
void CPU_SLDT(Bitu & selector);
void CPU_SIDT(Bitu & limit,Bitu & base);
void CPU_SGDT(Bitu & limit,Bitu & base);


void CPU_ARPL(Bitu & dest_sel,Bitu src_sel);
void CPU_LAR(Bitu selector,Bitu & ar);
void CPU_LSL(Bitu selector,Bitu & limit);

bool CPU_SET_CRX(Bitu cr,Bitu value);
Bitu CPU_GET_CRX(Bitu cr);

void CPU_SMSW(Bitu & word);
bool CPU_LMSW(Bitu word);

void CPU_VERR(Bitu selector);
void CPU_VERW(Bitu selector);

bool CPU_JMP(bool use32,Bitu selector,Bitu offset);
bool CPU_CALL(bool use32,Bitu selector,Bitu offset);
bool CPU_RET(bool use32,Bitu bytes);

bool Interrupt(Bitu num);
bool CPU_IRET(bool use32);
bool CPU_SetSegGeneral(SegNames seg,Bitu value);

void CPU_CPUID(void);
void CPU_HLT(void);

Bitu CPU_Pop16(void);
Bitu CPU_Pop32(void);
void CPU_Push16(Bitu value);
void CPU_Push32(Bitu value);

//Flag Handling
Bitu get_CF(void);
Bitu get_AF(void);
Bitu get_ZF(void);
Bitu get_SF(void);
Bitu get_OF(void);
Bitu get_PF(void);


#define SETFLAGSb(FLAGB)													\
{																			\
	SETFLAGBIT(OF,get_OF());												\
	flags.type=t_UNKNOWN;													\
	flags.word&=0xffffff00;flags.word|=(FLAGB&0xff);						\
}

#define SETFLAGSw(FLAGW)													\
{																			\
	flags.type=t_UNKNOWN;													\
	flags.word&=0xffff0000;flags.word|=(FLAGW&0xffff);						\
}

#define SETFLAGSd(FLAGD)													\
{																			\
	flags.type=t_UNKNOWN;													\
	flags.word=FLAGD;														\
}

#define FILLFLAGS															\
{																			\
	flags.word=(flags.word & ~FLAG_MASK) |									\
	(get_CF() ? FLAG_CF : 0 ) |												\
	(get_PF() ? FLAG_PF : 0 ) |												\
	(get_AF() ? FLAG_AF : 0 ) |												\
	(get_ZF() ? FLAG_ZF : 0 ) |												\
	(get_SF() ? FLAG_SF : 0 ) |												\
	(get_OF() ? FLAG_OF : 0 );												\
	 flags.type=t_UNKNOWN;													\
}		

#define LoadCF SETFLAGBIT(CF,get_CF());
#define LoadZF SETFLAGBIT(ZF,get_ZF());
#define LoadSF SETFLAGBIT(SF,get_SF());
#define LoadOF SETFLAGBIT(OF,get_OF());
#define LoadAF SETFLAGBIT(AF,get_AF());

// *********************************************************************
// Descriptor
// *********************************************************************

#define CR0_PROTECTION		0x00000001
#define CR0_FPUENABLED		0x00000002
#define CR0_FPUMONITOR		0x00000004
#define CR0_TASKSWITCH		0x00000008
#define CR0_FPUPRESENT		0x00000010
#define CR0_PAGING			0x80000000


#define DESC_INVALID				0x00
#define DESC_286_TSS_A				0x01
#define DESC_LDT					0x02
#define DESC_286_TSS_B				0x03
#define DESC_286_CALL_GATE			0x04
#define DESC_TASK_GATE				0x05
#define DESC_286_INT_GATE			0x06
#define DESC_286_TRAP_GATE			0x07

#define DESC_386_TSS_A				0x09
#define DESC_386_TSS_B				0x0b
#define DESC_386_CALL_GATE			0x0c
#define DESC_386_INT_GATE			0x0e
#define DESC_386_TRAP_GATE			0x0f

/* EU/ED Expand UP/DOWN RO/RW Read Only/Read Write NA/A Accessed */
#define DESC_DATA_EU_RO_NA			0x10
#define DESC_DATA_EU_RO_A			0x11
#define DESC_DATA_EU_RW_NA			0x12
#define DESC_DATA_EU_RW_A			0x13
#define DESC_DATA_ED_RO_NA			0x14
#define DESC_DATA_ED_RO_A			0x15
#define DESC_DATA_ED_RW_NA			0x16
#define DESC_DATA_ED_RW_A			0x17

/* N/R Readable  NC/C Confirming A/NA Accessed */
#define DESC_CODE_N_NC_A			0x18
#define DESC_CODE_N_NC_NA			0x19
#define DESC_CODE_R_NC_A			0x1a
#define DESC_CODE_R_NC_NA			0x1b
#define DESC_CODE_N_C_A				0x1c
#define DESC_CODE_N_C_NA			0x1d
#define DESC_CODE_R_C_A				0x1e
#define DESC_CODE_R_C_NA			0x1f

#pragma pack(1)
/* TSS Struct from Bochs - http://bochs.sf.net */
struct TSS_386 {
    Bit16u back, RESERVED0;      /* Backlink */
    Bit32u esp0;                 /* The CK stack pointer */
    Bit16u ss0,  RESERVED1;      /* The CK stack selector */
    Bit32u esp1;                 /* The parent KL stack pointer */
    Bit16u ss1,  RESERVED2;      /* The parent KL stack selector */
    Bit32u esp2;                 /* Unused */
    Bit16u ss2,  RESERVED3;      /* Unused */
    Bit32u cr3;                  /* The page directory pointer */
    Bit32u eip;                  /* The instruction pointer */
    Bit32u eflags;               /* The flags */
    Bit32u eax, ecx, edx, ebx;   /* The general purpose registers */
    Bit32u esp, ebp, esi, edi;   /* The special purpose registers */
    Bit16u es,   RESERVED4;      /* The extra selector */
    Bit16u cs,   RESERVED5;      /* The code selector */
    Bit16u ss,   RESERVED6;      /* The application stack selector */
    Bit16u ds,   RESERVED7;      /* The data selector */
    Bit16u fs,   RESERVED8;      /* And another extra selector */
    Bit16u gs,   RESERVED9;      /* ... and another one */
    Bit16u ldt,  RESERVED10;     /* The local descriptor table */
    Bit16u trap;                 /* The trap flag (for debugging) */
    Bit16u io;                   /* The I/O Map base address */
} GCC_ATTRIBUTE(packed);;

struct S_Descriptor {
	Bit32u limit_0_15	:16;
	Bit32u base_0_15	:16;
	Bit32u base_16_23	:8;
	Bit32u type			:5;
	Bit32u dpl			:2;
	Bit32u p			:1;
	Bit32u limit_16_19	:4;
	Bit32u avl			:1;
	Bit32u r			:1;
	Bit32u big			:1;
	Bit32u g			:1;
	Bit32u base_24_31	:8;
}GCC_ATTRIBUTE(packed);

struct G_Descriptor {
	Bit32u offset_0_15	:16;
	Bit32u selector		:16;
	Bit32u paramcount	:5;
	Bit32u reserved		:3;
	Bit32u type			:5;
	Bit32u dpl			:2;
	Bit32u p			:1;
	Bit32u offset_16_31	:16;
} GCC_ATTRIBUTE(packed);

#pragma pack()

class TaskStateSegment 
{
public:
	bool Get_ss_esp(Bitu which,Bitu & _ss,Bitu & _esp) {
		PhysPt reader=seg_base+offsetof(TSS_386,esp0)+which*8;
		_esp=mem_readd(reader);
		_ss=mem_readw(reader+4);
		return true;
	}
	bool Get_cr3(Bitu which,Bitu & _cr3) {
		_cr3=mem_readd(seg_base+offsetof(TSS_386,cr3));;
		return true;
	}
	
private:
	PhysPt seg_base;
	Bitu seg_limit;
	Bitu seg_value;
};


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
	Bitu GetOffset(void) {
		return (saved.gate.offset_16_31 << 16) | saved.gate.offset_0_15;
	}
	Bitu GetSelector(void) {
		return saved.gate.selector;
	}
	Bitu Type(void) {
		return saved.seg.type;
	}
	Bitu Conforming(void) {
		return saved.seg.type & 8;
	}
	Bitu DPL(void) {
		return saved.seg.dpl;
	}
	Bitu Big(void) {
		return saved.seg.big;
	}
public:
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
		Bitu address=selector & ~7;
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

#define STATE_PROTECTED			0x0001
#define STATE_USE32				0x0002
#define STATE_STACK32			0x0004

struct CPUBlock {
	Bitu cpl;							/* Current Privilege */
	Bitu state;
	Bitu cr0;
	GDTDescriptorTable gdt;
	DescriptorTable idt;
	struct {
		Bitu prefix,entry;
	} full;
	struct {
		Bitu eip,cs;
	} hlt;
};

extern CPUBlock cpu;

#endif

