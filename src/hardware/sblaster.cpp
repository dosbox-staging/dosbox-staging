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

#include <string.h>
#include "dosbox.h"
#include "inout.h"
#include "mixer.h"
#include "dma.h"
#include "pic.h"
#include "hardware.h"

#define SB_BASE 0x220
#define SB_IRQ 5
#define SB_DMA 1

#define DSP_RESET 0x06
#define DSP_READ_DATA 0x0A
#define DSP_WRITE_DATA 0x0C
#define DSP_WRITE_STATUS 0x0C
#define DSP_READ_STATUS 0x0E

#define DSP_NO_COMMAND 0

#define DSP_MAJOR 2
#define DSP_MINOR 0

#define DSP_BUFSIZE 64
#define DSP_DACSIZE 4096

enum {DSP_S_RESET,DSP_S_NORMAL,DSP_S_HIGHSPEED};
enum {
	MODE_NONE,MODE_DAC,
	MODE_PCM_8S,MODE_PCM_8A,
	MODE_ADPCM_4S
};

#if DEBUG_SBLASTER
#define SB_DEBUG LOG_DEBUG
#else
#define SB_DEBUG
#endif


struct SB_INFO {
	Bit16u freq;
	Bitu samples_total;
	Bitu samples_left;
	bool speaker;
	Bit8u time_constant;
	bool use_time_constant;

	Bit8u output_mode;
	/* DSP Stuff */
	Bit8u mode;
	Bit8u state;
	Bit8u cmd;
	Bit8u cmd_len;
	Bit8u cmd_in_pos;
	Bit8u cmd_in[DSP_BUFSIZE];
	Bit8u data_out[DSP_BUFSIZE];
	Bit8u data_out_pos;
	Bit8u data_out_used;
	struct {
		Bit8u data[DSP_DACSIZE];
		Bitu used;
		Bit8u last;
	} dac;
	Bit8u test_register;
/*ADPCM Part */
	Bits adpcm_reference;
	Bits adpcm_scale;
	Bits adpcm_remain;
/* Hardware setup part */
	Bit32u base;
	Bit8u irq;
	Bit8u dma;
	bool enabled;
	HWBlock hwblock;
	MIXER_Channel * chan;
};

static SB_INFO sb;
static Bit8u e2_value;
static Bit8u e2_count;

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

static void DSP_SetSpeaker(bool how) {
/* This should just set the mixer value */
	MIXER_Enable(sb.chan,how);
	sb.speaker=how;
}

static void DSP_HaltDMA(void) {

}

static INLINE void DSP_FlushData(void) {
	sb.data_out_used=0;
	sb.data_out_pos=0;
}

static void DSP_SetSampleRate(Bit32u rate) {
/* This directly changes the mixer */

	
}
static void DSP_StopDMA(void) {
	sb.mode=MODE_NONE;
//	MIXER_SetMode(sb.chan,MIXER_8MONO);
//	MIXER_SetFreq(sb.chan,22050);
}

static void DSP_StartDMATranfser(Bit8u mode) {
	sb.samples_left=sb.samples_total;
	if (sb.use_time_constant) {
		sb.freq=(1000000 / (256 - sb.time_constant));
	};
	switch (mode) {
	case MODE_PCM_8S:
		MIXER_SetFreq(sb.chan,sb.freq);
		SB_DEBUG("DSP:PCM 8 bit single cycle rate %d size %d",sb.freq,sb.samples_total);
		break;
	case MODE_PCM_8A:
		MIXER_SetFreq(sb.chan,sb.freq);
		SB_DEBUG("DSP:PCM 8 bit auto init rate %d size %d",sb.freq,sb.samples_total);

		break;
	case MODE_ADPCM_4S:
		MIXER_SetFreq(sb.chan,sb.freq);
		SB_DEBUG("DSP:ADPCM 4 bit single cycle rate %d size %X",sb.freq,sb.samples_total);
		break;

	default:
		LOG_ERROR("DSP:Illegal transfer mode %d",mode);
		return;
	}
	/* Hack to enable dma transfer when game has speaker disabled */
	DSP_SetSpeaker(true);
	sb.mode=mode;
}

static void DSP_AddData(Bit8u val) {
	if (sb.data_out_used<DSP_BUFSIZE) {
		Bit32u start=sb.data_out_used+sb.data_out_pos;
		if (start>=DSP_BUFSIZE) start-=DSP_BUFSIZE;
		sb.data_out[start]=val;
		sb.data_out_used++;
	} else {
		LOG_ERROR("SB:DSP:Data Output buffer full this is weird");
	}
}

