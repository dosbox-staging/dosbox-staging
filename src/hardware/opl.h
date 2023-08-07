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

#ifndef DOSBOX_OPL_H
#define DOSBOX_OPL_H

#include "dosbox.h"

#include <cmath>
#include <memory>
#include <queue>

#include "adlib_gold.h"
#include "mixer.h"
#include "inout.h"
#include "setup.h"
#include "pic.h"
#include "hardware.h"

#include "nuked/opl3.h"

class Timer {
public:
	Timer(int16_t micros);

	bool Update(const double time);
	void Reset();
	void SetCounter(const uint8_t val);
	void SetMask(const bool set);
	void Stop();
	void Start(const double time);

private:
	double start            = 0.0; // Rounded down start time
	double trigger          = 0.0; // Time when you overflow
	double clock_interval   = 0.0; // Clock interval
	double counter_interval = 0.0; // Cycle interval
	uint8_t counter         = 0;

	bool enabled  = false;
	bool overflow = false;
	bool masked   = false;
};

class Chip {
public:
	Chip();

	// Last selected register
	Timer timer0;
	Timer timer1;

	// Check for it being a write to the timer
	bool Write(const io_port_t addr, const uint8_t val);

	// Read the current timer state, will use current double
	uint8_t Read();
};

// The cache for 2 chips or an opl3
typedef uint8_t RegisterCache[512];

// Internal class used for dro capturing
class DroCapture;

enum class Mode { Opl2, DualOpl2, Opl3, Opl3Gold };

class OPL {
public:
	mixer_channel_t channel = {};

	RegisterCache cache = {};

	std::unique_ptr<DroCapture> capture = {};

	OPL(Section *configuration, const OplMode _oplmode);
	~OPL();

	// prevent copy
	OPL(const OPL &) = delete;

	// prevent assignment
	OPL &operator=(const OPL &) = delete;

private:
	IO_ReadHandleObject ReadHandler[3];
	IO_WriteHandleObject WriteHandler[3];

	std::queue<AudioFrame> fifo = {};

	Mode mode = {};

	Chip chip[2] = {};

	opl3_chip oplchip = {};
	uint8_t newm      = 0;

	std::unique_ptr<AdlibGold> adlib_gold = {};

	// Playback related
	double last_rendered_ms = 0.0;
	double ms_per_frame     = 0.0;

	// Last selected address in the chip for the different modes
	union {
		uint16_t normal = 0;
		uint8_t dual[2];
	} reg = {};

	static constexpr auto default_volume = 0xff;
	struct {
		uint8_t index = 0;
		uint8_t lvol  = default_volume;
		uint8_t rvol  = default_volume;

		bool active = false;
		bool mixer  = false;
	} ctrl = {};

	void Init(const uint16_t sample_rate);

	void AudioCallback(const uint16_t frames);
	AudioFrame RenderFrame();
	void RenderUpToNow();

	void PortWrite(const io_port_t port, const io_val_t value,
	               const io_width_t width);

	uint8_t PortRead(const io_port_t port, const io_width_t width);

	io_port_t WriteAddr(const io_port_t port, const uint8_t val);
	void WriteReg(const io_port_t selected_reg, const uint8_t val);
	void CacheWrite(const io_port_t port, const uint8_t val);
	void DualWrite(const uint8_t index, const uint8_t reg, const uint8_t value);

	void AdlibGoldControlWrite(const uint8_t val);
	uint8_t AdlibGoldControlRead(void);
};

#endif
