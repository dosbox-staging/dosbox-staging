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
#include <math.h> 
#include "dosbox.h"
#include "inout.h"
#include "mixer.h"
#include "dma.h"
#include "pic.h"
#include "hardware.h"
#include "setup.h"
#include "support.h"
#include "programs.h"

#define SB_PIC_EVENTS 0

#define DSP_MAJOR 3
#define DSP_MINOR 1

#define MIXER_INDEX 0x04
#define MIXER_DATA 0x05

#define DSP_RESET 0x06
#define DSP_READ_DATA 0x0A
#define DSP_WRITE_DATA 0x0C
#define DSP_WRITE_STATUS 0x0C
#define DSP_READ_STATUS 0x0E

#define DSP_NO_COMMAND 0

#define DMA_BUFSIZE 4096
#define DSP_BUFSIZE 64
#define DSP_DACSIZE 4096

//Should be enough for sound generated in millisecond blocks
#define SB_BUF_SIZE 8096

enum {DSP_S_RESET,DSP_S_NORMAL,DSP_S_HIGHSPEED};
enum SB_TYPES {SBT_NONE=0,SBT_1=1,SBT_PRO1=2,SBT_2=3,SBT_PRO2=4,SBT_16=6};
enum SB_IRQS {SB_IRQ_8,SB_IRQ_16,SB_IRQ_MPU};

enum DSP_MODES {
	MODE_NONE,MODE_DAC,
	MODE_SILENCE,
	MODE_DMA,MODE_DMA_PAUSE,MODE_DMA_WAIT,
};

enum DMA_MODES {
	DSP_DMA_NONE,
	DSP_DMA_2,DSP_DMA_3,DSP_DMA_4,DSP_DMA_8,
	DSP_DMA_16,DSP_DMA_16_ALIASED,
};

enum {
	PLAY_MONO,PLAY_STEREO,
};

struct SB_INFO {
	Bit16u freq;
	struct {
		bool stereo,filtered,sign,autoinit;
		bool stereoremain;
		DMA_MODES mode;
		Bitu previous;
		Bitu total,left;
		Bitu rate,rate_mul;
		Bitu index,add_index;
		Bit64u start;
		union {
			Bit8u  b8[DMA_BUFSIZE];
			Bit16s b16[DMA_BUFSIZE];
		} buf;
		Bitu bits;
		DmaChannel * chan;
		struct {
			Bit16s stereo;
		} remain;
		
	} dma;
	bool speaker;
	Bit8u time_constant;
	DSP_MODES mode;
	SB_TYPES type;
	OPL_Mode oplmode;
	struct {
		bool pending_8bit;
		bool pending_16bit;
	} irq;
	struct {
		Bit8u state;
		Bit8u cmd;
		Bit8u cmd_len;
		Bit8u cmd_in_pos;
		Bit8u cmd_in[DSP_BUFSIZE];
		struct {
			Bit8u data[DSP_BUFSIZE];
			Bitu pos,used;
		} in,out;
		Bit8u test_register;
		Bitu write_busy;
	} dsp;
	struct {
		Bit16s data[DSP_DACSIZE];
		Bitu used;
		Bit16s last;
	} dac;
	struct {
		Bit8u index;
		Bit8u dac[2],fm[2],cda[2],master[2],lin[2];
		Bit8u mic;
		bool enabled;
	} mixer;
	struct {
		Bit8u reference;
		Bits stepsize;
		bool haveref;
	} adpcm;
	struct {
		Bitu base;
		Bit8u irq;
		Bit8u dma8,dma16;
		Bitu rate;
		Bitu rate_conv;
	} hw;
	struct {
		Bit16s buf[SB_BUF_SIZE][2];
		Bitu pos;
	} out;
	struct {
		union {
			Bit16s m[DMA_BUFSIZE];
			Bit16s s[DMA_BUFSIZE][2];
		} buf;
		
		Bitu index,add_index;
	} tmp;
	struct {
		Bits value;
		Bitu count;
	} e2;
	MIXER_Channel * chan;
};



static SB_INFO sb;

static char * copyright_string="COPYRIGHT (C) CREATIVE TECHNOLOGY LTD, 1992.";

static Bit8u DSP_cmd_len[256] = {
  0,0,0,0, 0,2,0,0, 0,0,0,0, 0,0,0,0,  // 0x00
  1,0,0,0, 2,0,2,2, 0,0,0,0, 0,0,0,0,  // 0x10
  0,0,0,0, 2,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x20
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x30

  1,2,2,0, 0,0,0,0, 2,0,0,0, 0,0,0,0,  // 0x40
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x50
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x60
  0,0,0,0, 2,2,2,2, 0,0,0,0, 0,0,0,0,  // 0x70

  2,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x80
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x90
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0xa0
  3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3,  // 0xb0

  3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3,  // 0xc0
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0xd0
  1,0,1,0, 1,0,0,0, 0,0,0,0, 0,0,0,0,  // 0xe0
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0   // 0xf0
};

static int E2_incr_table[4][9] = {
  {  0x01, -0x02, -0x04,  0x08, -0x10,  0x20,  0x40, -0x80, -106 },
  { -0x01,  0x02, -0x04,  0x08,  0x10, -0x20,  0x40, -0x80,  165 },
  { -0x01,  0x02,  0x04, -0x08,  0x10, -0x20, -0x40,  0x80, -151 },
  {  0x01, -0x02,  0x04, -0x08, -0x10,  0x20, -0x40,  0x80,   90 }
};

#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif
#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif

static void DSP_ChangeMode(DSP_MODES mode);
static void CheckDMAEnd(void);
static void END_DMA_Event(void);

static void DSP_SetSpeaker(bool how) {
	MIXER_Enable(sb.chan,how);
	sb.speaker=how;
}

