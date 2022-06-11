/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2022  The DOSBox Staging Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef DOSBOX_ADLIB_GOLD_H
#define DOSBOX_ADLIB_GOLD_H

#include "dosbox.h"
#include "mixer.h"

#include "../libs/TDA8425_emu/TDA8425_emu.h"
#include "../libs/YM7128B_emu/YM7128B_emu.h"

class AdlibGoldSurroundProcessor {
public:
	AdlibGoldSurroundProcessor(const uint16_t sample_rate);
	~AdlibGoldSurroundProcessor();

	void ControlWrite(const uint8_t val) noexcept;
	AudioFrame Process(const AudioFrame &frame) noexcept;

	// prevent copying and assignment
	AdlibGoldSurroundProcessor(const AdlibGoldSurroundProcessor &) = delete;
	AdlibGoldSurroundProcessor &operator=(const AdlibGoldSurroundProcessor &) = delete;

private:
	YM7128B_ChipIdeal *chip = nullptr;

	struct {
		uint8_t sci  = 0;
		uint8_t a0   = 0;
		uint8_t addr = 0;
		uint8_t data = 0;
	} control_state = {};
};

class AdlibGoldStereoProcessor {
public:
	AdlibGoldStereoProcessor(const uint16_t sample_rate);
	~AdlibGoldStereoProcessor();

	void Reset() noexcept;
	void ControlWrite(const TDA8425_Reg reg, const TDA8425_Register data) noexcept;
	AudioFrame Process(const AudioFrame &frame) noexcept;

	// prevent copying and assignment
	AdlibGoldStereoProcessor(const AdlibGoldStereoProcessor &) = delete;
	AdlibGoldStereoProcessor &operator=(const AdlibGoldStereoProcessor &) = delete;

private:
	TDA8425_Chip *chip = nullptr;
};

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

#endif
