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
		flags.cf=get_CF();
		inst.op1.d=flags.result.d=inst.op1.d+1;
		flags.type=inst.code.op;
		break;
	case t_DECb:	case t_DECw:	case t_DECd:
		flags.cf=get_CF();
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
		inst.op1.ds=inst.op1.ds*inst.op2.ds;		
		flags.type=t_MUL;
		if ((inst.op1.ds> -32768)  && (inst.op1.ds<32767)) {
			flags.cf=false;flags.of=false;
		} else {
			flags.cf=true;flags.of=true;
		}
		break;
	case O_MULb:
		flags.type=t_MUL;
		reg_ax=reg_al*inst.op1.b;
		flags.cf=flags.of=((reg_ax & 0xff00) !=0);
		goto nextopcode;
	case O_MULw:
		{
			Bit32u tempu=(Bit32u)reg_ax*(Bit32u)inst.op1.w;
			reg_ax=(Bit16u)(tempu);
			reg_dx=(Bit16u)(tempu >> 16);
			flags.type=t_MUL;
			flags.cf=flags.of=(reg_dx !=0);
			goto nextopcode;
		}	
	case O_MULd:
		{
			Bit64u tempu=(Bit64u)reg_eax*(Bit64u)inst.op1.d;
			reg_eax=(Bit32u)(tempu);
			reg_edx=(Bit32u)(tempu >> 32);
			flags.type=t_MUL;
			flags.cf=flags.of=(reg_edx !=0);
			goto nextopcode;
		}		
	case O_IMULb:
		flags.type=t_MUL;
		reg_ax=((Bit8s)reg_al)*inst.op1.bs;
		flags.cf=flags.of=!((reg_ax & 0xff80)==0xff80 || (reg_ax & 0xff80)==0x0000);
		goto nextopcode;
	case O_IMULw:
		{
			Bit32s temps=(Bit16s)reg_ax*inst.op1.ws;
			reg_ax=(Bit16s)(temps);
			reg_dx=(Bit16s)(temps >> 16);
			flags.type=t_MUL;
			flags.cf=flags.of=!((temps & 0xffffff80)==0xffffff80 || (temps & 0xffffff80)==0x0000);
			goto nextopcode;
		}	
	case O_IMULd:
		{
			Bit64s temps=(Bit64s)reg_eax*(Bit64s)inst.op1.ds;
			reg_eax=(Bit32s)(temps);
			reg_edx=(Bit32s)(temps >> 32);
			flags.type=t_MUL;
			if ( (reg_edx==0xffffffff) && (reg_eax & 0x80000000) ) {
				flags.cf=flags.of=false;
			} else if ( (reg_edx==0x00000000) && (reg_eax<0x80000000) ) {
				flags.cf=flags.of=false;
			} else {
				flags.cf=flags.of=true;
			}
			goto nextopcode;
		}	
	case O_DIVb:
		{
			if (!inst.op1.b) goto doint;
			Bitu val=reg_ax;Bitu quo=val/inst.op1.b;
			reg_ah=(Bit8u)(val % inst.op1.b);
			reg_al=(Bit8u)quo;
			if (quo!=reg_al) { inst.op1.b=0;goto doint;}
			goto nextopcode;
		}
	case O_DIVw:
		{
			if (!inst.op1.w) goto doint;
			Bitu val=(reg_dx<<16)|reg_ax;Bitu quo=val/inst.op1.w;
			reg_dx=(Bit16u)(val % inst.op1.w);
			reg_ax=(Bit16u)quo;
			if (quo!=reg_ax) { inst.op1.b=0;goto doint;}
			goto nextopcode;
		}
	case O_IDIVb:
		{
			if (!inst.op1.b) goto doint;
			Bits val=(Bit16s)reg_ax;Bits quo=val/inst.op1.bs;
			reg_ah=(Bit8s)(val % inst.op1.bs);
			reg_al=(Bit8s)quo;
			if (quo!=(Bit8s)reg_al) { inst.op1.b=0;goto doint;}
			goto nextopcode;
		}
	case O_IDIVw:
		{
			if (!inst.op1.w) goto doint;
			Bits val=(Bit32s)((reg_dx<<16)|reg_ax);Bits quo=val/inst.op1.ws;
			reg_dx=(Bit16u)(val % inst.op1.ws);
			reg_ax=(Bit16s)quo;
			if (quo!=(Bit16s)reg_ax) { inst.op1.b=0;goto doint;}
			goto nextopcode;
		}			
	case O_AAM:
		reg_ah=reg_al / inst.op1.b;
		reg_al=reg_al % inst.op1.b;
		flags.type=t_UNKNOWN;
		flags.sf=(reg_ah & 0x80) > 0;
		flags.zf=(reg_ax == 0);
		//TODO PF
		flags.pf=0;
		goto nextopcode;
	case O_AAD:
		reg_al=reg_ah*inst.op1.b+reg_al;
		reg_ah=0;
		flags.cf=(reg_al>=0x80);
		flags.zf=(reg_al==0);
		//TODO PF
		flags.type=t_UNKNOWN;
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

		
	case O_LOOP:
		if (--reg_cx) break;
		goto nextopcode;
	case O_LOOPZ:
		if (--reg_cx && get_ZF()) break;
		goto nextopcode;
	case O_LOOPNZ:
		if (--reg_cx && !get_ZF()) break;
		goto nextopcode;
	case O_JCXZ:
		if (reg_cx) goto nextopcode;
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
	case O_CALL_N:
		SaveIP();
		Push_16(reg_ip);
		break;
	case O_CALL_F:
		Push_16(SegValue(cs));
		SaveIP();
		Push_16(reg_ip);
		break;
doint:
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
	case 0:
		break;
	default:
		LOG(LOG_ERROR|LOG_CPU,"OP:Unhandled code %d entry %X",inst.code.op,inst.entry);
		
}