static INLINE void SB_RaiseIRQ(SB_IRQS type) {
	LOG(LOG_SB,LOG_NORMAL)("Raising IRQ");
	PIC_ActivateIRQ(sb.hw.irq);
	switch (type) {
	case SB_IRQ_8:
		sb.irq.pending_8bit=true;
		break;
	case SB_IRQ_16:
		sb.irq.pending_16bit=true;
		break;
	}
}


static INLINE void DSP_FlushData(void) {
	sb.dsp.out.used=0;
	sb.dsp.out.pos=0;
}

static void DSP_StopDMA(void) {
	DSP_ChangeMode(MODE_NONE);
	sb.dma.left=0;
}

static void DSP_DMA_CallBack(DmaChannel * chan, DMAEvent event) {
	if (event==DMA_REACHED_TC) return;
	else if (event==DMA_MASKED) {
		if (sb.mode==MODE_DMA) {
			DSP_ChangeMode(MODE_DMA_WAIT);
			LOG(LOG_SB,LOG_NORMAL)("DMA deactivated,stopping output");
		}
	} else if (event==DMA_UNMASKED) {
		if (sb.mode==MODE_DMA_WAIT) {
			DSP_ChangeMode(MODE_DMA);
			CheckDMAEnd();
			LOG(LOG_SB,LOG_NORMAL)("DMA Activated,starting output, block %X",chan->basecnt);
		} else if (event==DMA_TRANSFEREND) {
			if (sb.mode==MODE_DMA) sb.mode=MODE_DMA_WAIT;
		}
	}
}


#define MIN_ADAPTIVE_STEP_SIZE 511
#define MAX_ADAPTIVE_STEP_SIZE 32767
#define DC_OFFSET_FADE 254

static INLINE Bits Clip(Bits sample) {
	if (sample>MAX_AUDIO) return MAX_AUDIO;
	if (sample<MIN_AUDIO) return MIN_AUDIO;
	return sample;
}

static INLINE Bit16s decode_ADPCM_4_sample(Bit8u sample,Bit8u & reference,Bits& scale) {
	static Bits scaleMap[8] = { -2, -1, 0, 0, 1, 1, 1, 1 };

	if (sample & 0x08) {
		reference = max(0x00, reference - ((sample & 0x07) << scale));
	} else {
		reference = min(0xff, reference + ((sample & 0x07) << scale));
	}
	scale = max(2, min(6, scaleMap[sample & 0x07]));
	return (((Bit8s)reference)^0x80)<<8;
}

static INLINE Bit16s decode_ADPCM_2_sample(Bit8u sample,Bit8u & reference,Bits& scale) {
	static Bits scaleMap[8] = { -2, -1, 0, 0, 1, 1, 1, 1 };

	if (sample & 0x02) {
		reference = max(0x00, reference - ((sample & 0x01) << (scale+2)));
	} else {
		reference = min(0xff, reference + ((sample & 0x01) << (scale+2)));
	}
	scale = max(2, min(6, scaleMap[sample & 0x07]));
	return (((Bit8s)reference)^0x80)<<8;
}

INLINE Bit16s decode_ADPCM_3_sample(Bit8u sample,Bit8u & reference,Bits& scale) {
	static Bits scaleMap[8] = { -2, -1, 0, 0, 1, 1, 1, 1 };

	if (sample & 0x04) {
		reference = max(0x00, reference - ((sample & 0x03) << (scale+1)));
	} else {
		reference = min(0xff, reference + ((sample & 0x03) << (scale+1)));
	}
	scale = max(2, min(6, scaleMap[sample & 0x07]));
	return (((Bit8s)reference)^0x80)<<8;
}

