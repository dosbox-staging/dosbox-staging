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
#include "setup.h"
#include "programs.h"
#include "math.h"
#include "regs.h"


#define GUS_BASE myGUS.portbase
#define GUS_RATE myGUS.rate
#define LOG_GUS

static MIXER_Channel * gus_chan;

static Bit16u vol8bit[256];
static Bit16s vol16bit[4096];

static Bit8u irqtable[8] = { 0, 2, 5, 3, 7, 11, 12, 15 };

static Bit8u dmatable[6] = { 3, 1, 5, 5, 6, 7 };

static Bit8u GUSRam[1024*1024]; // 1024K of GUS Ram

Bit32s AutoAmp=1024;

struct GFGus {
	Bit8u gRegSelect;
	Bit16u gRegData;
	Bit32u gDramAddr;
	Bit16u gCurChannel;

	Bit8u DMAControl;
	Bit16u dmaAddr;
	Bit8u TimerControl;
	Bit8u SampControl;
	Bit8u mixControl;

	Bit32u basefreq;
	Bit16u activechan;
	Bit32s mupersamp;
	Bit32s muperchan;

	
	Bit8u timerReg;
	struct GusTimer {
		Bit16u bytetimer;
		Bit32s countdown;
		Bit32s setting;
		bool active;
	} timers[2];


	Bit32u rate;
	Bit16u portbase;
	Bit16u dma1;
	Bit16u dma2;

	Bit16u irq1;
	Bit16u irq2;

	char ultradir[512];

	bool irqenabled;

	// IRQ status register values
	struct IRQStat {
		bool MIDITx;
		bool MIDIRx;
		bool T1;
		bool T2;
		bool Resv;
		bool WaveTable;
		bool VolRamp;
		bool DMATC;
	} irq;

} myGUS;

#define GUSFIFOSIZE 1024

struct IRQFifoEntry {
	Bit8u channum;
	bool WaveIRQ;
	bool RampIRQ;
};

struct IRQFifoDef {
	IRQFifoEntry entry[GUSFIFOSIZE];
	Bit16s stackpos;
} IRQFifo;

static void GUS_DMA_Callback(DmaChannel * chan,DMAEvent event);

// Routines to manage IRQ requests coming from the GUS
static void pushIRQ(Bit8u channum, bool WaveIRQ, bool RampIRQ) {
	IRQFifo.stackpos++;
	if(IRQFifo.stackpos < GUSFIFOSIZE) {
		myGUS.irq.WaveTable = WaveIRQ;
		myGUS.irq.VolRamp = RampIRQ;

		IRQFifo.entry[IRQFifo.stackpos].channum = channum;
		IRQFifo.entry[IRQFifo.stackpos].RampIRQ = RampIRQ;
		IRQFifo.entry[IRQFifo.stackpos].WaveIRQ = WaveIRQ;



	} else {
		LOG_GUS("GUS IRQ Fifo full!");
	}
}

static void popIRQ(IRQFifoEntry * tmpentry) {
	if(IRQFifo.stackpos<0) {
		tmpentry->channum = 0;
		tmpentry->RampIRQ = false;
		tmpentry->WaveIRQ = false;
		return;
	}
	memcpy(tmpentry, &IRQFifo.entry[IRQFifo.stackpos], sizeof(IRQFifoEntry));
	--IRQFifo.stackpos;
	if(IRQFifo.stackpos >= 0) {
		myGUS.irq.WaveTable = IRQFifo.entry[IRQFifo.stackpos].WaveIRQ;
		myGUS.irq.VolRamp = IRQFifo.entry[IRQFifo.stackpos].RampIRQ;
	} else {
		myGUS.irq.WaveTable = false;
		myGUS.irq.VolRamp = false;
	}
}


