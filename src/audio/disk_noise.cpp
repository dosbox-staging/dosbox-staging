// SPDX-FileCopyrightText:  2025-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "disk_noise.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "channel_names.h"
#include "config/setup.h"
#include "decoders/dr_flac.h"
#include "hardware/timer.h"
#include "mixer.h"
#include "utils/checks.h"

CHECK_NARROWING();

static std::unique_ptr<DiskNoises> disk_noises    = nullptr;
static const unsigned int DiskNoiseSampleRateInHz = 22050;

DiskNoises::DiskNoises(const DiskNoiseMode floppy_disk_noise_mode,
                       const DiskNoiseMode hard_disk_noise_mode,
                       const std::string& spin_up, const std::string& spin,
                       const std::vector<std::string>& hdd_seek_samples,
                       const std::string& floppy_spin_up,
                       const std::string& floppy_spin,
                       const std::vector<std::string>& floppy_seek_samples)
{
	if (floppy_disk_noise_mode == DiskNoiseMode::Off &&
	    hard_disk_noise_mode == DiskNoiseMode::Off) {
		return;
	}

	// Load samples
	hdd_noise = std::make_shared<DiskNoiseDevice>(DiskType::HardDisk,
	                                              hard_disk_noise_mode,
	                                              spin_up,
	                                              spin,
	                                              hdd_seek_samples);
	active_devices.emplace_back(hdd_noise);

	floppy_noise = std::make_shared<DiskNoiseDevice>(DiskType::Floppy,
	                                                 floppy_disk_noise_mode,
	                                                 floppy_spin_up,
	                                                 floppy_spin,
	                                                 floppy_seek_samples);
	active_devices.emplace_back(floppy_noise);

	// Start audio thread
	MIXER_LockMixerThread();

	const auto mixer_callback = std::bind(&DiskNoises::AudioCallback,
	                                      this,
	                                      std::placeholders::_1);

	mix_channel = MIXER_AddChannel(mixer_callback,
	                               DiskNoiseSampleRateInHz,
	                               ChannelName::DiskNoise,
	                               {});
	mix_channel->Enable(true);

	MIXER_UnlockMixerThread();
}

DiskNoises* DiskNoises::GetInstance()
{
	if (disk_noises != nullptr) {
		return disk_noises.get();
	}
	return nullptr;
}

void DiskNoises::AudioCallback(const int num_frames_requested)
{
	// Check if callback runs before mix_channel is assigned.
	assert(mix_channel != nullptr);

	// stereo interleaved buffer
	static std::vector<AudioFrame> out = {};
	out.clear();

	// Mix audio frames from all active devices
	for (auto i = 0; i < num_frames_requested; ++i) {
		AudioFrame mixed_sample = {};
		for (const auto& device : active_devices) {
			auto sample = device->GetNextFrame();
			mixed_sample += sample;
		}
		out.emplace_back(mixed_sample);
	}

	mix_channel->AddAudioFrames(out);
}

DiskNoises::~DiskNoises()
{
	// Stop audio thread and unregister channel
	if (mix_channel) {
		mix_channel->Enable(false);
		MIXER_DeregisterChannel(mix_channel);
		mix_channel.reset();
	}

	// Destroy devices and associated samples
	floppy_noise.reset();
	hdd_noise.reset();
	active_devices.clear();
}

