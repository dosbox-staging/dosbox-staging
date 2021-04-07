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

#include "midi_lasynth_model.h"

#if C_MT32EMU

#include <cassert>

#include "fs_utils.h"

// Construct a new model and ensure both PCM and control ROM(s) are provided
LASynthModel::LASynthModel(const std::string &rom_name,
                           const Rom *pcm_rom_full,
                           const Rom *pcm_rom_l,
                           const Rom *pcm_rom_h,
                           const Rom *ctrl_rom_full,
                           const Rom *ctrl_rom_1,
                           const Rom *ctrl_rom_2)
        : name(rom_name),
          version_pos(SetVersion()),
          pcm_full(pcm_rom_full),
          pcm_l(pcm_rom_l),
          pcm_h(pcm_rom_h),
          ctrl_full(ctrl_rom_full),
          ctrl_a(ctrl_rom_1),
          ctrl_b(ctrl_rom_2)
{
	assert(!name.empty());
	assert(version_pos != std::string::npos);
	assert(pcm_full || (pcm_l && pcm_h));
	assert(ctrl_full || (ctrl_a && ctrl_b));
}

// Checks if its ROMs can be positively found in the provided directory
bool LASynthModel::InDir(const service_t &service, const std::string &dir) const
{
	assert(service);

	auto find_rom = [&service, &dir](const Rom *rom) -> bool {
		if (!rom)
			return false;

		const std::string rom_path = dir + rom->filename;
		if (!path_exists(rom_path))
			return false;

		mt32emu_rom_info info;
		if (service->identifyROMFile(&info, rom_path.c_str(), nullptr) != MT32EMU_RC_OK)
			return false;

		if (rom->type == ROM_TYPE::UNVERSIONED)
			return true;

		const bool found_pcm = info.pcm_rom_id &&
		                       (rom->id == info.pcm_rom_id);
		const bool found_ctrl = info.control_rom_id &&
		                        (rom->id == info.control_rom_id);
		return found_pcm || found_ctrl;
	};

	const bool have_pcm = find_rom(pcm_full) ||
	                      (find_rom(pcm_l) && find_rom(pcm_h));
	const bool have_ctrl = find_rom(ctrl_full) ||
	                       (find_rom(ctrl_a) && find_rom(ctrl_b));
	return have_pcm && have_ctrl;
}

// If present, loads either the full or partial ROMs from the provided directory
bool LASynthModel::Load(const service_t &service, const std::string &dir) const
{
	if (!service || !InDir(service, dir))
		return false;

	auto load_rom = [&service, &dir](const Rom *rom_full,
	                                 mt32emu_return_code expected_code) -> bool {
		if (!rom_full)
			return false;
		const std::string rom_path = dir + rom_full->filename;
		const auto rcode = service->addROMFile(rom_path.c_str());
		return rcode == expected_code;
	};

	auto load_both = [&service, &dir](const Rom *rom_1, const Rom *rom_2,
	                                  mt32emu_return_code expected_code) -> bool {
		if (!rom_1 || !rom_2)
			return false;
		const std::string rom_1_path = dir + rom_1->filename;
		const std::string rom_2_path = dir + rom_2->filename;
		const auto rcode = service->mergeAndAddROMFiles(rom_1_path.c_str(),
		                                                rom_2_path.c_str());
		return rcode == expected_code;
	};

	const bool loaded_pcm = load_rom(pcm_full, MT32EMU_RC_ADDED_PCM_ROM) ||
	                        load_both(pcm_l, pcm_h, MT32EMU_RC_ADDED_PCM_ROM);
	const bool loaded_ctrl = load_rom(ctrl_full, MT32EMU_RC_ADDED_CONTROL_ROM) ||
	                         load_both(ctrl_a, ctrl_b,
	                                   MT32EMU_RC_ADDED_CONTROL_ROM);
	return loaded_pcm && loaded_ctrl;
}

const char *LASynthModel::GetVersion() const
{
	assert(version_pos != std::string::npos);
	return name.data() + version_pos;
}

bool LASynthModel::Matches(const std::string &model_name) const
{
	assert(!model_name.empty());
	return (name.rfind(model_name, 0) == 0);
}

size_t LASynthModel::SetVersion()
{
	// Given the versioned name "mt32_106", version_pos would be 5
	// Given the unversioned name "cm32l", version_pos would be 0
	// (if the underscore isn't found, find_first_of returns -1)
	const auto pos = name.find_first_of('_') + 1;
	assert(pos < name.size());
	return pos;
}

#endif // C_MT32EMU