// Returns a single 16-bit sample from the Gravis's RAM
static INLINE Bit16s GetSample(Bit32u Delta, Bit32u CurAddr, bool eightbit) {
	Bit32u useAddr;
	Bit32u holdAddr;
	useAddr = CurAddr >> 9;
	if(eightbit) {
		if(Delta >= 1024) {
			Bit8s tmpsmall = (Bit8s)GUSRam[useAddr];
			return (Bit16s)(tmpsmall << 7);
		} else {

			// Interpolate
			Bit8s b1 = (Bit8s)GUSRam[useAddr];
			Bit8s b2 = (Bit8s)GUSRam[useAddr+1];
			Bit32s w1 = b1 << 8;
			Bit32s w2 = b2 << 8;
			Bit32s diff = w2 - w1;
			return (Bit16s)(w1+((diff*(CurAddr&511))>>9));

		}
	} else {
		
		// Formula used to convert addresses for use with 16-bit samples
		holdAddr = useAddr & 0xc0000L;
		useAddr = useAddr & 0x1ffffL;
		useAddr = useAddr << 1;
		useAddr = (holdAddr | useAddr);

		if(Delta >= 1024) {
			
			return (Bit16s)((Bit16u)GUSRam[useAddr] | ((Bit16u)GUSRam[useAddr+1] << 8)) >> 2
		;

		} else {

			// Interpolate
			Bit16s w1 = (Bit16s)((Bit16u)GUSRam[useAddr] | ((Bit16u)GUSRam[useAddr+1] << 8));
			Bit16s w2 = (Bit16s)((Bit16u)GUSRam[useAddr+2] | ((Bit16u)GUSRam[useAddr+3] << 8));
			Bit32s diff = w2 - w1;
			return (Bit16s)(w1+((diff*(CurAddr&511))>>9));
		}
	}
}

class GUSChannels {
public:
	Bit8u voiceCont;
	Bit16u FreqCont;
	Bit32u RealDelta;
	Bit32u StartAddr;
	Bit32u EndAddr;
	Bit8s VolRampRate;
	Bit16s VolRampStart;
	Bit16s VolRampEnd;
	Bit8u VolRampStartOrg;
	Bit8u VolRampEndOrg;
	Bit32s CurVolume;
	Bit32u CurAddr;
	Bit8u PanPot;
	Bit8u VolControl;
	Bit8u channum;

	bool moving;
	bool playing;
	bool ramping;
	bool dir;
	bool voldir;

public:
	bool notifyonce;
	Bit32s leftvol;
	Bit32s rightvol;
	Bit32s nextramp;

	GUSChannels(Bit8u num) { 
		channum = num;
		playing = true; 
		ramping = false; 
		moving = false;
		dir = false;
		voldir = false;
		StartAddr = 0;
		EndAddr = 0;
		CurAddr = 0;
		VolRampRate = 0;
		VolRampStart = 0;
		VolRampEnd = 0;
		leftvol = 255;
		rightvol = 255;
		nextramp = 0;
		
	};

	// Voice control register
	void WriteVoiceCtrl(Bit8u val) {
		voiceCont = val;
		if (val & 0x3) moving = false;
		if ((val & 0x3) == 0) {
			//playing = true;
			moving = true;
		}
		dir = false;
		if((val & 0x40) !=0) dir = true;

	}
	Bit8u ReadVoiceCtrl(void) {
		Bit8u tmpval = voiceCont & 0xfe;
		if(!playing) tmpval++;
		return tmpval;
	}

	// Frequency control register
	void WriteFreqCtrl(Bit16u val) {
		FreqCont = val;
		int fc;
		fc = val;
		fc = fc >> 1;
		fc = fc * myGUS.basefreq;
		fc = fc - (myGUS.basefreq >> 1);
		fc = fc / 512;
		float simple;

		simple = ((float)fc / (float)GUS_RATE) * 512;
		RealDelta = (Bit32u)simple;
	}
	Bit16u ReadFreqCtrl(void) {
		return FreqCont;
	}

	// Used when GUS changes channel numbers during playback
	void UpdateFreqCtrl() { WriteFreqCtrl(FreqCont); }

	// Pan position register
	void WritePanPot(Bit8u val) {
		if(val<8) {
			leftvol = 255;
			rightvol = val << 5;
		} else {
			rightvol = 255;
			leftvol = (8-(val-8)) << 5;
		}
		PanPot = val;
	}
	Bit8u ReadPanPot(void) {
		return PanPot;
	}

	// Volume ramping control register
	void WriteVolControl(Bit8u val) {
		VolControl = val;
		if (val & 0x3) ramping = false;
		if ((val & 0x3) == 0) ramping = true;
		voldir = false;
		if((val & 0x40) !=0) voldir = true;

	}
	Bit8u ReadVolControl(void) {
		Bit8u tmpval = VolControl & 0xfe;
		if(!ramping) tmpval++;
		return tmpval;
	}


	// Methods to queue IRQ on ramp or sample end
	void NotifyEndSamp(void) {
		if(!notifyonce) {
			if((voiceCont & 0x20) != 0) {
				pushIRQ(channum,true,false);
				notifyonce = true;
			}
		}
	}
	void NotifyEndRamp(void) {
		if(!notifyonce) {
			if((VolControl & 0x20) != 0) {
				pushIRQ(channum,false,true);
				notifyonce = true;
			}
		}
	}

	// Debug routine to show current channel position
	void ShowAddr(void) {
		LOG_GUS("Chan %d Start %d End %d Current %d", channum, StartAddr>>9, EndAddr>>9, CurAddr>>9);
	}


