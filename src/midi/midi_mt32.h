/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2012-2020  sergm <sergm@bigmir.net>
 *  Copyright (C) 2020       The dosbox-staging team
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

#define MT32EMU_API_TYPE 3
#include <mt32emu/mt32emu.h>
#if MT32EMU_VERSION_MAJOR != 2 || MT32EMU_VERSION_MINOR < 1
#error Incompatible mt32emu library version
#endif

#include <SDL_thread.h>

#include "mixer.h"

using mt32_service_ptr_t = std::unique_ptr<MT32Emu::Service>;

class MidiHandler_mt32 final : public MidiHandler {
private:
	using mixer_channel_ptr_t =
	        std::unique_ptr<MixerChannel, decltype(&MIXER_DelChannel)>;

public:
	~MidiHandler_mt32();

	void Close() override;
	const char *GetName() const override { return "mt32"; }
	bool Open(const char *conf) override;
	void PlayMsg(const uint8_t *msg) override;
	void PlaySysex(uint8_t *sysex, size_t len) override;

private:
	uint32_t GetMidiEventTimestamp() const;
	void MixerCallBack(uint16_t len);
	static int ProcessingThread(void *data);
	void RenderingLoop();

	mixer_channel_ptr_t channel{nullptr, MIXER_DelChannel};
	mt32_service_ptr_t service{};

	// TODO: replace pointers with std::unique_ptr
	SDL_Thread *thread = nullptr;
	SDL_mutex *lock = nullptr;
	SDL_cond *framesInBufferChanged = nullptr;
	int16_t *audioBuffer = nullptr;

	// Ongoing state-tracking
	volatile uint32_t playedBuffers = 0;
	volatile uint16_t renderPos = 0;
	volatile uint16_t playPos = 0;

	// Buffer properties
	uint16_t audioBufferSize = 0;
	uint16_t framesPerAudioBuffer = 0;
	uint16_t minimumRenderFrames = 0;

	bool open = false;
	volatile bool stopProcessing = true;
};

#endif // C_MT32EMU

#endif
