// SPDX-FileCopyrightText:  2021-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PRIVATE_INNOVATION_H
#define DOSBOX_PRIVATE_INNOVATION_H

#include "dosbox.h"

#include <memory>
#include <queue>
#include <string>

#include "residfp/SID.h"

#include "audio/mixer.h"
#include "hardware/port.h"

class Innovation {
public:
	Innovation(const int sid_filter_strength,
	           const std::string& channel_filter_choice);

	~Innovation();

private:
	void AudioCallback(const int requested_frames);

	uint8_t ReadFromPort(io_port_t port, io_width_t width);
	void WriteToPort(io_port_t port, io_val_t value, io_width_t width);

	bool MaybeRenderFrame(float& frame);
	void RenderUpToNow();

	int16_t TallySilence(const int16_t sample);

	// Managed objects
	MixerChannelPtr channel               = nullptr;

	IO_ReadHandleObject read_handler      = {};
	IO_ReadHandleObject read_entertainer_id_handler = {};
	IO_WriteHandleObject write_handler    = {};

	std::unique_ptr<reSIDfp::SID> service = {};
	std::queue<float> fifo                = {};
	std::mutex mutex                      = {};

	// Initial configuration
	const double ms_per_clock    = 0.0;
	int idle_after_silent_frames = 0;

	// Runtime states
	double last_rendered_ms = 0.0;
	bool is_open            = false;
};

#endif // DOSBOX_PRIVATE_INNOVATION_H
