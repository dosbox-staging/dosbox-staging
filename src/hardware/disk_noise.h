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
	DiskNoiseDevice(const DiskType disk_type, const bool disk_noise_enabled,
	                const std::string& spin_up_sample_path,
	                const std::string& spin_sample_path,
	                const std::vector<std::string>& seek_sample_paths,
	                bool loop_spin_sample);
	~DiskNoiseDevice();
	void ActivateSpin();
	void PlaySeek();
	AudioFrame GetNextFrame();

private:
	bool disk_noise_enabled = false;

	struct SpinSample {
		std::vector<float> spin_up_sample;
		std::vector<float> sample;
		bool loop = false;
		std::vector<float>::iterator spin_up_it = spin_up_sample.end();
		std::vector<float>::iterator spin_it    = sample.end();
	} spin = {};

	struct SeekSample {
		std::vector<std::vector<float>> samples;
		std::vector<int> sample_weights;
		std::vector<float> current_sample;
		std::vector<float>::iterator current_it = current_sample.end();
	} seek = {};

	void LoadSample(const std::string& path, std::vector<float>& destination_buffer);
	void LoadSeekSamples(const std::vector<std::string>& paths);
	size_t ChooseWeightedSeekIndex() const;
};

class DiskNoises {
public:
	DiskNoises(const bool enable_floppy_disk_noise,
	           const bool enable_hard_disk_noise,
	           const std::string& spin_up, const std::string& spin,
	           const std::vector<std::string>& hdd_seek_samples,
	           const std::string& floppy_spin_up, const std::string& floppy_spin,
	           const std::vector<std::string>& floppy_seek_samples);
	~DiskNoises();

	std::shared_ptr<MixerChannel> mix_channel;
	std::vector<std::shared_ptr<DiskNoiseDevice>> active_devices;

	std::mutex device_mutex;

private:
	std::shared_ptr<DiskNoiseDevice> floppy_noise;
	std::shared_ptr<DiskNoiseDevice> hdd_noise;

	void AudioCallback(int frames);
};

#endif // DOSBOX_DISK_NOISE_H
