/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2024  The DOSBox Staging Team
 *  Copyright (C) 2002-2021  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "dosbox.h"

#include <array>
#include <iomanip>
#include <memory>
#include <queue>
#include <string>
#include <unistd.h>
#include <vector>

#include "autoexec.h"
#include "channel_names.h"
#include "control.h"
#include "dma.h"
#include "hardware.h"
#include "math_utils.h"
#include "mixer.h"
#include "pic.h"
#include "setup.h"
#include "shell.h"
#include "string_utils.h"

#define LOG_GUS 0 // set to 1 for detailed logging

// Global Constants
// ----------------

// AdLib emulation state constant
constexpr uint8_t ADLIB_CMD_DEFAULT = 85u;

// Environment variable names
const auto ultrasnd_env_name = "ULTRASND";
const auto ultradir_env_name = "ULTRADIR";

// Buffer and memory constants
constexpr uint32_t RAM_SIZE = 1024 * 1024; // 1 MB

// DMA transfer size and rate constants
constexpr uint32_t BYTES_PER_DMA_XFER = 8 * 1024;         // 8 KB per transfer
constexpr uint32_t ISA_BUS_THROUGHPUT = 32 * 1024 * 1024; // 32 MB/s
constexpr uint16_t DMA_TRANSFERS_PER_S = ISA_BUS_THROUGHPUT / BYTES_PER_DMA_XFER;
constexpr double MS_PER_DMA_XFER = millis_in_second / DMA_TRANSFERS_PER_S;

// Voice-channel and state related constants
constexpr uint8_t MAX_VOICES = 32u;
constexpr uint8_t MIN_VOICES = 14u;
constexpr uint8_t VOICE_DEFAULT_STATE = 3u;

// DMA and IRQ extents and quantities
constexpr uint8_t MIN_DMA_ADDRESS = 0u;
constexpr uint8_t MAX_DMA_ADDRESS = 7u;
constexpr uint8_t MIN_IRQ_ADDRESS = 0u;
constexpr uint8_t MAX_IRQ_ADDRESS = 15u;
constexpr uint8_t DMA_IRQ_ADDRESSES = 8u; // number of IRQ and DMA channels
constexpr uint16_t DMA_TC_STATUS_BITMASK = 0b100000000; // Status in 9th bit

// Pan position constants
constexpr uint8_t PAN_DEFAULT_POSITION = 7u;
constexpr uint8_t PAN_POSITIONS = 16u;  // 0: -45-deg, 7: centre, 15: +45-deg

// Timer delay constants
constexpr double TIMER_1_DEFAULT_DELAY = 0.080;
constexpr double TIMER_2_DEFAULT_DELAY = 0.320;

// Volume scaling and dampening constants
constexpr auto DELTA_DB = 0.002709201;     // 0.0235 dB increments
constexpr int16_t VOLUME_INC_SCALAR = 512; // Volume index increment scalar
constexpr uint16_t VOLUME_LEVELS = 4096u;

// Interwave addressing constant
constexpr int16_t WAVE_WIDTH = 1 << 9; // Wave interpolation width (9 bits)

// IO address quantities
constexpr uint8_t READ_HANDLERS = 8u;
constexpr uint8_t WRITE_HANDLERS = 9u;

// A group of parameters defining the Gus's voice IRQ control that's also shared
// (as a reference) into each instantiated voice.
struct VoiceIrq {
	uint32_t vol_state = 0u;
	uint32_t wave_state = 0u;
	uint8_t status = 0u;
};

// A group of parameters used in the Voice class to track the Wave and Volume
// controls.
struct VoiceCtrl {
	uint32_t &irq_state;
	int32_t start = 0;
	int32_t end = 0;
	int32_t pos = 0;
	int32_t inc = 0;
	uint16_t rate = 0;
	uint8_t state = VOICE_DEFAULT_STATE;
};

// Collection types involving constant quantities
using address_array_t     = std::array<uint8_t, DMA_IRQ_ADDRESSES>;
using pan_scalars_array_t = std::array<AudioFrame, PAN_POSITIONS>;
using ram_array_t         = std::vector<uint8_t>;
using read_io_array_t     = std::array<IO_ReadHandleObject, READ_HANDLERS>;
using vol_scalars_array_t = std::array<float, VOLUME_LEVELS>;
using write_io_array_t    = std::array<IO_WriteHandleObject, WRITE_HANDLERS>;

// A Voice is used by the Gus class and instantiates 32 of these.
// Each voice represents a single "mono" stream of audio having its own
// characteristics defined by the running program, such as:
//   - being 8bit or 16bit
//   - having a "position" along a left-right axis (panned)
//   - having its volume reduced by some amount (native-level down to 0)
//   - having start, stop, loop, and loop-backward controls
//   - informing the GUS DSP as to when an IRQ is needed to keep it playing
//
class Voice {
public:
	Voice(uint8_t num, VoiceIrq &irq) noexcept;
	Voice(Voice&&) = default;

	void RenderFrames(const ram_array_t& ram,
	                  const vol_scalars_array_t& vol_scalars,
	                  const pan_scalars_array_t& pan_scalars,
	                  std::vector<AudioFrame>& frames);

	uint8_t ReadVolState() const noexcept;
	uint8_t ReadWaveState() const noexcept;
	void ResetCtrls() noexcept;
	void WritePanPot(uint8_t pos) noexcept;
	void WriteVolRate(uint16_t rate) noexcept;
	void WriteWaveRate(uint16_t rate) noexcept;
	bool UpdateVolState(uint8_t state) noexcept;
	bool UpdateWaveState(uint8_t state) noexcept;

	VoiceCtrl vol_ctrl;
	VoiceCtrl wave_ctrl;

	uint32_t generated_8bit_ms = 0u;
	uint32_t generated_16bit_ms = 0u;

private:
	Voice() = delete;
	Voice(const Voice &) = delete;            // prevent copying
	Voice &operator=(const Voice &) = delete; // prevent assignment
	bool CheckWaveRolloverCondition() noexcept;
	bool Is16Bit() const noexcept;
	float GetVolScalar(const vol_scalars_array_t &vol_scalars);
	float GetSample(const ram_array_t &ram) noexcept;
	int32_t PopWavePos() noexcept;
	float PopVolScalar(const vol_scalars_array_t &vol_scalars);
	float Read8BitSample(const ram_array_t &ram, int32_t addr) const noexcept;
	float Read16BitSample(const ram_array_t &ram, int32_t addr) const noexcept;
	uint8_t ReadCtrlState(const VoiceCtrl &ctrl) const noexcept;
	void IncrementCtrlPos(VoiceCtrl &ctrl, bool skip_loop) noexcept;
	bool UpdateCtrlState(VoiceCtrl &ctrl, uint8_t state) noexcept;

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

	uint32_t irq_mask = 0u;
	uint8_t &shared_irq_status;
	uint8_t pan_position = PAN_DEFAULT_POSITION;
};

static void GUS_TimerEvent(uint32_t t);
static void GUS_DMA_Event(uint32_t val);

// The Gravis UltraSound GF1 DSP (classic)
// This class:
//   - Registers, receives, and responds to port address inputs, which are used
//     by the emulated software to configure and control the GUS card.
//   - Reads or provides audio samples via direct memory access (DMA)
//   - Provides shared resources to all of the Voices, such as the volume
//     reducing table, constant-power panning table, and IRQ states.
//   - Accumulates the audio from each active voice into a floating point
//     audio frame.
//   - Populates an autoexec line (ULTRASND=...) with its port, irq, and dma
//     addresses.
//
class Gus {
public:
	Gus(const io_port_t port_pref, const uint8_t dma_pref, const uint8_t irq_pref,
	    const char* ultradir, const std::string& filter_prefs);

	virtual ~Gus();

	bool CheckTimer(size_t t);
	void PrintStats();

	struct Timer {
		double delay = 0.0;
		uint8_t value = 0xff;
		bool has_expired = true;
		bool is_counting_down = false;
		bool is_masked = false;
		bool should_raise_irq = false;
	};
	Timer timer_one = {TIMER_1_DEFAULT_DELAY};
	Timer timer_two = {TIMER_2_DEFAULT_DELAY};
	bool PerformDmaTransfer();

private:
	Gus() = delete;
	Gus(const Gus &) = delete;            // prevent copying
	Gus &operator=(const Gus &) = delete; // prevent assignment

