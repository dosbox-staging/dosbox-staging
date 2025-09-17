// SPDX-FileCopyrightText:  2021-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "private/mt32_lasynth_model.h"

#if C_MT32EMU

#include <cassert>
#include <map>
#include <set>

#include "utils/fs_utils.h"

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

// Scans the requested directory for the requested ROM.
// Uses a static cache of directories and their ROMs to avoid repeat scans.
std::optional<std_fs::path> LASynthModel::FindRom(MT32Emu::Service& service,
                                                  const std_fs::path& dir,
                                                  const Rom* rom) const
{
	if (!rom) {
		return std::nullopt;
	}

	// Named types to make the directory cache self-documenting
	using RomId     = std::string;
	using RomsCache = std::map<RomId, std_fs::path>;
	using DirsCache = std::map<std_fs::path, RomsCache>;

	static DirsCache dirs_cache                 = {};
	static std::set<std_fs::path> unknown_files = {};

	auto find_rom_in_cache = [&](const RomsCache& roms_cache) {
		auto it = roms_cache.find(rom->id);
		return it != roms_cache.end() ? std::optional{it->second}
		                              : std::nullopt;
	};

	// Is the requested directory already cached?
	if (auto it = dirs_cache.find(dir); it != dirs_cache.end()) {
		return find_rom_in_cache(it->second);
	}

	// Cache miss: start a new ROM cache for the directory
	auto& roms_cache = dirs_cache[dir];

	// Scan the directory
	std::error_code ec = {};
	for (const auto& entry : std_fs::directory_iterator(dir, ec)) {
		if (ec) {
			continue;
		}

		auto canonical_path = std_fs::canonical(entry.path(), ec);
		if (ec || unknown_files.contains(canonical_path)) {
			continue;
		}

		// Is the file a valid MT32emu ROM?
		mt32emu_rom_info info = {};
		if (service.identifyROMFile(&info,
		                            canonical_path.string().c_str(),
		                            nullptr) != MT32EMU_RC_OK) {

			LOG_WARNING("MT32: Unknown file in ROM folder: %s",
			            canonical_path.string().c_str());

			unknown_files.emplace(std::move(canonical_path));
			continue;
		}

		// Add the ROM identifier and path to the cache
		auto rom_id = info.pcm_rom_id ? info.pcm_rom_id : info.control_rom_id;
		roms_cache.emplace(std::move(rom_id), std::move(canonical_path));
	}

	return find_rom_in_cache(roms_cache);
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
