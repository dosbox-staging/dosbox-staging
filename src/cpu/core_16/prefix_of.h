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
			INTERRUPT(6);
			break;
			GetRM;
			switch (rm & 0x38) {
			case 0x00:
			default:
				E_Exit("CPU:GRP6:Illegal call %2X",(rm>>3) &3);
			}
		}
		break;

	case 0x01:												/* GRP 7 */
		{
			GetRM;
			switch (rm & 0x38) {
			case 0x20:										/* SMSW */
			/* Let's seriously fake this call */
				if (rm>0xc0) {GetEArw;*earw=0;}
				else {GetEAa;SaveMw(eaa,0);}
				break;
			default:
				E_Exit("CPU:GRP7:Illegal call %2X",(rm>>3) &3);
			}
		}
		break;
	/* 0x02 LAR Gw,Ew (286) */
	/* 0x03 LSL Gw,Ew (286) */
	/* 0x05 LOADALL (286 only?) */
	/* 0x06 CLTS (286) */
	/* 0x07 LOADALL (386 only?) */
	/* 0x08 INVD (486) */
	/* 0x02 WBINVD (486) */
	/* 0x10 UMOV Eb,Gb (386) */
	/* 0x11 UMOV Ew,Gw (386) */
	/* 0x12 UMOV Gb,Eb (386) */
	/* 0x13 UMOV Gw,Ew (386) */
	/* 0x20 MOV Rd,CRx (386) */
	/* 0x21 MOV Rd,DRx (386) */
	/* 0x22 MOV CRx,Rd (386) */
	/* 0x23 MOV DRx,Rd (386) */
	/* 0x24 MOV Rd,TRx (386) */
	/* 0x26 MOV TRx,Rd (386) */
	/* 0x30 WRMSR (P5) */
	/* 0x31 RDTSC (P5) */
	/* 0x32 RDMSR (P5) */
	/* 0x33 RDPMC (P6) */
	/* 0x40-4F CMOVcc Gw,Ew (P6) */
	/* 0x50 PAVEB Rq,Eq (CYRIX MMX) */
	/* 0x51 PADDSIW Rq,Eq (CYRIX MMX) */
	/* 0x52 PMAGW Rq,Eq (CYRIX MMX) */
	/* 0x54 PDISTIB Rq,Eq (CYRIX MMX) */
	/* 0x55 PSUBSIW Rq,Eq (CYRIX MMX) */
	/* 0x58 PMVZB Rq,Eq (CYRIX MMX) */
	/* 0x59 PMULHRW Rq,Eq (CYRIX MMX) */
	/* 0x5A PMVNZB Rq,Eq (CYRIX MMX) */
	/* 0x5B PMVLZB Rq,Eq (CYRIX MMX) */
	/* 0x5C PMVGEZB Rq,Eq (CYRIX MMX) */
	/* 0x5D PMULHRIW Rq,Eq (CYRIX MMX) */
	/* 0x5E PMACHRIW Rq,Eq (CYRIX MMX) */
	/* 0x60 PUNPCKLBW Rq,Eq (MMX) */
	/* 0x61 PUNPCKLWD Rq,Eq (MMX) */
	/* 0x62 PUNPCKLDQ Rq,Eq (MMX) */
	/* 0x63 PACKSSWB Rq,Eq (MMX) */
	/* 0x64 PCMPGTB Rq,Eq (MMX) */
	/* 0x65 PCMPGTW Rq,Eq (MMX) */
	/* 0x66 PCMPGTD Rq,Eq (MMX) */
	/* 0x67 PACKUSWB Rq,Eq (MMX) */
	/* 0x68 PUNPCKHBW Rq,Eq (MMX) */
	/* 0x69 PUNPCKHWD Rq,Eq (MMX) */
	/* 0x6A PUNPCKHDQ Rq,Eq (MMX) */
	/* 0x6B PACKSSDW Rq,Eq (MMX) */
	/* 0x6E MOVD Rq,Ed (MMX) */
	/* 0x6F MOVQ Rq,Eq (MMX) */
	/* 0x71 PSLLW/PSRAW/PSRLW Rq,Ib (MMX) */
	/* 0x72 PSLLD/PSRAD/PSRLD Rq,Ib (MMX) */
	/* 0x73 PSLLQ/PSRLQ Rq,Ib (MMX) */
	/* 0x74 PCMPEQB Rq,Eq (MMX) */
	/* 0x75 PCMPEQW Rq,Eq (MMX) */
	/* 0x76 PCMPEQD Rq,Eq (MMX) */
	/* 0x77 EMMS (MMX) */
	/* 0x7E MOVD Ed,Rq (MMX) */
	/* 0x7F MOVQ Ed,Rq (MMX) */
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
	/* 0xa2 CPUID */
	case 0xa3:												/* BT Ew,Gw */
		{
			GetRMrw;
			Bit16u mask=1 << (*rmrw & 15);
			if (rm >= 0xc0 ) {
				GetEArw;
				flags.cf=(*earw & mask)>0;
			} else {
				GetEAa;Bit16u old=LoadMw(eaa);
				flags.cf=(old & mask)>0;
			}
			if (flags.type!=t_CF)	{ flags.prev_type=flags.type;flags.type=t_CF;	}
			break;
		}
	case 0xa4:												/* SHLD Ew,Gw,Ib */
		{
			GetRMrw;
			if (rm >= 0xc0 ) {GetEArw;DSHLW(*earw,*rmrw,Fetchb(),LoadRw,SaveRw);}
			else {GetEAa;DSHLW(eaa,*rmrw,Fetchb(),LoadMw,SaveMw);}
			break;
		}
	case 0xa5:												/* SHLD Ew,Gw,CL */
		{
			GetRMrw;
			if (rm >= 0xc0 ) {GetEArw;DSHLW(*earw,*rmrw,reg_cl,LoadRw,SaveRw);}
			else {GetEAa;DSHLW(eaa,*rmrw,reg_cl,LoadMw,SaveMw);}
			break;
		}
	/* 0xa6 XBTS (early 386 only) CMPXCHG (early 486 only) */
	/* 0xa7 IBTS (early 386 only) CMPXCHG (early 486 only) */
	case 0xa8:												/* PUSH GS */		
		Push_16(SegValue(gs));break;
	case 0xa9:												/* POP GS */		
		SegSet16(gs,Pop_16());break;
	/* 0xaa RSM */
	case 0xab:												/* BTS Ew,Gw */
		{
			GetRMrw;
			Bit16u mask=1 << (*rmrw & 15);
			if (rm >= 0xc0 ) {
				GetEArw;
				flags.cf=(*earw & mask)>0;
				*earw|=mask;
			} else {
				GetEAa;Bit16u old=LoadMw(eaa);
				flags.cf=(old & mask)>0;
				SaveMw(eaa,old | mask);
			}
			if (flags.type!=t_CF)	{ flags.prev_type=flags.type;flags.type=t_CF;	}
			break;
		}
	case 0xac:												/* SHRD Ew,Gw,Ib */
		{
			GetRMrw;
			if (rm >= 0xc0 ) {GetEArw;DSHRW(*earw,*rmrw,Fetchb(),LoadRw,SaveRw);}
			else {GetEAa;DSHRW(eaa,*rmrw,Fetchb(),LoadMw,SaveMw);}
			break;
		}
	case 0xad:												/* SHRD Ew,Gw,CL */
		{
			GetRMrw;
			if (rm >= 0xc0 ) {GetEArw;DSHRW(*earw,*rmrw,reg_cl,LoadRw,SaveRw);}
			else {GetEAa;DSHRW(eaa,*rmrw,reg_cl,LoadMw,SaveMw);}
			break;
		}
	case 0xaf:												/* IMUL Gw,Ew */
		{
			GetRMrw;
			Bit32s res;
			if (rm >= 0xc0 ) {GetEArw;res=(Bit32s)(*rmrw) * (Bit32s)(*earws);}
			else {GetEAa;res=(Bit32s)(*rmrw) *(Bit32s)LoadMws(eaa);}
			*rmrw=res & 0xFFFF;
			flags.type=t_MUL;
			if ((res> -32768)  && (res<32767)) {flags.cf=false;flags.of=false;}
			else {flags.cf=true;flags.of=true;}
			break;
		}
	/* 0xb0 CMPXCHG Eb,Gb */
	/* 0xb1 CMPXCHG Ew,Gw */
	/* 0xb2 LSS */
	case 0xb3:												/* BTR Ew,Gw */
		{
			GetRMrw;
			Bit16u mask=1 << (*rmrw & 15);
			if (rm >= 0xc0 ) {
				GetEArw;
				flags.cf=(*earw & mask)>0;
				*earw&= ~mask;
			} else {
				GetEAa;Bit16u old=LoadMw(eaa);
				flags.cf=(old & mask)>0;
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
			Bit16u mask=1 << (Fetchb() & 15);
			if (rm >= 0xc0 ) {
				GetEArw;
				flags.cf=(*earw & mask)>0;
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
				}
			} else {
				GetEAa;Bit16u old=LoadMw(eaa);
				flags.cf=(old & mask)>0;
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
				flags.cf=(*earw & mask)>0;
				*earw^=mask;
			} else {
				GetEAa;Bit16u old=LoadMw(eaa);
				flags.cf=(old & mask)>0;
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
				flags.zf = true;
			} else {
				result = 0;
				while ((value & 0x01)==0) { result++; value>>=1; }
				flags.zf = false;
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
				flags.zf = true;
			} else {
				result = 15;	// Operandsize-1
				while ((value & 0x8000)==0) { result--; value<<=1; }
				flags.zf = false;
				*rmrw = result;
			}
			flags.type=t_UNKNOWN;
			break;
		}
	/* 0xbd BSR Gw,Ew */
	case 0xbe:												/* MOVSX Gw,Eb */
		{
			GetRMrw;															
			if (rm >= 0xc0 ) {GetEArb;*rmrw=*earbs;}
			else {GetEAa;*rmrw=LoadMbs(eaa);}
			break;
		}
	/* 0xc0 XADD Eb,Gb (486) */
	/* 0xc1 XADD Ew,Gw (486) */
	/* 0xc7 CMPXCHG8B Mq (P5) */
	/* 0xc8-cf BSWAP Rw (odd behavior,486) */
	case 0xc8:	BSWAP(reg_eax);		break;
	case 0xc9:	BSWAP(reg_ecx);		break;
	case 0xca:	BSWAP(reg_edx);		break;
	case 0xcb:	BSWAP(reg_ebx);		break;
	case 0xcc:	BSWAP(reg_esp);		break;
	case 0xcd:	BSWAP(reg_ebp);		break;
	case 0xce:	BSWAP(reg_esi);		break;
	case 0xcf:	BSWAP(reg_edi);		break;
		
	/* 0xd1 PSRLW Rq,Eq (MMX) */
	/* 0xd2 PSRLD Rq,Eq (MMX) */
	/* 0xd3 PSRLQ Rq,Eq (MMX) */
	/* 0xd5 PMULLW Rq,Eq (MMX) */
	/* 0xd8 PSUBUSB Rq,Eq (MMX) */
	/* 0xd9 PSUBUSW Rq,Eq (MMX) */
	/* 0xdb PAND Rq,Eq (MMX) */
	/* 0xdc PADDUSB Rq,Eq (MMX) */
	/* 0xdd PADDUSW Rq,Eq (MMX) */
	/* 0xdf PANDN Rq,Eq (MMX) */
	/* 0xe1 PSRAW Rq,Eq (MMX) */
	/* 0xe2 PSRAD Rq,Eq (MMX) */
	/* 0xe5 PMULHW Rq,Eq (MMX) */
	/* 0xe8 PSUBSB Rq,Eq (MMX) */
	/* 0xe9 PSUBSW Rq,Eq (MMX) */
	/* 0xeb POR Rq,Eq (MMX) */
	/* 0xec PADDSB Rq,Eq (MMX) */
	/* 0xed PADDSW Rq,Eq (MMX) */
	/* 0xef PXOR Rq,Eq (MMX) */
	/* 0xf1 PSLLW Rq,Eq (MMX) */
	/* 0xf2 PSLLD Rq,Eq (MMX) */
	/* 0xf3 PSLLQ Rq,Eq (MMX) */
	/* 0xf5 PMADDWD Rq,Eq (MMX) */
	/* 0xf8 PSUBB Rq,Eq (MMX) */
	/* 0xf9 PSUBW Rq,Eq (MMX) */
	/* 0xfa PSUBD Rq,Eq (MMX) */
	/* 0xfc PADDB Rq,Eq (MMX) */
	/* 0xfd PADDW Rq,Eq (MMX) */
	/* 0xfe PADDD Rq,Eq (MMX) */
	default:
		 SUBIP(1);
		 E_Exit("CPU:Opcode 0F:%2X Unhandled",Fetchb());
	}