	// Generate the samples required by the callback routine
	// It should be noted that unless a channel is stopped, it will
	// continue to return the sample it is pointing to, regardless
	// of whether or not the channel is moving or ramping.
	void generateSamples(Bit16s * stream,Bit32u len) {
		int i;
		Bit16s tmpsamp;
		bool eightbit;
		eightbit = ((voiceCont & 0x4) == 0);
	
		notifyonce = false;

		for(i=0;i<(int)len;i++) {
			// Get sample
			tmpsamp = GetSample(RealDelta, CurAddr, eightbit);
		
			// Clip and convert log scale to PCM scale
			if(CurVolume>4095) CurVolume = 4095;
			if(CurVolume<0) CurVolume = 0;
			tmpsamp = (tmpsamp * vol16bit[CurVolume]) >> 12;

			// Output stereo sample
			stream[i<<1] = (Bit16s)(((Bit32s)tmpsamp * (Bit32s)leftvol)>>8) ;
			stream[(i<<1)+1] = (Bit16s)(((Bit32s)tmpsamp * rightvol)>>8);
			

			if(dir) {
				// Increment backwards
				if (moving) CurAddr -= RealDelta;

				//Thought 16-bit needed this
				//if ((!eightbit) && (moving)) CurAddr -= RealDelta;
				
				if(CurAddr <= StartAddr) {
					if((VolControl & 0x4) == 0) {
						if((voiceCont & 0x8) != 0) {
							if((voiceCont & 0x10) != 0) {
								dir = !dir;
							} else {
								CurAddr = EndAddr - (512-(CurAddr & 511));
							}

						} else {
							moving = false;
						}
						NotifyEndSamp();
					} else {
						NotifyEndSamp();

					}
					
				}

			} else {

				// Increment forwards
				if (moving) CurAddr += RealDelta;

				//Thought 16-bit needed this
				//if ((!eightbit) && (moving)) CurAddr += RealDelta;
				if(CurAddr >= EndAddr) {
					if((VolControl & 0x4) == 0) {
						if((voiceCont & 0x8) != 0) {
							if((voiceCont & 0x10) != 0) {
								dir = !dir;
							} else {
								CurAddr = StartAddr+(CurAddr & 511);
							}

						} else {
							moving = false;
						}
						NotifyEndSamp();
					} else {
						NotifyEndSamp();
					}
				}
			}

			// Update volume 
			if(ramping) {

				// Subtract ramp counter by elapsed microseconds
				nextramp -= myGUS.mupersamp;
				bool flagged;
				flagged = false;

				// Ramp volume until nextramp is a positive integer
				while(nextramp <= 0) {
					if(voldir) {
						CurVolume -= (VolRampRate & 0x3f);
						if (CurVolume <= 0) {
							CurVolume = 0;
							flagged = true;
						}

						if((vol16bit[CurVolume]<=VolRampStart) || (flagged)){
							if((VolControl & 0x8) != 0) {
								if((VolControl & 0x10) != 0) {
									voldir = !voldir;
								} else {
									CurVolume = VolRampEndOrg;
								}
							} else {
								ramping = false;
							}
							NotifyEndRamp();
						}

					} else {
						CurVolume += (VolRampRate & 0x3f);
						if (CurVolume >= 4095) {
							CurVolume = 4095;
							flagged = true;
						}

						if((vol16bit[CurVolume]>=VolRampEnd) || (flagged)){
							if((VolControl & 0x8) != 0) {
								if((VolControl & 0x10) != 0) {
									voldir = !voldir;
								} else {
									CurVolume = VolRampStartOrg;
								}
							} else {
								ramping = false;
							}
							NotifyEndRamp();
						}

					}


					switch(VolRampRate >> 6) {
					case 0:
						nextramp += myGUS.muperchan; 
						break;
					case 1:
						nextramp += myGUS.muperchan* 8;
						break;
					case 2:
						nextramp += myGUS.muperchan * 64;
						break;
					case 3:
						nextramp += myGUS.muperchan * 512;
						break;
					default:
						nextramp += myGUS.muperchan * 512;
						break;

					}
					
				}

			}


		}

	
	}


};


GUSChannels *guschan[33];

GUSChannels *curchan;


