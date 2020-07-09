/*
 *  Copyright (C) 2002-2020  The DOSBox Team
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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "dosbox.h"

#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string.h>

#include "inout.h"
#include "mixer.h"
#include "dma.h"
#include "pic.h"
#include "setup.h"
#include "shell.h"
#include "math.h"
#include "regs.h"
using namespace std;

//Extra bits of precision over normal gus
#define WAVE_FRACT 9
#define WAVE_FRACT_MASK ((1 << WAVE_FRACT)-1)
#define WAVE_MSWMASK ((1 << 16)-1)
#define WAVE_LSWMASK (0xffffffff ^ WAVE_MSWMASK)

#define GUS_PAN_POSITIONS 16 // 0 face-left, 7 face-forward, and 15 face-right
#define GUS_VOLUME_POSITIONS 4096
#define GUS_VOLUME_SCALE_DIV 1.002709201 // 0.0235 dB increments
#define GUS_RAM_SIZE         1048576 // 1 MB
#define GUS_BASE myGUS.portbase
#define GUS_RATE myGUS.rate
#define LOG_GUS              0

#define WCTRL_STOPPED			0x01
#define WCTRL_STOP				0x02
#define WCTRL_16BIT				0x04
#define WCTRL_LOOP				0x08
#define WCTRL_BIDIRECTIONAL		0x10
#define WCTRL_IRQENABLED		0x20
#define WCTRL_DECREASING		0x40
#define WCTRL_IRQPENDING		0x80

uint8_t adlib_commandreg = 0u;
static MixerChannel * gus_chan;
static uint8_t irqtable[8] = {0, 2, 5, 3, 7, 11, 12, 15};
static uint8_t dmatable[8] = {0, 1, 3, 5, 6, 7, 0, 0};
static uint8_t GUSRam[GUS_RAM_SIZE] = {0u};
static std::array<float, GUS_VOLUME_POSITIONS> vol_scalars = {0.0f};

struct Frame {
	float left = 0.0f;
	float right = 0.0f;
};

static std::array<Frame, GUS_PAN_POSITIONS> pan_scalars = {};

class GUSChannels;
static void CheckVoiceIrq(void);

struct GFGus {
	uint8_t gRegSelect = 0u;
	Bit16u gRegData;
	Bit32u gDramAddr;
	Bit16u gCurChannel;

	uint8_t DMAControl = 0u;
	Bit16u dmaAddr;
	uint8_t TimerControl = 0u;
	uint8_t SampControl = 0u;
	uint8_t mixControl = 0u;
	uint8_t ActiveChannels = 0u;
	Bit32u basefreq;

	struct GusTimer {
		uint8_t value = 0u;
		bool reached;
		bool raiseirq;
		bool masked;
		bool running;
		float delay;
	} timers[2];
	Bit32u rate;
	Bitu portbase;
	uint8_t dma1 = 0u;
	uint8_t dma2 = 0u;

	uint8_t irq1 = 0u;
	uint8_t irq2 = 0u;

	bool irqenabled;
	bool ChangeIRQDMA;
	// IRQ status register values
	uint8_t IRQStatus = 0u;
	Bit32u ActiveMask;
	uint8_t IRQChan = 0u;
	Bit32u RampIRQ;
	Bit32u WaveIRQ;
} myGUS;

Bitu DEBUG_EnableDebugger(void);

static void GUS_DMA_Callback(DmaChannel * chan,DMAEvent event);

class GUSChannels {
public:
	void WriteWaveCtrl(uint8_t val);

	typedef float (GUSChannels::*get_sample_f)() const;
	get_sample_f getSample = &GUSChannels::GetSample8;

	Bit32u WaveStart;
	Bit32u WaveEnd;
	Bit32u WaveAddr;
	Bit32u WaveAdd;
	uint8_t  WaveCtrl = 0u;
	Bit16u WaveFreq;

	uint32_t StartVolIndex = 0u;
	uint32_t EndVolIndex = 0u;
	uint32_t CurrentVolIndex = 0u;
	uint32_t IncrVolIndex = 0u;

	uint8_t RampRate = 0u;
	uint8_t RampCtrl = 0u;

	uint8_t PanPot = 0u;
	uint8_t channum = 0u;
	Bit32u irqmask;
	Bit32s VolLeft;
	Bit32s VolRight;

	GUSChannels(uint8_t num) { 
		channum = num;
		irqmask = 1 << num;
		WaveStart = 0;
		WaveEnd = 0;
		WaveAddr = 0;
		WaveAdd = 0;
		WaveFreq = 0;
		WaveCtrl = 3;
		RampRate = 0;
		RampCtrl = 3;
		VolLeft = 0;
		VolRight = 0;
		PanPot = 0x7;
	}

	// Fetch the next 8-bit sample from GUS memory returned as a floating
	// point type containing a value that spans the 16-bit signed range.
	// This implementation preserves up to 3 significant figures of the
	// inter-wave portion previously lost due to integer bit-shifting.
	inline float GetSample8() const
	{
		const uint32_t useAddr = WaveAddr >> WAVE_FRACT;
		float w1 = static_cast<int8_t>(GUSRam[useAddr]);
		// add a fraction of the next sample
		if (WaveAdd < (1 << WAVE_FRACT)) {
			const uint32_t nextAddr = (useAddr + 1) & (GUS_RAM_SIZE - 1);
			const float w2 = static_cast<float>(
			        static_cast<int8_t>(GUSRam[nextAddr]));
			const float diff = w2 - w1;
			constexpr float max_wave = static_cast<float>(1 << WAVE_FRACT);
			const float scale = (WaveAddr & WAVE_FRACT_MASK) / max_wave;
			w1 += diff * scale;

			// Ensure the sample with added inter-wave portion is
			// still within the true 8-bit range, albeit with far
			// more accuracy.
			assert(w1 <= std::numeric_limits<int8_t>::max() ||
			       w1 >= std::numeric_limits<int8_t>::min());
		}
		constexpr auto to_16bit_range =
		        1 << (std::numeric_limits<int16_t>::digits -
		              std::numeric_limits<int8_t>::digits);
		return w1 * to_16bit_range;
	}

	// Fetch the next 16-bit sample from GUS memory as a floating point
	// value.
	inline float GetSample16() const
	{
		// Formula used to convert addresses for use with 16-bit samples
		const uint32_t base = WaveAddr >> WAVE_FRACT;
		const uint32_t holdAddr = base & 0xc0000L;
		const uint32_t useAddr = holdAddr | ((base & 0x1ffffL) << 1);

		float w1 = static_cast<float>(
		        GUSRam[useAddr] |
		        (static_cast<int8_t>(GUSRam[useAddr + 1]) << 8));

		// add a fraction of the next sample
		if (WaveAdd < (1 << WAVE_FRACT)) {
			const float w2 = static_cast<float>(
			        static_cast<int8_t>(GUSRam[useAddr + 2]) |
			        (static_cast<int8_t>(GUSRam[useAddr + 3]) << 8));
			const float diff = w2 - w1;
			constexpr float max_wave = static_cast<float>(1 << WAVE_FRACT);
			const float scale = (WaveAddr & WAVE_FRACT_MASK) / max_wave;
			w1 += diff * scale;

			// Ensure the sample with added inter-wave portion is
			// still within the true 16-bit range.
			assert(w1 <= std::numeric_limits<int16_t>::max() ||
			       w1 >= std::numeric_limits<int16_t>::min());
		}
		return w1;
	}

	void WriteWaveFreq(uint16_t val)
	{
		WaveFreq = val;
		const float rate_ratio = static_cast<float>(myGUS.basefreq) / GUS_RATE;
		WaveAdd = static_cast<uint32_t>(val * rate_ratio / 2.0f);
	}

	inline uint8_t ReadWaveCtrl() {
		uint8_t ret=WaveCtrl;
		if (myGUS.WaveIRQ & irqmask) ret|=0x80;
		return ret;
	}
	void UpdateWaveRamp(void) { 
		WriteWaveFreq(WaveFreq);
		WriteRampRate(RampRate);
	}

	void WritePanPot(uint8_t val)
	{
		assertm(val >= 0 && val <= 15,
		        "Valid pan positions range from 0 to 15");
		PanPot = val;
	}

	uint8_t ReadPanPot(void) {
		return PanPot;
	}
	void WriteRampCtrl(uint8_t val) {
		Bit32u old=myGUS.RampIRQ;
		RampCtrl = val & 0x7f;
		//Manually set the irq
		if ((val & 0xa0)==0xa0) 
			myGUS.RampIRQ|=irqmask;
		else 
			myGUS.RampIRQ&=~irqmask;
		if (old != myGUS.RampIRQ) 
			CheckVoiceIrq();
	}
	INLINE uint8_t ReadRampCtrl(void) {
		uint8_t ret=RampCtrl;
		if (myGUS.RampIRQ & irqmask) ret |= 0x80;
		return ret;
	}
	void WriteRampRate(uint8_t val) {
		RampRate = val;
		double frameadd = (double)(RampRate & 63)/(double)(1 << (3*(val >> 6)));
		double realadd = frameadd*(double)myGUS.basefreq/(double)GUS_RATE;
		IncrVolIndex = static_cast<uint32_t>(realadd);
	}
	INLINE void WaveUpdate(void) {
		if (WaveCtrl & ( WCTRL_STOP | WCTRL_STOPPED)) return;
		Bit32s WaveLeft;
		if (WaveCtrl & WCTRL_DECREASING) {
			WaveAddr -= WaveAdd;
			WaveLeft = WaveStart-WaveAddr;
		} else {
			WaveAddr += WaveAdd;
			WaveLeft = WaveAddr-WaveEnd;
		}
		//Not yet reaching a boundary
		if (WaveLeft<0) 
			return;
		/* Generate an IRQ if needed */
		if (WaveCtrl & 0x20) {
			myGUS.WaveIRQ|=irqmask;
		}
		/* Check for not being in PCM operation */
		if (RampCtrl & 0x04) return;
		/* Check for looping */
		if (WaveCtrl & WCTRL_LOOP) {
			/* Bi-directional looping */
			if (WaveCtrl & WCTRL_BIDIRECTIONAL) WaveCtrl^= WCTRL_DECREASING;
			WaveAddr = (WaveCtrl & WCTRL_DECREASING) ? (WaveEnd-WaveLeft) : (WaveStart+WaveLeft);
		} else {
			WaveCtrl|=1;	//Stop the channel
			WaveAddr = (WaveCtrl & WCTRL_DECREASING) ? WaveStart : WaveEnd;
		}
	}

	inline void RampUpdate()
	{
		/* Check if ramping enabled */
		if (RampCtrl & 0x3) return;
		Bit32s RemainingVolIndexes;
		if (RampCtrl & 0x40) {
			CurrentVolIndex -= IncrVolIndex;
			RemainingVolIndexes = StartVolIndex - CurrentVolIndex;
		} else {
			CurrentVolIndex += IncrVolIndex;
			RemainingVolIndexes = CurrentVolIndex - EndVolIndex;
		}
		if (RemainingVolIndexes < 0) {
			return;
		}
		/* Generate an IRQ if needed */
		if (RampCtrl & 0x20) {
			myGUS.RampIRQ|=irqmask;
		}
		/* Check for looping */
		if (RampCtrl & 0x08) {
			/* Bi-directional looping */
			if (RampCtrl & 0x10) RampCtrl^=0x40;
			CurrentVolIndex = (RampCtrl & 0x40)
			                          ? (EndVolIndex - RemainingVolIndexes)
			                          : (StartVolIndex +
			                             RemainingVolIndexes);
		} else {
			RampCtrl|=1;	//Stop the channel
			CurrentVolIndex = (RampCtrl & 0x40) ? StartVolIndex
			                                    : EndVolIndex;
		}
	}

	void generateSamples(int32_t *stream, uint32_t len)
	{
		if (RampCtrl & WaveCtrl & 3) // Channel is disabled
			return;

		uint16_t tally = 0;
		while (tally++ < len) {
			const float sample = (this->*getSample)() *
			                     vol_scalars[CurrentVolIndex];
			*(stream++) += static_cast<int32_t>(
			        sample * pan_scalars[PanPot].left);
			*(stream++) += static_cast<int32_t>(
			        sample * pan_scalars[PanPot].right);
			WaveUpdate();
			RampUpdate();
		}
	}
};

