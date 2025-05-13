/*
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

#include "disk_noise.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "mixer.h"
#include "setup.h"
#include "timer.h"

// ******************************************************
// Simulated disk noise for floppy and hard disk drives
// ******************************************************

// Load a disk noise sample. Expects a WAV file with 16-bit PCM data at 44100 Hz.
void DiskNoiseDevice::LoadSample(const std::string& path, std::vector<int16_t>& buffer)
{
	constexpr auto sample_ext = ".wav";

	if (path.empty()) {
		return;
	}
	// Start with the name as-is and then try from resources
	const auto candidate_paths = {std_fs::path(path),
	                              std_fs::path(path + sample_ext),
	                              get_resource_path(DiskNoiseDir, path),
	                              get_resource_path(DiskNoiseDir,
	                                                path + sample_ext)};

	for (const auto& path : candidate_paths) {
		if (std_fs::exists(path) &&
		    (std_fs::is_regular_file(path) || std_fs::is_symlink(path))) {
			std::ifstream file(path, std::ios::binary);
			if (!file.is_open()) {
				LOG_WARNING("DiskNoiseDevice: Failed to open %s",
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
			LOG_DEBUG("DiskNoiseDevice: Loaded %zu samples from %s",
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
			seek_samples_.emplace_back(); // Placeholder for missing
			                              // sample
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
	const size_t max_weight = 10;
	seek_sample_weights_.resize(seek_samples_.size());
	for (size_t i = 0; i < seek_samples_.size(); ++i) {
		seek_sample_weights_[i] = static_cast<size_t>(max_weight - i);
	}
}

size_t DiskNoiseDevice::ChooseWeightedSeekIndex() const
{
	size_t total_weight = 0;
	for (size_t i = 0; i < seek_samples_.size(); ++i) {
		if (!seek_samples_[i].empty()) {
			total_weight += seek_sample_weights_[i];
		}
	}

	if (total_weight == 0) {
		return 0;
	}

	const size_t r = static_cast<size_t>(rand()) % total_weight;
	size_t sum     = 0;
	for (size_t i = 0; i < seek_samples_.size(); ++i) {
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
                                 const std::string& spin_channel_name,
                                 const std::string& seek_channel_name,
                                 const unsigned int& spin_volume,
                                 const unsigned int& seek_volume)
        : enable_disk_noise_(),
          spin_up_sample_(),
          spin_sample_(),
          current_seek_sample_(),
          seek_sample_weights_(),
          seek_samples_(),
          spin_pos_(0),
          seek_pos_(0),
          last_activity_(0),
          spin_channel_(nullptr),
          seek_channel_(nullptr)
{
	enable_disk_noise_ = enable_disk_noise;
	if (!enable_disk_noise_) {
		LOG_INFO("DiskNoiseDevice: Disk noise emulation disabled");
		return;
	}

	LOG_DEBUG("DiskNoiseDevice: Spin sample volume: %u", spin_volume);
	LOG_DEBUG("DiskNoiseDevice: Seek sample volume: %u", seek_volume);

	spin_volume_ = spin_volume;
	seek_volume_ = seek_volume;
	LoadSample(spin_up_sample_path, spin_up_sample_);
	LoadSample(spin_sample_path, spin_sample_);
	LoadSeekSamples(seek_sample_paths);

	using namespace std::placeholders;

	MIXER_LockMixerThread();

	auto spin_callback = std::bind(&DiskNoiseDevice::AudioCallbackSpin,
	                               this,
	                               std::placeholders::_1);

	auto seek_callback = std::bind(&DiskNoiseDevice::AudioCallbackSeek,
	                               this,
	                               std::placeholders::_1);

	spin_channel_ = MIXER_AddChannel(spin_callback,
	                                 44100,
	                                 spin_channel_name.c_str(),
	                                 {ChannelFeature::Stereo});
	seek_channel_ = MIXER_AddChannel(seek_callback,
	                                 44100,
	                                 seek_channel_name.c_str(),
	                                 {ChannelFeature::Stereo});

	MIXER_UnlockMixerThread();

	float vol_gain = percentage_to_gain(spin_volume_);
	spin_channel_->SetAppVolume({vol_gain, vol_gain});
	vol_gain = percentage_to_gain(seek_volume_);
	seek_channel_->SetAppVolume({vol_gain, vol_gain});

	spin_channel_->Enable(false);
	seek_channel_->Enable(false);

	// Automatically start spinning for HDDs
	if (spin_channel_name == "HDD_SPIN") {
		ActivateSpin();
	}
}

void DiskNoiseDevice::ActivateSpin()
{
	if (!enable_disk_noise_ || spin_channel_->is_enabled) {
		return;
	}
	LOG_DEBUG("DiskNoiseDevice: Activating spin");

	last_activity_ = GetTicks();
	spin_channel_->Enable(true);
}

void DiskNoiseDevice::PlaySeek()
{
	if (!enable_disk_noise_ || seek_channel_->is_enabled) {
		return;
	}

	const size_t index = ChooseWeightedSeekIndex();
	if (index >= seek_samples_.size() || seek_samples_[index].empty()) {
		return;
	}

	current_seek_sample_ = seek_samples_[index];
	seek_pos_            = 0;
	seek_channel_->Enable(true);
}

void DiskNoiseDevice::AudioCallbackSpin(const int frames)
{
	std::vector<int16_t> out(frames * 2);

	for (int i = 0; i < frames; ++i) {
		int16_t sample_r = 0;
		int16_t sample_l = 0;

		if (!spin_up_sample_.empty() &&
		    spin_up_pos_ < spin_up_sample_.size()) {
			// Still playing the spin-up sample
			sample_r = spin_up_sample_[spin_up_pos_++];
			sample_l = spin_up_sample_[spin_up_pos_++];
		} else if (!spin_sample_.empty()) {
			// Spin-up complete, loop the main spin sample
			sample_r = spin_sample_[spin_pos_++];
			sample_l = spin_sample_[spin_pos_++];
			if (spin_pos_ >= spin_sample_.size()) {
				spin_pos_ = 0;
			}
		}

		// Write to both left and right channels
		out[i * 2]     = sample_r;
		out[i * 2 + 1] = sample_l;
	}

	spin_channel_->AddSamples_s16(frames, out.data());
}

void DiskNoiseDevice::AudioCallbackSeek(const int frames)
{
	std::vector<int16_t> out(frames * 2);

	for (int i = 0; i < frames * 2; ++i) {
		if (!current_seek_sample_.empty() &&
		    seek_pos_ < current_seek_sample_.size()) {
			out[i] = (current_seek_sample_)[seek_pos_++];
		} else {
			out[i] = 0;
		}
	}

	if (current_seek_sample_.empty() ||
	    seek_pos_ >= current_seek_sample_.size()) {
		seek_channel_->Enable(false);
	}
	seek_channel_->AddSamples_s16(frames, out.data());
}

void DiskNoiseDevice::Shutdown()
{
	LOG_INFO("DiskNoiseDevice: Shutting down");
	spin_channel_->Enable(false);
	seek_channel_->Enable(false);
	spin_channel_.reset();
	seek_channel_.reset();
	MIXER_DeregisterChannel(spin_channel_);
	MIXER_DeregisterChannel(seek_channel_);
}

std::unique_ptr<DiskNoiseDevice> floppy_noise = nullptr;
std::unique_ptr<DiskNoiseDevice> hdd_noise    = nullptr;

void DISKNOISE_Init(Section* section)
{
	assert(section);
	const auto prop = static_cast<Section_prop*>(section);

	const bool enable_disk_noise = prop->Get_bool("enable_disk_noise_emulation");
	const std::string floppy_spin_up = prop->Get_string("floppy_spin_up_sample");
	const std::string floppy_spin = prop->Get_string("floppy_spin_sample");
	const unsigned int floppy_spin_volume = prop->Get_int("floppy_spin_volume");
	const unsigned int floppy_seek_volume = prop->Get_int("floppy_seek_volume");
	const std::string hdd_spin_up = prop->Get_string("hdd_spin_up_sample");
	const std::string hdd_spin    = prop->Get_string("hdd_spin_sample");
	const unsigned int hdd_spin_volume = prop->Get_int("hdd_spin_volume");
	const unsigned int hdd_seek_volume = prop->Get_int("hdd_seek_volume");

	std::vector<std::string> floppy_seek_paths;
	std::vector<std::string> hdd_seek_paths;

	for (int i = 1; i <= 9; ++i) {
		floppy_seek_paths.push_back(prop->Get_string(
		        "floppy_seek_sample_" + std::to_string(i)));
		hdd_seek_paths.push_back(
		        prop->Get_string("hdd_seek_sample_" + std::to_string(i)));
	}

	if (!floppy_spin.empty() ||
	    !std::all_of(floppy_seek_paths.begin(),
	                 floppy_seek_paths.end(),
	                 [](const auto& s) { return s.empty(); })) {
		floppy_noise = std::make_unique<DiskNoiseDevice>(enable_disk_noise,
		                                                 floppy_spin_up,
		                                                 floppy_spin,
		                                                 floppy_seek_paths,
		                                                 "FLOPPY_SPIN",
		                                                 "FLOPPY_SEEK",
		                                                 floppy_spin_volume,
		                                                 floppy_seek_volume);
	}

	if (!hdd_spin.empty() ||
	    !std::all_of(hdd_seek_paths.begin(),
	                 hdd_seek_paths.end(),
	                 [](const auto& s) { return s.empty(); })) {
		hdd_noise = std::make_unique<DiskNoiseDevice>(enable_disk_noise,
		                                              hdd_spin_up,
		                                              hdd_spin,
		                                              hdd_seek_paths,
		                                              "HDD_SPIN",
		                                              "HDD_SEEK",
		                                              hdd_spin_volume,
		                                              hdd_seek_volume);
	}
}

void DISKNOISE_ShutDown([[maybe_unused]] Section* section)
{
	floppy_noise->Shutdown();
	hdd_noise->Shutdown();
}

void init_disknoise_dosbox_settings(Section_prop& secprop)
{
	constexpr auto only_at_start = Property::Changeable::OnlyAtStart;

	auto* bool_prop = secprop.Add_bool("enable_disk_noise_emulation",
	                                   only_at_start,
	                                   false);
	bool_prop->Set_help(
	        "Enables emulated disk noises for hard disks and floppy drives.\n"
	        "This plays disk spinning and seek noises using configurable samples.\n"
	        "Up to 9 seek samples are supported per disk type. Sample 1 has\n"
	        "the highest chance of being played, and sample 9 has the lowest chance.\n"
	        "Sample should be in 16-bit PCM WAV format at 44100 Hz, and can be\n"
	        "located in the dosbox resources directory or beconfigured with an\n"
	        "absolute path.\n"
	        "This can be combined with the 'hdd_io_speed' and 'fdd_io_speed settings'\n"
	        "for an authentic experience. This feature is disabled by default.\n");

	auto* str_prop = secprop.Add_string("floppy_spin_up_sample",
	                                    only_at_start,
	                                    "fdd_spinup.wav");
	assert(str_prop);
	str_prop->Set_help(
	        "Path to a .wav file for the floppy spin-up noise.\n"
	        "Leave empty to not play this noise.\n");

	str_prop = secprop.Add_string("floppy_spin_sample", only_at_start, "fdd_spin.wav");
	assert(str_prop);
	str_prop->Set_help(
	        "Path to a .wav file for the floppy spin noise emulation.\n"
	        "Leave empty to not play this noise.\n");

	auto* int_prop = secprop.Add_int("floppy_spin_volume", only_at_start, 20);
	int_prop->SetMinMax(0, 100);
	int_prop->Set_help(
	        "Set the volume of the floppy disk spin sound between 0 to 100 (Default: 20).\n");

	int_prop = secprop.Add_int("floppy_seek_volume", only_at_start, 60);
	int_prop->SetMinMax(0, 100);
	int_prop->Set_help(
	        "Set the volume of the floppy disk seek sound between 0 to 100 (Default: 60).\n");

	str_prop = secprop.Add_string("hdd_spin_up_sample",
	                              only_at_start,
	                              "hdd_spinup.wav");
	assert(str_prop);
	str_prop->Set_help(
	        "Path to a .wav file for the hard disk spin-up noise.\n"
	        "Leave empty to not play this noise.\n");

	str_prop = secprop.Add_string("hdd_spin_sample", only_at_start, "hdd_spin.wav");
	assert(str_prop);
	str_prop->Set_help(
	        "Path to a .wav file for the hard disk spin noise emulation.\n"
	        "Leave empty to not play this noise.\n");

	int_prop = secprop.Add_int("hdd_spin_volume", only_at_start, 40);
	int_prop->SetMinMax(0, 100);
	int_prop->Set_help(
	        "Set the volume of hard disk spin sound between 0 to 100 (Default: 10).\n");

	int_prop = secprop.Add_int("hdd_seek_volume", only_at_start, 20);
	int_prop->SetMinMax(0, 100);
	int_prop->Set_help(
	        "Set the volume of the hard disk seek sound between 0 to 100 (Default: 40).\n");

	for (int i = 1; i <= 9; ++i) {
		const std::string idx = std::to_string(i);

		auto* fseek_prop =
		        secprop.Add_string("floppy_seek_sample_" + idx,
		                           Property::Changeable::OnlyAtStart,
		                           ("fdd_seek" + idx + ".wav").c_str());
		assert(fseek_prop);
		fseek_prop->Set_help("Path to floppy seek sample " + idx + ".");

		auto* hseek_prop =
		        secprop.Add_string("hdd_seek_sample_" + idx,
		                           Property::Changeable::OnlyAtStart,
		                           ("hdd_seek" + idx + ".wav").c_str());
		assert(hseek_prop);
		hseek_prop->Set_help("Path to hard disk seek sample " + idx + ".");
	}
}

void DISKNOISE_AddConfigSection(const ConfigPtr& conf)
{
	assert(conf);

	constexpr auto changeable_at_runtime = false; // TODO: Deal with runtime
	                                              // changes

	Section_prop* sec = conf->AddSection_prop("disknoise",
	                                          &DISKNOISE_Init,
	                                          changeable_at_runtime);
	assert(sec);
	init_disknoise_dosbox_settings(*sec);
}
