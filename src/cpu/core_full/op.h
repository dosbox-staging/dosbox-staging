/* Do the actual opcode */
switch (inst.code.op) {
	case t_ADDb:	case t_ADDw:	case t_ADDd:
		lf_var1d=inst.op1.d;
		lf_var2d=inst.op2.d;
		inst.op1.d=lf_resd=lf_var1d + lf_var2d;
		lflags.type=inst.code.op;
		break;
	case t_CMPb:	case t_CMPw:	case t_CMPd:
	case t_SUBb:	case t_SUBw:	case t_SUBd:
		lf_var1d=inst.op1.d;
		lf_var2d=inst.op2.d;
		inst.op1.d=lf_resd=lf_var1d - lf_var2d;
		lflags.type=inst.code.op;
		break;
	case t_ORb:		case t_ORw:		case t_ORd:
		lf_var1d=inst.op1.d;
		lf_var2d=inst.op2.d;
		inst.op1.d=lf_resd=lf_var1d | lf_var2d;
		lflags.type=inst.code.op;
		break;
	case t_XORb:	case t_XORw:	case t_XORd:
		lf_var1d=inst.op1.d;
		lf_var2d=inst.op2.d;
		inst.op1.d=lf_resd=lf_var1d ^ lf_var2d;
		lflags.type=inst.code.op;
		break;
	case t_TESTb:	case t_TESTw:	case t_TESTd:
	case t_ANDb:	case t_ANDw:	case t_ANDd:
		lf_var1d=inst.op1.d;
		lf_var2d=inst.op2.d;
		inst.op1.d=lf_resd=lf_var1d & lf_var2d;
		lflags.type=inst.code.op;
		break;
	case t_ADCb:	case t_ADCw:	case t_ADCd:
		lflags.oldcf=(get_CF()!=0);
		lf_var1d=inst.op1.d;
		lf_var2d=inst.op2.d;
		inst.op1.d=lf_resd=lf_var1d + lf_var2d + lflags.oldcf;
		lflags.type=inst.code.op;
		break;
	case t_SBBb:	case t_SBBw:	case t_SBBd:
		lflags.oldcf=(get_CF()!=0);
		lf_var1d=inst.op1.d;
		lf_var2d=inst.op2.d;
		inst.op1.d=lf_resd=lf_var1d - lf_var2d - lflags.oldcf;
		lflags.type=inst.code.op;
		break;
	case t_INCb:	case t_INCw:	case t_INCd:
		LoadCF;
		lf_var1d=inst.op1.d;
		inst.op1.d=lf_resd=inst.op1.d+1;
		lflags.type=inst.code.op;
		break;
	case t_DECb:	case t_DECw:	case t_DECd:
		LoadCF;
		lf_var1d=inst.op1.d;
		inst.op1.d=lf_resd=inst.op1.d-1;
		lflags.type=inst.code.op;
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
			DSHLW(inst.op1.w,inst.op2.w,inst.imm.b,LoadD,SaveD);
			break;
		}
	case O_DSHRw:
		{
			DSHRW(inst.op1.w,inst.op2.w,inst.imm.b,LoadD,SaveD);
			break;
		}
	case O_DSHLd:
		{
			DSHLD(inst.op1.d,inst.op2.d,inst.imm.b,LoadD,SaveD);
			break;
		}
	case O_DSHRd:
		{
			DSHRD(inst.op1.d,inst.op2.d,inst.imm.b,LoadD,SaveD);
			break;
		}

	case t_NEGb:
		lf_var1b=inst.op1.b;
		inst.op1.b=lf_resb=0-inst.op1.b;
		lflags.type=t_NEGb;
		break;
	case t_NEGw:
		lf_var1w=inst.op1.w;
		inst.op1.w=lf_resw=0-inst.op1.w;
		lflags.type=t_NEGw;
		break;
	case t_NEGd:
		lf_var1d=inst.op1.d;
		inst.op1.d=lf_resd=0-inst.op1.d;
		lflags.type=t_NEGd;
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

	case O_C_O:		inst.cond=TFLG_O;	break;
	case O_C_NO:	inst.cond=TFLG_NO;	break;
	case O_C_B:		inst.cond=TFLG_B;	break;
	case O_C_NB:	inst.cond=TFLG_NB;	break;
	case O_C_Z:		inst.cond=TFLG_Z;	break;
	case O_C_NZ:	inst.cond=TFLG_NZ;	break;
	case O_C_BE:	inst.cond=TFLG_BE;	break;
	case O_C_NBE:	inst.cond=TFLG_NBE;	break;
	case O_C_S:		inst.cond=TFLG_S;	break;
	case O_C_NS:	inst.cond=TFLG_NS;	break;
	case O_C_P:		inst.cond=TFLG_P;	break;
	case O_C_NP:	inst.cond=TFLG_NP;	break;
	case O_C_L:		inst.cond=TFLG_L;	break;
	case O_C_NL:	inst.cond=TFLG_NL;	break;
	case O_C_LE:	inst.cond=TFLG_LE;	break;
	case O_C_NLE:	inst.cond=TFLG_NLE;	break;

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
		FillFlags();
		CPU_CALL(false,inst.op2.d,inst.op1.d,GetIP());
		continue;
	case O_CALLFd:
		FillFlags();
		CPU_CALL(true,inst.op2.d,inst.op1.d,GetIP());
		continue;
	case O_JMPFw:
		FillFlags();
		CPU_JMP(false,inst.op2.d,inst.op1.d,GetIP());
		continue;
	case O_JMPFd:
		FillFlags();
		CPU_JMP(true,inst.op2.d,inst.op1.d,GetIP());
		continue;
	case O_INT:
		FillFlags();
