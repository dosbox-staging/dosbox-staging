#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <stdint.h>
#include <stddef.h>
#include "vgmcapture.h"
#include "dosbox.h"
#include "pic.h"
#include "timer.h"

#define FEEDBACK_SN76489 0x06
#define FEEDBACK_NCR489 0x22

#define CMD_SN_WRITE 0x50
#define CMD_YM2612_0_WRITE 0x52
#define CMD_YM2612_1_WRITE 0x53
#define CMD_YM3812_WRITE 0x5A
#define CMD_1ST_YM3812_WRITE 0x5A
#define CMD_2ND_YM3812_WRITE 0xAA
#define CMD_YMF262_0_WRITE 0x5E
#define CMD_YMF262_1_WRITE 0x5F
#define CMD_AY8910_WRITE 0xA0
#define CMD_SAA1099_WRITE 0xBD

#define CMD_WAIT_N_SAMPLES 0x61
#define CMD_WAIT_735_SAMPLES 0x62
#define CMD_WAIT_882_SAMPLES 0x63
#define CMD_DATA_BLOCK 0x67
#define CMD_WAIT_SHORT 0x70

#define CMD_SETUP_STREAM_CONTROL 0x90
#define CMD_SET_STREAM_DATA 0x91
#define CMD_SETUP_STREAM_FREQUENCY 0x92
#define CMD_START_STREAM 0x93
#define CMD_STOP_STREAM 0x94

#define CMD_END_OF_SOUND_DATA 0x66

#define CHIPID_YM2612 0x02
#define CHIPFLAG_TWO_CHIPS 0x40000000
#define CHIPFLAG_HARD_PAN 0x80000000
#define REG_SECOND_CHIP 0x80

#define SNFLAG_FREQ0_1024 0x01
#define SNFLAG_OUTPUT_NEGATIVE 0x02
#define SNFLAG_STEREO_OFF 0x04
#define SNFLAG_CLOCK_BY_8 0x08

#define MODE_ONE_SHOT 0
#define MODE_SQUARE_WAVE 3
#define MODE_UNDEFINED 255

#define YM2612_DAC_DATA 0x2A
#define YM2612_DAC_ENABLE 0x2B
#define AY8910_CHANNEL_A_FINE_TUNE 0x00
#define AY8910_CHANNEL_A_COARSE_TUNE 0x01
#define AY8910_CHANNEL_ENABLE 0x07
#define AY8910_CHANNEL_A_AMPLITUDE 0x08
#define SPK_AMPLITUDE 0x0C

/* Helper functions */
void static inline put_lsb16 (void *b, uint16_t x) {
	uint8_t* buffer = (uint8_t*) b;
	buffer[0] = x & 0xff;
	buffer[1] = x >> 8;
}
void static inline put_lsb32 (void *b, uint32_t x) {
	uint8_t* buffer = (uint8_t*) b;
	buffer[0] = x & 0xff;
	buffer[1] = x >>  8;
	buffer[2] = x >> 16;
	buffer[3] = x >> 24;
}

VGMCapture::VGMCapture(FILE *theHandle) {
	handle = theHandle;

	totalSamples = 0;
	samplesPassedFraction = 0;
	lastTickCount = PIC_FullIndex();

	SN_used = false;
	OPL_used = false;
	SAA_used = false;

	DAC_allowed = false;
	DAC_used = false;
	DMA_active = false;
	streamTail = 0;

	SPK_allowed = false;
	SPK_used = false;
	SPK_PITmode = MODE_UNDEFINED;
	SPK_period_current = 0;
	SPK_period_wanted = 0;
	SPK_clockGate = false;
	SPK_outputGate_current = false;
	SPK_outputGate_wanted = false;

	/* Variables for tracking whether any SN write needs to be output or not */
	SN_latch = 0;
	SN_previous = 0;
	for (int i=0; i<8; i++) {
		SN_regs[i][0] = 0xFF;
		SN_regs[i][1] = 0xFF;
	}
}