	void ActivateVoices(uint8_t requested_voices);
	void AudioCallback(uint16_t requested_frames);
	void BeginPlayback();
	void CheckIrq();
	void CheckVoiceIrq();
	uint32_t GetDmaOffset() noexcept;
	void UpdateDmaAddr(uint32_t offset) noexcept;
	void DmaCallback(const DmaChannel* chan, DMAEvent event);
	void StartDmaTransfers();
	bool IsDmaPcm16Bit() noexcept;
	bool IsDmaXfer16Bit() noexcept;
	uint16_t ReadFromRegister();

	void SetupEnvironment(const uint16_t port, const char* ultradir_env_val);
	void ClearEnvironment();

	void PopulatePanScalars() noexcept;
	void PopulateVolScalars() noexcept;
	void PrepareForPlayback() noexcept;
	uint16_t ReadFromPort(io_port_t port, io_width_t width);

	void RegisterIoHandlers();
	void Reset(uint8_t state);

	const std::vector<AudioFrame>& RenderFrames(const int num_requested_frames);

	void RenderUpToNow();
	void StopPlayback();
	void UpdateDmaAddress(uint8_t new_address);
	void UpdateWaveMsw(int32_t &addr) const noexcept;
	void UpdateWaveLsw(int32_t &addr) const noexcept;
	void WriteToPort(io_port_t port, io_val_t value, io_width_t width);

	void WriteToRegister();

	// Collections
	std::queue<AudioFrame> fifo     = {};
	vol_scalars_array_t vol_scalars = {{}};
	pan_scalars_array_t pan_scalars = {{}};
	ram_array_t ram                 = {};
	read_io_array_t read_handlers   = {};
	write_io_array_t write_handlers = {};
	std::vector<Voice> voices       = {};
	std::vector<AudioFrame> rendered_frames = {};

	const address_array_t dma_addresses = {
	        {MIN_DMA_ADDRESS, 1, 3, 5, 6, MAX_IRQ_ADDRESS, 0, 0}};
	const address_array_t irq_addresses = {
	        {MIN_IRQ_ADDRESS, 2, 5, 3, 7, 11, 12, MAX_IRQ_ADDRESS}};

	// Struct and pointer members
	VoiceIrq voice_irq = {};
	Voice *target_voice = nullptr;
	DmaChannel *dma_channel = nullptr;
	mixer_channel_t audio_channel = nullptr;

	// Playback related
	double last_rendered_ms = 0.0;
	double ms_per_render    = 0.0;
	int sample_rate_hz      = 0;

	uint8_t &adlib_command_reg = adlib_commandreg;

	// Port address
	io_port_t port_base = 0u;

	// Voice states
	uint32_t active_voice_mask = 0u;
	uint16_t voice_index = 0u;
	uint8_t active_voices = 0u;
	uint8_t prev_logged_voices = 0u;

	// RAM and register data 
	uint32_t dram_addr = 0u;
	uint16_t register_data = 0u;
	uint8_t selected_register = 0u;

	// Control states
	uint8_t mix_ctrl = 0x0b; // latches enabled, LINEs disabled
	uint8_t sample_ctrl = 0u;
	uint8_t timer_ctrl = 0u;

	// DMA states
	uint16_t dma_addr = 0u;
	uint8_t dma_addr_nibble = 0u;
	// dma_ctrl would normally be a uint8_t as real hardware uses 8 bits,
	// but we store the DMA terminal count status in the 9th bit
	uint16_t dma_ctrl = 0u;
	uint8_t dma1 = 0u; // playback DMA
	uint8_t dma2 = 0u; // recording DMA

	// IRQ states
	uint8_t irq1 = 0u; // playback IRQ
	uint8_t irq2 = 0u; // MIDI IRQ
	uint8_t irq_status = 0u;

	bool dac_enabled = false;
	bool irq_enabled = false;
	bool irq_previously_interrupted = false;
	bool is_running = false;
	bool should_change_irq_dma      = false;
};

// External Tie-in for OPL FM-audio
uint8_t adlib_commandreg = ADLIB_CMD_DEFAULT;

static std::unique_ptr<Gus> gus = nullptr;

Voice::Voice(uint8_t num, VoiceIrq &irq) noexcept
        : vol_ctrl{irq.vol_state},
          wave_ctrl{irq.wave_state},
          irq_mask(1 << num),
          shared_irq_status(irq.status)
{}

/*
Gravis SDK, Section 3.11. Rollover feature:
	Each voice has a 'rollover' feature that allows an application to be notified
	when a voice's playback position passes over a particular place in DRAM.  This
	is very useful for getting seamless digital audio playback.  Basically, the GF1
	will generate an IRQ when a voice's current position is  equal to the end
	position.  However, instead of stopping or looping back to the start position,
	the voice will continue playing in the same direction.  This means that there
	will be no pause (or gap) in the playback.

	Note that this feature is enabled/disabled through the voice's VOLUME control
	register (since there are no more bits available in the voice control
	registers).   A voice's loop enable bit takes precedence over the rollover. This
	means that if a voice's loop enable is on, it will loop when it hits the end
	position, regardless of the state of the rollover enable.
---
Joh Campbell, maintainer of DOSox-X:
	Despite the confusing description above, that means that looping takes
	precedence over rollover. If not looping, then rollover means to fire the IRQ
	but keep moving. If looping, then fire IRQ and carry out loop behavior. Gravis
	Ultrasound Windows 3.1 drivers expect this behavior, else Windows WAVE output
	will not work correctly.
*/
bool Voice::CheckWaveRolloverCondition() noexcept
{
	return (vol_ctrl.state & CTRL::BIT16) && !(wave_ctrl.state & CTRL::LOOP);
}

void Voice::IncrementCtrlPos(VoiceCtrl &ctrl, bool dont_loop_or_restart) noexcept
{
	if (ctrl.state & CTRL::DISABLED)
		return;
	int32_t remaining = 0;
	if (ctrl.state & CTRL::DECREASING) {
		ctrl.pos -= ctrl.inc;
		remaining = ctrl.start - ctrl.pos;
	} else {
		ctrl.pos += ctrl.inc;
		remaining = ctrl.pos - ctrl.end;
	}
	// Not yet reaching a boundary
	if (remaining < 0)
		return;

	// Generate an IRQ if requested
	if (ctrl.state & CTRL::RAISEIRQ) {
		ctrl.irq_state |= irq_mask;
	}

	// Allow the current position to move beyond its limit
	if (dont_loop_or_restart)
		return;

	// Should we loop?
	if (ctrl.state & CTRL::LOOP) {
		/* Bi-directional looping */
		if (ctrl.state & CTRL::BIDIRECTIONAL)
			ctrl.state ^= CTRL::DECREASING;
		ctrl.pos = (ctrl.state & CTRL::DECREASING)
		                   ? ctrl.end - remaining
		                   : ctrl.start + remaining;
	}
	// Otherwise, restart the position back to its start or end
	else {
		ctrl.state |= 1; // Stop the voice
		ctrl.pos = (ctrl.state & CTRL::DECREASING) ? ctrl.start : ctrl.end;
	}
	return;
}

bool Voice::Is16Bit() const noexcept
{
	return (wave_ctrl.state & CTRL::BIT16);
}

float Voice::GetSample(const ram_array_t &ram) noexcept
{
	const int32_t pos = PopWavePos();
	const auto addr = pos / WAVE_WIDTH;
	const auto fraction = pos & (WAVE_WIDTH - 1);
	const bool should_interpolate = wave_ctrl.inc < WAVE_WIDTH && fraction;
	const auto is_16bit = Is16Bit();
	float sample = is_16bit ? Read16BitSample(ram, addr)
	                        : Read8BitSample(ram, addr);
	if (should_interpolate) {
		const auto next_addr = addr + 1;
		const float next_sample = is_16bit ? Read16BitSample(ram, next_addr)
		                                   : Read8BitSample(ram, next_addr);
		constexpr float WAVE_WIDTH_INV = 1.0 / WAVE_WIDTH;
		sample += (next_sample - sample) *
		          static_cast<float>(fraction) * WAVE_WIDTH_INV;
	}
	assert(sample >= static_cast<float>(Min16BitSampleValue) &&
	       sample <= static_cast<float>(Max16BitSampleValue));
	return sample;
}

