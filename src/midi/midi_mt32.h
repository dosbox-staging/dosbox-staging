/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2012-2021  sergm <sergm@bigmir.net>
 *  Copyright (C) 2020-2021  The DOSBox Staging Team
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

#ifndef DOSBOX_MIDI_MT32_H
#define DOSBOX_MIDI_MT32_H

#include "midi_handler.h"

#if C_MT32EMU

#include <memory>
#include <thread>
#include <vector>

#define MT32EMU_API_TYPE 3
#include <mt32emu/mt32emu.h>
#if MT32EMU_VERSION_MAJOR != 2 || MT32EMU_VERSION_MINOR < 1
#error Incompatible mt32emu library version
#endif

#include "mixer.h"
#include "../libs/rwqueue/readerwritercircularbuffer.h"
#include "soft_limiter.h"

class MidiHandler_mt32 final : public MidiHandler {
private:
	static constexpr int FRAMES_PER_BUFFER = 1024; // synth granularity
	static constexpr int SAMPLES_PER_BUFFER = FRAMES_PER_BUFFER * 2; // L & R

	using render_buffer_t = std::vector<float>;
	using play_buffer_t = std::vector<int16_t>;
	using ring_t = moodycamel::BlockingReaderWriterCircularBuffer<play_buffer_t>;
	using channel_t = std::unique_ptr<MixerChannel, decltype(&MIXER_DelChannel)>;
	using conditional_t = moodycamel::weak_atomic<bool>;

public:
	using service_t = std::unique_ptr<MT32Emu::Service>;

	MidiHandler_mt32();
	~MidiHandler_mt32() override;
	void Close() override;
	const char *GetName() const override { return "mt32"; }
	bool Open(const char *conf) override;
	void PlayMsg(const uint8_t *msg) override;
	void PlaySysex(uint8_t *sysex, size_t len) override;

private:
	uint32_t GetMidiEventTimestamp() const;
	void MixerCallBack(uint16_t len);
	void SetMixerLevel(const AudioFrame &desired) noexcept;
	uint16_t GetRemainingFrames();
	void Render();

	// Managed objects
	channel_t channel{nullptr, MIXER_DelChannel};
	play_buffer_t play_buffer{};
	ring_t ring{4}; // Handle up to four buffers in the ring
	service_t service{};
	std::thread renderer{};
	AudioFrame limiter_ratio = {1.0f, 1.0f};
	SoftLimiter soft_limiter;

	// The following two members let us determine the total number of played
	// frames, which is used by GetMidiEventTimestamp() to calculate a total
	// time offset.
	uint32_t total_buffers_played = 0;
	uint16_t last_played_frame = 0; // relative frame-offset in the play buffer

	conditional_t keep_rendering = false;
	bool is_open = false;
};

#endif // C_MT32EMU

#endif