void GUSChannels::WriteWaveCtrl(uint8_t val)
{
	Bit32u oldirq = myGUS.WaveIRQ;
	WaveCtrl = val & 0x7f;
	getSample = (WaveCtrl & WCTRL_16BIT) ? &GUSChannels::GetSample16
	                                     : &GUSChannels::GetSample8;

	if ((val & 0xa0) == 0xa0)
		myGUS.WaveIRQ |= irqmask;
	else
		myGUS.WaveIRQ &= ~irqmask;
	if (oldirq != myGUS.WaveIRQ)
		CheckVoiceIrq();
}

static GUSChannels *guschan[32];
static GUSChannels *curchan;

static void GUSReset(void) {
	if((myGUS.gRegData & 0x1) == 0x1) {
		// Reset
		adlib_commandreg = 85;
		myGUS.IRQStatus = 0;
		myGUS.timers[0].raiseirq = false;
		myGUS.timers[1].raiseirq = false;
		myGUS.timers[0].reached = false;
		myGUS.timers[1].reached = false;
		myGUS.timers[0].running = false;
		myGUS.timers[1].running = false;

		myGUS.timers[0].value = 0xff;
		myGUS.timers[1].value = 0xff;
		myGUS.timers[0].delay = 0.080f;
		myGUS.timers[1].delay = 0.320f;

		myGUS.ChangeIRQDMA = false;
		myGUS.mixControl = 0x0b;	// latches enabled by default LINEs disabled
		// Stop all channels
		int i;
		for(i=0;i<32;i++) {
			guschan[i]->CurrentVolIndex = 0u;
			guschan[i]->WriteWaveCtrl(0x1);
			guschan[i]->WriteRampCtrl(0x1);
			guschan[i]->WritePanPot(0x7);
		}
		myGUS.IRQChan = 0;
	}
	if ((myGUS.gRegData & 0x4) != 0) {
		myGUS.irqenabled = true;
	} else {
		myGUS.irqenabled = false;
	}
}