void Voice::RenderFrames(const ram_array_t& ram,
                         const vol_scalars_array_t& vol_scalars,
                         const pan_scalars_array_t& pan_scalars,
                         std::vector<AudioFrame>& frames)
{
	if (vol_ctrl.state & wave_ctrl.state & CTRL::DISABLED)
		return;

	const auto pan_scalar = pan_scalars.at(pan_position);

	// Sum the voice's samples into the exising frames, angled in L-R space
	for (auto& frame : frames) {
		float sample = GetSample(ram);
		sample *= PopVolScalar(vol_scalars);
		frame.left += sample * pan_scalar.left;
		frame.right += sample * pan_scalar.right;
	}
	// Keep track of how many ms this voice has generated
	Is16Bit() ? generated_16bit_ms++ : generated_8bit_ms++;
}

// Returns the current wave position and increments the position
// to the next wave position.
int32_t Voice::PopWavePos() noexcept
{
	const int32_t current_pos = wave_ctrl.pos;
	IncrementCtrlPos(wave_ctrl, CheckWaveRolloverCondition());
	return current_pos;
}

// Returns the current vol scalar and increments the volume control's position.
float Voice::PopVolScalar(const vol_scalars_array_t &vol_scalars)
{
	// transform the current position into an index into the volume array
	const auto i = ceil_sdivide(vol_ctrl.pos, VOLUME_INC_SCALAR);
	IncrementCtrlPos(vol_ctrl, false); // don't check wave rollover
	return vol_scalars.at(static_cast<size_t>(i));
}

// Read an 8-bit sample scaled into the 16-bit range, returned as a float
float Voice::Read8BitSample(const ram_array_t &ram, const int32_t addr) const noexcept
{
	const auto i = static_cast<size_t>(addr) & 0xfffffu;
	constexpr auto bits_in_16 = std::numeric_limits<int16_t>::digits;
	constexpr auto bits_in_8 = std::numeric_limits<int8_t>::digits;
	constexpr float to_16bit_range = 1 << (bits_in_16 - bits_in_8);
	return static_cast<int8_t>(ram.at(i)) * to_16bit_range;
}

// Read a 16-bit sample returned as a float
float Voice::Read16BitSample(const ram_array_t &ram, const int32_t addr) const noexcept
{
	const auto upper = addr & 0b1100'0000'0000'0000'0000;
	const auto lower = addr & 0b0001'1111'1111'1111'1111;
	const auto i = static_cast<uint32_t>(upper | (lower << 1));
	return static_cast<int16_t>(host_readw(&ram.at(i)));
}

uint8_t Voice::ReadCtrlState(const VoiceCtrl &ctrl) const noexcept
{
	uint8_t state = ctrl.state;
	if (ctrl.irq_state & irq_mask)
		state |= 0x80;
	return state;
}

uint8_t Voice::ReadVolState() const noexcept
{
	return ReadCtrlState(vol_ctrl);
}

uint8_t Voice::ReadWaveState() const noexcept
{
	return ReadCtrlState(wave_ctrl);
}

void Voice::ResetCtrls() noexcept
{
	vol_ctrl.pos = 0u;
	UpdateVolState(0x1);
	UpdateWaveState(0x1);
	WritePanPot(PAN_DEFAULT_POSITION);
}

bool Voice::UpdateCtrlState(VoiceCtrl &ctrl, uint8_t state) noexcept
{
	const uint32_t orig_irq_state = ctrl.irq_state;
	// Manually set the irq
	if ((state & 0xa0) == 0xa0)
		ctrl.irq_state |= irq_mask;
	else
		ctrl.irq_state &= ~irq_mask;

	// Always update the state
	ctrl.state = state & 0x7f;

	// Indicate if the IRQ state changed
	return orig_irq_state != ctrl.irq_state;
}

bool Voice::UpdateVolState(uint8_t state) noexcept
{
	return UpdateCtrlState(vol_ctrl, state);
}

bool Voice::UpdateWaveState(uint8_t state) noexcept
{
	return UpdateCtrlState(wave_ctrl, state);
}

void Voice::WritePanPot(uint8_t pos) noexcept
{
	constexpr uint8_t max_pos = PAN_POSITIONS - 1;
	pan_position = std::min(pos, max_pos);
}

// Four volume-index-rate "banks" are available that define the number of
// volume indexes that will be incremented (or decremented, depending on the
// volume_ctrl value) each step, for a given voice.  The banks are:
//
// - 0 to 63, which defines single index increments,
// - 64 to 127 defines fractional index increments by 1/8th,
// - 128 to 191 defines fractional index increments by 1/64ths, and
// - 192 to 255 defines fractional index increments by 1/512ths.
//
// To ensure the smallest increment (1/512) effects an index change, we
// normalize all the volume index variables (including this) by multiplying by
// VOLUME_INC_SCALAR (or 512). Note that "index" qualifies all these variables
// because they are merely indexes into the vol_scalars[] array. The actual
// volume scalar value (a floating point fraction between 0.0 and 1.0) is never
// actually operated on, and is simply looked up from the final index position
// at the time of sample population.
void Voice::WriteVolRate(uint16_t val) noexcept
{
	vol_ctrl.rate = val;
	constexpr uint8_t bank_lengths = 63u;
	const int pos_in_bank = val & bank_lengths;
	const int decimator = 1 << (3 * (val >> 6));
	vol_ctrl.inc = ceil_sdivide(pos_in_bank * VOLUME_INC_SCALAR, decimator);

	// Sanity check the bounds of the incrementer
	assert(vol_ctrl.inc >= 0 && vol_ctrl.inc <= bank_lengths * VOLUME_INC_SCALAR);
}

void Voice::WriteWaveRate(uint16_t val) noexcept
{
	wave_ctrl.rate = val;
	wave_ctrl.inc = ceil_udivide(val, 2u);
}

Gus::Gus(const io_port_t port_pref, const uint8_t dma_pref, const uint8_t irq_pref,
         const char* ultradir, const std::string& filter_prefs)
        : ram(RAM_SIZE),
          dma2(dma_pref),
          irq1(irq_pref),
          irq2(irq_pref)
{
	// port operations are "zero-based" from the datum to the user's port
	constexpr io_port_t port_datum = 0x200;
	port_base = port_pref - port_datum;

	// Create the internal voice channels
	for (uint8_t i = 0; i < MAX_VOICES; ++i) {
		voices.emplace_back(i, voice_irq);
	}
	assert(voices.size() == MAX_VOICES);

	RegisterIoHandlers();

	// Register the Audio and DMA channels
	const auto mixer_callback = std::bind(&Gus::AudioCallback, this,
	                                      std::placeholders::_1);

	audio_channel = MIXER_AddChannel(mixer_callback,
	                                 use_mixer_rate,
	                                 ChannelName::GravisUltrasound,
	                                 {ChannelFeature::Sleep,
	                                  ChannelFeature::Stereo,
	                                  ChannelFeature::ReverbSend,
	                                  ChannelFeature::ChorusSend,
	                                  ChannelFeature::DigitalAudio});

	assert(audio_channel);

	// GUS is prone to accumulating beyond the 16-bit range so we scale back
	// by RMS.
	constexpr auto rms_squared = static_cast<float>(M_SQRT1_2);
	audio_channel->Set0dbScalar(rms_squared);

	if (!audio_channel->TryParseAndSetCustomFilter(filter_prefs)) {
		if (filter_prefs != "off")
			LOG_WARNING("GUS: Invalid 'gus_filter' setting: '%s', using 'off'",
			            filter_prefs.c_str());

		audio_channel->SetHighPassFilter(FilterState::Off);
		audio_channel->SetLowPassFilter(FilterState::Off);

		set_section_property_value("gus", "gus_filter", "off");
	}

	ms_per_render = millis_in_second / audio_channel->GetSampleRate();

	UpdateDmaAddress(dma_pref);

	// Populate the volume, pan, and auto-exec arrays
	PopulateVolScalars();
	PopulatePanScalars();
	SetupEnvironment(port_pref, ultradir);

	LOG_MSG("GUS: Running on port %xh, IRQ %d, and DMA %d", port_pref, irq1, dma1);
}

