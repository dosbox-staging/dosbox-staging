// SPDX-FileCopyrightText:  2025-2025 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_DISK_NOISE_H
#define DOSBOX_DISK_NOISE_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "dos/dos.h"
#include "mixer.h"

enum DiskNoiseIoType { Read, Write };

enum DiskNoiseSeekType {
	Sequential,
	RandomAccess,
};

enum class DiskNoiseMode {
	Off,
	SeekOnly,
	On,
};

class DiskNoiseDevice {
public:
	DiskNoiseDevice(const DiskType disk_type, const DiskNoiseMode disk_noise_mode,
	                const std::string& spin_up_sample_path,
	                const std::string& spin_sample_path,
	                const std::vector<std::string>& seek_sample_paths);
	~DiskNoiseDevice();
	void ActivateSpin();
	void PlaySeek();
	AudioFrame GetNextFrame();
	void SetLastIoPath(const std::string& path,
	                   DiskNoiseIoType disk_operation_type);

private:
	DiskNoiseMode disk_noise_mode    = DiskNoiseMode::Off;
	DiskType disk_type               = DiskType::HardDisk;
	std::mutex mutex                 = {};
	std::string last_file_read_path  = {};
	std::string last_file_write_path = {};
	DiskNoiseSeekType seek_type      = DiskNoiseSeekType::RandomAccess;

	struct SpinSample {
		std::vector<float> spin_up_sample = {};
		std::vector<float> sample         = {};
		bool loop                         = false;
		std::vector<float>::const_iterator spin_up_it;
		std::vector<float>::const_iterator spin_it;
	} spin = {};

	struct SeekSample {
		std::vector<std::vector<float>> samples = {};
		std::vector<int> sample_weights         = {};
		std::vector<float> current_sample       = {};
		std::vector<float>::const_iterator current_it;
	} seek = {};

	void LoadSample(const std::string& path,
	                std::vector<float>& destination_buffer);
	void LoadSeekSamples(const std::vector<std::string>& paths);
	size_t ChooseSeekIndex() const;
};

class DiskNoises {
public:
	DiskNoises(const DiskNoiseMode enable_floppy_disk_noise,
	           const DiskNoiseMode enable_hard_disk_noise,
	           const std::string& spin_up, const std::string& spin,
	           const std::vector<std::string>& hdd_seek_samples,
	           const std::string& floppy_spin_up, const std::string& floppy_spin,
	           const std::vector<std::string>& floppy_seek_samples);
	~DiskNoises();
	static DiskNoises* GetInstance();
	void SetLastIoPath(const std::string& path,
	                   DiskNoiseIoType disk_operation_type, DiskType disk_type);

	std::shared_ptr<MixerChannel> mix_channel                    = nullptr;
	std::vector<std::shared_ptr<DiskNoiseDevice>> active_devices = {};

private:
	std::shared_ptr<DiskNoiseDevice> floppy_noise = nullptr;
	std::shared_ptr<DiskNoiseDevice> hdd_noise    = nullptr;

	void AudioCallback(int frames);
};

void DISKNOISE_AddConfigSection(const ConfigPtr& conf);
void DISKNOISE_Init();
void DISKNOISE_Destroy();

#endif // DOSBOX_DISK_NOISE_H