AudioFrame DiskNoiseDevice::GetNextFrame()
{
	std::lock_guard<std::mutex> lock(mutex);
	constexpr float DiskNoiseGain = 0.2f;
	float sample                  = 0.0f;

	// Mix in spinup and spin samples
	if (!spin.spin_up_sample.empty()) {
		if (spin.spin_up_it != spin.spin_up_sample.end()) {
			sample += (*spin.spin_up_it) * DiskNoiseGain;
			++spin.spin_up_it;
		} else {
			// If spinup sample has finished playing, clear entry
			spin.spin_up_sample.clear();
			spin.spin_up_it = spin.spin_up_sample.end();
		}
	} else if (!spin.sample.empty() &&
	           (spin.spin_it != spin.sample.end() || spin.loop)) {
		// Loop the spin sound if enabled. Used for persistent HDD
		// noise. Not used for floppy noise because motor should stop
		// after read-write operations are done.
		if (spin.spin_it == spin.sample.end() && spin.loop) {
			spin.spin_it = spin.sample.begin();
		}
		if (spin.spin_it != spin.sample.end()) {
			sample += (*spin.spin_it) * DiskNoiseGain;
			++spin.spin_it;
		}
	}

	// Mix in seek sample, if it's playing
	if (!seek.current_sample.empty() &&
	    seek.current_it != seek.current_sample.end()) {
		sample += (*seek.current_it) * DiskNoiseGain;
		++seek.current_it;
	}

	// If seek sample has finished playing, clear entry
	if (!seek.current_sample.empty() &&
	    seek.current_it == seek.current_sample.end()) {
		seek.current_sample.clear();
		seek.current_it = seek.current_sample.end();
	}

	return AudioFrame{sample};
}

void DiskNoiseDevice::LoadSample(const std::string& path,
                                 std::vector<float>& destination_buffer)
{
	if (path.empty()) {
		return;
	}

	constexpr auto SampleExtension = ".flac";

	const auto candidate_paths = {std_fs::path(path),
	                              std_fs::path(path + SampleExtension),
	                              get_resource_path(DiskNoisesDir, path),
	                              get_resource_path(DiskNoisesDir,
	                                                path + SampleExtension)};

	for (const auto& candidate : candidate_paths) {
		if (!std_fs::exists(candidate)) {
			continue;
		}

		// Load entire file into memory
		std::ifstream file(candidate, std::ios::binary | std::ios::ate);
		if (!file) {
			LOG_ERR("DISKNOISE: Failed to open file '%s'",
			        candidate.string().c_str());
			continue;
		}

		std::streamsize size = file.tellg();
		file.seekg(0, std::ios::beg);

		std::vector<uint8_t> file_data = {};
		file_data.resize(static_cast<size_t>(size));
		if (!file.read(reinterpret_cast<char*>(file_data.data()), size)) {
			LOG_ERR("DISKNOISE: Failed to read file '%s'",
			        candidate.string().c_str());
			continue;
		}

		// Decode FLAC from memory
		drflac* decoder = drflac_open_memory(file_data.data(),
		                                     file_data.size(),
		                                     nullptr);
		if (!decoder) {
			LOG_ERR("DISKNOISE: Failed to parse FLAC file '%s'",
			        candidate.string().c_str());
			continue;
		}

		const auto channels         = decoder->channels;
		const auto sample_rate      = decoder->sampleRate;
		const auto total_frames     = decoder->totalPCMFrameCount;
		const auto HertzToKilohertz = 1000;

		if (channels != 1) {
			LOG_ERR("DISKNOISE: FLAC file '%s' is not mono",
			        candidate.string().c_str());

			drflac_close(decoder);
			continue;
		}		
		if (sample_rate != DiskNoiseSampleRateInHz) {
			LOG_ERR("DISKNOISE: FLAC file '%s' should be %d kHz, but %d kHz was found",
			        candidate.string().c_str(),
			        DiskNoiseSampleRateInHz / HertzToKilohertz,
			        sample_rate / HertzToKilohertz);

			drflac_close(decoder);
			continue;
		}

		destination_buffer.resize(static_cast<size_t>(total_frames) * channels);
		drflac_uint64 frames_read = drflac_read_pcm_frames_f32(
		        decoder, total_frames, destination_buffer.data());
		drflac_close(decoder);

		if (frames_read == 0) {
			LOG_ERR("DISKNOISE: Failed to decode FLAC frames from '%s'",
			        candidate.string().c_str());
			continue;
		}

		// Scale data to integer value range
		const auto scale = static_cast<float>(INT16_MAX);
		for (auto& sample : destination_buffer) {
			sample *= scale;
		}

		LOG_DEBUG("DISKNOISE: Loaded %zu samples from '%s'",
		          destination_buffer.size(),
		          candidate.string().c_str());
		return;
	}

	LOG_ERR("DISKNOISE: Failed to find FLAC file: '%s'", path.c_str());
}