void Gus::ActivateVoices(uint8_t requested_voices)
{
	requested_voices = clamp(requested_voices, MIN_VOICES, MAX_VOICES);
	if (requested_voices != active_voices) {
		active_voices = requested_voices;
		assert(active_voices <= voices.size());
		active_voice_mask = 0xffffffffu >> (MAX_VOICES - active_voices);

		// Gravis' calculation to convert from number of active voices
		// to playback frame rate. Ref: UltraSound Lowlevel ToolKit
		// v2.22 (21 December 1994), pp. 3 of 113.
		sample_rate_hz = 1000000.0 / (1.619695497 * active_voices);

		ms_per_render = millis_in_second / sample_rate_hz;

		audio_channel->SetSampleRate(sample_rate_hz);
	}
}

const std::vector<AudioFrame>& Gus::RenderFrames(const int num_requested_frames)
{
	// Size and zero the vector
	rendered_frames.resize(check_cast<size_t>(num_requested_frames));
	for (auto& frame : rendered_frames) {
		frame = {0.0f, 0.0f};
	}

	if (dac_enabled) {
		auto voice = voices.begin();
		const auto last_voice = voice + active_voices;
		while (voice < last_voice) {
			// Render all of the requested frames from each voice
			// before moving onto the next voice. This ensures each
			// voice can deliver all its samples without being
			// affected by state changes that (might) occur when
			// rendering subsequent voices.
			voice->RenderFrames(ram, vol_scalars, pan_scalars, rendered_frames);
			++voice;
		}
	}
	// If the DAC isn't enabled we still check the IRQ return a silent vector

	CheckVoiceIrq();
	return rendered_frames;
}

void Gus::RenderUpToNow()
{
	const auto now = PIC_FullIndex();

	// Wake up the channel and update the last rendered time datum.
	assert(audio_channel);
	if (audio_channel->WakeUp()) {
		last_rendered_ms = now;
		return;
	}

	const auto elapsed_ms = now - last_rendered_ms;
	if (elapsed_ms > ms_per_render) {
		// How many frames have elapsed since we last rendered?
		const auto num_elapsed_frames = iround(
		        std::floor(elapsed_ms / ms_per_render));
		assert(num_elapsed_frames > 0);

		// Enqueue in the FIFO that will be drained when the mixer pulls
		// frames
		for (auto& frame : RenderFrames(num_elapsed_frames)) {
			fifo.emplace(frame);
		}
		last_rendered_ms += num_elapsed_frames * ms_per_render;
	}
}

void Gus::AudioCallback(const uint16_t num_requested_frames)
{
	assert(audio_channel);

#if 0
	if (fifo.size())
		LOG_MSG("GUS: Queued %2lu cycle-accurate frames", fifo.size());
#endif

	auto num_frames_remaining = num_requested_frames;

	// First, send any frames we've queued since the last callback
	while (num_frames_remaining && fifo.size()) {
		audio_channel->AddSamples_sfloat(1, &fifo.front()[0]);
		fifo.pop();
		--num_frames_remaining;
	}
	// If the queue's run dry, render the remainder and sync-up our time datum
	if (num_frames_remaining > 0) {
		const auto frames = RenderFrames(num_frames_remaining);
		audio_channel->AddSamples_sfloat(num_frames_remaining,
		                                 &frames[0][0]);
	}
	last_rendered_ms = PIC_FullIndex();
}

void Gus::BeginPlayback()
{
	dac_enabled = ((register_data & 0x200) != 0);
	irq_enabled = ((register_data & 0x400) != 0);

	audio_channel->Enable(true);

	if (prev_logged_voices != active_voices) {
		LOG_MSG("GUS: Activated %u voices at %d Hz",
		        active_voices,
		        sample_rate_hz);

		prev_logged_voices = active_voices;
	}

	is_running = true;
}

void Gus::CheckIrq()
{
	const bool should_interrupt = irq_status & (irq_enabled ? 0xff : 0x9f);
	const bool lines_enabled = mix_ctrl & 0x08;
	if (should_interrupt && lines_enabled)
		PIC_ActivateIRQ(irq1);
	else if (irq_previously_interrupted)
		PIC_DeActivateIRQ(irq1);

#if LOG_GUS
	const auto state_str = should_interrupt && lines_enabled ? "activated"
	                     : irq_previously_interrupted        ? "deactivated"
	                                                         : "unchanged";
	LOG_MSG("GUS: CheckIrq: IRQ %s (should_interrupt: %d, lines: %d)",
	        state_str,
	        should_interrupt,
	        lines_enabled);
#endif

	irq_previously_interrupted = should_interrupt;
}

bool Gus::CheckTimer(const size_t t)
{
	auto &timer = t == 0 ? timer_one : timer_two;
	if (!timer.is_masked)
		timer.has_expired = true;
	if (timer.should_raise_irq) {
		irq_status |= 0x4 << t;
		CheckIrq();
	}
	return timer.is_counting_down;
}

void Gus::CheckVoiceIrq()
{
	irq_status &= 0x9f;
	const Bitu totalmask = (voice_irq.vol_state | voice_irq.wave_state) &
	                       active_voice_mask;
	if (!totalmask) {
		CheckIrq();
		return;
	}
	if (voice_irq.vol_state)
		irq_status |= 0x40;
	if (voice_irq.wave_state)
		irq_status |= 0x20;
	CheckIrq();
	while (!(totalmask & 1ULL << voice_irq.status)) {
		voice_irq.status++;
		if (voice_irq.status >= active_voices)
			voice_irq.status = 0;
	}
}

// Returns a 20-bit offset into the GUS's memory space holding the next
// DMA sample that will be read or written to via DMA. This offset
// is derived from the 16-bit DMA address register.
uint32_t Gus::GetDmaOffset() noexcept
{
	uint32_t adjusted;
	if(IsDmaXfer16Bit()) {
		const auto upper = dma_addr & 0b1100'0000'0000'0000;
		const auto lower = dma_addr & 0b0001'1111'1111'1111;
		adjusted = static_cast<uint32_t>(upper | (lower << 1));
	}
	else {
		adjusted = dma_addr;
	}
	return check_cast<uint32_t>(adjusted << 4) + dma_addr_nibble;
}

// Update the current 16-bit DMA position from the given 20-bit RAM offset
void Gus::UpdateDmaAddr(uint32_t offset) noexcept
{
	uint32_t adjusted;
	if (IsDmaXfer16Bit()) {
		const auto upper = offset & 0b1100'0000'0000'0000'0000;
		const auto lower = offset & 0b0011'1111'1111'1111'1110;
		adjusted = upper | (lower >> 1);
	}
	else {
		// Take the top 16 bits from the 20 bit address
		adjusted = offset & 0b1111'1111'1111'1111'0000;
	}
	// pack it into the 16-bit register
	dma_addr = static_cast<uint16_t>(adjusted >> 4);

	// hang onto the last nibble
	dma_addr_nibble = check_cast<uint8_t>(adjusted & 0xf);
}