static void GUSReset(void)
{
	if((myGUS.gRegData & 0x1) == 0x1) {
		// Reset
		myGUS.timerReg = 85;
		memset(&myGUS.irq, 0, sizeof(myGUS.irq));
	}
	if((myGUS.gRegData & 0x4) != 0) {
		myGUS.irqenabled = true;
	} else {
		myGUS.irqenabled = false;
	}

	myGUS.timers[0].active = false;
	myGUS.timers[1].active = false;
	myGUS.timers[0].bytetimer = 0x0;
	myGUS.timers[1].bytetimer = 0x0;
	
	// Stop all channels
	int i;
	for(i=0;i<32;i++) {
		guschan[i]->WriteVoiceCtrl(0x3);

	}
	IRQFifo.stackpos = -1;

}



static Bit16u ExecuteReadRegister(void) {
	Bit8u tmpreg;

	switch (myGUS.gRegSelect) {
	case 0x41: // Dma control register - read acknowledges DMA IRQ
		tmpreg = myGUS.DMAControl & 0xbf;
		if(myGUS.irq.DMATC) tmpreg |= 0x40;

		myGUS.irq.DMATC = false;
		PIC_DeActivateIRQ(myGUS.irq1);

		return (Bit16u)(tmpreg << 8);
	case 0x42:  // Dma address register
		return myGUS.dmaAddr;
	case 0x45:  // Timer control register.  Identical in operation to Adlib's timer
		return (Bit16u)(myGUS.TimerControl << 8);
		break;
	case 0x49:  // Dma sample register
		tmpreg = myGUS.DMAControl & 0xbf;
		if(myGUS.irq.DMATC) tmpreg |= 0x40;

		myGUS.irq.DMATC = false;
		PIC_DeActivateIRQ(myGUS.irq1);
		//LOG_GUS("Read sampling status, returned 0x%x", tmpreg);

		return (Bit16u)(tmpreg << 8);
	case 0x80: // Channel voice control read register
		if(curchan != NULL) {
			Bit8u sndout;
			sndout = curchan->voiceCont & 0xFC;
			if(!curchan->moving) sndout |= 0x3;

			return (Bit16u)(sndout<< 8);
		} else return 0x0300;

	case 0x82: // Channel MSB address register
		if(curchan != NULL) {
			return (Bit16u)(curchan->StartAddr >> 16);
		} else return 0x0000;
	case 0x83: // Channel LSW address register
		if(curchan != NULL) {
			return (Bit16u)(curchan->StartAddr & 0xffff);
		} else return 0x0000;
	case 0x89: // Channel volume register
		if(curchan != NULL) {
			return ((Bit16u)curchan->CurVolume << 4);
		} else return 0x0000;
	case 0x8a: // Channel MSB current address register
		if(curchan != NULL) {
			return (Bit16u)(curchan->CurAddr >> 16);
		} else return 0x0000;

	case 0x8b: // Channel LSW current address register
		if(curchan != NULL) {
			return (Bit16u)(curchan->CurAddr & 0xFFFF);
		} else return 0x0000;
	case 0x8d: // Channel volume control register
		if(curchan != NULL) {
			Bit8u volout;
			volout = curchan->VolControl & 0xFC;
			if(!curchan->ramping) volout |= 0x3;
			return (volout << 8);
		} else return 0x0300;

	case 0x8f: // General channel IRQ status register
		Bit8u temp;
		temp = 0x20;
		IRQFifoEntry tmpentry;
		PIC_DeActivateIRQ(myGUS.irq1);
		popIRQ(&tmpentry);
		if(!tmpentry.WaveIRQ) temp |= 0x80;
		if(!tmpentry.RampIRQ) temp |= 0x40;
		temp |= tmpentry.channum;

		return (Bit16u)(temp << 8);
	default:
		LOG_GUS("Read Register num 0x%x", myGUS.gRegSelect);
		return myGUS.gRegData;
	}
}

  
static void ExecuteGlobRegister(void) {
	int i;
	//LOG_GUS("Access global register %x with %x", myGUS.gRegSelect, myGUS.gRegData);
	switch(myGUS.gRegSelect) {
	case 0x0:  // Channel voice control register
		if(curchan != NULL) {
			curchan->WriteVoiceCtrl((Bit16u)myGUS.gRegData>>8);
		}
		break;
	case 0x1:  // Channel frequency control register
		if(curchan != NULL) {
			curchan->WriteFreqCtrl(myGUS.gRegData);
		}
		break;
	case 0x2:  // Channel MSB start address register
		if(curchan != NULL) {
			Bit32u tmpaddr = myGUS.gRegData << 16;
			curchan->StartAddr = (curchan->StartAddr & 0xFFFF) | tmpaddr;
		}
		break;
	case 0x3:  // Channel LSB start address register
		if(curchan != NULL) {
			Bit32u tmpaddr = (Bit32u)(myGUS.gRegData);
			curchan->StartAddr = (curchan->StartAddr & 0x1FFF0000) | tmpaddr;
		}
		break;
	case 0x4:  // Channel MSB end address register
		if(curchan != NULL) {
			Bit32u tmpaddr = (Bit32u)myGUS.gRegData << 16;
			curchan->EndAddr = (curchan->EndAddr & 0xFFFF) | tmpaddr;
		}
		break;
	case 0x5:  // Channel MSB end address register
		if(curchan != NULL) {
			Bit32u tmpaddr = (Bit32u)(myGUS.gRegData);
			curchan->EndAddr = (curchan->EndAddr & 0x1FFF0000) | tmpaddr;
		}
		break;
	case 0x6:  // Channel volume ramp rate register
		if(curchan != NULL) {
			Bit8u tmpdata = (Bit16u)myGUS.gRegData>>8;
			curchan->VolRampRate = tmpdata;
		}
		break;
	case 0x7:  // Channel volume ramp start register  EEEEMMMM
		if(curchan != NULL) {
			Bit8u tmpdata = (Bit16u)myGUS.gRegData >> 8;
			curchan->VolRampStart = vol8bit[tmpdata];
			curchan->VolRampStartOrg = tmpdata << 4;
		}
		break;
	case 0x8:  // Channel volume ramp end register  EEEEMMMM
		if(curchan != NULL) {
			Bit8u tmpdata = (Bit16u)myGUS.gRegData >> 8;
			curchan->VolRampEnd = vol8bit[tmpdata];	
			curchan->VolRampEndOrg = tmpdata << 4;
		}
		break;
	case 0x9:  // Channel current volume register
		if(curchan != NULL) {
			Bit16u tmpdata = (Bit16u)myGUS.gRegData >> 4;
			curchan->CurVolume = tmpdata;
		}
		break;
	case 0xA:  // Channel MSB current address register
		if(curchan != NULL) {
			Bit32u tmpaddr = ((Bit32u)myGUS.gRegData & 0x1fff) << 16;
			curchan->CurAddr = (curchan->CurAddr & 0xFFFF) | tmpaddr;
		}
		break;
	case 0xB:  // Channel LSW current address register
		if(curchan != NULL) {
			curchan->CurAddr = (curchan->CurAddr & 0xFFFF0000) | ((Bit32u)myGUS.gRegData);
		}
		break;
	case 0xC:  // Channel pan pot register
		if(curchan != NULL) {
			curchan->WritePanPot((Bit16u)myGUS.gRegData>>8);
		}
		break;
	case 0xD:  // Channel volume control register
		if(curchan != NULL) {
			curchan->WriteVolControl((Bit16u)myGUS.gRegData>>8);
		}
		break;
	case 0xE:  // Set active channel register
		myGUS.activechan = (myGUS.gRegData>>8) & 63;
		if(myGUS.activechan < 13) myGUS.activechan = 13;
		if(myGUS.activechan > 31) myGUS.activechan = 31;
		MIXER_Enable(gus_chan,true);
		myGUS.basefreq = (Bit32u)((float)1000000/(1.619695497*(float)(myGUS.activechan+1)));
		
		float simple;
		simple = (1.0f / (float)GUS_RATE) / 0.000001f;
		myGUS.mupersamp = (Bit32s)simple*1024;
		myGUS.muperchan = (Bit32s)((float)1.6 * (float)myGUS.activechan * 1024);
		LOG_GUS("GUS set to %d channels", myGUS.activechan);

		for(i=0;i<=myGUS.activechan;i++) {	if(guschan[i] != NULL) guschan[i]->UpdateFreqCtrl(); }
	
		break;
	case 0x10:  // Undocumented register used in Fast Tracker 2
		break;
	case 0x41:  // Dma control register
		myGUS.DMAControl = (Bit8u)(myGUS.gRegData>>8);
		DmaChannels[myGUS.dma1]->Register_Callback(
			(myGUS.DMAControl & 0x1) ? GUS_DMA_Callback : 0);
		break;
	case 0x42:  // Gravis DRAM DMA address register
		myGUS.dmaAddr = myGUS.gRegData;
		break;
	case 0x43:  // MSB Peek/poke DRAM position
		myGUS.gDramAddr = (0xff0000 & myGUS.gDramAddr) | ((Bit32u)myGUS.gRegData);
		break;
	case 0x44:  // LSW Peek/poke DRAM position
		myGUS.gDramAddr = (0xffff & myGUS.gDramAddr) | ((Bit32u)myGUS.gRegData>>8) << 16;
		break;
	case 0x45:  // Timer control register.  Identical in operation to Adlib's timer
		myGUS.TimerControl = (Bit8u)(myGUS.gRegData>>8);
		if((myGUS.TimerControl & 0x08) !=0) myGUS.timers[1].countdown = myGUS.timers[1].setting;
		if((myGUS.TimerControl & 0x04) !=0) myGUS.timers[0].countdown = myGUS.timers[0].setting;
		myGUS.irq.T1 = false;
		myGUS.irq.T2 = false;
		PIC_DeActivateIRQ(myGUS.irq1);
		break;
	case 0x46:  // Timer 1 control
		myGUS.timers[0].bytetimer = (Bit8u)(myGUS.gRegData>>8);
		myGUS.timers[0].setting = ((Bit32s)0xff - (Bit32s)myGUS.timers[0].bytetimer) * ((Bit32s)80 << 10);
		myGUS.timers[0].countdown = myGUS.timers[0].setting;
		break;
	case 0x47:  // Timer 2 control
		myGUS.timers[1].bytetimer = (Bit8u)(myGUS.gRegData>>8);
		myGUS.timers[1].setting = ((Bit32s)0xff - (Bit32s)myGUS.timers[1].bytetimer) * ((Bit32s)360 << 10);
		myGUS.timers[1].countdown = myGUS.timers[1].setting;
		break;
	case 0x49:  // DMA sampling control register
		myGUS.SampControl = (Bit8u)(myGUS.gRegData>>8);
		DmaChannels[myGUS.dma1]->Register_Callback(
			(myGUS.SampControl & 0x1)  ? GUS_DMA_Callback : 0);
		break;
	case 0x4c:  // GUS reset register
		GUSReset();
		break;
	default:
		LOG_GUS("Unimplemented global register %x -- %x", myGUS.gRegSelect, myGUS.gRegData);
	}
	return;
}