static void GenerateDMASound(Bitu size) {
	/* Check some variables */
	if (!size) return;
	if (!sb.dma.rate) return;
	/* First check if this transfer is gonna end in the next 2 milliseconds */
	Bitu index=(Bitu)(((float)sb.dma.left*(float)1000000)/(float)sb.dma.rate);
#if (SB_PIC_EVENTS)
	if (index-PIC_Index()<1000) PIC_AddEvent(END_DMA_Event,index);
#else 
	if (index<2000) size=sb.dma.left;
#endif
	Bitu read=sb.dma.chan->Read(size,sb.dma.buf.b8);
	sb.dma.left-=read;
	Bitu skip=0;Bitu i;Bitu rem;Bitu done=0;
	if (!read) {
		sb.mode=MODE_DMA_WAIT;
		return;
	}
	if (sb.dma.stereoremain) {
		sb.dma.stereoremain=false;
		sb.tmp.buf.m[done++]=sb.dma.remain.stereo; 
	}
	switch (sb.dma.mode) {
	case DSP_DMA_2:
		if (sb.adpcm.haveref) {
			sb.adpcm.haveref=false;
			sb.adpcm.reference=sb.dma.buf.b8[0];
			sb.adpcm.stepsize=MIN_ADAPTIVE_STEP_SIZE;
			skip++;read--;
		}
		for (i=0;i<read;i++) {
			sb.tmp.buf.m[done++]=decode_ADPCM_2_sample((sb.dma.buf.b8[skip+i] >> 6) & 0x3,sb.adpcm.reference,sb.adpcm.stepsize);
			sb.tmp.buf.m[done++]=decode_ADPCM_2_sample((sb.dma.buf.b8[skip+i] >> 4) & 0x3,sb.adpcm.reference,sb.adpcm.stepsize);
			sb.tmp.buf.m[done++]=decode_ADPCM_2_sample((sb.dma.buf.b8[skip+i] >> 2) & 0x3,sb.adpcm.reference,sb.adpcm.stepsize);
			sb.tmp.buf.m[done++]=decode_ADPCM_2_sample((sb.dma.buf.b8[skip+i] >> 0) & 0x3,sb.adpcm.reference,sb.adpcm.stepsize);
		}
		break;
	case DSP_DMA_3:
		if (sb.adpcm.haveref) {
			sb.adpcm.haveref=false;
			sb.adpcm.reference=sb.dma.buf.b8[0];
			sb.adpcm.stepsize=MIN_ADAPTIVE_STEP_SIZE;
			skip++;read--;
		}
		rem=read%3;read/=3;
		for (i=0;i<read;i++) {
			sb.tmp.buf.m[done++]=decode_ADPCM_3_sample( (sb.dma.buf.b8[skip+i*3]     )&7,sb.adpcm.reference,sb.adpcm.stepsize);
			sb.tmp.buf.m[done++]=decode_ADPCM_3_sample( (sb.dma.buf.b8[skip+i*3]  >>3)&7,sb.adpcm.reference,sb.adpcm.stepsize);
			sb.tmp.buf.m[done++]=decode_ADPCM_3_sample(((sb.dma.buf.b8[skip+i*3]  >>6)&3)&((sb.dma.buf.b8[i*3+1]&1)<<2),sb.adpcm.reference,sb.adpcm.stepsize);
			sb.tmp.buf.m[done++]=decode_ADPCM_3_sample( (sb.dma.buf.b8[skip+i*3+1]>>1)&7,sb.adpcm.reference,sb.adpcm.stepsize);
			sb.tmp.buf.m[done++]=decode_ADPCM_3_sample( (sb.dma.buf.b8[skip+i*3+1]>>4)&7,sb.adpcm.reference,sb.adpcm.stepsize);
			sb.tmp.buf.m[done++]=decode_ADPCM_3_sample(((sb.dma.buf.b8[skip+i*3+1]>>7)&1)&((sb.dma.buf.b8[i*3+2]&3)<<1),sb.adpcm.reference,sb.adpcm.stepsize);
			sb.tmp.buf.m[done++]=decode_ADPCM_3_sample( (sb.dma.buf.b8[skip+i*3+2]>>2)&7,sb.adpcm.reference,sb.adpcm.stepsize);
			sb.tmp.buf.m[done++]=decode_ADPCM_3_sample( (sb.dma.buf.b8[skip+i*3+2]>>5)&7,sb.adpcm.reference,sb.adpcm.stepsize);
		}
		if (rem) {
			sb.tmp.buf.m[done++]=decode_ADPCM_3_sample((sb.dma.buf.b8[skip+read*3])&7,sb.adpcm.reference,sb.adpcm.stepsize);
			sb.tmp.buf.m[done++]=decode_ADPCM_3_sample((sb.dma.buf.b8[skip+read*3]>>3)&7,sb.adpcm.reference,sb.adpcm.stepsize);
			if (rem==2) {
				sb.tmp.buf.m[done++]=decode_ADPCM_3_sample(((sb.dma.buf.b8[skip+read*3+1]>>6)&3)&(sb.dma.buf.b8[read*3/8]&1),     sb.adpcm.reference,sb.adpcm.stepsize);
				sb.tmp.buf.m[done++]=decode_ADPCM_3_sample((sb.dma.buf.b8[skip+read*3+1]>>1)&7,sb.adpcm.reference,sb.adpcm.stepsize);
				sb.tmp.buf.m[done++]=decode_ADPCM_3_sample((sb.dma.buf.b8[skip+read*3+1]>>4)&7,sb.adpcm.reference,sb.adpcm.stepsize);
			}
		}
		break;
	case DSP_DMA_4:
		if (sb.adpcm.haveref) {
			sb.adpcm.haveref=false;
			sb.adpcm.reference=sb.dma.buf.b8[0];
			sb.adpcm.stepsize=MIN_ADAPTIVE_STEP_SIZE;
			skip++;read--;
		}
		for (i=0;i<read;i++) {
			sb.tmp.buf.m[done++]=decode_ADPCM_4_sample(sb.dma.buf.b8[skip+i] >> 4,sb.adpcm.reference,sb.adpcm.stepsize);
			sb.tmp.buf.m[done++]=decode_ADPCM_4_sample(sb.dma.buf.b8[skip+i]& 0xf,sb.adpcm.reference,sb.adpcm.stepsize);
		}
		read*=2;
		break;
	case DSP_DMA_8:
		for (i=0;i<read;i++) sb.tmp.buf.m[done++]=((Bit8s)(sb.dma.buf.b8[i]^0x80))<<8;
		break;
	case DSP_DMA_16:
		for (i=0;i<read;i++) sb.tmp.buf.m[done++]=sb.dma.buf.b16[i];
		break;
	case DSP_DMA_16_ALIASED:
		for (i=0;i<read/2;i++) sb.tmp.buf.m[done++]=sb.dma.buf.b16[i];
		break;
	default:
		LOG_MSG("Unhandled dma mode %d",sb.dma.mode);
		sb.mode=MODE_NONE;
		return;
	}
	if (!sb.dma.left) {
		if (!sb.dma.autoinit) sb.mode=MODE_NONE;
		else sb.dma.left=sb.dma.total;
		if (sb.dma.mode >= DSP_DMA_16) SB_RaiseIRQ(SB_IRQ_16);
		else SB_RaiseIRQ(SB_IRQ_8);
	}
	Bit16s * stream=&sb.out.buf[sb.out.pos][0];
	if (!sb.dma.stereo) {
		Bitu pos;
		while (done>(pos=sb.tmp.index>>16)) {
			(*stream++)=sb.tmp.buf.m[pos];
			(*stream++)=sb.tmp.buf.m[pos];
			sb.tmp.index+=sb.tmp.add_index;
			sb.out.pos++;
		}
		sb.tmp.index&=0xffff;
	} else {
		if (done&1){
			sb.dma.remain.stereo=sb.tmp.buf.m[done-1];
			sb.dma.stereoremain=true;
		}
		Bitu pos;done>>=1;Bitu index_add=sb.tmp.add_index >> 1;
		while (done>(pos=sb.tmp.index>>16)) {
			(*stream++)=sb.tmp.buf.s[pos][0];
			(*stream++)=sb.tmp.buf.s[pos][1];
			sb.tmp.index+=index_add;
			sb.out.pos++;
		}
		sb.tmp.index&=0xffff;
	}
}

