/*
 *  Copyright (C) 2002-2003  The DOSBox Team
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

#include <fcntl.h>

static int m_device;

#define SEQ_MIDIPUTC    5

static void MIDI_PlaySysex(Bit8u * sysex,Bitu len) {
	Bit8u buf[1024];Bitu pos=0;
	for (;len>0;len--) {
		buf[pos++] = SEQ_MIDIPUTC;
		buf[pos++] = *sysex++;
		buf[pos++] = 0;					//Device 0?
		buf[pos++] = 0;
	}
	write(m_device,buf,pos);	
}

static void MIDI_PlayMsg(Bit32u msg) {
	Bit8u buf[128];Bitu pos=0;

	Bitu len=MIDI_evt_len[msg & 0xff];
	for (;len>0;len--) {
		buf[pos++] = SEQ_MIDIPUTC;
		buf[pos++] = msg & 0xff;
		buf[pos++] = 0;					//Device 0?
		buf[pos++] = 0;
		msg >>=8;
	}
	write(m_device,buf,pos);
}

static void MIDI_ShutDown(void) {
	if (m_device>0) close(m_device);
}

static bool MIDI_StartUp(void) {
	m_device=open("/dev/midi", O_WRONLY, 0);
	if (m_device<0) return false;
	return true;
}


