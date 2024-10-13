/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2021-2024  The DOSBox Staging Team
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
#include <unordered_set>

#include "fs_utils.h"

// Construct a new model and ensure both PCM and control ROM(s) are provided
LASynthModel::LASynthModel(const std::string& rom_name, const Rom* pcm_rom_full,
                           const Rom* pcm_rom_l, const Rom* pcm_rom_h,
                           const Rom* ctrl_rom_full, const Rom* ctrl_rom_1,
                           const Rom* ctrl_rom_2)
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

std::optional<std_fs::path> LASynthModel::FindRom(MT32Emu::Service& service,
                                                  const std_fs::path& dir,
                                                  const Rom* rom) const
{
	static std::unordered_set<std::string> unknown_files;
	if (!rom) {
		return {};
	}

	std::error_code ec;
	for (const auto& entry : std_fs::directory_iterator(dir, ec)) {
		if (ec) {
			continue;
		}

		const std::string filename = std_fs::canonical(entry.path(), ec).string();
		if (ec) {
			continue;
		}
		mt32emu_rom_info info;
		if (service.identifyROMFile(&info, filename.c_str(), nullptr) !=
		    MT32EMU_RC_OK) {

			// Only log unkown files one time (if not already in the
			// unknown_files set).
			if (unknown_files.insert(filename).second) {
				LOG_WARNING("MT32: Unknown file in ROM folder: %s",
				            filename.c_str());
			}
			continue;
		}

		const char* rom_id = nullptr;
		if (rom->type == ROM_TYPE::PCM) {
			rom_id = info.pcm_rom_id;
		} else if (rom->type == ROM_TYPE::CONTROL) {
			rom_id = info.control_rom_id;
		}

		if (rom_id && rom->id == rom_id) {
			return entry.path();
		}
	}
	return {};
}

// Checks if its ROMs can be positively found in the provided directory
bool LASynthModel::InDir(MT32Emu::Service& service, const std_fs::path& dir) const
{
	const bool have_pcm = FindRom(service, dir, pcm_full) ||
	                      (FindRom(service, dir, pcm_l) &&
	                       FindRom(service, dir, pcm_h));

	const bool have_ctrl = FindRom(service, dir, ctrl_full) ||
	                       (FindRom(service, dir, ctrl_a) &&
	                        FindRom(service, dir, ctrl_b));

	return have_pcm && have_ctrl;
}

// If present, loads either the full or partial ROMs from the provided directory
bool LASynthModel::Load(MT32Emu::Service& service, const std_fs::path& dir) const
{
	auto load_rom = [&](const Rom* rom_full,
	                    mt32emu_return_code expected_code) -> bool {
		const auto rom_path = FindRom(service, dir, rom_full);
		if (!rom_path) {
			return false;
		}
		const auto rcode = service.addROMFile(rom_path->string().c_str());
		return (rcode == expected_code);
	};

	auto load_both = [&](const Rom* rom_1,
	                     const Rom* rom_2,
	                     mt32emu_return_code expected_code) -> bool {
		const auto rom_1_path = FindRom(service, dir, rom_1);
		if (!rom_1_path) {
			return false;
		}
		const auto rom_2_path = FindRom(service, dir, rom_2);
		if (!rom_2_path) {
			return false;
		}

		const auto rcode = service.mergeAndAddROMFiles(
		        rom_1_path->string().c_str(), rom_2_path->string().c_str());

		return (rcode == expected_code);
	};

	const bool loaded_pcm = load_rom(pcm_full, MT32EMU_RC_ADDED_PCM_ROM) ||
	                        load_both(pcm_l, pcm_h, MT32EMU_RC_ADDED_PCM_ROM);

	const bool loaded_ctrl = load_rom(ctrl_full, MT32EMU_RC_ADDED_CONTROL_ROM) ||
	                         load_both(ctrl_a, ctrl_b, MT32EMU_RC_ADDED_CONTROL_ROM);

	return loaded_pcm && loaded_ctrl;
}

const char* LASynthModel::GetVersion() const
{
	assert(version_pos != std::string::npos);
	return name.c_str() + version_pos;
}

bool LASynthModel::Matches(const std::string& model_name) const
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
