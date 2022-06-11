/*
 *  Copyright (C) 2002-2021  The DOSBox Team
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

#include "adlib.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>

#include "cpu.h"
#include "setup.h"
#include "support.h"
#include "mapper.h"
#include "mem.h"
#include "dbopl.h"
#include "../libs/nuked/opl3.h"
#include "../libs/TDA8425_emu/TDA8425_emu.h"
#include "../libs/YM7128B_emu/YM7128B_emu.h"

#include "mame/emu.h"
#include "mame/fmopl.h"
#include "mame/ymf262.h"


#define OPL2_INTERNAL_FREQ    3600000   // The OPL2 operates at 3.6MHz
#define OPL3_INTERNAL_FREQ    14400000  // The OPL3 operates at 14.4MHz

static Adlib::Module *module = nullptr;

class AdlibGoldSurroundProcessor {
public:
	AdlibGoldSurroundProcessor(const uint16_t sample_rate);
	~AdlibGoldSurroundProcessor();

	void ControlWrite(const uint8_t val) noexcept;
	AudioFrame Process(const AudioFrame &frame) noexcept;

private:
	YM7128B_ChipIdeal *chip = nullptr;

	struct {
		uint8_t sci  = 0;
		uint8_t a0   = 0;
		uint8_t addr = 0;
		uint8_t data = 0;
	} control_state = {};
};

AdlibGoldSurroundProcessor::AdlibGoldSurroundProcessor(const uint16_t sample_rate)
{
	chip = (YM7128B_ChipIdeal *)malloc(sizeof(YM7128B_ChipIdeal));

	YM7128B_ChipIdeal_Ctor(chip);
	YM7128B_ChipIdeal_Setup(chip, sample_rate);
	YM7128B_ChipIdeal_Reset(chip);
	YM7128B_ChipIdeal_Start(chip);
}

AdlibGoldSurroundProcessor::~AdlibGoldSurroundProcessor()
{
	if (chip) {
		YM7128B_ChipIdeal_Stop(chip);
		YM7128B_ChipIdeal_Dtor(chip);
		free(chip);
		chip = nullptr;
	}
}

void AdlibGoldSurroundProcessor::ControlWrite(const uint8_t val) noexcept
{
	// Serial data
	const auto din = val & 1;
	// Bit clock
	const auto sci = val & 2;
	// Word clock
	const auto a0 = val & 4;

	// Change register data at the falling edge of 'a0' word clock
	if (control_state.a0 && !a0) {
//		DEBUG_LOG_MSG("ADLIBGOLD.SURROUND: Write control register %d, data: %d",
//		              control_state.addr,
//		              control_state.data);
		YM7128B_ChipIdeal_Write(chip, control_state.addr, control_state.data);
	} else {
		// Data is sent in serially through 'din' in MSB->LSB order,
		// synchronised by the 'sci' bit clock. Data should be read on
		// the rising edge of 'sci'.
		if (!control_state.sci && sci) {
			// The 'a0' word clock determines the type of the data.
			if (a0)
				// Data cycle
				control_state.data = (control_state.data << 1) | din;
			else
				// Address cycle
				control_state.addr = (control_state.addr << 1) | din;
		}
	}

	control_state.sci = sci;
	control_state.a0  = a0;
}

AudioFrame AdlibGoldSurroundProcessor::Process(const AudioFrame &frame) noexcept
{
	YM7128B_ChipIdeal_Process_Data data = {};
	data.inputs[0]                      = frame.left + frame.right;

	YM7128B_ChipIdeal_Process(chip, &data);

	return {data.outputs[0], data.outputs[1]};
}

class AdlibGoldStereoProcessor {
public:
	AdlibGoldStereoProcessor(const uint16_t sample_rate);
	~AdlibGoldStereoProcessor();

	void Reset() noexcept;
	void ControlWrite(const TDA8425_Reg reg, const TDA8425_Register data) noexcept;
	AudioFrame Process(const AudioFrame &frame) noexcept;

private:
	TDA8425_Chip *chip = nullptr;
};

AdlibGoldStereoProcessor::AdlibGoldStereoProcessor(const uint16_t sample_rate)
{
	chip = (TDA8425_Chip *)malloc(sizeof(TDA8425_Chip));

	TDA8425_Chip_Ctor(chip);
	TDA8425_Chip_Setup(chip,
	                   sample_rate,
	                   TDA8425_Pseudo_C1_Table[TDA8425_Pseudo_Preset_1],
	                   TDA8425_Pseudo_C2_Table[TDA8425_Pseudo_Preset_1],
	                   TDA8425_Tfilter_Mode_Disabled);
	TDA8425_Chip_Reset(chip);
	TDA8425_Chip_Start(chip);

	Reset();
}

AdlibGoldStereoProcessor::~AdlibGoldStereoProcessor()
{
	if (chip) {
		TDA8425_Chip_Stop(chip);
		TDA8425_Chip_Dtor(chip);
		free(chip);
		chip = nullptr;
	}
}

void AdlibGoldStereoProcessor::Reset() noexcept
{
	constexpr auto volume_0db    = 60;
	constexpr auto bass_0db      = 6;
	constexpr auto treble_0db    = 6;
	constexpr auto stereo_output = TDA8425_Selector_Stereo_1;
	constexpr auto linear_stereo = TDA8425_Mode_LinearStereo
	                            << TDA8425_Reg_SF_STL;

	ControlWrite(TDA8425_Reg_VL, volume_0db);
	ControlWrite(TDA8425_Reg_VR, volume_0db);
	ControlWrite(TDA8425_Reg_BA, bass_0db);
	ControlWrite(TDA8425_Reg_TR, treble_0db);
	ControlWrite(TDA8425_Reg_SF, stereo_output & linear_stereo);
}

void AdlibGoldStereoProcessor::ControlWrite(const TDA8425_Reg addr,
                                            const TDA8425_Register data) noexcept
{
	TDA8425_Chip_Write(chip, addr, data);
}

AudioFrame AdlibGoldStereoProcessor::Process(const AudioFrame &frame) noexcept
{
	TDA8425_Chip_Process_Data data = {};
	data.inputs[0][0]              = frame.left;
	data.inputs[1][0]              = frame.left;
	data.inputs[0][1]              = frame.right;
	data.inputs[1][1]              = frame.right;

	TDA8425_Chip_Process(chip, &data);

	return {data.outputs[0], data.outputs[1]};
}

class AdlibGold {
public:
	AdlibGold(const uint16_t sample_rate);

	void SurroundControlWrite(const uint8_t val) noexcept;
	void StereoControlWrite(const TDA8425_Reg reg,
	                        const TDA8425_Register data) noexcept;

	void Process(const int16_t *in, const uint32_t frames, float *out) noexcept;

private:
	std::unique_ptr<AdlibGoldSurroundProcessor> surround_processor;
	std::unique_ptr<AdlibGoldStereoProcessor> stereo_processor;
};

AdlibGold::AdlibGold(const uint16_t sample_rate)
{
	surround_processor = std::make_unique<AdlibGoldSurroundProcessor>(sample_rate);
	stereo_processor = std::make_unique<AdlibGoldStereoProcessor>(sample_rate);
}

void AdlibGold::StereoControlWrite(const TDA8425_Reg reg,
                                   const TDA8425_Register data) noexcept
{
	stereo_processor->ControlWrite(reg, data);
}

void AdlibGold::SurroundControlWrite(const uint8_t val) noexcept
{
	surround_processor->ControlWrite(val);
}

void AdlibGold::Process(const int16_t *in, const uint32_t frames, float *out) noexcept
{
	auto frames_left = frames;
	while (frames_left--) {
		AudioFrame frame = {static_cast<float>(in[0]),
		                    static_cast<float>(in[1])};

		const auto wet = surround_processor->Process(frame);
		// Additional wet signal level boost to make the emulated sound
		// more closely resemble real hardware recordings.
		constexpr auto wet_boost = 1.6;
		frame.left += wet.left * wet_boost;
		frame.right += wet.right * wet_boost;

		frame = stereo_processor->Process(frame);

		out[0] = frame.left;
		out[1] = frame.right;
		in += 2;
		out += 2;
	}
}

static AdlibGold *adlib_gold = nullptr;

constexpr auto render_frames = 1024;

namespace OPL2 {
	#include "opl.cpp"

struct Handler : public Adlib::Handler {
	virtual void WriteReg(uint32_t reg, uint8_t val) { adlib_write(reg, val); }
	virtual uint32_t WriteAddr(io_port_t, uint8_t val) { return val; }

	virtual void Generate(mixer_channel_t &chan, const uint16_t frames)
	{
		int16_t buf[render_frames];
		int remaining = frames;
		while (remaining > 0) {
			const auto todo = std::min(remaining, render_frames);
			adlib_getsample(buf, todo);
			chan->AddSamples_m16(todo, buf);
			remaining -= todo;
		}
	}
	virtual void Init(uint32_t rate) { adlib_init(rate); }
	~Handler() {}
};

} // namespace OPL2

namespace OPL3 {
	#define OPLTYPE_IS_OPL3
	#include "opl.cpp"

struct Handler : public Adlib::Handler {
	virtual void WriteReg(uint32_t reg, uint8_t val) { adlib_write(reg, val); }
	virtual uint32_t WriteAddr(io_port_t port, uint8_t val)
	{
		adlib_write_index(port, val);
		return opl_index;
	}
	virtual void Generate(mixer_channel_t &chan, const uint16_t frames)
	{
		int16_t buf[render_frames * 2];
		float float_buf[render_frames * 2];
		int remaining = frames;

		while (remaining > 0) {
			const auto todo = std::min(remaining, render_frames);
			adlib_getsample(buf, todo);

			if (adlib_gold) {
				adlib_gold->Process(buf, todo, float_buf);
				chan->AddSamples_sfloat(todo, float_buf);
			} else {
				chan->AddSamples_s16(todo, buf);
			}
			remaining -= todo;
		}
	}
	virtual void Init(uint32_t rate) { adlib_init(rate); }
	~Handler() {}
};

} // namespace OPL3

namespace MAMEOPL2 {

struct Handler : public Adlib::Handler {
	void *chip = nullptr;

	virtual void WriteReg(uint32_t reg, uint8_t val) {
		ym3812_write(chip, 0, reg);
		ym3812_write(chip, 1, val);
	}
	virtual uint32_t WriteAddr(io_port_t, uint8_t val) { return val; }
	virtual void Generate(mixer_channel_t &chan, const uint16_t frames)
	{
		int16_t buf[render_frames * 2];
		int remaining = frames;
		while (remaining > 0) {
			const auto todo = std::min(remaining, render_frames);
			ym3812_update_one(chip, buf, todo);
			chan->AddSamples_m16(todo, buf);
			remaining -= todo;
		}
	}
	virtual void Init(uint32_t rate)
	{
		chip = ym3812_init(0, OPL2_INTERNAL_FREQ, rate);
	}
	~Handler() {
		ym3812_shutdown(chip);
	}
};

}


namespace MAMEOPL3 {

struct Handler : public Adlib::Handler {
	void *chip = nullptr;

	virtual void WriteReg(uint32_t reg, uint8_t val) {
		ymf262_write(chip, 0, reg);
		ymf262_write(chip, 1, val);
	}
	virtual uint32_t WriteAddr(io_port_t, uint8_t val) { return val; }
	virtual void Generate(mixer_channel_t &chan, const uint16_t frames)
	{
		// We generate data for 4 channels, but only the first 2 are
		// connected on a pc
		int16_t buf[4][render_frames];
		float float_buf[render_frames * 2];
		int16_t result[render_frames][2];
		int16_t* buffers[4] = { buf[0], buf[1], buf[2], buf[3] };

		int remaining = frames;
		while (remaining > 0) {
			const auto todo = std::min(remaining, render_frames);
			ymf262_update_one(chip, buffers, todo);
			//Interleave the samples before mixing
			for (int i = 0; i < todo; i++) {
				result[i][0] = buf[0][i];
				result[i][1] = buf[1][i];
			}
			if (adlib_gold) {
				adlib_gold->Process(result[0], todo, float_buf);
				chan->AddSamples_sfloat(todo, float_buf);
			} else {
				chan->AddSamples_s16(todo, result[0]);
			}
			remaining -= todo;
		}
	}
	virtual void Init(uint32_t rate)
	{
		chip = ymf262_init(0, OPL3_INTERNAL_FREQ, rate);
	}
	~Handler() {
		ymf262_shutdown(chip);
	}
};

}

namespace NukedOPL {

struct Handler : public Adlib::Handler {
	opl3_chip chip = {};
	uint8_t newm = 0;

	void WriteReg(uint32_t reg, uint8_t val) override
	{
		OPL3_WriteRegBuffered(&chip, (uint16_t)reg, val);
		if (reg == 0x105)
			newm = reg & 0x01;
	}

	uint32_t WriteAddr(io_port_t port, uint8_t val) override
	{
		uint16_t addr;
		addr = val;
		if ((port & 2) && (addr == 0x05 || newm)) {
			addr |= 0x100;
		}
		return addr;
	}

	void Generate(mixer_channel_t &chan, uint16_t frames) override
	{
		int16_t buf[render_frames * 2];
		float float_buf[render_frames * 2];

		while (frames > 0) {
			uint32_t todo = frames > render_frames ? render_frames : frames;
			OPL3_GenerateStream(&chip, buf, todo);

			if (adlib_gold) {
				adlib_gold->Process(buf, todo, float_buf);
				chan->AddSamples_sfloat(todo, float_buf);
			} else {
				chan->AddSamples_s16(todo, buf);
			}
			frames -= todo;
		}
	}

	void Init(uint32_t rate) override
	{
		newm = 0;
		OPL3_Reset(&chip, rate);
	}
};

} // namespace NukedOPL


/*
	Main Adlib implementation

*/