void DiskNoiseDevice::LoadSeekSamples(const std::vector<std::string>& paths)
{
	for (const auto& path : paths) {
		if (path.empty()) {
			// Placeholder for missing sample files
			seek.samples.emplace_back();
			continue;
		}

		std::vector<float> sample = {};
		LoadSample(path, sample);
		if (!sample.empty()) {
			seek.samples.emplace_back(std::move(sample));
		} else {
			seek.samples.emplace_back();
		}
	}

	// Assign weights with descending priority (most weight to first samples)
	const int max_weight = 10;
	seek.sample_weights.resize(seek.samples.size());
	for (size_t i = 0; i < seek.samples.size(); ++i) {
		seek.sample_weights[i] = static_cast<int>(max_weight - i);
	}
}

size_t DiskNoiseDevice::ChooseSeekIndex() const
{
	if (seek.samples.empty()) {
		return 0;
	}

	// Sequential seek, always use the first two samples
	if (seek_type == DiskNoiseSeekType::Sequential) {
		if (seek.samples.size() == 1) {
			return 0;
		}
		return (rand() % 2);
	}

	// Choose a random sample
	switch (disk_type) {
	case DiskType::Floppy: {
		if (seek.samples.size() <= 2) {
			return rand() % seek.samples.size();
		}
		// For floppy disks, prefer the first two samples with 80% chance
		if (rand() % 10 < 8) {
			return rand() % 2;
		} else {
			// 20% chance to use any of the other samples
			std::vector<size_t> valid_indices;
			for (size_t i = 2; i < seek.samples.size(); ++i) {
				if (!seek.samples[i].empty()) {
					valid_indices.push_back(i);
				}
			}
			if (valid_indices.empty()) {
				return 0;
			}
			const size_t r = static_cast<size_t>(rand()) %
			                 valid_indices.size();
			return valid_indices[r];
		}
		break;
	}
	case DiskType::HardDisk: {
		std::vector<size_t> valid_indices;
		// For hard disks, use all samples with equal probability
		for (size_t i = 0; i < seek.samples.size(); ++i) {
			if (!seek.samples[i].empty()) {
				valid_indices.push_back(i);
			}
		}
		if (valid_indices.empty()) {
			return 0;
		}
		const size_t r = static_cast<size_t>(rand()) % valid_indices.size();
		return valid_indices[r];
	}
	case DiskType::CdRom:
		// CD-ROM does not currently support disk noise emulation
		return 0;

	default: assertm(false, "Invalid ChooseSeekIndex type"); return 0;
	}

	return 0;
}

// This function influences whether the disk should sound like it is
// doing more sequential read/writes or seek randomly
void DiskNoiseDevice::SetLastIoPath(const std::string& path,
                                    DiskNoiseIoType disk_operation_type)
{
	if (disk_noise_mode == DiskNoiseMode::Off || path.empty()) {
		return;
	}

	if (disk_operation_type == DiskNoiseIoType::Write) {
		if (path.compare(last_file_write_path) == 0) {
			seek_type = DiskNoiseSeekType::Sequential;
		} else {
			seek_type = DiskNoiseSeekType::RandomAccess;
		}
		last_file_write_path = path;
	} else {
		if (path.compare(last_file_read_path) == 0) {
			seek_type = DiskNoiseSeekType::Sequential;
		} else {
			seek_type = DiskNoiseSeekType::RandomAccess;
		}
		last_file_read_path = path;
	}
}

