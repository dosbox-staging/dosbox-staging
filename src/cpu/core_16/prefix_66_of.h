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

switch (Fetchb()) {


	case 0xa4:												/* SHLD Ed,Gd,Ib */
		{
			GetRMrd;
			if (rm >= 0xc0 ) {GetEArd;DSHLD(*eard,*rmrd,Fetchb(),LoadRd,SaveRd);}
			else {GetEAa;DSHLD(eaa,*rmrd,Fetchb(),LoadMd,SaveMd);}
			break;
		}
	case 0xac:												/* SHRD Ed,Gd,Ib */
		{
			GetRMrd;
			if (rm >= 0xc0 ) {GetEArd;DSHRD(*eard,*rmrd,Fetchb(),LoadRd,SaveRd);}
			else {GetEAa;DSHRD(eaa,*rmrd,Fetchb(),LoadMd,SaveMd);}
			break;
		}
	case 0xad:												/* SHRD Ed,Gd,Cl */
		{
			GetRMrd;
			if (rm >= 0xc0 ) {GetEArd;DSHRD(*eard,*rmrd,reg_cl,LoadRd,SaveRd);}
			else {GetEAa;DSHRD(eaa,*rmrd,reg_cl,LoadMd,SaveMd);}
			break;
		}

	case 0xb6:												/* MOVZX Gd,Eb */
		{
			GetRMrd;															
			if (rm >= 0xc0 ) {GetEArb;*rmrd=*earb;}
			else {GetEAa;*rmrd=LoadMb(eaa);}
			break;
		}
	case 0xaf:												/* IMUL Gd,Ed */
		{
			RMGdEdOp3(DIMULD,*rmrd);
			break;
		};
	case 0xb7:												/* MOVXZ Gd,Ew */
		{
			GetRMrd;
			if (rm >= 0xc0 ) {GetEArw;*rmrd=*earw;}
			else {GetEAa;*rmrd=LoadMw(eaa);}
			break;
		}
	case 0xba:												/* GRP8 Ed,Ib */
		{
			GetRM;
			if (rm >= 0xc0 ) {
				GetEArd;
				Bit32u mask=1 << (Fetchb() & 31);
				SETFLAGBIT(CF,(*eard & mask));
				switch (rm & 0x38) {
				case 0x20:									/* BT */
					break;
				case 0x28:									/* BTS */
					*eard|=mask;
					break;
				case 0x30:									/* BTR */
					*eard&=~mask;
					break;
				case 0x38:									/* BTC */
					if (GETFLAG(CF)) *eard&=~mask;
					else *eard|=mask;
					break;
				default:
					E_Exit("CPU:66:0F:BA:Illegal subfunction %X",rm & 0x38);
				}
			} else {
				GetEAa;Bit32u old=LoadMd(eaa);
				Bit32u mask=1 << (Fetchb() & 31);
				SETFLAGBIT(CF,(old & mask));
				switch (rm & 0x38) {
				case 0x20:									/* BT */
					break;
				case 0x28:									/* BTS */
					SaveMd(eaa,old|mask);
					break;
				case 0x30:									/* BTR */
					SaveMd(eaa,old & ~mask);
					break;
				case 0x38:									/* BTC */
					if (GETFLAG(CF)) old&=~mask;
					else old|=mask;
					SaveMd(eaa,old);
					break;
				default:
					E_Exit("CPU:66:0F:BA:Illegal subfunction %X",rm & 0x38);
				}
			}
			if (flags.type!=t_CF) flags.prev_type=flags.type;
			flags.type=t_CF;
			break;
		}
	case 0xbb:												/* BTC Ed,Gd */
		{
			GetRMrd;
			Bit32u mask=1 << (*rmrd & 31);
			if (rm >= 0xc0 ) {
				GetEArd;
				SETFLAGBIT(CF,(*eard & mask));
				*eard^=mask;
			} else {
				GetEAa;Bit32u old=LoadMd(eaa);
				SETFLAGBIT(CF,(old & mask));
				SaveMd(eaa,old ^ mask);
			}
			if (flags.type!=t_CF)	{ flags.prev_type=flags.type;flags.type=t_CF;	}
			break;
		}
	case 0xbc:												/* BSF Gd,Ed */
		{
			GetRMrd;
			Bit32u result,value;
			if (rm >= 0xc0) { GetEArd; value=*eard; } 
			else			{ GetEAa; value=LoadMd(eaa); }
			if (value==0) {
				SETFLAGBIT(ZF,true);
			} else {
				result = 0;
				while ((value & 0x01)==0) { result++; value>>=1; }
				SETFLAGBIT(ZF,false);
				*rmrd = result;
			}
			flags.type=t_UNKNOWN;
			break;
		}
	case 0xbd:												/*  BSR Gd,Ed */
		{
			GetRMrd;
			Bit32u result,value;
			if (rm >= 0xc0) { GetEArd; value=*eard; } 
			else			{ GetEAa; value=LoadMd(eaa); }
			if (value==0) {
				SETFLAGBIT(ZF,true);
			} else {
				result = 35;	// Operandsize-1
				while ((value & 0x80000000)==0) { result--; value<<=1; }
				SETFLAGBIT(ZF,false);
				*rmrd = result;
			}
			flags.type=t_UNKNOWN;
			break;
		}
	case 0xbe:												/* MOVSX Gd,Eb */
		{
			GetRMrd;															
			if (rm >= 0xc0 ) {GetEArb;*rmrd=*(Bit8s *)earb;}
			else {GetEAa;*rmrd=LoadMbs(eaa);}
			break;
		}
	case 0xbf:												/* MOVSX Gd,Ew */
		{
			GetRMrd;															
			if (rm >= 0xc0 ) {GetEArw;*rmrd=*(Bit16s *)earw;}
			else {GetEAa;*rmrd=LoadMws(eaa);}
			break;
		}
	default:
		SUBIP(1);
		E_Exit("CPU:Opcode 66:0F:%2X Unhandled",Fetchb());
}
