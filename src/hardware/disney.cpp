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
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <string.h>
#include "dosbox.h"
#include "inout.h"
#include "mixer.h"
#include "pic.h"
#include "setup.h"

#define DISNEY_BASE 0x0378

#define DISNEY_SIZE 32

static struct {
	Bit8u data;
	Bit8u status;
	Bit8u control;
	Bit8u buffer[DISNEY_SIZE];
	Bitu used;Bitu last_used;
	MixerChannel * chan;
} disney;

static void disney_write(Bitu port,Bitu val,Bitu iolen) {
	if (!disney.last_used) {
		disney.chan->Enable(true);
	}
	disney.last_used=PIC_Ticks;
	switch (port-DISNEY_BASE) {
	case 0:		/* Data Port */
		disney.data=val;
		break;
	case 1:		/* Status Port */		
		LOG(LOG_MISC,LOG_NORMAL)("DISNEY:Status write %x",val);
		break;
	case 2:		/* Control Port */
//		LOG_WARN("DISNEY:Control write %x",val);
		if (val&0x8) {
			if (disney.used<DISNEY_SIZE) {
				disney.buffer[disney.used++]=disney.data;
			}
		}
		if (val&0x10) LOG(LOG_MISC,LOG_ERROR)("DISNEY:Parallel IRQ Enabled");
		disney.control=val;
		break;
	}
}

static Bitu disney_read(Bitu port,Bitu iolen) {
	switch (port-DISNEY_BASE) {
	case 0:		/* Data Port */
//		LOG(LOG_MISC,LOG_NORMAL)("DISNEY:Read from data port");
		return disney.data;
		break;
	case 1:		/* Status Port */	
//		LOG(LOG_MISC,"DISNEY:Read from status port %X",disney.status);
		if (disney.used>=16) return 0x40;
		else return 0x0;
		break;
	case 2:		/* Control Port */
		LOG(LOG_MISC,LOG_NORMAL)("DISNEY:Read from control port");
		return disney.control;
		break;
	}
	return 0xff;
}


static void DISNEY_CallBack(Bitu len) {
	if (!len) return;
	if (disney.used>len) {
		disney.chan->AddSamples_m8(len,disney.buffer);
		memmove(disney.buffer,&disney.buffer[len],disney.used-len);
		disney.used-=len;
	} else {
		disney.chan->AddSamples_m8(disney.used,disney.buffer);
		disney.chan->AddSilence();
		disney.used=0;
	}
	if (disney.last_used+5000<PIC_Ticks) {
		disney.last_used=0;
		disney.chan->Enable(false);
	}
}


void DISNEY_Init(Section* sec) {
	MSG_Add("DISNEY_CONFIGFILE_HELP","Nothing to setup yet!\n");
	Section_prop * section=static_cast<Section_prop *>(sec);
	if(!section->Get_bool("disney")) return;

	IO_RegisterWriteHandler(DISNEY_BASE,disney_write,IO_MB,3);
	IO_RegisterReadHandler(DISNEY_BASE,disney_read,IO_MB,3);

	disney.chan=MIXER_AddChannel(&DISNEY_CallBack,7000,"DISNEY");

	disney.status=0x84;
	disney.control=0;
	disney.used=0;
	disney.last_used=0;
}

