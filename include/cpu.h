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

/* Some common Defines */
/* A CPU Handler */
typedef Bitu (CPU_Decoder)(void);
extern CPU_Decoder * cpudecoder;

/* CPU Cycle Timing */
extern Bits CPU_Cycles;
extern Bits CPU_CycleLeft;
extern Bits CPU_CycleMax;

//CPU Stuff
void SetCPU16bit();

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

class Descriptor
{
public:
	void LoadValues	(Bit32u address) {
		Bit32u* data = (Bit32u*)&desc;
		*data	  = mem_readd(address);
		*(data+1) = mem_readd(address+1);
	}
	void SaveValues	(Bit32u address) {
		Bit32u* data = (Bit32u*)&desc;
		mem_writed(address,*data);
		mem_writed(address+1,*(data+1));
	}
	Bit32u	GetBase		(void) { return (desc.base_24_31<<24) | (desc.base_16_23<<16) | desc.base_0_15; };
	Bit32u	GetLimit	(void) {
		Bit32u limit = (desc.limit_16_19<<16) | desc.limit_0_15;
		if (desc.g)	return (limit<<12) | 0xFFF;
		return limit;
	}

public:
#pragma pack(1)
	typedef struct SDescriptor {
		Bit32u limit_0_15	:16;
		Bit32u base_0_15	:16;
		Bit32u base_16_23	:8;
		Bit32u type			:5;
		Bit32u dpl			:2;
		Bit32u p			:1;
		Bit32u limit_16_19	:4;
		Bit32u avl			:1;
		Bit32u r			:1;
		Bit32u d			:1;
		Bit32u g			:1;
		Bit32u base_24_31	:8;
	} TDescriptor;
#pragma pack()
	
	TDescriptor desc;
};

class DescriptorTable
{
	Bit32u	GetBase			(void)			{ return baseAddress;	};
	Bit16u	GetLimit		(void)			{ return limit;			};
	void	SetBase			(Bit32u base)	{ baseAddress = base;	};
	void	SetLimit		(Bit16u size)	{ limit		  = size;	};

	bool GetDescriptor	(Bit16u selector, Descriptor& desc) {
			// selector = the plain index 
		if (selector>=limit>>3) return false;
		desc.LoadValues(baseAddress+(selector<<3));
		return true;
	};

private:
	Bit32u  baseAddress;
	Bit16u	limit;
};

#endif