VGMCapture::~VGMCapture() {
	logTimeDifference();
	buffer.push_back(CMD_END_OF_SOUND_DATA);

	/* If DAC was used, we need an extra header to add a ChpVol entry to make the DAC louder */
	uint32_t extraHeaderSize = 0;
	if (DAC_used) {
		extraHeaderSize = sizeof(struct VGMExtraHeader);
		put_lsb32(&extraHeader.theSize, 0x0C);
		put_lsb32(&extraHeader.rofsChpClock, 0);
		put_lsb32(&extraHeader.rofsChpVol, offsetof(VGMExtraHeader, entryCount) -offsetof(VGMExtraHeader, rofsChpVol));
		extraHeader.entryCount = 1;
		extraHeader.chipID = CHIPID_YM2612;
		extraHeader.flags = 0;
		if (OPL_used)
			extraHeader.volume = 0x250 | 0x8000; // Relative volume, *2.5 for OPL+DAC
		else
			extraHeader.volume = 0x200 | 0x8000; // Relative volume, *2.0 for PSG+DAC
	}
	/* Build the .VGM header */
	size_t headerSize = (extraHeaderSize || SAA_used)? 0x100: (SPK_used? 0x80: (OPL_used? 0x60: 0x40));
	size_t vgmSize = headerSize + extraHeaderSize + buffer.size();
	memset(&header, 0, sizeof(header));
	strcpy(header.id, "Vgm ");
	put_lsb32(&header.version, SAA_used? 0x171: (extraHeaderSize? 0x170: 0x151));
	put_lsb32(&header.samplesInFile, totalSamples);
	put_lsb32(&header.rofsEOF, vgmSize -offsetof(VGMHeader, rofsEOF));
	put_lsb32(&header.rofsData, headerSize +extraHeaderSize -offsetof(VGMHeader, rofsData));
	if (extraHeaderSize) {
		put_lsb32(&header.rofsExtraHeader, headerSize -offsetof(VGMHeader, rofsExtraHeader));
	}
	if (SN_used) {
		put_lsb32(&header.clockSN76489, 3579545);
		put_lsb16(&header.SNfeedback, (machine == MCH_PCJR)? FEEDBACK_SN76489: FEEDBACK_NCR489);
		header.SNshiftRegisterWidth = 16;
		header.SNflags = SNFLAG_FREQ0_1024 | SNFLAG_STEREO_OFF;
	}
	if (OPL_used) {
		switch(oplmode) {
			case OPL_opl3gold:
			case OPL_opl3:
				put_lsb32(&header.clockYMF262, 14318180);
				break;
			case OPL_dualopl2:
				put_lsb32(&header.clockYM3812, 3579545 | CHIPFLAG_TWO_CHIPS | CHIPFLAG_HARD_PAN);
				break;
			default:
				put_lsb32(&header.clockYM3812, 3579545);
				break;
		}
	}
	if (SAA_used) {
		put_lsb32(&header.clockSAA1099, 7159090 | CHIPFLAG_TWO_CHIPS);
	}
	if (DAC_used) {
		put_lsb32(&header.clockYM2612, 7670454);
	}
	if (SPK_used) {
		put_lsb32(&header.clockAY8910, 1789750);
		header.typeAY8910 = 0; // AY8910
		header.flagsAY8910 = 0x01; // Legacy output
	}
	fseek(handle, 0, SEEK_SET);
	fwrite(&header, 1, headerSize, handle);
	if (extraHeaderSize)
		fwrite(&extraHeader, 1, extraHeaderSize, handle);
	fwrite(&buffer[0], 1, buffer.size(), handle);
	fclose(handle);
}

