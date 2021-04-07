/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2021-2021  The DOSBox Staging Team
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

#ifndef DOSBOX_MIDI_LA_SYNTH_MODEL_H
#define DOSBOX_MIDI_LA_SYNTH_MODEL_H

#include "dosbox.h"

#if C_MT32EMU

#include <memory>
#include <string>

#define MT32EMU_API_TYPE 3
#include <mt32emu/mt32emu.h>

// An LA Synth Model consists of PCM and Control ROMs either in full or partial
// form.
class LASynthModel {
public:
	enum class ROM_TYPE { UNVERSIONED, VERSIONED };

	struct Rom {
		const std::string id;
		const std::string filename;
		const ROM_TYPE type;
	};

	LASynthModel(const std::string &rom_name,
	             const Rom *pcm_rom_full,
	             const Rom *pcm_rom_l,
	             const Rom *pcm_rom_h,
	             const Rom *ctrl_rom_full,
	             const Rom *ctrl_rom_a,
	             const Rom *ctrl_rom_b);

	LASynthModel() = delete;
	LASynthModel(LASynthModel &) = delete;
	LASynthModel &operator=(LASynthModel &) = delete;

	const char *GetName() const { return name.c_str(); }

	// The version may be postfixed onto the model's name using an
	// underscore. If the model is unversioned, then the name is returned.
	// The name "mt32_107" returns version "107".
	// The name "mt32_bluer" returns version "bluer".
	// The name "mt32" doesn't have a version, so "mt32" is returned.
	const char *GetVersion() const;

	// Returns true if the model has a matches that provided "mt32" or "cm32l".
	bool Matches(const std::string &model_name) const;

	using service_t = std::unique_ptr<MT32Emu::Service>;
	bool InDir(const service_t &service, const std::string &dir) const;
	bool Load(const service_t &service, const std::string &dir) const;

private:
	size_t SetVersion();

	const std::string name = {};
	const size_t version_pos = std::string::npos;

	// PCM ROMs. Partials are in low-high form
	const Rom *pcm_full = nullptr;
	const Rom *pcm_l = nullptr;
	const Rom *pcm_h = nullptr;

	// Control ROMs. Partials are in a-b form
	const Rom *ctrl_full = nullptr;
	const Rom *ctrl_a = nullptr;
	const Rom *ctrl_b = nullptr;
};

#endif // C_MT32EMU

#endif
