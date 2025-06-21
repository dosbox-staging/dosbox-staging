// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

/* Write the data from the opcode */
switch (inst.code.save) {
/* Byte */
	case S_C_Eb:
		inst_op1_b=inst.cond ? 1 : 0;
		[[fallthrough]];
	case S_Eb:
		if (inst.rm<0xc0) SaveMb(inst.rm_eaa,inst_op1_b);
		else reg_8(static_cast<uint8_t>(inst.rm_eai))=inst_op1_b;
		break;
	case S_Gb:
		reg_8(static_cast<uint8_t>(inst.rm_index))=inst_op1_b;
		break;
	case S_EbGb:
		if (inst.rm<0xc0) SaveMb(inst.rm_eaa,inst_op1_b);
		else reg_8(static_cast<uint8_t>(inst.rm_eai))=inst_op1_b;
		reg_8(static_cast<uint8_t>(inst.rm_index))=inst_op2_b;
		break;
/* Word */
	case S_Ew:
		if (inst.rm<0xc0) SaveMw(inst.rm_eaa,inst_op1_w);
		else reg_16(static_cast<uint8_t>(inst.rm_eai))=inst_op1_w;
		break;
	case S_Gw:
		reg_16(static_cast<uint8_t>(inst.rm_index))=inst_op1_w;
		break;
	case S_EwGw:
		if (inst.rm<0xc0) SaveMw(inst.rm_eaa,inst_op1_w);
		else reg_16(static_cast<uint8_t>(inst.rm_eai))=inst_op1_w;
		reg_16(static_cast<uint8_t>(inst.rm_index))=inst_op2_w;
		break;
/* Dword */
	case S_Ed:
		if (inst.rm<0xc0) SaveMd(inst.rm_eaa,inst_op1_d);
		else reg_32(static_cast<uint8_t>(inst.rm_eai))=inst_op1_d;
		break;
	case S_EdMw:		/* Special one 16 to memory, 32 zero extend to reg */
		if (inst.rm<0xc0) SaveMw(inst.rm_eaa,inst_op1_w);
		else reg_32(static_cast<uint8_t>(inst.rm_eai))=inst_op1_d;
		break;
	case S_Gd:
		reg_32(static_cast<uint8_t>(inst.rm_index))=inst_op1_d;
		break;
	case S_EdGd:
		if (inst.rm<0xc0) SaveMd(inst.rm_eaa,inst_op1_d);
		else reg_32(static_cast<uint8_t>(inst.rm_eai))=inst_op1_d;
		reg_32(static_cast<uint8_t>(inst.rm_index))=inst_op2_d;
		break;

	case S_REGb:
		reg_8(inst.code.extra)=inst_op1_b;
		break;
	case S_REGw:
		reg_16(inst.code.extra)=inst_op1_w;
		break;
	case S_REGd:
		reg_32(inst.code.extra)=inst_op1_d;
		break;
	case S_SEGm:
		if (CPU_SetSegGeneral((SegNames)inst.rm_index,inst_op1_w)) RunException();
		break;
	case S_SEGGw:
		if (CPU_SetSegGeneral((SegNames)inst.code.extra,inst_op2_w)) RunException();
		reg_16(static_cast<uint8_t>(inst.rm_index))=inst_op1_w;
		break;
	case S_SEGGd:
		if (CPU_SetSegGeneral((SegNames)inst.code.extra,inst_op2_w)) RunException();
		reg_32(static_cast<uint8_t>(inst.rm_index))=inst_op1_d;
		break;
	case S_PUSHw:
		Push_16(inst_op1_w);
		break;
	case S_PUSHd:
		Push_32(inst_op1_d);
		break;

	case S_C_AIPw:
		if (!inst.cond) goto nextopcode;
		[[fallthrough]];
	case S_AIPw:
		SaveIP();
		reg_eip+=inst_op1_d;
		reg_eip&=0xffff;
		continue;
	case S_C_AIPd:
		if (!inst.cond) goto nextopcode;
		[[fallthrough]];
	case S_AIPd:
		SaveIP();
		reg_eip+=inst_op1_d;
		continue;
	case S_IPIw:
		reg_esp+=Fetchw();
		[[fallthrough]];
	case S_IP:
		SaveIP();
		reg_eip=inst_op1_d;
		continue;
	case 0:
		break;
	default:
		LOG(LOG_CPU,LOG_ERROR)("SAVE:Unhandled code %d entry %X",inst.code.save,static_cast<uint32_t>(inst.entry));
}