bool Gus::PerformDmaTransfer()
{
	if (dma_channel->is_masked || !(dma_ctrl & 0x01)) {
		return false;
	}

#if LOG_GUS
	LOG_MSG("GUS DMA event: max %u bytes. DMA: tc=%u mask=0 cnt=%u",
	        BYTES_PER_DMA_XFER,
	        dma_channel->has_reached_terminal_count ? 1 : 0,
	        dma_channel->curr_count + 1);
#endif

	// Get the current DMA offset relative to the block of GUS memory
	const auto offset = GetDmaOffset();

	// Get the pending DMA count from channel
	const uint16_t desired = dma_channel->curr_count + 1;

	// Will the maximum transfer stay within the GUS RAM's size?
	assert(static_cast<size_t>(offset) + desired <= ram.size());

	// Perform the DMA transfer
	const bool is_reading = !(dma_ctrl & 0x2);
	const auto transfered =
	        (is_reading ? dma_channel->Read(desired, &ram.at(offset))
	                    : dma_channel->Write(desired, &ram.at(offset)));

	// Did we get everything we asked for?
	assert(transfered == desired);

	// scale the transfer by the DMA channel's bit-depth
	const auto bytes_transfered = transfered * (dma_channel->is_16bit + 1u);

	// Update the GUS's DMA address with the current position
	UpdateDmaAddr(check_cast<uint32_t>(offset + bytes_transfered));

	// If requested, invert the loaded samples' most-significant bits
	if (is_reading && dma_ctrl & 0x80) {
		auto ram_pos = ram.begin() + offset;
		const auto ram_pos_end = ram_pos + bytes_transfered;
		// adjust our start and skip size if handling 16-bit PCM samples
		ram_pos += IsDmaPcm16Bit() ? 1u : 0u;
		const auto skip = IsDmaPcm16Bit() ? 2u : 1u;
		assert(ram_pos >= ram.begin() && ram_pos <= ram_pos_end && ram_pos_end <= ram.end());
		while (ram_pos < ram_pos_end) {
			*ram_pos ^= 0x80;
			ram_pos += skip;
		}
	}
	// Raise the TC irq if needed
	if ((dma_ctrl & 0x20) != 0) {
		// We've hit the terminal count, so enable that bit
		dma_ctrl |= DMA_TC_STATUS_BITMASK;
		irq_status |= 0x80;
		CheckIrq();
		assert(dma_channel->has_reached_terminal_count);
		return false;
	}
	return true;
}

bool Gus::IsDmaPcm16Bit() noexcept
{
	return dma_ctrl & 0x40;
}

bool Gus::IsDmaXfer16Bit() noexcept
{
	// What bit-size should DMA memory be transferred as?
	// Mode PCM/DMA  Address Use-16  Note
	// 0x00   8/ 8   Any     No      Most DOS programs
	// 0x04   8/16   >= 4    Yes     16-bit if using High DMA
	// 0x04   8/16   < 4     No      8-bit if using Low DMA
	// 0x40  16/ 8   Any     No      Windows 3.1, Quake
	// 0x44  16/16   >= 4    Yes     Windows 3.1, Quake
	return (dma_ctrl & 0x4) && (dma1 >= 4);
}

static void GUS_DMA_Event(uint32_t)
{
	if (gus->PerformDmaTransfer())
		PIC_AddEvent(GUS_DMA_Event, MS_PER_DMA_XFER);
}

void Gus::StartDmaTransfers()
{
	PIC_AddEvent(GUS_DMA_Event, MS_PER_DMA_XFER);
}

void Gus::DmaCallback(const DmaChannel*, DMAEvent event)
{
	if (event == DMA_UNMASKED)
		StartDmaTransfers();
}

void Gus::SetupEnvironment(uint16_t port, const char* ultradir_env_val)
{
	// Ensure our port and addresses will fit in our format widths
	// The config selection controls their actual values, so this is a
	// maximum-limit.
	assert(port < 0xfff);
	assert(dma1 < 10 && dma2 < 10);
	assert(irq1 <= 12 && irq2 <= 12);

	// ULTRASND variable
	char ultrasnd_env_val[] = "HHH,D,D,II,II";
	safe_sprintf(ultrasnd_env_val, "%x,%u,%u,%u,%u", port, dma1, dma2, irq1, irq2);
	LOG_MSG("GUS: Setting '%s' environment variable to '%s'",
	        ultrasnd_env_name,
	        ultrasnd_env_val);
	AUTOEXEC_SetVariable(ultrasnd_env_name, ultrasnd_env_val);

	// ULTRADIR variable
	LOG_MSG("GUS: Setting '%s' environment variable to '%s'",
	        ultradir_env_name,
	        ultradir_env_val);
	AUTOEXEC_SetVariable(ultradir_env_name, ultradir_env_val);
}

void Gus::ClearEnvironment()
{
	AUTOEXEC_SetVariable(ultrasnd_env_name, "");
	AUTOEXEC_SetVariable(ultradir_env_name, "");
}

// Generate logarithmic to linear volume conversion tables
void Gus::PopulateVolScalars() noexcept
{
	constexpr auto VOLUME_LEVEL_DIVISOR = 1.0 + DELTA_DB;
	double scalar = 1.0;
	auto volume = vol_scalars.end();
	// The last element starts at 1.0 and we divide downward to
	// the first element that holds zero, which is directly assigned
	// after the loop.
	while (volume != vol_scalars.begin()) {
		*(--volume) = static_cast<float>(scalar);
		scalar /= VOLUME_LEVEL_DIVISOR;
	}
	vol_scalars.front() = 0.0;
}

/*
Constant-Power Panning
-------------------------
The GUS SDK describes having 16 panning positions (0 through 15)
with 0 representing the full-left rotation, 7 being the mid-point,
and 15 being the full-right rotation.  The SDK also describes
that output power is held constant through this range.

	#!/usr/bin/env python3
	import math
	print(f'Left-scalar  Pot Norm.   Right-scalar | Power')
	print(f'-----------  --- -----   ------------ | -----')
	for pot in range(16):
		norm = (pot - 7.) / (7.0 if pot < 7 else 8.0)
		direction = math.pi * (norm + 1.0 ) / 4.0
		lscale = math.cos(direction)
		rscale = math.sin(direction)
		power = lscale * lscale + rscale * rscale
		print(f'{lscale:.5f} <~~~ {pot:2} ({norm:6.3f})'\
		      f' ~~~> {rscale:.5f} | {power:.3f}')

	Left-scalar  Pot Norm.   Right-scalar | Power
	-----------  --- -----   ------------ | -----
	1.00000 <~~~  0 (-1.000) ~~~> 0.00000 | 1.000
	0.99371 <~~~  1 (-0.857) ~~~> 0.11196 | 1.000
	0.97493 <~~~  2 (-0.714) ~~~> 0.22252 | 1.000
	0.94388 <~~~  3 (-0.571) ~~~> 0.33028 | 1.000
	0.90097 <~~~  4 (-0.429) ~~~> 0.43388 | 1.000
	0.84672 <~~~  5 (-0.286) ~~~> 0.53203 | 1.000
	0.78183 <~~~  6 (-0.143) ~~~> 0.62349 | 1.000
	0.70711 <~~~  7 ( 0.000) ~~~> 0.70711 | 1.000
	0.63439 <~~~  8 ( 0.125) ~~~> 0.77301 | 1.000
	0.55557 <~~~  9 ( 0.250) ~~~> 0.83147 | 1.000
	0.47140 <~~~ 10 ( 0.375) ~~~> 0.88192 | 1.000
	0.38268 <~~~ 11 ( 0.500) ~~~> 0.92388 | 1.000
	0.29028 <~~~ 12 ( 0.625) ~~~> 0.95694 | 1.000
	0.19509 <~~~ 13 ( 0.750) ~~~> 0.98079 | 1.000
	0.09802 <~~~ 14 ( 0.875) ~~~> 0.99518 | 1.000
	0.00000 <~~~ 15 ( 1.000) ~~~> 1.00000 | 1.000
*/
void Gus::PopulatePanScalars() noexcept
{
	int i = 0;
	auto pan_scalar = pan_scalars.begin();
	while (pan_scalar != pan_scalars.end()) {
		// Normalize absolute range [0, 15] to [-1.0, 1.0]
		const auto norm = (i - 7.0) / (i < 7 ? 7 : 8);
		// Convert to an angle between 0 and 90-degree, in radians
		const auto angle = (norm + 1) * M_PI / 4;
		pan_scalar->left = static_cast<float>(cos(angle));
		pan_scalar->right = static_cast<float>(sin(angle));
		++pan_scalar;
		++i;

		// LOG_DEBUG("GUS: pan_scalar[%u] = %f | %f",
		//          i,
		//          pan_scalar->left,
		//          pan_scalar->right);
	}
}

