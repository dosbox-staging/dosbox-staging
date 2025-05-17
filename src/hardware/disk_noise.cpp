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

void DiskNoises::Initialize(const bool enable_floppy_disk_noise,
                            const bool enable_hard_disk_noise,
                            const std::string& spin_up, const std::string& spin,
                            const std::vector<std::string>& hdd_seek_samples,
                            const std::string& floppy_spin_up,
                            const std::string& floppy_spin,
                            const std::vector<std::string>& floppy_seek_samples)
{

	MIXER_LockMixerThread();
	const auto mixer_callback = std::bind(&DiskNoises::AudioCallback,
	                                      this,
	                                      std::placeholders::_1);
	mix_channel = MIXER_AddChannel(mixer_callback,
	                               SampleRate,
	                               ChannelName::DiskNoise,
	                               {ChannelFeature::Stereo});
	mix_channel->Enable(true);
	MIXER_UnlockMixerThread();
	float vol_gain = percentage_to_gain(100);
	mix_channel->SetAppVolume({vol_gain, vol_gain});

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

void DiskNoises::AudioCallback(const int frames)
{
	// stereo interleaved buffer
	std::vector<AudioFrame> out(frames, {0.0f, 0.0f});

	for (auto* device : disk_noises->active_devices) {
		bool sample_is_playing = false;
		for (int i = 0; i < frames; ++i) {
			float mixed_l = 0.0f;
			float mixed_r = 0.0f;

			std::vector<float> sample = device->GetSample();

			out[i].left += sample[0];
			out[i].right += sample[1];
		}
	}

	disk_noises->mix_channel->AddAudioFrames(out);
}

void DiskNoises::Shutdown()
{
	if (floppy_noise) {
		floppy_noise->Shutdown();
		floppy_noise.reset();
	}

	if (hdd_noise) {
		hdd_noise->Shutdown();
		hdd_noise.reset();
	}
	if (active_devices.empty() && mix_channel) {
		mix_channel->Enable(false);
		MIXER_DeregisterChannel(mix_channel);
		mix_channel.reset();
	}
}

std::vector<float> DiskNoiseDevice::GetSample()
{
	const float DisknoiseGain = 0.2f;
	float sample_l            = 0.0f;
	float sample_r            = 0.0f;

	// Mix in spinup and spin samples
	if (!spin.spin_up_sample.empty() &&
	    spin.spin_up_pos + 1 < spin.spin_up_sample.size()) {
		sample_l += spin.spin_up_sample[spin.spin_up_pos] * DisknoiseGain;
		sample_r += spin.spin_up_sample[spin.spin_up_pos++] * DisknoiseGain;
	} else if (!spin.sample.empty() &&
	           (spin.pos + 1 < spin.sample.size() || spin.loop)) {
		sample_l += spin.sample[spin.pos] * DisknoiseGain;
		sample_r += spin.sample[spin.pos++] * DisknoiseGain;

		// Loop the spin sound if enabled. Used for
		// persistent HDD noise Not used for floppy
		// noise because motor should stop after r/w
		// operations aredone
		if (spin.pos >= spin.sample.size() && spin.loop) {
			spin.pos = 0;
		}
	}

	// Mix in seek sample, if it's playing
	if (!seek.current_sample.empty() &&
	    seek.pos + 1 < seek.current_sample.size()) {
		sample_l += seek.current_sample[seek.pos] * DisknoiseGain;
		sample_r += seek.current_sample[seek.pos++] * DisknoiseGain;
	}

	// If seek sample has finished playing, clear entry
	if (!seek.current_sample.empty() && seek.pos >= seek.current_sample.size()) {
		seek.current_sample.clear();
	}

	return {sample_l, sample_r};
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
		if (sample_rate != disk_noises->SampleRate) {
			LOG_WARNING("DISKNOISE: FLAC file %s should be %dkHz, but %dkHz was found",
			            candidate.c_str(),
			            disk_noises->SampleRate / HertzToKilohertz,
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

	disk_noises->active_devices.push_back(this);
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

void DiskNoiseDevice::Shutdown()
{
	std::lock_guard<std::mutex> lock(disk_noises->device_mutex);
	disk_noises->active_devices.erase(
	        std::remove(disk_noises->active_devices.begin(),
	                    disk_noises->active_devices.end(),
	                    this),
	        disk_noises->active_devices.end());
}

static void disknoise_init(Section* section)
{
	disk_noises = std::make_unique<DiskNoises>();

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
	disk_noises->Initialize(enable_floppy_disk_noise,
	                        enable_hard_disk_noise,
	                        spin_up,
	                        spin,
	                        hdd_seek_samples,
	                        floppy_spin_up,
	                        floppy_spin,
	                        floppy_seek_samples);
}

static void disknoise_shutdown([[maybe_unused]] Section* section)
{
	disk_noises->Shutdown();
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