static void GenerateSound(Bitu size) {
	while (sb.out.pos<size) {	
		Bitu samples=size-sb.out.pos;
		switch (sb.mode) {
		case MODE_DMA_WAIT:
		case MODE_DMA_PAUSE:
		case MODE_NONE:
			memset(&sb.out.buf[sb.out.pos],0,samples*4);
			sb.out.pos+=samples;
			break;
		case MODE_DAC:
			/* Stretch the inputted dac data over len samples */
			{

				Bit16s * stream=&sb.out.buf[sb.out.pos][0];
				if (sb.dac.used) {
					Bitu dac_add=(sb.dac.used<<16)/samples;
					Bitu dac_pos=0;
					Bitu len=samples;
					while (len-->0) {
						*(stream++)=sb.dac.data[dac_pos>>16];
						*(stream++)=sb.dac.data[dac_pos>>16];
						dac_pos+=dac_add;
					}
					dac_pos-=dac_add;
					sb.dac.last=sb.dac.data[dac_pos>>16];
					sb.dac.used=0;
				} else {
					memset(stream,sb.dac.last,samples);
					sb.mode=MODE_NONE;
				}
				sb.out.pos+=samples;
				break;
			}
		case MODE_DMA:
			{
				Bitu len=samples*sb.dma.rate_mul;
				if (len & 0xffff) len=1+(len>>16);
				else len>>=16;
				if (sb.dma.stereo && (len & 1)) len++;
				GenerateDMASound(len);
				break;	
			}
			
		}
	}
	if (sb.out.pos>SB_BUF_SIZE) {
		LOG(LOG_SB,LOG_ERROR)("Generation Buffer Full!!!!");
		sb.out.pos=0;
	}
}

static void END_DMA_Event(Bitu val) {
	GenerateDMASound(sb.dma.left);
}

static void CheckDMAEnd(void) {
	if (!sb.dma.rate) return;
	Bitu index=(Bitu)(((float)sb.dma.left*(float)1000000)/(float)sb.dma.rate);
	if (index<(1000-PIC_Index())) {
#if 1
		LOG(LOG_SB,LOG_NORMAL)("Sub millisecond transfer scheduling IRQ in %d microseconds",index);	
		PIC_AddEvent(END_DMA_Event,index);
#else
		GenerateDMASound(sb.dma.left);		
#endif
	}
}

static void DSP_ChangeMode(DSP_MODES mode) {
	if (!sb.speaker) DSP_SetSpeaker(true);
	if (mode==sb.mode) return;
	/* Generate sound until now */
	Bitu index=PIC_Index();
	GenerateSound((sb.hw.rate_conv*index)>>16);
	sb.mode=mode;
}

static void DSP_RaiseIRQEvent(Bitu val) {
	SB_RaiseIRQ(SB_IRQ_8);
}

static void DSP_DoDMATranfser(DMA_MODES mode) {
	char * type;
	DSP_ChangeMode(MODE_NONE);
	sb.dma.left=sb.dma.total;
	sb.dma.mode=mode;
	sb.tmp.index=0;
	sb.dma.rate_mul=(sb.dma.rate<<16)/sb.hw.rate;
	sb.tmp.add_index=(sb.dma.rate<<16)/sb.hw.rate;
	switch (mode) {
	case DSP_DMA_2:type="2-bits ADPCM";break;
	case DSP_DMA_3:type="3-bits ADPCM";break;
	case DSP_DMA_4:type="4-bits ADPCM";break;
	case DSP_DMA_8:type="8-bits PCM";break;
	case DSP_DMA_16_ALIASED:type="16-bits(aliased) PCM";break;
	case DSP_DMA_16:type="16-bits PCM";break;
	default:
		LOG(LOG_SB,LOG_ERROR)("DSP:Illegal transfer mode %d",mode);
		return;
	}
	DSP_ChangeMode(MODE_DMA_WAIT);
	sb.dma.mode=mode;
	sb.dma.chan->Register_Callback(DSP_DMA_CallBack);
#if (C_DEBUG)
	LOG(LOG_SB,LOG_NORMAL)("DMA Transfer:%s %s %s dma-rate %d size %d",
		type,
		sb.dma.stereo ? "Stereo" : "Mono",
		sb.dma.autoinit ? "Auto-Init" : "Single-Cycle",
		sb.dma.rate,sb.dma.total
	);
#endif
}

static void DSP_PrepareDMA_Old(DMA_MODES mode,bool autoinit) {
	sb.dma.autoinit=autoinit;
	if (!autoinit) sb.dma.total=1+sb.dsp.in.data[0]+(sb.dsp.in.data[1] << 8);
	sb.dma.rate=sb.freq;
	sb.dma.chan=DmaChannels[sb.hw.dma8];
	DSP_DoDMATranfser(mode);
}

static void DSP_PrepareDMA_New(DMA_MODES mode,bool autoinit,Bitu length) {
	sb.dma.total=length;
	sb.dma.rate=sb.freq * (sb.dma.stereo ? 2 : 1);
	sb.dma.rate_mul=(sb.dma.rate<<16)/sb.hw.rate;
	sb.dma.autoinit=autoinit;
	if (mode==DSP_DMA_16) {
		if (sb.hw.dma16!=0xff) sb.dma.chan=DmaChannels[sb.hw.dma16];
		else {
			sb.dma.chan=DmaChannels[sb.hw.dma8];
			mode=DSP_DMA_16_ALIASED;
		}
	} else sb.dma.chan=DmaChannels[sb.hw.dma8];
	DSP_DoDMATranfser(mode);
}


