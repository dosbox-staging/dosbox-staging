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
#include "mapper.h"
#include "pic.h"
#include "hardware.h"

#define SYSEX_SIZE 1024
#define RAWBUF	1024

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

  0,2,3,2, 0,0,1,0, 1,0,1,1, 1,0,1,0   // 0xf0
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
	virtual void PlayMsg(Bit8u * msg) {};
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
	Bit8u cmd_buf[8];
	struct {
		Bit8u buf[SYSEX_SIZE];
		Bitu used;
	} sysex;
	bool available;
	MidiHandler * handler;
	struct {
		FILE * handle;
		Bit8u buffer[RAWBUF+SYSEX_SIZE];
		bool capturing;
		Bitu used,done;
		Bit32u last;
	} raw;
} midi;

static Bit8u midi_header[]={
	'M','T','h','d',			/* Bit32u, Header Chunk */
	0x0,0x0,0x0,0x6,			/* Bit32u, Chunk Length */
	0x0,0x0,					/* Bit16u, Format, 0=single track */
	0x0,0x1,					/* Bit16u, Track Count, 1 track */
	0x01,0xf4,					/* Bit16u, Timing, 2 beats/second with 500 frames */
	'M','T','r','k',			/* Bit32u, Track Chunk */
	0x0,0x0,0x0,0x0,			/* Bit32u, Chunk Length */
	//Track data
};

#define ADDBUF(_VAL_) midi.raw.buffer[midi.raw.used++]=(Bit8u)(_VAL_);
static INLINE void RawAddNumber(Bit32u val) {
	if (val & 0xfe00000) ADDBUF(0x80|((val >> 21) & 0x7f));
	if (val & 0xfffc000) ADDBUF(0x80|((val >> 14) & 0x7f));
	if (val & 0xfffff80) ADDBUF(0x80|((val >> 7) & 0x7f));
	ADDBUF(val  & 0x7f);
}
static INLINE void RawAddDelta(void) {
	if (!midi.raw.handle) {
		midi.raw.handle=OpenCaptureFile("Raw Midi",".mid");
		if (!midi.raw.handle) {
			midi.raw.capturing=false;		
			return;
		}
		fwrite(midi_header,1,sizeof(midi_header),midi.raw.handle);
		midi.raw.last=PIC_Ticks;
	}
	Bit32u delta=PIC_Ticks-midi.raw.last;
	midi.raw.last=PIC_Ticks;
	RawAddNumber(delta);
}

static INLINE void RawAddData(Bit8u * data,Bitu len) {
	for (Bitu i=0;i<len;i++) ADDBUF(data[i]);
	if (midi.raw.used>=RAWBUF) {
		fwrite(midi.raw.buffer,1,midi.raw.used,midi.raw.handle);
		midi.raw.done+=midi.raw.used;
		midi.raw.used=0;
	}
}

static void MIDI_SaveRawEvent(void) {
	/* Check for previously opened wave file */
	if (midi.raw.capturing) {
		LOG_MSG("Stopping raw midi saving.");
		midi.raw.capturing=false;
		if (!midi.raw.handle) return;
		ADDBUF(0x00);//Delta time
		ADDBUF(0xff);ADDBUF(0x2F);ADDBUF(0x00);		//End of track event
		fwrite(midi.raw.buffer,1,midi.raw.used,midi.raw.handle);
		midi.raw.done+=midi.raw.used;
		fseek(midi.raw.handle,18, SEEK_SET);
		Bit8u size[4];
		size[0]=(Bit8u)(midi.raw.done >> 24);
		size[1]=(Bit8u)(midi.raw.done >> 16);
		size[2]=(Bit8u)(midi.raw.done >> 8);
		size[3]=(Bit8u)(midi.raw.done >> 0);
		fwrite(&size,1,4,midi.raw.handle);
		fclose(midi.raw.handle);
		midi.raw.handle=0;
	} else {
		LOG_MSG("Preparing for raw midi capture, will start with first data.");
		midi.raw.used=0;
		midi.raw.done=0;
		midi.raw.handle=0;
		midi.raw.capturing=true;
	}
}


void MIDI_RawOutByte(Bit8u data) {
	/* Test for a active sysex tranfer */
	if (midi.status==0xf0) {
		if (!(data&0x80)) { 
			if (midi.sysex.used<(SYSEX_SIZE-1)) midi.sysex.buf[midi.sysex.used++]=data;
			return;
		} else {
			midi.sysex.buf[midi.sysex.used++]=0xf7;
			midi.handler->PlaySysex(midi.sysex.buf,midi.sysex.used);
			LOG(LOG_ALL,LOG_NORMAL)("Sysex message size %d",midi.sysex.used);
			if (midi.raw.capturing) {
				RawAddDelta();
				ADDBUF(0xf0);
				RawAddNumber(midi.sysex.used-1);
				RawAddData(&midi.sysex.buf[1],midi.sysex.used-1);
			}
		}
	}
	if (data&0x80) {
		midi.status=data;
		midi.cmd_pos=0;
		midi.cmd_len=MIDI_evt_len[data];
		if (midi.status==0xf0) {
			midi.sysex.buf[0]=0xf0;
			midi.sysex.used=1;
		}
	}
	if (midi.cmd_len) {
		midi.cmd_buf[midi.cmd_pos++]=data;
		if (midi.cmd_pos >= midi.cmd_len) {
			if (midi.raw.capturing) {
				RawAddDelta();
				RawAddData(midi.cmd_buf,midi.cmd_len);
			}
			midi.handler->PlayMsg(midi.cmd_buf);
			midi.cmd_pos=1;		//Use Running status
		}
	}
}

bool MIDI_Available(void)  {
	return midi.available;
}

static void MIDI_Stop(Section* sec) {
	if (midi.raw.handle) MIDI_SaveRawEvent();
}

void MIDI_Init(Section * sec) {
	Section_prop * section=static_cast<Section_prop *>(sec);
	const char * dev=section->Get_string("device");
	const char * conf=section->Get_string("config");
	/* If device = "default" go for first handler that works */
	MidiHandler * handler;
	MAPPER_AddHandler(MIDI_SaveRawEvent,MK_f8,MMOD1|MMOD2,"caprawmidi","Cap MIDI");
	sec->AddDestroyFunction(&MIDI_Stop);
	midi.status=0x00;
	midi.cmd_pos=0;
	midi.cmd_len=0;
	midi.raw.capturing=false;
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
}

