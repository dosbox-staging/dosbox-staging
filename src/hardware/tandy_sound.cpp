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

/* 
	Probably just use the mame code for the same chip sometime 
*/

#include <math.h>
#include "dosbox.h"
#include "inout.h"
#include "mixer.h"
#include "mem.h"

#define TANDY_DIV 111860 
#define TANDY_RATE 22050
#define BIT_SHIFT 16
#define TANDY_VOLUME 10000
static MIXER_Channel * tandy_chan;
struct TandyChannel {
	Bit32u div;
	Bit32u freq_add;
	Bit32u freq_pos;
};


struct TandyBlock {
	TandyChannel chan[3];

	Bit32s volume[4];
	Bit8u reg;
};

static TandyBlock tandy;


#define REG_CHAN0DIV	0						/* 0 0 0 */
#define REG_CHAN0ATT	1						/* 0 0 1 */
#define REG_CHAN1DIV	2						/* 0 1 0 */
#define REG_CHAN1ATT	3						/* 0 1 1 */
#define REG_CHAN2DIV	4						/* 1 0 0 */
#define REG_CHAN2ATT	5						/* 1 0 1 */

#define REG_NOISEATT	7						/* 1 1 1 */

//TODO a db volume table :)
static Bit32s vol_table[16];


static void write_pc0(Bit32u port,Bit8u val) {
	/* Test for a command byte */
	if (val & 0x80) {
		tandy.reg=(val>>4) & 7;
		switch (tandy.reg) {
		case REG_CHAN0DIV:
		case REG_CHAN1DIV:
		case REG_CHAN2DIV:
			tandy.chan[tandy.reg>>1].div=val&15;
			break;
		case REG_CHAN0ATT:
		case REG_CHAN1ATT:
		case REG_CHAN2ATT:
		case REG_NOISEATT:
			tandy.volume[tandy.reg>>1]=vol_table[val&15];
			if (tandy.volume[0] || tandy.volume[1] || tandy.volume[2] || tandy.volume[3]) {
				MIXER_Enable(tandy_chan,true);
			} else {
				MIXER_Enable(tandy_chan,false);
			}
			break;
		default:
//			LOG_WARN("TANDY:Illegal register %d selected",tandy.reg);
			break;
		}
	} else {
/* Dual byte command */
		switch (tandy.reg) {
#define MAKE_ADD(DIV)(Bit32u)((2 << BIT_SHIFT)/((float)TANDY_RATE/((float)TANDY_DIV/(float)DIV)));
		case REG_CHAN0DIV:
		case REG_CHAN1DIV:
		case REG_CHAN2DIV:
			tandy.chan[tandy.reg>>1].div|=(val & 63)<<4;
			tandy.chan[tandy.reg>>1].freq_add=MAKE_ADD(tandy.chan[tandy.reg>>1].div);
//			tandy.chan[tandy.reg>>1].freq_pos=0;
			break;		
		default:
			LOG_WARN("TANDY:Illegal dual byte reg %d",tandy.reg);
		};
	}

}

static void TANDYSOUND_CallBack(Bit8u * stream,Bit32u len) {
	for (Bit32u i=0;i<len;i++) {
		Bit32s sample=0;
		/* Generate the sound from the 3 channels */	
		for (Bit32u c=0;c<3;c++) {
			if (tandy.volume[c]) {
				if (tandy.chan[c].freq_pos<(1 << BIT_SHIFT)) sample+=tandy.volume[c];
				else sample-=tandy.volume[c];
				tandy.chan[c].freq_pos+=tandy.chan[c].freq_add;
				if (tandy.chan[c].freq_pos>=(2 << BIT_SHIFT)) tandy.chan[c].freq_pos-=(2 << BIT_SHIFT);
			}
		}
		/* Generate the noise channel */

		if (sample>MAX_AUDIO) *(Bit16s *)stream=MAX_AUDIO;
		else if (sample<MIN_AUDIO) *(Bit16s *)stream=MIN_AUDIO;
		else *(Bit16s *)stream=(Bit16s)sample;
		stream+=2;
	}
};

void TANDY_Init(void) {
	IO_RegisterWriteHandler(0xc0,write_pc0,"Tandy Sound");
	tandy_chan=MIXER_AddChannel(&TANDYSOUND_CallBack,TANDY_RATE,"TANDY");
	MIXER_Enable(tandy_chan,false);
	MIXER_SetMode(tandy_chan,MIXER_16MONO);
	/* Calculate the volume table */
	float out=TANDY_VOLUME;
	for (Bit32u i=0;i<15;i++) {
		vol_table[i]=(Bit32s)out;
		out /= (float)1.258925412;	/* = 10 ^ (2/20) = 2dB */
	}
	vol_table[15]=0;
	/* Setup a byte for tandy detection */
	real_writeb(0xffff,0xe,0xfd);
}

