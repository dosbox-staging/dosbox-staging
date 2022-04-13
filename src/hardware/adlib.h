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


#ifndef DOSBOX_ADLIB_H
#define DOSBOX_ADLIB_H

#include "dosbox.h"
#include "mixer.h"
#include "inout.h"
#include "setup.h"
#include "pic.h"
#include "hardware.h"

#include <cmath>

namespace Adlib {

class Timer {
	//Rounded down start time
	double start;
	//Time when you overflow
	double trigger;
	//Clock interval
	double clockInterval;
	//cycle interval
	double counterInterval;
	uint8_t counter;
	bool enabled;
	bool overflow;
	bool masked;

public:
	Timer(int16_t micros)
	        : start(0.0),
	          trigger(0.0),
	          clockInterval(micros * 0.001), // interval in milliseconds
	          counterInterval(0.0),
	          counter(0),
	          enabled(false),
	          overflow(false),
	          masked(false)
	{
		SetCounter(0);
	}

	//Update returns with true if overflow
	//Properly syncs up the start/end to current time and changing intervals
	bool Update(const double time)
	{
		if (enabled && (time >= trigger)) {
			// How far into the next cycle
			const double deltaTime = time - trigger;
			// Sync start to last cycle
			const auto counterMod = fmod(deltaTime, counterInterval);
			start = time - counterMod;
			trigger = start + counterInterval;
			//Only set the overflow flag when not masked
			if (!masked) {
				overflow = true;
			}
		}
		return overflow;
	}

	//On a reset make sure the start is in sync with the next cycle
	void Reset() {
		overflow = false;
	}

	void SetCounter(uint8_t val) {
		counter = val;
		//Interval for next cycle
		counterInterval = (256 - counter) * clockInterval;
	}

	void SetMask(bool set) {
		masked = set;
		if (masked)
			overflow = false;
	}

	void Stop( ) {
		enabled = false;
	}

	void Start(const double time)
	{
		// Only properly start when not running before
		if (!enabled) {
			enabled = true;
			overflow = false;
			//Sync start to the last clock interval
			const auto clockMod = fmod(time, clockInterval);
			start = time - clockMod;
			//Overflow trigger
			trigger = start + counterInterval;
		}
	}
};

struct Chip {
	//Last selected register
	Timer timer0, timer1;
	//Check for it being a write to the timer
	bool Write( uint32_t addr, uint8_t val );
	//Read the current timer state, will use current double
	uint8_t Read( );

	Chip();
};

//The type of handler this is
typedef enum {
	MODE_OPL2,
	MODE_DUALOPL2,
	MODE_OPL3,
	MODE_OPL3GOLD
} Mode;

class Handler {
public:
	//Write an address to a chip, returns the address the chip sets
	virtual uint32_t WriteAddr(io_port_t port, uint8_t val) = 0;
	//Write to a specific register in the chip
	virtual void WriteReg( uint32_t addr, uint8_t val ) = 0;
	//Generate a certain amount of samples
	virtual void Generate(mixer_channel_t &chan, uint16_t samples) = 0;
	//Initialize at a specific sample rate and mode
	virtual void Init(uint32_t rate) = 0;
	virtual ~Handler() = default;
};

//The cache for 2 chips or an opl3
typedef uint8_t RegisterCache[512];

//Internal class used for dro capturing
class Capture;

class Module: public Module_base {
	IO_ReadHandleObject ReadHandler[3];
	IO_WriteHandleObject WriteHandler[3];

	//Mode we're running in
	Mode mode;
	//Last selected address in the chip for the different modes
	union {
		uint32_t normal;
		uint8_t dual[2];
	} reg;
	struct {
		bool active;
		uint8_t index;
		uint8_t lvol;
		uint8_t rvol;
		bool mixer;
	} ctrl;
	void CacheWrite( uint32_t reg, uint8_t val );
	void DualWrite( uint8_t index, uint8_t reg, uint8_t val );
	void CtrlWrite( uint8_t val );
	uint8_t CtrlRead(void);

public:
	static OPL_Mode oplmode;
	mixer_channel_t mixerChan;
	uint32_t lastUsed;				//Ticks when adlib was last used to turn of mixing after a few second

	Handler* handler;				//Handler that will generate the sound
	RegisterCache cache;
	Capture* capture;
	Chip	chip[2];

	//Handle port writes
	void PortWrite(io_port_t port, io_val_t value, io_width_t width);
	uint8_t PortRead(io_port_t port, io_width_t width);
	void Init(Mode m);

	Module(Section *configuration);
	~Module() override;

	Module(const Module&) = delete; // prevent copy
	Module& operator=(const Module&) = delete; // prevent assignment
};

} // namespace Adlib

#endif
