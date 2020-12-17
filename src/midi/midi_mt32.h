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

#define MT32EMU_API_TYPE 3
#include <mt32emu/mt32emu.h>
#if MT32EMU_VERSION_MAJOR != 2 || MT32EMU_VERSION_MINOR < 1
#error Incompatible mt32emu library version
#endif

#include "mixer.h"

struct SDL_Thread;

class MidiHandler_mt32 final : public MidiHandler {
public:
	~MidiHandler_mt32();

	const char *GetName() const override { return "mt32"; }
	bool Open(const char *conf) override;
	void Close() override;
	void PlayMsg(const uint8_t *msg) override;
	void PlaySysex(uint8_t *sysex, size_t len) override;
	bool noise = false; // extra logging verbosity (to-be-renamed)

private:
	MixerChannel *chan = nullptr;
	MT32Emu::Service *service = nullptr;
	SDL_Thread *thread = nullptr;
	SDL_mutex *lock = nullptr;
	SDL_cond *framesInBufferChanged = nullptr;
	int16_t *audioBuffer = nullptr;
	uint16_t audioBufferSize = 0;
	uint16_t framesPerAudioBuffer = 0;
	uint16_t minimumRenderFrames = 0;
	volatile uint16_t renderPos = 0;
	volatile uint16_t playPos = 0;
	volatile uint32_t playedBuffers = 0;
	volatile bool stopProcessing = true;
	bool open = false;
	bool renderInThread = false;
	void MixerCallBack(uint16_t len);
	static int processingThread(void *);
	uint32_t GetMidiEventTimestamp();
	void renderingLoop();
};

#endif // C_MT32EMU

#endif
