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

	CASE_D(0x01)												/* ADD Ed,Gd */
		RMEdGd(ADDD);break;	
	CASE_D(0x03)												/* ADD Gd,Ed */
		RMGdEd(ADDD);break;
	CASE_D(0x05)												/* ADD EAX,Id */
		EAXId(ADDD);break;
	CASE_D(0x06)												/* PUSH ES */		
		Push_32(SegValue(es));break;
	CASE_D(0x07)												/* POP ES */
		POPSEG(es,Pop_32(),4);break;
	CASE_D(0x09)												/* OR Ed,Gd */
		RMEdGd(ORD);break;
	CASE_D(0x0b)												/* OR Gd,Ed */
		RMGdEd(ORD);break;
	CASE_D(0x0d)												/* OR EAX,Id */
		EAXId(ORD);break;
	CASE_D(0x0e)												/* PUSH CS */		
		Push_32(SegValue(cs));break;
	CASE_D(0x11)												/* ADC Ed,Gd */
		RMEdGd(ADCD);break;	
	CASE_D(0x13)												/* ADC Gd,Ed */
		RMGdEd(ADCD);break;
	CASE_D(0x15)												/* ADC EAX,Id */
		EAXId(ADCD);break;
	CASE_D(0x16)												/* PUSH SS */
		Push_32(SegValue(ss));break;
	CASE_D(0x17)												/* POP SS */
		POPSEG(ss,Pop_32(),4);
		CPU_Cycles++;
		break;
	CASE_D(0x19)												/* SBB Ed,Gd */
		RMEdGd(SBBD);break;
	CASE_D(0x1b)												/* SBB Gd,Ed */
		RMGdEd(SBBD);break;
	CASE_D(0x1d)												/* SBB EAX,Id */
		EAXId(SBBD);break;
	CASE_D(0x1e)												/* PUSH DS */		
		Push_32(SegValue(ds));break;
	CASE_D(0x1f)												/* POP DS */
		POPSEG(ds,Pop_32(),4);break;
	CASE_D(0x21)												/* AND Ed,Gd */
		RMEdGd(ANDD);break;	
	CASE_D(0x23)												/* AND Gd,Ed */
		RMGdEd(ANDD);break;
	CASE_D(0x25)												/* AND EAX,Id */
		EAXId(ANDD);break;
	CASE_D(0x29)												/* SUB Ed,Gd */
		RMEdGd(SUBD);break;
	CASE_D(0x2b)												/* SUB Gd,Ed */
		RMGdEd(SUBD);break;
	CASE_D(0x2d)												/* SUB EAX,Id */
		EAXId(SUBD);break;
	CASE_D(0x31)												/* XOR Ed,Gd */
		RMEdGd(XORD);break;	
	CASE_D(0x33)												/* XOR Gd,Ed */
		RMGdEd(XORD);break;
	CASE_D(0x35)												/* XOR EAX,Id */
		EAXId(XORD);break;
	CASE_D(0x39)												/* CMP Ed,Gd */
		RMEdGd(CMPD);break;
	CASE_D(0x3b)												/* CMP Gd,Ed */
		RMGdEd(CMPD);break;
	CASE_D(0x3d)												/* CMP EAX,Id */
		EAXId(CMPD);break;
	CASE_D(0x40)												/* INC EAX */
		INCD(reg_eax,LoadRd,SaveRd);break;
	CASE_D(0x41)												/* INC ECX */
		INCD(reg_ecx,LoadRd,SaveRd);break;
	CASE_D(0x42)												/* INC EDX */
		INCD(reg_edx,LoadRd,SaveRd);break;
	CASE_D(0x43)												/* INC EBX */
		INCD(reg_ebx,LoadRd,SaveRd);break;
	CASE_D(0x44)												/* INC ESP */
		INCD(reg_esp,LoadRd,SaveRd);break;
	CASE_D(0x45)												/* INC EBP */
		INCD(reg_ebp,LoadRd,SaveRd);break;
	CASE_D(0x46)												/* INC ESI */
		INCD(reg_esi,LoadRd,SaveRd);break;
	CASE_D(0x47)												/* INC EDI */
		INCD(reg_edi,LoadRd,SaveRd);break;
	CASE_D(0x48)												/* DEC EAX */
		DECD(reg_eax,LoadRd,SaveRd);break;
	CASE_D(0x49)												/* DEC ECX */
		DECD(reg_ecx,LoadRd,SaveRd);break;
	CASE_D(0x4a)												/* DEC EDX */
		DECD(reg_edx,LoadRd,SaveRd);break;
	CASE_D(0x4b)												/* DEC EBX */
		DECD(reg_ebx,LoadRd,SaveRd);break;
	CASE_D(0x4c)												/* DEC ESP */
		DECD(reg_esp,LoadRd,SaveRd);break;
	CASE_D(0x4d)												/* DEC EBP */
		DECD(reg_ebp,LoadRd,SaveRd);break;
	CASE_D(0x4e)												/* DEC ESI */
		DECD(reg_esi,LoadRd,SaveRd);break;
	CASE_D(0x4f)												/* DEC EDI */
		DECD(reg_edi,LoadRd,SaveRd);break;
	CASE_D(0x50)												/* PUSH EAX */
		Push_32(reg_eax);break;
	CASE_D(0x51)												/* PUSH ECX */
		Push_32(reg_ecx);break;
	CASE_D(0x52)												/* PUSH EDX */
		Push_32(reg_edx);break;
	CASE_D(0x53)												/* PUSH EBX */
		Push_32(reg_ebx);break;
	CASE_D(0x54)												/* PUSH ESP */
		Push_32(reg_esp);break;
	CASE_D(0x55)												/* PUSH EBP */
		Push_32(reg_ebp);break;
	CASE_D(0x56)												/* PUSH ESI */
		Push_32(reg_esi);break;
	CASE_D(0x57)												/* PUSH EDI */
		Push_32(reg_edi);break;
	CASE_D(0x58)												/* POP EAX */
		reg_eax=Pop_32();break;
	CASE_D(0x59)												/* POP ECX */
		reg_ecx=Pop_32();break;
	CASE_D(0x5a)												/* POP EDX */
		reg_edx=Pop_32();break;
	CASE_D(0x5b)												/* POP EBX */
		reg_ebx=Pop_32();break;
	CASE_D(0x5c)												/* POP ESP */
		reg_esp=Pop_32();break;
	CASE_D(0x5d)												/* POP EBP */
		reg_ebp=Pop_32();break;
	CASE_D(0x5e)												/* POP ESI */
		reg_esi=Pop_32();break;
	CASE_D(0x5f)												/* POP EDI */
		reg_edi=Pop_32();break;
	CASE_D(0x60)												/* PUSHAD */
	{
		Bitu tmpesp = reg_esp;
		Push_32(reg_eax);Push_32(reg_ecx);Push_32(reg_edx);Push_32(reg_ebx);
		Push_32(tmpesp);Push_32(reg_ebp);Push_32(reg_esi);Push_32(reg_edi);
	}; break;
	CASE_D(0x61)												/* POPAD */
		reg_edi=Pop_32();reg_esi=Pop_32();reg_ebp=Pop_32();Pop_32();//Don't save ESP
		reg_ebx=Pop_32();reg_edx=Pop_32();reg_ecx=Pop_32();reg_eax=Pop_32();
		break;
	CASE_D(0x62)												/* BOUND Ed */
		{
			Bit32s bound_min, bound_max;
			GetRMrd;GetEAa;
			bound_min=LoadMd(eaa);
			bound_max=LoadMd(eaa+4);
			if ( (((Bit32s)*rmrd) < bound_min) || (((Bit32s)*rmrd) > bound_max) ) {
				EXCEPTION(5);
			}
		}
		break;
	CASE_D(0x63)												/* ARPL Ed,Rd */
		{
			FillFlags();
			GetRMrw;
			if (rm >= 0xc0 ) {
				GetEArd;Bitu new_sel=(Bit16u)*eard;
				CPU_ARPL(new_sel,*rmrw);
				*eard=(Bit32u)new_sel;
			} else {
				GetEAa;Bitu new_sel=LoadMw(eaa);
				CPU_ARPL(new_sel,*rmrw);
				SaveMd(eaa,(Bit32u)new_sel);
			}
		}
		break;
	CASE_D(0x68)												/* PUSH Id */
		Push_32(Fetchd());break;
	CASE_D(0x69)												/* IMUL Gd,Ed,Id */
		RMGdEdOp3(DIMULD,Fetchds());
		break;
	CASE_D(0x6a)												/* PUSH Ib */
		Push_32(Fetchbs());break;
	CASE_D(0x6b)												/* IMUL Gd,Ed,Ib */
		RMGdEdOp3(DIMULD,Fetchbs());
		break;
	CASE_D(0x81)												/* Grpl Ed,Id */
		{
			GetRM;Bitu which=(rm>>3)&7;
			if (rm >= 0xc0) {
				GetEArd;Bit32u id=Fetchd();
				switch (which) {
				case 0x00:ADDD(*eard,id,LoadRd,SaveRd);break;
				case 0x01: ORD(*eard,id,LoadRd,SaveRd);break;
				case 0x02:ADCD(*eard,id,LoadRd,SaveRd);break;
				case 0x03:SBBD(*eard,id,LoadRd,SaveRd);break;
				case 0x04:ANDD(*eard,id,LoadRd,SaveRd);break;
				case 0x05:SUBD(*eard,id,LoadRd,SaveRd);break;
				case 0x06:XORD(*eard,id,LoadRd,SaveRd);break;
				case 0x07:CMPD(*eard,id,LoadRd,SaveRd);break;
				}
			} else {
				GetEAa;Bit32u id=Fetchd();
				switch (which) {
				case 0x00:ADDD(eaa,id,LoadMd,SaveMd);break;
				case 0x01: ORD(eaa,id,LoadMd,SaveMd);break;
				case 0x02:ADCD(eaa,id,LoadMd,SaveMd);break;
				case 0x03:SBBD(eaa,id,LoadMd,SaveMd);break;
				case 0x04:ANDD(eaa,id,LoadMd,SaveMd);break;
				case 0x05:SUBD(eaa,id,LoadMd,SaveMd);break;
				case 0x06:XORD(eaa,id,LoadMd,SaveMd);break;
				case 0x07:CMPD(eaa,id,LoadMd,SaveMd);break;
				}
			}
		}
		break;
	CASE_D(0x83)												/* Grpl Ed,Ix */
		{
			GetRM;Bitu which=(rm>>3)&7;
			if (rm >= 0xc0) {
				GetEArd;Bit32u id=(Bit32s)Fetchbs();
				switch (which) {
				case 0x00:ADDD(*eard,id,LoadRd,SaveRd);break;
				case 0x01: ORD(*eard,id,LoadRd,SaveRd);break;
				case 0x02:ADCD(*eard,id,LoadRd,SaveRd);break;
				case 0x03:SBBD(*eard,id,LoadRd,SaveRd);break;
				case 0x04:ANDD(*eard,id,LoadRd,SaveRd);break;
				case 0x05:SUBD(*eard,id,LoadRd,SaveRd);break;
				case 0x06:XORD(*eard,id,LoadRd,SaveRd);break;
				case 0x07:CMPD(*eard,id,LoadRd,SaveRd);break;
				}
			} else {
				GetEAa;Bit32u id=(Bit32s)Fetchbs();
				switch (which) {
				case 0x00:ADDD(eaa,id,LoadMd,SaveMd);break;
				case 0x01: ORD(eaa,id,LoadMd,SaveMd);break;
				case 0x02:ADCD(eaa,id,LoadMd,SaveMd);break;
				case 0x03:SBBD(eaa,id,LoadMd,SaveMd);break;
				case 0x04:ANDD(eaa,id,LoadMd,SaveMd);break;
				case 0x05:SUBD(eaa,id,LoadMd,SaveMd);break;
				case 0x06:XORD(eaa,id,LoadMd,SaveMd);break;
				case 0x07:CMPD(eaa,id,LoadMd,SaveMd);break;
				}
			}
		}
		break;
	CASE_D(0x85)												/* TEST Ed,Gd */
		RMEdGd(TESTD);break;
	CASE_D(0x87)												/* XCHG Ed,Gd */
		{	
			GetRMrd;Bit32u oldrmrd=*rmrd;
			if (rm >= 0xc0 ) {GetEArd;*rmrd=*eard;*eard=oldrmrd;}
			else {GetEAa;*rmrd=LoadMd(eaa);SaveMd(eaa,oldrmrd);}
			break;
		}
	CASE_D(0x89)												/* MOV Ed,Gd */
		{	
			GetRMrd;
			if (rm >= 0xc0 ) {GetEArd;*eard=*rmrd;}
			else {GetEAa;SaveMd(eaa,*rmrd);}
			break;
		}
	CASE_D(0x8b)												/* MOV Gd,Ed */
		{	
			GetRMrd;
			if (rm >= 0xc0 ) {GetEArd;*rmrd=*eard;}
			else {GetEAa;*rmrd=LoadMd(eaa);}
			break;
		}
	CASE_D(0x8c)												/* Mov Ew,Sw */
			{
				GetRM;Bit16u val;Bitu which=(rm>>3)&7;
				switch (which) {
				case 0x00:					/* MOV Ew,ES */
					val=SegValue(es);break;
				case 0x01:					/* MOV Ew,CS */
					val=SegValue(cs);break;
				case 0x02:					/* MOV Ew,SS */
					val=SegValue(ss);break;
				case 0x03:					/* MOV Ew,DS */
					val=SegValue(ds);break;
				case 0x04:					/* MOV Ew,FS */
					val=SegValue(fs);break;
				case 0x05:					/* MOV Ew,GS */
					val=SegValue(gs);break;
				default:
					val=0;
					E_Exit("CPU:8c:Illegal RM Byte");
				}
				if (rm >= 0xc0 ) {GetEArd;*eard=val;}
				else {GetEAa;SaveMw(eaa,val);}
				break;
			}	
	CASE_D(0x8d)												/* LEA Gd */
		{
			//Little hack to always use segprefixed version
			core.seg_prefix_base=0;
			GetRMrd;
			if (TEST_PREFIX_ADDR) {
				*rmrd=(Bit32u)(*GetEA_SEG_ADDR[rm])();
			} else {
				*rmrd=(Bit32u)(*GetEA_SEG[rm])();
			}
			break;
		}
	CASE_D(0x8f)												/* POP Ed */
		{
			Bit32u val=Pop_32();
			GetRM;
			if (rm >= 0xc0 ) {GetEArd;*eard=val;}
			else {GetEAa;SaveMd(eaa,val);}
			break;
		}
	CASE_D(0x91)												/* XCHG ECX,EAX */
		{ Bit32u temp=reg_eax;reg_eax=reg_ecx;reg_ecx=temp;break;}
	CASE_D(0x92)												/* XCHG EDX,EAX */
		{ Bit32u temp=reg_eax;reg_eax=reg_edx;reg_edx=temp;break;}
		break;
	CASE_D(0x93)												/* XCHG EBX,EAX */
		{ Bit32u temp=reg_eax;reg_eax=reg_ebx;reg_ebx=temp;break;}
		break;
	CASE_D(0x94)												/* XCHG ESP,EAX */
		{ Bit32u temp=reg_eax;reg_eax=reg_esp;reg_esp=temp;break;}
		break;
	CASE_D(0x95)												/* XCHG EBP,EAX */
		{ Bit32u temp=reg_eax;reg_eax=reg_ebp;reg_ebp=temp;break;}
		break;
	CASE_D(0x96)												/* XCHG ESI,EAX */
		{ Bit32u temp=reg_eax;reg_eax=reg_esi;reg_esi=temp;break;}
		break;
	CASE_D(0x97)												/* XCHG EDI,EAX */
		{ Bit32u temp=reg_eax;reg_eax=reg_edi;reg_edi=temp;break;}
		break;
	CASE_D(0x98)												/* CWDE */
		reg_eax=(Bit16s)reg_ax;break;
	CASE_D(0x99)												/* CDQ */
		if (reg_eax & 0x80000000) reg_edx=0xffffffff;
		else reg_edx=0;
		break;
	CASE_D(0x9a)												/* CALL FAR Ad */
		{ 
			Bit32u newip=Fetchd();Bit16u newcs=Fetchw();
			LEAVECORE;
			CPU_CALL(true,newcs,newip);
			goto decode_start;
		}
	CASE_D(0x9c)												/* PUSHFD */
		FillFlags();
		Push_32(reg_flags);
		break;
	CASE_D(0x9d)												/* POPFD */
		SETFLAGSd(Pop_32())
