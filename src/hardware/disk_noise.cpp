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

#include "../libs/decoders/dr_flac.h"
#include "channel_names.h"
#include "checks.h"
#include "mixer.h"
#include "setup.h"
#include "timer.h"

CHECK_NARROWING();

// Static member definitions
std::shared_ptr<MixerChannel> DiskNoiseDevice::mix_channel_ = nullptr;
std::vector<DiskNoiseDevice*> DiskNoiseDevice::active_devices_;
std::mutex DiskNoiseDevice::device_mutex_;

// Custom read procedure for drflac
static size_t drflac_stream_read_proc(void* pUserData, void* pBufferOut,
                                      size_t bytesToRead)
{
	auto* stream = static_cast<std::istream*>(pUserData);
	stream->read(static_cast<char*>(pBufferOut),
	             static_cast<std::streamsize>(bytesToRead));
	return static_cast<size_t>(stream->gcount());
}

// Custom seek procedure for drflac
static drflac_bool32 drflac_stream_seek_proc(void* pUserData, int offset,
                                             drflac_seek_origin origin)
{
	auto* stream               = static_cast<std::istream*>(pUserData);
	std::ios_base::seekdir dir = (origin == drflac_seek_origin_start)
	                                   ? std::ios::beg
	                                   : std::ios::cur;
	stream->clear(); // clear EOF/fail flags
	stream->seekg(offset, dir);
	return stream->good() ? DRFLAC_TRUE : DRFLAC_FALSE;
}

void DiskNoiseDevice::LoadSample(const std::string& path, std::vector<float>& buffer)
{
	if (path.empty()) {
		return;
	}

	constexpr auto SampleExtension = ".flac";

	const auto candidate_paths = {std_fs::path(path),
	                              std_fs::path(path + SampleExtension),
	                              get_resource_path(DiskNoiseDir, path),
	                              get_resource_path(DiskNoiseDir,
	                                                path + SampleExtension)};

	for (const auto& candidate : candidate_paths) {
		if (!std_fs::exists(candidate)) {
			continue;
		}

		// --- Load entire file into memory ---
		std::ifstream file(candidate, std::ios::binary | std::ios::ate);
		if (!file) {
			LOG_WARNING("DISKNOISE: Failed to open file %s",
			            candidate.c_str());
			continue;
		}

		std::streamsize size = file.tellg();
		file.seekg(0, std::ios::beg);

		std::vector<uint8_t> file_data(static_cast<size_t>(size));
		if (!file.read(reinterpret_cast<char*>(file_data.data()), size)) {
			LOG_WARNING("DISKNOISE: Failed to read file %s",
			            candidate.c_str());
			continue;
		}

		// --- Decode FLAC from memory ---
		drflac* decoder = drflac_open_memory(file_data.data(),
		                                     file_data.size(),
		                                     nullptr);
		if (!decoder) {
			LOG_WARNING("DISKNOISE: drflac failed to parse FLAC from %s",
			            candidate.c_str());
			continue;
		}

		const unsigned int channels      = decoder->channels;
		const unsigned int sample_rate   = decoder->sampleRate;
		const drflac_uint64 total_frames = decoder->totalPCMFrameCount;
		const unsigned int hertz_to_kHz  = 1000;

		if (channels != 1) {
			LOG_WARNING("DISKNOISE: FLAC file %s is not mono.",
			            candidate.c_str());
			drflac_close(decoder);
			continue;
		}
		if (sample_rate != SampleRate) {
			LOG_WARNING("DISKNOISE: FLAC file %s should be %dkHz, but %dkHz was found",
			            candidate.c_str(),
			            SampleRate / hertz_to_kHz,
			            sample_rate / hertz_to_kHz);
			drflac_close(decoder);
			continue;
		}

		std::vector<float> temp(static_cast<size_t>(total_frames) * channels);
		drflac_uint64 frames_read = drflac_read_pcm_frames_f32(decoder,
		                                                       total_frames,
		                                                       temp.data());
		drflac_close(decoder);

		if (frames_read == 0) {
			LOG_WARNING("DISKNOISE: Failed to decode frames from %s",
			            candidate.c_str());
			continue;
		}

		// Scale data to integer value range
		const float scale = static_cast<float>(INT16_MAX);
		for (auto& sample : temp) {
			sample *= scale;
		}

		buffer = std::move(temp);
		LOG_DEBUG("DISKNOISE: Loaded %zu float samples from %s",
		          buffer.size(),
		          candidate.c_str());
		return;
	}

	LOG_WARNING("DISKNOISE: Failed to find FLAC sample: %s", path.c_str());
}

