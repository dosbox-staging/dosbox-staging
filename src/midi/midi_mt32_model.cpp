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

#include "midi_mt32_model.h"

#if C_MT32EMU

#include <cassert>

#include "fs_utils.h"

// Construct a new model and ensure both PCM and control ROM(s) are provided
Model::Model(const std::string &rom_name,
             const Rom *pcm_full,
             const Rom *pcm_a,
             const Rom *pcm_b,
             const Rom *ctrl_full,
             const Rom *ctrl_a,
             const Rom *ctrl_b)
        : name(rom_name),
          pcmFull(pcm_full),
          pcmA(pcm_a),
          pcmB(pcm_b),
          ctrlFull(ctrl_full),
          ctrlA(ctrl_a),
          ctrlB(ctrl_b)
{
	assert(!name.empty());
	assert(pcmFull || (pcmA && pcmB));
	assert(ctrlFull || (ctrlA && ctrlB));
}

// Checks if its ROMs can be positively found in the provided directory
bool Model::InDir(const service_t &service, const std::string &dir)
{
	assert(service);
	if (inDir.find(dir) != inDir.end())
		return inDir[dir];

	auto find_rom = [&service, &dir](const Rom *Rom) -> bool {
		if (!Rom)
			return false;

		const std::string rom_path = dir + Rom->filename;
		if (!path_exists(rom_path))
			return false;

		mt32emu_rom_info info;
		if (service->identifyROMFile(&info, rom_path.c_str(), nullptr) != MT32EMU_RC_OK)
			return false;

		if (Rom->type == ROM_TYPE::UNVERSIONED)
			return true;

		const bool found_pcm = info.pcm_rom_id && (Rom->id == info.pcm_rom_id);
		const bool found_ctrl = info.control_rom_id && (Rom->id == info.control_rom_id);
		return found_pcm || found_ctrl;
	};

	auto find_both = [find_rom](const Rom *rom_a, const Rom *rom_b) -> bool {
		return find_rom(rom_a) && find_rom(rom_b);
	};

	const bool have_pcm = find_rom(pcmFull) || find_both(pcmA, pcmB);
	const bool have_ctrl = find_rom(ctrlFull) || find_both(ctrlA, ctrlB);
	const bool have_both = have_pcm && have_ctrl;
	inDir[dir] = have_both;
	return have_both;
}

// If present, loads either the full or partial ROMs from the provided directory
bool Model::Load(const service_t &service, const std::string &dir)
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
	auto load_both = [&service, &dir](const Rom *rom_a, const Rom *rom_b,
	                                  mt32emu_return_code expected_code) -> bool {
		if (!rom_a || !rom_b)
			return false;
		const std::string rom_a_path = dir + rom_a->filename;
		const std::string rom_b_path = dir + rom_b->filename;
		const auto rcode = service->mergeAndAddROMFiles(rom_a_path.c_str(),
		                                                rom_b_path.c_str());
		return rcode == expected_code;
	};
	const bool loaded_pcm = load_rom(pcmFull, MT32EMU_RC_ADDED_PCM_ROM) ||
	                        load_both(pcmA, pcmB, MT32EMU_RC_ADDED_PCM_ROM);
	const bool loaded_ctrl = load_rom(ctrlFull, MT32EMU_RC_ADDED_CONTROL_ROM) ||
	                         load_both(ctrlA, ctrlB, MT32EMU_RC_ADDED_CONTROL_ROM);
	return loaded_pcm && loaded_ctrl;
}

// Returns the model's version, which is postfixed on it's name. 
// If a version doesn't exist, 
const std::string &Model::Version()
{
	if (!version.empty())
		return version;

	const auto pos = name.find_first_of('_');
	version = (pos == std::string::npos) ? name : name.substr(pos + 1);
	return version;
}
const std::string &Model::Name() const
{
	return name;
}

bool Model::operator<(const Model &other) const
{
	return name < other.Name();
}

bool Model::operator==(const Model &other) const
{
	return name == other.Name();
}

#endif // C_MT32EMU