DiskNoiseDevice::DiskNoiseDevice(const DiskType disk_type,
                                 const DiskNoiseMode disk_noise_mode,
                                 const std::string& spin_up_sample_path,
                                 const std::string& spin_sample_path,
                                 const std::vector<std::string>& seek_sample_paths)
        : disk_noise_mode(disk_noise_mode),
          disk_type(disk_type)
{
	if (disk_noise_mode == DiskNoiseMode::Off) {
		LOG_INFO("DISKNOISE: Disk noise emulation disabled");
		return;
	}

	// Only hard disk noises loop the spin sample
	spin.loop = (disk_type == DiskType::HardDisk);

	// Only attempt to load spin samples if disk noise mode is "on" instead
	// of "seek-only"
	if (disk_noise_mode == DiskNoiseMode::On) {
		LoadSample(spin_up_sample_path, spin.spin_up_sample);
		LoadSample(spin_sample_path, spin.sample);
	}

	// Start iterators at the beginning or end, depending on whether the
	// disk noise is looping (HDD) or not (FDD)
	// This prevents fdd spin noise on initial startup
	if (disk_type == DiskType::HardDisk && !spin.spin_up_sample.empty()) {
		spin.spin_up_it = spin.spin_up_sample.begin();
	} else {
		spin.spin_up_it = spin.spin_up_sample.end();
	}

	if (disk_type == DiskType::HardDisk && !spin.sample.empty()) {
		spin.spin_it = spin.sample.begin();
	} else {
		spin.spin_it = spin.sample.end();
	}

	LoadSeekSamples(seek_sample_paths);
	seek.current_sample.clear();
	seek.current_it = seek.current_sample.end();

	auto io_callback = [this]() {
		// This callback is called from the DOS code
		// to trigger the spin and seek sounds
		ActivateSpin();
		PlaySeek();
	};

	DOS_RegisterIoCallback(io_callback, disk_type);
}

// Destructor unregisters the callbacks
DiskNoiseDevice::~DiskNoiseDevice()
{
	DOS_UnregisterIoCallback(disk_type);
}

void DiskNoiseDevice::ActivateSpin()
{
	// Lock mutex to guard against GetNextFrame
	std::lock_guard<std::mutex> lock(mutex);

	if (disk_noise_mode == DiskNoiseMode::Off) {
		return;
	}

	// Floppy spin samples can be re-started at any time
	if (!spin.loop) {
		// Check if the sample is still playing and don't interrupt if
		// it does
		if (spin.sample.empty() || spin.spin_it != spin.sample.end()) {
			return;
		}
		// Restart spin sample
		spin.spin_it = spin.sample.begin();
	}
}

void DiskNoiseDevice::PlaySeek()
{
	// Lock mutex to guard against GetNextFrame
	std::lock_guard<std::mutex> lock(mutex);

	if (disk_noise_mode == DiskNoiseMode::Off) {
		return;
	}

	// Check if the sample is still playing and don't interrupt if it is
	if (!seek.current_sample.empty() &&
	    seek.current_it != seek.current_sample.end()) {
		return;
	}

	// Select a seek sample
	const size_t index = ChooseSeekIndex();
	if (index >= seek.samples.size() || seek.samples[index].empty()) {
		return;
	}

	// Set the new seek sample
	if (!seek.samples[index].empty()) {
		seek.current_sample = seek.samples[index];
		seek.current_it     = seek.current_sample.begin();
	}
}

void DiskNoises::SetLastIoPath(const std::string& path,
                               DiskNoiseIoType disk_operation_type, DiskType disk_type)
{
	switch (disk_type) {
	case DiskType::Floppy:
		if (disk_noises && disk_noises->floppy_noise) {
			disk_noises->floppy_noise->SetLastIoPath(path, disk_operation_type);
		}
		break;
	case DiskType::HardDisk:
		if (disk_noises && disk_noises->hdd_noise) {
			disk_noises->hdd_noise->SetLastIoPath(path, disk_operation_type);
		}
		break;
	case DiskType::CdRom:
		// CD-ROM does not currently support disk noise emulation
		break;
	default: assertm(false, "Invalid DiskType type"); break;
	}
}