void VGMCapture::logTimeDifference(void) {
	uint16_t interval;

	/* Determine the time that has passed since the last write. Make sure that rounding errors do not accumulate over time. */
	float ticksPassed = PIC_FullIndex() - lastTickCount;
	if (ticksPassed <0) ticksPassed = -ticksPassed;
	float samplesPassedFloat = ticksPassed * 44100 / 1000 +samplesPassedFraction;
	uint32_t samplesPassed = samplesPassedFloat;
	samplesPassedFraction = samplesPassedFloat -samplesPassed;

	totalSamples += samplesPassed;
	while (samplesPassed) {
		uint16_t interval = (samplesPassed > 65535)? 65535: samplesPassed;
		if (interval <= 16) { /* Intervals from 1-16 can be expressed in one byte as one 7x command */
			buffer.push_back(CMD_WAIT_SHORT + interval -1);
		} else
		if (interval <= 32) { /* Intervals from 17-32 can be expressed in two bytes as two 7x commands (16 plus x) */
			buffer.push_back(CMD_WAIT_SHORT + 16 -1);
			buffer.push_back(CMD_WAIT_SHORT + interval -16 -1);
		} else
		switch(interval) {
			case 735:  buffer.push_back(CMD_WAIT_735_SAMPLES); break;
			case 882:  buffer.push_back(CMD_WAIT_882_SAMPLES); break;
			case 1470: buffer.push_back(CMD_WAIT_735_SAMPLES); buffer.push_back(CMD_WAIT_735_SAMPLES); break;
			case 1617: buffer.push_back(CMD_WAIT_735_SAMPLES); buffer.push_back(CMD_WAIT_882_SAMPLES); break;
			case 1764: buffer.push_back(CMD_WAIT_882_SAMPLES); buffer.push_back(CMD_WAIT_882_SAMPLES); break;
			default:   buffer.push_back(CMD_WAIT_N_SAMPLES);
				   buffer.push_back(interval & 0xFF);
				   buffer.push_back(interval >> 8);
		}
		samplesPassed -= interval;
	}
	lastTickCount = PIC_FullIndex();
}

void VGMCapture::ioWrite_SN(uint8_t value, int (&cache)[8]) {
	if (!SN_used) {
		SN_used = true;
		logTimeDifference();
		/* Initialize the chip to cached values */
		ioWrite_SN(0x80 |(cache[0]&0x0F), cache);
		ioWrite_SN(cache[0] >> 4, cache);
		ioWrite_SN(0x90 | cache[1], cache);
		
		ioWrite_SN(0xA0 |(cache[2]&0x0F), cache);
		ioWrite_SN(cache[2] >> 4, cache);
		ioWrite_SN(0xB0 | cache[3], cache);
		
		ioWrite_SN(0xC0 |(cache[4]&0x0F), cache);
		ioWrite_SN(cache[4] >> 4, cache);
		ioWrite_SN(0xD0 | cache[5], cache);
		
		ioWrite_SN(0xE0 | cache[6], cache);
		ioWrite_SN(0xF0 | cache[7], cache);
	}
	/* Determine if the write changed anything. Only output byte if it did, otherwise Zak McKracken (and others) will sound horribly wrong. */
	bool highLow = value >> 7;
	if (highLow) SN_latch = value;
	uint8_t regNum = (SN_latch >> 4) & 7;
	uint8_t *reg = &SN_regs[regNum][highLow];
	uint8_t newData = highLow? value&0x0F: value&0x7F;
	if (*reg != newData) {
		logTimeDifference();
		if ((value & 0x80)==0 && SN_latch!= SN_previous) {
			/* If a frequency low byte has changed but the high byte has not, then the high byte must be output as well! */
			buffer.push_back(CMD_SN_WRITE);
			buffer.push_back(SN_latch);
		}
		*reg = newData;
		buffer.push_back(CMD_SN_WRITE);
		buffer.push_back(value);
		SN_previous = value;
	}
}

