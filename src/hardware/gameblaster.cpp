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

#include <math.h>
#include "dosbox.h"
#include "inout.h"
#include "mixer.h"
#include "mem.h"
#include "hardware.h"

#define CMS_RATE 22050
#define CMS_VOLUME 6000


#define FREQ_SHIFT 16

#define SIN_ENT 1024
#define SIN_MAX (SIN_ENT << FREQ_SHIFT)

#ifndef PI
#define PI 3.14159265358979323846
#endif



struct CMS {
	struct {
		Bit32u freq_pos;
		Bit32u freq_add;
		Bit16s * vol_left;
		Bit16s * vol_right;
		Bit8u octave;
		Bit8u freq;
	} chan[6];
	struct {
		Bit32u freq_pos;
		Bit32u freq_add;
		Bit32u random_val;
	} noise[2];

	Bit8u voice_enabled;
	Bit8u noise_enabled;
	Bit8u reg;
};

static Bit32u freq_table[256][8];
static Bit32u noise_freq[3];
static Bit16s vol_table[16];
static Bit16s sin_table[16][SIN_ENT];

static MIXER_Channel * cms_chan;
static CMS cms_block[2];

static void write_cms(Bit32u port,Bit8u val) {
	Bit32u sel=(port>>1)&1;
	CMS * cms=&cms_block[sel];
	switch (port & 1) {
	case 1:		/* Register Select */
		cms->reg=val;
		break;
	case 0:		/* Write Register */
		switch (cms->reg) {
		case 0x00:	case 0x01:	case 0x02:	/* Volume Select */
		case 0x03:	case 0x04:	case 0x05:		
			cms->chan[cms->reg].vol_left=sin_table[val & 0xf];
			cms->chan[cms->reg].vol_right=sin_table[(val>>4) & 0xf];
			break;
		case 0x08:	case 0x09:	case 0x0a:	/* Frequency Select */
		case 0x0b:	case 0x0c:	case 0x0d:		
			{
				Bit8u chan=cms->reg-0x08;
				cms->chan[chan].freq=val;
				/* Get a new entry in the freq table */
				cms->chan[chan].freq_add=freq_table[cms->chan[chan].freq][cms->chan[chan].octave];
				break;
			}
		case 0x10:	case 0x11:	case 0x12:	/* Octave Select */
			{
				Bit8u chan=(cms->reg-0x10)*2;
				cms->chan[chan].octave=val&7;
				cms->chan[chan].freq_add=freq_table[cms->chan[chan].freq][cms->chan[chan].octave];
				chan++;
				cms->chan[chan].octave=(val>>4)&7;
				cms->chan[chan].freq_add=freq_table[cms->chan[chan].freq][cms->chan[chan].octave];
			}
			break;
		case 0x14:	/* Frequency enable */
			cms->voice_enabled=val;
			//TODO Check for enabling of speaker maybe 
			break;
		case 0x15:	/* Noise Enable */
			cms->noise_enabled=val;
			//TODO Check for enabling of speaker maybe 
			break;
		case 0x16:	/* Noise generator setup */	
			cms->noise[0].freq_add=noise_freq[val & 3];
			cms->noise[1].freq_add=noise_freq[(val>>4) & 3];
			break;
		default:
			LOG_ERROR("CMS %d:Illegal register %X2 Selected for write",sel,cms->reg);
			break;
		};
		break;
	}

};

static void CMS_CallBack(Bit8u * stream,Bit32u len) {
	/* Generate the CMS wave */
	/* Generate 12 channels of sound data this could be nice */
	for (Bit32u l=0;l<len;l++) {
		register Bit32s left,right;
		left=right=0;
		for (int c=0;c<2;c++) {
			CMS * cms=&cms_block[c];
			Bit8u use_voice=1;
			for (int chan=0;chan<6;chan++) {
				if (cms->noise_enabled & use_voice) {

				} else if (cms->voice_enabled & use_voice) {
					int pos=cms->chan[chan].freq_pos>>FREQ_SHIFT;
					left+=cms->chan[chan].vol_left[pos];
					right+=cms->chan[chan].vol_right[pos];
					cms->chan[chan].freq_pos+=cms->chan[chan].freq_add;
					if (cms->chan[chan].freq_pos>=SIN_MAX) 
						cms->chan[chan].freq_pos-=SIN_MAX;
				}
				use_voice<<=1;
			}
		}
		if (left>MAX_AUDIO) *(Bit16s *)stream=MAX_AUDIO;
		else if (left<MIN_AUDIO) *(Bit16s *)stream=MIN_AUDIO;
		else *(Bit16s *)stream=(Bit16s)left;
		stream+=2;

		if (right>MAX_AUDIO) *(Bit16s *)stream=MAX_AUDIO;
		else if (right<MIN_AUDIO) *(Bit16s *)stream=MIN_AUDIO;
		else *(Bit16s *)stream=(Bit16s)right;
		stream+=2;

	}

}

