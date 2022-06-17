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

#include "bit_view.h"
#include "dosbox.h"
#include "mixer.h"

class SurroundProcessor;
class StereoProcessor;

enum class StereoProcessorControlReg {
	VolumeLeft,
	VolumeRight,
	Bass,
	Treble,
	SwitchFunctions,
};

union StereoProcessorSwitchFunctions {
	uint8_t data = 0;
	bit_view<0, 3> source_selector;
	bit_view<3, 2> stereo_mode;
};

enum class StereoProcessorSourceSelector : uint8_t {
    SoundA1 = 2,
    SoundA2 = 3,
    SoundB1 = 4,
    SoundB2 = 5,
    Stereo1 = 6,
    Stereo2 = 7,
};

// Apparently, the values for LinearStereo and PseudoStereo are switched in
// the specs.
enum class StereoProcessorStereoMode : uint8_t {
	ForcedMono    = 0,
	LinearStereo  = 1,
	PseudoStereo  = 2,
	SpatialStereo = 3,
};

class AdlibGold {
public:
	AdlibGold(const uint16_t sample_rate);
	~AdlibGold();

	void SurroundControlWrite(const uint8_t val) noexcept;
	void StereoControlWrite(const StereoProcessorControlReg reg,
	                        const uint8_t data) noexcept;

	void Process(const int16_t *in, const uint32_t frames, float *out) noexcept;

private:
	std::unique_ptr<SurroundProcessor> surround_processor = {};
	std::unique_ptr<StereoProcessor> stereo_processor = {};
};

#endif