namespace Adlib {


/* Raw DRO capture stuff */

#ifdef _MSC_VER
#pragma pack (1)
#endif

#define HW_OPL2 0
#define HW_DUALOPL2 1
#define HW_OPL3 2

struct RawHeader {
	uint8_t id[8];				/* 0x00, "DBRAWOPL" */
	uint16_t versionHigh;			/* 0x08, size of the data following the m */
	uint16_t versionLow;			/* 0x0a, size of the data following the m */
	uint32_t commands;			/* 0x0c, uint32_t amount of command/data pairs */
	uint32_t milliseconds;		/* 0x10, uint32_t Total milliseconds of data in this chunk */
	uint8_t hardware;				/* 0x14, uint8_t Hardware Type 0=opl2,1=dual-opl2,2=opl3 */
	uint8_t format;				/* 0x15, uint8_t Format 0=cmd/data interleaved, 1 maybe all cdms, followed by all data */
	uint8_t compression;			/* 0x16, uint8_t Compression Type, 0 = No Compression */
	uint8_t delay256;				/* 0x17, uint8_t Delay 1-256 msec command */
	uint8_t delayShift8;			/* 0x18, uint8_t (delay + 1)*256 */			
	uint8_t conversionTableSize;	/* 0x191, uint8_t Raw Conversion Table size */
} GCC_ATTRIBUTE(packed);
#ifdef _MSC_VER
#pragma pack()
#endif
/*
	The Raw Tables is < 128 and is used to convert raw commands into a full register index 
	When the high bit of a raw command is set it indicates the cmd/data pair is to be sent to the 2nd port
	After the conversion table the raw data follows immediatly till the end of the chunk
*/

//Table to map the opl register to one <127 for dro saving
class Capture {
	uint8_t ToReg[127];       // 127 entries to go from raw data to registers
	uint8_t RawUsed = 0;      // How many entries in the ToPort are used
	uint8_t ToRaw[256];       // 256 entries to go from port index to raw data
	uint8_t delay256 = 0;
	uint8_t delayShift8 = 0;
	RawHeader header;