static HWBlock hw_cms;
static bool cms_enabled;

static void CMS_Enable(bool enable) {
	if (enable) {
		cms_enabled=true;
		MIXER_Enable(cms_chan,true);
		IO_RegisterWriteHandler(0x220,write_cms,"CMS");
		IO_RegisterWriteHandler(0x221,write_cms,"CMS");
		IO_RegisterWriteHandler(0x222,write_cms,"CMS");
		IO_RegisterWriteHandler(0x223,write_cms,"CMS");
	} else {
		cms_enabled=false;
		MIXER_Enable(cms_chan,false);
		IO_FreeWriteHandler(0x220);
		IO_FreeWriteHandler(0x221);
		IO_FreeWriteHandler(0x222);
		IO_FreeWriteHandler(0x223);		
	}
}


static void CMS_InputHandler(char * line) {
	bool s_off=ScanCMDBool(line,"OFF");
	bool s_on=ScanCMDBool(line,"ON");
	char * rem=ScanCMDRemain(line);
	if (rem) {
		sprintf(line,"Illegal Switch");
		return;
	}
	if (s_on && s_off) {
		sprintf(line,"Can't use /ON and /OFF at the same time");
		return;
	}
	if (s_on) {
		CMS_Enable(true);
		sprintf(line,"Creative Music System has been enabled");
		return;
	} 
	if (s_off) {
		CMS_Enable(false);
		sprintf(line,"Creative Music System has been disabled");
		return;
	} 
	return;
}

static void CMS_OutputHandler (char * towrite) {
	if(cms_enabled) {
		sprintf(towrite,"IO %X",0x220);
	} else {
		sprintf(towrite,"Disabled");
	}
};


void CMS_Init(void) {
	Bits i;
/* Register the Mixer CallBack */
	cms_chan=MIXER_AddChannel(CMS_CallBack,CMS_RATE,"CMS");
	MIXER_SetMode(cms_chan,MIXER_16STEREO);
	MIXER_Enable(cms_chan,false);
/* Register with the hardware setup tool */
	hw_cms.dev_name="CMS";
	hw_cms.full_name="Creative Music System";
	hw_cms.next=0;
	hw_cms.help="/ON  Enables CMS\n/OFF Disables CMS\n";	
	hw_cms.get_input=CMS_InputHandler;
	hw_cms.show_status=CMS_OutputHandler;
	HW_Register(&hw_cms);
	CMS_Enable(false);
/* Make the frequency/octave table */ 
	double log_start=log10(27.34375);
	double log_add=(log10(54.609375)-log10(27.34375))/256;
	for (i=0;i<256;i++) {
		double freq=pow(10,log_start);
		for (int k=0;k<8;k++) {
			freq_table[i][k]=(Bit32u)((double)SIN_MAX/(CMS_RATE/freq));
			freq*=2;
		}
		log_start+=log_add;
	}
//	noise_freq[0]=(Bit32u)(FREQ_MAX/((float)CMS_RATE/(float)28000));
//	noise_freq[1]=(Bit32u)(FREQ_MAX/((float)CMS_RATE/(float)14000));
//	noise_freq[2]=(Bit32u)(FREQ_MAX/((float)CMS_RATE/(float)6800));	
	for (int s=0;s<SIN_ENT;s++) {
		double out=sin( (2*PI/SIN_ENT)*s)*CMS_VOLUME;	
		for (i=15;i>=0;i--) {
			sin_table[i][s]=(Bit16s)out;
//			out /= (float)1.258925412;	/* = 10 ^ (2/20) = 2dB */
			out /= 1.1;
		}
	}


}