static void DSP_AddData(Bit8u val) {
	if (sb.dsp.out.used<DSP_BUFSIZE) {
		Bitu start=sb.dsp.out.used+sb.dsp.out.pos;
		if (start>=DSP_BUFSIZE) start-=DSP_BUFSIZE;
		sb.dsp.out.data[start]=val;
		sb.dsp.out.used++;
	} else {
		LOG(LOG_SB,LOG_ERROR)("DSP:Data Output buffer full");
	}
}


static void DSP_Reset(void) {
	LOG(LOG_SB,LOG_ERROR)("DSP:Reset");
	PIC_DeActivateIRQ(sb.hw.irq);
	DSP_ChangeMode(MODE_NONE);
	sb.dsp.cmd_len=0;
	sb.dsp.in.pos=0;
	sb.dsp.write_busy=0;
	sb.dma.left=0;
	sb.dma.total=0;
	sb.dma.stereo=false;
	sb.dma.autoinit=false;
	sb.freq=22050;
	sb.time_constant=45;
	sb.dac.used=0;
	sb.dac.last=0;
	sb.e2.value=0xaa;
	sb.e2.count=0;
	sb.irq.pending_8bit=false;
	sb.irq.pending_16bit=false;
	DSP_SetSpeaker(false);
}


static void DSP_DoReset(Bit8u val) {
	if ((val&1)!=0) {
//TODO Get out of highspeed mode
		DSP_Reset();
		sb.dsp.state=DSP_S_RESET;
	} else {
		DSP_FlushData();
		DSP_AddData(0xaa);
		sb.dsp.state=DSP_S_NORMAL;
	}
}

static void DSP_E2_DMA_CallBack(DmaChannel * chan, DMAEvent event) {
	if (event==DMA_UNMASKED) {
		Bit8u val=sb.e2.value;
		DmaChannels[sb.hw.dma8]->Register_Callback(0);
		DmaChannels[sb.hw.dma8]->Write(1,&val);
	}
}

static void DSP_ADC_CallBack(DmaChannel * chan, DMAEvent event) {
	if (event!=DMA_UNMASKED) return;
	Bit8u val=128;
    while (sb.dma.left--) {
		DmaChannels[sb.hw.dma8]->Write(1,&val);
	}
	SB_RaiseIRQ(SB_IRQ_8);
	DmaChannels[sb.hw.dma8]->Register_Callback(0);
}

Bitu DEBUG_EnableDebugger(void);