/* Order in which OPL2/OPL3 registers must be initialized from the register cache when capturing starts. 0xFFFF ends the list. */
static const uint16_t initOrderOPL2[] = {
	0x001, 0x008,
	0x020, 0x040, 0x060, 0x080, 0x0E0, 0x023, 0x043, 0x063, 0x083, 0x0E3, 0x0C0, 0x0A0, 0x0B0,
	0x021, 0x041, 0x061, 0x081, 0x0E1, 0x024, 0x044, 0x064, 0x084, 0x0E4, 0x0C1, 0x0A1, 0x0B1,
	0x022, 0x042, 0x062, 0x082, 0x0E2, 0x025, 0x045, 0x065, 0x085, 0x0E5, 0x0C2, 0x0A2, 0x0B2,
	0x028, 0x048, 0x068, 0x088, 0x0E8, 0x02B, 0x04B, 0x06B, 0x08B, 0x0EB, 0x0C3, 0x0A3, 0x0B3,
	0x029, 0x049, 0x069, 0x089, 0x0E9, 0x02C, 0x04C, 0x06C, 0x08C, 0x0EC, 0x0C4, 0x0A4, 0x0B4,
	0x02A, 0x04A, 0x06A, 0x08A, 0x0EA, 0x02D, 0x04D, 0x06D, 0x08D, 0x0ED, 0x0C5, 0x0A5, 0x0B5,
	0x030, 0x050, 0x070, 0x090, 0x0F0, 0x033, 0x053, 0x073, 0x093, 0x0F3, 0x0C6, 0x0A6, 0x0B6,
	0x031, 0x051, 0x071, 0x091, 0x0F1, 0x034, 0x054, 0x074, 0x094, 0x0F4, 0x0C7, 0x0A7, 0x0B7,
	0x032, 0x052, 0x072, 0x092, 0x0F2, 0x035, 0x055, 0x075, 0x095, 0x0F5, 0x0C8, 0x0A8, 0x0B8,
	0x0BD, 0xFFFF
};
static const uint16_t initOrderOPL3[] = {
	0x105, 0x104, 0x001, 0x008,
	0x020, 0x040, 0x060, 0x080, 0x0E0, 0x023, 0x043, 0x063, 0x083, 0x0E3, 0x0C0, 0x0A0, 0x0B0,
	0x021, 0x041, 0x061, 0x081, 0x0E1, 0x024, 0x044, 0x064, 0x084, 0x0E4, 0x0C1, 0x0A1, 0x0B1,
	0x022, 0x042, 0x062, 0x082, 0x0E2, 0x025, 0x045, 0x065, 0x085, 0x0E5, 0x0C2, 0x0A2, 0x0B2,
	0x028, 0x048, 0x068, 0x088, 0x0E8, 0x02B, 0x04B, 0x06B, 0x08B, 0x0EB, 0x0C3, 0x0A3, 0x0B3,
	0x029, 0x049, 0x069, 0x089, 0x0E9, 0x02C, 0x04C, 0x06C, 0x08C, 0x0EC, 0x0C4, 0x0A4, 0x0B4,
	0x02A, 0x04A, 0x06A, 0x08A, 0x0EA, 0x02D, 0x04D, 0x06D, 0x08D, 0x0ED, 0x0C5, 0x0A5, 0x0B5,
	0x030, 0x050, 0x070, 0x090, 0x0F0, 0x033, 0x053, 0x073, 0x083, 0x0F3, 0x0C6, 0x0A6, 0x0B6,
	0x031, 0x051, 0x071, 0x091, 0x0F1, 0x034, 0x054, 0x074, 0x084, 0x0F4, 0x0C7, 0x0A7, 0x0B7,
	0x032, 0x052, 0x072, 0x092, 0x0F2, 0x035, 0x055, 0x075, 0x085, 0x0F5, 0x0C8, 0x0A8, 0x0B8,
	0x120, 0x140, 0x160, 0x180, 0x1E0, 0x123, 0x143, 0x163, 0x183, 0x1E3, 0x1C0, 0x1A0, 0x1B0,
	0x121, 0x141, 0x161, 0x181, 0x1E1, 0x124, 0x144, 0x164, 0x184, 0x1E4, 0x1C1, 0x1A1, 0x1B1,
	0x122, 0x142, 0x162, 0x182, 0x1E2, 0x125, 0x145, 0x165, 0x185, 0x1E5, 0x1C2, 0x1A2, 0x1B2,
	0x128, 0x148, 0x168, 0x188, 0x1E8, 0x12B, 0x14B, 0x16B, 0x18B, 0x1EB, 0x1C3, 0x1A3, 0x1B3,
	0x129, 0x149, 0x169, 0x189, 0x1E9, 0x12C, 0x14C, 0x16C, 0x18C, 0x1EC, 0x1C4, 0x1A4, 0x1B4,
	0x12A, 0x14A, 0x16A, 0x18A, 0x1EA, 0x12D, 0x14D, 0x16D, 0x18D, 0x1ED, 0x1C5, 0x1A5, 0x1B5,
	0x130, 0x150, 0x170, 0x190, 0x1F0, 0x133, 0x153, 0x173, 0x193, 0x1F3, 0x1C6, 0x1A6, 0x1B6,
	0x131, 0x151, 0x171, 0x191, 0x1F1, 0x134, 0x154, 0x174, 0x194, 0x1F4, 0x1C7, 0x1A7, 0x1B7,
	0x132, 0x152, 0x172, 0x192, 0x1F2, 0x135, 0x155, 0x175, 0x195, 0x1F5, 0x1C8, 0x1A8, 0x1B8,
	0x0BD, 0xFFFF
};
void VGMCapture::ioWrite_OPL(bool chip, uint8_t index, uint8_t value, uint8_t (&cache)[512]) {
	logTimeDifference();
	if (!OPL_used) {
		OPL_used = true;
		/* Initialize the chip to cached values */
		int i=0, initReg;
		if (oplmode==OPL_opl3 || oplmode==OPL_opl3gold) {
			while ((initReg = initOrderOPL3[i++]) != 0xFFFF) {
				ioWrite_OPL(initReg >>8, initReg & 0xFF, cache[initReg], cache);
			}
		} else {
			while ((initReg = initOrderOPL2[i++]) != 0xFFFF) {
				ioWrite_OPL(0, initReg, cache[initReg], cache);
				if (oplmode==OPL_dualopl2) ioWrite_OPL(1, initReg, cache[initReg +0x100], cache);
			}
		}
	}
	switch(oplmode) {
		case OPL_opl3gold:
		case OPL_opl3:
			buffer.push_back(CMD_YMF262_0_WRITE + chip);
			buffer.push_back(index);
			buffer.push_back(value);
			break;
		case OPL_dualopl2:
			buffer.push_back(chip? CMD_2ND_YM3812_WRITE: CMD_1ST_YM3812_WRITE);
			buffer.push_back(index);
			buffer.push_back(value);
			break;
		default:
			buffer.push_back(CMD_YM3812_WRITE);
			buffer.push_back(index);
			buffer.push_back(value);
			break;
	}
}

