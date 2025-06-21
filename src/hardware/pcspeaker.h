// SPDX-FileCopyrightText:  2022-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PCSPEAKER_H
#define DOSBOX_PCSPEAKER_H

#include "dosbox.h"

#include <string_view>

#include "mixer.h"
#include "rwqueue.h"
#include "timer.h"

class PcSpeaker {
public:
	RWQueue<float> output_queue{1};
	MixerChannelPtr channel = nullptr;
	float frame_counter = 0.0f;

	virtual ~PcSpeaker() = default;

	virtual void SetFilterState(const FilterState filter_state) = 0;
	virtual bool TryParseAndSetCustomFilter(const std::string& filter_choice) = 0;
	virtual void SetCounter(const int cntr, const PitMode pit_mode) = 0;
	virtual void SetPITControl(const PitMode pit_mode)              = 0;
	virtual void SetType(const PpiPortB &port_b)                    = 0;
	virtual void PicCallback(const int requested_frames)            = 0;
};

#endif
