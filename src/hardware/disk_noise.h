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
	DiskNoiseDevice(const DiskType disk_type,
					const bool enable_disk_noise,
	                const std::string& spin_up_sample_path,
	                const std::string& spin_sample_path,
	                const std::vector<std::string>& seek_sample_paths,
	                bool loop_spin_sample);

	void ActivateSpin();
	void PlaySeek();
	std::vector<float> GetSample();
	void Shutdown();

private:
	bool enable_disk_noise    = false;

	struct {
		std::vector<float> spin_up_sample;
		std::vector<float> sample;
		size_t pos = 0;
		size_t spin_up_pos = 0;
		bool loop = false;
	} spin = {};

	struct {
		std::vector<float> current_sample;
		std::vector<std::vector<float>> samples;
		std::vector<int> sample_weights;
		size_t pos = 0;
	} seek = {};

	void LoadSample(const std::string& path, std::vector<float>& buffer);
	void LoadSeekSamples(const std::vector<std::string>& paths);
	int ChooseWeightedSeekIndex() const;
};

class DiskNoises {
public:
	DiskNoises();
	void Initialize(const bool enable_floppy_disk_noise,
	                const bool enable_hard_disk_noise,
	                const std::string& spin_up, const std::string& spin,
	                const std::vector<std::string>& hdd_seek_samples,
	                const std::string& floppy_spin_up,
	                const std::string& floppy_spin,
	                const std::vector<std::string>& floppy_seek_samples);
	void Shutdown();

	std::shared_ptr<MixerChannel> mix_channel;
	std::vector<DiskNoiseDevice*> active_devices;
	std::mutex device_mutex;
	const unsigned int SampleRate = 22050;

private:
	std::unique_ptr<DiskNoiseDevice> floppy_noise;
	std::unique_ptr<DiskNoiseDevice> hdd_noise;

	void AudioCallback(int frames);
};

#endif // DOSBOX_DISK_NOISE_H
