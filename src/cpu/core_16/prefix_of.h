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
				E_Exit("CPU:GRP7:Illegal call %2X",rm);
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
	case 0xa0:												/* PUSH FS */		
		Push_16(Segs[fs].value);break;
	case 0xa1:												/* POP FS */		
		SetSegment_16(fs,Pop_16());break;
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
	case 0xa8:												/* PUSH GS */		
		Push_16(Segs[gs].value);break;
	case 0xa9:												/* POP GS */		
		SetSegment_16(gs,Pop_16());break;
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
	case 0xb4:												/* LFS */
		{	
			GetRMrw;GetEAa;
			*rmrw=LoadMw(eaa);SetSegment_16(fs,LoadMw(eaa+2));
			break;
		}
	case 0xb5:												/* LGS */
		{	
			GetRMrw;GetEAa;
			*rmrw=LoadMw(eaa);SetSegment_16(gs,LoadMw(eaa+2));
			break;
		}
	case 0xb6:												/* MOVZX Gw,Eb */
		{
			GetRMrw;															
			if (rm >= 0xc0 ) {GetEArb;*rmrw=*earb;}
			else {GetEAa;*rmrw=LoadMb(eaa);}
			break;
		}
	case 0xba:												/* GRP8 Ew,Ib */
		{
			GetRM;
			if (rm >= 0xc0 ) {
				GetEArw;
				Bit16u mask=1 << (Fetchb() & 15);
				flags.cf=(*earw & mask)>0;
				switch (rm & 0x38) {
				case 0x20:									/* BT */
					break;
				case 0x28:									/* BTS */
					*earw|=mask;
					break;
				case 0x30:									/* BTR */
					*earw&=~mask;
					break;
				case 0x38:									/* BTC */
					if (flags.cf) *earw&=~mask;
					else *earw|=mask;
					break;
				}
			} else {
				GetEAa;Bit16u old=LoadMw(eaa);
				Bit16u mask=1 << (Fetchb() & 15);
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
					if (flags.cf) old&=~mask;
					else old|=mask;
					SaveMw(eaa,old);
					break;
				}
			}
			if (flags.type!=t_CF) flags.prev_type=flags.type;
			flags.type=t_CF;
			break;
		}
	case 0xbe:												/* MOVSX Gw,Eb */
		{
			GetRMrw;															
			if (rm >= 0xc0 ) {GetEArb;*rmrw=*earbs;}
			else {GetEAa;*rmrw=LoadMbs(eaa);}
			break;
		}
	default:
		 SUBIP(1);
		 E_Exit("CPU:Opcode 0F:%2X Unhandled",Fetchb());
	}