static Bitu read_gus(Bitu port,Bitu iolen) {

	switch(port - GUS_BASE) {
	case 0x206:
		Bit8u temp;
		temp = 0;
		if(myGUS.irq.MIDITx) temp |= 1;
		if(myGUS.irq.MIDIRx) temp |= 2;
		if(myGUS.irq.T1) temp |= 4;
		if(myGUS.irq.T2) temp |= 8;
		if(myGUS.irq.Resv) temp |= 16;
		if(myGUS.irq.WaveTable) temp |= 32;
		if(myGUS.irq.VolRamp) temp |= 64;
		if(myGUS.irq.DMATC) temp |= 128;
		PIC_DeActivateIRQ(myGUS.irq1);
		return temp;
	case 0x208:
		Bit8u tmptime;
		tmptime = 0;

		if(myGUS.irq.T1) tmptime |= (1 << 6);
		if(myGUS.irq.T2) tmptime |= (1 << 5);
		if((myGUS.irq.T1) || (myGUS.irq.T2)) tmptime |= (1 << 7);
		return tmptime;
	case 0x20a:
		return myGUS.timerReg;
	case 0x302:
		return (Bit8u)myGUS.gCurChannel;
	case 0x303:
		return myGUS.gRegSelect;
	case 0x304:
		if (iolen==2) return ExecuteReadRegister() & 0xffff;
		else return ExecuteReadRegister() & 0xff;
	case 0x305:
		return ExecuteReadRegister() >> 8;
	case 0x307:
		if(myGUS.gDramAddr < sizeof(GUSRam)) {
			return GUSRam[myGUS.gDramAddr];
		} else {
			return 0;
		}
	default:
		LOG_GUS("Read GUS at port 0x%x", port);
		break;
	}

	return 0xff;
}