	FILE*  handle = nullptr;  // File used for writing
	uint32_t startTicks = 0;    // Start used to check total raw length on end
	uint32_t lastTicks = 0;     // Last ticks when last last cmd was added
	uint8_t  buf[1024];         // 16 added for delay commands and what not
	uint32_t bufUsed = 0;

	RegisterCache* cache;

	void MakeEntry( uint8_t reg, uint8_t& raw ) {
		ToReg[ raw ] = reg;
		ToRaw[ reg ] = raw;
		raw++;
	}
	void MakeTables( void ) {
		uint8_t index = 0;
		memset( ToReg, 0xff, sizeof ( ToReg ) );
		memset( ToRaw, 0xff, sizeof ( ToRaw ) );
		//Select the entries that are valid and the index is the mapping to the index entry
		MakeEntry( 0x01, index );					//0x01: Waveform select
		MakeEntry( 0x04, index );					//104: Four-Operator Enable
		MakeEntry( 0x05, index );					//105: OPL3 Mode Enable
		MakeEntry( 0x08, index );					//08: CSW / NOTE-SEL
		MakeEntry( 0xbd, index );					//BD: Tremolo Depth / Vibrato Depth / Percussion Mode / BD/SD/TT/CY/HH On
		//Add the 32 byte range that hold the 18 operators
		for ( int i = 0 ; i < 24; i++ ) {
			if ( (i & 7) < 6 ) {
				MakeEntry(0x20 + i, index );		//20-35: Tremolo / Vibrato / Sustain / KSR / Frequency Multiplication Facto
				MakeEntry(0x40 + i, index );		//40-55: Key Scale Level / Output Level 
				MakeEntry(0x60 + i, index );		//60-75: Attack Rate / Decay Rate 
				MakeEntry(0x80 + i, index );		//80-95: Sustain Level / Release Rate
				MakeEntry(0xe0 + i, index );		//E0-F5: Waveform Select
			}
		}
		//Add the 9 byte range that hold the 9 channels
		for ( int i = 0 ; i < 9; i++ ) {
			MakeEntry(0xa0 + i, index );			//A0-A8: Frequency Number
			MakeEntry(0xb0 + i, index );			//B0-B8: Key On / Block Number / F-Number(hi bits) 
			MakeEntry(0xc0 + i, index );			//C0-C8: FeedBack Modulation Factor / Synthesis Type
		}
		//Store the amount of bytes the table contains
		RawUsed = index;
//		assert( RawUsed <= 127 );
		delay256 = RawUsed;
		delayShift8 = RawUsed+1; 
	}

