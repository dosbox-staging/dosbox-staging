/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2025-2025  The DOSBox Staging Team
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
	                const float& spin_volume, const float& seek_volume);

	void ActivateSpin();
	void PlaySeek();
	void Shutdown();

private:
	const unsigned int SampleRate = 22050;
	bool enable_disk_noise_       = false;

	float spin_volume_ = 0.0;
	float seek_volume_ = 0.0;
	std::vector<float> spin_up_sample_;
	std::vector<float> spin_sample_;
	std::vector<float> current_seek_sample_;
	std::vector<std::vector<float>> seek_samples_;
	std::vector<int> seek_sample_weights_;

	size_t spin_pos_    = 0;
	size_t spin_up_pos_ = 0;
	size_t seek_pos_    = 0;

	static std::shared_ptr<MixerChannel> mix_channel_;
	static std::vector<DiskNoiseDevice*> active_devices_;
	static std::mutex device_mutex_;

	void LoadSample(const std::string& path, std::vector<float>& buffer);
	void LoadSeekSamples(const std::vector<std::string>& paths);
	int ChooseWeightedSeekIndex() const;
	static void AudioCallback(int frames);
};

// Expose the disk noise devices to be able to affect them from hdd/fdd code
extern std::unique_ptr<DiskNoiseDevice> floppy_noise;
extern std::unique_ptr<DiskNoiseDevice> hdd_noise;

#endif // DOSBOX_DISK_NOISE_H
