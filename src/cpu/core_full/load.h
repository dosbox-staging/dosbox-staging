switch (inst.code.load) {
/* General loading */
	case L_MODRM:
		DecodeModRM();
l_MODRMswitch:
		switch (inst.code.extra) {	
/* Byte */
		case M_Ib:
			inst.op1.d=Fetchb();
			break;
		case M_Ebx:
			if (inst.rm<0xc0) inst.op1.ds=(Bit8s)LoadMb(inst.rm_eaa);
			else inst.op1.ds=(Bit8s)reg_8(inst.rm_eai);
			break;
		case M_EbIb:
			inst.op2.d=Fetchb();
		case M_Eb:
			if (inst.rm<0xc0) inst.op1.d=LoadMb(inst.rm_eaa);
			else inst.op1.d=reg_8(inst.rm_eai);
			break;
		case M_EbGb:
			if (inst.rm<0xc0) inst.op1.d=LoadMb(inst.rm_eaa);
			else inst.op1.d=reg_8(inst.rm_eai);
			inst.op2.d=reg_8(inst.rm_index);
			break;
		case M_GbEb:
			if (inst.rm<0xc0) inst.op2.d=LoadMb(inst.rm_eaa);
			else inst.op2.d=reg_8(inst.rm_eai);
		case M_Gb:
			inst.op1.d=reg_8(inst.rm_index);;
			break;
/* Word */
		case M_Iw:
			inst.op1.d=Fetchw();
			break;
		case M_EwxGwx:
			inst.op2.ds=(Bit16s)reg_16(inst.rm_index);
			goto l_M_Ewx;
		case M_EwxIbx:
			inst.op2.ds=Fetchbs();
			goto l_M_Ewx;
		case M_EwxIwx:
			inst.op2.ds=Fetchws();
l_M_Ewx:		
		case M_Ewx:
			if (inst.rm<0xc0) inst.op1.ds=(Bit16s)LoadMw(inst.rm_eaa);
			else inst.op1.ds=(Bit16s)reg_16(inst.rm_eai);
			break;
		case M_EwIbx:
			inst.op2.ds=Fetchbs();
			goto l_M_Ew;			
		case M_EwIw:
			inst.op2.d=Fetchw();
			goto l_M_Ew;
		case M_EwGwCL:
			inst.imm.d=reg_cl;
			goto l_M_EwGw;
		case M_EwGwIb:
			inst.imm.d=Fetchb();
l_M_EwGw:			
		case M_EwGw:
			inst.op2.d=reg_16(inst.rm_index);
l_M_Ew:
		case M_Ew:
			if (inst.rm<0xc0) inst.op1.d=LoadMw(inst.rm_eaa);
			else inst.op1.d=reg_16(inst.rm_eai);
			break;
		case M_GwEw:
			if (inst.rm<0xc0) inst.op2.d=LoadMw(inst.rm_eaa);
			else inst.op2.d=reg_16(inst.rm_eai);
		case M_Gw:
			inst.op1.d=reg_16(inst.rm_index);;
			break;
/* DWord */
		case M_Id:
			inst.op1.d=Fetchd();
			break;
		case M_EdxGdx:
			inst.op2.ds=(Bit32s)reg_32(inst.rm_index);
		case M_Edx:
			if (inst.rm<0xc0) inst.op1.d=(Bit32s)LoadMd(inst.rm_eaa);
			else inst.op1.d=(Bit32s)reg_32(inst.rm_eai);
			break;
		case M_EdIbx:
			inst.op2.ds=Fetchbs();
			goto l_M_Ed;
		case M_EdId:
			inst.op2.d=Fetchd();
			goto l_M_Ed;			
		case M_EdGdCL:
			inst.imm.d=reg_cl;
			goto l_M_EdGd;
		case M_EdGdIb:
			inst.imm.d=Fetchb();
l_M_EdGd:
		case M_EdGd:
			inst.op2.d=reg_32(inst.rm_index);
l_M_Ed:
		case M_Ed:
			if (inst.rm<0xc0) inst.op1.d=LoadMd(inst.rm_eaa);
			else inst.op1.d=reg_32(inst.rm_eai);
			break;
		case M_GdEd:
			if (inst.rm<0xc0) inst.op2.d=LoadMd(inst.rm_eaa);
			else inst.op2.d=reg_32(inst.rm_eai);
		case M_Gd:
			inst.op1.d=reg_32(inst.rm_index);
			break;
/* Others */		

		case M_SEG:
			//TODO Check for limit
			inst.op1.d=SegValue((SegNames)inst.rm_index);
			break;
		case M_Efw:
			if (inst.rm>=0xC0) {
				LOG(LOG_CPU|LOG_ERROR,"MODRM:Illegal M_Efw ");
				goto nextopcode;
			}
			inst.op1.d=LoadMw(inst.rm_eaa);
			inst.op2.d=LoadMw(inst.rm_eaa+2);
			break;
		case M_Efd:
			if (inst.rm>=0xc0) {
				LOG(LOG_CPU|LOG_ERROR,"MODRM:Illegal M_Efw ");
				goto nextopcode;
			}
			inst.op1.d=LoadMd(inst.rm_eaa);
			inst.op2.d=LoadMw(inst.rm_eaa+4);
			break;
		case M_EA:
			inst.op1.d=inst.rm_off;
			break;
		case M_POPw:
			inst.op1.d = Pop_16();
			break;
		case M_POPd:
			inst.op1.d = Pop_32();
			break;
		case M_GRP:
			inst.code=Groups[inst.code.op][inst.rm_index];
			goto l_MODRMswitch;
		case M_GRP_Ib:
			inst.op2.d=Fetchb();
			inst.code=Groups[inst.code.op][inst.rm_index];
			goto l_MODRMswitch;
		case M_GRP_CL:
			inst.op2.d=reg_cl;
			inst.code=Groups[inst.code.op][inst.rm_index];
			goto l_MODRMswitch;
		case M_GRP_1:
			inst.op2.d=1;
			inst.code=Groups[inst.code.op][inst.rm_index];
			goto l_MODRMswitch;
			
			/* Should continue with normal handler afterwards */
		case 0:
			break;
		default:
			LOG(LOG_CPU|LOG_ERROR,"MODRM:Unhandled load %d entry %x",inst.code.extra,inst.entry);
			break;
		}
		break;
	case L_POPw:
		inst.op1.d = Pop_16();
		break;
	case L_POPd:
		inst.op1.d = Pop_32();
		break;
	case L_POPfw:
		inst.op1.d = Pop_16();
		inst.op2.d = Pop_16();
		break;
	case L_POPfd:
		inst.op1.d = Pop_32();
		inst.op2.d = Pop_16();
		break;
	case L_Ib:
		inst.op1.d=Fetchb();
		break;
	case L_Ibx:
		inst.op1.ds=Fetchbs();
		break;
	case L_Iw:
		inst.op1.d=Fetchw();
		break;
	case L_Iwx:
		inst.op1.ds=Fetchws();
		break;
	case L_Idx:
	case L_Id:
		inst.op1.d=Fetchd();
		break;
	case L_Ifw:
		inst.op1.d=Fetchw();
		inst.op2.d=Fetchw();
		break;

/* Direct load of registers */
	case L_REGbIb:
		inst.op2.d=Fetchb();
	case L_REGb:
		inst.op1.d=reg_8(inst.code.extra);
		break;
	case L_REGwIw:
		inst.op2.d=Fetchw();
	case L_REGw:
		inst.op1.d=reg_16(inst.code.extra);
		break;
	case L_REGdId:
		inst.op2.d=Fetchd();
	case L_REGd:
		inst.op1.d=reg_32(inst.code.extra);
		break;
	case L_FLG:
		FILLFLAGS;
		inst.op1.d = flags.word;
		break;
	case L_SEG:
		inst.op1.d=SegValue((SegNames)inst.code.extra);
		break;
/* Depending on addressize */
	case L_OP:
		if (inst.prefix & PREFIX_ADDR) {
			inst.rm_eaa=Fetchd();
		} else {
			inst.rm_eaa=Fetchw();
		}
		if (inst.prefix & PREFIX_SEG) {
			inst.rm_eaa+=inst.seg.base;
		} else {
			inst.rm_eaa+=SegBase(ds);
		}
		break;
	/* Special cases */
	case L_DOUBLE:
		inst.entry|=0x100;
		goto restartopcode;
	case L_PRESEG:
		inst.prefix|=PREFIX_SEG;
		inst.seg.base=SegBase((SegNames)inst.code.extra);
		goto restartopcode;
	case L_PREREPNE:
		inst.prefix|=PREFIX_REP;
		inst.repz=false;
		goto restartopcode;
	case L_PREREP:
		inst.prefix|=PREFIX_REP;
		inst.repz=true;
		goto restartopcode;
	case L_PREOP:
		inst.entry^=0x200;
		goto restartopcode;
	case L_PREADD:
		inst.prefix^=PREFIX_ADDR;
		goto restartopcode;
	case L_VAL:
		inst.op1.d=inst.code.extra;
		break;
	case L_INTO:
		if (!get_OF()) goto nextopcode;
		inst.op1.d=4;
		break;
	case D_IRETw:
		CPU_IRET(false);
		LoadIP();
		goto nextopcode;
	case D_IRETd:
		CPU_IRET(true);
		LoadIP();
		goto nextopcode;
	case D_RETFwIw:
		CPU_RET(false,Fetchw());
		LoadIP();
		goto nextopcode;
	case D_RETFw:
		CPU_RET(false,0);
		LoadIP();
		goto nextopcode;
	case D_RETFdIw:
		CPU_RET(true,Fetchw());
		LoadIP();
		goto nextopcode;
	case D_RETFd:
		CPU_RET(true,0);
		LoadIP();
		goto nextopcode;
/* Direct operations */
	case L_STRING:
		#include "string.h"
		goto nextopcode;
	case D_PUSHAw:
		{
			Bit16u old_sp=reg_sp;
			Push_16(reg_ax);Push_16(reg_cx);Push_16(reg_dx);Push_16(reg_bx);
			Push_16(old_sp);Push_16(reg_bp);Push_16(reg_si);Push_16(reg_di);
		}
		goto nextopcode;
	case D_PUSHAd:
		{
			Bit32u old_esp=reg_esp;
			Push_32(reg_eax);Push_32(reg_ecx);Push_32(reg_edx);Push_32(reg_ebx);
			Push_32(old_esp);Push_32(reg_ebp);Push_32(reg_esi);Push_32(reg_edi);
		}
		goto nextopcode;
	case D_POPAw:
		reg_di=Pop_16();reg_si=Pop_16();reg_bp=Pop_16();Pop_16();//Don't save SP
		reg_bx=Pop_16();reg_dx=Pop_16();reg_cx=Pop_16();reg_ax=Pop_16();
		goto nextopcode;
	case D_POPAd:
		reg_edi=Pop_32();reg_esi=Pop_32();reg_ebp=Pop_32();Pop_32();//Don't save ESP
		reg_ebx=Pop_32();reg_edx=Pop_32();reg_ecx=Pop_32();reg_eax=Pop_32();
		goto nextopcode;
	case D_SETALC:
		reg_al = get_CF() ? 0xFF : 0;
		goto nextopcode;
	case D_XLAT:
		if (inst.prefix & PREFIX_SEG) reg_al=LoadMb(inst.seg.base+reg_bx+reg_al);
		else reg_al=LoadMb(SegBase(ds)+reg_bx+reg_al);
		goto nextopcode;
	case D_CBW:
		reg_ax=(Bit8s)reg_al;
		goto nextopcode;
	case D_CWDE:
		reg_eax=(Bit16s)reg_ax;
		goto nextopcode;
	case D_CWD:
		if (reg_ax & 0x8000) reg_dx=0xffff;
		else reg_dx=0;
		goto nextopcode;
	case D_CDQ:
		if (reg_eax & 0x80000000) reg_edx=0xffffffff;
		else reg_edx=0;
		goto nextopcode;
	case D_CLI:
		SETFLAGBIT(IF,false);
		goto nextopcode;
	case D_STI:
		SETFLAGBIT(IF,true);
		if (PIC_IRQCheck) {
			SaveIP();
			PIC_runIRQs();	
			LoadIP();
		}
		goto nextopcode;
	case D_STC:
		SETFLAGBIT(CF,true);
		if (flags.type!=t_CF) flags.prev_type=flags.type;
		flags.type=t_CF;
		goto nextopcode;
	case D_CLC:
		SETFLAGBIT(CF,false);
		if (flags.type!=t_CF) flags.prev_type=flags.type;
		flags.type=t_CF;
		goto nextopcode;
	case D_CMC:
		SETFLAGBIT(CF,!get_CF());
		if (flags.type!=t_CF) flags.prev_type=flags.type;
		flags.type=t_CF;
		goto nextopcode;
	case D_CLD:
		SETFLAGBIT(DF,false);
		goto nextopcode;
	case D_STD:
		SETFLAGBIT(DF,true);
		goto nextopcode;
	case D_NOP:
		goto nextopcode;
	case D_ENTERw:
		{
			Bit16u bytes=Fetchw();Bit8u level=Fetchb();
			Push_16(reg_bp);reg_bp=reg_sp;reg_sp-=bytes;
			EAPoint reader=SegBase(ss)+reg_bp;
			for (Bit8u i=1;i<level;i++) {Push_16(LoadMw(reader));reader-=2;}
			if (level) Push_16(reg_bp);
			goto nextopcode;
		}
	case D_LEAVEw:
		reg_sp=reg_bp;
		reg_bp=Pop_16();
		goto nextopcode;
	case D_DAA:
		DAA();
		goto nextopcode;
	case D_DAS:
		DAS();
		goto nextopcode;
	case D_AAA:
		AAA();
		goto nextopcode;
	case D_AAS:
		AAS();
		goto nextopcode;
	case D_CPUID:
		CPU_CPUID();
		goto nextopcode;
	default:
		LOG(LOG_CPU|LOG_ERROR,"LOAD:Unhandled code %d opcode %X",inst.code.load,inst.entry);
		break;
}