void VGMCapture::ioWrite_SAA(bool chip, uint8_t index, uint8_t value, uint8_t (&cache)[2][32]) {
	logTimeDifference();
	if (!SAA_used) {
		SAA_used = true;
		/* Initialize the chip to cached values */
		for (int initChip=0; initChip<2; initChip++) {
			for (int initReg=0; initReg<32; initReg++) {
				ioWrite_SAA(initChip, initReg, cache[initChip][initReg], cache);
			}
		}
	}
	buffer.push_back(CMD_SAA1099_WRITE);
	buffer.push_back(chip? index | REG_SECOND_CHIP: index);
	buffer.push_back(value);
}


void VGMCapture::ioWrite_DAC(uint8_t value) {
	if (!DAC_allowed) return;
	logTimeDifference();
	if (!DAC_used) {
		DAC_used = true;
		buffer.push_back(CMD_YM2612_0_WRITE);
		buffer.push_back(YM2612_DAC_ENABLE);
		buffer.push_back(0x80);
	}
	buffer.push_back(CMD_YM2612_0_WRITE);
	buffer.push_back(YM2612_DAC_DATA);
	buffer.push_back(value); /* Unsigned 8-bit PCM data */
}

void VGMCapture::DAC_startDMA(uint32_t rate, uint32_t length, uint8_t* data) {
	if (!DAC_allowed) return;
	if (DMA_active) DAC_stopDMA();	
	logTimeDifference();

	uint32_t sampleStartInStream;
	/* Check if that particular sample has already been played before.
	   If so, no need to put write it out as another data block again. */
	bool found = false;
	for (auto& prev: previousPCMs) {
		if (prev.data.size() >= length && memcmp(&prev.data[0], data, length)==0) {
			found = true;
			sampleStartInStream = prev.start;
			break;
		}
	}
	if (!found) {
		/* That sample has not been played before. Output to .VGM and keep in "previousPCMs" buffer. */
		struct previousPCM prev;
		prev.start = streamTail;	
		sampleStartInStream = streamTail;
		streamTail += length;

		buffer.push_back(CMD_DATA_BLOCK);
		buffer.push_back(CMD_END_OF_SOUND_DATA); // Dummy
		buffer.push_back(0x00);	// YM2612 data
		buffer.push_back( length      & 0xFF);
		buffer.push_back((length>> 8) & 0xFF);
		buffer.push_back((length>>16) & 0xFF);
		buffer.push_back((length>>24) & 0xFF);

		for (uint32_t i=0; i<length; i++) {
			buffer.push_back(data[i]);
			prev.data.push_back(data[i]);
		}
		previousPCMs.push_back(prev);
	}

	if (!DAC_used) {
		DAC_used = true;
		buffer.push_back(CMD_YM2612_0_WRITE);
		buffer.push_back(YM2612_DAC_ENABLE);
		buffer.push_back(0x80);
	}
	buffer.push_back(CMD_SETUP_STREAM_CONTROL);
	buffer.push_back(0x00); // Stream ID
	buffer.push_back(CHIPID_YM2612);
	buffer.push_back(0x00);	// YM2612 chip 0
	buffer.push_back(YM2612_DAC_DATA); // YM2612: DAC write
	
	buffer.push_back(CMD_SET_STREAM_DATA);
	buffer.push_back(0x00); // Stream ID
	buffer.push_back(0x00); // Data Bank ID
	buffer.push_back(0x01); // Step size 1
	buffer.push_back(0x00); // Step base 0
	
	buffer.push_back(CMD_SETUP_STREAM_FREQUENCY);
	buffer.push_back(0x00); // Stream ID
	buffer.push_back( rate      & 0xFF);
	buffer.push_back((rate>> 8) & 0xFF);
	buffer.push_back((rate>>16) & 0xFF);
	buffer.push_back((rate>>24) & 0xFF);
	
	buffer.push_back(CMD_START_STREAM);
	buffer.push_back(0x00); // Stream ID
	buffer.push_back( sampleStartInStream      & 0xFF);
	buffer.push_back((sampleStartInStream>> 8) & 0xFF);
	buffer.push_back((sampleStartInStream>>16) & 0xFF);
	buffer.push_back((sampleStartInStream>>24) & 0xFF);
	buffer.push_back(0x01); // "length" shall denote the number of commands (i.e. sample points)
	buffer.push_back( length      & 0xFF);
	buffer.push_back((length>> 8) & 0xFF);
	buffer.push_back((length>>16) & 0xFF);
	buffer.push_back((length>>24) & 0xFF);
	DMA_active = true;
}

