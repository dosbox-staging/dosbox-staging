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

#include <string.h>
#include "dosbox.h"
#include "inout.h"
#include "mixer.h"
#include "dma.h"
#include "pic.h"
#include "hardware.h"
#include "setup.h"
#include "programs.h"



#define DISNEY_BASE 0x0378

#define DISNEY_SIZE 32

static struct {
	Bit8u data;
	Bit8u status;
	Bit8u control;
	Bit8u buffer[DISNEY_SIZE];
	Bitu used;
	MIXER_Channel * chan;
} disney;

static void disney_write(Bit32u port,Bit8u val) {
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

static Bit8u disney_read(Bit32u port) {

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


static void DISNEY_CallBack(Bit8u * stream,Bit32u len) {
	if (!len) return;
	if (disney.used>len) {
		memcpy(stream,disney.buffer,len);
		memmove(disney.buffer,&disney.buffer[len],disney.used-len);
		disney.used-=len;
		return;
	} else {
		memcpy(stream,disney.buffer,disney.used);
		memset(stream+disney.used,0x80,len-disney.used);
		disney.used=0;
	}
}


void DISNEY_Init(Section* sec) {
	MSG_Add("DISNEY_CONFIGFILE_HELP","Nothing to setup yet!\n");
	Section_prop * section=static_cast<Section_prop *>(sec);
	if(!section->Get_bool("disney")) return;

	IO_RegisterWriteHandler(DISNEY_BASE,disney_write,"DISNEY");
	IO_RegisterWriteHandler(DISNEY_BASE+1,disney_write,"DISNEY");
	IO_RegisterWriteHandler(DISNEY_BASE+2,disney_write,"DISNEY");

	IO_RegisterReadHandler(DISNEY_BASE,disney_read,"DISNEY");
	IO_RegisterReadHandler(DISNEY_BASE+1,disney_read,"DISNEY");
	IO_RegisterReadHandler(DISNEY_BASE+2,disney_read,"DISNEY");

	disney.chan=MIXER_AddChannel(&DISNEY_CallBack,7000,"DISNEY");
	MIXER_SetMode(disney.chan,MIXER_8MONO);
	MIXER_Enable(disney.chan,true);

	disney.status=0x84;
	disney.control=0;
	disney.used=0;
}