static void write_gus(Bitu port,Bitu val,Bitu iolen) {
	switch(port - GUS_BASE) {
	case 0x200:
		myGUS.mixControl = val;
		break;
	case 0x208:
		myGUS.timerReg = val;
		break;
	case 0x209:
		myGUS.timers[0].active = ((val & 0x1) > 0);
		myGUS.timers[1].active = ((val & 0x2) > 0);
		break;
	case 0x20b:
		if((myGUS.mixControl & 0x40) != 0) {
			// IRQ configuration
			Bit8u temp = val & 0x7; // Select GF1 irq
			if(myGUS.irq1 == irqtable[temp]) {
			} else {
				LOG_GUS("Attempt to assign GUS to wrong IRQ - at %x set to %x", myGUS.irq1, irqtable[temp]);
			}
		} else {
			// DMA configuration
			Bit8u temp = val & 0x7; // Select playback IRQ
			if(myGUS.dma1 == dmatable[temp]) {
			} else {
				LOG_GUS("Attempt to assign GUS to wrong DMA - at %x, assigned %x", myGUS.dma1, dmatable[temp]);
			}
		}

		break;
	case 0x302:
		myGUS.gCurChannel = val ;
		if (myGUS.gCurChannel > 32) myGUS.gCurChannel = 32;
		curchan = guschan[val];
		break;
	case 0x303:
		myGUS.gRegSelect = val;
		myGUS.gRegData = 0;
		break;
	case 0x304:
		if (iolen==2) myGUS.gRegData=val;
		else myGUS.gRegData = (0xff00 & myGUS.gRegData) | val;
		ExecuteGlobRegister();
		break;
	case 0x305:
		myGUS.gRegData = (0x00ff & myGUS.gRegData) | val << 8;
		ExecuteGlobRegister();
		break;

	case 0x307:
		if(myGUS.gDramAddr < sizeof(GUSRam)) GUSRam[myGUS.gDramAddr] = val;
		break;
	default:
		LOG_GUS("Write GUS at port 0x%x with %x", port, val);
		break;
	}
}