#if CPU_TRAP_CHECK
		if (GETFLAG(TF)) {	
			cpudecoder=CPU_Core_Normal_Trap_Run;
			goto decode_end;
		}
#endif
#ifdef CPU_PIC_CHECK
		if (GETFLAG(IF) && PIC_IRQCheck) goto decode_end;
#endif

		break;
	CASE_D(0xa1)												/* MOV EAX,Od */
		{
			GetEADirect;
			reg_eax=LoadMd(eaa);
		}
		break;
	CASE_D(0xa3)												/* MOV Od,EAX */
		{
			GetEADirect;
			SaveMd(eaa,reg_eax);
		}
		break;
	CASE_D(0xa5)												/* MOVSD */
		DoString(R_MOVSD);break;
	CASE_D(0xa7)												/* CMPSD */
		DoString(R_CMPSD);break;
	CASE_D(0xa9)												/* TEST EAX,Id */
		EAXId(TESTD);break;
	CASE_D(0xab)												/* STOSD */
		DoString(R_STOSD);break;
	CASE_D(0xad)												/* LODSD */
		DoString(R_LODSD);break;
	CASE_D(0xaf)												/* SCASD */
		DoString(R_SCASD);break;
	CASE_D(0xb8)												/* MOV EAX,Id */
		reg_eax=Fetchd();break;
	CASE_D(0xb9)												/* MOV ECX,Id */
		reg_ecx=Fetchd();break;
	CASE_D(0xba)												/* MOV EDX,Iw */
		reg_edx=Fetchd();break;
	CASE_D(0xbb)												/* MOV EBX,Id */
		reg_ebx=Fetchd();break;
	CASE_D(0xbc)												/* MOV ESP,Id */
		reg_esp=Fetchd();break;
	CASE_D(0xbd)												/* MOV EBP.Id */
		reg_ebp=Fetchd();break;
	CASE_D(0xbe)												/* MOV ESI,Id */
		reg_esi=Fetchd();break;
	CASE_D(0xbf)												/* MOV EDI,Id */
		reg_edi=Fetchd();break;
	CASE_D(0xc1)												/* GRP2 Ed,Ib */
		GRP2D(Fetchb());break;
	CASE_D(0xc2)												/* RETN Iw */
			{ 
				Bit16u addsp=Fetchw();
				SETIP(Pop_32());reg_esp+=addsp;
				break;  
			}
	CASE_D(0xc3)												/* RETN */
			SETIP(Pop_32());
			break;	
	CASE_D(0xc4)												/* LES */
		{	
			GetRMrd;GetEAa;
			LOADSEG(es,LoadMw(eaa+4));
			*rmrd=LoadMd(eaa);
			break;
		}
	CASE_D(0xc5)												/* LDS */
		{	
			GetRMrd;GetEAa;
			LOADSEG(ds,LoadMw(eaa+4));
			*rmrd=LoadMd(eaa);
			break;
		}
	CASE_D(0xc7)												/* MOV Ed,Id */
		{
			GetRM;
			if (rm >= 0xc0) {GetEArd;*eard=Fetchd();}
			else {GetEAa;SaveMd(eaa,Fetchd());}
			break;
		}
	CASE_D(0xc8)												/* ENTER Iw,Ib */
		{
			Bitu bytes=Fetchw();Bitu level=Fetchb() & 0x1f;
			Bitu frame_ptr=reg_esp-4;
			if (cpu.stack.big) {
				reg_esp-=4;
				mem_writed(SegBase(ss)+reg_esp,reg_ebp);
				for (Bitu i=1;i<level;i++) {	
					reg_ebp-=4;reg_esp-=4;
					mem_writed(SegBase(ss)+reg_esp,mem_readd(SegBase(ss)+reg_ebp));
				}
				if (level) {
					reg_esp-=4;
					mem_writed(SegBase(ss)+reg_esp,(Bit32u)frame_ptr);
				}
				reg_esp-=bytes;
			} else {
				reg_sp-=4;
				mem_writed(SegBase(ss)+reg_sp,reg_ebp);
				for (Bitu i=1;i<level;i++) {	
					reg_bp-=4;reg_sp-=4;
					mem_writed(SegBase(ss)+reg_sp,mem_readd(SegBase(ss)+reg_bp));
				}
				if (level) {
					reg_sp-=4;
					mem_writed(SegBase(ss)+reg_sp,(Bit32u)frame_ptr);
				}
				reg_sp-=bytes;
			}
			reg_ebp=frame_ptr;
			break;
		}
	CASE_D(0xc9)												/* LEAVE */
		reg_esp&=~cpu.stack.mask;
		reg_esp|=(reg_ebp&cpu.stack.mask);
		reg_ebp=Pop_32();
		break;
	CASE_D(0xca)												/* RETF Iw */
		{ 
			Bitu words=Fetchw();
			LEAVECORE;
			CPU_RET(true,words,core.ip_lookup-core.op_start);
			goto decode_start;
		}
	CASE_D(0xcb)												/* RETF */			
		{ 
			LEAVECORE;
            CPU_RET(true,0,core.ip_lookup-core.op_start);
			goto decode_start;
		}
	CASE_D(0xcf)												/* IRET */
		{
			LEAVECORE;
			CPU_IRET(true);
#if CPU_TRAP_CHECK
			if (GETFLAG(TF)) {	
				cpudecoder=CPU_Core_Normal_Trap_Run;
				return CBRET_NONE;
			}
#endif
#ifdef CPU_PIC_CHECK
			if (GETFLAG(IF) && PIC_IRQCheck) return CBRET_NONE;
#endif
//TODO TF check
			goto decode_start;
		}
	CASE_D(0xd1)												/* GRP2 Ed,1 */
		GRP2D(1);break;
	CASE_D(0xd3)												/* GRP2 Ed,CL */
		GRP2D(reg_cl);break;
	CASE_D(0xe5)												/* IN EAX,Ib */
		reg_eax=IO_ReadD(Fetchb());
		break;
	CASE_D(0xe7)												/* OUT Ib,EAX */
		IO_WriteD(Fetchb(),reg_eax);
		break;
	CASE_D(0xe8)												/* CALL Jd */
		{ 
			Bit32s newip=Fetchds();
			Push_32((Bit32u)GETIP);
			ADDIPd(newip);
			break;
		}
	CASE_D(0xe9)												/* JMP Jd */
		ADDIPd(Fetchds());
		break;
	CASE_D(0xea)												/* JMP Ad */
		{ 
			Bit32u newip=Fetchd();
			Bit16u newcs=Fetchw();
			LEAVECORE;
			CPU_JMP(true,newcs,newip,core.ip_lookup-core.op_start);
			goto decode_start;
		}
	CASE_D(0xed)												/* IN EAX,DX */
		reg_eax=IO_ReadD(reg_dx);
		break;
	CASE_D(0xef)												/* OUT DX,EAX */
		IO_WriteD(reg_dx,reg_eax);
		break;
	CASE_D(0xf7)												/* GRP3 Ed(,Id) */
		{ 
			GetRM;Bitu which=(rm>>3)&7;
			switch (which) {
			case 0x00:											/* TEST Ed,Id */
			case 0x01:											/* TEST Ed,Id Undocumented*/
				{
					if (rm >= 0xc0 ) {GetEArd;TESTD(*eard,Fetchd(),LoadRd,SaveRd);}
					else {GetEAa;TESTD(eaa,Fetchd(),LoadMd,SaveMd);}
					break;
				}
			case 0x02:											/* NOT Ed */
				{
					if (rm >= 0xc0 ) {GetEArd;*eard=~*eard;}
					else {GetEAa;SaveMd(eaa,~LoadMd(eaa));}
					break;
				}
			case 0x03:											/* NEG Ed */
				{
					lflags.type=t_NEGd;
					if (rm >= 0xc0 ) {
							GetEArd;lf_var1d=*eard;lf_resd=0-lf_var1d;
						*eard=lf_resd;
					} else {
						GetEAa;lf_var1d=LoadMd(eaa);lf_resd=0-lf_var1d;
							SaveMd(eaa,lf_resd);
					}
					break;
				}
			case 0x04:											/* MUL EAX,Ed */
				RMEd(MULD);
				break;
			case 0x05:											/* IMUL EAX,Ed */
				RMEd(IMULD);
				break;
			case 0x06:											/* DIV Ed */
				RMEd(DIVD);
				break;
			case 0x07:											/* IDIV Ed */
				RMEd(IDIVD);
				break;
			}
			break;
		}
	CASE_D(0xff)												/* GRP 5 Ed */
		{
			GetRM;Bitu which=(rm>>3)&7;
			switch (which) {
			case 0x00:											/* INC Ed */
				RMEd(INCD);
				break;		
			case 0x01:											/* DEC Ed */
				RMEd(DECD);
				break;
			case 0x02:											/* CALL NEAR Ed */
				if (rm >= 0xc0 ) {GetEArd;Push_32(GETIP);SETIP(*eard);}
				else {GetEAa;Push_32(GETIP);SETIP(LoadMd(eaa));}
				break;
			case 0x03:											/* CALL FAR Ed */
				{
					GetEAa;
					Bit32u newip=LoadMd(eaa);
					Bit16u newcs=LoadMw(eaa+4);
					LEAVECORE;
					CPU_CALL(true,newcs,newip);
					goto decode_start;
				}
				break;
			case 0x04:											/* JMP NEAR Ed */	
				if (rm >= 0xc0 ) {GetEArd;SETIP(*eard);}
				else {GetEAa;SETIP(LoadMd(eaa));}
				break;
			case 0x05:											/* JMP FAR Ed */	
				{
					GetEAa;
					Bit32u newip=LoadMd(eaa);
					Bit16u newcs=LoadMw(eaa+4);
					LEAVECORE;
					CPU_JMP(true,newcs,newip,core.ip_lookup-core.op_start);
					goto decode_start;
				}
				break;
			case 0x06:											/* Push Ed */
				if (rm >= 0xc0 ) {GetEArd;Push_32(*eard);}
				else {GetEAa;Push_32(LoadMd(eaa));}
				break;
			default:
				E_Exit("CPU:66:GRP5:Illegal call %2X",which);
				break;
			}
			break;
		}


