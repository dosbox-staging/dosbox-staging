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

// Constants used in member definitions
constexpr int BUFFER_FRAMES = 48;
constexpr int BUFFER_SAMPLES = BUFFER_FRAMES * 2;
constexpr uint8_t DMA_IRQ_ADDRESSES = 8u; // number of IRQ and DMA channels
constexpr uint8_t MAX_VOICES = 32u;
constexpr uint8_t MAX_DMA_ADDRESS = 7u;
constexpr uint8_t MAX_IRQ_ADDRESS = 15u;
constexpr uint8_t MIN_DMA_ADDRESS = 1u;
constexpr uint8_t MIN_IRQ_ADDRESS = 2u;
constexpr float ONE_AMP = 1.0f; // first amplitude value
constexpr uint8_t PAN_DEFAULT_POSITION = 7u;
constexpr uint8_t PAN_POSITIONS = 16u;  // 0: -45-deg, 7: centre, 15: +45-deg
constexpr uint32_t RAM_SIZE = 1048576u; // 1 MB
constexpr uint8_t READ_HANDLERS = 8u;
constexpr float TIMER_1_DEFAULT_DELAY = 0.080f;
constexpr float TIMER_2_DEFAULT_DELAY = 0.320f;
constexpr uint8_t VOICE_DEFAULT_STATE = 3u;
constexpr uint16_t VOLUME_LEVELS = 4096u;
constexpr uint8_t WRITE_HANDLERS = 9u;

// A simple stereo audio frame that's used by the Gus and Voice classes.
struct AudioFrame {
	float left = 0.0f;
	float right = 0.0f;
};

// A group of parameters defining the Gus's voice IRQ control that's also shared
// (as a reference) into each instantiated voice.
struct VoiceIrq {
	uint32_t state = 0u;
	uint8_t count = 0u;
};

// A group of parameters used in the Voice class to track the Wave and Volume
// controls.
struct VoiceControl {
	int32_t start = 0;
	int32_t end = 0;
	int32_t pos = 0;
	int32_t inc = 0;
	uint16_t rate = 0;
	uint8_t state = VOICE_DEFAULT_STATE;
};

// Collection types involving constant quantities
using address_array_t = std::array<uint8_t, DMA_IRQ_ADDRESSES>;
using autoexec_array_t = std::array<AutoexecObject, 2>;
using read_io_array_t = std::array<IO_ReadHandleObject, READ_HANDLERS>;
using write_io_array_t = std::array<IO_WriteHandleObject, WRITE_HANDLERS>;

// A Voice, used by the Gus class, which instantiates 32 of these.
// Each voice represents a single "mono" stream of audio having it's own
// characteristics defined by the running program, such as:
//   - being 8bit or 16bit
//   - having a position placed left or right (panned)
//   - having a its volume scaled (from native-level down to 0)
//   - having start, stop, loop, and loop-backward controls
//   - informing the GUS DSP as to when an IRQ is needed to keep it playing
//
class Voice {
public:
	Voice(uint8_t num, VoiceIrq &irq);
	bool CheckWaveRolloverCondition();
	void GenerateSamples(float *stream,
	                     const uint8_t *ram,
	                     const float *vol_scalars,
	                     const AudioFrame *pan_scalars,
	                     const int requested_frames);

	void WritePanPot(uint8_t pos);
	void WriteVolRate(uint16_t val);
	void WriteWaveRate(uint16_t val);

	VoiceControl wave_ctrl = {};
	VoiceControl vol_ctrl = {};
	uint32_t irq_mask = 0u;
	uint32_t generated_8bit_ms = 0u;
	uint32_t generated_16bit_ms = 0u;

private:
	Voice() = delete;
	Voice(const Voice &) = delete;            // prevent copying
	Voice &operator=(const Voice &) = delete; // prevent assignment
	bool Is8Bit() const;
	float GetInterWavePercent() const;
	float GetInterWavePortion(const uint8_t *ram, const float sample) const;
	float Read8BitSample(const uint8_t *ram, const int32_t addr) const;
	float Read16BitSample(const uint8_t *ram, const int32_t addr) const;
	float GetSample(const uint8_t *ram) const;
	float GetVolumeScalar(const float *vol_scalars) const;
	void IncrementAddress();
	void IncrementVolScalar(const float *vol_scalars);
	uint8_t ReadPanPot() const;
	int32_t IncrementControl(VoiceControl &ctrl, bool skip_loop);

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

	// shared IRQ with the GUS DSP
	VoiceIrq &shared_irq;
	float sample_vol_scalar = 0;
	int32_t sample_address = 0u;
	uint8_t pan_position = PAN_DEFAULT_POSITION;
};

static void GUS_TimerEvent(Bitu t);
static void GUS_DMA_Event(Bitu val);

using voice_array_t = std::array<std::unique_ptr<Voice>, MAX_VOICES>;

