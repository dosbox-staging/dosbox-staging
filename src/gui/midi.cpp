/*
 *  Copyright (C) 2002-2004  The DOSBox Team
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

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "dosbox.h"
#include "cross.h"
#include "support.h"
#include "setup.h"

#define SYSEX_SIZE 1024

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

class MidiHandler;

MidiHandler * handler_list=0;

class MidiHandler {
public:
	MidiHandler() {
		next=handler_list;
		handler_list=this;
	};
	virtual bool Open(const char * conf) { return true; };
	virtual void Close(void) {};
	virtual void PlayMsg(Bit32u msg) {};
	virtual void PlaySysex(Bit8u * sysex,Bitu len) {};
	virtual char * GetName(void) { return "none"; };
	MidiHandler * next;
};

MidiHandler Midi_none;

/* Include different midi drivers, lowest ones get checked first for default */

#if defined(MACOSX)

#include "midi_coreaudio.h"

#elif defined (WIN32)

#include "midi_win32.h"

#else

#include "midi_oss.h"

#endif

#if defined (HAVE_ALSA)

#include "midi_alsa.h"

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
	MidiHandler * handler;
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
			midi.handler->PlaySysex(midi.sysex.buf,midi.sysex.used);
			LOG(LOG_ALL,LOG_NORMAL)("Sysex message size %d",midi.sysex.used);
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
		midi.handler->PlayMsg(midi.cmd_msg);
		midi.cmd_msg=midi.status;
		midi.cmd_pos=1;
	}
}

bool MIDI_Available(void)  {
	return midi.available;
}

void MIDI_Init(Section * sec) {
	Section_prop * section=static_cast<Section_prop *>(sec);
	const char * dev=section->Get_string("device");
	const char * conf=section->Get_string("config");
	/* If device = "default" go for first handler that works */
	MidiHandler * handler;
	if (!strcasecmp(dev,"default")) goto getdefault;
	handler=handler_list;
	while (handler) {
		if (!strcasecmp(dev,handler->GetName())) {
			if (!handler->Open(conf)) {
				LOG_MSG("MIDI:Can't open device:%s with config:%s.",dev,conf);	
				goto getdefault;
			}
			midi.handler=handler;
			midi.available=true;	
			LOG_MSG("MIDI:Opened device:%s",handler->GetName());
			return;
		}
		handler=handler->next;
	}
	LOG_MSG("MIDI:Can't find device:%s, finding default handler.",dev);	
getdefault:	
	handler=handler_list;
	while (handler) {
		if (handler->Open(conf)) {
			midi.available=true;	
			midi.handler=handler;
			LOG_MSG("MIDI:Opened device:%s",handler->GetName());
			return;
		}
		handler=handler->next;
	}
	/* This shouldn't be possible */
	midi.available=false;
}