static void DSP_DoCommand(void) {
//	LOG_MSG("DSP Command %X",sb.dsp.cmd);
	switch (sb.dsp.cmd) {
	case 0x04:	/* DSP Statues SB 2.0/pro version */
		DSP_FlushData();
		DSP_AddData(0xff);			//Everthing enabled
		break;
	case 0x10:	/* Direct DAC */
		DSP_ChangeMode(MODE_DAC);
		if (sb.dac.used<DSP_DACSIZE) {
			sb.dac.data[sb.dac.used++]=(Bit8s(sb.dsp.in.data[0] ^ 0x80)) << 8;
		}
		break;
	case 0x24:	/* Singe Cycle 8-Bit DMA ADC */
		sb.dma.left=sb.dma.total=1+sb.dsp.in.data[0]+(sb.dsp.in.data[1] << 8);
		LOG(LOG_SB,LOG_ERROR)("DSP:Faked ADC for %d bytes",sb.dma.total);
		DmaChannels[sb.hw.dma8]->Register_Callback(DSP_ADC_CallBack);
		break;
	case 0x14:	/* Singe Cycle 8-Bit DMA DAC */
	case 0x91:	/* Singe Cycle 8-Bit DMA High speed DAC */
		DSP_PrepareDMA_Old(DSP_DMA_8,false);
		break;
	case 0x90:	/* Auto Init 8-bit DMA High Speed */
	case 0x1c:	/* Auto Init 8-bit DMA */
		DSP_PrepareDMA_Old(DSP_DMA_8,true);
		break;
	case 0x40:	/* Set Timeconstant */
		sb.freq=(1000000 / (256 - sb.dsp.in.data[0]));
		break;
	case 0x41:	/* Set Output Samplerate */
	case 0x42:	/* Set Input Samplerate */
		sb.freq=(sb.dsp.in.data[0] << 8)  | sb.dsp.in.data[1];
		break;
	case 0x48:	/* Set DMA Block Size */
		//TODO Maybe check limit for new irq?
		sb.dma.total=1+sb.dsp.in.data[0]+(sb.dsp.in.data[1] << 8);
		break;
    case 0x75:	/* 075h : Single Cycle 4-bit ADPCM Reference */
		sb.adpcm.haveref=true;
    case 0x74:  /* 074h : Single Cycle 4-bit ADPCM */	
		DSP_PrepareDMA_Old(DSP_DMA_4,false);
		break;
	case 0x77:	/* 077h : Single Cycle 3-bit(2.6bit) ADPCM Reference*/
		sb.adpcm.haveref=true;
	case 0x76:  /* 074h : Single Cycle 3-bit(2.6bit) ADPCM */
		DSP_PrepareDMA_Old(DSP_DMA_3,false);
		break;
	case 0x17:	/* 017h : Single Cycle 2-bit ADPCM Reference*/
		sb.adpcm.haveref=true;
	case 0x16:  /* 074h : Single Cycle 2-bit ADPCM */
		DSP_PrepareDMA_Old(DSP_DMA_2,false);
		break;
	case 0x80:	/* Silence DAC */
		PIC_AddEvent(&DSP_RaiseIRQEvent,
			(1000000*(1+sb.dsp.in.data[0]+(sb.dsp.in.data[1] << 8))/sb.freq));
		break;
	case 0xb0:	case 0xb2:	case 0xb4:	case 0xb6:
    case 0xc0:	case 0xc2:	case 0xc4:	case 0xc6:
		/* Generic 8/16 bit DMA */
		sb.dma.stereo=(sb.dsp.in.data[0] & 0x20) > 0;
		sb.dma.sign=(sb.dsp.in.data[0] & 0x10) > 0;
		DSP_PrepareDMA_New((sb.dsp.cmd & 0x10) ? DSP_DMA_16 : DSP_DMA_8,
			(sb.dsp.cmd & 0x4)>0,
			1+sb.dsp.in.data[1]+(sb.dsp.in.data[2] << 8)
		);
		break;
	case 0xd0:	/* Halt 8-bit DMA */
	case 0xd5:	/* Halt 16-bit DMA */
		if (sb.dma.left) {
			DSP_ChangeMode(MODE_DMA_PAUSE);
#if SB_PIC_EVENTS
			PIC_RemoveEvents(END_DMA_Event);
#endif
		} else DSP_ChangeMode(MODE_NONE);
		break;
	case 0xd1:	/* Enable Speaker */
		DSP_SetSpeaker(true);
		break;
	case 0xd3:	/* Disable Speaker */
		DSP_SetSpeaker(false);
		break;
	case 0xd4:	/* Continue DMA */
		DSP_ChangeMode(MODE_DMA_WAIT);
		sb.dma.chan->Register_Callback(DSP_DMA_CallBack);
		break;
	case 0xda:	/* Exit Autoinitialize 8-bit */
		/* Set mode to single transfer so it ends with current block */
		sb.dma.autoinit=false;		//Should stop itself
		break;
	case 0xe0:	/* DSP Identification - SB2.0+ */
		DSP_FlushData();
		DSP_AddData(~sb.dsp.in.data[0]);
		break;
	case 0xe1:	/* Get DSP Version */
		DSP_FlushData();
		switch (sb.type) {
		case SBT_1:
			DSP_AddData(0x1);DSP_AddData(0x1);break;
		case SBT_2:
			DSP_AddData(0x2);DSP_AddData(0x1);break;
		case SBT_PRO1:
			DSP_AddData(0x3);DSP_AddData(0x0);break;
		case SBT_PRO2:
			DSP_AddData(0x3);DSP_AddData(0x2);break;
		case SBT_16:
			DSP_AddData(0x4);DSP_AddData(0x5);break;
		}
		break;
	case 0xe2:	/* Weird DMA identification write routine */
		{
			LOG(LOG_SB,LOG_NORMAL)("DSP Function 0xe2");
			for (Bitu i = 0; i < 8; i++)
				if ((sb.dsp.in.data[0] >> i) & 0x01) sb.e2.value += E2_incr_table[sb.e2.count % 4][i];
			 sb.e2.value += E2_incr_table[sb.e2.count % 4][8];
			 sb.e2.count++;
			 DmaChannels[sb.hw.dma8]->Register_Callback(DSP_E2_DMA_CallBack);
		}
		break;
	case 0xe3:	/* DSP Copyright */
		{
			DSP_FlushData();
			for (Bit32u i=0;i<=strlen(copyright_string);i++) {
				DSP_AddData(copyright_string[i]);
			}
		}
		break;
	case 0xe4:	/* Write Test Register */
		sb.dsp.test_register=sb.dsp.in.data[0];
		break;
	case 0xe8:	/* Read Test Register */
		DSP_FlushData();
		DSP_AddData(sb.dsp.test_register);;
		break;
	case 0xf2:	/* Trigger 8bit IRQ */
		SB_RaiseIRQ(SB_IRQ_8);
		break;
	default:
		LOG(LOG_SB,LOG_ERROR)("DSP:Unhandled command %2X",sb.dsp.cmd);
		break;
	}
	sb.dsp.cmd=DSP_NO_COMMAND;
	sb.dsp.cmd_len=0;
	sb.dsp.in.pos=0;
}

static void DSP_DoWrite(Bit8u val) {
	switch (sb.dsp.cmd) {
	case DSP_NO_COMMAND:
		sb.dsp.cmd=val;
		sb.dsp.cmd_len=DSP_cmd_len[val];
		sb.dsp.in.pos=0;
		if (!sb.dsp.cmd_len) DSP_DoCommand();
		break;
	default:
		sb.dsp.in.data[sb.dsp.in.pos]=val;
		sb.dsp.in.pos++;
		if (sb.dsp.in.pos>=sb.dsp.cmd_len) DSP_DoCommand();
	}
}

static Bit8u DSP_ReadData(void) {
	Bit8u data=0;	
	if (sb.dsp.out.used) {
		data=sb.dsp.out.data[sb.dsp.out.pos];
		sb.dsp.out.pos++;
		if (sb.dsp.out.pos>=DSP_BUFSIZE) sb.dsp.out.pos-=DSP_BUFSIZE;
		sb.dsp.out.used--;
	}
	return data;
}

//The soundblaster manual says 2.0 Db steps but we'll go for a bit less
#define CALCVOL(_VAL) (float)pow(10.0f,((float)(31-_VAL)*-1.3f)/20)
static void CTMIXER_UpdateVolumes(void) {
	if (!sb.mixer.enabled) return;
	MIXER_SetVolume(MIXER_FindChannel("SB"),CALCVOL(sb.mixer.dac[0]),CALCVOL(sb.mixer.dac[1]));
	MIXER_SetVolume(MIXER_FindChannel("FM"),CALCVOL(sb.mixer.fm[0]),CALCVOL(sb.mixer.fm[1]));
}

static void CTMIXER_Reset(void) {
	sb.mixer.fm[0]=
	sb.mixer.fm[1]=
	sb.mixer.dac[0]=
	sb.mixer.dac[1]=31;
	CTMIXER_UpdateVolumes();
}

#define SETPROVOL(_WHICH_,_VAL_)				\
	_WHICH_[0]= 0x1 | ((_VAL_ & 0xf0) >> 3);	\
	_WHICH_[1]= 0x1 | ((_VAL_ & 0x0f) << 1);