static INLINE void GUS_CheckIRQ(void) {
	if (myGUS.IRQStatus && (myGUS.mixControl & 0x08)) 
		PIC_ActivateIRQ(myGUS.irq1);

}

static void CheckVoiceIrq(void) {
	myGUS.IRQStatus&=0x9f;
	Bitu totalmask=(myGUS.RampIRQ|myGUS.WaveIRQ) & myGUS.ActiveMask;
	if (!totalmask) return;
	if (myGUS.RampIRQ) myGUS.IRQStatus|=0x40;
	if (myGUS.WaveIRQ) myGUS.IRQStatus|=0x20;
	GUS_CheckIRQ();
	for (;;) {
		Bit32u check=(1 << myGUS.IRQChan);
		if (totalmask & check) return;
		myGUS.IRQChan++;
		if (myGUS.IRQChan>=myGUS.ActiveChannels) myGUS.IRQChan=0;
	}
}

static Bit16u ExecuteReadRegister(void) {
	uint8_t tmpreg;
//	LOG_MSG("Read global reg %x",myGUS.gRegSelect);
	switch (myGUS.gRegSelect) {
	case 0x41: // Dma control register - read acknowledges DMA IRQ
		tmpreg = myGUS.DMAControl & 0xbf;
		tmpreg |= (myGUS.IRQStatus & 0x80) >> 1;
		myGUS.IRQStatus&=0x7f;
		return (Bit16u)(tmpreg << 8);
	case 0x42:  // Dma address register
		return myGUS.dmaAddr;
	case 0x45:  // Timer control register.  Identical in operation to Adlib's timer
		return (Bit16u)(myGUS.TimerControl << 8);
		break;
	case 0x49:  // Dma sample register
		tmpreg = myGUS.DMAControl & 0xbf;
		tmpreg |= (myGUS.IRQStatus & 0x80) >> 1;
		return (Bit16u)(tmpreg << 8);
	case 0x80: // Channel voice control read register
		if (curchan) return curchan->ReadWaveCtrl() << 8;
		else return 0x0300;

	case 0x82: // Channel MSB start address register
		if (curchan) return (Bit16u)(curchan->WaveStart >> 16);
		else return 0x0000;
	case 0x83: // Channel LSW start address register
		if (curchan) return (Bit16u)(curchan->WaveStart );
		else return 0x0000;

	case 0x89: // Channel volume register
		if (curchan)
			return (Bit16u)(curchan->CurrentVolIndex << 4);
		else
			return 0x0000;
	case 0x8a: // Channel MSB current address register
		if (curchan) return (Bit16u)(curchan->WaveAddr >> 16);
		else return 0x0000;
	case 0x8b: // Channel LSW current address register
		if (curchan) return (Bit16u)(curchan->WaveAddr );
		else return 0x0000;

	case 0x8d: // Channel volume control register
		if (curchan) return curchan->ReadRampCtrl() << 8;
		else return 0x0300;
	case 0x8f: // General channel IRQ status register
		tmpreg=myGUS.IRQChan|0x20;
		Bit32u mask;
		mask=1 << myGUS.IRQChan;
        if (!(myGUS.RampIRQ & mask)) tmpreg|=0x40;
		if (!(myGUS.WaveIRQ & mask)) tmpreg|=0x80;
		myGUS.RampIRQ&=~mask;
		myGUS.WaveIRQ&=~mask;
		CheckVoiceIrq();
		return (Bit16u)(tmpreg << 8);
	default:
#if LOG_GUS
		LOG_MSG("Read Register num 0x%x", myGUS.gRegSelect);
#endif
		return myGUS.gRegData;
	}
}

