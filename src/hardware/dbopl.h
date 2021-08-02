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
#include "dosbox.h"

//Use 8 handlers based on a small logatirmic wavetabe and an exponential table for volume
#define WAVE_HANDLER	10
//Use a logarithmic wavetable with an exponential table for volume
#define WAVE_TABLELOG	11
//Use a linear wavetable with a multiply table for volume
#define WAVE_TABLEMUL	12

//Select the type of wave generator routine
#define DBOPL_WAVE WAVE_TABLEMUL

namespace DBOPL {

struct Chip;
struct Operator;
struct Channel;

#if (DBOPL_WAVE == WAVE_HANDLER)
typedef Bits (*WaveHandler)(Bitu i, Bitu volume);
#endif

typedef Bits ( DBOPL::Operator::*VolumeHandler) ( );
typedef Channel *(DBOPL::Channel::*SynthHandler)(Chip *chip,
                                                 uint16_t samples,
                                                 Bit32s *output);

//Different synth modes that can generate blocks of data
typedef enum {
	sm2AM,
	sm2FM,
	sm3AM,
	sm3FM,
	sm4Start,
	sm3FMFM,
	sm3AMFM,
	sm3FMAM,
	sm3AMAM,
	sm6Start,
	sm2Percussion,
	sm3Percussion,
} SynthMode;

//Shifts for the values contained in chandata variable
enum {
	SHIFT_KSLBASE = 16,
	SHIFT_KEYCODE = 24,
};

struct Operator {
public:
	//Masks for operator 20 values
	enum {
		MASK_KSR = 0x10,
		MASK_SUSTAIN = 0x20,
		MASK_VIBRATO = 0x40,
		MASK_TREMOLO = 0x80,
	};

	typedef enum {
		OFF,
		RELEASE,
		SUSTAIN,
		DECAY,
		ATTACK,
	} State;

	VolumeHandler volHandler;

#if (DBOPL_WAVE == WAVE_HANDLER)
	WaveHandler waveHandler;	//Routine that generate a wave 
#else
	Bit16s* waveBase;
	Bit32u waveMask;
	Bit32u waveStart;
#endif
	Bit32u waveIndex;			//WAVE_BITS shifted counter of the frequency index
	Bit32u waveAdd;				//The base frequency without vibrato
	Bit32u waveCurrent;			//waveAdd + vibratao

	Bit32u chanData;			//Frequency/octave and derived data coming from whatever channel controls this
	Bit32u freqMul;				//Scale channel frequency with this, TODO maybe remove?
	Bit32u vibrato;				//Scaled up vibrato strength
	Bit32s sustainLevel;		//When stopping at sustain level stop here
	Bit32s totalLevel;			//totalLevel is added to every generated volume
	Bit32u currentLevel;		//totalLevel + tremolo
	Bit32s volume;				//The currently active volume
	
	Bit32u attackAdd;			//Timers for the different states of the envelope
	Bit32u decayAdd;
	Bit32u releaseAdd;
	Bit32u rateIndex;			//Current position of the evenlope

	Bit8u rateZero;				//Bits for the different states of the envelope having no changes
	Bit8u keyOn;				//Bitmask of different values that can generate keyon
	//Registers, also used to check for changes
	Bit8u reg20, reg40, reg60, reg80, regE0;
	//Active part of the envelope we're in
	Bit8u state;
	//0xff when tremolo is enabled
	Bit8u tremoloMask;
	//Strength of the vibrato
	Bit8u vibStrength;
	//Keep track of the calculated KSR so we can check for changes
	Bit8u ksr;
private:
	void SetState( Bit8u s );
	void UpdateAttack( const Chip* chip );
	void UpdateRelease( const Chip* chip );
	void UpdateDecay( const Chip* chip );
public:
	void UpdateAttenuation();
	void UpdateRates( const Chip* chip );
	void UpdateFrequency( );

	void Write20( const Chip* chip, Bit8u val );
	void Write40( const Chip* chip, Bit8u val );
	void Write60( const Chip* chip, Bit8u val );
	void Write80( const Chip* chip, Bit8u val );
	void WriteE0( const Chip* chip, Bit8u val );

	bool Silent() const;
	void Prepare( const Chip* chip );

