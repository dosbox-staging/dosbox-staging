/* Do the actual opcode */
switch (inst.code.op) {
	case t_ADDb:	case t_ADDw:	case t_ADDd:
		flags.var1.d=inst.op1.d;
		flags.var2.d=inst.op2.d;
		inst.op1.d=flags.result.d=flags.var1.d + flags.var2.d;
		flags.type=inst.code.op;
		break;
	case t_CMPb:	case t_CMPw:	case t_CMPd:
	case t_SUBb:	case t_SUBw:	case t_SUBd:
		flags.var1.d=inst.op1.d;
		flags.var2.d=inst.op2.d;
		inst.op1.d=flags.result.d=flags.var1.d - flags.var2.d;
		flags.type=inst.code.op;
		break;
	case t_ORb:		case t_ORw:		case t_ORd:
		flags.var1.d=inst.op1.d;
		flags.var2.d=inst.op2.d;
		inst.op1.d=flags.result.d=flags.var1.d | flags.var2.d;
		flags.type=inst.code.op;
		break;
	case t_XORb:	case t_XORw:	case t_XORd:
		flags.var1.d=inst.op1.d;
		flags.var2.d=inst.op2.d;
		inst.op1.d=flags.result.d=flags.var1.d ^ flags.var2.d;
		flags.type=inst.code.op;
		break;
	case t_TESTb:	case t_TESTw:	case t_TESTd:
	case t_ANDb:	case t_ANDw:	case t_ANDd:
		flags.var1.d=inst.op1.d;
		flags.var2.d=inst.op2.d;
		inst.op1.d=flags.result.d=flags.var1.d & flags.var2.d;
		flags.type=inst.code.op;
		break;
	case t_ADCb:	case t_ADCw:	case t_ADCd:
		flags.oldcf=get_CF();
		flags.var1.d=inst.op1.d;
		flags.var2.d=inst.op2.d;
		inst.op1.d=flags.result.d=flags.var1.d + flags.var2.d + flags.oldcf;
		flags.type=inst.code.op;
		break;
	case t_SBBb:	case t_SBBw:	case t_SBBd:
		flags.oldcf=get_CF();
		flags.var1.d=inst.op1.d;
		flags.var2.d=inst.op2.d;
		inst.op1.d=flags.result.d=flags.var1.d - flags.var2.d - flags.oldcf;
		flags.type=inst.code.op;
		break;
	case t_INCb:	case t_INCw:	case t_INCd:
		SETFLAGBIT(CF,get_CF());
		inst.op1.d=flags.result.d=inst.op1.d+1;
		flags.type=inst.code.op;
		break;
	case t_DECb:	case t_DECw:	case t_DECd:
		SETFLAGBIT(CF,get_CF());
		inst.op1.d=flags.result.d=inst.op1.d-1;
		flags.type=inst.code.op;
		break;
/* Using the instructions.h defines */
	case t_ROLb:
		ROLB(inst.op1.b,inst.op2.b,LoadD,SaveD);
		break;
	case t_ROLw:
		ROLW(inst.op1.w,inst.op2.b,LoadD,SaveD);
		break;
	case t_ROLd:
		ROLD(inst.op1.d,inst.op2.b,LoadD,SaveD);
		break;

	case t_RORb:
		RORB(inst.op1.b,inst.op2.b,LoadD,SaveD);
		break;
	case t_RORw:
		RORW(inst.op1.w,inst.op2.b,LoadD,SaveD);
		break;
	case t_RORd:
		RORD(inst.op1.d,inst.op2.b,LoadD,SaveD);
		break;

	case t_RCLb:
		RCLB(inst.op1.b,inst.op2.b,LoadD,SaveD);
		break;
	case t_RCLw:
		RCLW(inst.op1.w,inst.op2.b,LoadD,SaveD);
		break;
	case t_RCLd:
		RCLD(inst.op1.d,inst.op2.b,LoadD,SaveD);
		break;

	case t_RCRb:
		RCRB(inst.op1.b,inst.op2.b,LoadD,SaveD);
		break;
	case t_RCRw:
		RCRW(inst.op1.w,inst.op2.b,LoadD,SaveD);
		break;
	case t_RCRd:
		RCRD(inst.op1.d,inst.op2.b,LoadD,SaveD);
		break;

	case t_SHLb:
		SHLB(inst.op1.b,inst.op2.b,LoadD,SaveD);
		break;
	case t_SHLw:
		SHLW(inst.op1.w,inst.op2.b,LoadD,SaveD);
		break;
	case t_SHLd:
		SHLD(inst.op1.d,inst.op2.b,LoadD,SaveD);
		break;

	case t_SHRb:
		SHRB(inst.op1.b,inst.op2.b,LoadD,SaveD);
		break;
	case t_SHRw:
		SHRW(inst.op1.w,inst.op2.b,LoadD,SaveD);
		break;
	case t_SHRd:
		SHRD(inst.op1.d,inst.op2.b,LoadD,SaveD);
		break;

	case t_SARb:
		SARB(inst.op1.b,inst.op2.b,LoadD,SaveD);
		break;
	case t_SARw:
		SARW(inst.op1.w,inst.op2.b,LoadD,SaveD);
		break;
	case t_SARd:
		SARD(inst.op1.d,inst.op2.b,LoadD,SaveD);
		break;

	case O_DSHLw:
		{
			DSHLW(inst.op1.w,inst.op2.w,inst.imm.b,,LoadD,SaveD);
			break;
		}
	case O_DSHRw:
		{
			DSHRW(inst.op1.w,inst.op2.w,inst.imm.b,,LoadD,SaveD);
			break;
		}
	case O_DSHLd:
		{
			DSHLD(inst.op1.d,inst.op2.d,inst.imm.b,,LoadD,SaveD);
			break;
		}
	case O_DSHRd:
		{
			DSHRD(inst.op1.d,inst.op2.d,inst.imm.b,,LoadD,SaveD);
			break;
		}

	case t_NEGb:
		flags.var1.b=inst.op1.b;
		inst.op1.b=flags.result.b=0-inst.op1.b;
		flags.type=t_NEGb;
		break;
	case t_NEGw:
		flags.var1.w=inst.op1.w;
		inst.op1.w=flags.result.w=0-inst.op1.w;
		flags.type=t_NEGw;
		break;
	case t_NEGd:
		flags.var1.d=inst.op1.d;
		inst.op1.d=flags.result.d=0-inst.op1.d;
		flags.type=t_NEGd;
		break;
	
	case O_NOT:
		inst.op1.d=~inst.op1.d;
		break;	
		
	/* Special instructions */
	case O_IMULRw:
		DIMULW(inst.op1.ws,inst.op1.ws,inst.op2.ws,LoadD,SaveD);
		break;
	case O_IMULRd:
		DIMULD(inst.op1.ds,inst.op1.ds,inst.op2.ds,LoadD,SaveD);
		break;
	case O_MULb:
		MULB(inst.op1.b,LoadD,0);
		goto nextopcode;
	case O_MULw:
		MULW(inst.op1.w,LoadD,0);
		goto nextopcode;
	case O_MULd:
		MULD(inst.op1.d,LoadD,0);
		goto nextopcode;
	case O_IMULb:
		IMULB(inst.op1.b,LoadD,0);
		goto nextopcode;
	case O_IMULw:
		IMULW(inst.op1.w,LoadD,0);
		goto nextopcode;
	case O_IMULd:
		IMULD(inst.op1.d,LoadD,0);
		goto nextopcode;
	case O_DIVb:
		DIVB(inst.op1.b,LoadD,0);
		goto nextopcode;
	case O_DIVw:
		DIVW(inst.op1.w,LoadD,0);
		goto nextopcode;
	case O_DIVd:
		DIVD(inst.op1.d,LoadD,0);
		goto nextopcode;
	case O_IDIVb:
		IDIVB(inst.op1.b,LoadD,0);
		goto nextopcode;
	case O_IDIVw:
		IDIVW(inst.op1.w,LoadD,0);
		goto nextopcode;
	case O_IDIVd:
		IDIVD(inst.op1.d,LoadD,0);
		goto nextopcode;
	case O_AAM:
		AAM(inst.op1.b);
		goto nextopcode;
	case O_AAD:
		AAD(inst.op1.b);
		goto nextopcode;

	case O_C_O:		inst.cond=get_OF();								break;
	case O_C_NO:	inst.cond=!get_OF();							break;
	case O_C_B:		inst.cond=get_CF();								break;
	case O_C_NB:	inst.cond=!get_CF();							break;
	case O_C_Z:		inst.cond=get_ZF();								break;
	case O_C_NZ:	inst.cond=!get_ZF();							break;
	case O_C_BE:	inst.cond=get_CF() || get_ZF();					break;
	case O_C_NBE:	inst.cond=!get_CF() && !get_ZF();				break;
	case O_C_S:		inst.cond=get_SF();								break;
	case O_C_NS:	inst.cond=!get_SF();							break;
	case O_C_P:		inst.cond=get_PF();								break;
	case O_C_NP:	inst.cond=!get_PF();							break;
	case O_C_L:		inst.cond=get_SF() != get_OF();					break;
	case O_C_NL:	inst.cond=get_SF() == get_OF();					break;
	case O_C_LE:	inst.cond=get_ZF() || (get_SF() != get_OF());	break;
	case O_C_NLE:	inst.cond=(get_SF() == get_OF()) && !get_ZF();	break;

	case O_ALOP:
		reg_al=LoadMb(inst.rm_eaa);
		goto nextopcode;
	case O_AXOP:
		reg_ax=LoadMw(inst.rm_eaa);
		goto nextopcode;
	case O_EAXOP:
		reg_eax=LoadMd(inst.rm_eaa);
		goto nextopcode;
	case O_OPAL:
		SaveMb(inst.rm_eaa,reg_al);
		goto nextopcode;
	case O_OPAX:
		SaveMw(inst.rm_eaa,reg_ax);
		goto nextopcode;
	case O_OPEAX:
		SaveMd(inst.rm_eaa,reg_eax);
		goto nextopcode;
	case O_SEGDS:
		inst.code.extra=ds;
		break;
	case O_SEGES:
		inst.code.extra=es;
		break;
	case O_SEGFS:
		inst.code.extra=fs;
		break;
	case O_SEGGS:
		inst.code.extra=gs;
		break;
	case O_SEGSS:
		inst.code.extra=ss;
		break;
	
	case O_LOOP:
		if (inst.prefix & PREFIX_ADDR) {
			if (--reg_ecx) break;
		} else {
			if (--reg_cx) break;
		}
		goto nextopcode;
	case O_LOOPZ:
		if (inst.prefix & PREFIX_ADDR) {
			if (--reg_ecx && get_ZF()) break;
		} else {
			if (--reg_cx && get_ZF()) break;
		}
		goto nextopcode;
	case O_LOOPNZ:
		if (inst.prefix & PREFIX_ADDR) {
			if (--reg_ecx && !get_ZF()) break;
		} else {
			if (--reg_cx && !get_ZF()) break;
		}
		goto nextopcode;
	case O_JCXZ:
		if (inst.prefix & PREFIX_ADDR) {
			if (reg_ecx) goto nextopcode;
		} else {
			if (reg_cx) goto nextopcode;
		}
		break;
	case O_XCHG_AX:
		{
			Bit16u temp=reg_ax;
			reg_ax=inst.op1.w;
			inst.op1.w=temp;
			break;
		}
	case O_XCHG_EAX:
		{
			Bit32u temp=reg_eax;
			reg_eax=inst.op1.d;
			inst.op1.d=temp;
			break;
		}
	case O_CALLNw:
		SaveIP();
		Push_16(reg_ip);
		break;
	case O_CALLNd:
		SaveIP();
		Push_32(reg_eip);
		break;
	case O_CALLFw:
		SaveIP();
		CPU_CALL(false,inst.op2.d,inst.op1.d);
		LoadIP();
		goto nextopcode;
	case O_CALLFd:
		SaveIP();
		CPU_CALL(true,inst.op2.d,inst.op1.d);
		LoadIP();
		goto nextopcode;
	case O_JMPFw:
		CPU_JMP(false,inst.op2.d,inst.op1.d);
		LoadIP();
		goto nextopcode;
	case O_INT:
		SaveIP();
#if C_DEBUG
		if (inst.entry==0xcc) if (DEBUG_Breakpoint()) return 1;
		else if (DEBUG_IntBreakpoint(inst.op1.b)) return 1;
#endif
		Interrupt(inst.op1.b);
		LoadIP();
		break;
	case O_INb:
		reg_al=IO_Read(inst.op1.d);
		goto nextopcode;
	case O_INw:
		reg_ax=IO_Read(inst.op1.d) | (IO_Read(inst.op1.d+1) << 8);
		goto nextopcode;
	case O_INd:
		reg_eax=IO_Read(inst.op1.d) | (IO_Read(inst.op1.d+1) << 8) | (IO_Read(inst.op1.d+2) << 16) | (IO_Read(inst.op1.d+3) << 24);
		goto nextopcode;
	case O_OUTb:
		IO_Write(inst.op1.d,reg_al);
		goto nextopcode;
	case O_OUTw:
		IO_Write(inst.op1.d+0,(Bit8u)reg_ax);
		IO_Write(inst.op1.d+1,(Bit8u)(reg_ax >> 8));
		goto nextopcode;
	case O_OUTd:
		IO_Write(inst.op1.d+0,(Bit8u)reg_eax);
		IO_Write(inst.op1.d+1,(Bit8u)(reg_eax >> 8));
		IO_Write(inst.op1.d+2,(Bit8u)(reg_eax >> 16));
		IO_Write(inst.op1.d+3,(Bit8u)(reg_eax >> 24));
		goto nextopcode;
	case O_CBACK:
		if (inst.op1.d<CB_MAX) { 
			SaveIP();
			Bitu ret=CallBack_Handlers[inst.op1.d]();
			switch (ret) {
			case CBRET_NONE:
				LoadIP();
				goto nextopcode;
			case CBRET_STOP:
				return ret;
			default:
				E_Exit("CPU:Callback %d returned illegal %d code",inst.op1.d,ret);
			}
		} else {  
			E_Exit("Too high CallBack Number %d called",inst.op1.d);				
		}
	case O_GRP6w:
	case O_GRP6d:
		switch (inst.rm_index) {
		case 0x02:	/* LLDT */
			CPU_LLDT(inst.op1.d);
			goto nextopcode;		/* Else value will saved */
		default:
			LOG(LOG_ERROR|LOG_CPU,"Group 6 Illegal subfunction %X",inst.rm_index);
		}
		break;
	case O_GRP7w:
	case O_GRP7d:
		switch (inst.rm_index) {
		case 0:		/* SGDT */
			{
				Bitu limit,base;
				CPU_SGDT(limit,base);
				SaveMw(inst.rm_eaa,limit);
				SaveMd(inst.rm_eaa+2,base);
				break;
			}
		case 1:		/* SIDT */
			{
				Bitu limit,base;
				CPU_SIDT(limit,base);
				SaveMw(inst.rm_eaa,limit);
				SaveMd(inst.rm_eaa+2,base);
				break;
			}
		case 2:		/* LGDT */
			CPU_LGDT(LoadMw(inst.rm_eaa),LoadMd(inst.rm_eaa+2)&((inst.code.op == O_GRP7w) ? 0xFFFFFF : 0xFFFFFFFF));
			break;
		case 3:		/* LIDT */
			CPU_LIDT(LoadMw(inst.rm_eaa),LoadMd(inst.rm_eaa+2)&((inst.code.op == O_GRP7w) ? 0xFFFFFF : 0xFFFFFFFF));
			break;
		case 4:		/* SMSW */
			{
				Bitu word;CPU_SMSW(word);
				SaveMw(inst.rm_eaa,word);
				break;
			}
		case 6:		/* LMSW */
			{
				Bitu word=LoadMw(inst.rm_eaa);
				CPU_LMSW(word);
				break;
			}
		default:
			LOG(LOG_ERROR|LOG_CPU,"Group 7 Illegal subfunction %X",inst.rm_index);
		}
		break;
	case O_M_Cd_Rd:
		CPU_SET_CRX(inst.rm_index,inst.op1.d);
		break;
	case O_M_Rd_Cd:
		inst.op1.d=CPU_GET_CRX(inst.rm_index);
		break;
	case O_LAR:
		{
			Bitu ar;CPU_LAR(inst.op1.d,ar);
			inst.op2.d=ar;
		}
		break;
	case O_BTd:
	case O_BTSd:
	case O_BTCd:
	case O_BTRd:
		{
			Bitu val;PhysPt read;
			Bitu mask=1 << (inst.op1.d & 31);
			FILLFLAGS;
			if (inst.rm<0xc0) {
				read=inst.rm_eaa+4*(inst.op1.d / 32);
				val=mem_readd(read);
			} else {
				val=reg_32(inst.rm_eai);
			}
			SETFLAGBIT(CF,(val&mask)>0);
			if (inst.code.op==O_BTSd) val|=mask;
			if (inst.code.op==O_BTRd) val&=~mask;
			if (inst.code.op==O_BTCd) val^=mask;
			if (inst.code.op==O_BTd) break;
			if (inst.rm<0xc0) {
				mem_writed(read,val);
			} else {
				reg_32(inst.rm_eai)=val;
			}
		}
		break;
	case 0:
		break;
	default:
		LOG(LOG_ERROR|LOG_CPU,"OP:Unhandled code %d entry %X",inst.code.op,inst.entry);
		
}