void Gus::PrepareForPlayback() noexcept
{
	// Initialize the voice states
	for (auto &voice : voices)
		voice.ResetCtrls();

	// Initialize the OPL emulator state
	adlib_command_reg = ADLIB_CMD_DEFAULT;

	voice_irq = VoiceIrq{};
	timer_one = Timer{TIMER_1_DEFAULT_DELAY};
	timer_two = Timer{TIMER_2_DEFAULT_DELAY};

	if (!is_running) {
		register_data = 0x100; // DAC/IRQ disabled
		is_running = true;
	}
}

void Gus::PrintStats()
{
	// Aggregate stats from all voices
	uint32_t combined_8bit_ms = 0u;
	uint32_t combined_16bit_ms = 0u;
	uint32_t used_8bit_voices = 0u;
	uint32_t used_16bit_voices = 0u;
	for (const auto &voice : voices) {
		if (voice.generated_8bit_ms) {
			combined_8bit_ms += voice.generated_8bit_ms;
			used_8bit_voices++;
		}
		if (voice.generated_16bit_ms) {
			combined_16bit_ms += voice.generated_16bit_ms;
			used_16bit_voices++;
		}
	}
	const uint32_t combined_ms = combined_8bit_ms + combined_16bit_ms;

	// Is there enough information to be meaningful?
	if (combined_ms < 10000u || !(used_8bit_voices + used_16bit_voices))
		return;

	// Print info about the type of audio and voices used
	if (used_16bit_voices == 0u)
		LOG_MSG("GUS: Audio comprised of 8-bit samples from %u voices",
		        used_8bit_voices);
	else if (used_8bit_voices == 0u)
		LOG_MSG("GUS: Audio comprised of 16-bit samples from %u voices",
		        used_16bit_voices);
	else {
		const auto ratio_8bit = ceil_udivide(100u * combined_8bit_ms,
		                                     combined_ms);
		const auto ratio_16bit = ceil_udivide(100u * combined_16bit_ms,
		                                      combined_ms);
		LOG_MSG("GUS: Audio was made up of %u%% 8-bit %u-voice and "
		        "%u%% 16-bit %u-voice samples",
		        ratio_8bit, used_8bit_voices, ratio_16bit,
		        used_16bit_voices);
	}
}

uint16_t Gus::ReadFromPort(const io_port_t port, io_width_t width)
{
	//	LOG_MSG("GUS: Read from port %x", port);
	switch (port - port_base) {
	case 0x206: return irq_status;
	case 0x208:
		uint8_t time;
		time = 0u;
		if (timer_one.has_expired)
			time |= (1 << 6);
		if (timer_two.has_expired)
			time |= 1 << 5;
		if (time & 0x60)
			time |= 1 << 7;
		if (irq_status & 0x04)
			time |= 1 << 2;
		if (irq_status & 0x08)
			time |= 1 << 1;
		return time;
	case 0x20a: return adlib_command_reg;
	case 0x302: return static_cast<uint8_t>(voice_index);
	case 0x303: return selected_register;
	case 0x304:
		if (width == io_width_t::word)
			return ReadFromRegister() & 0xffff;
		else
			return ReadFromRegister() & 0xff;
	case 0x305: return ReadFromRegister() >> 8;
	case 0x307:
		return dram_addr < ram.size() ? ram.at(dram_addr) : 0;
	default:
#if LOG_GUS
		LOG_MSG("GUS: Read at port %#x", port);
#endif
		break;
	}
	return 0xff;
}

uint16_t Gus::ReadFromRegister()
{
	// LOG_MSG("GUS: Read register %x", selected_register);
	uint8_t reg = 0;

	// Registers that read from the general DSP
	switch (selected_register) {
	case 0x41: // DMA control register - read acknowledges DMA IRQ
		reg = dma_ctrl & 0xbf;
		// get the status and store it in bit 6 of the register
		reg |= (dma_ctrl & DMA_TC_STATUS_BITMASK) >> 2;
		dma_ctrl &= ~DMA_TC_STATUS_BITMASK; // clear the status bit
		irq_status &= 0x7f;
		CheckIrq();
		return static_cast<uint16_t>(reg << 8);
	case 0x42: // DMA address register
		return dma_addr;
	case 0x45: // Timer control register matches Adlib's behavior
		return static_cast<uint16_t>(timer_ctrl << 8);
	case 0x49: // DMA sample register
		reg = dma_ctrl & 0xbf;
		// get the status and store it in bit 6 of the register
		reg |= (dma_ctrl & DMA_TC_STATUS_BITMASK) >> 2;
		return static_cast<uint16_t>(reg << 8);
	case 0x4c: // Reset register
		reg = is_running ? 1 : 0;
		if (dac_enabled)
			reg |= 2;
		if (irq_enabled)
			reg |= 4;
		return static_cast<uint16_t>(reg << 8);
	case 0x8f: // General voice IRQ status register
		reg = voice_irq.status | 0x20;
		uint32_t mask;
		mask = 1 << voice_irq.status;
		if (!(voice_irq.vol_state & mask))
			reg |= 0x40;
		if (!(voice_irq.wave_state & mask))
			reg |= 0x80;
		voice_irq.vol_state &= ~mask;
		voice_irq.wave_state &= ~mask;
		CheckVoiceIrq();
		return static_cast<uint16_t>(reg << 8);
	default:
		break;
		// If the above weren't triggered, then fall-through
		// to the voice-specific register switch below.
	}

	if (!target_voice)
		return (selected_register == 0x80 || selected_register == 0x8d)
		               ? 0x0300
		               : 0u;

	// Registers that read from from the current voice
	switch (selected_register) {
	case 0x80: // Voice wave control read register
		return static_cast<uint16_t>(target_voice->ReadWaveState() << 8);
	case 0x82: // Voice MSB start address register
		return static_cast<uint16_t>(target_voice->wave_ctrl.start >> 16);
	case 0x83: // Voice LSW start address register
		return static_cast<uint16_t>(target_voice->wave_ctrl.start);
	case 0x89: // Voice volume register
	{
		const int i = ceil_sdivide(target_voice->vol_ctrl.pos,
		                           VOLUME_INC_SCALAR);
		assert(i >= 0 && i < static_cast<int>(vol_scalars.size()));
		return static_cast<uint16_t>(i << 4);
	}
	case 0x8a: // Voice MSB current address register
		return static_cast<uint16_t>(target_voice->wave_ctrl.pos >> 16);
	case 0x8b: // Voice LSW current address register
		return static_cast<uint16_t>(target_voice->wave_ctrl.pos);
	case 0x8d: // Voice volume control register
		return static_cast<uint16_t>(target_voice->ReadVolState() << 8);
	default:
#if LOG_GUS
		LOG_MSG("GUS: Register %#x not implemented for reading",
		        selected_register);
#endif
		break;
	}
	return register_data;
}

void Gus::RegisterIoHandlers()
{
	using namespace std::placeholders;
	
	// Register the IO read addresses
	assert(read_handlers.size() > 7);
	const auto read_from = std::bind(&Gus::ReadFromPort, this, _1, _2);
	read_handlers.at(0).Install(0x302 + port_base, read_from, io_width_t::byte);
	read_handlers.at(1).Install(0x303 + port_base, read_from, io_width_t::byte);
	read_handlers.at(2).Install(0x304 + port_base, read_from, io_width_t::word);
	read_handlers.at(3).Install(0x305 + port_base, read_from, io_width_t::byte);
	read_handlers.at(4).Install(0x206 + port_base, read_from, io_width_t::byte);
	read_handlers.at(5).Install(0x208 + port_base, read_from, io_width_t::byte);
	read_handlers.at(6).Install(0x307 + port_base, read_from, io_width_t::byte);
	// Board Only
	read_handlers.at(7).Install(0x20a + port_base, read_from, io_width_t::byte);

	// Register the IO write addresses
	// We'll leave the MIDI interface to the MPU-401
	// Ditto for the Joystick
	// GF1 Synthesizer
	assert(write_handlers.size() > 8);
	const auto write_to = std::bind(&Gus::WriteToPort, this, _1, _2, _3);
	write_handlers.at(0).Install(0x302 + port_base, write_to, io_width_t::byte);
	write_handlers.at(1).Install(0x303 + port_base, write_to, io_width_t::byte);
	write_handlers.at(2).Install(0x304 + port_base, write_to, io_width_t::word);
	write_handlers.at(3).Install(0x305 + port_base, write_to, io_width_t::byte);
	write_handlers.at(4).Install(0x208 + port_base, write_to, io_width_t::byte);
	write_handlers.at(5).Install(0x209 + port_base, write_to, io_width_t::byte);
	write_handlers.at(6).Install(0x307 + port_base, write_to, io_width_t::byte);
	// Board Only
	write_handlers.at(7).Install(0x200 + port_base, write_to, io_width_t::byte);
	write_handlers.at(8).Install(0x20b + port_base, write_to, io_width_t::byte);
}