// The Gravis UltraSound (Gus) DSP
// This class:
//   - Registers, receives, and responds to port address inputs, which are used
//     by the emulated software to configure and control the Gus.
//   - Reads audio content provided via direct memory access (DMA)
//   - Provides common resources to the Voices, such as the volume and pan tables
//   - Integrates the audio from its voices into a 16-bit stereo output stream
//   - Populates an autoexec line (ULTRASND=...) with its port, irq, and dma addresses
//
class Gus {
public:
	Gus(uint16_t port, uint8_t dma, uint8_t irq, const std::string &dir);
	bool CheckTimer(size_t t);
	void PrintStats();

	struct Timer {
		float delay = 0.0f;
		uint8_t value = 0xff;
		bool has_expired = true;
		bool is_counting_down = false;
		bool is_masked = false;
		bool should_raise_irq = false;
	};
	Timer timers[2] = {{TIMER_1_DEFAULT_DELAY}, {TIMER_2_DEFAULT_DELAY}};

	// moved to public to use the PIC queue
	uint8_t dma_ctrl = 0u;
	uint8_t dma1 = 0u; // recording DMA
	void GUS_DMA_Event_Transfer(DmaChannel *, uint32_t dmawords);

private:
	Gus() = delete;
	Gus(const Gus &) = delete;            // prevent copying
	Gus &operator=(const Gus &) = delete; // prevent assignment

	void ActivateVoices(uint8_t requested_voices);
	void AudioCallback(uint16_t requested_frames);
	void BeginPlayback();
	void CheckIrq();
	void CheckVoiceIrq();

	size_t Dma8Addr();
	size_t Dma16Addr();

	void GUS_DMA_Callback(DmaChannel *chan, DMAEvent event);
	void GUS_StartDMA();
	void GUS_StopDMA();
	bool IsDma16Bit();
	uint16_t ReadFromRegister();
	void PopulateAutoExec(uint16_t port, const std::string &dir);
	void PopulatePanScalars();
	void PopulateVolScalars();
	void PrepareForPlayback();
	uint8_t ReadCtrl(const VoiceControl &ctrl) const;
	size_t ReadFromPort(const size_t port, const size_t iolen);
	void RegisterIoHandlers();
	void Reset(uint8_t state);
	void SoftLimit(const float *in, int16_t *out);
	void StopPlayback();
	void UpdateWaveMsw(int32_t &addr) const;
	void UpdateWaveLsw(int32_t &addr) const;
	void UpdatePeakAmplitudes(const float *stream);
	void WriteCtrl(VoiceControl &ctrl, uint32_t irq_mask, uint8_t val);
	void WriteToPort(size_t port, size_t val, size_t iolen);
	void WriteToRegister();

	// Collections
	float vol_scalars[VOLUME_LEVELS] = {};
	float accumulator[BUFFER_SAMPLES] = {0};
	int16_t scaled[BUFFER_SAMPLES] = {};
	AudioFrame pan_scalars[PAN_POSITIONS] = {};
	uint8_t ram[RAM_SIZE] = {0u};
	read_io_array_t read_handlers = {};   // std::functions
	write_io_array_t write_handlers = {}; // std::functions
	const address_array_t dma_addresses = {
	        {0, MIN_DMA_ADDRESS, 3, 5, 6, MAX_IRQ_ADDRESS, 0, 0}};
	const address_array_t irq_addresses = {
	        {0, MIN_IRQ_ADDRESS, 5, 3, 7, 11, 12, MAX_IRQ_ADDRESS}};
	voice_array_t voices = {{nullptr}};
	autoexec_array_t autoexec_lines = {};

	// Struct and pointer members
	Voice *voice = nullptr;
	VoiceIrq voice_irq = {};
	MixerObject mixer_channel = {};
	AudioFrame peak = {ONE_AMP, ONE_AMP};
	uint8_t &adlib_command_reg = adlib_commandreg;
	MixerChannel *audio_channel = nullptr;

	// Port address
	size_t port_base = 0u;

	// Voice states
	uint32_t active_voice_mask = 0u;
	uint16_t voice_index = 0u;
	uint8_t active_voices = 0u;
	uint8_t prev_logged_voices = 0u;

	// Register and playback rate
	uint32_t dram_addr = 0u;
	uint32_t playback_rate = 0u;
	uint16_t register_data = 0u;
	uint8_t selected_register = 0u;

	// Control states
	uint8_t mix_ctrl = 0x0b; // latches enabled, LINEs disabled
	uint8_t sample_ctrl = 0u;
	uint8_t timer_ctrl = 0u;

	// DMA states
	uint16_t dma_addr = 0u;
	uint8_t dma2 = 0u; // playback DMA

	// IRQ states
	uint8_t irq1 = 0u; // playback IRQ
	uint8_t irq2 = 0u; // MIDI IRQ
	uint8_t irq_status = 0u;
	bool irq_enabled = false;
	bool should_change_irq_dma = false;
};

#endif
