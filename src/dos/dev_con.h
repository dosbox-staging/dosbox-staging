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

class device_CON : public DOS_Device {
public:
	device_CON();
	bool Read(Bit8u * data,Bit16u * size);
	bool Write(Bit8u * data,Bit16u * size);
	bool Seek(Bit32u * pos,Bit32u type);
	bool Close();
	Bit16u GetInformation(void);
private:
	Bit8u cache;
};

bool device_CON::Read(Bit8u * data,Bit16u * size) {
	Bit16u oldax=reg_ax;
	Bit16u count=0;
	if ((cache) && (*size)) {
		data[count++]=cache;
		cache=0;
	}
	while (*size>count) {
		reg_ah=0;
		CALLBACK_RunRealInt(0x16);
		switch(reg_al) {
		case 13:
			data[count++]=0x0D;
//			if (*size>count) data[count++]=0x0A;
//			else cache=0x0A;
			*size=count;
			reg_ax=oldax;
			return true;
		default:
			data[count++]=reg_al;
			break;
		case 0:
			data[count++]=reg_al;
			if (*size>count) data[count++]=reg_ah;
			else cache=reg_ah;
			break;
		}
	}
	*size=count;
	reg_ax=oldax;
	return true;
}

extern void INT10_TeletypeOutput(Bit8u chr,Bit8u attr,bool showattr, Bit8u page);
bool device_CON::Write(Bit8u * data,Bit16u * size) {
//TODO Hack a way to call int 0x10
	Bit16u oldax=reg_ax;Bit16u oldbx=reg_bx;
	Bit16u count=0;
	while (*size>count) {
/*
		reg_al=data[count];
		reg_ah=0x0e;
		reg_bx=0x0007;
		CALLBACK_RunRealInt(0x10);
*/
		INT10_TeletypeOutput(data[count],7,false,0);
		count++;
	}
	*size=count;
//	reg_ax=oldax;reg_bx=oldbx;
	return true;
}

bool device_CON::Seek(Bit32u * pos,Bit32u type) {
	return false;
}

bool device_CON::Close() {
	return false;
}

Bit16u device_CON::GetInformation(void) {
	Bit16u head=mem_readw(BIOS_KEYBOARD_BUFFER_HEAD);
	Bit16u tail=mem_readw(BIOS_KEYBOARD_BUFFER_TAIL);
	
	if ((head==tail) && !cache) return 0x80D3; /* No Key Available */
	return 0x8093;		/* Key Available */
};


device_CON::device_CON() {
	name="CON";
	cache=0;
}