void Gus::StopPlayback()
{
	// Halt playback before altering the DSP state
	audio_channel->Enable(false);

	dac_enabled = false;
	irq_enabled = false;
	irq_status = 0;
	irq_previously_interrupted = false;

	dma_ctrl = 0u;
	mix_ctrl = 0xb; // latches enabled, LINEs disabled
	timer_ctrl = 0u;
	sample_ctrl = 0u;

	target_voice = nullptr;
	voice_index = 0u;
	active_voices = 0u;

	UpdateDmaAddr(0);
	dram_addr = 0u;
	register_data = 0u;
	selected_register = 0u;
	should_change_irq_dma = false;
	PIC_RemoveEvents(GUS_TimerEvent);
	is_running = false;
}

static void GUS_TimerEvent(uint32_t t)
{
	if (gus->CheckTimer(t)) {
		const auto &timer = t == 0 ? gus->timer_one : gus->timer_two;
		PIC_AddEvent(GUS_TimerEvent, timer.delay, t);
	}
}

static void gus_destroy(Section*);

void Gus::UpdateDmaAddress(const uint8_t new_address)
{
	using namespace std::placeholders;

	// Has it changed?
	if (new_address == dma1) {
		return;
	}

	// Reset the old channel
	if (dma_channel) {
		dma_channel->Reset();
	}

	// Update the address, channel, and callback
	dma1 = new_address;
	dma_channel = DMA_GetChannel(dma1);
	assert(dma_channel);
	dma_channel->ReserveFor(ChannelName::GravisUltrasound, gus_destroy);
	dma_channel->RegisterCallback(std::bind(&Gus::DmaCallback, this, _1, _2));
#if LOG_GUS
	LOG_MSG("GUS: Assigned DMA1 address to %u", dma1);
#endif
}

void Gus::WriteToPort(io_port_t port, io_val_t value, io_width_t width)
{
	RenderUpToNow();

	const auto val = check_cast<uint16_t>(value);

	//	LOG_MSG("GUS: Write to port %x val %x", port, val);
	switch (port - port_base) {
	case 0x200:
		mix_ctrl = static_cast<uint8_t>(val);
		should_change_irq_dma = true;
		return;
	case 0x208: adlib_command_reg = static_cast<uint8_t>(val); break;
	case 0x209:
		// TODO adlib_command_reg should be 4 for this to work
		// else it should just latch the value
		if (val & 0x80) {
			timer_one.has_expired = false;
			timer_two.has_expired = false;
			return;
		}
		timer_one.is_masked = (val & 0x40) > 0;
		timer_two.is_masked = (val & 0x20) > 0;
		if (val & 0x1) {
			if (!timer_one.is_counting_down) {
				PIC_AddEvent(GUS_TimerEvent, timer_one.delay, 0);
				timer_one.is_counting_down = true;
			}
		} else
			timer_one.is_counting_down = false;
		if (val & 0x2) {
			if (!timer_two.is_counting_down) {
				PIC_AddEvent(GUS_TimerEvent, timer_two.delay, 1);
				timer_two.is_counting_down = true;
			}
		} else
			timer_two.is_counting_down = false;
		break;
		// TODO Check if 0x20a register is also available on the gus
		// like on the interwave
	case 0x20b:
		if (!should_change_irq_dma)
			break;
		should_change_irq_dma = false;
		if (mix_ctrl & 0x40) {
			// IRQ configuration, only use low bits for irq 1
			const auto i = val & 7u;
			const auto &address = irq_addresses.at(i);
			if (address)
				irq1 = address;
#if LOG_GUS
			LOG_MSG("GUS: Assigned IRQ1 to %d", irq1);
#endif
		} else {
			// DMA configuration, only use low bits for dma 1
			const uint8_t i = val & 0x7;
			const auto address = dma_addresses.at(i);
			if (address)
				UpdateDmaAddress(address);
		}
		break;
	case 0x302:
		voice_index = val & 31;
		target_voice = &voices.at(voice_index);
		break;
	case 0x303:
		selected_register = static_cast<uint8_t>(val);
		register_data = 0;
		break;
	case 0x304:
		if (width == io_width_t::word) {
			register_data = val;
			WriteToRegister();
		} else
			register_data = val;
		break;
	case 0x305:
		register_data = static_cast<uint16_t>((0x00ff & register_data) |
		                                      val << 8);
		WriteToRegister();
		break;
	case 0x307:
		if (dram_addr < ram.size())
			ram.at(dram_addr) = static_cast<uint8_t>(val);
		break;
	default:
#if LOG_GUS
		LOG_MSG("GUS: Write to port %#x with value %x", port, val);
#endif
		break;
	}
}

void Gus::UpdateWaveLsw(int32_t &addr) const noexcept
{
	constexpr auto WAVE_LSW_MASK = ~((1 << 16) - 1); // Lower wave mask
	const auto lower = addr & WAVE_LSW_MASK;
	addr = lower | register_data;
}

void Gus::UpdateWaveMsw(int32_t &addr) const noexcept
{
	constexpr auto WAVE_MSW_MASK = (1 << 16) - 1; // Upper wave mask
	const auto upper = register_data & 0x1fff;
	const auto lower = addr & WAVE_MSW_MASK;
	addr = lower | (upper << 16);
}