static void GUS_DMA_Callback(DmaChannel * chan,DMAEvent event) {
	if (event!=DMA_UNMASKED) return;
	Bitu dmaaddr = myGUS.dmaAddr << 4;
	if((myGUS.DMAControl & 0x2) == 0) {
		Bitu read=chan->Read(chan->currcnt+1,&GUSRam[dmaaddr]);
		//Check for 16 or 8bit channel
		read*=(chan->DMA16+1);
		if((myGUS.DMAControl & 0x80) != 0) {
			//Invert the MSB to convert twos compliment form
			Bitu i;
			if((myGUS.DMAControl & 0x40) == 0) {
				// 8-bit data
				for(i=dmaaddr;i<(dmaaddr+read);i++) GUSRam[i] ^= 0x80;
			} else {
				// 16-bit data
				for(i=dmaaddr+1;i<(dmaaddr+read-1);i+=2) GUSRam[i] ^= 0x80;
			}
		}
	} else {
		//Read data out of UltraSound
		chan->Write(chan->currcnt+1,&GUSRam[dmaaddr]);
	}
	/* Raise the TC irq if needed */
	if((myGUS.DMAControl & 0x20) != 0) {
		myGUS.irq.DMATC = true;
		PIC_ActivateIRQ(myGUS.irq1);
	}
	chan->Register_Callback(0);
}


static void GUS_CallBack(Bit8u * stream,Bit32u len) {

	Bit16s *bufptr;
	Bit16s buffer[4096];
	Bit32s tmpbuf[4096];

	memset(&buffer[0],0,len*4);
	memset(&tmpbuf[0],0,len*8);

	Bitu i,t;
	for(i=0;i<=myGUS.activechan;i++) {
		if (guschan[i]->playing) {
			guschan[i]->generateSamples(&buffer[0],len);
			for(t=0;t<len*2;t++) {
				tmpbuf[t]+=(Bit32s)buffer[t];
			}
		}
	}

	if(myGUS.irqenabled) {
		if(IRQFifo.stackpos >= 0) {
			PIC_ActivateIRQ(myGUS.irq1);
		}
	}

	if(myGUS.timers[0].active) {
		myGUS.timers[0].countdown-=(len * (Bit32u)myGUS.mupersamp);
		if(!myGUS.irq.T1) {
			// Expire Timer 1
			if(myGUS.timers[0].countdown < 0) {
				if((myGUS.TimerControl & 0x04) !=0) {
					PIC_ActivateIRQ(myGUS.irq1);
				}
				myGUS.irq.T1 = true;
				//LOG_GUS("T1 timer expire");
				//myGUS.timers[0].countdown = myGUS.timers[0].setting;
			}
		}
	}
	if(myGUS.timers[1].active) {
		if(!myGUS.irq.T2) {
			myGUS.timers[1].countdown-=(len * (Bit32u)myGUS.mupersamp);
			// Expire Timer 2
			if(myGUS.timers[1].countdown < 0) {
				if((myGUS.TimerControl & 0x08) !=0) {
					PIC_ActivateIRQ(myGUS.irq1);
				}
				myGUS.irq.T2 = true;
			}
		}
	}

	
	Bit32s sample;
	bufptr = (Bit16s *)stream;
//	LOG_GUS("AutoAmp: %i\n",AutoAmp);
	for(i=0;i<len*2;i++)
	{
		sample=(tmpbuf[i]*AutoAmp)>>9;
		if (sample>32767)
		{
			sample=32767;
			AutoAmp--;
		} else if (sample<-32768)
		{
			sample=-32768;
			AutoAmp--;
		}
		bufptr[i] = (Bit16s)(sample);
	}

}