static void GUS_TimerEvent(Bitu val) {
	if (!myGUS.timers[val].masked) myGUS.timers[val].reached=true;
	if (myGUS.timers[val].raiseirq) {
		myGUS.IRQStatus|=0x4 << val;
		GUS_CheckIRQ();
	}
	if (myGUS.timers[val].running) 
		PIC_AddEvent(GUS_TimerEvent,myGUS.timers[val].delay,val);
}

 
static void ExecuteGlobRegister(void) {
	int i;
//	if (myGUS.gRegSelect|1!=0x44) LOG_MSG("write global register %x with %x", myGUS.gRegSelect, myGUS.gRegData);
	switch(myGUS.gRegSelect) {
	case 0x0:  // Channel voice control register
		if(curchan) curchan->WriteWaveCtrl((Bit16u)myGUS.gRegData>>8);
		break;
	case 0x1:  // Channel frequency control register
		if(curchan) curchan->WriteWaveFreq(myGUS.gRegData);
		break;
	case 0x2:  // Channel MSW start address register
		if (curchan) {
			Bit32u tmpaddr = (Bit32u)((myGUS.gRegData & 0x1fff) << 16);
			curchan->WaveStart = (curchan->WaveStart & WAVE_MSWMASK) | tmpaddr;
		}
		break;
	case 0x3:  // Channel LSW start address register
		if(curchan != NULL) {
			Bit32u tmpaddr = (Bit32u)(myGUS.gRegData);
			curchan->WaveStart = (curchan->WaveStart & WAVE_LSWMASK) | tmpaddr;
		}
		break;
	case 0x4:  // Channel MSW end address register
		if(curchan != NULL) {
			Bit32u tmpaddr = (Bit32u)(myGUS.gRegData & 0x1fff) << 16;
			curchan->WaveEnd = (curchan->WaveEnd & WAVE_MSWMASK) | tmpaddr;
		}
		break;
	case 0x5:  // Channel MSW end address register
		if(curchan != NULL) {
			Bit32u tmpaddr = (Bit32u)(myGUS.gRegData);
			curchan->WaveEnd = (curchan->WaveEnd & WAVE_LSWMASK) | tmpaddr;
		}
		break;
	case 0x6:  // Channel volume ramp rate register
		if(curchan != NULL) {
			uint8_t tmpdata = (Bit16u)myGUS.gRegData>>8;
			curchan->WriteRampRate(tmpdata);
		}
		break;
	case 0x7:  // Channel volume ramp start register  EEEEMMMM
		if(curchan != NULL) {
			uint8_t tmpdata = (Bit16u)myGUS.gRegData >> 8;
			curchan->StartVolIndex = tmpdata << 4;
		}
		break;
	case 0x8:  // Channel volume ramp end register  EEEEMMMM
		if(curchan != NULL) {
			uint8_t tmpdata = (Bit16u)myGUS.gRegData >> 8;
			curchan->EndVolIndex = tmpdata << 4;
		}
		break;
	case 0x9:  // Channel current volume register
		if(curchan != NULL) {
			Bit16u tmpdata = (Bit16u)myGUS.gRegData >> 4;
			curchan->CurrentVolIndex = tmpdata;
		}
		break;
	case 0xA:  // Channel MSW current address register
		if(curchan != NULL) {
			Bit32u tmpaddr = (Bit32u)(myGUS.gRegData & 0x1fff) << 16;
			curchan->WaveAddr = (curchan->WaveAddr & WAVE_MSWMASK) | tmpaddr;
		}
		break;
	case 0xB:  // Channel LSW current address register
		if(curchan != NULL) {
			Bit32u tmpaddr = (Bit32u)(myGUS.gRegData);
			curchan->WaveAddr = (curchan->WaveAddr & WAVE_LSWMASK) | tmpaddr;
		}
		break;
	case 0xC:  // Channel pan pot register
		if(curchan) curchan->WritePanPot((Bit16u)myGUS.gRegData>>8);
		break;
	case 0xD:  // Channel volume control register
		if(curchan) curchan->WriteRampCtrl((Bit16u)myGUS.gRegData>>8);
		break;
	case 0xE:  // Set active channel register
		myGUS.gRegSelect = myGUS.gRegData>>8;		//JAZZ Jackrabbit seems to assume this?
		myGUS.ActiveChannels = 1+((myGUS.gRegData>>8) & 63);
		if(myGUS.ActiveChannels < 14) myGUS.ActiveChannels = 14;
		if(myGUS.ActiveChannels > 32) myGUS.ActiveChannels = 32;
		myGUS.ActiveMask=0xffffffffU >> (32-myGUS.ActiveChannels);
		gus_chan->Enable(true);
		myGUS.basefreq = (Bit32u)(0.5 + 1000000.0/(1.619695497*(double)(myGUS.ActiveChannels)));
#if LOG_GUS
		LOG_MSG("GUS set to %d channels, freq %d", myGUS.ActiveChannels, myGUS.basefreq);
#endif
		for (i=0;i<myGUS.ActiveChannels;i++) guschan[i]->UpdateWaveRamp();
		break;
	case 0x10:  // Undocumented register used in Fast Tracker 2
		break;
	case 0x41:  // Dma control register
		myGUS.DMAControl = static_cast<uint8_t>(myGUS.gRegData>>8);
		GetDMAChannel(myGUS.dma1)->Register_Callback(
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
		myGUS.TimerControl = static_cast<uint8_t>(myGUS.gRegData>>8);
		myGUS.timers[0].raiseirq=(myGUS.TimerControl & 0x04)>0;
		if (!myGUS.timers[0].raiseirq) myGUS.IRQStatus&=~0x04;
		myGUS.timers[1].raiseirq=(myGUS.TimerControl & 0x08)>0;
		if (!myGUS.timers[1].raiseirq) myGUS.IRQStatus&=~0x08;
		break;
	case 0x46:  // Timer 1 control
		myGUS.timers[0].value = static_cast<uint8_t>(myGUS.gRegData>>8);
		myGUS.timers[0].delay = (0x100 - myGUS.timers[0].value) * 0.080f;
		break;
	case 0x47:  // Timer 2 control
		myGUS.timers[1].value = static_cast<uint8_t>(myGUS.gRegData>>8);
		myGUS.timers[1].delay = (0x100 - myGUS.timers[1].value) * 0.320f;
		break;
	case 0x49:  // DMA sampling control register
		myGUS.SampControl = static_cast<uint8_t>(myGUS.gRegData>>8);
		GetDMAChannel(myGUS.dma1)->Register_Callback(
			(myGUS.SampControl & 0x1)  ? GUS_DMA_Callback : 0);
		break;
	case 0x4c:  // GUS reset register
		GUSReset();
		break;
	default:
#if LOG_GUS
		LOG_MSG("Unimplemented global register %x -- %x", myGUS.gRegSelect, myGUS.gRegData);
#endif
		break;
	}
	return;
}


static Bitu read_gus(Bitu port,Bitu iolen) {
//	LOG_MSG("read from gus port %x",port);
	switch(port - GUS_BASE) {
	case 0x206:
		return myGUS.IRQStatus;
	case 0x208:
		uint8_t tmptime;
		tmptime = 0u;
		if (myGUS.timers[0].reached) tmptime |= (1 << 6);
		if (myGUS.timers[1].reached) tmptime |= (1 << 5);
		if (tmptime & 0x60) tmptime |= (1 << 7);
		if (myGUS.IRQStatus & 0x04) tmptime|=(1 << 2);
		if (myGUS.IRQStatus & 0x08) tmptime|=(1 << 1);
		return tmptime;
	case 0x20a:
		return adlib_commandreg;
	case 0x302:
		return static_cast<uint8_t>(myGUS.gCurChannel);
	case 0x303:
		return myGUS.gRegSelect;
	case 0x304:
		if (iolen==2) return ExecuteReadRegister() & 0xffff;
		else return ExecuteReadRegister() & 0xff;
	case 0x305:
		return ExecuteReadRegister() >> 8;
	case 0x307:
		if(myGUS.gDramAddr < GUS_RAM_SIZE) {
			return GUSRam[myGUS.gDramAddr];
		} else {
			return 0;
		}
	default:
#if LOG_GUS
		LOG_MSG("Read GUS at port 0x%x", port);
#endif
		break;
	}

	return 0xff;
}


static void write_gus(Bitu port,Bitu val,Bitu iolen) {
//	LOG_MSG("Write gus port %x val %x",port,val);
	switch(port - GUS_BASE) {
	case 0x200:
		myGUS.mixControl = static_cast<uint8_t>(val);
		myGUS.ChangeIRQDMA = true;
		return;
	case 0x208:
		adlib_commandreg = static_cast<uint8_t>(val);
		break;
	case 0x209:
//TODO adlib_commandreg should be 4 for this to work else it should just latch the value
		if (val & 0x80) {
			myGUS.timers[0].reached=false;
			myGUS.timers[1].reached=false;
			return;
		}
		myGUS.timers[0].masked=(val & 0x40)>0;
		myGUS.timers[1].masked=(val & 0x20)>0;
		if (val & 0x1) {
			if (!myGUS.timers[0].running) {
				PIC_AddEvent(GUS_TimerEvent,myGUS.timers[0].delay,0);
				myGUS.timers[0].running=true;
			}
		} else myGUS.timers[0].running=false;
		if (val & 0x2) {
			if (!myGUS.timers[1].running) {
				PIC_AddEvent(GUS_TimerEvent,myGUS.timers[1].delay,1);
				myGUS.timers[1].running=true;
			}
		} else myGUS.timers[1].running=false;
		break;
//TODO Check if 0x20a register is also available on the gus like on the interwave
	case 0x20b:
		if (!myGUS.ChangeIRQDMA) break;
		myGUS.ChangeIRQDMA=false;
		if (myGUS.mixControl & 0x40) {
			// IRQ configuration, only use low bits for irq 1
			if (irqtable[val & 0x7]) myGUS.irq1=irqtable[val & 0x7];
#if LOG_GUS
			LOG_MSG("Assigned GUS to IRQ %d", myGUS.irq1);
#endif
		} else {
			// DMA configuration, only use low bits for dma 1
			if (dmatable[val & 0x7]) myGUS.dma1=dmatable[val & 0x7];
#if LOG_GUS
			LOG_MSG("Assigned GUS to DMA %d", myGUS.dma1);
#endif
		}
		break;
	case 0x302:
		myGUS.gCurChannel = val & 31 ;
		curchan = guschan[myGUS.gCurChannel];
		break;
	case 0x303:
		myGUS.gRegSelect = static_cast<uint8_t>(val);
		myGUS.gRegData = 0;
		break;
	case 0x304:
		if (iolen==2) {
			myGUS.gRegData=(Bit16u)val;
			ExecuteGlobRegister();
		} else myGUS.gRegData = (Bit16u)val;
		break;
	case 0x305:
		myGUS.gRegData = (Bit16u)((0x00ff & myGUS.gRegData) | val << 8);
		ExecuteGlobRegister();
		break;
	case 0x307:
		if(myGUS.gDramAddr < GUS_RAM_SIZE) GUSRam[myGUS.gDramAddr] = static_cast<uint8_t>(val);
		break;
	default:
#if LOG_GUS
		LOG_MSG("Write GUS at port 0x%x with %x", port, val);
#endif
		break;
	}
}

static void GUS_DMA_Callback(DmaChannel * chan,DMAEvent event) {
	if (event!=DMA_UNMASKED) return;
	Bitu dmaaddr;
	//Calculate the dma address
	//DMA transfers can't cross 256k boundaries, so you should be safe to just determine the start once and go from there
	//Bit 2 - 0 = if DMA channel is an 8 bit channel(0 - 3).
	if (myGUS.DMAControl & 0x4)
		dmaaddr = (((myGUS.dmaAddr & 0x1fff) << 1) | (myGUS.dmaAddr & 0xc000)) << 4;
	else
		dmaaddr = myGUS.dmaAddr << 4;
	//Reading from dma?
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
				for(i=dmaaddr+1;i<(dmaaddr+read);i+=2) GUSRam[i] ^= 0x80;
			}
		}
	//Writing to dma
	} else {
		chan->Write(chan->currcnt+1,&GUSRam[dmaaddr]);
	}
	/* Raise the TC irq if needed */
	if((myGUS.DMAControl & 0x20) != 0) {
		myGUS.IRQStatus |= 0x80;
		GUS_CheckIRQ();
	}
	chan->Register_Callback(0);
}