	void KeyOn( Bit8u mask);
	void KeyOff( Bit8u mask);

	template< State state>
	Bits TemplateVolume( );

	Bit32s RateForward( Bit32u add );
	Bitu ForwardWave();
	Bitu ForwardVolume();

	Bits GetSample( Bits modulation );
	Bits GetWave( Bitu index, Bitu vol );
public:
	Operator();
};

struct Channel {
	Operator op[2]; //Leave on top of struct for simpler pointer math.
	inline Operator* Op( Bitu index ) {
		return &( ( this + (index >> 1) )->op[ index & 1 ]);
	}
	SynthHandler synthHandler;
	Bit32u chanData;		//Frequency/octave and derived values
	Bit32s old[2];			//Old data for feedback

	Bit8u feedback;			//Feedback shift
	Bit8u regB0;			//Register values to check for changes
	Bit8u regC0;
	//This should correspond with reg104, bit 6 indicates a Percussion channel, bit 7 indicates a silent channel
	Bit8u fourMask;
	Bit8s maskLeft;		//Sign extended values for both channel's panning
	Bit8s maskRight;

	//Forward the channel data to the operators of the channel
	void SetChanData( const Chip* chip, Bit32u data );
	//Change in the chandata, check for new values and if we have to forward to operators
	void UpdateFrequency( const Chip* chip, Bit8u fourOp );
	void UpdateSynth(const Chip* chip);
	void WriteA0( const Chip* chip, Bit8u val );
	void WriteB0( const Chip* chip, Bit8u val );
	void WriteC0( const Chip* chip, Bit8u val );

	//call this for the first channel
	template< bool opl3Mode >
	void GeneratePercussion( Chip* chip, Bit32s* output );

	//Generate blocks of data in specific modes
	template <SynthMode mode>
	Channel *BlockTemplate(Chip *chip, uint16_t samples, Bit32s *output);
	Channel();
};

struct Chip {
	// 18 channels with 2 operators each.
	// Leave on top of struct for simpler pointer math.
	Channel chan[18];

	// This is used as the base counter for vibrato and tremolo
	uint32_t lfoCounter = 0;
	uint32_t lfoAdd = 0;

	uint32_t noiseCounter = 0;
	uint32_t noiseAdd = 0;
	uint32_t noiseValue = 0;

	// Frequency scales for the different multiplications
	uint32_t freqMul[16];
	// Rates for decay and release for rate of this chip
	uint32_t linearRates[76];
	// Best match attack rates for the rate of this chip
	uint32_t attackRates[76];

	uint8_t reg104 = 0;
	uint8_t reg08 = 0;
	uint8_t reg04 = 0;
	uint8_t regBD = 0;
	uint8_t vibratoIndex = 0;
	uint8_t tremoloIndex = 0;
	int8_t vibratoSign = 0;
	uint8_t vibratoShift = 0;
	uint8_t tremoloValue = 0;
	uint8_t vibratoStrength = 0;
	uint8_t tremoloStrength = 0;
	uint8_t waveFormMask = 0x0; // Mask for allowed wave forms
	int8_t opl3Active = 0; // 0 or -1 when enabled

	//Return the maximum amount of samples before and LFO change
	Bit32u ForwardLFO(uint16_t samples);
	Bit32u ForwardNoise();

	void WriteBD( Bit8u val );
	void WriteReg(Bit32u reg, Bit8u val );

	Bit32u WriteAddr(uint16_t port, Bit8u val);

	void GenerateBlock2(uint16_t samples, Bit32s *output);
	void GenerateBlock3(uint16_t samples, Bit32s *output);

	//Update the synth handlers in all channels
	void UpdateSynths();
	void Generate(uint16_t samples);
	void Setup( Bit32u r );

	Chip() = default; // can't be placed on top because of offset math
};

struct Handler : public Adlib::Handler {
	DBOPL::Chip chip = {};
	virtual Bit32u WriteAddr(io_port_t port, Bit8u val);
	virtual void WriteReg( Bit32u addr, Bit8u val );
	virtual void Generate(MixerChannel *chan, uint16_t samples);
	virtual void Init(uint32_t rate);
};

} // namespace DBOPL