#if C_DEBUG
		if (((inst.entry & 0xFF)==0xcc) && DEBUG_Breakpoint()) 
			return debugCallback;
		else if (DEBUG_IntBreakpoint(inst.op1.b)) 
			return debugCallback;
#endif
		CPU_SW_Interrupt(inst.op1.b,GetIP());
		continue;
	case O_INb:
		if (CPU_IO_Exception(inst.op1.d,1)) RunException();
		reg_al=IO_ReadB(inst.op1.d);
		goto nextopcode;
	case O_INw:
		if (CPU_IO_Exception(inst.op1.d,2)) RunException();
		reg_ax=IO_ReadW(inst.op1.d);
		goto nextopcode;
	case O_INd:
		if (CPU_IO_Exception(inst.op1.d,4)) RunException();
		reg_eax=IO_ReadD(inst.op1.d);
		goto nextopcode;
	case O_OUTb:
		if (CPU_IO_Exception(inst.op1.d,1)) RunException();
		IO_WriteB(inst.op1.d,reg_al);
		goto nextopcode;
	case O_OUTw:
		if (CPU_IO_Exception(inst.op1.d,2)) RunException();
		IO_WriteW(inst.op1.d,reg_ax);
		goto nextopcode;
	case O_OUTd:
		if (CPU_IO_Exception(inst.op1.d,4)) RunException();
		IO_WriteD(inst.op1.d,reg_eax);
		goto nextopcode;
	case O_CBACK:
		FillFlags();SaveIP();
		return inst.op1.d;
	case O_GRP6w:
	case O_GRP6d:
		switch (inst.rm_index) {
		case 0x00:	/* SLDT */
			{
				Bitu selector;
				CPU_SLDT(selector);
				inst.op1.d=(Bit32u)selector;
			}
			break;
		case 0x01:	/* STR */
			{
				Bitu selector;
				CPU_STR(selector);
				inst.op1.d=(Bit32u)selector;
			}
			break;
		case 0x02:	/* LLDT */
			CPU_LLDT(inst.op1.d);
			goto nextopcode;		/* Else value will saved */
		case 0x03:	/* LTR */
			CPU_LTR(inst.op1.d);
			goto nextopcode;		/* Else value will saved */
		case 0x04:	/* VERR */
			FillFlags();
			CPU_VERR(inst.op1.d);
			goto nextopcode;		/* Else value will saved */
		case 0x05:	/* VERW */
			FillFlags();
			CPU_VERW(inst.op1.d);
			goto nextopcode;		/* Else value will saved */
		default:
			LOG(LOG_CPU,LOG_ERROR)("Group 6 Illegal subfunction %X",inst.rm_index);
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
				goto nextopcode;
			}
		case 1:		/* SIDT */
			{
				Bitu limit,base;
				CPU_SIDT(limit,base);
				SaveMw(inst.rm_eaa,limit);
				SaveMd(inst.rm_eaa+2,base);
				goto nextopcode;
			}
		case 2:		/* LGDT */
			CPU_LGDT(LoadMw(inst.rm_eaa),LoadMd(inst.rm_eaa+2)&((inst.code.op == O_GRP7w) ? 0xFFFFFF : 0xFFFFFFFF));
			goto nextopcode;
		case 3:		/* LIDT */
			CPU_LIDT(LoadMw(inst.rm_eaa),LoadMd(inst.rm_eaa+2)&((inst.code.op == O_GRP7w) ? 0xFFFFFF : 0xFFFFFFFF));
			goto nextopcode;
		case 4:		/* SMSW */
			{
				Bitu word;CPU_SMSW(word);
				inst.op1.d=word;
				break;
			}
		case 6:		/* LMSW */
			FillFlags();
			if (CPU_LMSW(inst.op1.w)) RunException();
			goto nextopcode;
		default:
			LOG(LOG_CPU,LOG_ERROR)("Group 7 Illegal subfunction %X",inst.rm_index);
		}
		break;
	case O_M_CRx_Rd:
		if (CPU_WRITE_CRX(inst.rm_index,inst.op1.d)) RunException();
		break;
	case O_M_Rd_CRx:
		if (CPU_READ_CRX(inst.rm_index,inst.op1.d)) RunException();
		break;
	case O_M_DRx_Rd:
//		LOG(LOG_CPU,LOG_NORMAL)("MOV DR%d,%X",inst.rm_index,inst.op1.d);
		break;
	case O_M_Rd_DRx:
		inst.op1.d=0;
//		LOG(LOG_CPU,LOG_NORMAL)("MOV %X,DR%d",inst.op1.d,inst.rm_index);
		break;
	case O_LAR:
		{
			FillFlags();
			Bitu ar=inst.op2.d;
			CPU_LAR(inst.op1.w,ar);
			inst.op1.d=(Bit32u)ar;
		}
		break;
	case O_LSL:
		{
			FillFlags();
			Bitu limit=inst.op2.d;
			CPU_LSL(inst.op1.w,limit);
			inst.op1.d=(Bit32u)limit;
		}
		break;
	case O_ARPL:
		{
			if ((reg_flags & FLAG_VM) || !cpu.pmode) goto illegalopcode;
			FillFlags();
			Bitu new_sel=inst.op1.d;
			CPU_ARPL(new_sel,inst.op2.d);
			inst.op1.d=(Bit32u)new_sel;
		}
		break;
	case O_BSFw:
		{
			FillFlags();
			if (!inst.op1.w) {
				SETFLAGBIT(ZF,true);
				goto nextopcode;
			} else {
				Bitu count=0;
				while (1) {
					if (inst.op1.w & 0x1) break;
					count++;inst.op1.w>>=1;
				}
				inst.op1.d=count;
				SETFLAGBIT(ZF,false);
			}
		}
		break;
	case O_BSFd:
		{
			FillFlags();
			if (!inst.op1.d) {
				SETFLAGBIT(ZF,true);
				goto nextopcode;
			} else {
				Bitu count=0;
				while (1) {
					if (inst.op1.d & 0x1) break;
					count++;inst.op1.d>>=1;
				}
				inst.op1.d=count;
				SETFLAGBIT(ZF,false);
			}
		}
		break;
	case O_BSRw:
		{
			FillFlags();
			if (!inst.op1.w) {
				SETFLAGBIT(ZF,true);
				goto nextopcode;
			} else {
				Bitu count=15;
				while (1) {
					if (inst.op1.w & 0x8000) break;
					count--;inst.op1.w<<=1;
				}
				inst.op1.d=count;
				SETFLAGBIT(ZF,false);
			}
		}
		break;
	case O_BSRd:
		{
			FillFlags();
			if (!inst.op1.d) {
				SETFLAGBIT(ZF,true);
				goto nextopcode;
			} else {
				Bitu count=31;
				while (1) {
					if (inst.op1.d & 0x80000000) break;
					count--;inst.op1.d<<=1;
				}
				inst.op1.d=count;
				SETFLAGBIT(ZF,false);
			}
		}
		break;
	case O_BTw:
		FillFlags();
		SETFLAGBIT(CF,(inst.op1.d & (1 << (inst.op2.d & 15))));
		break;
	case O_BTSw:
		FillFlags();
		SETFLAGBIT(CF,(inst.op1.d & (1 << (inst.op2.d & 15))));
		inst.op1.d|=(1 << (inst.op2.d & 15));
		break;
	case O_BTCw:
		FillFlags();
		SETFLAGBIT(CF,(inst.op1.d & (1 << (inst.op2.d & 15))));
		inst.op1.d^=(1 << (inst.op2.d & 15));
		break;
	case O_BTRw:
		FillFlags();
		SETFLAGBIT(CF,(inst.op1.d & (1 << (inst.op2.d & 15))));
		inst.op1.d&=~(1 << (inst.op2.d & 15));
		break;
	case O_BTd:
		FillFlags();
		SETFLAGBIT(CF,(inst.op1.d & (1 << (inst.op2.d & 31))));
		break;
	case O_BTSd:
		FillFlags();
		SETFLAGBIT(CF,(inst.op1.d & (1 << (inst.op2.d & 31))));
		inst.op1.d|=(1 << (inst.op2.d & 31));
		break;
	case O_BTCd:
		FillFlags();
		SETFLAGBIT(CF,(inst.op1.d & (1 << (inst.op2.d & 31))));
		inst.op1.d^=(1 << (inst.op2.d & 31));
		break;
	case O_BTRd:
		FillFlags();
		SETFLAGBIT(CF,(inst.op1.d & (1 << (inst.op2.d & 31))));
		inst.op1.d&=~(1 << (inst.op2.d & 31));
		break;
	case O_BSWAP:
		BSWAP(inst.op1.d);
		break;
	case O_FPU:
