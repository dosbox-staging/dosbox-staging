/*
 *  Copyright (C) 2022 Jon Dennis
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef REELMAGIC_PLAYER_H
#define REELMAGIC_PLAYER_H

#include "audio/mixer.h"
#include "util/rwqueue.h"

struct ReelMagicAudio {
	MixerChannelPtr channel = nullptr;
	RWQueue<AudioFrame> output_queue{1};
};

extern ReelMagicAudio reel_magic_audio;

#endif
