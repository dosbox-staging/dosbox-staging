/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Copyright (C) 2020-2020  The dosbox-staging team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef DOSBOX_GUS_H
#define DOSBOX_GUS_H

#include "dosbox.h"

#include <array>
#include <memory>
#include <string>

#include "dma.h"
#include "hardware.h"
#include "mixer.h"
#include "pic.h"
#include "shell.h"

constexpr uint8_t BUFFER_FRAMES = 64u;
constexpr uint8_t DMA_IRQ_ADDRESSES = 8u; // number of IRQ and DMA channels
constexpr uint8_t MAX_VOICES = 32u;
constexpr float ONE_AMP = 1.0f;              // first amplitude value
constexpr uint8_t PAN_DEFAULT_POSITION = 7u;
constexpr uint8_t PAN_POSITIONS = 16u;  // 0: -45-deg, 7: centre, 15: +45-deg
constexpr uint32_t RAM_SIZE = 1048576u; // 1 MB
constexpr uint8_t READ_HANDLERS = 8u;
constexpr float TIMER_1_DEFAULT_DELAY = 0.080f;
constexpr float TIMER_2_DEFAULT_DELAY = 0.320f;
constexpr uint8_t VOICE_DEFAULT_STATE = 3u;
constexpr uint16_t VOLUME_LEVELS = 4096u;
constexpr uint8_t WRITE_HANDLERS = 9u;


struct AudioFrame {
	float left = 0.0f;
	float right = 0.0f;
};

// A set of common IRQs shared between the DSP and each voice
struct VoiceIrq {
	uint32_t state = 0u;
	uint8_t count = 0u;
};

struct VoiceControl {
	int32_t start = 0;
	int32_t end = 0;
	int32_t pos = 0;
	int32_t inc = 0;
	uint16_t rate = 0;
	uint8_t state = VOICE_DEFAULT_STATE;
};

// A Single GUS Voice
class Voice {
public:
	Voice(uint8_t num, VoiceIrq &irq);
	bool CheckWaveRolloverCondition();
	void ClearStats();
	void GenerateSamples(float *stream,
	                     const uint8_t *ram,
	                     const float *vol_scalars,
	                     const AudioFrame *pan_scalars,
	                     AudioFrame &peak,
	                     uint16_t requested_frames);

	void WritePanPot(uint8_t pos);
	void WriteVolRate(uint16_t val);
	void WriteWaveRate(uint16_t val);
	void UpdateWaveAndVol();
	void UpdateWaveBitDepth();

	// bit-depth tracking
	uint32_t generated_8bit_ms = 0u;
	uint32_t generated_16bit_ms = 0u;

	VoiceControl wctrl;
	VoiceControl vctrl;
	uint32_t irq_mask = 0u;

private:
	Voice() = delete;
	Voice(const Voice &) = delete;            // prevent copying
	Voice &operator=(const Voice &) = delete; // prevent assignment

	inline float GetSample8(const uint8_t *ram, const int32_t addr) const;
	inline float GetSample16(const uint8_t *ram, const int32_t addr) const;
	uint8_t ReadPanPot() const;
	void UpdateControl(VoiceControl &ctrl, bool skip_loop);

	// Control states
	enum CTRL : uint8_t {
		RESET = 0x01,
		STOPPED = 0x02,
		DISABLED = RESET | STOPPED,
		BIT16 = 0x04,
		LOOP = 0x08,
		BIDIRECTIONAL = 0x10,
		RAISEIRQ = 0x20,
		DECREASING = 0x40,
	};

	typedef std::function<float(const uint8_t *, const int32_t)> get_sample_f;
	get_sample_f GetSample = std::bind(&Voice::GetSample8, this, std::placeholders::_1, std::placeholders::_2);

	// shared IRQs with the GUS DSP
	VoiceIrq &shared_irq;
	uint32_t &generated_ms = generated_8bit_ms;
	uint8_t pan_position = PAN_DEFAULT_POSITION;
};

