/* Write the data from the opcode */
switch (inst.code.save) {
/* Byte */
	case S_C_Eb:
		inst.op1.b=inst.cond;
	case S_Eb:
		if (inst.rm<0xc0) SaveMb(inst.rm_eaa,inst.op1.b);
		else reg_8(inst.rm_eai)=inst.op1.b;
		break;	
	case S_Gb:
		reg_8(inst.rm_index)=inst.op1.b;
		break;	
	case S_EbGb:
		if (inst.rm<0xc0) SaveMb(inst.rm_eaa,inst.op1.b);
		else reg_8(inst.rm_eai)=inst.op1.b;
		reg_8(inst.rm_index)=inst.op2.b;
		break;	
/* Word */
	case S_Ew:
		if (inst.rm<0xc0) SaveMw(inst.rm_eaa,inst.op1.w);
		else reg_16(inst.rm_eai)=inst.op1.w;
		break;	
	case S_Gw:
		reg_16(inst.rm_index)=inst.op1.w;
		break;	
	case S_EwGw:
		if (inst.rm<0xc0) SaveMw(inst.rm_eaa,inst.op1.w);
		else reg_16(inst.rm_eai)=inst.op1.w;
		reg_16(inst.rm_index)=inst.op2.w;
		break;	
/* Dword */
	case S_Ed:
		if (inst.rm<0xc0) SaveMd(inst.rm_eaa,inst.op1.d);
		else reg_32(inst.rm_eai)=inst.op1.d;
		break;	
	case S_Gd:
		reg_32(inst.rm_index)=inst.op1.d;
		break;	
	case S_EdGd:
		if (inst.rm<0xc0) SaveMd(inst.rm_eaa,inst.op1.d);
		else reg_32(inst.rm_eai)=inst.op1.d;
		reg_32(inst.rm_index)=inst.op2.d;
		break;	

	case S_REGb:
		reg_8(inst.code.extra)=inst.op1.b;
		break;
	case S_REGw:
		reg_16(inst.code.extra)=inst.op1.w;
		break;
	case S_REGd:
		reg_32(inst.code.extra)=inst.op1.d;
		break;	
	case S_SEGI:
		SegSet16(inst.code.extra,inst.op1.w);
		break;
	case S_SEGm:
		SegSet16(inst.rm_index,inst.op1.w);
		break;
	case S_SEGGw:
		reg_16(inst.rm_index)=inst.op1.w;
		SegSet16(inst.code.extra,inst.op2.w);
		break;
	case S_PUSHw:
		Push_16(inst.op1.w);
		break;
	case S_PUSHd:
		Push_32(inst.op1.d);
		break;

	case S_C_ADDIP:
		if (!inst.cond) goto nextopcode;
	case S_ADDIP:
		SaveIP();
		reg_eip+=inst.op1.d;
		reg_eip&=0xffff;
		LoadIP();
		break;
	case S_CSIPIw:
		reg_esp+=Fetchw();
	case S_CSIP:
		reg_eip=inst.op1.d;
		SegSet16(cs,inst.op2.w);
		LoadIP();
		break;
	case S_IPIw:
		reg_esp+=Fetchw();
	case S_IP:
		SaveIP();
		reg_eip=inst.op1.d;
		LoadIP();
		break;
	case S_FLGb:
		flags.type=t_UNKNOWN;											\
		flags.cf	=(inst.op1.d & 0x001)>0;flags.pf	=(inst.op1.d & 0x004)>0;		\
		flags.af	=(inst.op1.d & 0x010)>0;flags.zf	=(inst.op1.d & 0x040)>0;		\
		flags.sf	=(inst.op1.d & 0x080)>0;
		break;
	case S_FLGw:
		Save_Flagsw(inst.op1.w);
		break;
	case 0:
		break;
	default:
		LOG(LOG_ERROR|LOG_CPU,"SAVE:Unhandled code %d entry %X",inst.code.save,inst.entry);
}
