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

#include "disk_noise.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "checks.h"
#include "mixer.h"
#include "setup.h"
#include "timer.h"

CHECK_NARROWING();

// Static member definitions
std::shared_ptr<MixerChannel> DiskNoiseDevice::mix_channel_ = nullptr;
std::vector<DiskNoiseDevice*> DiskNoiseDevice::active_devices_;
std::mutex DiskNoiseDevice::device_mutex_;

// Load a disk noise sample. Expects a WAV file with 16-bit PCM data at 44100 Hz.
void DiskNoiseDevice::LoadSample(const std::string& path, std::vector<int16_t>& buffer)
{
	constexpr auto SampleExtension = ".wav";

	if (path.empty()) {
		return;
	}
	// Start with the name as-is and then try from resources
	const auto candidate_paths = {std_fs::path(path),
	                              std_fs::path(path + SampleExtension),
	                              get_resource_path(DiskNoiseDir, path),
	                              get_resource_path(DiskNoiseDir,
	                                                path + SampleExtension)};

	for (const auto& path : candidate_paths) {
		if (std_fs::exists(path) &&
		    (std_fs::is_regular_file(path) || std_fs::is_symlink(path))) {
			std::ifstream file(path, std::ios::binary);
			if (!file.is_open()) {
				LOG_WARNING("DISKNOISE: Failed to open %s",
				            path.c_str());
				return;
			}

			// Skip 44 bytes of the WAV header
			char header[44];
			file.read(header, 44);

			std::vector<int16_t> temp;
			int16_t sample;
			while (file.read(reinterpret_cast<char*>(&sample),
			                 sizeof(sample))) {
				temp.push_back(sample);
			}
			LOG_DEBUG("DISKNOISE: Loaded %zu samples from %s",
			          temp.size(),
			          path.c_str());
			buffer = std::move(temp);
			return;
		}
	}
}

void DiskNoiseDevice::LoadSeekSamples(const std::vector<std::string>& paths)
{
	for (const auto& path : paths) {
		if (path.empty()) {
			// Placeholder for missing sample files
			seek_samples_.emplace_back();
			continue;
		}

		std::vector<int16_t> sample;
		LoadSample(path, sample);
		if (!sample.empty()) {
			seek_samples_.push_back(std::move(sample));
		} else {
			seek_samples_.emplace_back();
		}
	}

	// Assign weights with descending priority (most weight to first samples)
	const int max_weight = 10;
	seek_sample_weights_.resize(seek_samples_.size());
	for (unsigned int i = 0; i < seek_samples_.size(); ++i) {
		seek_sample_weights_[i] = static_cast<int>(max_weight - i);
	}
}

int DiskNoiseDevice::ChooseWeightedSeekIndex() const
{
	int total_weight = 0;
	for (unsigned int i = 0; i < seek_samples_.size(); ++i) {
		if (!seek_samples_[i].empty()) {
			total_weight += seek_sample_weights_[i];
		}
	}

	if (total_weight == 0) {
		return 0;
	}

	const int r = static_cast<int>(rand()) % total_weight;
	int sum     = 0;
	for (unsigned int i = 0; i < seek_samples_.size(); ++i) {
		if (seek_samples_[i].empty()) {
			continue;
		}

		sum += seek_sample_weights_[i];
		if (r < sum) {
			return i;
		}
	}
	return 0;
}
DiskNoiseDevice::DiskNoiseDevice(const bool enable_disk_noise,
                                 const std::string& spin_up_sample_path,
                                 const std::string& spin_sample_path,
                                 const std::vector<std::string>& seek_sample_paths,
                                 const float& spin_volume, const float& seek_volume)
        : spin_volume_(spin_volume),
          seek_volume_(seek_volume),
          spin_up_sample_(),
          spin_sample_(),
          current_seek_sample_(),
          seek_samples_(),
          seek_sample_weights_()
{
	enable_disk_noise_ = enable_disk_noise;
	if (!enable_disk_noise_) {
		LOG_INFO("DISKNOISE: Disk noise emulation disabled");
		return;
	}

	LoadSample(spin_up_sample_path, spin_up_sample_);
	LoadSample(spin_sample_path, spin_sample_);
	LoadSeekSamples(seek_sample_paths);

	std::lock_guard<std::mutex> lock(device_mutex_);

	// Lazily create the static mixer channel if it doesn't exist yet
	if (!mix_channel_) {
		MIXER_LockMixerThread();
		mix_channel_ = MIXER_AddChannel(AudioCallback,
		                                44100,
		                                "DISKNOISE",
		                                {ChannelFeature::Stereo});
		mix_channel_->Enable(true);
		MIXER_UnlockMixerThread();
		float vol_gain = percentage_to_gain(100);
		mix_channel_->SetAppVolume({vol_gain, vol_gain});
	}

	active_devices_.push_back(this);
}

