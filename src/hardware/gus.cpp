#include <string.h>
#include "dosbox.h"
#include "inout.h"
#include "mixer.h"
#include "dma.h"
#include "pic.h"
#include "hardware.h"
#include "setup.h"
#include "programs.h"
#include "math.h"
#include "regs.h"


#define GUS_BASE myGUS.portbase
#define GUS_RATE myGUS.rate

static MIXER_Channel * gus_chan;

static Bit16u vol8bit[256];
static Bit16s vol16bit[4096];

static Bit8u irqtable[8] = { 0, 2, 5, 3, 7, 11, 12, 15 };

static Bit8u dmatable[6] = { 3, 1, 5, 5, 6, 7 };

static Bit8u GUSRam[1024*1024]; // 1024K of GUS Ram

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
	Bit16u mupersamp;
	Bit16u muperchan;

	
	Bit16u timerReg;
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

static void UseDMA(DmaChannel *useDMA);

struct IRQFifoEntry {
	Bit8u channum;
	bool WaveIRQ;
	bool RampIRQ;
};

struct IRQFifoDef {
	IRQFifoEntry entry[GUSFIFOSIZE];
	Bit16s stackpos;
} IRQFifo;

static void pushIRQ(Bit8u channum, bool WaveIRQ, bool RampIRQ) {
	IRQFifo.stackpos++;
	//LOG_MSG("Stack position %d", IRQFifo.stackpos);
	if(IRQFifo.stackpos < GUSFIFOSIZE) {
		myGUS.irq.WaveTable = WaveIRQ;
		myGUS.irq.VolRamp = RampIRQ;

		IRQFifo.entry[IRQFifo.stackpos].channum = channum;
		IRQFifo.entry[IRQFifo.stackpos].RampIRQ = RampIRQ;
		IRQFifo.entry[IRQFifo.stackpos].WaveIRQ = WaveIRQ;



	} else {
		LOG_MSG("GUS IRQ Fifo full!");
	}
}

