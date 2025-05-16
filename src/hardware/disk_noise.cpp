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


DiskNoises::DiskNoises() {
	mix_channel = nullptr;
}

static DiskNoises* GetDiskNoises()
{
	if (!disk_noises) {
		disk_noises = std::make_unique<DiskNoises>();
	}
	return disk_noises.get();
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

		const auto channels         = decoder->channels;
		const auto sample_rate      = decoder->sampleRate;
		const auto total_frames     = decoder->totalPCMFrameCount;
		const auto HertzToKilohertz = 1000;

		if (channels != 1) {
			LOG_WARNING("DISKNOISE: FLAC file %s is not mono.",
			            candidate.c_str());
			drflac_close(decoder);
			continue;
		}
		if (sample_rate != SampleRate) {
			LOG_WARNING("DISKNOISE: FLAC file %s should be %dkHz, but %dkHz was found",
			            candidate.c_str(),
			            SampleRate / HertzToKilohertz,
			            sample_rate / HertzToKilohertz);
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
			seek.samples.emplace_back();
			continue;
		}

		std::vector<float> sample;
		LoadSample(path, sample);
		if (!sample.empty()) {
			seek.samples.push_back(std::move(sample));
		} else {
			seek.samples.emplace_back();
		}
	}

	// Assign weights with descending priority (most weight to first samples)
	const int max_weight = 10;
	seek.sample_weights.resize(
	        seek.samples.size());
	for (unsigned int i = 0; i < seek.samples.size(); ++i) {
		seek.sample_weights[i] = static_cast<int>(
		        max_weight - i);
	}
}

int DiskNoiseDevice::ChooseWeightedSeekIndex() const
{
	int total_weight = 0;
	for (unsigned int i = 0; i < seek.samples.size(); ++i) {
		if (!seek.samples[i].empty()) {
			total_weight += seek.sample_weights[i];
		}
	}

	if (total_weight == 0) {
		return 0;
	}

	const int r = static_cast<int>(rand()) % total_weight;
	int sum     = 0;
	for (unsigned int i = 0; i < seek.samples.size(); ++i) {
		if (seek.samples[i].empty()) {
			continue;
		}

		sum += seek.sample_weights[i];
		if (r < sum) {
			return i;
		}
	}
	return 0;
}
DiskNoiseDevice::DiskNoiseDevice(const DiskType disk_type,
								 const bool enable_disk_noise,
                                 const std::string& spin_up_sample_path,
                                 const std::string& spin_sample_path,
                                 const std::vector<std::string>& seek_sample_paths,
                                 bool loop_spin_sample)
        : enable_disk_noise(enable_disk_noise)
{
	if (!enable_disk_noise) {
		LOG_INFO("DISKNOISE: Disk noise emulation disabled");
		return;
	}
	spin.loop = loop_spin_sample;
	LoadSample(spin_up_sample_path, spin.spin_up_sample);
	LoadSample(spin_sample_path, spin.sample);
	LoadSeekSamples(seek_sample_paths);

	std::lock_guard<std::mutex> lock(GetDiskNoises()->device_mutex);

	// Lazily create the static mixer channel if it doesn't exist yet
	if (!GetDiskNoises()->mix_channel) {
		MIXER_LockMixerThread();
		GetDiskNoises()->mix_channel =
		        MIXER_AddChannel(AudioCallback,
		                         SampleRate,
		                         ChannelName::DiskNoise,
		                         {ChannelFeature::Stereo});
		GetDiskNoises()->mix_channel->Enable(true);
		MIXER_UnlockMixerThread();
		float vol_gain = percentage_to_gain(100);
		GetDiskNoises()->mix_channel->SetAppVolume({vol_gain, vol_gain});
	}

	GetDiskNoises()->active_devices.push_back(this);
	DOS_RegisterIoCallback([this]() {
		// This callback is called from the DOS code
		// to trigger the spin and seek sounds
		ActivateSpin();
		PlaySeek();
	}, disk_type);
}

void DiskNoiseDevice::ActivateSpin()
{
	if (!enable_disk_noise) {
		return;
	}

	if (!GetDiskNoises()->mix_channel->is_enabled) {
		GetDiskNoises()->mix_channel->Enable(true);
	}

	// Floppy spin samples can be re-started at any time
	if (!spin.loop) {
		// Check if the sample is still playing and don't interrupt if
		// it does
		if (spin.sample.empty() || spin.pos + 1 < spin.sample.size()) {
			return;
		}
		// Restart spin sample
		spin.pos = 0;
	}
}

void DiskNoiseDevice::PlaySeek()
{
	if (!enable_disk_noise) {
		return;
	}

	const size_t index = ChooseWeightedSeekIndex();
	if (index >= seek.samples.size() ||
	    seek.samples[index].empty()) {
		return;
	}

	// Check if the sample is still playing and don't interrupt if it does
	if (!seek.current_sample.empty() &&
	    seek.pos + 1 < seek.current_sample.size()) {
		return;
	}

	seek.current_sample = seek.samples[index];
	seek.pos            = 0;
}

