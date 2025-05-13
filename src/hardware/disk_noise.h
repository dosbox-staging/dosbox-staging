/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2019-2024  The DOSBox Staging Team
 *  Copyright (C) 2002-2021  The DOSBox Team
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

#ifndef DOSBOX_DISK_NOISE_H
#define DOSBOX_DISK_NOISE_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "mixer.h"

class DiskNoiseDevice {
public:
	DiskNoiseDevice(const bool enable_disk_noise,
	                const std::string& spin_up_sample_path,
	                const std::string& spin_sample_path,
	                const std::vector<std::string>& seek_sample_paths,
	                const std::string& spin_channel_name,
	                const std::string& seek_channel_name,
	                const unsigned int& spin_volume,
	                const unsigned int& seek_volume);

	void ActivateSpin();
	void PlaySeek();
	void Shutdown();

private:
	void LoadSample(const std::string& path, std::vector<int16_t>& buffer);
	void LoadSeekSamples(const std::vector<std::string>& paths);
	size_t ChooseWeightedSeekIndex() const;
	void AudioCallbackSpin(const int len);
	void AudioCallbackSeek(const int len);

	bool enable_disk_noise_;
	std::vector<int16_t> spin_up_sample_;
	size_t spin_up_pos_ = 0;
	std::vector<int16_t> spin_sample_;
	std::vector<int16_t> current_seek_sample_;
	std::vector<size_t> seek_sample_weights_;
	std::vector<std::vector<int16_t>> seek_samples_;
	unsigned int spin_volume_ = 50;
	unsigned int seek_volume_ = 50;
	unsigned int spin_pos_    = 0;
	unsigned int seek_pos_    = 0;
	int last_activity_        = 0;

	std::shared_ptr<MixerChannel> spin_channel_;
	std::shared_ptr<MixerChannel> seek_channel_;

	static constexpr int spinup_fade_ms = 300;
};

// Expose the disk noise devices to be able to affect them from hdd/fdd code
extern std::unique_ptr<DiskNoiseDevice> floppy_noise;
extern std::unique_ptr<DiskNoiseDevice> hdd_noise;

#endif // DOSBOX_DISK_NOISE_H