void VGMCapture::DAC_stopDMA(void) {
	if (!DAC_allowed) return;
	if (DMA_active) {
		DMA_active = false;
		logTimeDifference();
		buffer.push_back(CMD_STOP_STREAM);
		buffer.push_back(0x00); // Stream ID
	}
};

void VGMCapture::SPK_enable(void) {
	if (!SPK_used) {
		SPK_used = true;
		buffer.push_back(CMD_AY8910_WRITE);
		buffer.push_back(AY8910_CHANNEL_ENABLE); // Channel enable
		buffer.push_back(~1); // Enable Tone Channel A
	}
}

extern Bitu getTimerRate(void);
void VGMCapture::SPK_update(void) {
	#define PWMactive (SPK_clockGate && SPK_outputGate_wanted && SPK_PITmode==MODE_ONE_SHOT)
	/* Update the output gate, unless PWM mode is active (PIT mode set to One-Shot and the clock gate is active */
	if (SPK_outputGate_current != SPK_outputGate_wanted && !PWMactive ) {
		SPK_outputGate_current = SPK_outputGate_wanted;
		logTimeDifference();
		SPK_enable();
		buffer.push_back(CMD_AY8910_WRITE);
		buffer.push_back(AY8910_CHANNEL_A_AMPLITUDE); // Channel A Amplitude
		buffer.push_back(SPK_outputGate_current? SPK_AMPLITUDE: 0x00);		
	}
	/* We want a zero period in the AY chip so as not to interfere with manual clicking of the speaker, or with PWM sound */
	uint32_t PWMperiod;
	if (!SPK_clockGate || PWMactive) {
		PWMperiod = SPK_period_wanted;
		SPK_period_wanted = 0;
	}
	/* Update the the period if it changed */
	if (SPK_period_current != SPK_period_wanted) {
		SPK_period_current = SPK_period_wanted;
		logTimeDifference();
		SPK_enable();
		uint32_t AYperiod = SPK_period_current *3 >>5;	
		buffer.push_back(CMD_AY8910_WRITE);
		buffer.push_back(AY8910_CHANNEL_A_FINE_TUNE);
		buffer.push_back(AYperiod & 0xFF);
		buffer.push_back(CMD_AY8910_WRITE);
		buffer.push_back(AY8910_CHANNEL_A_COARSE_TUNE);
		buffer.push_back(AYperiod >> 8);		
	}
	if (PWMactive) {
		uint32_t ch0count = getTimerRate();
		if (ch0count >= (PIT_TICK_RATE/3000)) {
			// Low frequency Pulse-Width Modulation: Square wave with variable duty cycle.
			// PIT channel 0's counter is the base frequency; channel 2's counter is the duty cycle, or (at very low duty cycles) the volume.
			// TODO: Quality is rather poor.
			logTimeDifference();
			SPK_enable();
			// Positive period
			buffer.push_back(CMD_AY8910_WRITE);
			buffer.push_back(AY8910_CHANNEL_A_AMPLITUDE); // Channel A Amplitude
			float PWMduration = ( 1000.0f / PIT_TICK_RATE) * (float) PWMperiod;
			float PWMsamples  = (44100.0f / PIT_TICK_RATE) * (float) PWMperiod;
			samplesPassedFraction += PWMsamples;
			buffer.push_back(SPK_AMPLITUDE);
			// Negative period
			logTimeDifference();		
			buffer.push_back(CMD_AY8910_WRITE);
			buffer.push_back(AY8910_CHANNEL_A_AMPLITUDE); // Channel A Amplitude
			buffer.push_back(0x00);
			lastTickCount += PWMduration;
			SPK_PITmode = MODE_UNDEFINED;
		} else {
			// High frequency Pulse-Width Modulation: Used to play back PCM samples.
			// PIT channel 0's counter is the sampling period; channel 2's counter is the amplitude.
			ioWrite_DAC(255 * PWMperiod / ch0count);
		}
	}
}

void VGMCapture::SPK_setPeriod(uint32_t ch2count, uint8_t ch2mode) {
	if (!SPK_allowed) return;
	
	SPK_PITmode = ch2mode;
	SPK_period_wanted = ch2count;
	SPK_update();
}

void VGMCapture::SPK_setType(bool clockGate, bool outputGate) {
	if (!SPK_allowed) return;

	SPK_clockGate = clockGate;
	SPK_outputGate_wanted = outputGate;
	SPK_update();
}
