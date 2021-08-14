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

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#define MT32EMU_API_TYPE 3
#include <mt32emu/mt32emu.h>

#include "mixer.h"
#include "rwqueue.h"
#include "soft_limiter.h"

static_assert(MT32EMU_VERSION_MAJOR > 2 ||
                      (MT32EMU_VERSION_MAJOR == 2 && MT32EMU_VERSION_MINOR >= 5),
              "libmt32emu >= 2.5.0 required (using " MT32EMU_VERSION ")");

class MidiHandler_mt32 final : public MidiHandler {
private:
	using channel_t = std::unique_ptr<MixerChannel, decltype(&MIXER_DelChannel)>;

public:
	using service_t = std::unique_ptr<MT32Emu::Service>;

	MidiHandler_mt32();
	~MidiHandler_mt32() override;
	void Close() override;
	const char *GetName() const override { return "mt32"; }
	MIDI_RC ListAll(Program *caller) override;
	bool Open(const char *conf) override;
	void PlayMsg(const uint8_t *msg) override;
	void PlaySysex(uint8_t *sysex, size_t len) override;
	void PrintStats();

private:
	uint32_t GetMidiEventTimestamp() const;
	service_t GetService();
	void MixerCallBack(uint16_t len);
	void SetMixerLevel(const AudioFrame &desired) noexcept;
	uint16_t GetRemainingFrames();
	void Render();

	// Managed objects
	channel_t channel{nullptr, MIXER_DelChannel};

	std::vector<int16_t> play_buffer = {};
	static constexpr auto num_buffers = 4;
	RWQueue<std::vector<int16_t>> playable{num_buffers};
	RWQueue<std::vector<int16_t>> backstock{num_buffers};

	std::mutex service_mutex = {};
	service_t service = {};
	std::thread renderer = {};
	SoftLimiter soft_limiter;

	// The following two members let us determine the total number of played
	// frames, which is used by GetMidiEventTimestamp() to calculate a total
	// time offset.
	uint32_t total_buffers_played = 0;
	uint16_t last_played_frame = 0; // relative frame-offset in the play buffer

	std::atomic_bool keep_rendering = {};
	bool is_open = false;
};

#endif // C_MT32EMU

#endif