// Generate logarithmic to linear volume conversion tables
void MakeTables(void) 
{
	int i;

	for(i=0;i<256;i++) {
		float a,b;
		a = pow(2.0f,(float)(i >> 4));
		b = 1.0f+((float)(i & 0xf))/(float)16;
		a *= b;
		a /= 16;
		vol8bit[i] = (Bit16u)a;
	}
	for(i=0;i<4096;i++) {
		float a,b;
		a = pow(2.0f,(float)(i >> 8));
		b = 1.0f+((float)(i & 0xff))/(float)256;
		a *= b;
		a /= 16;
		vol16bit[i] = (Bit16u)a;
	}


}


void GUS_Init(Section* sec) {

	
	memset(&myGUS,0,sizeof(myGUS));
	memset(GUSRam,0,1024*1024);

	Section_prop * section=static_cast<Section_prop *>(sec);
	if(!section->Get_bool("gus")) return;
	myGUS.rate=section->Get_int("rate");

	myGUS.portbase = section->Get_hex("base") - 0x200;
	myGUS.dma1 = section->Get_int("dma1");
	myGUS.dma2 = section->Get_int("dma2");
	myGUS.irq1 = section->Get_int("irq1");
	myGUS.irq2 = section->Get_int("irq2");
	strcpy(&myGUS.ultradir[0], section->Get_string("ultradir"));

	myGUS.timerReg = 85;
	myGUS.timers[0].active = false;
	myGUS.timers[1].active = false;
	

	memset(&myGUS.irq, 0, sizeof(myGUS.irq));
	IRQFifo.stackpos = -1;
	myGUS.irqenabled = false;

	// We'll leave the MIDI interface to the MPU-401 

	// Ditto for the Joystick 

	// GF1 Synthesizer 
	
	IO_RegisterWriteHandler(0x302 + GUS_BASE,write_gus,IO_MB);
	IO_RegisterReadHandler(0x302 + GUS_BASE,read_gus,IO_MB);

	IO_RegisterWriteHandler(0x303 + GUS_BASE,write_gus,IO_MB);
	IO_RegisterReadHandler(0x303 + GUS_BASE,read_gus,IO_MB);

	IO_RegisterWriteHandler(0x304 + GUS_BASE,write_gus,IO_MB|IO_MW);
	IO_RegisterReadHandler(0x304 + GUS_BASE,read_gus,IO_MB|IO_MW);

	IO_RegisterWriteHandler(0x305 + GUS_BASE,write_gus,IO_MB);
	IO_RegisterReadHandler(0x305 + GUS_BASE,read_gus,IO_MB);

	IO_RegisterReadHandler(0x206 + GUS_BASE,read_gus,IO_MB);

	IO_RegisterWriteHandler(0x208 + GUS_BASE,write_gus,IO_MB);
	IO_RegisterReadHandler(0x208 + GUS_BASE,read_gus,IO_MB);

	IO_RegisterWriteHandler(0x209 + GUS_BASE,write_gus,IO_MB);

	IO_RegisterWriteHandler(0x307 + GUS_BASE,write_gus,IO_MB);
	IO_RegisterReadHandler(0x307 + GUS_BASE,read_gus,IO_MB);

	// Board Only 

	IO_RegisterWriteHandler(0x200 + GUS_BASE,write_gus,IO_MB);
	IO_RegisterReadHandler(0x20A + GUS_BASE,read_gus,IO_MB);
	IO_RegisterWriteHandler(0x20B + GUS_BASE,write_gus,IO_MB);

	PIC_RegisterIRQ(myGUS.irq1,0,"GUS");
	PIC_RegisterIRQ(myGUS.irq2,0,"GUS");

//	DmaChannels[myGUS.dma1]->Register_TC_Callback(GUS_DMA_TC_Callback);

	MakeTables();

	int i;
	for(i=0;i<=32;i++) {
		guschan[i] = new GUSChannels(i);
	}

	// Register the Mixer CallBack 

	gus_chan=MIXER_AddChannel(GUS_CallBack,GUS_RATE,"GUS");
	MIXER_SetMode(gus_chan,MIXER_16STEREO);
	MIXER_Enable(gus_chan,false);

	int portat = 0x200+GUS_BASE;
	// ULTRASND=Port,DMA1,DMA2,IRQ1,IRQ2
	SHELL_AddAutoexec("SET ULTRASND=%3X,%d,%d,%d,%d",portat,myGUS.dma1,myGUS.dma2,myGUS.irq1,myGUS.irq2);
	SHELL_AddAutoexec("SET ULTRADIR=%s", myGUS.ultradir);
}