	void ClearBuf( void ) {
		fwrite( buf, 1, bufUsed, handle );
		header.commands += bufUsed / 2;
		bufUsed = 0;
	}
	void AddBuf( uint8_t raw, uint8_t val ) {
		buf[bufUsed++] = raw;
		buf[bufUsed++] = val;
		if ( bufUsed >= sizeof( buf ) ) {
			ClearBuf();
		}
	}
	void AddWrite( uint32_t regFull, uint8_t val ) {
		uint8_t regMask = regFull & 0xff;
		/*
			Do some special checks if we're doing opl3 or dualopl2 commands
			Although you could pretty much just stick to always doing opl3 on the player side
		*/
		//Enabling opl3 4op modes will make us go into opl3 mode
		if ( header.hardware != HW_OPL3 && regFull == 0x104 && val && (*cache)[0x105] ) {
			header.hardware = HW_OPL3;
		} 
		//Writing a keyon to a 2nd address enables dual opl2 otherwise
		//Maybe also check for rhythm
		if ( header.hardware == HW_OPL2 && regFull >= 0x1b0 && regFull <=0x1b8 && val ) {
			header.hardware = HW_DUALOPL2;
		}
		uint8_t raw = ToRaw[ regMask ];
		if ( raw == 0xff )
			return;
		if ( regFull & 0x100 )
			raw |= 128;
		AddBuf( raw, val );
	}
	void WriteCache(void)
	{
		/* Check the registers to add */
		for (uint16_t i = 0; i < 256; i++) {
			auto val = (*cache)[i];
			// Silence the note on entries
			if (i >= 0xb0 && i <= 0xb8) {
				val &= ~0x20;
			}
			if (i == 0xbd) {
				val &= ~0x1f;
			}

			if (val) {
				AddWrite( i, val );
			}
			val = (*cache)[ 0x100 + i ];

			if (i >= 0xb0 && i <= 0xb8) {
				val &= ~0x20;
			}
			if (val) {
				AddWrite( 0x100 + i, val );
			}
		}
	}
	void InitHeader( void ) {
		memset( &header, 0, sizeof( header ) );
		memcpy( header.id, "DBRAWOPL", 8 );
		header.versionLow = 0;
		header.versionHigh = 2;
		header.delay256 = delay256;
		header.delayShift8 = delayShift8;
		header.conversionTableSize = RawUsed;
	}
	void CloseFile( void ) {
		if ( handle ) {
			ClearBuf();
			/* Endianize the header and write it to beginning of the file */
			header.versionHigh  = host_to_le(header.versionHigh);
			header.versionLow   = host_to_le(header.versionLow);
			header.commands     = host_to_le(header.commands);
			header.milliseconds = host_to_le(header.milliseconds);
			fseek( handle, 0, SEEK_SET );
			fwrite( &header, 1, sizeof( header ), handle );
			fclose( handle );
			handle = 0;
		}
	}
public:
	bool DoWrite( uint32_t regFull, uint8_t val ) {
		uint8_t regMask = regFull & 0xff;
		//Check the raw index for this register if we actually have to save it
		if ( handle ) {
			/*
				Check if we actually care for this to be logged, else just ignore it
			*/
			uint8_t raw = ToRaw[ regMask ];
			if ( raw == 0xff ) {
				return true;
			}
			/* Check if this command will not just replace the same value 
			   in a reg that doesn't do anything with it
			*/
			if ( (*cache)[ regFull ] == val )
				return true;
			/* Check how much time has passed */
			uint32_t passed = PIC_Ticks - lastTicks;
			lastTicks = PIC_Ticks;
			header.milliseconds += passed;

			//if ( passed > 0 ) LOG_MSG( "Delay %d", passed ) ;
			
			// If we passed more than 30 seconds since the last command, we'll restart the the capture
			if ( passed > 30000 ) {
				CloseFile();
				goto skipWrite; 
			}
			while (passed > 0) {
				if (passed < 257) {			//1-256 millisecond delay
					AddBuf( delay256, passed - 1 );
					passed = 0;
				} else {
					const auto shift = (passed >> 8);
					passed -= shift << 8;
					AddBuf( delayShift8, shift - 1 );
				}
			}
			AddWrite( regFull, val );
			return true;
		}
skipWrite:
		//Not yet capturing to a file here
		//Check for commands that would start capturing, if it's not one of them return
		if ( !(
			//note on in any channel 
			( regMask>=0xb0 && regMask<=0xb8 && (val&0x020) ) ||
			//Percussion mode enabled and a note on in any percussion instrument
			( regMask == 0xbd && ( (val&0x3f) > 0x20 ) )
		)) {
			return true;
		}
	  	handle = OpenCaptureFile("Raw Opl",".dro");
		if (!handle)
			return false;
		InitHeader();
		//Prepare space at start of the file for the header
		fwrite( &header, 1, sizeof(header), handle );
		/* write the Raw To Reg table */
		fwrite( &ToReg, 1, RawUsed, handle );
		/* Write the cache of last commands */
		WriteCache( );
		/* Write the command that triggered this */
		AddWrite( regFull, val );
		//Init the timing information for the next commands
		lastTicks = PIC_Ticks;	
		startTicks = PIC_Ticks;
		return true;
	}