static void CTMIXER_Write(Bit8u val) {
	LOG_MSG("Write mixer %x %x",sb.mixer.index,val);
	switch (sb.mixer.index) {
	case 0x02:		/* Master Voulme (SBPRO) Obsolete? */
	case 0x22:		/* Master Volume (SBPRO) */
		SETPROVOL(sb.mixer.master,val);
		break;
	case 0x04:		/* DAC Volume (SBPRO) */
		SETPROVOL(sb.mixer.dac,val);
		CTMIXER_UpdateVolumes();
		break;
	case 0x06:		/* FM output selection, Somewhat obsolete with dual OPL SBpro */
		SETPROVOL(sb.mixer.fm,val);
		sb.mixer.fm[1]=sb.mixer.fm[0];
		CTMIXER_UpdateVolumes();
		//TODO Change FM Mode if only 1 fm channel is selected
		break;
	case 0x0a:		/* Mic Level */
		sb.mixer.mic=(val & 0xf) << 1;
		break;
	case 0x0e:		/* Output/Stereo Select */
		sb.dma.stereo=(val & 0x2) > 0;
		sb.dma.filtered=(val & 0x20) > 0;
		LOG(LOG_SB,LOG_WARN)("Mixer set to %s",sb.dma.stereo ? "STEREO" : "MONO");
		break;
	case 0x26:		/* FM Volume (SBPRO) */
		SETPROVOL(sb.mixer.fm,val);
		CTMIXER_UpdateVolumes();	
		break;
	case 0x28:		/* CD Audio Volume (SBPRO) */
		SETPROVOL(sb.mixer.cda,val);
		break;
	case 0x2e:		/* Line-IN Volume (SBPRO) */
		SETPROVOL(sb.mixer.lin,val);
		break;
	case 0x80:		/* IRQ Select */
		sb.hw.irq=0xff;
		if (val & 0x1) sb.hw.irq=2;
		else if (val & 0x2) sb.hw.irq=5;
		else if (val & 0x4) sb.hw.irq=7;
		else if (val & 0x8) sb.hw.irq=10;
		break;
	case 0x81:		/* DMA Select */
		sb.hw.dma8=0xff;
		sb.hw.dma16=0xff;
		if (val & 0x1) sb.hw.dma8=0;
		else if (val & 0x2) sb.hw.dma8=1;
		else if (val & 0x8) sb.hw.dma8=3;
		if (val & 0x20) sb.hw.dma16=5;
		else if (val & 0x40) sb.hw.dma16=6;
		else if (val & 0x80) sb.hw.dma16=7;
		LOG(LOG_SB,LOG_NORMAL)("Mixer select dma8:%x dma16:%x",sb.hw.dma8,sb.hw.dma16);
		break;
	default:
		LOG(LOG_SB,LOG_WARN)("MIXER:Write %X to unhandled index %X",val,sb.mixer.index);
	}
}

#define MAKEPROVOL(_WHICH_)			\
	(((_WHICH_[0] & 0x1e) << 3) | ((_WHICH_[1] & 0x1e) >> 1))
	
static Bit8u CTMIXER_Read(void) {
	Bit8u ret;
	if ( sb.mixer.index< 0x80) LOG_MSG("Read mixer %x",sb.mixer.index);
	switch (sb.mixer.index) {
	case 0x00:		/* RESET */
		return 0x00;
	case 0x02:		/* Master Voulme (SBPRO) Obsolete? */
	case 0x22:		/* Master Volume (SBPRO) */
		return	MAKEPROVOL(sb.mixer.master);
	case 0x04:		/* DAC Volume (SBPRO) */
        return	MAKEPROVOL(sb.mixer.dac);
//	case 0x06:		/* FM output selection, Somewhat obsolete with dual OPL SBpro */
	case 0x0a:		/* Mic Level (SBPRO) */
		return (sb.mixer.mic >> 1);
	case 0x0e:		/* Output/Stereo Select */
		return 0x11|(sb.dma.stereo ? 0x02 : 0x00)|(sb.dma.filtered ? 0x20 : 0x00);
	case 0x26:		/* FM Volume (SBPRO) */
		return MAKEPROVOL(sb.mixer.fm);
	case 0x28:		/* CD Audio Volume (SBPRO) */
		return MAKEPROVOL(sb.mixer.cda);
	case 0x2e:		/* Line-IN Volume (SBPRO) */
		return MAKEPROVOL(sb.mixer.lin);
	case 0x80:		/* IRQ Select */
		switch (sb.hw.irq) {
		case 2:  return 0x1;
		case 5:  return 0x2;
		case 7:  return 0x4;
		case 10: return 0x8;
		}
	case 0x81:		/* DMA Select */
		ret=0;
		switch (sb.hw.dma8) {
		case 0:ret|=0x1;break;
		case 1:ret|=0x2;break;
		case 3:ret|=0x8;break;
		}
		switch (sb.hw.dma16) {
		case 5:ret|=0x20;break;
		case 6:ret|=0x40;break;
		case 7:ret|=0x80;break;
		}
		return ret;
	case 0x82:
		return	(sb.irq.pending_8bit ? 0x1 : 0) |
				(sb.irq.pending_16bit ? 0x2 : 0);
	default:		/* IRQ Status */
		LOG(LOG_SB,LOG_WARN)("MIXER:Read from unhandled index %X",sb.mixer.index);
		ret=0xa;
	}
	return ret;
}