void DiskNoiseDevice::LoadSeekSamples(const std::vector<std::string>& paths)
{
	for (const auto& path : paths) {
		if (path.empty()) {
			// Placeholder for missing sample files
			seek_samples_.emplace_back();
			continue;
		}

		std::vector<float> sample;
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
                                 const float& spin_volume,
                                 const float& seek_volume, bool loop_spin_sample)
        : spin_volume_(spin_volume),
          seek_volume_(seek_volume),
          spin_up_sample_(),
          spin_sample_(),
          current_seek_sample_(),
          seek_samples_(),
          seek_sample_weights_()
{
	enable_disk_noise_ = enable_disk_noise;
	loop_spin_sample_  = loop_spin_sample;

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
		                                SampleRate,
		                                ChannelName::DiskNoise,
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

	// Floppy spin samples can be re-started at any time
	if (!loop_spin_sample_) {
		// Check if the sample is still playing and don't interrupt if
		// it does
		if (spin_sample_.empty() || spin_pos_ + 1 < spin_sample_.size()) {
			return;
		}
		// Restart spin sample
		spin_pos_ = 0;
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

	// Find out how many samples are actually playing
	int active_samples = 0;
	for (auto* device : active_devices_) {
		if ((!device->spin_up_sample_.empty() &&
		     device->spin_up_pos_ + 1 < device->spin_up_sample_.size()) ||
		    (!device->spin_sample_.empty() ||
		     device->spin_pos_ + 1 < device->spin_sample_.size())) {
			active_samples++;
		}
		if (!device->current_seek_sample_.empty() &&
		    (device->seek_pos_ + 1 < device->current_seek_sample_.size() ||
		     device->loop_spin_sample_)) {
			active_samples++;
		}
	}

	// Fill the output buffer with silence if no active samples
	if (active_samples == 0) {
		mix_channel_->AddSamples_sfloat(frames, out.data());
		return;
	}

	const float mix_scale = 1.0f / static_cast<float>(active_devices_.size());

	auto scale_sample = [](float sample, float volume, float scale) -> float {
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
				        device->spin_up_sample_[device->spin_up_pos_],
				        device->spin_volume_,
				        mix_scale);
				mixed_r += scale_sample(
				        device->spin_up_sample_[device->spin_up_pos_++],
				        device->spin_volume_,
				        mix_scale);
			} else if (!device->spin_sample_.empty() &&
			           (device->spin_pos_ + 1 <
			                    device->spin_sample_.size() ||
			            device->loop_spin_sample_)) {
				mixed_l += scale_sample(
				        device->spin_sample_[device->spin_pos_],
				        device->spin_volume_,
				        mix_scale);
				mixed_r += scale_sample(
				        device->spin_sample_[device->spin_pos_++],
				        device->spin_volume_,
				        mix_scale);

				// Loop the spin sound if enabled. Used for
				// persistent HDD noise Not used for floppy
				// noise because motor should stop after r/w
				// operations aredone
				if (device->spin_pos_ >= device->spin_sample_.size() &&
				    device->loop_spin_sample_) {
					device->spin_pos_ = 0;
				}
			}

			if (!device->current_seek_sample_.empty() &&
			    device->seek_pos_ + 1 <
			            device->current_seek_sample_.size()) {
				mixed_l += scale_sample(
				        device->current_seek_sample_[device->seek_pos_],
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

	const auto spin_up = "hdd_spinup.flac";
	const auto spin    = "hdd_spin.flac";
	std::vector<std::string> hdd_seek_samples;
	for (int i = 1; i <= 9; ++i) {
		hdd_seek_samples.push_back("hdd_seek" + std::to_string(i) + ".flac");
	}

	const auto floppy_spin_up = "fdd_spinup.flac";
	const auto floppy_spin    = "fdd_spin.flac";
	std::vector<std::string> floppy_seek_samples;
	for (int i = 1; i <= 9; ++i) {
		floppy_seek_samples.push_back("fdd_seek" + std::to_string(i) + ".flac");
	}

	constexpr float hdd_spin_volume = 0.4f;
	constexpr float hdd_seek_volume = 0.2f;

	constexpr float floppy_spin_volume = 0.2f;
	constexpr float floppy_seek_volume = 0.6f;

	hdd_noise = std::make_unique<DiskNoiseDevice>(enable_hard_disk_noise,
	                                              spin_up,
	                                              spin,
	                                              hdd_seek_samples,
	                                              hdd_spin_volume,
	                                              hdd_seek_volume,
	                                              true);

	floppy_noise = std::make_unique<DiskNoiseDevice>(enable_floppy_disk_noise,
	                                                 floppy_spin_up,
	                                                 floppy_spin,
	                                                 floppy_seek_samples,
	                                                 floppy_spin_volume,
	                                                 floppy_seek_volume,
	                                                 false);
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
