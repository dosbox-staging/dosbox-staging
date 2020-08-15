/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2002-2011  The DOSBox Team
 *  Copyright (C) 2020       Nikos Chantziaras <realnc@gmail.com>
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

#ifndef DOSBOX_MIDI_FLUIDSYNTH_H
#define DOSBOX_MIDI_FLUIDSYNTH_H

#include "midi_handler.h"

#if C_FLUIDSYNTH

#include <memory>
#include <fluidsynth.h>

#include "mixer.h"

class MidiHandlerFluidsynth final : public MidiHandler {
private:
	using fsynth_ptr_t = std::unique_ptr<fluid_synth_t, decltype(&delete_fluid_synth)>;

	using fluid_settings_ptr_t =
	        std::unique_ptr<fluid_settings_t, decltype(&delete_fluid_settings)>;

	using mixer_channel_ptr_t =
	        std::unique_ptr<MixerChannel, decltype(&MIXER_DelChannel)>;

public:
	const char *GetName() const override { return "fluidsynth"; }
	bool Open(const char *conf) override;
	void Close() override;
	void PlayMsg(const uint8_t *msg) override;
	void PlaySysex(uint8_t *sysex, size_t len) override;

private:
	MidiHandlerFluidsynth() = default;

	static MidiHandlerFluidsynth instance;

	fluid_settings_ptr_t settings{nullptr, &delete_fluid_settings};
	fsynth_ptr_t synth{nullptr, &delete_fluid_synth};
	mixer_channel_ptr_t channel{nullptr, MIXER_DelChannel};
	bool is_open = false;

	static void mixer_callback(uint16_t len); // see: MIXER_Handler
};

#endif // C_FLUIDSYNTH

#endif
