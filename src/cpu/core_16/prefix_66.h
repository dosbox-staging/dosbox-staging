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

restart_66:	
switch(Fetchb()) {
	case 0x01:												/* ADD Ed,Gd */
		RMEdGd(ADDD);break;	
	case 0x03:												/* ADD Gd,Ed */
		RMGdEd(ADDD);break;
	case 0x05:												/* ADD EAX,Id */
		EAXId(ADDD);break;
	case 0x09:												/* OR Ed,Gd */
		RMEdGd(ORD);break;
	case 0x0b:												/* OR Gd,Ed */
		RMGdEd(ORD);break;
	case 0x0d:												/* OR EAX,Id */
		EAXId(ORD);break;
	case 0x0f:												/* 2 Byte opcodes */
#include "prefix_66_of.h"
		break;
	case 0x11:												/* ADC Ed,Gd */
		RMEdGd(ADCD);break;	
	case 0x13:												/* ADC Gd,Ed */
		RMGdEd(ADCD);break;
	case 0x15:												/* ADC EAX,Id */
		EAXId(ADCD);break;
	case 0x19:												/* SBB Ed,Gd */
		RMEdGd(SBBD);break;
	case 0x1b:												/* SBB Gd,Ed */
		RMGdEd(SBBD);break;
	case 0x1d:												/* SBB EAX,Id */
		EAXId(SBBD);break;
	case 0x21:												/* AND Ed,Gd */
		RMEdGd(ANDD);break;	
	case 0x23:												/* AND Gd,Ed */
		RMGdEd(ANDD);break;
	case 0x25:												/* AND EAX,Id */
		EAXId(ANDD);break;
	case 0x29:												/* SUB Ed,Gd */
		RMEdGd(SUBD);break;
	case 0x2b:												/* SUB Gd,Ed */
		RMGdEd(SUBD);break;
	case 0x2d:												/* SUB EAX,Id */
		EAXId(SUBD);break;
	case 0x31:												/* XOR Ed,Gd */
		RMEdGd(XORD);break;	
	case 0x33:												/* XOR Gd,Ed */
		RMGdEd(XORD);break;
	case 0x35:												/* XOR EAX,Id */
		EAXId(XORD);break;
	case 0x39:												/* CMP Ed,Gd */
		RMEdGd(CMPD);break;
	case 0x3b:												/* CMP Gd,Ed */
		RMGdEd(CMPD);break;
	case 0x3d:												/* CMP EAX,Id */
		EAXId(CMPD);break;
	case 0x26:												/* SEG ES: */
		SegPrefix_66(es);break;
	case 0x2e:												/* SEG CS: */
		SegPrefix_66(cs);break;
	case 0x36:												/* SEG SS: */
		SegPrefix_66(ss);break;
	case 0x3e:												/* SEG DS: */
		SegPrefix_66(ds);break;
	case 0x40:												/* INC EAX */
		INCD(reg_eax,LoadRd,SaveRd);break;
	case 0x41:												/* INC ECX */
		INCD(reg_ecx,LoadRd,SaveRd);break;
	case 0x42:												/* INC EDX */
		INCD(reg_edx,LoadRd,SaveRd);break;
	case 0x43:												/* INC EBX */
		INCD(reg_ebx,LoadRd,SaveRd);break;
	case 0x44:												/* INC ESP */
		INCD(reg_esp,LoadRd,SaveRd);break;
	case 0x45:												/* INC EBP */
		INCD(reg_ebp,LoadRd,SaveRd);break;
	case 0x46:												/* INC ESI */
		INCD(reg_esi,LoadRd,SaveRd);break;
	case 0x47:												/* INC EDI */
		INCD(reg_edi,LoadRd,SaveRd);break;
	case 0x48:												/* DEC EAX */
		DECD(reg_eax,LoadRd,SaveRd);break;
	case 0x49:												/* DEC ECX */
		DECD(reg_ecx,LoadRd,SaveRd);break;
	case 0x4a:												/* DEC EDX */
		DECD(reg_edx,LoadRd,SaveRd);break;
	case 0x4b:												/* DEC EBX */
		DECD(reg_ebx,LoadRd,SaveRd);break;
	case 0x4c:												/* DEC ESP */
		DECD(reg_esp,LoadRd,SaveRd);break;
	case 0x4d:												/* DEC EBP */
		DECD(reg_ebp,LoadRd,SaveRd);break;
	case 0x4e:												/* DEC ESI */
		DECD(reg_esi,LoadRd,SaveRd);break;
	case 0x4f:												/* DEC EDI */
		DECD(reg_edi,LoadRd,SaveRd);break;
	case 0x50:												/* PUSH EAX */
		Push_32(reg_eax);break;
	case 0x51:												/* PUSH ECX */
		Push_32(reg_ecx);break;
	case 0x52:												/* PUSH EDX */
		Push_32(reg_edx);break;
	case 0x53:												/* PUSH EBX */
		Push_32(reg_ebx);break;
	case 0x54:												/* PUSH ESP */
		Push_32(reg_esp);break;
	case 0x55:												/* PUSH EBP */
		Push_32(reg_ebp);break;
	case 0x56:												/* PUSH ESI */
		Push_32(reg_esi);break;
	case 0x57:												/* PUSH EDI */
		Push_32(reg_edi);break;
	case 0x58:												/* POP EAX */
		reg_eax=Pop_32();break;
	case 0x59:												/* POP ECX */
		reg_ecx=Pop_32();break;
	case 0x5a:												/* POP EDX */
		reg_edx=Pop_32();break;
	case 0x5b:												/* POP EBX */
		reg_ebx=Pop_32();break;
	case 0x5c:												/* POP ESP */
		reg_esp=Pop_32();break;
	case 0x5d:												/* POP EBP */
		reg_ebp=Pop_32();break;
	case 0x5e:												/* POP ESI */
		reg_esi=Pop_32();break;
	case 0x5f:												/* POP EDI */
		reg_edi=Pop_32();break;
	case 0x60:												/* PUSHAD */
		Push_32(reg_eax);Push_32(reg_ecx);Push_32(reg_edx);Push_32(reg_ebx);
		Push_32(reg_esp);Push_32(reg_ebp);Push_32(reg_esi);Push_32(reg_edi);
		break;
	case 0x61:												/* POPAD */
		reg_edi=Pop_32();reg_esi=Pop_32();reg_ebp=Pop_32();Pop_32();//Don't save ESP
		reg_ebx=Pop_32();reg_edx=Pop_32();reg_ecx=Pop_32();reg_eax=Pop_32();
		break;
	case 0x64:												/* SEG FS: */
		SegPrefix_66(fs);break;
	case 0x65:												/* SEG GS: */
		SegPrefix_66(gs);break;
	case 0x67:												/* Address Size Prefix */
#ifdef CPU_PREFIX_67
			core_16.prefixes|=PREFIX_ADDR;
			lookupEATable=EAPrefixTable[core_16.prefixes];
			goto restart_66;
#else
			NOTDONE;
#endif
	case 0x68:												/* PUSH Id */
		Push_32(Fetchd());break;
	case 0x69:												/* IMUL Gd,Ed,Id */
		RMGdEdOp3(DIMULD,Fetchds());
		break;
	case 0x6a:												/* PUSH Ib */
		Push_32(Fetchbs());break;
	case 0x6b:												/* IMUL Gd,Ed,Ib */
		RMGdEdOp3(DIMULD,Fetchbs());
		break;
	case 0x81:												/* Grpl Ed,Id */
		{
			GetRM;
			if (rm>= 0xc0) {
				GetEArd;Bit32u id=Fetchd();
				switch (rm & 0x38) {
				case 0x00:ADDD(*eard,id,LoadRd,SaveRd);break;
				case 0x08: ORD(*eard,id,LoadRd,SaveRd);break;
				case 0x10:ADCD(*eard,id,LoadRd,SaveRd);break;
				case 0x18:SBBD(*eard,id,LoadRd,SaveRd);break;
				case 0x20:ANDD(*eard,id,LoadRd,SaveRd);break;
				case 0x28:SUBD(*eard,id,LoadRd,SaveRd);break;
				case 0x30:XORD(*eard,id,LoadRd,SaveRd);break;
				case 0x38:CMPD(*eard,id,LoadRd,SaveRd);break;
				}
			} else {
				GetEAa;Bit32u id=Fetchd();
				switch (rm & 0x38) {
				case 0x00:ADDD(eaa,id,LoadMd,SaveMd);break;
				case 0x08: ORD(eaa,id,LoadMd,SaveMd);break;
				case 0x10:ADCD(eaa,id,LoadMd,SaveMd);break;
				case 0x18:SBBD(eaa,id,LoadMd,SaveMd);break;
				case 0x20:ANDD(eaa,id,LoadMd,SaveMd);break;
				case 0x28:SUBD(eaa,id,LoadMd,SaveMd);break;
				case 0x30:XORD(eaa,id,LoadMd,SaveMd);break;
				case 0x38:CMPD(eaa,id,LoadMd,SaveMd);break;
				}
			}
		}
		break;
	case 0x83:												/* Grpl Ed,Ix */
		{
			GetRM;
			if (rm>= 0xc0) {
				GetEArd;Bit32u id=(Bit32s)Fetchbs();
				switch (rm & 0x38) {
				case 0x00:ADDD(*eard,id,LoadRd,SaveRd);break;
				case 0x08: ORD(*eard,id,LoadRd,SaveRd);break;
				case 0x10:ADCD(*eard,id,LoadRd,SaveRd);break;
				case 0x18:SBBD(*eard,id,LoadRd,SaveRd);break;
				case 0x20:ANDD(*eard,id,LoadRd,SaveRd);break;
				case 0x28:SUBD(*eard,id,LoadRd,SaveRd);break;
				case 0x30:XORD(*eard,id,LoadRd,SaveRd);break;
				case 0x38:CMPD(*eard,id,LoadRd,SaveRd);break;
				}
			} else {
				GetEAa;Bit32u id=(Bit32s)Fetchbs();
				switch (rm & 0x38) {
				case 0x00:ADDD(eaa,id,LoadMd,SaveMd);break;
				case 0x08: ORD(eaa,id,LoadMd,SaveMd);break;
				case 0x10:ADCD(eaa,id,LoadMd,SaveMd);break;
				case 0x18:SBBD(eaa,id,LoadMd,SaveMd);break;
				case 0x20:ANDD(eaa,id,LoadMd,SaveMd);break;
				case 0x28:SUBD(eaa,id,LoadMd,SaveMd);break;
				case 0x30:XORD(eaa,id,LoadMd,SaveMd);break;
				case 0x38:CMPD(eaa,id,LoadMd,SaveMd);break;
				}
			}
		}
		break;
	case 0x85:												/* TEST Ed,Gd */
		RMEdGd(TESTD);break;
	case 0x87:												/* XCHG Ev,Gv */
		{	
			GetRMrd;Bit32u oldrmrd=*rmrd;
			if (rm >= 0xc0 ) {GetEArd;*rmrd=*eard;*eard=oldrmrd;}
			else {GetEAa;*rmrd=LoadMd(eaa);SaveMd(eaa,oldrmrd);}
			break;
		}
	case 0x89:												/* MOV Ed,Gd */
		{	
			GetRMrd;
			if (rm >= 0xc0 ) {GetEArd;*eard=*rmrd;}
			else {GetEAa;SaveMd(eaa,*rmrd);}
			break;
		}
	case 0x8b:												/* MOV Gd,Ed */
		{	
			GetRMrd;
			if (rm >= 0xc0 ) {GetEArd;*rmrd=*eard;}
			else {GetEAa;*rmrd=LoadMd(eaa);}
			break;
		}
	case 0x8c:												
		LOG(LOG_CPU,LOG_NORMAL)("CPU:66:8c looped back");
		break;
	case 0x8d:												/* LEA */
		{
			core_16.segbase=0;
			core_16.prefixes|=PREFIX_SEG;
			lookupEATable=EAPrefixTable[core_16.prefixes];
			GetRMrd;GetEAa;
			*rmrd=(Bit32u)eaa;
			break;
		}
	case 0x8f:												/* POP Ed */
		{
			GetRM;
			if (rm >= 0xc0 ) {GetEArd;*eard=Pop_32();}
			else {GetEAa;SaveMd(eaa,Pop_32());}
			break;
		}
	case 0x90:												/* NOP */
		break;
	case 0x91:												/* XCHG ECX,EAX */
		{ Bit32u temp=reg_eax;reg_eax=reg_ecx;reg_ecx=temp; }
		break;
	case 0x92:												/* XCHG EDX,EAX */
		{ Bit32u temp=reg_eax;reg_eax=reg_edx;reg_edx=temp; }
		break;
	case 0x93:												/* XCHG EBX,EAX */
		{ Bit32u temp=reg_eax;reg_eax=reg_ebx;reg_ebx=temp; }
		break;
	case 0x94:												/* XCHG ESP,EAX */
		{ Bit32u temp=reg_eax;reg_eax=reg_esp;reg_esp=temp; }
		break;
	case 0x95:												/* XCHG EBP,EAX */
		{ Bit32u temp=reg_eax;reg_eax=reg_ebp;reg_ebp=temp; }
		break;
	case 0x96:												/* XCHG ESI,EAX */
		{ Bit32u temp=reg_eax;reg_eax=reg_esi;reg_esi=temp; }
		break;
	case 0x97:												/* XCHG EDI,EAX */
		{ Bit32u temp=reg_eax;reg_eax=reg_edi;reg_edi=temp; }
		break;
	case 0x98:												/* CWD */
		reg_eax=(Bit16s)reg_ax;break;
	case 0x99:												/* CDQ */
		if (reg_eax & 0x80000000) reg_edx=0xffffffff;
		else reg_edx=0;
		break;
	case 0x9c:												/* PUSHFD */
		FILLFLAGS;
		Push_32(flags.word);
		break;
	case 0x9d:												/* POPFD */
		SETFLAGSd(Pop_32())
		CheckTF();
#ifdef CPU_PIC_CHECK
		if (GETFLAG(IF) && PIC_IRQCheck) goto decode_end;
#endif
		break;
	case 0xa1:												/* MOV EAX,Ow */
		reg_eax=LoadMd(GetEADirect[core_16.prefixes]());
		break;
	case 0xa3:												/* MOV Ow,EAX */
		SaveMd(GetEADirect[core_16.prefixes](),reg_eax);
		break;
	case 0xa5:												/* MOVSD */
		{
			stringSI;stringDI;SaveMd(to,LoadMd(from));
			if (GETFLAG(DF)) { reg_si-=4;reg_di-=4; }
			else { reg_si+=4;reg_di+=4;}
		}
		break;
	case 0xa7:												/* CMPSD */
		{	
			stringSI;stringDI; CMPD(to,LoadMd(from),LoadMd,0);
			if (GETFLAG(DF)) { reg_si-=4;reg_di-=4; }
			else { reg_si+=4;reg_di+=4;}
		}
		break;
	case 0xa9:												/* TEST EAX,Id */
		EAXId(TESTD);
		break;
	case 0xab:												/* STOSD */
		{
			stringDI;
			SaveMd(to,reg_eax);
			if (GETFLAG(DF)) { reg_di-=4; }
			else {reg_di+=4;}
			break;
		}
	case 0xad:												/* LODSD */
		{
			stringSI;
			reg_eax=LoadMd(from);
			if (GETFLAG(DF)) { reg_si-=4;}
			else {reg_si+=4;}
			break;
		}
	case 0xaf:												/* SCASD */
		{
			stringDI;
			CMPD(reg_eax,LoadMd(to),LoadRd,0);
			if (GETFLAG(DF)) { reg_di-=4; }
			else {reg_di+=4;}
			break;
		}
	case 0xb8:												/* MOV EAX,Id */
		reg_eax=Fetchd();break;
	case 0xb9:												/* MOV ECX,Id */
		reg_ecx=Fetchd();break;
	case 0xba:												/* MOV EDX,Iw */
		reg_edx=Fetchd();break;
	case 0xbb:												/* MOV EBX,Id */
		reg_ebx=Fetchd();break;
	case 0xbc:												/* MOV ESP,Id */
		reg_esp=Fetchd();break;
	case 0xbd:												/* MOV EBP.Id */
		reg_ebp=Fetchd();break;
	case 0xbe:												/* MOV ESI,Id */
		reg_esi=Fetchd();break;
	case 0xbf:												/* MOV EDI,Id */
		reg_edi=Fetchd();break;
	case 0xc1:												/* GRP2 Ed,Ib */
		GRP2D(Fetchb());break;
	case 0xc4:												/* LES */
		{	
			GetRMrd;GetEAa;
			*rmrd=LoadMd(eaa);SegSet16(es,LoadMw(eaa+4));
			break;
		}
	case 0xc5:												/* LDS */
		{	
			GetRMrd;GetEAa;
			*rmrd=LoadMd(eaa);SegSet16(ds,LoadMw(eaa+4));
			break;
		}
	case 0xc7:												/* MOV Ed,Id */
		{
			GetRM;
			if (rm>0xc0) {GetEArd;*eard=Fetchd();}
			else {GetEAa;SaveMd(eaa,Fetchd());}
			break;
		}
	case 0xd1:												/* GRP2 Ed,1 */
		GRP2D(1);break;
	case 0xd3:												/* GRP2 Ed,CL */
		GRP2D(reg_cl);break;
	case 0xed:												/* IN EAX,DX */
		reg_eax=IO_Read(reg_dx) | 
				(IO_Read(reg_dx+1) << 8) | 
				(IO_Read(reg_dx+2) << 16) | 
				(IO_Read(reg_dx+3) << 24);
		break;
	case 0xef:												/* OUT DX,EAX */
		IO_Write(reg_dx,(Bit8u)(reg_eax>>0));
		IO_Write(reg_dx+1,(Bit8u)(reg_eax>>8));
		IO_Write(reg_dx+2,(Bit8u)(reg_eax>>16));
		IO_Write(reg_dx+3,(Bit8u)(reg_eax>>24));
		break;
	case 0xf2:												/* REPNZ */
		Repeat_Normal(false,true);
		continue;
	case 0xf3:												/* REPZ */
		Repeat_Normal(true,true);
		continue;
	case 0xf7:												/* GRP3 Ed(,Id) */
		{ 
			GetRM;
			switch ((rm & 0x38)>>3) {
			case 0x00:					/* TEST Ed,Id */
			case 0x01:					/* TEST Ed,Id Undocumented*/
				{
					if (rm >= 0xc0 ) {GetEArd;TESTD(*eard,Fetchd(),LoadRd,SaveRd);}
					else {GetEAa;TESTD(eaa,Fetchd(),LoadMd,SaveMd);}
					break;
				}
			case 0x02:					/* NOT Ed */
				{
					if (rm >= 0xc0 ) {GetEArd;*eard=~*eard;}
					else {GetEAa;SaveMd(eaa,~LoadMd(eaa));}
					break;
				}
			case 0x03:					/* NEG Ed */
				{
					flags.type=t_NEGd;
					if (rm >= 0xc0 ) {
							GetEArd;flags.var1.d=*eard;flags.result.d=0-flags.var1.d;
						*eard=flags.result.d;
					} else {
						GetEAa;flags.var1.d=LoadMd(eaa);flags.result.d=0-flags.var1.d;
							SaveMd(eaa,flags.result.d);
					}
					break;
				}
			case 0x04:					/* MUL EAX,Ed */
				RMEd(MULD);
				break;
			case 0x05:					/* IMUL EAX,Ed */
				RMEd(IMULD);
				break;
			case 0x06:					/* DIV Ed */
				RMEd(DIVD);
				break;
			case 0x07:					/* IDIV Ed */
				RMEd(IDIVD);
				break;
			}
			break;
		}
		case 0xff:												/* Group 5 */
		{
			GetRM;
			switch (rm & 0x38) {
			case 0x00:										/* INC Ed */
				RMEd(INCD);
				break;		
			case 0x08:										/* DEC Ed */
				RMEd(DECD);
				break;
			case 0x30:										/* Push Ed */
				if (rm >= 0xc0 ) {GetEArd;Push_32(*eard);}
				else {GetEAa;Push_32(LoadMd(eaa));}
				break;
			default:
				E_Exit("CPU:66:GRP5:Illegal call %2X",rm & 0x38);
				break;
			}
			break;
		}
	default:
		NOTDONE66;
	}