#if C_FPU
		switch (((inst.rm>=0xc0) << 3) | inst.code.save) {
		case 0x00:	FPU_ESC0_EA(inst.rm,inst.rm_eaa);break;
		case 0x01:	FPU_ESC1_EA(inst.rm,inst.rm_eaa);break;
		case 0x02:	FPU_ESC2_EA(inst.rm,inst.rm_eaa);break;
		case 0x03:	FPU_ESC3_EA(inst.rm,inst.rm_eaa);break;
		case 0x04:	FPU_ESC4_EA(inst.rm,inst.rm_eaa);break;
		case 0x05:	FPU_ESC5_EA(inst.rm,inst.rm_eaa);break;
		case 0x06:	FPU_ESC6_EA(inst.rm,inst.rm_eaa);break;
		case 0x07:	FPU_ESC7_EA(inst.rm,inst.rm_eaa);break;

		case 0x08:	FPU_ESC0_Normal(inst.rm);break;
		case 0x09:	FPU_ESC1_Normal(inst.rm);break;
		case 0x0a:	FPU_ESC2_Normal(inst.rm);break;
		case 0x0b:	FPU_ESC3_Normal(inst.rm);break;
		case 0x0c:	FPU_ESC4_Normal(inst.rm);break;
		case 0x0d:	FPU_ESC5_Normal(inst.rm);break;
		case 0x0e:	FPU_ESC6_Normal(inst.rm);break;
		case 0x0f:	FPU_ESC7_Normal(inst.rm);break;
		}
		goto nextopcode;
#else
		LOG(LOG_CPU,LOG_ERROR)("Unhandled FPU ESCAPE %d",inst.code.save);
		goto nextopcode;
#endif
	case O_BOUNDw:
		{
			Bit16s bound_min, bound_max;
			bound_min=LoadMw(inst.rm_eaa);
			bound_max=LoadMw(inst.rm_eaa+2);
			if ( (((Bit16s)inst.op1.w) < bound_min) || (((Bit16s)inst.op1.w) > bound_max) ) {
				EXCEPTION(5);
			}
		}
		break;
	case 0:
		break;
	default:
		LOG(LOG_CPU,LOG_ERROR)("OP:Unhandled code %d entry %X",inst.code.op,inst.entry);
		
}