void Gus::WriteToRegister()
{
	RenderUpToNow();

	// Registers that write to the general DSP
	switch (selected_register) {
	case 0xe: // Set number of active voices
		selected_register = register_data >> 8; // Jazz Jackrabbit needs this
		{
			const uint8_t num_voices = 1 + ((register_data >> 8) & 31);
			ActivateVoices(num_voices);
		}
		return;
	case 0x10: // Undocumented register used in Fast Tracker 2
		return;
	case 0x41: // DMA control register
		// Clear all bits except the status and then replace dma_ctrl's
		// lower bits with reg's top 8 bits
		dma_ctrl &= DMA_TC_STATUS_BITMASK;
		dma_ctrl |= register_data >> 8;
		if (dma_ctrl & 1)
			StartDmaTransfers();
		return;
	case 0x42: // Gravis DRAM DMA address register
		dma_addr = register_data;
		dma_addr_nibble = 0u; // invalidate the nibble
		return;
	case 0x43: // LSW Peek/poke DRAM position
		dram_addr = (0xf0000 & dram_addr) |
		            (static_cast<uint32_t>(register_data));
		return;
	case 0x44: // MSB Peek/poke DRAM position
		dram_addr = (0x0ffff & dram_addr) |
		            (static_cast<uint32_t>(register_data) & 0x0f00) << 8;
		return;
	case 0x45: // Timer control register.  Identical in operation to Adlib's
		timer_ctrl = static_cast<uint8_t>(register_data >> 8);
		timer_one.should_raise_irq = (timer_ctrl & 0x04) > 0;
		if (!timer_one.should_raise_irq)
			irq_status &= ~0x04;
		timer_two.should_raise_irq = (timer_ctrl & 0x08) > 0;
		if (!timer_two.should_raise_irq)
			irq_status &= ~0x08;
		if (!timer_one.should_raise_irq && !timer_two.should_raise_irq)
			CheckIrq();
		return;
	case 0x46: // Timer 1 control
		timer_one.value = static_cast<uint8_t>(register_data >> 8);
		timer_one.delay = (0x100 - timer_one.value) * TIMER_1_DEFAULT_DELAY;
		return;
	case 0x47: // Timer 2 control
		timer_two.value = static_cast<uint8_t>(register_data >> 8);
		timer_two.delay = (0x100 - timer_two.value) * TIMER_2_DEFAULT_DELAY;
		return;
	case 0x49: // DMA sampling control register
		sample_ctrl = static_cast<uint8_t>(register_data >> 8);
		if (sample_ctrl & 1)
			StartDmaTransfers();
		return;
	case 0x4c: // Runtime control
		{
			const auto state = (register_data >> 8) & 7;
			if (state == 0)
				StopPlayback();
			else if (state == 1)
				PrepareForPlayback();
			else if (active_voices)
				BeginPlayback();
		}
		return;
	default:
		break;
		// If the above weren't triggered, then fall-through
		// to the target_voice-specific switch below.
	}

	// All the registers below operated on the target voice
	if (!target_voice)
		return;

	uint8_t data = 0;
	// Registers that write to the current voice
	switch (selected_register) {
	case 0x0: // Voice wave control register
		if (target_voice->UpdateWaveState(register_data >> 8))
			CheckVoiceIrq();
		break;
	case 0x1: // Voice rate control register
		target_voice->WriteWaveRate(register_data);
		break;
	case 0x2: // Voice MSW start address register
		UpdateWaveMsw(target_voice->wave_ctrl.start);
		break;
	case 0x3: // Voice LSW start address register
		UpdateWaveLsw(target_voice->wave_ctrl.start);
		break;
	case 0x4: // Voice MSW end address register
		UpdateWaveMsw(target_voice->wave_ctrl.end);
		break;
	case 0x5: // Voice LSW end address register
		UpdateWaveLsw(target_voice->wave_ctrl.end);
		break;
	case 0x6: // Voice volume rate register
		target_voice->WriteVolRate(register_data >> 8);
		break;
	case 0x7: // Voice volume start register  EEEEMMMM
		data = register_data >> 8;
		// Don't need to bounds-check the value because it's implied:
		// 'data' is a uint8, so is 255 at most. 255 << 4 = 4080, which
		// falls within-bounds of the 4096-long vol_scalars array.
		target_voice->vol_ctrl.start = (data << 4) * VOLUME_INC_SCALAR;
		break;
	case 0x8: // Voice volume end register  EEEEMMMM
		data = register_data >> 8;
		// Same as above regarding bound-checking.
		target_voice->vol_ctrl.end = (data << 4) * VOLUME_INC_SCALAR;
		break;
	case 0x9: // Voice current volume register
		// Don't need to bounds-check the value because it's implied:
		// reg data is a uint16, and 65535 >> 4 takes it down to 4095,
		// which is the last element in the 4096-long vol_scalars array.
		target_voice->vol_ctrl.pos = (register_data >> 4) * VOLUME_INC_SCALAR;
		break;
	case 0xa: // Voice MSW current address register
		UpdateWaveMsw(target_voice->wave_ctrl.pos);
		break;
	case 0xb: // Voice LSW current address register
		UpdateWaveLsw(target_voice->wave_ctrl.pos);
		break;
	case 0xc: // Voice pan pot register
		target_voice->WritePanPot(register_data >> 8);
		break;
	case 0xd: // Voice volume control register
		if (target_voice->UpdateVolState(register_data >> 8))
			CheckVoiceIrq();
		break;
	default:
#if LOG_GUS
		LOG_MSG("GUS: Register %#x not implemented for writing",
		        selected_register);
#endif
		break;
	}
	return;
}

Gus::~Gus()
{
	LOG_MSG("GUS: Shutting down");
	StopPlayback();

	// Prevent discovery of the GUS via the environment
	ClearEnvironment();

	// Stop the game from accessing the IO ports
	for (auto &rh : read_handlers)
		rh.Uninstall();
	for (auto &wh : write_handlers)
		wh.Uninstall();

	// Deregister the mixer channel, after which it's cleaned up
	assert(audio_channel);
	MIXER_DeregisterChannel(audio_channel);

	// Deregister the DMA source once the mixer channel is gone, which can
	// pull samples from DMA.
	if (dma_channel) {
		dma_channel->Reset();
	}
}

static void gus_destroy([[maybe_unused]] Section *sec)
{
	// GUS destroy is run when the user wants to deactivate the GUS:
	// C:\> config -set gus=false
	// TODO: therefore, this function should also remove the
	//       ULTRASND and ULTRADIR environment variables.

	if (gus) {
		gus->PrintStats();
		gus.reset();
	}
}

static void gus_init(Section *sec)
{
	assert(sec);
	const Section_prop *conf = dynamic_cast<Section_prop *>(sec);
	if (!conf || !conf->Get_bool("gus"))
		return;

	// Read the GUS config settings
	const auto port = static_cast<uint16_t>(conf->Get_hex("gusbase"));

	const auto dma = clamp(static_cast<uint8_t>(conf->Get_int("gusdma")),
	                       MIN_DMA_ADDRESS,
	                       MAX_DMA_ADDRESS);

	const auto irq = clamp(static_cast<uint8_t>(conf->Get_int("gusirq")),
	                       MIN_IRQ_ADDRESS,
	                       MAX_IRQ_ADDRESS);

	const std::string ultradir = conf->Get_string("ultradir");

	const std::string filter_prefs = conf->Get_string("gus_filter");

	// Instantiate the GUS with the settings
	gus = std::make_unique<Gus>(port, dma, irq, ultradir.c_str(), filter_prefs);

	constexpr auto changeable_at_runtime = true;
	sec->AddDestroyFunction(&gus_destroy, changeable_at_runtime);
}

void init_gus_dosbox_settings(Section_prop &secprop)
{
	constexpr auto when_idle = Property::Changeable::WhenIdle;

	auto *bool_prop = secprop.Add_bool("gus", when_idle, false);
	assert(bool_prop);
	bool_prop->Set_help(
	        "Enable Gravis UltraSound emulation (disabled by default).\n"
	        "The default settings of base address 240, IRQ 5, and DMA 3 have been chosen\n"
	        "so the GUS can coexist with a Sound Blaster card. This works fine for the\n"
	        "majority of programs, but some games and demos expect the GUS factory\n"
	        "defaults of base address 220, IRQ 11, and DMA 1.");

	auto *hex_prop = secprop.Add_hex("gusbase", when_idle, 0x240);
	assert(hex_prop);
	hex_prop->Set_values(
	        {"240", "220", "260", "280", "2a0", "2c0", "2e0", "300"});
	hex_prop->Set_help("The IO base address of the Gravis UltraSound (240 by default).");

	auto *int_prop = secprop.Add_int("gusirq", when_idle, 5);
	assert(int_prop);
	int_prop->Set_values({"3", "5", "7", "9", "10", "11", "12"});
	int_prop->Set_help("The IRQ number of the Gravis UltraSound (5 by default).");

	int_prop = secprop.Add_int("gusdma", when_idle, 3);
	assert(int_prop);
	int_prop->Set_values({"0", "1", "3", "5", "6", "7"});
	int_prop->Set_help("The DMA channel of the Gravis UltraSound (3 by default).");

	auto* str_prop = secprop.Add_string("ultradir", when_idle, "C:\\ULTRASND");
	assert(str_prop);
	str_prop->Set_help(
	        "Path to UltraSound directory ('C:\\ULTRASND' by default).\n"
	        "In this directory there should be a 'MIDI' directory that contains the patch\n"
	        "files for GUS playback.");

	str_prop = secprop.Add_string("gus_filter", when_idle, "off");
	assert(str_prop);
	str_prop->Set_help(
	        "Filter for the Gravis UltraSound audio output:\n"
	        "  off:       Don't filter the output (default).\n"
	        "  <custom>:  Custom filter definition; see 'sb_filter' for details.");
}

void GUS_AddConfigSection(const config_ptr_t& conf)
{
	assert(conf);

	constexpr auto changeable_at_runtime = true;

	Section_prop* sec = conf->AddSection_prop("gus", &gus_init, changeable_at_runtime);
	assert(sec);
	init_gus_dosbox_settings(*sec);
}
