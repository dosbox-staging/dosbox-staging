// SPDX-FileCopyrightText:  2024-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PRIVATE_SBLASTER_H
#define DOSBOX_PRIVATE_SBLASTER_H

#include "hardware/audio/opl.h"

#include "audio/mixer.h"
#include "hardware/port.h"
#include "utils/rwqueue.h"

class SoundBlaster final {
public:
	SoundBlaster(Section* conf);
	~SoundBlaster();

	void SetChannelRateHz(const int requested_rate_hz);

	// Returns true of the channel was sleeping and was awoken.
	// Returns false if the channel was already awake.
	bool MaybeWakeUp();

	// Public members used by MIXER_PullFromQueueCallback
	RWQueue<AudioFrame> output_queue{1};
	MixerChannelPtr channel        = nullptr;
	std::atomic<int> frames_needed = 0;

private:
	void SetupEnvironment();
	void ClearEnvironment();

	void MixerCallback(const int frames_requested);

	static constexpr auto BlasterEnvVar = "BLASTER";

	IO_ReadHandleObject read_handlers[0x10]   = {};
	IO_WriteHandleObject write_handlers[0x10] = {};

	OplMode oplmode  = OplMode::None;
	bool cms_enabled = false;
};

#endif // DOSBOX_PRIVATE_SBLASTER_H