static void GUS_CallBack(uint16_t len)
{
	Bit32s buffer[MIXER_BUFSIZE][2];
	memset(buffer, 0, len * sizeof(buffer[0]));

	for (Bitu i = 0; i < myGUS.ActiveChannels; i++) {
		guschan[i]->generateSamples(buffer[0], len);
	}
	gus_chan->AddSamples_s32(len, buffer[0]);
	CheckVoiceIrq();
}

// Generate logarithmic to linear volume conversion tables
static void PopulateVolScalars(void)
{
	double out = 1.0;
	for (uint16_t i = GUS_VOLUME_POSITIONS - 1; i > 0; --i) {
		vol_scalars.at(i) = static_cast<float>(out);
		out /= GUS_VOLUME_SCALE_DIV;
	}
	vol_scalars[0] = 0.0f;
}

/*
Constant-Power Panning
-------------------------
The GUS SDK describes having 16 panning positions (0 through 15)
with 0 representing all full left rotation through to center or
mid-point at 7, to full-right rotation at 15.  The SDK also
describes that output power is held constant through this range.

	#!/usr/bin/env python3
	import math
	print(f'Left-scalar  Pot Norm.   Right-scalar | Power')
	print(f'-----------  --- -----   ------------ | -----')
	for pot in range(16):
		norm = (pot - 7.) / (7.0 if pot < 7 else 8.0)
		direction = math.pi * (norm + 1.0 ) / 4.0
		lscale = math.cos(direction)
		rscale = math.sin(direction)
		power = lscale * lscale + rscale * rscale
		print(f'{lscale:.5f} <~~~ {pot:2} ({norm:6.3f})'\
			f' ~~~> {rscale:.5f} | {power:.3f}')

	Left-scalar  Pot Norm.   Right-scalar | Power
	-----------  --- -----   ------------ | -----
	1.00000 <~~~  0 (-1.000) ~~~> 0.00000 | 1.000
	0.99371 <~~~  1 (-0.857) ~~~> 0.11196 | 1.000
	0.97493 <~~~  2 (-0.714) ~~~> 0.22252 | 1.000
	0.94388 <~~~  3 (-0.571) ~~~> 0.33028 | 1.000
	0.90097 <~~~  4 (-0.429) ~~~> 0.43388 | 1.000
	0.84672 <~~~  5 (-0.286) ~~~> 0.53203 | 1.000
	0.78183 <~~~  6 (-0.143) ~~~> 0.62349 | 1.000
	0.70711 <~~~  7 ( 0.000) ~~~> 0.70711 | 1.000
	0.63439 <~~~  8 ( 0.125) ~~~> 0.77301 | 1.000
	0.55557 <~~~  9 ( 0.250) ~~~> 0.83147 | 1.000
	0.47140 <~~~ 10 ( 0.375) ~~~> 0.88192 | 1.000
	0.38268 <~~~ 11 ( 0.500) ~~~> 0.92388 | 1.000
	0.29028 <~~~ 12 ( 0.625) ~~~> 0.95694 | 1.000
	0.19509 <~~~ 13 ( 0.750) ~~~> 0.98079 | 1.000
	0.09802 <~~~ 14 ( 0.875) ~~~> 0.99518 | 1.000
	0.00000 <~~~ 15 ( 1.000) ~~~> 1.00000 | 1.000
*/
static void PopulatePanScalars()
{
	for (uint8_t pos = 0u; pos < GUS_PAN_POSITIONS; ++pos) {
		// Normalize absolute range [0, 15] to [-1.0, 1.0]
		const double norm = (pos - 7.0f) / (pos < 7u ? 7 : 8);
		// Convert to an angle between 0 and 90-degree, in radians
		const double angle = (norm + 1) * M_PI / 4;
		pan_scalars.at(pos).left = static_cast<float>(cos(angle));
		pan_scalars.at(pos).right = static_cast<float>(sin(angle));
		// DEBUG_LOG_MSG("GUS: pan_scalar[%u] = %f | %f", pos, pan_scalars.at(pos).left,
		//               pan_scalars.at(pos).right);
	}
}