	Capture(RegisterCache *_cache)
		: header(),
		  cache(_cache)
	{
		MakeTables();
	}

	virtual ~Capture()
	{
		CloseFile();
	}

	Capture(const Capture&) = delete; // prevent copy
	Capture& operator=(const Capture&) = delete; // prevent assignment
};

/*
Chip
*/

Chip::Chip() : timer0(80), timer1(320) {
}

bool Chip::Write( uint32_t reg, uint8_t val ) {
	//if(reg == 0x02 || reg == 0x03 || reg == 0x04) LOG(LOG_MISC,LOG_ERROR)("write adlib timer %X %X",reg,val);
	switch ( reg ) {
	case 0x02:
		timer0.Update(PIC_FullIndex() );
		timer0.SetCounter(val);
		return true;
	case 0x03:
		timer1.Update(PIC_FullIndex());
		timer1.SetCounter(val);
		return true;
	case 0x04:
		//Reset overflow in both timers
		if ( val & 0x80 ) {
			timer0.Reset();
			timer1.Reset();
		} else {
			const auto time = PIC_FullIndex();
			if (val & 0x1) {
				timer0.Start(time);
			}
			else {
				timer0.Stop();
			}
			if (val & 0x2) {
				timer1.Start(time);
			}
			else {
				timer1.Stop();
			}
			timer0.SetMask((val & 0x40) > 0);
			timer1.SetMask((val & 0x20) > 0);
		}
		return true;
	}
	return false;
}


uint8_t Chip::Read( ) {
	const auto time(PIC_FullIndex());
	uint8_t ret = 0;
	//Overflow won't be set if a channel is masked
	if (timer0.Update(time)) {
		ret |= 0x40;
		ret |= 0x80;
	}
	if (timer1.Update(time)) {
		ret |= 0x20;
		ret |= 0x80;
	}
	return ret;
}

void Module::CacheWrite(uint32_t port, uint8_t val)
{
	// capturing?
	if (capture) {
		capture->DoWrite(port, val);
	}
	//Store it into the cache
	cache[port] = val;
}

void Module::DualWrite(uint8_t index, uint8_t port, uint8_t val)
{
	// Make sure you don't use opl3 features
	// Don't allow write to disable opl3
	if (port == 5) {
		return;
	}
	//Only allow 4 waveforms
	if (port >= 0xE0) {
		val &= 3;
	}
	//Write to the timer?
	if (chip[index].Write(port, val))
		return;
	//Enabling panning
	if (port >= 0xc0 && port <= 0xc8) {
		val &= 0x0f;
		val |= index ? 0xA0 : 0x50;
	}
	const uint32_t full_port = port + (index ? 0x100 : 0);
	handler->WriteReg(full_port, val);
	CacheWrite(full_port, val);
}

void Module::CtrlWrite(uint8_t val)
{
	switch (ctrl.index) {
	case 0x04:
		if (adlib_gold) {
			DEBUG_LOG_MSG("ADLIBGOLD.STEREO: Control write, final output volume left: %d",
			              val & 0x3f);
			adlib_gold->StereoControlWrite(TDA8425_Reg_VL, val);
		}
		break;
	case 0x05:
		if (adlib_gold) {
			DEBUG_LOG_MSG("ADLIBGOLD.STEREO: Control write, final output volume right: %d",
			              val & 0x3f);
			adlib_gold->StereoControlWrite(TDA8425_Reg_VR, val);
		}
		break;
	case 0x06:
		if (adlib_gold) {
			DEBUG_LOG_MSG("ADLIBGOLD.STEREO: Control write, bass: %d",
			              val & 0xf);
			adlib_gold->StereoControlWrite(TDA8425_Reg_BA, val);
		}
		break;
	case 0x07:
		if (adlib_gold) {
			DEBUG_LOG_MSG("ADLIBGOLD.STEREO: Control write, treble: %d",
			              val & 0xf);
			// Additional treble boost to make the emulated sound
			// more closely resemble real hardware recordings.
			adlib_gold->StereoControlWrite(TDA8425_Reg_TR,
			                               val < 0xf ? val + 1 : 0xf);
		}
		break;

	case 0x08:
		if (adlib_gold) {
			DEBUG_LOG_MSG("ADLIBGOLD.STEREO: Control write, input selector: 0x%02x, stereo mode: 0x%02x",
			              val & 6,
			              val & 18);
			adlib_gold->StereoControlWrite(TDA8425_Reg_SF, val);
		}
		break;

	case 0x09: /* Left FM Volume */ ctrl.lvol = val; goto setvol;
	case 0x0a: /* Right FM Volume */
		ctrl.rvol = val;
	setvol:
		if (ctrl.mixer) {
			// Dune cdrom uses 32 volume steps in an apparent
			// mistake, should be 128
			mixerChan->SetVolume((float)(ctrl.lvol & 0x1f) / 31.0f,
			                     (float)(ctrl.rvol & 0x1f) / 31.0f);
		}
		break;

	case 0x18: /* Surround */
		if (adlib_gold)
			adlib_gold->SurroundControlWrite(val);
	}
}

uint8_t Module::CtrlRead(void)
{
	switch (ctrl.index) {
	case 0x00: /* Board Options */
		if (adlib_gold)
			return 0x50; // 16-bit ISA, surround module, no
			             // telephone/CDROM
		else
			return 0x70; // 16-bit ISA, no telephone/surround/CD-ROM

	case 0x09: /* Left FM Volume */ return ctrl.lvol;
	case 0x0a: /* Right FM Volume */ return ctrl.rvol;
	case 0x15:                 /* Audio Relocation */
		return 0x388 >> 3; // Cryo installer detection
	}
	return 0xff;
}

void Module::PortWrite(io_port_t port, io_val_t value, io_width_t)
{
	const auto val = check_cast<uint8_t>(value);
	// Keep track of last write time
	lastUsed = PIC_Ticks;
	//Maybe only enable with a keyon?
	if (!mixerChan->is_enabled) {
		mixerChan->Enable(true);
	}
	if ( port&1 ) {
		switch ( mode ) {
		case MODE_OPL3GOLD:
			if ( port == 0x38b ) {
				if ( ctrl.active ) {
					CtrlWrite( val );
					break;
				}
			}
			[[fallthrough]];
		case MODE_OPL2:
		case MODE_OPL3:
			if ( !chip[0].Write( reg.normal, val ) ) {
				handler->WriteReg( reg.normal, val );
				CacheWrite( reg.normal, val );
			}
			break;
		case MODE_DUALOPL2:
			//Not a 0x??8 port, then write to a specific port
			if ( !(port & 0x8) ) {
				uint8_t index = ( port & 2 ) >> 1;
				DualWrite( index, reg.dual[index], val );
			} else {
				//Write to both ports
				DualWrite( 0, reg.dual[0], val );
				DualWrite( 1, reg.dual[1], val );
			}
			break;
		}
	} else {
		//Ask the handler to write the address
		//Make sure to clip them in the right range
		switch ( mode ) {
		case MODE_OPL2:
			reg.normal = handler->WriteAddr( port, val ) & 0xff;
			break;
		case MODE_OPL3GOLD:
			if ( port == 0x38a ) {
				if ( val == 0xff ) {
					ctrl.active = true;
					break;
				} else if ( val == 0xfe ) {
					ctrl.active = false;
					break;
				} else if ( ctrl.active ) {
					ctrl.index = val & 0xff;
					break;
				}
			}
			[[fallthrough]];
		case MODE_OPL3:
			reg.normal = handler->WriteAddr( port, val ) & 0x1ff;
			break;
		case MODE_DUALOPL2:
			//Not a 0x?88 port, when write to a specific side
			if ( !(port & 0x8) ) {
				uint8_t index = ( port & 2 ) >> 1;
				reg.dual[index] = val & 0xff;
			} else {
				reg.dual[0] = val & 0xff;
				reg.dual[1] = val & 0xff;
			}
			break;
		}
	}
}

uint8_t Module::PortRead(io_port_t port, io_width_t)
{
	// roughly half a micro (as we already do 1 micro on each port read and
	// some tests revealed it taking 1.5 micros to read an adlib port)
	auto delaycyc = (CPU_CycleMax / 2048);
	if (GCC_UNLIKELY(delaycyc > CPU_Cycles))
		delaycyc = CPU_Cycles;
	CPU_Cycles -= delaycyc;
	CPU_IODelayRemoved += delaycyc;

	switch ( mode ) {
	case MODE_OPL2:
		//We allocated 4 ports, so just return -1 for the higher ones
		if ( !(port & 3 ) ) {
			//Make sure the low bits are 6 on opl2
			return chip[0].Read() | 0x6;
		} else {
			return 0xff;
		}
	case MODE_OPL3GOLD:
		if ( ctrl.active ) {
			if ( port == 0x38a ) {
				return 0; //Control status, not busy
			} else if ( port == 0x38b ) {
				return CtrlRead();
			}
		}
		[[fallthrough]];
	case MODE_OPL3:
		//We allocated 4 ports, so just return -1 for the higher ones
		if ( !(port & 3 ) ) {
			return chip[0].Read();
		} else {
			return 0xff;
		}
	case MODE_DUALOPL2:
		//Only return for the lower ports
		if ( port & 1 ) {
			return 0xff;
		}
		//Make sure the low bits are 6 on opl2
		return chip[ (port >> 1) & 1].Read() | 0x6;
	}
	return 0;
}

void Module::Init(Mode m)
{
	mode = m;
	memset(cache, 0, ARRAY_LEN(cache));

	switch (mode) {
	case MODE_OPL3:
		break;
	case MODE_OPL3GOLD:
		adlib_gold = new AdlibGold(mixerChan->GetSampleRate());
		break;
	case MODE_OPL2: break;
	case MODE_DUALOPL2:
		// Setup opl3 mode in the hander
		handler->WriteReg(0x105, 1);
		// Also set it up in the cache so the capturing will start opl3
		CacheWrite(0x105, 1);
		break;
	}
}

} // namespace Adlib

