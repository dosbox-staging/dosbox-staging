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


#include "dosbox.h"
#include "setup.h"

Bit8u MIDI_evt_len[256] = {
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x00
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x10
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x20
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x30
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x40
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x50
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x60
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x70

  3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3,  // 0x80
  3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3,  // 0x90
  3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3,  // 0xa0
  3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3,  // 0xb0

  2,2,2,2, 2,2,2,2, 2,2,2,2, 2,2,2,2,  // 0xc0
  2,2,2,2, 2,2,2,2, 2,2,2,2, 2,2,2,2,  // 0xd0

  3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3,  // 0xe0

  0,2,3,2, 0,0,1,1, 1,0,1,1, 1,0,1,1   // 0xf0
};

#define SYSEX_SIZE 1024
 
#if defined (WIN32)

#include "midi_win32.h"

#elif defined (UNIX) && !defined(__BEOS__)

#include "midi_oss.h"

#else	/* Fall back to no device playing */

static void MIDI_PlayMsg(Bit32u msg) {}
static void MIDI_PlaySysex(Bit8u * sysex,Bitu len) {};
static bool MIDI_StartUp(void) { return false;}

#endif


static struct {
	Bitu status;
	Bitu cmd_len;
	Bitu cmd_pos;

	Bit32u cmd_msg;
	Bit8u data[4];
	struct {
		Bit8u buf[SYSEX_SIZE];
		Bitu used;
		bool active;
	} sysex;
	bool available;
} midi;


void MIDI_RawOutByte(Bit8u data) {
	/* Test for a new status byte */
	if (midi.sysex.active && !(data&0x80)) {
		if (midi.sysex.used<(SYSEX_SIZE-1)) midi.sysex.buf[midi.sysex.used++]=data;
		return;
	} else if (data&0x80) {
		if (midi.sysex.active) {
			/* Play a sysex message */
			midi.sysex.buf[midi.sysex.used++]=0xf7;
			MIDI_PlaySysex(midi.sysex.buf,midi.sysex.used);
			LOG(0,"Sysex message size %d",midi.sysex.used);
			midi.sysex.active=false;
			if (data==0xf7) return;
		}
		midi.status=data;
		midi.cmd_pos=0;
		midi.cmd_msg=0;
		if (midi.status==0xf0) {
			midi.sysex.active=true;
			midi.sysex.buf[0]=0xf0;
			midi.sysex.used=1;
			return;
		}
		midi.cmd_len=MIDI_evt_len[data];
	}
	midi.cmd_msg|=data << (8 * midi.cmd_pos);
	midi.cmd_pos++;
	if (midi.cmd_pos >= midi.cmd_len) {
		MIDI_PlayMsg(midi.cmd_msg);
		midi.cmd_msg=midi.status;
		midi.cmd_pos=1;
	}
}

bool MIDI_Available(void)  {
	return midi.available;
}


void MIDI_Init(Section * sect) {
	midi.available=MIDI_StartUp();
}