void DiskNoiseDevice::ActivateSpin()
{
	if (!enable_disk_noise_) {
		return;
	}

	if (!mix_channel_->is_enabled) {
		mix_channel_->Enable(true);
	}
}

void DiskNoiseDevice::PlaySeek()
{
	if (!enable_disk_noise_) {
		return;
	}

	const size_t index = ChooseWeightedSeekIndex();
	if (index >= seek_samples_.size() || seek_samples_[index].empty()) {
		return;
	}

	// Check if the sample is still playing and don't interrupt if it does
	if (!current_seek_sample_.empty() &&
	    seek_pos_ + 1 < current_seek_sample_.size()) {
		return;
	}

	current_seek_sample_ = seek_samples_[index];
	seek_pos_            = 0;
}

void DiskNoiseDevice::AudioCallback(const int frames)
{
	// stereo interleaved buffer
	std::vector<float> out(frames * 2, 0.0f);

	std::lock_guard<std::mutex> lock(device_mutex_);
	const size_t num_devices = active_devices_.size();

	// Find out how many samples need to be mixed
	int active_samples = 0;
	for (auto* device : active_devices_) {
		if ((!device->spin_up_sample_.empty() &&
		     device->spin_up_pos_ + 1 < device->spin_up_sample_.size()) ||
		    !device->spin_sample_.empty()) {
			active_samples++;
		}
		if (!device->current_seek_sample_.empty() &&
		    device->seek_pos_ + 1 < device->current_seek_sample_.size()) {
			active_samples++;
		}
	}

	// Fill the output buffer with silence if no active samples
	if (active_samples == 0) {
		mix_channel_->AddSamples_sfloat(frames, out.data());
		return;
	}

	const float mix_scale = 1.0f / static_cast<float>(active_samples);

	auto scale_sample = [](int16_t sample, float volume, float scale) -> float {
		return static_cast<float>(sample) * volume * scale;
	};

	for (auto* device : active_devices_) {
		for (int i = 0; i < frames; ++i) {
			float mixed_l = 0.0f;
			float mixed_r = 0.0f;

			if (!device->spin_up_sample_.empty() &&
			    device->spin_up_pos_ + 1 <
			            device->spin_up_sample_.size()) {
				mixed_l += scale_sample(
				        device->spin_up_sample_[device->spin_up_pos_++],
				        device->spin_volume_,
				        mix_scale);
				mixed_r += scale_sample(
				        device->spin_up_sample_[device->spin_up_pos_++],
				        device->spin_volume_,
				        mix_scale);
			} else if (!device->spin_sample_.empty()) {
				mixed_l += scale_sample(
				        device->spin_sample_[device->spin_pos_++],
				        device->spin_volume_,
				        mix_scale);
				mixed_r += scale_sample(
				        device->spin_sample_[device->spin_pos_++],
				        device->spin_volume_,
				        mix_scale);
				if (device->spin_pos_ >= device->spin_sample_.size()) {
					device->spin_pos_ = 0;
				}
			}

			if (!device->current_seek_sample_.empty() &&
			    device->seek_pos_ + 1 <
			            device->current_seek_sample_.size()) {
				mixed_l += scale_sample(
				        device->current_seek_sample_[device->seek_pos_++],
				        device->seek_volume_,
				        mix_scale);
				mixed_r += scale_sample(
				        device->current_seek_sample_[device->seek_pos_++],
				        device->seek_volume_,
				        mix_scale);
			}

			out[i * 2] += mixed_l;
			out[i * 2 + 1] += mixed_r;
		}

		if (!device->current_seek_sample_.empty() &&
		    device->seek_pos_ >= device->current_seek_sample_.size()) {
			device->current_seek_sample_.clear();
		}
	}

	mix_channel_->AddSamples_sfloat(frames, out.data());
}