static void OPL_CallBack(uint16_t len)
{
	module->handler->Generate(module->mixerChan, len);
	// Disable the sound generation after 30 seconds of silence
	if ((PIC_Ticks - module->lastUsed) > 30000) {
		uint8_t i;
		for (i=0xb0;i<0xb9;i++) if (module->cache[i]&0x20||module->cache[i+0x100]&0x20) break;
		if (i==0xb9) module->mixerChan->Enable(false);
		else module->lastUsed = PIC_Ticks;
	}
}

/*
	Save the current state of the operators as instruments in an reality adlib tracker file
*/
#if 0
static void SaveRad() {
	char b[16 * 1024];
	int w = 0;

	FILE* handle = OpenCaptureFile("RAD Capture",".rad");
	if ( !handle )
		return;
	//Header
	fwrite( "RAD by REALiTY!!", 1, 16, handle );
	b[w++] = 0x10;		//version
	b[w++] = 0x06;		//default speed and no description
	//Write 18 instuments for all operators in the cache
	for ( int i = 0; i < 18; i++ ) {
		uint8_t* set = module->cache + ( i / 9 ) * 256;
		const auto offset = ((i % 9) / 3) * 8 + (i % 3);
		uint8_t* base = set + offset;
		b[w++] = 1 + i;		//instrument number
		b[w++] = base[0x23];
		b[w++] = base[0x20];
		b[w++] = base[0x43];
		b[w++] = base[0x40];
		b[w++] = base[0x63];
		b[w++] = base[0x60];
		b[w++] = base[0x83];
		b[w++] = base[0x80];
		b[w++] = set[0xc0 + (i % 9)];
		b[w++] = base[0xe3];
		b[w++] = base[0xe0];
	}
	b[w++] = 0;		//instrument 0, no more instruments following
	b[w++] = 1;		//1 pattern following
	//Zero out the remaining part of the file a bit to make rad happy
	for ( int i = 0; i < 64; i++ ) {
		b[w++] = 0;
	}
	fwrite( b, 1, w, handle );
	fclose( handle );
};
#endif

