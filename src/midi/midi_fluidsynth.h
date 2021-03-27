/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2021  The DOSBox Staging Team
 *  Copyright (C) 2020-2020  Nikos Chantziaras <realnc@gmail.com>
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

#include <atomic>
#include <memory>
#include <vector>
#include <fluidsynth.h>
#include <thread>

#include "mixer.h"
#include "rwqueue.h"
#include "soft_limiter.h"

class MidiHandlerFluidsynth final : public MidiHandler {
public:
	MidiHandlerFluidsynth();
	~MidiHandlerFluidsynth() override;
	void PrintStats();
	const char *GetName() const override { return "fluidsynth"; }
	bool Open(const char *conf) override;
	void Close() override;
	void PlayMsg(const uint8_t *msg) override;
	void PlaySysex(uint8_t *sysex, size_t len) override;
	MIDI_RC ListAll(Program *caller) override;

private:
	void MixerCallBack(uint16_t requested_frames);
	void SetMixerLevel(const AudioFrame &levels) noexcept;
	uint16_t GetRemainingFrames();
	void Render();

	using fluid_settings_ptr_t =
	        std::unique_ptr<fluid_settings_t, decltype(&delete_fluid_settings)>;
	using fsynth_ptr_t = std::unique_ptr<fluid_synth_t, decltype(&delete_fluid_synth)>;
	using channel_t = std::unique_ptr<MixerChannel, decltype(&MIXER_DelChannel)>;

	fluid_settings_ptr_t settings{nullptr, &delete_fluid_settings};
	fsynth_ptr_t synth{nullptr, &delete_fluid_synth};
	channel_t channel{nullptr, MIXER_DelChannel};
	std::string selected_font = "";

	std::vector<int16_t> play_buffer = {};
	static constexpr auto num_buffers = 8;
	RWQueue<std::vector<int16_t>> playable{num_buffers};
	RWQueue<std::vector<int16_t>> backstock{num_buffers};

	std::thread renderer = {};
	SoftLimiter soft_limiter;

	uint16_t last_played_frame = 0; // relative frame-offset in the play buffer
	std::atomic_bool keep_rendering = {};
	bool is_open = false;
};

#endif // C_FLUIDSYNTH

#endif