static void DSP_Reset(void) {
	sb.mode=MODE_NONE;
	sb.cmd_len=0;
	sb.cmd_in_pos=0;
	sb.use_time_constant=false;
	sb.dac.used=0;
	sb.dac.last=0x80;
	e2_value=0xaa;
	e2_count=0;
	DSP_HaltDMA();
	MIXER_SetFreq(sb.chan,22050);
	MIXER_SetMode(sb.chan,MIXER_8MONO);
	DSP_SetSpeaker(false);
}
static void DSP_DoReset(Bit8u val) {
	if (val&1!=0) {
//TODO Get out of highspeed mode
		DSP_Reset();
		sb.state=DSP_S_RESET;
	} else {
		DSP_FlushData();
		DSP_AddData(0xaa);
		sb.state=DSP_S_NORMAL;
	}
};

static bool dac_warn=false;
static void DSP_DoCommand(void) {
	switch (sb.cmd) {
	case 0x10:	/* Direct DAC */
		sb.mode=MODE_DAC;
		if (sb.dac.used<DSP_DACSIZE) {
			sb.dac.data[sb.dac.used++]=sb.cmd_in[0];
		}
		break;
	case 0x14:	/* Singe Cycle 8-Bit DMA */
		/* Set the length of the transfer */
		sb.samples_total=1+sb.cmd_in[0]+(sb.cmd_in[1] << 8);
		DSP_StartDMATranfser(MODE_PCM_8S);
		break;
	case 0x1c:	/* Auto Init 8-bit DMA */
		DSP_StartDMATranfser(MODE_PCM_8A);
		break;
	case 0x40:	/* Set Timeconstant */
		sb.use_time_constant=true;
		sb.time_constant=sb.cmd_in[0];
		break;
	case 0x48:	/* Set DMA Block Size */
		sb.samples_total=1+sb.cmd_in[0]+(sb.cmd_in[1] << 8);
		break;
    case 0x75:  /* 075h : Single Cycle 4-bit ADPCM Reference */
		sb.adpcm_scale=0;
		sb.adpcm_reference=-1;
		sb.adpcm_remain=-1;
    case 0x74:  /* 074h : Single Cycle 4-bit ADPCM */	
		sb.samples_total=1+sb.cmd_in[0]+(sb.cmd_in[1] << 8);
		DSP_StartDMATranfser(MODE_ADPCM_4S);
		break;
	case 0xd0:	/* Halt 8-bit DMA */
		DSP_HaltDMA();
		break;
	case 0xd1:	/* Enable Speaker */
		DSP_SetSpeaker(true);
		break;
	case 0xd3:	/* Disable Speaker */
		DSP_SetSpeaker(false);
		break;
	case 0xe0:	/* DSP Identification - SB2.0+ */
		DSP_FlushData();
		DSP_AddData(~sb.cmd_in[0]);
		break;
	case 0xe1:	/* Get DSP Version */
		DSP_FlushData();
		DSP_AddData(DSP_MAJOR);
		DSP_AddData(DSP_MINOR);
		break;
	case 0xe2:	/* Weird DMA identification write routine */
		{
/*
		for (Bit8u i = 0; i < 8; i++)
        if ((sb.cmd_in[0] >> i) & 0x01) m_E2Value += E2_incr_table[m_E2Count % 4][i];

      m_E2Value += E2_incr_table[m_E2Count % 4][8];
      m_E2Count++;
*/
//TODO Ofcourse :)
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
		sb.test_register=sb.cmd_in[0];
		break;
	case 0xe8:	/* Read Test Register */
		DSP_FlushData();
		DSP_AddData(sb.test_register);;
		break;

	case 0xf2:	/* Trigger 8bit IRQ */
		DSP_FlushData();
		DSP_AddData(0xaa);
		PIC_ActivateIRQ(sb.irq);
		break;
	default:
		LOG_WARN("SB:DSP:Unhandled command %2X",sb.cmd);
	}
	sb.cmd=DSP_NO_COMMAND;
	sb.cmd_len=0;
	sb.cmd_in_pos=0;
}



static void DSP_DoWrite(Bit8u val) {
	switch (sb.cmd) {
	case DSP_NO_COMMAND:
		sb.cmd=val;
		sb.cmd_len=DSP_cmd_len[val];
		sb.cmd_in_pos=0;
		if (!sb.cmd_len) DSP_DoCommand();
		break;
	default:
		sb.cmd_in[sb.cmd_in_pos]=val;
		sb.cmd_in_pos++;
		if (sb.cmd_in_pos>=sb.cmd_len) DSP_DoCommand();
	}
}

static Bit8u DSP_ReadData(void) {
	Bit8u data=0;	
	if (sb.data_out_used) {
		data=sb.data_out[sb.data_out_pos];
		sb.data_out_pos++;
		if (sb.data_out_pos>=DSP_BUFSIZE) sb.data_out_pos-=DSP_BUFSIZE;
		sb.data_out_used--;
	}
	return data;
}

static Bit8u read_sb(Bit32u port) {
	switch (port-sb.base) {
	case DSP_READ_DATA:
		return DSP_ReadData();
	case DSP_READ_STATUS:
		//TODO Acknowledge 8bit irq
		//TODO See for high speed dma :)
		if (sb.data_out_used) return 0xff;
		else return 0x7f;
	case DSP_WRITE_STATUS:
		switch (sb.state) {
		case DSP_S_NORMAL:
			return 0x7f;
		case DSP_S_RESET:
			return 0xff;
		}
		return 0xff;
/* For now loop FM Stuff to 0x388 */
	case 0x00:	case 0x02:	case 0x08:
		return IO_Read(0x388);
	case DSP_RESET:
		return 0xff;
	default:
		LOG_WARN("SB:Unhandled read from SB Port %4X",port);
		break;
	}
	return 0xff;
}

static void write_sb(Bit32u port,Bit8u val) {
	switch (port-sb.base) {
	case DSP_RESET:
		DSP_DoReset(val);
		break;
	case DSP_WRITE_DATA:
		DSP_DoWrite(val);
		break;
/* For now loop FM Stuff to 0x388 */
	case 0x00:	case 0x02:	case 0x08:
		IO_Write(0x388,val);
		break;
	case 0x01:	case 0x03:	case 0x09:
		IO_Write(0x389,val);
		break;

	default:
		LOG_WARN("SB:Unhandled write to SB Port %4X",port);
		break;
	}
}


INLINE Bit8u decode_ADPCM_4_sample(
    Bit8u sample,
    Bits& reference,
    Bits& scale)
{
  static int scaleMap[8] = { -2, -1, 0, 0, 1, 1, 1, 1 };

  if (sample & 0x08) {
    reference = max(0x00, reference - ((sample & 0x07) << scale));
  } else {
    reference = min(0xff, reference + ((sample & 0x07) << scale));
  }

  scale = max(2, min(6, scaleMap[sample & 0x07]));

  return (Bit8u)reference;
}

static void SBLASTER_CallBack(Bit8u * stream,Bit32u len) {
	unsigned char tmpbuf[65536];
	if (!len) return;
	switch (sb.mode) {
	case MODE_NONE:
		/* If there isn't a mode it's 8 bit mono mode speaker should be disabled normally */
		memset(stream,0x80,len);
		break;
	case MODE_DAC:
		/* Stretch the inputted dac data over len samples */
		{
			if (sb.dac.used) {
				Bitu dac_add=(sb.dac.used<<16)/len;
				Bitu dac_pos=0;
				while (len-->0) {
					*(stream++)=sb.dac.data[dac_pos>>16];
					dac_pos+=dac_add;
				}
				dac_pos-=dac_add;
				sb.dac.last=sb.dac.data[dac_pos>>16];
				sb.dac.used=0;
			} else {
				memset(stream,sb.dac.last,len);
			}
		}
		break;
	case MODE_PCM_8A:
		{
			Bit16u read=DMA_8_Read(sb.dma,stream,(Bit16u)len);
			if (sb.samples_left>read) {
				sb.samples_left-=read;
			} else {
				if (read>(sb.samples_total+sb.samples_left)) sb.samples_left=sb.samples_total;
				else sb.samples_left=sb.samples_total+sb.samples_left-read;			
				PIC_ActivateIRQ(sb.irq);
			}
			if (read<len) memset(stream+read,0x80,len-read);
		}
		break;
	case MODE_PCM_8S:
		{
			Bit16u read;
			if (sb.samples_left>=len) {
				read=DMA_8_Read(sb.dma,stream,(Bit16u)len);
				sb.samples_left-=read;
			} else {
				read=DMA_8_Read(sb.dma,stream,(Bit16u)sb.samples_left);
				sb.samples_left=0;
			} 
			if (read<len) memset(stream+read,0x80,len-read);
			if (sb.samples_left==0) {
				DSP_StopDMA();
				PIC_ActivateIRQ(sb.irq);
			}	
		}
		break;
	case MODE_ADPCM_4S:
		{
			if (sb.adpcm_remain>=0) {
				*stream++=decode_ADPCM_4_sample((Bit8u)sb.adpcm_remain,sb.adpcm_reference,sb.adpcm_scale);
				len--;
			}
			Bitu dma_size=len/2+(len&1);			//Amount of bytes that need to be transferred 
			Bit8u * decode_pos=tmpbuf;
			if (sb.adpcm_reference < 0) {
				dma_size++;
			}
			Bit16u read;
			/* Read from the DMA Channel */
			if (sb.samples_left>=dma_size) {
				read=DMA_8_Read(sb.dma,decode_pos,(Bit16u)dma_size);
			} else if (sb.samples_left<dma_size) {
				read=DMA_8_Read(sb.dma,decode_pos,(Bit16u)sb.samples_left);
			}
			sb.samples_left-=read;
			if (sb.adpcm_reference < 0 && read) {
				sb.adpcm_reference=*decode_pos++;
				read--;
			}
			if ((read*2U)<(Bitu)len) {
				memset(stream+read*2,0x80,len-read*2);
				len=read*2;
			}
			if (sb.samples_left==0) {
				DSP_StopDMA();
				PIC_ActivateIRQ(sb.irq);
			}
			if (!read) return;
			/* Decode the actual samples read from dma */
			for (Bitu i=len/2;i>0;i--) {
				*stream++=decode_ADPCM_4_sample(*decode_pos >> 4,sb.adpcm_reference,sb.adpcm_scale);
				*stream++=decode_ADPCM_4_sample(*decode_pos++   ,sb.adpcm_reference,sb.adpcm_scale);
			}
			if (len & 1) {
				*stream++=decode_ADPCM_4_sample(*decode_pos >> 4,sb.adpcm_reference,sb.adpcm_scale);
				sb.adpcm_remain=*decode_pos & 0xf;
			} else {
				sb.adpcm_remain=-1;
			}
		}
		break;
	}	/* End switch */
}



static bool SB_enabled;

static void SB_Enable(bool enable) {
	Bitu i;
	if (enable) {
		sb.enabled=true;
		for (i=sb.base+4;i<sb.base+0xf;i++) {
			IO_RegisterReadHandler(i,read_sb,"SB");
			IO_RegisterWriteHandler(i,write_sb,"SB");
		}
		PIC_RegisterIRQ(sb.irq,0,"SB");
		DSP_Reset();
	} else {
		sb.enabled=false;
		PIC_FreeIRQ(sb.irq);
		for (i=sb.base+4;i<sb.base+0xf;i++){
			IO_FreeReadHandler(i);
			IO_FreeWriteHandler(i);
		};
	}
}


static void SB_InputHandler(char * line) {
	bool s_off,s_on,s_base,s_irq,s_dma;
	Bits n_base,n_irq,n_dma;
	s_off=ScanCMDBool(line,"OFF");
	s_on=ScanCMDBool(line,"ON");
	s_base=ScanCMDHex(line,"BASE",&n_base);
	s_irq=ScanCMDHex(line,"IRQ",&n_irq);
	s_dma=ScanCMDHex(line,"DMA",&n_dma);
	char * rem=ScanCMDRemain(line);
	if (rem) {
		sprintf(line,"Illegal Switch");
		return;
	}
	if (s_on && s_off) {
		sprintf(line,"Can't use /ON and /OFF at the same time");
		return;
	}
	bool was_enabled=sb.enabled;
	if (sb.enabled) SB_Enable(false);
	if (s_base) {
		sb.base=n_base;
	}
	if (s_irq) {
		sb.irq=n_irq;
	}
	if (s_dma) {
		sb.dma=n_dma;
	}
	if (s_on) {
		SB_Enable(true);
	} 
	if (s_off) {
		SB_Enable(false);
		sprintf(line,"Sound Blaster has been disabled");
		return;
	} 
	if (was_enabled) SB_Enable(true);
	sprintf(line,"Sound Blaster enabled with IO %X IRQ %X DMA %X",sb.base,sb.irq,sb.dma);
	return;
}

static void SB_OutputHandler (char * towrite) {
	if(sb.enabled) {
		sprintf(towrite,"IO %X IRQ %X DMA %X",sb.base,sb.irq,sb.dma);
	} else {
		sprintf(towrite,"Disabled");
	}
};








void SBLASTER_Init(void) {
	sb.chan=MIXER_AddChannel(&SBLASTER_CallBack,22050,"SBLASTER");
	MIXER_Enable(sb.chan,false);
	sb.state=DSP_S_NORMAL;
/* Setup the hardware handler part */
	sb.base=SB_BASE;
	sb.irq=SB_IRQ;
	sb.dma=SB_DMA;
	SB_Enable(true);

	sb.hwblock.dev_name="SB";
	sb.hwblock.full_name="Sound Blaster 1.5";
	sb.hwblock.next=0;
	sb.hwblock.help=
		"/ON        Enables SB\n"
		"/OFF       Disables SB\n"
		"/BASE XXX  Set Base Addres 200-260\n"
		"/IRQ  X    Set IRQ 1-9\n"
		"/DMA  X    Set 8-Bit DMA Channel 0-3\n";
	sb.hwblock.get_input=SB_InputHandler;
	sb.hwblock.show_status=SB_OutputHandler;
	HW_Register(&sb.hwblock);
	
	
	SHELL_AddAutoexec("SET BLASTER=A%3X I%d D%d T3",sb.base,sb.irq,sb.dma);
}