INLINE Bit16s GetSample(Bit32u Delta, Bit32u CurAddr, bool eightbit) {
	Bit32u useAddr;
	Bit32u holdAddr;
	useAddr = CurAddr >> 9;
	if(eightbit) {
		if(Delta >= 1024) {
			Bit8s tmpsmall = (Bit8s)GUSRam[useAddr];
			return (Bit16s)(tmpsmall << 7);
		} else {
			Bit8s b1 = (Bit8s)GUSRam[useAddr];
			Bit8s b2 = (Bit8s)GUSRam[useAddr+1];
			Bit16s w1 = b1 << 7;
			Bit16s w2 = b2 << 7;
			Bit16s diff = w2 - w1;
			return (w1 + (((Bit32s)diff * (Bit32s)(CurAddr & 0x3fe)) >> 10));

		}
	} else {
		
		holdAddr = useAddr & 0xc0000L;
		useAddr = useAddr & 0x1ffffL;
		useAddr = useAddr << 1;
		useAddr = (holdAddr | useAddr);
		//LOG_MSG("Sample from %x (%d)", useAddr, useAddr);
		
		if(Delta >= 1024) {
			
			return (Bit16s)((Bit16u)GUSRam[useAddr] | ((Bit16u)GUSRam[useAddr+1] << 8)) >> 1;

		} else {
			Bit16s w1 = (Bit16s)((Bit16u)GUSRam[useAddr] | ((Bit16u)GUSRam[useAddr+1] << 8));
			Bit16s w2 = (Bit16s)((Bit16u)GUSRam[useAddr+2] | ((Bit16u)GUSRam[useAddr+3] << 8));
			Bit16s diff = w2 - w1;
			return (w1 + (((Bit32s)diff * (Bit32s)((CurAddr) & 0x3fe)) >> 10)) >> 1;

		}
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
	//LOG_MSG("Stack position %d", IRQFifo.stackpos);
	if(IRQFifo.stackpos >= 0) {
		myGUS.irq.WaveTable = IRQFifo.entry[IRQFifo.stackpos].WaveIRQ;
		myGUS.irq.VolRamp = IRQFifo.entry[IRQFifo.stackpos].RampIRQ;
	} else {
		myGUS.irq.WaveTable = false;
		myGUS.irq.VolRamp = false;
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
	Bit16u channum;

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

	GUSChannels(Bit16u num) { 
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

	void WriteFreqCtrl(Bit16u val) {
		FreqCont = val;
		//RealDelta = (myGUS.basefreq * (Bit32u)val) / GUS_RATE;
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

	void ShowAddr(void) {
		LOG_MSG("Chan %d Start %d End %d Current %d", channum, StartAddr>>9, EndAddr>>9, CurAddr>>9);
	}


	void generateSamples(Bit16s * stream,Bit32u len) {
		int i;
		Bit16s tmpsamp;
		bool eightbit;
		eightbit = ((voiceCont & 0x4) == 0);
	
		notifyonce = false;

		//if((voiceCont & 0x4) == 0) {
			// 8-bit data
			for(i=0;i<len;i++) {

				tmpsamp = GetSample(RealDelta, CurAddr, eightbit);
			
				if(CurVolume>4095) CurVolume = 4095;
				if(CurVolume<0) CurVolume = 0;
				tmpsamp = (tmpsamp * vol16bit[CurVolume]) >> 12;

				stream[i<<1] = (Bit16s)(((Bit32s)tmpsamp * (Bit32s)leftvol)>>8) ;
				stream[(i<<1)+1] = (Bit16s)(((Bit32s)tmpsamp * rightvol)>>8);
				
				if(dir) {
					if (moving) CurAddr -= RealDelta;
					if ((!eightbit) && (moving)) CurAddr -= RealDelta;
					
					if(CurAddr <= StartAddr) {
						if((VolControl & 0x4) == 0) {
							if((voiceCont & 0x8) != 0) {
								if((voiceCont & 0x10) != 0) {
									dir = !dir;
								} else {
									CurAddr = EndAddr;
								}

							} else {
								moving = false;
								//NotifyEndSamp();
								//break;
							}
							NotifyEndSamp();
						} else {
							NotifyEndSamp();

						}
						
					}

				} else {
					if (moving) CurAddr += RealDelta;
					if ((!eightbit) && (moving)) CurAddr += RealDelta;
					//LOG_MSG("Cur %x to %x or (%x, %x) %d", CurAddr >> 9, EndAddr >> 9, CurAddr, EndAddr, (CurAddr >= EndAddr) );
					if(CurAddr >= EndAddr) {
						if((VolControl & 0x4) == 0) {
							if((voiceCont & 0x8) != 0) {
								if((voiceCont & 0x10) != 0) {
									dir = !dir;
								} else {
									CurAddr = StartAddr;
								}

							} else {
								moving = false;
								//NotifyEndSamp();
								//break;
							}
							NotifyEndSamp();
						} else {
							NotifyEndSamp();
						}
					}
				}


				// Update volume 
				if(ramping) {
					nextramp -= myGUS.mupersamp;
					bool flagged;
					flagged = false;
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
							nextramp += myGUS.muperchan * 8;
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
		//} else {
			// 16-bit data
		//	LOG_MSG("16-bit data not supported yet!");

		//}
	}


};


GUSChannels *guschan[32];

GUSChannels *curchan;


static void GUSReset(void)
{
	if((myGUS.gRegData & 0x1) == 0x1) {
		// Reset
		myGUS.timerReg = 85;
		memset(&myGUS.irq, 0, sizeof(myGUS.irq));
		//IRQFifo.stackpos = -1;
	}
	if((myGUS.gRegData & 0x4) != 0) {
		myGUS.irqenabled = true;
	} else {
		myGUS.irqenabled = false;
	}
	//LOG_MSG("Gus reset");
	myGUS.timers[0].active = false;
	myGUS.timers[1].active = false;
	int i;
	for(i=0;i<32;i++) {
		guschan[i]->WriteVoiceCtrl(0x3);

	}
	IRQFifo.stackpos = -1;


	//MIXER_Enable(gus_chan,false);

}



static Bit16u ExecuteReadRegister(void) {
	Bit8u tmpreg;

	switch (myGUS.gRegSelect) {
	case 0x41:

		tmpreg = myGUS.DMAControl & 0xbf;
		if(myGUS.irq.DMATC) tmpreg |= 0x40;

		myGUS.irq.DMATC = false;
		PIC_DeActivateIRQ(myGUS.irq1);

		return (Bit16u)(tmpreg << 8);
	case 0x42:
		return myGUS.dmaAddr;
	case 0x49:
		tmpreg = myGUS.DMAControl & 0xbf;
		if(myGUS.irq.DMATC) tmpreg |= 0x40;

		myGUS.irq.DMATC = false;
		PIC_DeActivateIRQ(myGUS.irq1);
		//LOG_MSG("Read sampling status, returned 0x%x", tmpreg);

		return (Bit16u)(tmpreg << 8);
	case 0x80:
		if(curchan != NULL) {
			Bit8u sndout;
			sndout = curchan->voiceCont & 0xFC;
			if(!curchan->moving) sndout |= 0x3;

			return (Bit16u)(sndout<< 8);
		} else return 0x0300;

	case 0x82:
		if(curchan != NULL) {
			return (curchan->StartAddr >> 16);
		} else return 0x0000;
	case 0x83:
		if(curchan != NULL) {
			return (curchan->StartAddr & 0xffff);
		} else return 0x0000;
	case 0x89:
		if(curchan != NULL) {
			return (curchan->CurVolume << 4);
		} else return 0x0000;
	case 0x8a:
		if(curchan != NULL) {
			return (curchan->CurAddr >> 16);
		} else return 0x0000;

	case 0x8b:
		if(curchan != NULL) {
			return (curchan->CurAddr & 0xFFFF);
		} else return 0x0000;
	case 0x8d:
		if(curchan != NULL) {
			Bit8u volout;
			volout = curchan->VolControl & 0xFC;
			if(!curchan->ramping) volout |= 0x3;
			//LOG_MSG("Ret at 8D %x - chan %d - ramp %d move %d rate %d pos %d dir %d py %d", volout, curchan->channum, curchan->ramping, curchan->moving, curchan->VolRampRate & 0x3f, vol16bit[curchan->CurVolume], curchan->voldir, curchan->playing);
			return (volout << 8);
		} else return 0x0300;

	case 0x8f: 
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
		LOG_MSG("Read Register num 0x%x", myGUS.gRegSelect);
		return myGUS.gRegData;
	}
}

  
static void ExecuteGlobRegister(void) {
	//LOG_MSG("Access global register %x with %x", myGUS.gRegSelect, myGUS.gRegData);
	switch(myGUS.gRegSelect) {
	case 0x0:
		if(curchan != NULL) {
			curchan->WriteVoiceCtrl((Bit8u)myGUS.gRegData);
		}
		break;
	case 0x1:
		if(curchan != NULL) {
			curchan->WriteFreqCtrl(myGUS.gRegData);
		}
		break;
	case 0x2:
		if(curchan != NULL) {
			Bit32u tmpaddr = myGUS.gRegData << 16;
			curchan->StartAddr = (curchan->StartAddr & 0xFFFF) | tmpaddr;
		}
		break;
	case 0x3:
		if(curchan != NULL) {
			Bit32u tmpaddr = (Bit32u)(myGUS.gRegData);
			curchan->StartAddr = (curchan->StartAddr & 0x1FFF0000) | tmpaddr;
		}
		break;
	case 0x4:
		if(curchan != NULL) {
			Bit32u tmpaddr = (Bit32u)myGUS.gRegData << 16;
			curchan->EndAddr = (curchan->EndAddr & 0xFFFF) | tmpaddr;
		}
		break;
	case 0x5:
		if(curchan != NULL) {
			Bit32u tmpaddr = (Bit32u)(myGUS.gRegData);
			curchan->EndAddr = (curchan->EndAddr & 0x1FFF0000) | tmpaddr;
		}
		break;
	case 0x6:
		if(curchan != NULL) {
			Bit8u tmpdata = (Bit8u)myGUS.gRegData;
			curchan->VolRampRate = tmpdata;
		}
		break;
	case 0x7:
		if(curchan != NULL) {
			Bit8u tmpdata = (Bit8u)myGUS.gRegData;
			curchan->VolRampStart = vol8bit[tmpdata];
			curchan->VolRampStartOrg = tmpdata << 4;
		}
		break;
	case 0x8:
		if(curchan != NULL) {
			Bit8u tmpdata = (Bit8u)myGUS.gRegData;
			curchan->VolRampEnd = vol8bit[tmpdata];	
			curchan->VolRampEndOrg = tmpdata << 4;
		}
		break;
	case 0x9:
		if(curchan != NULL) {
			Bit16u tmpdata = (Bit16u)myGUS.gRegData >> 4;
			curchan->CurVolume = tmpdata;
		}
		break;
	case 0xA:
		if(curchan != NULL) {
			curchan->CurAddr = (curchan->CurAddr & 0xFFFF) | ((Bit32u)myGUS.gRegData << 16);
		}
		break;
	case 0xB:
		if(curchan != NULL) {
			curchan->CurAddr = (curchan->CurAddr & 0xFFFF0000) | ((Bit32u)myGUS.gRegData);
		}
		break;
	case 0xC:
		if(curchan != NULL) {
			curchan->WritePanPot((Bit8u)myGUS.gRegData);
		}
		break;
	case 0xD:
		if(curchan != NULL) {
			curchan->WriteVolControl((Bit8u)myGUS.gRegData);
		}
		break;
	case 0xE:
		myGUS.activechan = myGUS.gRegData & 63;
		if(myGUS.activechan < 14) myGUS.activechan = 14;
		MIXER_Enable(gus_chan,true);
		myGUS.basefreq = (Bit32u)((float)1000000/(1.619695497*(float)myGUS.activechan));
		
		float simple;
		simple = (1.0 / (float)GUS_RATE) / 0.000001;
		myGUS.mupersamp = (Bit16u)simple;
		myGUS.muperchan = (Bit16u)((float)1.6 * (float)myGUS.activechan);
		LOG_MSG("GUS set to %d channels", myGUS.activechan);
		break;
	case 0x41:
		myGUS.DMAControl = (Bit8u)myGUS.gRegData;
		if ((myGUS.DMAControl & 0x1) != 0) {
			//LOG_MSG("GUS request DMA transfer");
			if(DmaChannels[myGUS.dma1]->enabled) UseDMA(DmaChannels[myGUS.dma1]);
		}
		break;
	case 0x42:
		myGUS.dmaAddr = myGUS.gRegData;
		break;
	case 0x43:
		myGUS.gDramAddr = (0xff0000 & myGUS.gDramAddr) | ((Bit32u)myGUS.gRegData);
		break;
	case 0x44:
		myGUS.gDramAddr = (0xffff & myGUS.gDramAddr) | ((Bit32u)myGUS.gRegData) << 16;
		break;
	case 0x45:
		myGUS.TimerControl = (Bit8u)myGUS.gRegData;
		if((myGUS.TimerControl & 0x08) !=0) myGUS.timers[1].countdown = myGUS.timers[1].setting;
		if((myGUS.TimerControl & 0x04) !=0) myGUS.timers[0].countdown = myGUS.timers[0].setting;
		myGUS.irq.T1 = false;
		myGUS.irq.T2 = false;
		PIC_DeActivateIRQ(myGUS.irq1);
		break;

	case 0x46:
		myGUS.timers[0].bytetimer = (Bit8u)myGUS.gRegData;
		myGUS.timers[0].setting = ((Bit32s)0xff - (Bit32s)myGUS.timers[0].bytetimer) * (Bit32s)80;
		myGUS.timers[0].countdown = myGUS.timers[0].setting;
		break;
	case 0x47:
		myGUS.timers[1].bytetimer = (Bit8u)myGUS.gRegData;
		myGUS.timers[1].setting = ((Bit32s)0xff - (Bit32s)myGUS.timers[1].bytetimer) * (Bit32s)360;
		myGUS.timers[1].countdown = myGUS.timers[1].setting;
		break;
	case 0x49:
		myGUS.SampControl = (Bit8u)myGUS.gRegData;
		if ((myGUS.SampControl & 0x1) != 0) {
			if(DmaChannels[myGUS.dma1]->enabled) UseDMA(DmaChannels[myGUS.dma1]);
		}
		break;
	case 0x4c:
		GUSReset();
		break;
	default:
		LOG_MSG("Unimplemented global register %x -- %x", myGUS.gRegSelect, myGUS.gRegData);
	}
	return;
}


static Bit16u read_gus16(Bit32u port) {

	return ExecuteReadRegister();
}

static void write_gus16(Bit32u port,Bit16u val) {
	myGUS.gRegData = val;
	ExecuteGlobRegister();
}


static Bit8u read_gus(Bit32u port) {

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
		return ExecuteReadRegister() & 0xff;
	case 0x305:
		return ExecuteReadRegister() >> 8;
	case 0x307:
		if(myGUS.gDramAddr < sizeof(GUSRam)) {
			return GUSRam[myGUS.gDramAddr];
		} else {
			return 0;
		}
	default:
		LOG_MSG("Read GUS at port 0x%x", port);
		break;
	}

	return 0xff;
}


static void write_gus(Bit32u port,Bit8u val) {
	FILE *fp;

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
		
		fp = fopen("gusdump.raw","wb");
		fwrite(&GUSRam[0],1024*1024,1,fp);
		fclose(fp);


		//LOG_MSG("Timer output reg %x", val);
		break;
	case 0x20b:
		if((myGUS.mixControl & 0x40) != 0) {
			// IRQ configuration
			Bit8u temp = val & 0x7; // Select GF1 irq
			if(myGUS.irq1 == irqtable[temp]) {
			} else {
				LOG_MSG("Attempt to assign GUS to wrong IRQ - at %x set to %x", myGUS.irq1, irqtable[temp]);
			}
		} else {
			// DMA configuration
			Bit8u temp = val & 0x7; // Select playback IRQ
			if(myGUS.dma1 == dmatable[temp]) {
			} else {
				LOG_MSG("Attempt to assign GUS to wrong DMA - at %x, assigned %x", myGUS.dma1, dmatable[temp]);
			}
		}

		break;
	case 0x302:
		myGUS.gCurChannel = val;
		curchan = guschan[val];
		break;
	case 0x303:
		myGUS.gRegSelect = val;
		myGUS.gRegData = 0;
		break;
	case 0x304:
		myGUS.gRegData = (0x00ff & myGUS.gRegData) | val << 8;
		ExecuteGlobRegister();
		break;
	case 0x305:
		myGUS.gRegData = (0xff00 & myGUS.gRegData) | val;
		ExecuteGlobRegister();
		break;

	case 0x307:
		if(myGUS.gDramAddr < sizeof(GUSRam)) GUSRam[myGUS.gDramAddr] = val;
		break;
	default:
		LOG_MSG("Write GUS at port 0x%x with %x", port, val);
		break;
	}
	

}

static void UseDMA(DmaChannel *useDMA) {
	Bit32s dmaaddr = myGUS.dmaAddr << 4;
	if((myGUS.DMAControl & 0x2) == 0) {
		//Write data into UltraSound
		Bit32s comsize = useDMA->currcnt;
		
		useDMA->Read(useDMA->currcnt,&GUSRam[dmaaddr]);
		if((myGUS.DMAControl & 0x80) != 0) {
			//Invert the MSB to convert twos compliment form

			int i;
			if((myGUS.DMAControl & 0x40) == 0) {
				// 8-bit data
				for(i=dmaaddr;i<(dmaaddr+comsize);i++) GUSRam[i] ^= 0x80;
			} else {
				// 16-bit data
				for(i=dmaaddr+1;i<(dmaaddr+comsize-1);i+=2) GUSRam[i] ^= 0x80;
			}
		}
	} else {
		//Read data out of UltraSound
		useDMA->Write(useDMA->currcnt,&GUSRam[dmaaddr]);

	}
}

static void GUS_DMA_Callback(void *useChannel, bool tc) {
	DmaChannel *myDMA;
	myDMA = (DmaChannel *)useChannel;

	if(tc) {
		if((myGUS.DMAControl & 0x20) != 0) {
			myGUS.irq.DMATC = true;
			PIC_ActivateIRQ(myGUS.irq2);
		}
	} else {
		if ((myGUS.DMAControl & 0x1) != 0)  UseDMA(myDMA);
	}
	
	
}

static void GUS_CallBack(Bit8u * stream,Bit32u len) {

	Bit16s *bufptr;
	Bit16s buffer[4096];
	Bit32s tmpbuf[4096];

	memset(&buffer[0],0,len*4);
	memset(&tmpbuf[0],0,len*8);

	int i,t;
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
			if(myGUS.timers[0].countdown < 0) {
				if((myGUS.TimerControl & 0x04) !=0) {
					PIC_ActivateIRQ(myGUS.irq1);
				}
				myGUS.irq.T1 = true;
				//LOG_MSG("T1 timer expire");
				//myGUS.timers[0].countdown = myGUS.timers[0].setting;
			}
		}
	}
	if(myGUS.timers[1].active) {
		if(!myGUS.irq.T2) {
			myGUS.timers[1].countdown-=(len * (Bit32u)myGUS.mupersamp);
			if(myGUS.timers[1].countdown < 0) {
				if((myGUS.TimerControl & 0x08) !=0) {
					PIC_ActivateIRQ(myGUS.irq1);
				}
				myGUS.irq.T2 = true;
				//LOG_MSG("T2 timer expire");
				//myGUS.timers[1].countdown = myGUS.timers[1].setting;
			}
		}
	}

		

	bufptr = (Bit16s *)stream;
	//for(i=0;i<len*2;i++) bufptr[i] = (Bit16s)(tmpbuf[i] / myGUS.activechan);
	for(i=0;i<len*2;i++) bufptr[i] = (Bit16s)(tmpbuf[i]);
	//for(i=0;i<len;i++) {
	//	bufptr[i] = (Bit16s)(tmpbuf[i]);
	//}

}

void MakeTables(void) 
{
	int i;
	float testvol;

	for(i=0;i<256;i++) {
		float a,b;
		a = pow(2,(float)(i >> 4));
		b = 1.0+((float)(i & 0xf))/(float)16;
		a *= b;
		a /= 16;
		vol8bit[i] = (Bit16u)a;
	}
	for(i=0;i<4096;i++) {
		float a,b;
		a = pow(2,(float)(i >> 8));
		b = 1.0+((float)(i & 0xff))/(float)256;
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
	
	IO_RegisterWriteHandler(0x302 + GUS_BASE,write_gus,"GF1 Page Register");
	IO_RegisterReadHandler(0x302 + GUS_BASE,read_gus,"GF1 Page Register");

	IO_RegisterWriteHandler(0x303 + GUS_BASE,write_gus,"GF1 Global Register Select");
	IO_RegisterReadHandler(0x303 + GUS_BASE,read_gus,"GF1 Global Register Select");

	IO_RegisterWriteHandler(0x304 + GUS_BASE,write_gus,"GF1 Global Data Low Byte");
	IO_RegisterReadHandler(0x304 + GUS_BASE,read_gus,"GF1 Global Data Low Byte");

	IO_RegisterWriteWHandler(0x304 + GUS_BASE,write_gus16);
	IO_RegisterReadWHandler(0x304 + GUS_BASE,read_gus16);

	IO_RegisterWriteHandler(0x305 + GUS_BASE,write_gus,"GF1 Global Data High Byte");
	IO_RegisterReadHandler(0x305 + GUS_BASE,read_gus,"GF1 Global Data High Byte");

	IO_RegisterReadHandler(0x206 + GUS_BASE,read_gus,"GF1 IRQ Status Register");

	IO_RegisterWriteHandler(0x208 + GUS_BASE,write_gus,"Timer Control Reg");
	IO_RegisterReadHandler(0x208 + GUS_BASE,read_gus,"Timer Control Reg");

	IO_RegisterWriteHandler(0x209 + GUS_BASE,write_gus,"Timer Data IO");

	IO_RegisterWriteHandler(0x307 + GUS_BASE,write_gus,"DRAM IO");
	IO_RegisterReadHandler(0x307 + GUS_BASE,read_gus,"DRAM IO");

	// Board Only 

	IO_RegisterWriteHandler(0x200 + GUS_BASE,write_gus,"Mix Control Register");
	IO_RegisterReadHandler(0x20A + GUS_BASE,read_gus,"GUS Undocumented");
	IO_RegisterWriteHandler(0x20B + GUS_BASE,write_gus,"IRQ/DMA Control Register");

	PIC_RegisterIRQ(myGUS.irq1,0,"GUS");
	PIC_RegisterIRQ(myGUS.irq2,0,"GUS");

	DmaChannels[myGUS.dma1]->RegisterCallback(GUS_DMA_Callback);

	MakeTables();

	int i;
	for(i=0;i<32;i++) {
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



