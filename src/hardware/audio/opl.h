// SPDX-FileCopyrightText:  2002-2024 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_OPL_H
#define DOSBOX_OPL_H

#include "dosbox.h"

#include <cmath>
#include <memory>
#include <queue>

#include "ESFMu/esfm.h"
#include "nuked/opl3.h"

#include "private/adlib_gold.h"

#include "audio/mixer.h"
#include "config/setup.h"
#include "hardware/pic.h"
#include "hardware/port.h"

enum class OplMode { None, Opl2, DualOpl2, Opl3, Opl3Gold, Esfm };

class Timer {
public:
	Timer(const int micros);

	bool Update(const double time);
	void Reset();

	void SetCounter(const uint8_t val);
	uint8_t GetCounter() const;

	void SetMask(const bool set);
	bool IsMasked() const;

	void Stop();
	void Start(const double time);
	bool IsEnabled() const;

private:
	// Rounded down start time
	double start = 0.0;

	// Time when you overflow
	double trigger = 0.0;

	// Clock interval
	double clock_interval = 0.0;

	// Cycle interval
	double counter_interval = 0.0;

	uint8_t counter = 0;

	bool enabled  = false;
	bool overflow = false;
	bool masked   = false;
};

class OplChip {
public:
	OplChip();

	// Last selected register
	Timer timer0;
	Timer timer1;

	// Check for it being a write to the timer
	bool Write(const io_port_t addr, const uint8_t val);

	// Read the current timer state, will use current double
	uint8_t Read();

	uint8_t EsfmReadbackReg(const uint16_t reg);
};

// The cache for two OPL chips (Dual OPL2) or an OPL3 (stereo)
typedef uint8_t OplRegisterCache[512];

enum class EsfmMode { Legacy, Native };

class Opl {
public:
	MixerChannelPtr channel = {};

	OplRegisterCache cache = {};

	Opl(Section* configuration, const OplMode opl_mode);
	~Opl();

	// prevent copy
	Opl(Opl&) = delete;

	// prevent assignment
	Opl& operator=(Opl&) = delete;

private:
	IO_ReadHandleObject ReadHandler[3];
	IO_WriteHandleObject WriteHandler[3];

	std::queue<AudioFrame> fifo = {};
	std::mutex mutex = {};

	OplChip chip[2]  = {};

	struct {
		OplMode mode   = OplMode::None;
		opl3_chip chip = {};
		uint8_t newm   = 0;
	} opl = {};

	std::unique_ptr<AdlibGold> adlib_gold = {};

	struct {
		esfm_chip chip = {};
		EsfmMode mode  = EsfmMode::Legacy;
	} esfm = {};

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

		bool active        = false;
		bool mixer_enabled = false;

		bool wants_dc_bias_removed = false;
	} ctrl = {};

	void Init();

	void AudioCallback(const int frames);
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

	void EsfmSetLegacyMode();
};

void OPL_Init(Section *sec, OplMode mode);
void OPL_Destroy();

// CMS/Game Blaster, OPL, and ESFM configuration and initialisation
void OPL_AddConfigSettings(const ConfigPtr &conf);

#endif // DOSBOX_OPL_H
