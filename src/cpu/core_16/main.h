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

bool repcheck;
restart:
	switch(Fetchb()) {
	case 0x00:												/* ADD Eb,Gb */
		RMEbGb(ADDB);break;
	case 0x01:												/* ADD Ew,Gw */
		RMEwGw(ADDW);break;	
	case 0x02:												/* ADD Gb,Eb */
		RMGbEb(ADDB);break;
	case 0x03:												/* ADD Gw,Ew */
		RMGwEw(ADDW);break;
	case 0x04:												/* ADD AL,Ib */
		ALIb(ADDB);break;
	case 0x05:												/* ADD AX,Iw */
		AXIw(ADDW);break;
	case 0x06:												/* PUSH ES */		
		Push_16(Segs[es].value);break;
	case 0x07:												/* POP ES */		
		SetSegment_16(es,Pop_16());break;
	case 0x08:												/* OR Eb,Gb */
		RMEbGb(ORB);break;
	case 0x09:												/* OR Ew,Gw */
		RMEwGw(ORW);break;
	case 0x0a:												/* OR Gb,Eb */
		RMGbEb(ORB);break;
	case 0x0b:												/* OR Gw,Ew */
		RMGwEw(ORW);break;
	case 0x0c:												/* OR AL,Ib */
		ALIb(ORB);break;
	case 0x0d:												/* OR AX,Iw */
		AXIw(ORW);break;
	case 0x0e:												/* PUSH CS */		
		Push_16(Segs[cs].value);break;
	case 0x0f:												/* 2 byte opcodes*/		
		#include "prefix_of.h"
		break;
	case 0x10:												/* ADC Eb,Gb */
		RMEbGb(ADCB);break;
	case 0x11:												/* ADC Ew,Gw */
		RMEwGw(ADCW);break;	
	case 0x12:												/* ADC Gb,Eb */
		RMGbEb(ADCB);break;
	case 0x13:												/* ADC Gw,Ew */
		RMGwEw(ADCW);break;
	case 0x14:												/* ADC AL,Ib */
		ALIb(ADCB);break;
	case 0x15:												/* ADC AX,Iw */
		AXIw(ADCW);break;
	case 0x16:												/* PUSH SS */		
		Push_16(Segs[ss].value);break;
	case 0x17:												/* POP SS */		
		SetSegment_16(ss,Pop_16());break;
	case 0x18:												/* SBB Eb,Gb */
		RMEbGb(SBBB);break;
	case 0x19:												/* SBB Ew,Gw */
		RMEwGw(SBBW);break;
	case 0x1a:												/* SBB Gb,Eb */
		RMGbEb(SBBB);break;
	case 0x1b:												/* SBB Gw,Ew */
		RMGwEw(SBBW);break;
	case 0x1c:												/* SBB AL,Ib */
		ALIb(SBBB);break;
	case 0x1d:												/* SBB AX,Iw */
		AXIw(SBBW);break;
	case 0x1e:												/* PUSH DS */		
		Push_16(Segs[ds].value);break;
	case 0x1f:												/* POP DS */		
		SetSegment_16(ds,Pop_16());break;
	case 0x20:												/* AND Eb,Gb */
		RMEbGb(ANDB);break;
	case 0x21:												/* AND Ew,Gw */
		RMEwGw(ANDW);break;	
	case 0x22:												/* AND Gb,Eb */
		RMGbEb(ANDB);break;
	case 0x23:												/* AND Gw,Ew */
		RMGwEw(ANDW);break;
	case 0x24:												/* AND AL,Ib */
		ALIb(ANDB);break;
	case 0x25:												/* AND AX,Iw */
		AXIw(ANDW);break;
	case 0x26:												/* SEG ES: */
		SegPrefix(es);break;
	case 0x27:												/* DAA */
		if (((reg_al & 0x0F)>0x09) || get_AF()) {
			reg_al+=0x06;
			flags.af=true;
		} else {
			flags.af=false;
		}
		flags.cf=get_CF();
		if ((reg_al > 0x9F) || flags.cf) {
			reg_al+=0x60;
			flags.cf=true;
		}
		flags.sf=(reg_al>>7)>0;
		flags.zf=(reg_al==0);
		flags.type=t_UNKNOWN;
		break;
	case 0x28:												/* SUB Eb,Gb */
		RMEbGb(SUBB);break;
	case 0x29:												/* SUB Ew,Gw */
		RMEwGw(SUBW);break;
	case 0x2a:												/* SUB Gb,Eb */
		RMGbEb(SUBB);break;
	case 0x2b:												/* SUB Gw,Ew */
		RMGwEw(SUBW);break;
	case 0x2c:												/* SUB AL,Ib */
		ALIb(SUBB);break;
	case 0x2d:												/* SUB AX,Iw */
		AXIw(SUBW);break;
	case 0x2e:												/* SEG CS: */
		SegPrefix(cs);break;
	case 0x2f:												/* DAS */
		if (((reg_al & 0x0f) > 9) || get_AF()) {
			reg_al-=6;
			flags.af=true;
		} else {
			flags.af=false;
		}
		if ((reg_al>0x9f) || get_CF()) {
			reg_al-=0x60;
			flags.cf=true;
		} else {
			flags.cf=false;
		}
		flags.type=t_UNKNOWN;
		break;  
	case 0x30:												/* XOR Eb,Gb */
		RMEbGb(XORB);break;
	case 0x31:												/* XOR Ew,Gw */
		RMEwGw(XORW);break;	
	case 0x32:												/* XOR Gb,Eb */
		RMGbEb(XORB);break;
	case 0x33:												/* XOR Gw,Ew */
		RMGwEw(XORW);break;
	case 0x34:												/* XOR AL,Ib */
		ALIb(XORB);break;
	case 0x35:												/* XOR AX,Iw */
		AXIw(XORW);break;
	case 0x36:												/* SEG SS: */
		SegPrefix(ss);break;
	case 0x37:												/* AAA */
		if (get_AF() || ((reg_al & 0xf) > 9))
		{
			reg_al += 6;
			reg_ah += 1;
			flags.af=true;
			flags.cf=true;
		} else {
			flags.af=false;
			flags.cf=false;
		}
		reg_al &= 0x0F;
		flags.type=t_UNKNOWN;
		break;  
	case 0x38:												/* CMP Eb,Gb */
		RMEbGb(CMPB);break;
	case 0x39:												/* CMP Ew,Gw */
		RMEwGw(CMPW);break;
	case 0x3a:												/* CMP Gb,Eb */
		RMGbEb(CMPB);break;
	case 0x3b:												/* CMP Gw,Ew */
		RMGwEw(CMPW);break;
	case 0x3c:												/* CMP AL,Ib */
		ALIb(CMPB);break;
	case 0x3d:												/* CMP AX,Iw */
		AXIw(CMPW);break;
	case 0x3e:												/* SEG DS: */
		SegPrefix(ds);break;
	case 0x3f:												/* AAS */
		if (((reg_al & 0x0f)>9) || get_AF()) {
			reg_al=(reg_al-6) & 0xF;
			reg_ah--;
			flags.af=flags.cf=true;
		} else {
			flags.af=flags.cf=false;
		}
		flags.type=t_UNKNOWN;
		break;
	case 0x40:												/* INC AX */
		INCW(reg_ax,LoadRw,SaveRw);break;
	case 0x41:												/* INC CX */
		INCW(reg_cx,LoadRw,SaveRw);break;
	case 0x42:												/* INC DX */
		INCW(reg_dx,LoadRw,SaveRw);break;
	case 0x43:												/* INC BX */
		INCW(reg_bx,LoadRw,SaveRw);break;
	case 0x44:												/* INC SP */
		INCW(reg_sp,LoadRw,SaveRw);break;
	case 0x45:												/* INC BP */
		INCW(reg_bp,LoadRw,SaveRw);break;
	case 0x46:												/* INC SI */
		INCW(reg_si,LoadRw,SaveRw);break;
	case 0x47:												/* INC DI */
		INCW(reg_di,LoadRw,SaveRw);break;
	case 0x48:												/* DEC AX */
		DECW(reg_ax,LoadRw,SaveRw);break;
	case 0x49:												/* DEC CX */
  	DECW(reg_cx,LoadRw,SaveRw);break;
		case 0x4a:												/* DEC DX */
			DECW(reg_dx,LoadRw,SaveRw);break;
		case 0x4b:												/* DEC BX */
			DECW(reg_bx,LoadRw,SaveRw);break;
		case 0x4c:												/* DEC SP */
			DECW(reg_sp,LoadRw,SaveRw);break;
		case 0x4d:												/* DEC BP */
			DECW(reg_bp,LoadRw,SaveRw);break;
		case 0x4e:												/* DEC SI */
			DECW(reg_si,LoadRw,SaveRw);break;
		case 0x4f:												/* DEC DI */
			DECW(reg_di,LoadRw,SaveRw);break;
		case 0x50:												/* PUSH AX */
			Push_16(reg_ax);break;
		case 0x51:												/* PUSH CX */
			Push_16(reg_cx);break;
		case 0x52:												/* PUSH DX */
			Push_16(reg_dx);break;
		case 0x53:												/* PUSH BX */
			Push_16(reg_bx);break;
		case 0x54:												/* PUSH SP */
//TODO Check if this is correct i think it's SP+2 or something
			Push_16(reg_sp);break;
		case 0x55:												/* PUSH BP */
			Push_16(reg_bp);break;
		case 0x56:												/* PUSH SI */
			Push_16(reg_si);break;
		case 0x57:												/* PUSH DI */
			Push_16(reg_di);break;
		case 0x58:												/* POP AX */
			reg_ax=Pop_16();break;
		case 0x59:												/* POP CX */
			reg_cx=Pop_16();break;
		case 0x5a:												/* POP DX */
			reg_dx=Pop_16();break;
		case 0x5b:												/* POP BX */
			reg_bx=Pop_16();break;
		case 0x5c:												/* POP SP */
			reg_sp=Pop_16();break;
		case 0x5d:												/* POP BP */
			reg_bp=Pop_16();break;
		case 0x5e:												/* POP SI */
			reg_si=Pop_16();break;
		case 0x5f:												/* POP DI */
			reg_di=Pop_16();break;
		case 0x60:												/* PUSHA */
			Push_16(reg_ax);Push_16(reg_cx);Push_16(reg_dx);Push_16(reg_bx);
			Push_16(reg_sp);Push_16(reg_bp);Push_16(reg_si);Push_16(reg_di);
			break;
		case 0x61:												/* POPA */
			reg_di=Pop_16();reg_si=Pop_16();reg_bp=Pop_16();Pop_16();//Don't save SP
			reg_bx=Pop_16();reg_dx=Pop_16();reg_cx=Pop_16();reg_ax=Pop_16();
			break;
		case 0x62:												/* BOUND */
			{
				Bit16s bound_min, bound_max;
				GetRMrw;GetEAa;
				bound_min=LoadMw(eaa);
				bound_max=LoadMw(eaa+2);
				if ( (*rmrw < bound_min) || (*rmrw > bound_max) ) {
					INTERRUPT(5);
				}
			}
			break;
		case 0x63:												/* ARPL */
			NOTDONE;break;
#ifdef CPU_386
		case 0x64:												/* SEG FS: */
			SegPrefix(fs);break;
		case 0x65:												/* SEG GS: */
			SegPrefix(gs);break;
		case 0x66:												/* Operand Size Prefix */
			#include "prefix_66.h"
			break;
		case 0x67:												/* Address Size Prefix */
			NOTDONE;
			break;
#endif
		case 0x68:												/* PUSH Iw */
			Push_16(Fetchw());break;
		case 0x69:												/* IMUL Gw,Ew,Iw */
			{
				GetRMrw;
				Bit32s res;
				if (rm >= 0xc0 ) {GetEArw;res=(Bit32s)(*earws) * (Bit32s)Fetchws();}
				else {GetEAa;res=(Bit32s)LoadMws(eaa) * (Bit32s)Fetchws();}
				*rmrw=res & 0xFFFF;
				flags.type=t_MUL;
				if ((res> -32768)  && (res<32767)) {flags.cf=false;flags.of=false;}
				else {flags.cf=true;flags.of=true;}
				break;
			};	  
		case 0x6a:												/* PUSH Ib */
			Push_16(Fetchbs());break;
		case 0x6b:												/* IMUL Gw,Ew,Ib */
			{
				GetRMrw;Bit32s res;
				if (rm >= 0xc0 ) {GetEArw;res=(Bit32s)(*earws) * (Bit32s)Fetchbs();}
				else {GetEAa;res=(Bit32s)LoadMws(eaa) * (Bit32s)Fetchbs();}
				*rmrw=res & 0xFFFF;
				flags.type=t_MUL;
				if ((res> -32768)  && (res<32767)) {flags.cf=false;flags.of=false;}
				else {flags.cf=true;flags.of=true;}
				break;
			}	  
		case 0x6c:												/* INSB */
			{
				stringDI;
				SaveMb(to,IO_Read(reg_dx));
				if (flags.df) reg_di--; else reg_di++;
				break;
			}
		case 0x6d:												/* INSW */
			{ 
				stringDI;
				SaveMb(to,IO_Read(reg_dx));
				SaveMb((to+1),IO_Read(reg_dx+1));
				if (flags.df) reg_di-=2; else reg_di+=2;
				break;
			}
		case 0x6e:												/* OUTSB */
			{
				stringSI;
				IO_Write(reg_dx,LoadMb(from));
				if (flags.df) reg_si--; else reg_si++;
				break;
			}
		case 0x6f:												/* OUTSW */
			{
				stringSI;
				IO_Write(reg_dx,LoadMb(from));
				IO_Write(reg_dx+1,LoadMb(from+1));
				if (flags.df) reg_si-=2; else reg_si+=2;
				break;
			}
		case 0x70:												/* JO */
			JumpSIb(get_OF());break;
		case 0x71:												/* JNO */
			JumpSIb(!get_OF());break;
		case 0x72:												/* JB */
			JumpSIb(get_CF());break;
		case 0x73:												/* JNB */
			JumpSIb(!get_CF());break;
		case 0x74:												/* JZ */
   			JumpSIb(get_ZF());break;
		case 0x75:												/* JNZ */
			JumpSIb(!get_ZF());	break;
		case 0x76:												/* JBE */
			JumpSIb(get_CF() || get_ZF());break;
		case 0x77:												/* JNBE */
			JumpSIb(!get_CF() && !get_ZF());break;
		case 0x78:												/* JS */
			JumpSIb(get_SF());break;
		case 0x79:												/* JNS */
			JumpSIb(!get_SF());break;
		case 0x7a:												/* JP */
			JumpSIb(get_PF());break;
		case 0x7b:												/* JNP */
			JumpSIb(!get_PF());break;
		case 0x7c:												/* JL */
			JumpSIb(get_SF() != get_OF());break;
		case 0x7d:												/* JNL */
			JumpSIb(get_SF() == get_OF());break;
		case 0x7e:												/* JLE */
			JumpSIb(get_ZF() || (get_SF() != get_OF()));break;
		case 0x7f:												/* JNLE */
			JumpSIb((get_SF() == get_OF()) && !get_ZF());break;
		case 0x80:												/* Grpl Eb,Ib */
		case 0x82:												/* Grpl Eb,Ib Mirror instruction*/
			{
				GetRM;
				if (rm>= 0xc0) {
					GetEArb;Bit8u ib=Fetchb();
					switch (rm & 0x38) {
					case 0x00:ADDB(*earb,ib,LoadRb,SaveRb);break;
					case 0x08: ORB(*earb,ib,LoadRb,SaveRb);break;
					case 0x10:ADCB(*earb,ib,LoadRb,SaveRb);break;
					case 0x18:SBBB(*earb,ib,LoadRb,SaveRb);break;
					case 0x20:ANDB(*earb,ib,LoadRb,SaveRb);break;
					case 0x28:SUBB(*earb,ib,LoadRb,SaveRb);break;
					case 0x30:XORB(*earb,ib,LoadRb,SaveRb);break;
					case 0x38:CMPB(*earb,ib,LoadRb,SaveRb);break;
					}
				} else {
					GetEAa;Bit8u ib=Fetchb();
					switch (rm & 0x38) {
					case 0x00:ADDB(eaa,ib,LoadMb,SaveMb);break;
					case 0x08: ORB(eaa,ib,LoadMb,SaveMb);break;
					case 0x10:ADCB(eaa,ib,LoadMb,SaveMb);break;
					case 0x18:SBBB(eaa,ib,LoadMb,SaveMb);break;
					case 0x20:ANDB(eaa,ib,LoadMb,SaveMb);break;
					case 0x28:SUBB(eaa,ib,LoadMb,SaveMb);break;
					case 0x30:XORB(eaa,ib,LoadMb,SaveMb);break;
					case 0x38:CMPB(eaa,ib,LoadMb,SaveMb);break;
					}
				}
				break;
			}
		case 0x81:												/* Grpl Ew,Iw */
			{
				GetRM;
				if (rm>= 0xc0) {
					GetEArw;Bit16u iw=Fetchw();
					switch (rm & 0x38) {
					case 0x00:ADDW(*earw,iw,LoadRw,SaveRw);break;
					case 0x08: ORW(*earw,iw,LoadRw,SaveRw);break;
					case 0x10:ADCW(*earw,iw,LoadRw,SaveRw);break;
					case 0x18:SBBW(*earw,iw,LoadRw,SaveRw);break;
					case 0x20:ANDW(*earw,iw,LoadRw,SaveRw);break;
					case 0x28:SUBW(*earw,iw,LoadRw,SaveRw);break;
					case 0x30:XORW(*earw,iw,LoadRw,SaveRw);break;
					case 0x38:CMPW(*earw,iw,LoadRw,SaveRw);break;
					}
				} else {
					GetEAa;Bit16u iw=Fetchw();
					switch (rm & 0x38) {
					case 0x00:ADDW(eaa,iw,LoadMw,SaveMw);break;
					case 0x08: ORW(eaa,iw,LoadMw,SaveMw);break;
					case 0x10:ADCW(eaa,iw,LoadMw,SaveMw);break;
					case 0x18:SBBW(eaa,iw,LoadMw,SaveMw);break;
					case 0x20:ANDW(eaa,iw,LoadMw,SaveMw);break;
					case 0x28:SUBW(eaa,iw,LoadMw,SaveMw);break;
					case 0x30:XORW(eaa,iw,LoadMw,SaveMw);break;
					case 0x38:CMPW(eaa,iw,LoadMw,SaveMw);break;
					}
				}
				break;
			}
		case 0x83:												/* Grpl Ew,Ix */
			{
				GetRM;
				if (rm>= 0xc0) {
					GetEArw;Bit16u iw=(Bit16s)Fetchbs();
					switch (rm & 0x38) {
					case 0x00:ADDW(*earw,iw,LoadRw,SaveRw);break;
					case 0x08: ORW(*earw,iw,LoadRw,SaveRw);break;
					case 0x10:ADCW(*earw,iw,LoadRw,SaveRw);break;
					case 0x18:SBBW(*earw,iw,LoadRw,SaveRw);break;
					case 0x20:ANDW(*earw,iw,LoadRw,SaveRw);break;
					case 0x28:SUBW(*earw,iw,LoadRw,SaveRw);break;
					case 0x30:XORW(*earw,iw,LoadRw,SaveRw);break;
					case 0x38:CMPW(*earw,iw,LoadRw,SaveRw);break;
					}
				} else {
					GetEAa;Bit16u iw=(Bit16s)Fetchbs();
					switch (rm & 0x38) {
					case 0x00:ADDW(eaa,iw,LoadMw,SaveMw);break;
					case 0x08: ORW(eaa,iw,LoadMw,SaveMw);break;
					case 0x10:ADCW(eaa,iw,LoadMw,SaveMw);break;
					case 0x18:SBBW(eaa,iw,LoadMw,SaveMw);break;
					case 0x20:ANDW(eaa,iw,LoadMw,SaveMw);break;
					case 0x28:SUBW(eaa,iw,LoadMw,SaveMw);break;
					case 0x30:XORW(eaa,iw,LoadMw,SaveMw);break;
					case 0x38:CMPW(eaa,iw,LoadMw,SaveMw);break;
					}
				}
				break;
			}
		case 0x84:												/* TEST Eb,Gb */
			RMEbGb(TESTB);break;
		case 0x85:												/* TEST Ew,Gw */
			RMEwGw(TESTW);break;
		case 0x86:												/* XCHG Eb,Gb */
			{	
				GetRMrb;Bit8u oldrmrb=*rmrb;
				if (rm >= 0xc0 ) {GetEArb;*rmrb=*earb;*earb=oldrmrb;}
				else {GetEAa;*rmrb=LoadMb(eaa);SaveMb(eaa,oldrmrb);}
				break;
			}
		case 0x87:												/* XCHG Ew,Gw */
			{	
				GetRMrw;Bit16u oldrmrw=*rmrw;
				if (rm >= 0xc0 ) {GetEArw;*rmrw=*earw;*earw=oldrmrw;}
				else {GetEAa;*rmrw=LoadMw(eaa);SaveMw(eaa,oldrmrw);}
				break;
			}
		case 0x88:												/* MOV Eb,Gb */
			{	
				GetRMrb;
				if (rm >= 0xc0 ) {GetEArb;*earb=*rmrb;}
				else {GetEAa;SaveMb(eaa,*rmrb);}
				break;
			}
		case 0x89:												/* MOV Ew,Gw */
			{	
				GetRMrw;
				if (rm >= 0xc0 ) {GetEArw;*earw=*rmrw;}
				else {GetEAa;SaveMw(eaa,*rmrw);}
				break;
			}
		case 0x8a:												/* MOV Gb,Eb */
			{	
				GetRMrb;
				if (rm >= 0xc0 ) {GetEArb;*rmrb=*earb;}
				else {GetEAa;*rmrb=LoadMb(eaa);}
				break;
			}
		case 0x8b:												/* MOV Gw,Ew */
			{	
				GetRMrw;
				if (rm >= 0xc0 ) {GetEArw;*rmrw=*earw;}
				else {GetEAa;*rmrw=LoadMw(eaa);}
				break;
			}
		case 0x8c:												/* Mov Ew,Sw */
			{
				GetRM;Bit16u val;
				switch (rm & 0x38) {
				case 0x00:					/* MOV Ew,ES */
					val=Segs[es].value;break;
				case 0x08:					/* MOV Ew,CS */
					val=Segs[cs].value;break;
				case 0x10:					/* MOV Ew,SS */
					val=Segs[ss].value;break;
				case 0x18:					/* MOV Ew,DS */
					val=Segs[ds].value;break;
				case 0x20:					/* MOV Ew,FS */
					val=Segs[fs].value;break;
				case 0x28:					/* MOV Ew,GS */
					val=Segs[gs].value;break;
				default:
				        val=0;
					E_Exit("CPU:8c:Illegal RM Byte");
				}
				if (rm >= 0xc0 ) {GetEArw;*earw=val;}
				else {GetEAa;SaveMw(eaa,val);}
				break;
			}
		case 0x8d:												/* LEA */
			{
				GetRMrw;		
				switch (rm & 0xC7) {
				case 0x00:*rmrw=reg_bx+reg_si;break;
				case 0x01:*rmrw=reg_bx+reg_di;break;	
				case 0x02:*rmrw=reg_bp+reg_si;break;
				case 0x03:*rmrw=reg_bp+reg_di;break;
				case 0x04:*rmrw=reg_si;break;
				case 0x05:*rmrw=reg_di;break;
				case 0x06:*rmrw=Fetchw();break;
				case 0x07:*rmrw=reg_bx;break;
				case 0x40:*rmrw=reg_bx+reg_si+Fetchbs();break;
				case 0x41:*rmrw=reg_bx+reg_di+Fetchbs();break;
				case 0x42:*rmrw=reg_bp+reg_si+Fetchbs();break;
				case 0x43:*rmrw=reg_bp+reg_di+Fetchbs();break;
				case 0x44:*rmrw=reg_si+Fetchbs();break;
				case 0x45:*rmrw=reg_di+Fetchbs();break;
				case 0x46:*rmrw=reg_bp+Fetchbs();break;
				case 0x47:*rmrw=reg_bx+Fetchbs();break;
				case 0x80:*rmrw=reg_bx+reg_si+Fetchw();break;
				case 0x81:*rmrw=reg_bx+reg_di+Fetchw();break;
				case 0x82:*rmrw=reg_bp+reg_si+Fetchw();break;
				case 0x83:*rmrw=reg_bp+reg_di+Fetchw();break;
				case 0x84:*rmrw=reg_si+Fetchw();break;
				case 0x85:*rmrw=reg_di+Fetchw();break;
				case 0x86:*rmrw=reg_bp+Fetchw();break;
				case 0x87:*rmrw=reg_bx+Fetchw();break;	
				default:
					E_Exit("CPU:8d:Illegal LEA RM Byte");
				}
				break;
			}
		case 0x8e:												/* MOV Sw,Ew */
			{
				GetRM;Bit16u val;
				if (rm >= 0xc0 ) {GetEArw;val=*earw;}
				else {GetEAa;val=LoadMw(eaa);}
				switch (rm & 0x38) {
				case 0x00:					/* MOV ES,Ew */
					SetSegment_16(es,val);break;
				case 0x08:					/* MOV CS,Ew Illegal*/
					E_Exit("CPU:Illegal MOV CS Call");
					break;
				case 0x10:					/* MOV SS,Ew */
					SetSegment_16(ss,val);break;
				case 0x18:					/* MOV DS,Ew */
					SetSegment_16(ds,val);break;
				case 0x20:					/* MOV FS,Ew */
					SetSegment_16(fs,val);break;
				case 0x28:					/* MOV GS,Ew */
					SetSegment_16(gs,val);break;
				default:
					E_Exit("CPU:8e:Illegal RM Byte");
				}
				break;
			}							
		case 0x8f:												/* POP Ew */
			{
				GetRM;
				if (rm >= 0xc0 ) {GetEArw;*earw=Pop_16();}
				else {GetEAa;SaveMw(eaa,Pop_16());}
				break;
			}
		case 0x90:												/* NOP */
			break;
		case 0x91:												/* XCHG CX,AX */
			{ Bit16u temp=reg_ax;reg_ax=reg_cx;reg_cx=temp; }
			break;
		case 0x92:												/* XCHG DX,AX */
			{ Bit16u temp=reg_ax;reg_ax=reg_dx;reg_dx=temp; }
			break;
		case 0x93:												/* XCHG BX,AX */
			{ Bit16u temp=reg_ax;reg_ax=reg_bx;reg_bx=temp; }
			break;
		case 0x94:												/* XCHG SP,AX */
			{ Bit16u temp=reg_ax;reg_ax=reg_sp;reg_sp=temp; }
			break;
		case 0x95:												/* XCHG BP,AX */
			{ Bit16u temp=reg_ax;reg_ax=reg_bp;reg_bp=temp; }
			break;
		case 0x96:												/* XCHG SI,AX */
			{ Bit16u temp=reg_ax;reg_ax=reg_si;reg_si=temp; }
			break;
		case 0x97:												/* XCHG DI,AX */
			{ Bit16u temp=reg_ax;reg_ax=reg_di;reg_di=temp; }
			break;
		case 0x98:												/* CBW */
			reg_ax=(Bit8s)reg_al;break;
		case 0x99:												/* CWD */
			if (reg_ax & 0x8000) reg_dx=0xffff;
			else reg_dx=0;
			break;
		case 0x9a:												/* CALL Ap */
			{ 
				Bit16u newip=Fetchw();Bit16u newcs=Fetchw();
				Push_16(Segs[cs].value);Push_16(GETIP);
				SetSegment_16(cs,newcs);SETIP(newip);
				break;
			}
		case 0x9b:												/* WAIT */
			break; /* No waiting here */
		case 0x9c:												/* PUSHF */
			{
				Bit16u pflags=
					(get_CF() << 0)   | (get_PF() << 2) | (get_AF() << 4) | 
					(get_ZF() << 6)   | (get_SF() << 7) | (flags.tf << 8) |
					(flags.intf << 9) |(flags.df << 10) | (get_OF() << 11) | 
					(flags.io << 12) | (flags.nt <<14);
				Push_16(pflags);
				break;
			}
		case 0x9d:												/* POPF */
			{
				Bit16u bits=Pop_16();
				Save_Flagsw(bits);
				break;
			}
		case 0x9e:												/* SAHF */
			flags.of	=get_OF();
			flags.type=t_UNKNOWN;
			flags.cf	=(reg_ah & 0x001)!=0;flags.pf	=(reg_ah & 0x004)!=0;
			flags.af	=(reg_ah & 0x010)!=0;flags.zf	=(reg_ah & 0x040)!=0;
			flags.sf	=(reg_ah & 0x080)!=0;
			break;
		case 0x9f:												/* LAHF */
			{
				reg_ah=(get_CF() << 0) | (get_PF() << 2) | (get_AF() << 4) | 
					    (get_ZF() << 6) | (get_SF() << 7);
				break;
			}
		case 0xa0:												/* MOV AL,Ob */
			if (segprefix_on)	{
				reg_al=LoadMb(segprefix_base+Fetchw());
				SegPrefixReset;
			} else {
				reg_al=LoadMb(SegBase(ds)+Fetchw());
			}
			break;
		case 0xa1:												/* MOV AX,Ow */
			if (segprefix_on)	{
				reg_ax=LoadMw(segprefix_base+Fetchw());
				SegPrefixReset;
			} else {
				reg_ax=LoadMw(SegBase(ds)+Fetchw());
			}
			break;
		case 0xa2:												/* MOV Ob,AL */
			if (segprefix_on)	{
				SaveMb((segprefix_base+Fetchw()),reg_al);
				SegPrefixReset;
			} else {
				SaveMb((SegBase(ds)+Fetchw()),reg_al);
			}
			break;
		case 0xa3:												/* MOV Ow,AX */
			if (segprefix_on)	{
				SaveMw((segprefix_base+Fetchw()),reg_ax);
				SegPrefixReset;
			} else {
				SaveMw((SegBase(ds)+Fetchw()),reg_ax);
			}
			break;
		case 0xa4:												/* MOVSB */
			{	
				stringSI;stringDI;
				SaveMb(to,LoadMb(from));;
				if (flags.df) { reg_si--;reg_di--; }
				else {reg_si++;reg_di++;}
				break;
			}
		case 0xa5:												/* MOVSW */
			{	
				stringSI;stringDI;
				SaveMw(to,LoadMw(from));
				if (flags.df) { reg_si-=2;reg_di-=2; }
				else {reg_si+=2;reg_di+=2;}
				break;
			}
		case 0xa6:												/* CMPSB */
			{	
				stringSI;stringDI;
				CMPB(from,LoadMb(to),LoadMb,0);
				if (flags.df) { reg_si--;reg_di--; }
				else {reg_si++;reg_di++;}
				break;
			}
		case 0xa7:												/* CMPSW */
			{	
				stringSI;stringDI;
				CMPW(from,LoadMw(to),LoadMw,0);
				if (flags.df) { reg_si-=2;reg_di-=2; }
				else {reg_si+=2;reg_di+=2;}
				break;
			}
		case 0xa8:												/* TEST AL,Ib */
			ALIb(TESTB);break;
		case 0xa9:												/* TEST AX,Iw */
			AXIw(TESTW);break;
		case 0xaa:												/* STOSB */
			{
				stringDI;
				SaveMb(to,reg_al);
				if (flags.df) { reg_di--; }
				else {reg_di++;}
				break;
			}
		case 0xab:												/* STOSW */
			{
				stringDI;
				SaveMw(to,reg_ax);
				if (flags.df) { reg_di-=2; }
				else {reg_di+=2;}
				break;
			}
		case 0xac:												/* LODSB */
			{
				stringSI;
				reg_al=LoadMb(from);
				if (flags.df) { reg_si--; }
				else {reg_si++;}
				break;
			}
		case 0xad:												/* LODSW */
			{
				stringSI;
				reg_ax=LoadMw(from);
				if (flags.df) { reg_si-=2;}
				else {reg_si+=2;}
				break;
			}
		case 0xae:												/* SCASB */
			{
				stringDI;
				CMPB(reg_al,LoadMb(to),LoadRb,0);
				if (flags.df) { reg_di--; }
				else {reg_di++;}
				break;
			}
		case 0xaf:												/* SCASW */
			{
				stringDI;
				CMPW(reg_ax,LoadMw(to),LoadRw,0);
				if (flags.df) { reg_di-=2; }
				else {reg_di+=2;}
				break;
			}
		case 0xb0:												/* MOV AL,Ib */
			reg_al=Fetchb();break;
		case 0xb1:												/* MOV CL,Ib */
			reg_cl=Fetchb();break;
		case 0xb2:												/* MOV DL,Ib */
			reg_dl=Fetchb();break;
		case 0xb3:												/* MOV BL,Ib */
			reg_bl=Fetchb();break;
		case 0xb4:												/* MOV AH,Ib */
			reg_ah=Fetchb();break;
		case 0xb5:												/* MOV CH,Ib */
			reg_ch=Fetchb();break;
		case 0xb6:												/* MOV DH,Ib */
			reg_dh=Fetchb();break;
		case 0xb7:												/* MOV BH,Ib */
			reg_bh=Fetchb();break;
		case 0xb8:												/* MOV AX,Iw */
			reg_ax=Fetchw();break;
		case 0xb9:												/* MOV CX,Iw */
			reg_cx=Fetchw();break;
		case 0xba:												/* MOV DX,Iw */
			reg_dx=Fetchw();break;
		case 0xbb:												/* MOV BX,Iw */
			reg_bx=Fetchw();break;
		case 0xbc:												/* MOV SP,Iw */
			reg_sp=Fetchw();break;
		case 0xbd:												/* MOV BP.Iw */
			reg_bp=Fetchw();break;
		case 0xbe:												/* MOV SI,Iw */
			reg_si=Fetchw();break;
		case 0xbf:												/* MOV DI,Iw */
			reg_di=Fetchw();break;
		case 0xc0:												/* GRP2 Eb,Ib */
			GRP2B(Fetchb());break;
		case 0xc1:												/* GRP2 Ew,Ib */
			GRP2W(Fetchb());break;
		case 0xc2:												/* RETN Iw */
			{ 
				Bit16u addsp=Fetchw();
				SETIP(Pop_16());reg_sp+=addsp;
				break;  
			}
		case 0xc3:												/* RETN */
			SETIP(Pop_16());
			break;
		case 0xc4:												/* LES */
			{	
				GetRMrw;GetEAa;
				*rmrw=LoadMw(eaa);SetSegment_16(es,LoadMw(eaa+2));
				break;
			}
		case 0xc5:												/* LDS */
			{	
				GetRMrw;GetEAa;
				*rmrw=LoadMw(eaa);SetSegment_16(ds,LoadMw(eaa+2));
				break;
			}
		case 0xc6:												/* MOV Eb,Ib */
			{
				GetRM;
				if (rm>0xc0) {GetEArb;*earb=Fetchb();}
				else {GetEAa;SaveMb(eaa,Fetchb());}
				break;
			}
		case 0xc7:												/* MOV EW,Iw */
			{
				GetRM;
				if (rm>0xc0) {GetEArw;*earw=Fetchw();}
				else {GetEAa;SaveMw(eaa,Fetchw());}
				break;
			}
		case 0xc8:												/* ENTER Iw,Ib */
			{
				Bit16u bytes=Fetchw();Bit8u level=Fetchb();
				Push_16(reg_bp);reg_bp=reg_sp;reg_sp-=bytes;
				EAPoint reader=SegBase(ss)+reg_bp;
				for (Bit8u i=1;i<level;i++) {Push_16(LoadMw(reader));reader-=2;}
				if (level) Push_16(reg_bp);
				break;
			}
		case 0xc9:												/* LEAVE */
			reg_sp=reg_bp;reg_bp=Pop_16();break;
		case 0xca:												/* RETF Iw */
			{ 
				Bit16u addsp=Fetchw();
				Bit16u newip=Pop_16();Bit16u newcs=Pop_16();
				reg_sp+=addsp;SetSegment_16(cs,newcs);SETIP(newip);
				break;
			}
		case 0xcb:												/* RETF */			
			{
				Bit16u newip=Pop_16();Bit16u newcs=Pop_16();
				SetSegment_16(cs,newcs);SETIP(newip);
			}
			break;
		case 0xcc:												/* INT3 */
			INTERRUPT(3);
			break;
		case 0xcd:												/* INT Ib */	
			{
				Bit8u num=Fetchb();
				INTERRUPT(num);				
			}
			break;
		case 0xce:												/* INTO */
			if (get_OF()) INTERRUPT(4);
			break;
		case 0xcf:												/* IRET */
			{
				Bit16u newip=Pop_16();Bit16u newcs=Pop_16();
				SetSegment_16(cs,newcs);SETIP(newip);
				Bit16u pflags=Pop_16();Save_Flagsw(pflags);
				break;
			}
		case 0xd0:												/* GRP2 Eb,1 */
			GRP2B(1);break;
		case 0xd1:												/* GRP2 Ew,1 */
			GRP2W(1);break;
		case 0xd2:												/* GRP2 Eb,CL */
			GRP2B(reg_cl);break;
		case 0xd3:												/* GRP2 Ew,CL */
			GRP2W(reg_cl);break;
		case 0xd4:												/* AAM Ib */
			{
				Bit8u ib=Fetchb();

				reg_ah=reg_al / ib;
				reg_al=reg_al % ib;
				flags.type=t_UNKNOWN;
				flags.sf=(reg_ah & 0x80) > 0;
				flags.zf=(reg_ax == 0);
				//TODO PF
				flags.pf=0;
			}
			break;
		case 0xd5:												/* AAD Ib */
			reg_al=reg_ah*Fetchb()+reg_al;
			reg_ah=0;
			flags.cf=(reg_al>=0x80);
			flags.zf=(reg_al==0);
			//TODO PF
			flags.type=t_UNKNOWN;
			break;
		case 0xd6:												/* Not in intel specs */
			NOTDONE;
			break;
		case 0xd7:												/* XLAT */
			if (segprefix_on) {
				reg_al=LoadMb(segprefix_base+(Bit16u)(reg_bx+reg_al));
				SegPrefixReset;
			} else {
				reg_al=LoadMb(SegBase(ds)+(Bit16u)(reg_bx+reg_al));
			}
			break;
#ifdef CPU_FPU
		case 0xd8:												/* FPU ESC 0 */
			 FPU_ESC(0);break;
		case 0xd9:												/* FPU ESC 1 */
			 FPU_ESC(1);break;
		case 0xda:												/* FPU ESC 2 */
			 FPU_ESC(2);break;
		case 0xdb:												/* FPU ESC 3 */
			 FPU_ESC(3);break;
		case 0xdc:												/* FPU ESC 4 */
			 FPU_ESC(4);break;
		case 0xdd:												/* FPU ESC 5 */
			 FPU_ESC(5);break;
		case 0xde:												/* FPU ESC 6 */
			 FPU_ESC(6);break;
		case 0xdf:												/* FPU ESC 7 */
			 FPU_ESC(7);break;
#else 
		case 0xd8:												/* FPU ESC 0 */
		case 0xd9:												/* FPU ESC 1 */
		case 0xda:												/* FPU ESC 2 */
		case 0xdb:												/* FPU ESC 3 */
		case 0xdc:												/* FPU ESC 4 */
		case 0xdd:												/* FPU ESC 5 */
		case 0xde:												/* FPU ESC 6 */
		case 0xdf:												/* FPU ESC 7 */
			{
				Bit8u rm;
				if (rm<0xc0) GetEAa;
			}
			break;
#endif
		case 0xe0:												/* LOOPNZ */
			if ((--reg_cx) && !get_ZF()) ADDIPFAST(Fetchbs());
			else ADDIPFAST(1);
			break;
		case 0xe1:												/* LOOPZ */
			if ((--reg_cx) && get_ZF()) ADDIPFAST(Fetchbs());
			else ADDIPFAST(1);
			break;
		case 0xe2:												/* LOOP */
			if ((--reg_cx)) ADDIPFAST(Fetchbs());
			else ADDIPFAST(1);
			break;
		case 0xe3:												/* JCXZ */
			if (!reg_cx) ADDIPFAST(Fetchbs());
			else ADDIPFAST(1);
			break;
		case 0xe4:												/* IN AL,Ib */
			{ Bit16u port=Fetchb();reg_al=IO_Read(port);}
			break;
		case 0xe5:												/* IN AX,Ib */
			{ Bit16u port=Fetchb();reg_al=IO_Read(port);reg_ah=IO_Read(port+1);}
			break;
		case 0xe6:												/* OUT Ib,AL */
			{ Bit16u port=Fetchb();IO_Write(port,reg_al);}
			break;
		case 0xe7:												/* OUT Ib,AX */
			{ Bit16u port=Fetchb();IO_Write(port,reg_al);IO_Write(port+1,reg_ah);}
			break;
		case 0xe8:												/* CALL Jw */
			{ 
				Bit16s newip=Fetchws();
				Push_16(GETIP);
				ADDIP(newip);
				break;
			}
		case 0xe9:												/* JMP Jw */
			ADDIP(Fetchws());
			break;
		case 0xea:												/* JMP Ap */
			{ 
				Bit16u newip=Fetchw();
				Bit16u newcs=Fetchw();
				SetSegment_16(cs,newcs);
				SETIP(newip);
				break;
			}
		case 0xeb:												/* JMP Jb*/
			ADDIPFAST(Fetchbs());
			break;
		case 0xec:												/* IN AL,DX */
			reg_al=IO_Read(reg_dx);
			break;
		case 0xed:												/* IN AX,DX */
			reg_al=IO_Read(reg_dx);
			reg_ah=IO_Read(reg_dx+1);			
			break;
		case 0xee:												/* OUT DX,AL */
			IO_Write(reg_dx,reg_al); 
			break;
		case 0xef:												/* OUT DX,AX */
			IO_Write(reg_dx,reg_al);
			IO_Write(reg_dx+1,reg_ah);
			break;
		case 0xf0:												/* LOCK */
			LOG_ERROR("CPU:LOCK");
			break;
		case 0xf1:												/* Weird call undocumented */
			E_Exit("CPU:F1:Not Handled");
			break;
		case 0xf2:												/* REPNZ */
			repcheck=false;
			goto repstart;
		case 0xf3:												/* REPZ */
			repcheck=true;
			repstart:
			{	
				EAPoint to=SegBase(es);
				EAPoint from;
				if (segprefix_on) {
					from=(segprefix_base);
					SegPrefixReset;
				} else {
					from=SegBase(ds);
				}
				Bit16s direct;
				if (flags.df) direct=-1;
				else direct=1;
				reploop:
				Bit8u repcode=Fetchb();
				switch (repcode)	{
				case 0x26:			/* ES Prefix */
					from=SegBase(es);
					goto reploop;
				case 0x2e:			/* CS Prefix */
					from=SegBase(cs);
					goto reploop;
				case 0x36:			/* SS Prefix */
					from=SegBase(ss);
					goto reploop;
				case 0x3e:			/* DS Prefix */
					from=SegBase(ds);
					goto reploop;
#ifdef CPU_386
				case 0x66:
					Rep_66(direct,from,to);
					break;
#endif												
					
				case 0x6c:			/* REP INSB */
					{
						for (Bit32u temp=reg_cx;temp>0;temp--) {
								SaveMb(to,IO_Read(reg_dx));
								to+=direct;
						};
						reg_di+=Bit16s(reg_cx*direct);reg_cx=0;
						break;
					}
				case 0x6d:			/* REP INSW */
					{	
						for (Bit32u temp=reg_cx;temp>0;temp--) {
							SaveMb(to,IO_Read(reg_dx));
							SaveMb((to+1),IO_Read(reg_dx+1));
							to+=direct*2;
						}	
						reg_di+=Bit16s(reg_cx*direct*2);reg_cx=0;
						break;
					}
				case 0x6e:			/* REP OUTSB */
					for (;reg_cx>0;reg_cx--) {
						IO_Write(reg_dx,LoadMb(from+reg_si));
						reg_si+=direct;
					}	
					break;
				case 0x6f:			/* REP OUTSW */
					for (;reg_cx>0;reg_cx--) {
						IO_Write(reg_dx,LoadMb(from+reg_si));
						IO_Write(reg_dx+1,LoadMb(from+reg_si+1));
						reg_si+=direct*2;
					}	
					break;
				case 0xa4:			/* REP MOVSB */
					for (;reg_cx>0;reg_cx--) {
						SaveMb(to+reg_di,LoadMb(from+reg_si));
						reg_di+=direct;
						reg_si+=direct;
					}	
					break;
				case 0xa5:			/* REP MOVSW */
					for (;reg_cx>0;reg_cx--) {
						SaveMw(to+reg_di,LoadMw(from+reg_si));
						reg_di+=direct*2;
						reg_si+=direct*2;
					}	
					break;
				case 0xa6:			/* REP CMPSB */
					if (!reg_cx) break;
					for (;reg_cx>0;) {
						reg_cx--;
						if ((LoadMb(from+reg_si)==LoadMb(to+reg_di))!=repcheck) {
							reg_di+=direct;
							reg_si+=direct;
							break;
						}
						reg_di+=direct;
						reg_si+=direct;
					}	
					CMPB(from+(reg_si-direct),LoadMb(to+(reg_di-direct)),LoadMb,0);
					break;
				case 0xa7:			/* REP CMPSW */
					if (!reg_cx) break;
					for (;reg_cx>0;) {
						reg_cx--;
						if ((LoadMw(from+reg_si)==LoadMw(to+reg_di))!=repcheck) {
							reg_di+=direct*2;
							reg_si+=direct*2;
							break;
						}
						reg_di+=direct*2;
						reg_si+=direct*2;
					}	
					CMPW(from+(reg_si-direct*2),LoadMw(to+(reg_di-direct*2)),LoadMw,0);
					break;
				case 0xaa:			/* REP STOSB */
					for (;reg_cx>0;reg_cx--) {
						SaveMb(to+reg_di,reg_al);
						reg_di+=direct;
					}	
					break;
				case 0xab:			/* REP STOSW */
					for (;reg_cx>0;reg_cx--) {
						SaveMw(to+reg_di,reg_ax);
						reg_di+=direct*2;
					}	
					break;
				case 0xac:			/* REP LODSB */
					for (;reg_cx>0;reg_cx--) {
						reg_al=LoadMb(from+reg_si);
						reg_si+=direct;
					}
					break;
				case 0xad:			/* REP LODSW */
					for (;reg_cx>0;reg_cx--) {
						reg_ax=LoadMw(from+reg_si);
						reg_si+=direct*2;
					}
					break;
				case 0xae:			/* REP SCASB */
					if (!reg_cx) break;
					for (;reg_cx>0;) {
						reg_cx--;
						if ((reg_al==LoadMb(to+reg_di))!=repcheck) {
							reg_di+=direct;
							break;
						}
						reg_di+=direct;
					}	
					CMPB(reg_al,LoadMb(to+(reg_di-direct)),LoadRb,0);
					break;
				case 0xaf:			/* REP SCASW */
					if (!reg_cx) break;
					for (;reg_cx>0;) {
						reg_cx--;
						if ((reg_ax==LoadMw(to+reg_di))!=repcheck) {
							reg_di+=direct*2;
								break;
						}
						reg_di+=direct*2;
					}	
					CMPW(reg_ax,LoadMw(to+(reg_di-direct*2)),LoadRw,0);
					break;
				default:
					E_Exit("Illegal REP prefix %2X",repcode);
				}
				break;
			}	
		case 0xf4:												/* HLT */
			break;
		case 0xf5:												/* CMC */
			flags.cf=!get_CF();
			if (flags.type!=t_CF) flags.prev_type=flags.type;
			flags.type=t_CF;
			break;
		case 0xf6:												/* GRP3 Eb(,Ib) */
			{	
			GetRM;
			switch (rm & 0x38) {
			case 0x00:					/* TEST Eb,Ib */
			case 0x08:					/* TEST Eb,Ib Undocumented*/
				{
					if (rm >= 0xc0 ) {GetEArb;TESTB(*earb,Fetchb(),LoadRb,0)}
					else {GetEAa;TESTB(eaa,Fetchb(),LoadMb,0);}
					break;
				}
			case 0x10:					/* NOT Eb */
				{
					if (rm >= 0xc0 ) {GetEArb;*earb=~*earb;}
					else {GetEAa;SaveMb(eaa,~LoadMb(eaa));}
					break;
				}
			case 0x18:					/* NEG Eb */
				{
					flags.type=t_NEGb;
					if (rm >= 0xc0 ) {
						GetEArb;flags.var1.b=*earb;flags.result.b=0-flags.var1.b;
						*earb=flags.result.b;
					} else {
						GetEAa;flags.var1.b=LoadMb(eaa);flags.result.b=0-flags.var1.b;
 						SaveMb(eaa,flags.result.b);
					}
					break;
				}
			case 0x20:					/* MUL AL,Eb */
				{
					flags.type=t_MUL;
					if (rm >= 0xc0 ) {GetEArb;reg_ax=reg_al * (*earb);}
					else {GetEAa;reg_ax=reg_al * LoadMb(eaa);}
					flags.cf=flags.of=((reg_ax & 0xff00) !=0);
					break;
				}
			case 0x28:					/* IMUL AL,Eb */
				{
					flags.type=t_MUL;
					if (rm >= 0xc0 ) {GetEArb;reg_ax=(Bit8s)reg_al * (*earbs);}
					else {GetEAa;reg_ax=((Bit8s)reg_al) * LoadMbs(eaa);}
					flags.cf=flags.of=!((reg_ax & 0xff80)==0xff80 || (reg_ax & 0xff80)==0x0000);
					break;
				}
			case 0x30:					/* DIV Eb */
				{
//					flags.type=t_DIV;
					Bit8u val;
					if (rm >= 0xc0 ) {GetEArb;val=*earb;}
					else {GetEAa;val=LoadMb(eaa);}
					if (val==0)	{INTERRUPT(0);break;}
					Bit16u quotientu=reg_ax / val;
					reg_ah=(Bit8u)(reg_ax % val);
					reg_al=quotientu & 0xff;
					if (quotientu!=reg_al) 
						INTERRUPT(0);
					break;
				}
			case 0x38:					/* IDIV Eb */
				{
//					flags.type=t_DIV;
					Bit8s val;
					if (rm >= 0xc0 ) {GetEArb;val=*earbs;}
					else {GetEAa;val=LoadMbs(eaa);}
					if (val==0)	{INTERRUPT(0);break;}
					Bit16s quotients=((Bit16s)reg_ax) / val;
					reg_ah=(Bit8s)(((Bit16s)reg_ax) % val);
					reg_al=quotients & 0xff;
					if (quotients!=(Bit8s)reg_al) 
						INTERRUPT(0);
					break;
				}
			}
			break;}
		case 0xf7:												/* GRP3 Ew(,Iw) */
			{ GetRM;
			switch (rm & 0x38) {
			case 0x00:					/* TEST Ew,Iw */
			case 0x08:					/* TEST Ew,Iw Undocumented*/
				{
					if (rm >= 0xc0 ) {GetEArw;TESTW(*earw,Fetchw(),LoadRw,SaveRw);}
					else {GetEAa;TESTW(eaa,Fetchw(),LoadMw,SaveMw);}
					break;
				}
			case 0x10:					/* NOT Ew */
				{
					if (rm >= 0xc0 ) {GetEArw;*earw=~*earw;}
					else {GetEAa;SaveMw(eaa,~LoadMw(eaa));}
					break;
				}
			case 0x18:					/* NEG Ew */
				{
					flags.type=t_NEGw;
					if (rm >= 0xc0 ) {
						GetEArw;flags.var1.w=*earw;flags.result.w=0-flags.var1.w;
						*earw=flags.result.w;
					} else {
						GetEAa;flags.var1.w=LoadMw(eaa);flags.result.w=0-flags.var1.w;
 						SaveMw(eaa,flags.result.w);
					}
					break;
				}
			case 0x20:					/* MUL AX,Ew */
				{
					flags.type=t_MUL;Bit32u tempu;
					if (rm >= 0xc0 ) {GetEArw;tempu=reg_ax * (*earw);}
					else {GetEAa;tempu=reg_ax * LoadMw(eaa);}
					reg_ax=(Bit16u)(tempu & 0xffff);reg_dx=(Bit16u)(tempu >> 16);
					flags.cf=flags.of=(reg_dx !=0);
					break;
				}
			case 0x28:					/* IMUL AX,Ew */
				{
					flags.type=t_MUL;Bit32s temps;
					if (rm >= 0xc0 ) {GetEArw;temps=((Bit16s)reg_ax) * (*earws);}
					else {GetEAa;temps=((Bit16s)reg_ax) * LoadMws(eaa);}
					reg_ax=Bit16u(temps & 0xffff);reg_dx=(Bit16u)(temps >> 16);
					if ( (reg_dx==0xffff) && (reg_ax & 0x8000) ) {
						flags.cf=flags.of=false;
					} else if ( (reg_dx==0x0000) && (reg_ax<0x8000) ) {
						flags.cf=flags.of=false;
					} else {
						flags.cf=flags.of=true;
					}
					break;
				}
			case 0x30:					/* DIV Ew */
				{
//					flags.type=t_DIV;
					Bit16u val;
					if (rm >= 0xc0 ) {GetEArw;val=*earw;}
					else {GetEAa;val=LoadMw(eaa);}
					if (val==0)	{INTERRUPT(0);break;}
					Bit32u tempu=(reg_dx<<16)|reg_ax;
					Bit32u quotientu=tempu/val;
					reg_dx=(Bit16u)(tempu % val);
					reg_ax=(Bit16u)(quotientu & 0xffff);
					if (quotientu>0xffff) 
						INTERRUPT(0);
					break;
				}
			case 0x38:					/* IDIV Ew */
				{
//					flags.type=t_DIV;
					Bit16s val;
					if (rm >= 0xc0 ) {GetEArw;val=*earws;}
					else {GetEAa;val=LoadMws(eaa);}
					if (val==0)	{INTERRUPT(0);break;}
					Bit32s temps=(reg_dx<<16)|reg_ax;
					Bit32s quotients=temps/val;
					reg_dx=(Bit16s)(temps % val);
					reg_ax=(Bit16s)quotients;
					if (quotients!=(Bit16s)reg_ax) 
						INTERRUPT(0);
					break;
				}

			}
			break;
			}
		case 0xf8:												/* CLC */
			flags.cf=false;
			if (flags.type!=t_CF) flags.prev_type=flags.type;
			flags.type=t_CF;
			break;
		case 0xf9:												/* STC */
			flags.cf=true;
			if (flags.type!=t_CF) flags.prev_type=flags.type;
			flags.type=t_CF;
			break;
		case 0xfa:												/* CLI */
			flags.intf=false;
			break;
		case 0xfb:												/* STI */
			flags.intf=true;
			if (flags.intf && PIC_IRQCheck) {
				SAVEIP;	
				PIC_runIRQs();	
				LOADIP;
			};
			break;
		case 0xfc:												/* CLD */
			flags.df=false;
			break;
		case 0xfd:												/* STD */
			flags.df=true;
			break;
		case 0xfe:												/* GRP4 Eb */
			{
				GetRM;
				switch (rm & 0x38) {
				case 0x00:					/* INC Eb */
					flags.cf=get_CF();flags.type=t_INCb;
					if (rm >= 0xc0 ) {GetEArb;flags.result.b=*earb+=1;}
					else {GetEAa;flags.result.b=LoadMb(eaa)+1;SaveMb(eaa,flags.result.b);}
					break;		
				case 0x08:					/* DEC Eb */
					flags.cf=get_CF();flags.type=t_DECb;
					if (rm >= 0xc0 ) {GetEArb;flags.result.b=*earb-=1;}
					else {GetEAa;flags.result.b=LoadMb(eaa)-1;SaveMb(eaa,flags.result.b);}
					break;
				case 0x38:					/* CallBack */
					{
						Bit32u ret;
						Bit16u call=Fetchw();
						SAVEIP;
						if (call<CB_MAX) { 
							ret=CallBack_Handlers[call]();
						} else {  
							E_Exit("Too high CallBack Number %d called",call);				
						}
						switch (ret) {
						case CBRET_NONE:
							LOADIP;
						break;
						case CBRET_STOP:return ret;
						default:
							E_Exit("CPU:Callback %d returned illegal %d code",call,ret);
						};
						break;
					}
				default:
					E_Exit("Illegal GRP4 Call %d",(rm>>3) & 7);
					break;
				}
				break;
			}
		case 0xff:												/* GRP5 Ew */
			{
				GetRM;
				switch (rm & 0x38) {
				case 0x00:					/* INC Ew */
					flags.cf=get_CF();flags.type=t_INCw;
					if (rm >= 0xc0 ) {GetEArw;flags.result.w=*earw+=1;}
					else {GetEAa;flags.result.w=LoadMw(eaa)+1;SaveMw(eaa,flags.result.w);}
					break;		
				case 0x08:					/* DEC Ew */
					flags.cf=get_CF();flags.type=t_DECw;
					if (rm >= 0xc0 ) {GetEArw;flags.result.w=*earw-=1;}
					else {GetEAa;flags.result.w=LoadMw(eaa)-1;SaveMw(eaa,flags.result.w);}
					break;
				case 0x10:					/* CALL Ev */
					if (rm >= 0xc0 ) {GetEArw;Push_16(GETIP);SETIP(*earw);}
					else {GetEAa;Push_16(GETIP);SETIP(LoadMw(eaa));}
					break;
				case 0x18:					/* CALL Ep */
					{
						Push_16(Segs[cs].value);
						GetEAa;Push_16(GETIP);
						Bit16u newip=LoadMw(eaa);
						Bit16u newcs=LoadMw(eaa+2);
						SetSegment_16(cs,newcs);
						SETIP(newip);
					}
					break;
				case 0x20:					/* JMP Ev */	
					if (rm >= 0xc0 ) {GetEArw;SETIP(*earw);}
					else {GetEAa;SETIP(LoadMw(eaa));}
					break;
				case 0x28:					/* JMP Ep */	
					{
						GetEAa;
						Bit16u newip=LoadMw(eaa);
						Bit16u newcs=LoadMw(eaa+2);
						SetSegment_16(cs,newcs);
						SETIP(newip);
					}
					break;
				case 0x30:					/* PUSH Ev */
					if (rm >= 0xc0 ) {GetEArw;Push_16(*earw);}
					else {GetEAa;Push_16(LoadMw(eaa));}
					break;
				default:
					E_Exit("CPU:GRP5:Illegal Call %2X",rm & 0x38);
					break;
				}
				break;
		}
	}

