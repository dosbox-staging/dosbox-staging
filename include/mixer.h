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

typedef void (*MIXER_MixHandler)(Bit8u * sampdate,Bit32u len);

#define MIXER_8MONO		0
#define MIXER_8STEREO	1
#define MIXER_16MONO	2
#define MIXER_16STEREO	3

#define MAX_AUDIO ((1<<(16-1))-1)
#define MIN_AUDIO -(1<<(16-1))



struct MIXER_Channel;


MIXER_Channel * MIXER_AddChannel(MIXER_MixHandler handler,Bit32u freq,char * name);
void MIXER_SetVolume(MIXER_Channel * chan,Bit8u vol);
void MIXER_SetFreq(MIXER_Channel * chan,Bit32u freq);
void MIXER_SetMode(MIXER_Channel * chan,Bit8u mode);
void MIXER_Enable(MIXER_Channel * chan,bool enable);

/* PC Speakers functions, tightly related to the timer functions */

void PCSPEAKER_SetCounter(Bitu cntr,Bitu mode);
void PCSPEAKER_SetType(Bitu mode);