void DiskNoiseDevice::Shutdown()
{
	std::lock_guard<std::mutex> lock(device_mutex_);
	active_devices_.erase(std::remove(active_devices_.begin(),
	                                  active_devices_.end(),
	                                  this),
	                      active_devices_.end());

	if (active_devices_.empty() && mix_channel_) {
		mix_channel_->Enable(false);
		MIXER_DeregisterChannel(mix_channel_);
		mix_channel_.reset();
	}
}

std::unique_ptr<DiskNoiseDevice> floppy_noise = nullptr;
std::unique_ptr<DiskNoiseDevice> hdd_noise    = nullptr;

void DISKNOISE_Init(Section* section)
{
	assert(section);
	const auto prop = static_cast<Section_prop*>(section);

	const bool enable_floppy_disk_noise = prop->Get_bool("floppy_disk_noise");
	const bool enable_hard_disk_noise = prop->Get_bool("hard_disk_noise");

	const auto spin_up = "hdd_spinup.wav";
	const auto spin    = "hdd_spin.wav";
	std::vector<std::string> hdd_seek_samples;
	for (int i = 1; i <= 9; ++i) {
		hdd_seek_samples.push_back("hdd_seek" + std::to_string(i) + ".wav");
	}

	const auto floppy_spin_up = "fdd_spinup.wav";
	const auto floppy_spin    = "fdd_spin.wav";
	std::vector<std::string> floppy_seek_samples;
	for (int i = 1; i <= 9; ++i) {
		floppy_seek_samples.push_back("fdd_seek" + std::to_string(i) + ".wav");
	}

	constexpr float hdd_spin_volume = 1.0f; // 0.4f
	constexpr float hdd_seek_volume = 1.0f; // 0.2f

	constexpr float floppy_spin_volume = 0.2f;
	constexpr float floppy_seek_volume = 0.6f;

	hdd_noise = std::make_unique<DiskNoiseDevice>(enable_hard_disk_noise,
	                                              spin_up,
	                                              spin,
	                                              hdd_seek_samples,
	                                              hdd_spin_volume,
	                                              hdd_seek_volume);

	floppy_noise = std::make_unique<DiskNoiseDevice>(enable_floppy_disk_noise,
	                                                 floppy_spin_up,
	                                                 floppy_spin,
	                                                 floppy_seek_samples,
	                                                 floppy_spin_volume,
	                                                 floppy_seek_volume);
}

void DISKNOISE_ShutDown([[maybe_unused]] Section* section)
{
	floppy_noise->Shutdown();
	hdd_noise->Shutdown();
}

void init_disknoise_dosbox_settings(Section_prop& secprop)
{
	constexpr auto only_at_start = Property::Changeable::OnlyAtStart;

	auto* bool_prop = secprop.Add_bool("hard_disk_noise", only_at_start, false);
	bool_prop->Set_help(
	        "Enables emulated noises for hard disks.\n"
	        "This plays disk spinning and seek noises using pre-recorded samples.\n"
	        "This can be combined with the 'hdd_io_speed' setting for an'\n"
	        "authentic experience. This feature is disabled by default.\n");

	bool_prop = secprop.Add_bool("floppy_disk_noise", only_at_start, false);
	bool_prop->Set_help(
	        "Enables emulated noises for floppy disks.\n"
	        "This can be combined with the 'fdd_io_speed' setting for an'\n"
	        "authentic experience. This feature is disabled by default.\n");
}

void DISKNOISE_AddConfigSection(const ConfigPtr& conf)
{
	assert(conf);

	constexpr auto changeable_at_runtime = false;

	Section_prop* sec = conf->AddSection_prop("disknoise",
	                                          &DISKNOISE_Init,
	                                          changeable_at_runtime);
	assert(sec);
	init_disknoise_dosbox_settings(*sec);
}