static DiskNoiseMode get_disk_noise_mode(const std::string& mode)
{
    if (has_true(mode)) {
		return DiskNoiseMode::On;
	} else if (mode == "seek-only") {
		return DiskNoiseMode::SeekOnly;
	} else {
        assert(has_false(mode));
		return DiskNoiseMode::Off;
	}
}

void DISKNOISE_Init()
{
	const auto section = get_section("disknoise");
	assert(section);

	constexpr auto MaxNumSeekSamples = 9;

	const auto prop = static_cast<SectionProp*>(section);

	const auto enable_floppy_disk_noise = get_disk_noise_mode(
	        prop->GetString("floppy_disk_noise"));
	const auto enable_hard_disk_noise = get_disk_noise_mode(
	        prop->GetString("hard_disk_noise"));

	const auto spin_up = "hdd_spinup.flac";
	const auto spin    = "hdd_spin.flac";

	std::vector<std::string> hdd_seek_samples = {};

	for (auto i = 1; i <= MaxNumSeekSamples; ++i) {
		hdd_seek_samples.emplace_back("hdd_seek" + std::to_string(i) + ".flac");
	}

	const auto floppy_spin_up = "fdd_spinup.flac";
	const auto floppy_spin    = "fdd_spin.flac";

	std::vector<std::string> floppy_seek_samples = {};

	for (auto i = 1; i <= MaxNumSeekSamples; ++i) {
		floppy_seek_samples.emplace_back("fdd_seek" +
		                                 std::to_string(i) + ".flac");
	}
	disk_noises = std::make_unique<DiskNoises>(enable_floppy_disk_noise,
	                                           enable_hard_disk_noise,
	                                           spin_up,
	                                           spin,
	                                           hdd_seek_samples,
	                                           floppy_spin_up,
	                                           floppy_spin,
	                                           floppy_seek_samples);
}

void DISKNOISE_Destroy()
{
	MIXER_LockMixerThread();
	disk_noises.reset();
	MIXER_UnlockMixerThread();
}

static void init_disknoise_config_settings(SectionProp& secprop)
{
	constexpr auto Always = Property::Changeable::Always;

	auto* str_prop = secprop.AddString("hard_disk_noise", Always, "off");
	str_prop->SetValues({"off", "seek-only", "on"});
	str_prop->SetHelp(
	        "Enable emulated hard disk noises ('off' by default). Plays spinning disk and\n"
	        "seek noise sounds when enabled. It's recommended to set 'hard_disk_speed' to\n"
	        "lower than 'maximum' for an authentic experience. Possible values:\n"
	        "\n"
	        "  off:           No hard disk noises (default).\n"
	        "  seek-only:     Play hard disk seek noises only, no spin noises.\n"
	        "  on:            Play both hard disk seek and spin noises.\n"
	        "\n"
	        "Note: You can customise the disk noise volume by changing the volume of the\n"
	        "      `DISKNOISE` mixer channel.");

	str_prop = secprop.AddString("floppy_disk_noise", Always, "off");
	str_prop->SetValues({"off", "seek-only", "on"});
	str_prop->SetHelp(
	        "Enable emulated floppy disk noises ('off' by default). Plays spinning disk and\n"
	        "seek noise sounds when enabled. It's recommended to set 'floppy_disk_speed' to\n"
	        "lower than 'maximum' for an authentic experience. Possible values:\n"
	        "\n"
	        "  off:           No floppy disk noises (default).\n"
	        "  seek-only:     Play floppy disk seek noises only, no spin noises.\n"
	        "  on:            Play both floppy disk seek and spin noises.\n"
	        "\n"
	        "Note: You can customise the disk noise volume by changing the volume of the\n"
	        "      `DISKNOISE` mixer channel.");
}

void DISKNOISE_AddConfigSection(const ConfigPtr& conf)
{
	assert(conf);

	auto section = conf->AddSection("disknoise");

	init_disknoise_config_settings(*section);
}
