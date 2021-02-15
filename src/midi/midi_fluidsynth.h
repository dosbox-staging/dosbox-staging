/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2021  Nikos Chantziaras <realnc@gmail.com>
 *  Copyright (C) 2020-2021  The DOSBox Staging Team
 *  Copyright (C) 2002-2011  The DOSBox Team
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

#ifndef DOSBOX_MIDI_FLUIDSYNTH_H
#define DOSBOX_MIDI_FLUIDSYNTH_H

#include "midi_handler.h"

#if C_FLUIDSYNTH

#include <memory>
#include <vector>
#include <fluidsynth.h>

#include "mixer.h"
#include "soft_limiter.h"

class MidiHandlerFluidsynth final : public MidiHandler {
private:
	using fsynth_ptr_t = std::unique_ptr<fluid_synth_t, decltype(&delete_fluid_synth)>;

	using fluid_settings_ptr_t =
	        std::unique_ptr<fluid_settings_t, decltype(&delete_fluid_settings)>;

	using mixer_channel_ptr_t =
	        std::unique_ptr<MixerChannel, decltype(&MIXER_DelChannel)>;

public:
	MidiHandlerFluidsynth();
	void PrintStats();
	const char *GetName() const override { return "fluidsynth"; }
	bool Open(const char *conf) override;
	void Close() override;
	void PlayMsg(const uint8_t *msg) override;
	void PlaySysex(uint8_t *sysex, size_t len) override;

private:
	static constexpr uint16_t expected_max_frames = (96000 / 1000) + 4;
	void MixerCallBack(uint16_t requested_frames);
	void SetMixerLevel(const AudioFrame &prescale_level) noexcept;

	fluid_settings_ptr_t settings{nullptr, &delete_fluid_settings};
	fsynth_ptr_t synth{nullptr, &delete_fluid_synth};
	mixer_channel_ptr_t channel{nullptr, MIXER_DelChannel};
	AudioFrame prescale_level = {1.0f, 1.0f};
	SoftLimiter soft_limiter;
	std::vector<float> stream{};

	bool is_open = false;
};

#endif // C_FLUIDSYNTH

#endif