void DiskNoiseDevice::AudioCallback(const int frames)
{
	// stereo interleaved buffer
	std::vector<float> out(frames * 2, 0.0f);

	std::lock_guard<std::mutex> lock(GetDiskNoises()->device_mutex);

	const float mix_scale = 1.0f /
	                        static_cast<float>(
	                                GetDiskNoises()->active_devices.size());

	auto scale_sample = [](float sample, float volume, float scale) -> float {
		return static_cast<float>(sample) * volume * scale;
	};

	bool some_sample_is_playing = false;
	for (auto* device : GetDiskNoises()->active_devices) {
		bool sample_is_playing = false;
		for (int i = 0; i < frames; ++i) {
			float mixed_l = 0.0f;
			float mixed_r = 0.0f;

			if (!device->spin.spin_up_sample.empty() &&
			    device->spin.spin_up_pos + 1 <
			            device->spin.spin_up_sample.size()) {
				some_sample_is_playing = true;
				mixed_l += scale_sample(
				        device->spin.spin_up_sample
				                [device->spin.spin_up_pos],
				        device->DisknoiseGain,
				        mix_scale);
				mixed_r += scale_sample(
				        device->spin.spin_up_sample
				                [device->spin.spin_up_pos++],
				        device->DisknoiseGain,
				        mix_scale);
			} else if (!device->spin.sample.empty() &&
			           (device->spin.pos + 1 <
			                    device->spin.sample.size() ||
			            device->spin.loop)) {
				some_sample_is_playing = true;
				mixed_l += scale_sample(
				        device->spin.sample[device->spin
				                                                 .pos],
				        device->DisknoiseGain,
				        mix_scale);
				mixed_r += scale_sample(
				        device->spin.sample[device->spin
				                                                 .pos++],
				        device->DisknoiseGain,
				        mix_scale);

				// Loop the spin sound if enabled. Used for
				// persistent HDD noise Not used for floppy
				// noise because motor should stop after r/w
				// operations aredone
				if (device->spin.pos >=
				            device->spin.sample.size() &&
				    device->spin.loop) {
					device->spin.pos = 0;
				}
			}

			if (!device->seek.current_sample.empty() &&
			    device->seek.pos + 1 <
			            device->seek
			                    .current_sample.size()) {
				some_sample_is_playing = true;
				mixed_l += scale_sample(
				        device->seek.current_sample
				                [device->seek.pos],
				        device->DisknoiseGain,
				        mix_scale);
				mixed_r += scale_sample(
				        device->seek.current_sample
				                [device->seek.pos++],
				        device->DisknoiseGain,
				        mix_scale);
			}

			out[i * 2] += mixed_l;
			out[i * 2 + 1] += mixed_r;
		}

		if (!device->seek.current_sample.empty() &&
		    device->seek.pos >=
		            device->seek.current_sample.size()) {
			device->seek.current_sample.clear();
		}
	}

	// Fill the output buffer with silence if no active samples
	if (!some_sample_is_playing) {
		GetDiskNoises()->mix_channel->AddSamples_sfloat(frames, out.data());
		return;
	}

	GetDiskNoises()->mix_channel->AddSamples_sfloat(frames, out.data());
}

void DiskNoiseDevice::Shutdown()
{
	std::lock_guard<std::mutex> lock(GetDiskNoises()->device_mutex);
	GetDiskNoises()->active_devices.erase(
	        std::remove(GetDiskNoises()->active_devices.begin(),
	                    GetDiskNoises()->active_devices.end(),
	                    this),
	        GetDiskNoises()->active_devices.end());

	if (GetDiskNoises()->active_devices.empty() && GetDiskNoises()->mix_channel) {
		GetDiskNoises()->mix_channel->Enable(false);
		MIXER_DeregisterChannel(GetDiskNoises()->mix_channel);
		GetDiskNoises()->mix_channel.reset();
	}
}

std::unique_ptr<DiskNoiseDevice> floppy_noise = nullptr;
std::unique_ptr<DiskNoiseDevice> hdd_noise    = nullptr;

static void disknoise_init(Section* section)
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

	hdd_noise = std::make_unique<DiskNoiseDevice>(DiskType::HardDisk,
	                                              enable_hard_disk_noise,
	                                              spin_up,
	                                              spin,
	                                              hdd_seek_samples,
	                                              true);

	floppy_noise = std::make_unique<DiskNoiseDevice>(DiskType::Floppy,
	                                                 enable_floppy_disk_noise,
	                                                 floppy_spin_up,
	                                                 floppy_spin,
	                                                 floppy_seek_samples,
	                                                 false);
}

static void disknoise_shutdown([[maybe_unused]] Section* section)
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
	                                          &disknoise_init,
	                                          changeable_at_runtime);
	assert(sec);
	init_disknoise_dosbox_settings(*sec);
}
