// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

{
	const auto si_base = (inst.prefix & PREFIX_SEG) ? inst.seg.base
	                                                : SegBase(ds);
	const auto di_base = SegBase(es);

	const auto is_32bit_addr = (inst.prefix & PREFIX_ADDR) != 0;

	const uint32_t add_mask = is_32bit_addr ? 0xFFFFFFFF : 0xFFFF;
	uint32_t si_index       = is_32bit_addr ? reg_esi : reg_si;
	uint32_t di_index       = is_32bit_addr ? reg_edi : reg_di;

	// Count has to be large enough to both hold an unsigned 32-bit
	// value and also be used as a signed counter. To test the
	// maximum, press enter during Frontier First Encounter's intro
	// sequence on an arm64 or PPC platform, in which case all bits
	// of ECX will be set.
	//
	int64_t count = is_32bit_addr ? reg_ecx : reg_cx;

	int32_t count_left = 0;

	if (!(inst.prefix & PREFIX_REP)) {
		count=1;
	} else {
		/* Calculate amount of ops to do before cycles run out */
		CPU_Cycles++;
		if ((count > CPU_Cycles) && (inst.code.op < R_SCASB)) {
			count_left=count-CPU_Cycles;
			count=CPU_Cycles;
			CPU_Cycles=0;
			LoadIP();
		} else {
			/* Won't interrupt scas and cmps instruction since they can interrupt themselves */
			if (inst.code.op<R_SCASB) CPU_Cycles-=count;
			count_left=0;
		}
	}
	auto add_index = cpu.direction;
	if (count) switch (inst.code.op) {
	case R_OUTSB:
		for (;count>0;count--) {
			IO_WriteB(reg_dx,LoadMb(si_base+si_index));
			si_index=(si_index+add_index) & add_mask;
		}
		break;
	case R_OUTSW:
		add_index *= 2;
		for (;count>0;count--) {
			IO_WriteW(reg_dx,LoadMw(si_base+si_index));
			si_index=(si_index+add_index) & add_mask;
		}
		break;
	case R_OUTSD:
		add_index *= 4;
		for (;count>0;count--) {
			IO_WriteD(reg_dx,LoadMd(si_base+si_index));
			si_index=(si_index+add_index) & add_mask;
		}
		break;
	case R_INSB:
		for (;count>0;count--) {
			SaveMb(di_base+di_index,IO_ReadB(reg_dx));
			di_index=(di_index+add_index) & add_mask;
		}
		break;
	case R_INSW:
		add_index *= 2;
		for (;count>0;count--) {
			SaveMw(di_base+di_index,IO_ReadW(reg_dx));
			di_index=(di_index+add_index) & add_mask;
		}
		break;
	case R_INSD:
		add_index *= 4;
		for (;count>0;count--) {
			SaveMd(di_base+di_index,IO_ReadD(reg_dx));
			di_index=(di_index+add_index) & add_mask;
		}
		break;
	case R_STOSB:
		for (;count>0;count--) {
			SaveMb(di_base+di_index,reg_al);
			di_index=(di_index+add_index) & add_mask;
		}
		break;
	case R_STOSW:
		add_index *= 2;
		for (;count>0;count--) {
			SaveMw(di_base+di_index,reg_ax);
			di_index=(di_index+add_index) & add_mask;
		}
		break;
	case R_STOSD:
		add_index *= 4;
		for (;count>0;count--) {
			SaveMd(di_base+di_index,reg_eax);
			di_index=(di_index+add_index) & add_mask;
		}
		break;
	case R_MOVSB:
		for (;count>0;count--) {
			SaveMb(di_base+di_index,LoadMb(si_base+si_index));
			di_index=(di_index+add_index) & add_mask;
			si_index=(si_index+add_index) & add_mask;
		}
		break;
	case R_MOVSW:
		add_index *= 2;
		for (;count>0;count--) {
			SaveMw(di_base+di_index,LoadMw(si_base+si_index));
			di_index=(di_index+add_index) & add_mask;
			si_index=(si_index+add_index) & add_mask;
		}
		break;
	case R_MOVSD:
		add_index *= 4;
		for (;count>0;count--) {
			SaveMd(di_base+di_index,LoadMd(si_base+si_index));
			di_index=(di_index+add_index) & add_mask;
			si_index=(si_index+add_index) & add_mask;
		}
		break;
	case R_LODSB:
		for (;count>0;count--) {
			reg_al=LoadMb(si_base+si_index);
			si_index=(si_index+add_index) & add_mask;
		}
		break;
	case R_LODSW:
		add_index *= 2;
		for (;count>0;count--) {
			reg_ax=LoadMw(si_base+si_index);
			si_index=(si_index+add_index) & add_mask;
		}
		break;
	case R_LODSD:
		add_index *= 4;
		for (;count>0;count--) {
			reg_eax=LoadMd(si_base+si_index);
			si_index=(si_index+add_index) & add_mask;
		}
		break;
	case R_SCASB:
		{
			uint8_t val2 = 0;
			for (;count>0;) {
				count--;CPU_Cycles--;
				val2=LoadMb(di_base+di_index);
				di_index=(di_index+add_index) & add_mask;
				if ((reg_al==val2)!=inst.repz) break;
			}
			CMPB(reg_al,val2,LoadD,0);
		}
		break;
	case R_SCASW:
		{
			add_index *= 2;
			uint16_t val2 = 0;
			for (;count>0;) {
				count--;CPU_Cycles--;
				val2=LoadMw(di_base+di_index);
				di_index=(di_index+add_index) & add_mask;
				if ((reg_ax==val2)!=inst.repz) break;
			}
			CMPW(reg_ax,val2,LoadD,0);
		}
		break;
	case R_SCASD:
		{
			add_index *= 4;
			uint32_t val2 = 0;
			for (;count>0;) {
				count--;CPU_Cycles--;
				val2=LoadMd(di_base+di_index);
				di_index=(di_index+add_index) & add_mask;
				if ((reg_eax==val2)!=inst.repz) break;
			}
			CMPD(reg_eax,val2,LoadD,0);
		}
		break;
	case R_CMPSB:
		{
			uint8_t val1 = 0;
			uint8_t val2 = 0;
			for (;count>0;) {
				count--;CPU_Cycles--;
				val1=LoadMb(si_base+si_index);
				val2=LoadMb(di_base+di_index);
				si_index=(si_index+add_index) & add_mask;
				di_index=(di_index+add_index) & add_mask;
				if ((val1==val2)!=inst.repz) break;
			}
			CMPB(val1,val2,LoadD,0);
		}
		break;
	case R_CMPSW:
		{
			add_index *= 2;
			uint16_t val1 = 0;
			uint16_t val2 = 0;
			for (;count>0;) {
				count--;CPU_Cycles--;
				val1=LoadMw(si_base+si_index);
				val2=LoadMw(di_base+di_index);
				si_index=(si_index+add_index) & add_mask;
				di_index=(di_index+add_index) & add_mask;
				if ((val1==val2)!=inst.repz) break;
			}
			CMPW(val1,val2,LoadD,0);
		}
		break;
	case R_CMPSD:
		{
			add_index *= 4;
			uint32_t val1 = 0;
			uint32_t val2 = 0;
			for (;count>0;) {
				count--;CPU_Cycles--;
				val1=LoadMd(si_base+si_index);
				val2=LoadMd(di_base+di_index);
				si_index=(si_index+add_index) & add_mask;
				di_index=(di_index+add_index) & add_mask;
				if ((val1==val2)!=inst.repz) break;
			}
			CMPD(val1,val2,LoadD,0);
		}
		break;
	default:
		LOG(LOG_CPU, LOG_ERROR)
		("Unhandled string %d entry %X", inst.code.op,
		 static_cast<uint32_t>(inst.entry));
	}
	/* Clean up after certain amount of instructions */
	reg_esi&=(~add_mask);
	reg_esi|=(si_index & add_mask);
	reg_edi&=(~add_mask);
	reg_edi|=(di_index & add_mask);
	if (inst.prefix & PREFIX_REP) {
		count+=count_left;
		reg_ecx&=(~add_mask);
		reg_ecx|=(count & add_mask);
	}
}