class GUS:public Module_base{
private:
	IO_ReadHandleObject ReadHandler[8];
	IO_WriteHandleObject WriteHandler[9];
	AutoexecObject autoexecline[2];
	MixerObject MixerChan;
public:
	GUS(Section* configuration):Module_base(configuration){
		if(!IS_EGAVGA_ARCH) return;
		Section_prop * section=static_cast<Section_prop *>(configuration);
		if(!section->Get_bool("gus")) return;

		memset(&myGUS, 0, sizeof(myGUS));

		myGUS.rate=section->Get_int("gusrate");
	
		myGUS.portbase = section->Get_hex("gusbase") - 0x200;
		int dma_val = section->Get_int("gusdma");
		if ((dma_val<0) || (dma_val>255)) dma_val = 3;	// sensible default
		int irq_val = section->Get_int("gusirq");
		if ((irq_val<0) || (irq_val>255)) irq_val = 5;	// sensible default
		myGUS.dma1 = static_cast<uint8_t>(dma_val);
		myGUS.dma2 = static_cast<uint8_t>(dma_val);
		myGUS.irq1 = static_cast<uint8_t>(irq_val);
		myGUS.irq2 = static_cast<uint8_t>(irq_val);

		// We'll leave the MIDI interface to the MPU-401 
		// Ditto for the Joystick 
		// GF1 Synthesizer 
		ReadHandler[0].Install(0x302 + GUS_BASE,read_gus,IO_MB);
		WriteHandler[0].Install(0x302 + GUS_BASE,write_gus,IO_MB);
	
		WriteHandler[1].Install(0x303 + GUS_BASE,write_gus,IO_MB);
		ReadHandler[1].Install(0x303 + GUS_BASE,read_gus,IO_MB);
	
		WriteHandler[2].Install(0x304 + GUS_BASE,write_gus,IO_MB|IO_MW);
		ReadHandler[2].Install(0x304 + GUS_BASE,read_gus,IO_MB|IO_MW);
	
		WriteHandler[3].Install(0x305 + GUS_BASE,write_gus,IO_MB);
		ReadHandler[3].Install(0x305 + GUS_BASE,read_gus,IO_MB);
	
		ReadHandler[4].Install(0x206 + GUS_BASE,read_gus,IO_MB);
	
		WriteHandler[4].Install(0x208 + GUS_BASE,write_gus,IO_MB);
		ReadHandler[5].Install(0x208 + GUS_BASE,read_gus,IO_MB);
	
		WriteHandler[5].Install(0x209 + GUS_BASE,write_gus,IO_MB);
	
		WriteHandler[6].Install(0x307 + GUS_BASE,write_gus,IO_MB);
		ReadHandler[6].Install(0x307 + GUS_BASE,read_gus,IO_MB);
	
		// Board Only 
	
		WriteHandler[7].Install(0x200 + GUS_BASE,write_gus,IO_MB);
		ReadHandler[7].Install(0x20A + GUS_BASE,read_gus,IO_MB);
		WriteHandler[8].Install(0x20B + GUS_BASE,write_gus,IO_MB);
	
	//	DmaChannels[myGUS.dma1]->Register_TC_Callback(GUS_DMA_TC_Callback);

		PopulateVolScalars();
		PopulatePanScalars();
	
		for (uint8_t chan_ct=0; chan_ct<32; chan_ct++) {
			guschan[chan_ct] = new GUSChannels(chan_ct);
		}
		// Register the Mixer CallBack 
		gus_chan=MixerChan.Install(GUS_CallBack,GUS_RATE,"GUS");
		myGUS.gRegData=0x1;
		GUSReset();
		myGUS.gRegData=0x0;
		const Bitu portat = 0x200 + GUS_BASE;

		// ULTRASND=Port,DMA1,DMA2,IRQ1,IRQ2
		// [GUS port], [GUS DMA (recording)], [GUS DMA (playback)], [GUS IRQ (playback)], [GUS IRQ (MIDI)]
		ostringstream temp;
		temp << "SET ULTRASND=" << hex << setw(3) << portat << ","
		     << dec << (Bitu)myGUS.dma1 << "," << (Bitu)myGUS.dma2 << ","
		     << (Bitu)myGUS.irq1 << "," << (Bitu)myGUS.irq2 << ends;
		// Create autoexec.bat lines
		autoexecline[0].Install(temp.str());
		autoexecline[1].Install(std::string("SET ULTRADIR=") + section->Get_string("ultradir"));
	}


	~GUS() {
		if(!IS_EGAVGA_ARCH) return;
		Section_prop * section=static_cast<Section_prop *>(m_configuration);
		if(!section->Get_bool("gus")) return;
	
		myGUS.gRegData=0x1;
		GUSReset();
		myGUS.gRegData=0x0;
	
		for(Bitu i=0;i<32;i++) {
			delete guschan[i];
		}

		memset(&myGUS, 0, sizeof(myGUS));
	}
};

static GUS* test;

void GUS_ShutDown(Section* /*sec*/) {
	delete test;	
}

void GUS_Init(Section* sec) {
	test = new GUS(sec);
	sec->AddDestroyFunction(&GUS_ShutDown,true);
}