class Gus {
public:
	Gus(uint16_t port, uint8_t dma, uint8_t irq, const std::string &dir);
	~Gus();
	bool CheckTimer(size_t t);
	
	struct Timer {
		float delay = 0.0f;
		uint8_t value = 0xff;
		bool has_expired = false;
		bool is_counting_down = false;
		bool is_masked = false;
		bool should_raise_irq = false;
	};
	Timer timers[2] = {{TIMER_1_DEFAULT_DELAY}, {TIMER_2_DEFAULT_DELAY}};

private:
	Gus() = delete;
	Gus(const Gus &) = delete;            // prevent copying
	Gus &operator=(const Gus &) = delete; // prevent assignment

	void AudioCallback(uint16_t requested_frames);
	void CheckIrq();
	void CheckVoiceIrq();
	void DmaCallback(DmaChannel *dma_channel, DMAEvent event);
	uint16_t ReadFromRegister();
	void PopulateAutoExec(uint16_t port, const std::string &dir);
	void PopulatePanScalars();
	void PopulateVolScalars();
	void PrintStats();
	uint8_t ReadCtrl(const VoiceControl &ctrl) const;
	size_t ReadFromPort(const size_t port, const size_t iolen);
	void Reset();
	bool SoftLimit(float (&)[BUFFER_FRAMES][2], int16_t (&)[BUFFER_FRAMES][2]);
	void UpdateWaveMsw(int32_t &addr) const;
	void UpdateWaveLsw(int32_t &addr) const;
	void WriteCtrl(VoiceControl &ctrl, uint32_t irq_mask, uint8_t val);
	void WriteToPort(size_t port, size_t val, size_t iolen);
	void WriteToRegister();

	// Collections
	std::array<uint8_t, RAM_SIZE> ram = {{0u}};
	std::array<AutoexecObject, 2> autoexec_lines = {};
	std::array<float, VOLUME_LEVELS> vol_scalars = {};
	std::array<AudioFrame, PAN_POSITIONS> pan_scalars = {};
	const std::array<uint8_t, DMA_IRQ_ADDRESSES> dma_addresses = {{0, 1, 3, 5, 6, 7, 0, 0}};
	const std::array<uint8_t, DMA_IRQ_ADDRESSES> irq_addresses = {{0, 2, 5, 3, 7, 11, 12, 15}};
	std::array<std::unique_ptr<Voice>, MAX_VOICES> voices = {{nullptr}};
	std::array<IO_ReadHandleObject, READ_HANDLERS> read_handlers = {};
	std::array<IO_WriteHandleObject, WRITE_HANDLERS> write_handlers = {};

	// Compound and pointer members
	Voice *voice = nullptr;
	VoiceIrq voice_irq = {};
	MixerObject mixer_channel = {};
	AudioFrame peak_amplitude = {ONE_AMP, ONE_AMP};
	uint8_t &adlib_command_reg = adlib_commandreg;
	MixerChannel *audio_channel = nullptr;

	// Port address
	size_t port_base = 0u;

	// Voice states
	uint32_t active_voice_mask = 0u;
	uint16_t voice_index = 0u;
	uint8_t active_voices = 0u;

	// Register and playback rate
	uint32_t dram_addr = 0u;
	uint32_t playback_rate = 0u;
	uint16_t register_data = 0u;
	uint16_t reset_register = 0u;
	uint8_t selected_register = 0u;

	// Control states
	uint8_t mix_ctrl = 0x0b; // latches enabled, LINEs disabled
	uint8_t sample_ctrl = 0u;
	uint8_t timer_ctrl = 0u;
	uint8_t dma_ctrl = 0u;

	// DMA states
	uint16_t dma_addr = 0u;
	uint8_t dma1 = 0u; // recording DMA
	uint8_t dma2 = 0u; // playback DMA

	// IRQ states
	uint8_t irq1 = 0u; // playback IRQ
	uint8_t irq2 = 0u; // MIDI IRQ
	uint8_t irq_status = 0u;
	bool irq_enabled = false;
	bool should_change_irq_dma = false;
};

#endif
