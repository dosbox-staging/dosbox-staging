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

switch(Fetchb()) {
	case 0x00:												/* GRP 6 */
		{
			GetRM;Bitu which=(rm>>3)&7;
			switch (which) {
			case 0x00:	/* SLDT */
			case 0x01:	/* STR */
				{
					Bitu saveval;
					if (!which) CPU_SLDT(saveval);
					else CPU_STR(saveval);
					if (rm>0xc0) {GetEArw;*earw=saveval;}
					else {GetEAa;SaveMw(eaa,saveval);}
				}
				break;
			case 0x02:case 0x03:case 0x04:case 0x05:
				{
					Bitu loadval;
					if (rm >= 0xc0 ) {GetEArw;loadval=*earw;}
					else {GetEAa;loadval=LoadMw(eaa);}
					break;
					switch (which) {
					case 0x02:CPU_LLDT(loadval);break;
					case 0x03:CPU_LTR(loadval);break;
					case 0x04:CPU_VERR(loadval);break;
					case 0x05:CPU_VERW(loadval);break;
					}
				}
			default:
				LOG(LOG_CPU,LOG_ERROR)("GRP6:Illegal call %2X",which);
			}
		}
		break;
	case 0x01:											/* Group 7 Ew */
		{
			GetRM;Bitu which=(rm>>3)&7;
			if (rm < 0xc0)	{ //First ones all use EA
				GetEAa;Bitu limit,base;
				switch (which) {
				case 0x00:								/* SGDT */
					CPU_SGDT(limit,base);
					SaveMw(eaa,limit);
					SaveMd(eaa+2,base);
					break;
				case 0x01:								/* SIDT */
					CPU_SIDT(limit,base);
					SaveMw(eaa,limit);
					SaveMd(eaa+2,base);
					break;
				case 0x02:								/* LGDT */
					CPU_LGDT(LoadMw(eaa),LoadMd(eaa+2) & 0xFFFFFF);
					break;
				case 0x03:								/* LIDT */
					CPU_LIDT(LoadMw(eaa),LoadMd(eaa+2) & 0xFFFFFF);
					break;
				case 0x04:								/* SMSW */
					CPU_SMSW(limit);
					SaveMw(eaa,limit);
					break;
				case 0x06:								/* LMSW */
					limit=LoadMw(eaa);
					if (!CPU_LMSW(limit)) goto decode_end;
					break;
				}
			} else {
				GetEArw;Bitu limit;
				switch (which) {
				case 0x04:								/* SMSW */
					CPU_SMSW(limit);
					*earw=limit;
					break;
				case 0x06:								/* LMSW */
					if (!CPU_LMSW(*earw)) goto decode_end;
					break;
				default:
					LOG(LOG_CPU,LOG_ERROR)("Illegal group 7 RM subfunction %d",which);
					break;
				}
			}
		}
		break;
	case 0x20:												/* MOV Rd.CRx */
		{
			GetRM;
			Bitu which=(rm >> 3) & 7;
			if (rm >= 0xc0 ) {
				GetEArd;
				*eard=CPU_GET_CRX(which);
			} else {
				GetEAa;
				LOG(LOG_CPU,LOG_ERROR)("MOV XXX,CR%d with non-register",which);
			}
		}
		break;
	case 0x22:												/* MOV CRx,Rd */
		{
			GetRM;
			Bitu which=(rm >> 3) & 7;
			if (rm >= 0xc0 ) {
				GetEArd;
				if (!CPU_SET_CRX(which,*eard)) goto decode_end;
			} else {
				GetEAa;
				LOG(LOG_CPU,LOG_ERROR)("MOV CR%,XXX with non-register",which);
			}
		}
		break;
	case 0x23:												/* MOV DRx,Rd */
		{
			GetRM;
			Bitu which=(rm >> 3) & 7;
			if (rm >= 0xc0 ) {
				GetEArd;
			} else {
				GetEAa;
				LOG(LOG_CPU,LOG_ERROR)("MOV DR%,XXX with non-register",which);
			}
		}
		break;
	case 0x80:												/* JO */
		JumpSIw(get_OF());break;
	case 0x81:												/* JNO */
		JumpSIw(!get_OF());break;
	case 0x82:												/* JB */
		JumpSIw(get_CF());break;
	case 0x83:												/* JNB */
		JumpSIw(!get_CF());break;
	case 0x84:												/* JZ */
		JumpSIw(get_ZF());break;
	case 0x85:												/* JNZ */
		JumpSIw(!get_ZF());	break;
	case 0x86:												/* JBE */
		JumpSIw(get_CF() || get_ZF());break;
	case 0x87:												/* JNBE */
		JumpSIw(!get_CF() && !get_ZF());break;
	case 0x88:												/* JS */
		JumpSIw(get_SF());break;
	case 0x89:												/* JNS */
		JumpSIw(!get_SF());break;
	case 0x8a:												/* JP */
		JumpSIw(get_PF());break;
	case 0x8b:												/* JNP */
		JumpSIw(!get_PF());break;
	case 0x8c:												/* JL */
		JumpSIw(get_SF() != get_OF());break;
	case 0x8d:												/* JNL */
		JumpSIw(get_SF() == get_OF());break;
	case 0x8e:												/* JLE */
		JumpSIw(get_ZF() || (get_SF() != get_OF()));break;
	case 0x8f:												/* JNLE */
		JumpSIw((get_SF() == get_OF()) && !get_ZF());break;

	case 0x90:												/* SETO */
		SETcc(get_OF());break;
	case 0x91:												/* SETNO */
		SETcc(!get_OF());break;
	case 0x92:												/* SETB */
		SETcc(get_CF());break;
	case 0x93:												/* SETNB */
		SETcc(!get_CF());break;
	case 0x94:												/* SETZ */
		SETcc(get_ZF());break;
	case 0x95:												/* SETNZ */
		SETcc(!get_ZF());	break;
	case 0x96:												/* SETBE */
		SETcc(get_CF() || get_ZF());break;
	case 0x97:												/* SETNBE */
		SETcc(!get_CF() && !get_ZF());break;
	case 0x98:												/* SETS */
		SETcc(get_SF());break;
	case 0x99:												/* SETNS */
		SETcc(!get_SF());break;
	case 0x9a:												/* SETP */
		SETcc(get_PF());break;
	case 0x9b:												/* SETNP */
		SETcc(!get_PF());break;
	case 0x9c:												/* SETL */
		SETcc(get_SF() != get_OF());break;
	case 0x9d:												/* SETNL */
		SETcc(get_SF() == get_OF());break;
	case 0x9e:												/* SETLE */
		SETcc(get_ZF() || (get_SF() != get_OF()));break;
	case 0x9f:												/* SETNLE */
		SETcc((get_SF() == get_OF()) && !get_ZF());break;

	case 0xa0:												/* PUSH FS */		
		Push_16(SegValue(fs));break;
	case 0xa1:												/* POP FS */		
		SegSet16(fs,Pop_16());break;
	case 0xa2:
		CPU_CPUID();
		break;
	case 0xa3:												/* BT Ew,Gw */
		{
			GetRMrw;
			Bit16u mask=1 << (*rmrw & 15);
			if (rm >= 0xc0 ) {
				GetEArw;
				SETFLAGBIT(CF,(*earw & mask));
			} else {
				GetEAa;Bit16u old=LoadMw(eaa);
				SETFLAGBIT(CF,(old & mask));
			}
			if (flags.type!=t_CF)	{ flags.prev_type=flags.type;flags.type=t_CF;	}
			break;
		}
	case 0xa4:												/* SHLD Ew,Gw,Ib */
		RMEwGwOp3(DSHLW,Fetchb());
		break;
	case 0xa5:												/* SHLD Ew,Gw,CL */
		RMEwGwOp3(DSHLW,reg_cl);
		break;
	case 0xa8:												/* PUSH GS */		
		Push_16(SegValue(gs));break;
	case 0xa9:												/* POP GS */		
		SegSet16(gs,Pop_16());break;
	case 0xab:												/* BTS Ew,Gw */
		{
			GetRMrw;
			Bit16u mask=1 << (*rmrw & 15);
			if (rm >= 0xc0 ) {
				GetEArw;
				SETFLAGBIT(CF,(*earw & mask));
				*earw|=mask;
			} else {
				GetEAa;Bit16u old=LoadMw(eaa);
				SETFLAGBIT(CF,(old & mask));
				SaveMw(eaa,old | mask);
			}
			if (flags.type!=t_CF)	{ flags.prev_type=flags.type;flags.type=t_CF;	}
			break;
		}
	case 0xac:												/* SHRD Ew,Gw,Ib */
		RMEwGwOp3(DSHRW,Fetchb());
		break;
	case 0xad:												/* SHRD Ew,Gw,CL */
		RMEwGwOp3(DSHRW,reg_cl);
		break;
	case 0xaf:												/* IMUL Gw,Ew */
		RMGwEwOp3(DIMULW,*rmrw);
		break;
	
	case 0xb2:												/* LSS */
		{	
			GetRMrw;GetEAa;
			*rmrw=LoadMw(eaa);SegSet16(ss,LoadMw(eaa+2));
			break;
		}
	case 0xb3:												/* BTR Ew,Gw */
		{
			GetRMrw;
			Bit16u mask=1 << (*rmrw & 15);
			if (rm >= 0xc0 ) {
				GetEArw;
				SETFLAGBIT(CF,(*earw & mask));
				*earw&= ~mask;
			} else {
				GetEAa;Bit16u old=LoadMw(eaa);
				SETFLAGBIT(CF,(old & mask));
				SaveMw(eaa,old & ~mask);
			}
			if (flags.type!=t_CF)	{ flags.prev_type=flags.type;flags.type=t_CF;	}
			break;
		}
	case 0xb4:												/* LFS */
		{	
			GetRMrw;GetEAa;
			*rmrw=LoadMw(eaa);SegSet16(fs,LoadMw(eaa+2));
			break;
		}
	case 0xb5:												/* LGS */
		{	
			GetRMrw;GetEAa;
			*rmrw=LoadMw(eaa);SegSet16(gs,LoadMw(eaa+2));
			break;
		}
	case 0xb6:												/* MOVZX Gw,Eb */
		{
			GetRMrw;															
			if (rm >= 0xc0 ) {GetEArb;*rmrw=*earb;}
			else {GetEAa;*rmrw=LoadMb(eaa);}
			break;
		}
	case 0xb7:												/* MOVZX Gw,Ew */
	case 0xbf:												/* MOVSX Gw,Ew */
		{
			GetRMrw;															
			if (rm >= 0xc0 ) {GetEArw;*rmrw=*earw;}
			else {GetEAa;*rmrw=LoadMw(eaa);}
			break;
		}
	case 0xba:												/* GRP8 Ew,Ib */
		{
			GetRM;
			if (rm >= 0xc0 ) {
				GetEArw;
				Bit16u mask=1 << (Fetchb() & 15);
				SETFLAGBIT(CF,(*earw & mask));
				switch (rm & 0x38) {
				case 0x20:									/* BT */
					break;
				case 0x28:									/* BTS */
					*earw|=mask;
					break;
				case 0x30:									/* BTR */
					*earw&= ~mask;
					break;
				case 0x38:									/* BTC */
					*earw^=mask;
					break;
				default:
					E_Exit("CPU:0F:BA:Illegal subfunction %X",rm & 0x38);
				}
			} else {
				GetEAa;Bit16u old=LoadMw(eaa);
				Bit16u mask=1 << (Fetchb() & 15);
				SETFLAGBIT(CF,(old & mask));
				switch (rm & 0x38) {
				case 0x20:									/* BT */
					break;
				case 0x28:									/* BTS */
					SaveMw(eaa,old|mask);
					break;
				case 0x30:									/* BTR */
					SaveMw(eaa,old & ~mask);
					break;
				case 0x38:									/* BTC */
					SaveMw(eaa,old ^ mask);
					break;
				default:
					E_Exit("CPU:0F:BA:Illegal subfunction %X",rm & 0x38);
				}
			}
			if (flags.type!=t_CF)	{ flags.prev_type=flags.type;flags.type=t_CF;	}
			break;
		}
	case 0xbb:												/* BTC Ew,Gw */
		{
			GetRMrw;
			Bit16u mask=1 << (*rmrw & 15);
			if (rm >= 0xc0 ) {
				GetEArw;
				SETFLAGBIT(CF,(*earw & mask));
				*earw^=mask;
			} else {
				GetEAa;Bit16u old=LoadMw(eaa);
				SETFLAGBIT(CF,(old & mask));
				SaveMw(eaa,old ^ mask);
			}
			if (flags.type!=t_CF)	{ flags.prev_type=flags.type;flags.type=t_CF;	}
			break;
		}
	case 0xbc:												/* 0xbc BSF Gw,Ew */
		{
			GetRMrw;
			Bit16u result,value;
			if (rm >= 0xc0) { GetEArw; value=*earw; } 
			else			{ GetEAa; value=LoadMw(eaa); }
			if (value==0) {
				SETFLAGBIT(ZF,true);
			} else {
				result = 0;
				while ((value & 0x01)==0) { result++; value>>=1; }
				SETFLAGBIT(ZF,false);
				*rmrw = result;
			}
			flags.type=t_UNKNOWN;
			break;
		}
	case 0xbd:												/* 0xbd BSR Gw,Ew */
		{
			GetRMrw;
			Bit16u result,value;
			if (rm >= 0xc0) { GetEArw; value=*earw; } 
			else			{ GetEAa; value=LoadMw(eaa); }
			if (value==0) {
				SETFLAGBIT(ZF,true);
			} else {
				result = 15;	// Operandsize-1
				while ((value & 0x8000)==0) { result--; value<<=1; }
				SETFLAGBIT(ZF,false);
				*rmrw = result;
			}
			flags.type=t_UNKNOWN;
			break;
		}
	case 0xbe:												/* MOVSX Gw,Eb */
		{
			GetRMrw;															
			if (rm >= 0xc0 ) {GetEArb;*rmrw=*(Bit8s *)earb;}
			else {GetEAa;*rmrw=LoadMbs(eaa);}
			break;
		}
	case 0xc8:	BSWAP(reg_eax);		break;
	case 0xc9:	BSWAP(reg_ecx);		break;
	case 0xca:	BSWAP(reg_edx);		break;
	case 0xcb:	BSWAP(reg_ebx);		break;
	case 0xcc:	BSWAP(reg_esp);		break;
	case 0xcd:	BSWAP(reg_ebp);		break;
	case 0xce:	BSWAP(reg_esi);		break;
	case 0xcf:	BSWAP(reg_edi);		break;
		
	default:
		 SUBIP(1);
		 E_Exit("CPU:Opcode 0F:%2X Unhandled",Fetchb());
	}
