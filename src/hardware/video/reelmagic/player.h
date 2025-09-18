// SPDX-FileCopyrightText:  2022-2022 Jon Dennis
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef REELMAGIC_PLAYER_H
#define REELMAGIC_PLAYER_H

#include "audio/mixer.h"
#include "utils/rwqueue.h"

struct ReelMagicAudio {
	MixerChannelPtr channel = nullptr;
	RWQueue<AudioFrame> output_queue{1};
};

extern ReelMagicAudio reel_magic_audio;

#endif