static Bitu read_sb(Bitu port,Bitu iolen) {
	switch (port-sb.hw.base) {
	case MIXER_INDEX:
		return sb.mixer.index;
	case MIXER_DATA:
		return CTMIXER_Read();
	case DSP_READ_DATA:
		return DSP_ReadData();
	case DSP_READ_STATUS:
		//TODO See for high speed dma :)
		sb.irq.pending_8bit=false;
		if (sb.dsp.out.used) return 0xff;
		else return 0x7f;
	case DSP_WRITE_STATUS:
		switch (sb.dsp.state) {
		case DSP_S_NORMAL:
			sb.dsp.write_busy++;
			if (sb.dsp.write_busy & 8) return 0xff;
			return 0x7f;
		case DSP_S_RESET:
			return 0xff;
		}
		return 0xff;
	case DSP_RESET:
		return 0xff;
	default:
		LOG(LOG_SB,LOG_NORMAL)("Unhandled read from SB Port %4X",port);
		break;
	}
	return 0xff;
}

static void write_sb(Bitu port,Bitu val,Bitu iolen) {
	switch (port-sb.hw.base) {
	case DSP_RESET:
		DSP_DoReset(val);
		break;
	case DSP_WRITE_DATA:
		DSP_DoWrite(val);
		break;
	case MIXER_INDEX:
		sb.mixer.index=val;
		break;
	case MIXER_DATA:
		CTMIXER_Write(val);
		break;
	default:
		LOG(LOG_SB,LOG_NORMAL)("Unhandled write to SB Port %4X",port);
		break;
	}
}

static void adlib_gusforward(Bitu port,Bitu val,Bitu iolen) {
	adlib_commandreg=val;
}

static void SBLASTER_CallBack(Bit8u * stream,Bit32u len) {
	if (!len) return;
	GenerateSound(len);
	memcpy(stream,sb.out.buf,len*4);
	if (sb.out.pos>=len) {
		memcpy(sb.out.buf,&sb.out.buf[len],(sb.out.pos-len)*4);	
		sb.out.pos-=len;
	}
	else sb.out.pos=0;
	if (sb.mode==MODE_NONE) DSP_SetSpeaker(false);
	return;
}


void SBLASTER_Init(Section* sec) {
	Bitu i;
	Section_prop * section=static_cast<Section_prop *>(sec);
	const char * sbtype=section->Get_string("type");
	sb.hw.base=section->Get_hex("base");
	sb.hw.irq=section->Get_int("irq");
	sb.hw.dma8=section->Get_int("dma");
	sb.hw.dma16=section->Get_int("hdma");
	sb.mixer.enabled=section->Get_bool("mixer");
	sb.hw.rate=section->Get_int("sbrate");
	sb.hw.rate_conv=(sb.hw.rate<<16)/1000000;
	if (!strcasecmp(sbtype,"sb1")) sb.type=SBT_1;
	else if (!strcasecmp(sbtype,"sb2")) sb.type=SBT_2;
	else if (!strcasecmp(sbtype,"sbpro1")) sb.type=SBT_PRO1;
	else if (!strcasecmp(sbtype,"sbpro2")) sb.type=SBT_PRO2;
	else if (!strcasecmp(sbtype,"sb16")) sb.type=SBT_16;
	else if (!strcasecmp(sbtype,"none")) sb.type=SBT_NONE;
	else sb.type=SBT_16;
	
	if (machine!=MCH_VGA && sb.type==SBT_16) sb.type=SBT_PRO2;

	/* OPL/CMS Init */
	const char * omode=section->Get_string("oplmode");
	Bitu oplrate=section->Get_int("oplrate");
	OPL_Mode opl_mode;
	if (!strcasecmp(omode,"none")) opl_mode=OPL_none;	
	else if (!strcasecmp(omode,"cms")) opl_mode=OPL_cms;
	else if (!strcasecmp(omode,"opl2")) opl_mode=OPL_opl2;
	else if (!strcasecmp(omode,"dualopl2")) opl_mode=OPL_dualopl2;
	else if (!strcasecmp(omode,"opl3")) opl_mode=OPL_opl3;
	/* Else assume auto */
	else {
		switch (sb.type) {
		case SBT_NONE:opl_mode=OPL_none;break;
		case SBT_1:opl_mode=OPL_opl2;break;
		case SBT_2:opl_mode=OPL_opl2;break;
		case SBT_PRO1:opl_mode=OPL_dualopl2;break;
		case SBT_PRO2:
		case SBT_16:
			opl_mode=OPL_opl3;break;
		}
	}
	switch (opl_mode) {
	case OPL_none:
		IO_RegisterWriteHandler(0x388,adlib_gusforward,IO_MB);
		break;
	case OPL_cms:
		IO_RegisterWriteHandler(0x388,adlib_gusforward,IO_MB);
		CMS_Init(section,sb.hw.base,oplrate);
		break;
	case OPL_opl2:
		CMS_Init(section,sb.hw.base,oplrate);
	case OPL_dualopl2:
	case OPL_opl3:
		OPL_Init(section,sb.hw.base,opl_mode,oplrate);
		break;
	}
	if (sb.type==SBT_NONE) return;
	sb.chan=MIXER_AddChannel(&SBLASTER_CallBack,22050,"SB");
	MIXER_Enable(sb.chan,false);
	sb.dsp.state=DSP_S_NORMAL;
	MIXER_SetFreq(sb.chan,sb.hw.rate);
	MIXER_SetMode(sb.chan,MIXER_16STEREO);

	for (i=4;i<0xf;i++) {
		if (i==8 || i==9) continue;
		//Disable mixer ports for lower soundblaster
		if (sb.type<SBT_PRO1 && (i==4 || i==5)) continue; 
		IO_RegisterReadHandler(sb.hw.base+i,read_sb,IO_MB);
		IO_RegisterWriteHandler(sb.hw.base+i,write_sb,IO_MB);
	}
	DSP_Reset();
	CTMIXER_Reset();
	char hdma[8]="";
	if (sb.type==SBT_16) {
		sprintf(hdma,"H%d ",sb.hw.dma16);
	}
	SHELL_AddAutoexec("SET BLASTER=A%3X I%d D%d %sT%d",sb.hw.base,sb.hw.irq,sb.hw.dma8,hdma,sb.type);
}