static void OPL_SaveRawEvent(bool pressed) {
	if (!pressed)
		return;
//	SaveRad();return;
	/* Check for previously opened wave file */
	if ( module->capture ) {
		delete module->capture;
		module->capture = 0;
		LOG_MSG("Stopped Raw OPL capturing.");
	} else {
		LOG_MSG("Preparing to capture Raw OPL, will start with first note played.");
		module->capture = new Adlib::Capture( &module->cache );
	}
}

namespace Adlib {

static Handler * make_opl_handler(const std::string &oplemu, OPL_Mode mode)
{
	if (oplemu == "fast") {
		const bool is_opl3 = (mode >= OPL_opl3);
		return new DBOPL::Handler(is_opl3);
	}
	if (oplemu == "compat") {
		if (mode == OPL_opl2)
			return new OPL2::Handler();
		else
			return new OPL3::Handler();
	}
	if (oplemu == "mame") {
		if (mode == OPL_opl2)
			return new MAMEOPL2::Handler();
		else
			return new MAMEOPL3::Handler();
	}
	if (oplemu == "nuked") {
		return new NukedOPL::Handler();
	}
	return new NukedOPL::Handler();
}

Module::Module(Section *configuration)
        : Module_base(configuration),
          mode(MODE_OPL2), // TODO this is set in Init and there's no good default
          reg{0},          // union
          ctrl{false, 0, 0xff, 0xff, false},
          mixerChan(nullptr),
          lastUsed(0),
          handler(nullptr),
          capture(nullptr)
{
	Section_prop * section=static_cast<Section_prop *>(configuration);
	const auto base = static_cast<uint16_t>(section->Get_hex("sbbase"));

	ctrl.mixer = section->Get_bool("sbmixer");

	std::set channel_features = {ChannelFeature::ReverbSend, ChannelFeature::ChorusSend};
	if (oplmode != OPL_opl2)
		channel_features.emplace(ChannelFeature::Stereo);

	mixerChan = MIXER_AddChannel(OPL_CallBack, 0, "FM", channel_features);

	//Used to be 2.0, which was measured to be too high. Exact value depends on card/clone.
	mixerChan->SetScale( 1.5f );  

	handler = make_opl_handler(section->Get_string("oplemu"), oplmode);
	handler->Init(mixerChan->GetSampleRate());

	bool single = false;
	switch ( oplmode ) {
	case OPL_opl2:
		single = true;
		Init( Adlib::MODE_OPL2 );
		break;
	case OPL_dualopl2:
		Init( Adlib::MODE_DUALOPL2 );
		break;
	case OPL_opl3:
		Init( Adlib::MODE_OPL3 );
		break;
	case OPL_opl3gold:
		Init( Adlib::MODE_OPL3GOLD );
		break;
	case OPL_cms:
	case OPL_none:
		break;
	}
	using namespace std::placeholders;

	const auto read_from = std::bind(&Module::PortRead, this, _1, _2);
	const auto write_to = std::bind(&Module::PortWrite, this, _1, _2, _3);

	// 0x388-0x38b ports (read/write)
	constexpr io_port_t port_0x388 = 0x388;
	WriteHandler[0].Install(port_0x388, write_to, io_width_t::byte, 4);
	ReadHandler[0].Install(port_0x388, read_from, io_width_t::byte, 4);

	// 0x220-0x223 ports (read/write)
	if (!single) {
		WriteHandler[1].Install(base, write_to, io_width_t::byte, 4);
		ReadHandler[1].Install(base, read_from, io_width_t::byte, 4);
	}
	// 0x228-0x229 ports (write)
	WriteHandler[2].Install(base + 8u, write_to, io_width_t::byte, 2);

	// 0x228 port (read)
	ReadHandler[2].Install(base + 8u, read_from, io_width_t::byte, 1);

	MAPPER_AddHandler(OPL_SaveRawEvent, SDL_SCANCODE_UNKNOWN, 0,
	                  "caprawopl", "Rec. OPL");
}

Module::~Module()
{
	delete capture;
	capture = nullptr;
	delete handler;
	handler = nullptr;

	if (adlib_gold) {
		delete adlib_gold;
		adlib_gold = nullptr;
	}
}

//Initialize static members
OPL_Mode Module::oplmode=OPL_none;

} // namespace Adlib

void OPL_Init(Section* sec,OPL_Mode oplmode) {
	Adlib::Module::oplmode = oplmode;
	module = new Adlib::Module( sec );
}

void OPL_ShutDown(Section* /*sec*/){
	delete module;
	module = 0;

}
