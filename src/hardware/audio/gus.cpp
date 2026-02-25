// SPDX-FileCopyrightText:  2020-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "private/gus.h"

#include <array>
#include <iomanip>
#include <memory>
#include <queue>
#include <string>
#include <unistd.h>
#include <vector>

#include "audio/channel_names.h"
#include "config/config.h"
#include "config/setup.h"
#include "dosbox.h"
#include "hardware/pic.h"
#include "hardware/timer.h"
#include "misc/notifications.h"
#include "shell/autoexec.h"
#include "shell/shell.h"
#include "utils/bit_view.h"
#include "utils/math_utils.h"
#include "utils/string_utils.h"

#define LOG_GUS 0 // set to 1 for detailed logging

static std::unique_ptr<Gus> gus = {};

Voice::Voice(uint8_t num, VoiceIrq& irq) noexcept
        : vol_ctrl{irq.vol_state},
          wave_ctrl{irq.wave_state},
          irq_mask(1 << num),
          shared_irq_status(irq.status)
{}

/*
Gravis SDK, Section 3.11. Rollover feature:
        Each voice has a 'rollover' feature that allows an application to be
notified when a voice's playback position passes over a particular place in
DRAM.  This is very useful for getting seamless digital audio playback.
Basically, the GF1 will generate an IRQ when a voice's current position is equal
to the end position.  However, instead of stopping or looping back to the start
position, the voice will continue playing in the same direction.  This means
that there will be no pause (or gap) in the playback.

        Note that this feature is enabled/disabled through the voice's VOLUME
control register (since there are no more bits available in the voice control
        registers).   A voice's loop enable bit takes precedence over the
rollover. This means that if a voice's loop enable is on, it will loop when it
hits the end position, regardless of the state of the rollover enable.
---
Joh Campbell, maintainer of DOSox-X:
        Despite the confusing description above, that means that looping takes
        precedence over rollover. If not looping, then rollover means to fire
the IRQ but keep moving. If looping, then fire IRQ and carry out loop behavior.
Gravis Ultrasound Windows 3.1 drivers expect this behavior, else Windows WAVE
output will not work correctly.
*/
bool Voice::CheckWaveRolloverCondition() noexcept
{
	return (vol_ctrl.state & CTRL::BIT16) && !(wave_ctrl.state & CTRL::LOOP);
}

void Voice::IncrementCtrlPos(VoiceCtrl& ctrl, bool dont_loop_or_restart) noexcept
{
	if (ctrl.state & CTRL::DISABLED) {
		return;
	}
	int32_t remaining = 0;
	if (ctrl.state & CTRL::DECREASING) {
		ctrl.pos -= ctrl.inc;
		remaining = ctrl.start - ctrl.pos;
	} else {
		ctrl.pos += ctrl.inc;
		remaining = ctrl.pos - ctrl.end;
	}
	// Not yet reaching a boundary
	if (remaining < 0) {
		return;
	}

	// Generate an IRQ if requested
	if (ctrl.state & CTRL::RAISEIRQ) {
		ctrl.irq_state |= irq_mask;
	}

	// Allow the current position to move beyond its limit
	if (dont_loop_or_restart) {
		return;
	}

	// Should we loop?
	if (ctrl.state & CTRL::LOOP) {
		/* Bi-directional looping */
		if (ctrl.state & CTRL::BIDIRECTIONAL) {
			ctrl.state ^= CTRL::DECREASING;
		}
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

float Voice::GetSample(const ram_array_t& ram) noexcept
{
	const int32_t pos             = PopWavePos();
	const auto addr               = pos / WAVE_WIDTH;
	const auto fraction           = pos & (WAVE_WIDTH - 1);
	const bool should_interpolate = wave_ctrl.inc < WAVE_WIDTH && fraction;
	const auto is_16bit           = Is16Bit();
	float sample                  = is_16bit ? Read16BitSample(ram, addr)
	                                         : Read8BitSample(ram, addr);
	if (should_interpolate) {
		const auto next_addr           = addr + 1;
		const float next_sample        = is_16bit
		                                       ? Read16BitSample(ram, next_addr)
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
	if (vol_ctrl.state & wave_ctrl.state & CTRL::DISABLED) {
		return;
	}

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
float Voice::PopVolScalar(const vol_scalars_array_t& vol_scalars)
{
	// transform the current position into an index into the volume array
	const auto i = ceil_sdivide(vol_ctrl.pos, VOLUME_INC_SCALAR);
	IncrementCtrlPos(vol_ctrl, false); // don't check wave rollover
	return vol_scalars.at(static_cast<size_t>(i));
}

// Read an 8-bit sample scaled into the 16-bit range, returned as a float
float Voice::Read8BitSample(const ram_array_t& ram, const int32_t addr) const noexcept
{
	const auto i                   = static_cast<size_t>(addr) & 0xfffff;
	constexpr auto bits_in_16      = std::numeric_limits<int16_t>::digits;
	constexpr auto bits_in_8       = std::numeric_limits<int8_t>::digits;
	constexpr float to_16bit_range = 1 << (bits_in_16 - bits_in_8);
	return static_cast<int8_t>(ram.at(i)) * to_16bit_range;
}

// Read a 16-bit sample returned as a float
float Voice::Read16BitSample(const ram_array_t& ram, const int32_t addr) const noexcept
{
	const auto upper = addr & 0b1100'0000'0000'0000'0000;
	const auto lower = addr & 0b0001'1111'1111'1111'1111;
	const auto i     = static_cast<uint32_t>(upper | (lower << 1));
	return static_cast<int16_t>(host_readw(&ram.at(i)));
}

uint8_t Voice::ReadCtrlState(const VoiceCtrl& ctrl) const noexcept
{
	uint8_t state = ctrl.state;
	if (ctrl.irq_state & irq_mask) {
		state |= 0x80;
	}
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
	vol_ctrl.pos = 0;
	UpdateVolState(0x1);
	UpdateWaveState(0x1);
	WritePanPot(PAN_DEFAULT_POSITION);
}

bool Voice::UpdateCtrlState(VoiceCtrl& ctrl, uint8_t state) noexcept
{
	const uint32_t orig_irq_state = ctrl.irq_state;
	// Manually set the irq
	if ((state & 0xa0) == 0xa0) {
		ctrl.irq_state |= irq_mask;
	} else {
		ctrl.irq_state &= ~irq_mask;
	}

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
	pan_position              = std::min(pos, max_pos);
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
	vol_ctrl.rate                  = val;
	constexpr uint8_t bank_lengths = 63;
	const int pos_in_bank          = val & bank_lengths;
	const int decimator            = 1 << (3 * (val >> 6));
	vol_ctrl.inc = ceil_sdivide(pos_in_bank * VOLUME_INC_SCALAR, decimator);

	// Sanity check the bounds of the incrementer
	assert(vol_ctrl.inc >= 0 && vol_ctrl.inc <= bank_lengths * VOLUME_INC_SCALAR);
}

void Voice::WriteWaveRate(uint16_t val) noexcept
{
	wave_ctrl.rate = val;
	wave_ctrl.inc  = ceil_udivide(val, 2u);
}

// We use IRQ2 in GUS's public API (conf, environment, and the IO port 2xB
// address selector lookup tables) because that's what the documentation
// describes and more critically, it's what games and applications expect and
// use via the IO port.
//
// However we convert IRQ2 to IRQ9 internally because that's how real hardware
// worked (IBM reserved IRQ2 for cascading to the second controller where it
// becomes IRQ9), so we translate IRQ2 to 9 and vice-versa on this API
// boundaries. This is also what DOSBox expects: it uses IRQ9 instead of IRQ2.

static constexpr uint8_t to_internal_irq(const uint8_t irq)
{
	assert(irq != 9);
	return irq == 2 ? 9 : irq;
}

static constexpr uint8_t to_external_irq(const uint8_t irq)
{
	assert(irq != 2);
	return irq == 9 ? 2 : irq;
}

static void gus_pic_callback()
{
	if (!gus || !gus->channel->is_enabled) {
		return;
	}

	gus->frame_counter += gus->channel->GetFramesPerTick();
	const auto requested_frames = ifloor(gus->frame_counter);
	gus->frame_counter -= static_cast<float>(requested_frames);
	gus->PicCallback(requested_frames);
}

Gus::Gus(const io_port_t port_pref, const uint8_t dma_pref, const uint8_t irq_pref,
         const char* ultradir, const std::string& filter_prefs)
        : ram(RAM_SIZE),
          dma2(dma_pref),
          irq1(to_internal_irq(irq_pref)),
          irq2(to_internal_irq(irq_pref))
{
	MIXER_LockMixerThread();

	// port operations are "zero-based" from the datum to the user's port
	constexpr io_port_t port_datum = 0x200;
	port_base                      = port_pref - port_datum;

	// Create the internal voice channels
	for (uint8_t i = 0; i < MAX_VOICES; ++i) {
		voices.emplace_back(i, voice_irq);
	}
	assert(voices.size() == MAX_VOICES);

	RegisterIoHandlers();

	constexpr bool Stereo      = true;
	constexpr bool SignedData  = true;
	constexpr bool NativeOrder = true;

	// Register the Audio and DMA channels
	const auto mixer_callback = std::bind(
	        MIXER_PullFromQueueCallback<Gus, AudioFrame, Stereo, SignedData, NativeOrder>,
	        std::placeholders::_1,
	        this);

	channel = MIXER_AddChannel(mixer_callback,
	                           UseMixerRate,
	                           ChannelName::GravisUltrasound,
	                           {ChannelFeature::Sleep,
	                            ChannelFeature::Stereo,
	                            ChannelFeature::ReverbSend,
	                            ChannelFeature::ChorusSend,
	                            ChannelFeature::DigitalAudio});

	assert(channel);

	// We render at the GUS' internal mixer rate, then ZOH upsample to
	// the native 44.1 kHz GUS rate. This emulates the behaviour of the
	// real GF1 chip which always outputs a 44.1 kHz sample stream to
	// the DAC, but starts dropping samples in the internal mixer above
	// 14 active voices due to bandwidth limitations. Technically, we
	// could emulate this exact behaviour, but in practice it would
	// make little to no difference compared to our current method.
	//
	channel->SetResampleMethod(ResampleMethod::ZeroOrderHoldAndResample);
	channel->SetZeroOrderHoldUpsamplerTargetRate(GusOutputSampleRate);

	// GUS is prone to accumulating beyond the 16-bit range so we scale back
	// by RMS.
	constexpr auto rms_squared = static_cast<float>(M_SQRT1_2);
	channel->Set0dbScalar(rms_squared);

	SetFilter(filter_prefs);

	ms_per_render = MillisInSecond / channel->GetSampleRate();

	UpdatePlaybackDmaAddress(dma_pref);
	UpdateRecordingDmaAddress(dma_pref);

	// Populate the volume, pan, and auto-exec arrays
	PopulateVolScalars();
	PopulatePanScalars();
	SetupEnvironment(port_pref, ultradir);

	output_queue.Resize(iceil(channel->GetFramesPerBlock() * 2.0f));
	TIMER_AddTickHandler(gus_pic_callback);

	LOG_MSG("GUS: Running on port %xh, IRQ %d, and DMA %d",
	        port_pref,
	        to_external_irq(irq1),
	        dma1);

	MIXER_UnlockMixerThread();
}

void Gus::SetFilter(const std::string& filter_prefs)
{
	// The filter parameters have been tweaked by analysing real hardware
	// recordings of the GUS Classic (GF1 chip).
	//
	auto enable_filter = [&]() {
		constexpr auto Order        = 1;
		constexpr auto CutoffFreqHz = 8000;

		channel->ConfigureLowPassFilter(Order, CutoffFreqHz);
		channel->SetLowPassFilter(FilterState::On);
	};

	if (const auto maybe_bool = parse_bool_setting(filter_prefs)) {
		if (*maybe_bool) {
			enable_filter();
		} else {
			channel->SetLowPassFilter(FilterState::Off);
		}
	} else if (!channel->TryParseAndSetCustomFilter(filter_prefs)) {
		NOTIFY_DisplayWarning(Notification::Source::Console,
		                      "GUS",
		                      "PROGRAM_CONFIG_INVALID_SETTING",
		                      "gus_filter",
		                      filter_prefs.c_str(),
		                      "on");

		set_section_property_value("gus", "gus_filter", "on");
		enable_filter();
	}
}

void Gus::ActivateVoices(uint8_t requested_voices)
{
	requested_voices = clamp(requested_voices, MIN_VOICES, MAX_VOICES);
	if (requested_voices != active_voices) {
		active_voices = requested_voices;
		assert(active_voices <= voices.size());
		active_voice_mask = 0xffffffffu >> (MAX_VOICES - active_voices);

		// Authentically emulate the playback rate degradation
		// dependent on the number of active voices (hardware
		// channels) of the original GF1 chip found on the GUS
		// Classic and MAX boards.
		//
		// The playback rate is 44.1 kHz up until 14 active voices,
		// then it linearly drops to 19,293 Hz with all 32 voices
		// enabled.
		//
		// Gravis' calculation to convert from number of active
		// voices to playback frame rate. Ref: UltraSound
		// Lowlevel ToolKit v2.22 (21 December 1994), pp. 3 of
		// 113.
		//
		sample_rate_hz = ifloor(1000000.0 / (1.619695497 * active_voices));

		ms_per_render = MillisInSecond / sample_rate_hz;

		channel->SetSampleRate(sample_rate_hz);
	}

	if (active_voices && prev_logged_voices != active_voices) {
		LOG_MSG("GUS: Activated %u voices at %d Hz",
		        active_voices,
		        sample_rate_hz);

		prev_logged_voices = active_voices;
	}
}

const std::vector<AudioFrame>& Gus::RenderFrames(const int num_requested_frames)
{
	// Size and zero the vector
	rendered_frames.resize(check_cast<size_t>(num_requested_frames));
	for (auto& frame : rendered_frames) {
		frame = {0.0f, 0.0f};
	}

	if (reset_register.is_running && reset_register.is_dac_enabled) {
		auto voice            = voices.begin();
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
	assert(channel);
	if (channel->WakeUp()) {
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

void Gus::PicCallback(const int num_requested_frames)
{
	assert(channel);

#if 0
	if (fifo.size())
		LOG_MSG("GUS: Queued %2lu cycle-accurate frames", fifo.size());
#endif

	auto num_frames_remaining = num_requested_frames;

	// First, send any frames we've queued since the last callback
	while (num_frames_remaining && fifo.size()) {
		AudioFrame frame = fifo.front();
		fifo.pop();
		output_queue.NonblockingEnqueue(std::move(frame));
		--num_frames_remaining;
	}
	// If the queue's run dry, render the remainder and sync-up our time datum
	if (num_frames_remaining > 0) {
		auto frames = RenderFrames(num_frames_remaining);
		output_queue.NonblockingBulkEnqueue(frames, num_frames_remaining);
	}
	last_rendered_ms = PIC_FullIndex();
}

void Gus::CheckIrq()
{
	const bool should_interrupt = irq_status &
	                              (reset_register.are_irqs_enabled ? 0xff : 0x9f);

	if (should_interrupt && mix_control_register.latches_enabled) {
		PIC_ActivateIRQ(irq1);
	} else if (irq_previously_interrupted) {
		PIC_DeActivateIRQ(irq1);
	}

#if LOG_GUS
	const auto state_str = (should_interrupt && mix_control_register.latches_enabled)
	                             ? "activated"
	                     : irq_previously_interrupted ? "deactivated"
	                                                  : "unchanged";
	LOG_MSG("GUS: CheckIrq: IRQ %s (should_interrupt: %d, latches: %d)",
	        state_str,
	        should_interrupt,
	        mix_control_register.latches_enabled);
#endif

	irq_previously_interrupted = should_interrupt;
}

bool Gus::CheckTimer(const size_t t)
{
	auto& timer = t == 0 ? timer_one : timer_two;
	if (!timer.is_masked) {
		timer.has_expired = true;
	}
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
	if (voice_irq.vol_state) {
		irq_status |= 0x40;
	}
	if (voice_irq.wave_state) {
		irq_status |= 0x20;
	}
	CheckIrq();
	while (!(totalmask & 1ULL << voice_irq.status)) {
		voice_irq.status++;
		if (voice_irq.status >= active_voices) {
			voice_irq.status = 0;
		}
	}
}

// Returns a 20-bit offset into the GUS's memory space holding the next
// DMA sample that will be read or written to via DMA. This offset
// is derived from the 16-bit DMA address register.
uint32_t Gus::GetDmaOffset() noexcept
{
	uint32_t adjusted;
	if (IsDmaXfer16Bit()) {
		const auto upper = dma_addr & 0b1100'0000'0000'0000;
		const auto lower = dma_addr & 0b0001'1111'1111'1111;
		adjusted         = static_cast<uint32_t>(upper | (lower << 1));
	} else {
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
		adjusted         = upper | (lower >> 1);
	} else {
		// Take the top 16 bits from the 20 bit address
		adjusted = offset & 0b1111'1111'1111'1111'0000;
	}
	// pack it into the 16-bit register
	dma_addr = static_cast<uint16_t>(adjusted >> 4);

	// hang onto the last nibble
	dma_addr_nibble = check_cast<uint8_t>(adjusted & 0xf);
}

template <SampleSize sample_size>
bool Gus::SizedDmaTransfer()
{
	if (dma_channel->is_masked || !dma_control_register.is_enabled) {
		return false;
	}

#if LOG_GUS
	LOG_MSG("GUS DMA event: max %u bytes. DMA: tc=%u mask=0 cnt=%u",
	        BYTES_PER_DMA_XFER,
	        dma_channel->has_reached_terminal_count ? 1 : 0,
	        static_cast<uint32_t>(dma_channel->curr_count + 1));
#endif

	// Get the current DMA offset relative to the block of GUS memory
	const auto offset = GetDmaOffset();

	// Get the pending DMA count from channel
	const uint32_t desired = dma_channel->curr_count + 1;

	// Will the maximum transfer stay within the GUS RAM's size?
	assert(static_cast<size_t>(offset) + desired <= ram.size());

	// Perform the DMA transfer
	const auto transfered = dma_control_register.is_direction_gus_to_host
	                              ? dma_channel->Write(desired, &ram.at(offset))
	                              : dma_channel->Read(desired, &ram.at(offset));

	// Did we get everything we asked for?
	assert(transfered == desired);

	// scale the transfer by the DMA channel's bit-depth
	const auto bytes_transfered = transfered * (dma_channel->is_16bit + 1u);

	// Update the GUS's DMA address with the current position
	UpdateDmaAddr(check_cast<uint32_t>(offset + bytes_transfered));

	// If requested, invert the loaded samples' most-significant bits
	if (!dma_control_register.is_direction_gus_to_host &&
	    dma_control_register.are_samples_high_bit_inverted) {
		auto ram_pos           = ram.begin() + offset;
		const auto ram_pos_end = ram_pos + bytes_transfered;

		// adjust our start and skip size if handling 16-bit PCM samples
		ram_pos += (sample_size == SampleSize::Bits16) ? 1 : 0;
		const auto skip = (sample_size == SampleSize::Bits16) ? 2 : 1;

		assert(ram_pos >= ram.begin() && ram_pos <= ram_pos_end &&
		       ram_pos_end <= ram.end());
		while (ram_pos < ram_pos_end) {
			*ram_pos ^= 0x80;
			ram_pos += skip;
		}
	}

	if (dma_channel->has_reached_terminal_count) {
		dma_control_register.has_pending_terminal_count_irq = true;

		if (dma_control_register.wants_irq_on_terminal_count) {
			irq_status |= 0x80;
			CheckIrq();
		}
		return false;
	}
	return true;
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
	return (dma_control_register.is_channel_16bit && dma1 >= 4);
}

static void gus_dma_event(uint32_t)
{
	if (gus->PerformDmaTransfer()) {
		PIC_AddEvent(gus_dma_event, MS_PER_DMA_XFER);
	}
}

void Gus::StartDmaTransfers()
{
	PIC_RemoveEvents(gus_dma_event);
	PIC_AddEvent(gus_dma_event, MS_PER_DMA_XFER);
}

void Gus::DmaCallback(const DmaChannel*, DmaEvent event)
{
	if (event == DmaEvent::IsUnmasked) {
		StartDmaTransfers();
	}
}

void Gus::SetupEnvironment(uint16_t port, const char* ultradir_env_val)
{
	// Ensure our port and addresses will fit in our format widths
	// The config selection controls their actual values, so this is a
	// maximum-limit.
	assert(port < 0xfff);

	// ULTRASND variable
	char ultrasnd_env_val[] = "HHH,D,D,II,II";
	safe_sprintf(ultrasnd_env_val,
	             "%x,%u,%u,%u,%u",
	             port,
	             dma1,
	             dma2,
	             to_external_irq(irq1),
	             to_external_irq(irq2));
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
	double scalar                       = 1.0;
	auto volume                         = vol_scalars.end();
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
	int i           = 0;
	auto pan_scalar = pan_scalars.begin();
	while (pan_scalar != pan_scalars.end()) {
		// Normalize absolute range [0, 15] to [-1.0, 1.0]
		const auto norm = (i - 7.0) / (i < 7 ? 7 : 8);
		// Convert to an angle between 0 and 90-degree, in radians
		const auto angle  = (norm + 1) * M_PI / 4;
		pan_scalar->left  = static_cast<float>(cos(angle));
		pan_scalar->right = static_cast<float>(sin(angle));
		++pan_scalar;
		++i;

		// LOG_DEBUG("GUS: pan_scalar[%u] = %f | %f",
		//          i,
		//          pan_scalar->left,
		//          pan_scalar->right);
	}
}

void Gus::MirrorAdLibCommandRegister(const uint8_t reg_value)
{
	adlib_command_reg = reg_value;
}

void Gus::PrintStats()
{
	// Aggregate stats from all voices
	uint32_t combined_8bit_ms  = 0;
	uint32_t combined_16bit_ms = 0;
	uint32_t used_8bit_voices  = 0;
	uint32_t used_16bit_voices = 0;
	for (const auto& voice : voices) {
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
	if (combined_ms < 10000u || !(used_8bit_voices + used_16bit_voices)) {
		return;
	}

	// Print info about the type of audio and voices used
	if (used_16bit_voices == 0u) {
		LOG_MSG("GUS: Audio comprised of 8-bit samples from %u voices",
		        used_8bit_voices);
	} else if (used_8bit_voices == 0u) {
		LOG_MSG("GUS: Audio comprised of 16-bit samples from %u voices",
		        used_16bit_voices);
	} else {
		const auto ratio_8bit  = ceil_udivide(100u * combined_8bit_ms,
                                                     combined_ms);
		const auto ratio_16bit = ceil_udivide(100u * combined_16bit_ms,
		                                      combined_ms);
		LOG_MSG("GUS: Audio was made up of %u%% 8-bit %u-voice and "
		        "%u%% 16-bit %u-voice samples",
		        ratio_8bit,
		        used_8bit_voices,
		        ratio_16bit,
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
		time = 0;
		if (timer_one.has_expired) {
			time |= (1 << 6);
		}
		if (timer_two.has_expired) {
			time |= 1 << 5;
		}
		if (time & 0x60) {
			time |= 1 << 7;
		}
		if (irq_status & 0x04) {
			time |= 1 << 2;
		}
		if (irq_status & 0x08) {
			time |= 1 << 1;
		}
		return time;
	case 0x20a: return adlib_command_reg;
	case 0x302: return static_cast<uint8_t>(voice_index);
	case 0x303: return selected_register;
	case 0x304:
		if (width == io_width_t::word) {
			return ReadFromRegister() & 0xffff;
		} else {
			return ReadFromRegister() & 0xff;
		}
	case 0x305: return ReadFromRegister() >> 8;
	case 0x307: return dram_addr < ram.size() ? ram.at(dram_addr) : 0;
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
		reg = dma_control_register.data;
		dma_control_register.has_pending_terminal_count_irq = false;
		irq_status &= 0x7f;
		CheckIrq();
		return static_cast<uint16_t>(reg << 8);
	case 0x42: // DMA address register
		return dma_addr;
	case 0x45: // Timer control register matches Adlib's behavior
		return static_cast<uint16_t>(timer_ctrl << 8);
	case 0x49: // DMA sample register
		reg = dma_control_register.data;
		return static_cast<uint16_t>(reg << 8);
	case 0x4c: // Reset register
		return static_cast<uint16_t>(reset_register.data << 8);
	case 0x8f: // General voice IRQ status register
		reg = voice_irq.status | 0x20;
		uint32_t mask;
		mask = 1 << voice_irq.status;
		if (!(voice_irq.vol_state & mask)) {
			reg |= 0x40;
		}
		if (!(voice_irq.wave_state & mask)) {
			reg |= 0x80;
		}
		voice_irq.vol_state &= ~mask;
		voice_irq.wave_state &= ~mask;
		CheckVoiceIrq();
		return static_cast<uint16_t>(reg << 8);
	default:
		break;
		// If the above weren't triggered, then fall-through
		// to the voice-specific register switch below.
	}

	if (!target_voice) {
		return (selected_register == 0x80 || selected_register == 0x8d)
		             ? 0x0300
		             : 0;
	}

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

static void gus_timer_event(uint32_t t)
{
	if (gus->CheckTimer(t)) {
		const auto& timer = t == 0 ? gus->timer_one : gus->timer_two;
		PIC_AddEvent(gus_timer_event, timer.delay, t);
	}
}

void Gus::Reset() noexcept
{
	// Halt playback before altering the DSP state
	channel->Enable(false);

	irq_status                 = 0;
	irq_previously_interrupted = false;

	// Reset the OPL emulator state
	adlib_command_reg = ADLIB_CMD_DEFAULT;

	dma_control_register.data = {};

	sample_ctrl = 0;

	timer_ctrl = 0;
	timer_one  = Timer{TIMER_1_DEFAULT_DELAY};
	timer_two  = Timer{TIMER_2_DEFAULT_DELAY};

	// Reset the voice states
	for (auto& voice : voices) {
		voice.ResetCtrls();
	}
	voice_irq     = VoiceIrq{};
	target_voice  = nullptr;
	voice_index   = 0;
	active_voices = 0;

	UpdateDmaAddr(0);
	dram_addr             = 0;
	register_data         = 0;
	selected_register     = 0;
	should_change_irq_dma = false;
	PIC_RemoveEvents(gus_timer_event);

	reset_register.data       = {};
	mix_control_register.data = MixControlRegisterDefaultState;
}

static void gus_evict(Section* sec);

void Gus::UpdateRecordingDmaAddress(const uint8_t new_address)
{
	dma2 = new_address;

	// TODO: Populate when we have audio input writing to a DMA channel

#if LOG_GUS
	LOG_MSG("GUS: Assigned recording DMA address to %u", dma2);
#endif
}

void Gus::UpdatePlaybackDmaAddress(const uint8_t new_address)
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
	dma1        = new_address;
	dma_channel = DMA_GetChannel(dma1);
	assert(dma_channel);

	dma_channel->ReserveFor(ChannelName::GravisUltrasound, gus_evict);
	dma_channel->RegisterCallback(std::bind(&Gus::DmaCallback, this, _1, _2));

#if LOG_GUS
	LOG_MSG("GUS: Assigned playback DMA address to %u", dma1);
#endif
}

void Gus::WriteToPort(io_port_t port, io_val_t value, io_width_t width)
{
	RenderUpToNow();

	const auto val = check_cast<uint16_t>(value);

	//	LOG_MSG("GUS: Write to port %x val %x", port, val);
	switch (port - port_base) {
	case 0x200:
		mix_control_register.data = static_cast<uint8_t>(val);
		should_change_irq_dma     = true;
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
				PIC_AddEvent(gus_timer_event, timer_one.delay, 0);
				timer_one.is_counting_down = true;
			}
		} else {
			timer_one.is_counting_down = false;
		}
		if (val & 0x2) {
			if (!timer_two.is_counting_down) {
				PIC_AddEvent(gus_timer_event, timer_two.delay, 1);
				timer_two.is_counting_down = true;
			}
		} else {
			timer_two.is_counting_down = false;
		}
		break;
		// TODO Check if 0x20a register is also available on the gus
		// like on the interwave
	case 0x20b:
		if (should_change_irq_dma) {

			//  The write to 2XB MUST occur as the NEXT IOW or else
			//  the write to 2XB will be locked out and not occur.
			//  This is to prevent an application that is probing
			//  for cards from accidentally corrupting the latches.
			//  UltraSound Software Development Kit (SDK),
			//  Section 2.13.
			should_change_irq_dma = false;

			const auto address_select = AddressSelectRegister{
			        static_cast<uint8_t>(val)};

			const auto ch1_selector = address_select.channel1_selector;

			const auto ch2_selector = address_select.channel2_selector;

			if (mix_control_register.irq_control_selected) {

				// Application is selecting IRQ addresses
				if (ch1_selector &&
				    ch1_selector < IrqAddresses.size()) {
					irq1 = to_internal_irq(
					        IrqAddresses[ch1_selector]);
				}

				if (address_select.channel2_combined_with_channel1) {
					// Channel 2 can be combined if it's
					// selector is 0
					if (ch2_selector == 0) {
						irq2 = irq1;
					}
				} else if (ch2_selector &&
				           ch2_selector < IrqAddresses.size()) {
					irq2 = to_internal_irq(
					        IrqAddresses[ch2_selector]);
				}
#if LOG_GUS
				LOG_MSG("GUS: Assigned GF1 IRQ to %d and MIDI IRQ to %d",
				        irq1,
				        irq2);
#endif
			} else {

				// Application is selecting DMA addresses
				if (ch1_selector &&
				    ch1_selector < DmaAddresses.size()) {
					UpdatePlaybackDmaAddress(
					        DmaAddresses[ch1_selector]);
				}

				if (address_select.channel2_combined_with_channel1) {
					// Channel 2 can be combined if it's
					// selector is 0
					if (ch2_selector == 0) {
						UpdateRecordingDmaAddress(dma1);
					}
				} else if (ch2_selector &&
				           ch2_selector < DmaAddresses.size()) {
					UpdateRecordingDmaAddress(
					        DmaAddresses[ch2_selector]);
				}
			}
		}
		break;
	case 0x302:
		voice_index  = val & 31;
		target_voice = &voices.at(voice_index);
		break;
	case 0x303:
		selected_register = static_cast<uint8_t>(val);
		register_data     = 0;
		break;
	case 0x304:
		if (width == io_width_t::word) {
			register_data = val;
			WriteToRegister();
		} else {
			register_data = val;
		}
		break;
	case 0x305:
		register_data = static_cast<uint16_t>((0x00ff & register_data) |
		                                      val << 8);
		WriteToRegister();
		break;
	case 0x307:
		if (dram_addr < ram.size()) {
			ram.at(dram_addr) = static_cast<uint8_t>(val);
		}
		break;
	default:
#if LOG_GUS
		LOG_MSG("GUS: Write to port %#x with value %x", port, val);
#endif
		break;
	}
}

void Gus::UpdateWaveLsw(int32_t& addr) const noexcept
{
	constexpr auto WAVE_LSW_MASK = ~((1 << 16) - 1); // Lower wave mask
	const auto lower             = addr & WAVE_LSW_MASK;
	addr                         = lower | register_data;
}

void Gus::UpdateWaveMsw(int32_t& addr) const noexcept
{
	constexpr auto WAVE_MSW_MASK = (1 << 16) - 1; // Upper wave mask
	const auto upper             = register_data & 0x1fff;
	const auto lower             = addr & WAVE_MSW_MASK;
	addr                         = lower | (upper << 16);
}

void Gus::WriteToRegister()
{
	RenderUpToNow();

	// Registers that write to the general DSP
	switch (selected_register) {
	case 0xe: // Set number of active voices
		selected_register = register_data >> 8; // Jazz Jackrabbit needs
		                                        // this
		{
			const uint8_t num_voices = 1 + ((register_data >> 8) & 31);
			ActivateVoices(num_voices);
		}
		return;
	case 0x10: // Undocumented register used in Fast Tracker 2
		return;
	case 0x41: // DMA control register
		dma_control_register.data = static_cast<uint8_t>(register_data >> 8);

		// This is the only place where the application tells the GUS if
		// the incoming DMA samples are 16-bit or 8-bit. It's a one-shot
		// write in bit 6 that can't be read back because this bit takes
		// on a different meaning when reading the DMA control register.
		//
		PerformDmaTransfer = std::bind(
		        dma_control_register.are_samples_16bit
		                ? &Gus::SizedDmaTransfer<SampleSize::Bits16>
		                : &Gus::SizedDmaTransfer<SampleSize::Bits8>,
		        this);

		if (dma_control_register.is_enabled) {
			StartDmaTransfers();
		}
		return;
	case 0x42: // Gravis DRAM DMA address register
		dma_addr        = register_data;
		dma_addr_nibble = 0; // invalidate the nibble
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
		if (!timer_one.should_raise_irq) {
			irq_status &= ~0x04;
		}
		timer_two.should_raise_irq = (timer_ctrl & 0x08) > 0;
		if (!timer_two.should_raise_irq) {
			irq_status &= ~0x08;
		}
		if (!timer_one.should_raise_irq && !timer_two.should_raise_irq) {
			CheckIrq();
		}
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
		if (sample_ctrl & 1) {
			StartDmaTransfers();
		}
		return;
	case 0x4c: // Reset register
		reset_register.data = static_cast<uint8_t>(register_data >> 8);
		if (!reset_register.is_running) {
			Reset();
		}
		return;
	default:
		break;
		// If the above weren't triggered, then fall-through
		// to the target_voice-specific switch below.
	}

	// All the registers below operated on the target voice
	if (!target_voice) {
		return;
	}

	uint8_t data = 0;
	// Registers that write to the current voice
	switch (selected_register) {
	case 0x0: // Voice wave control register
		if (target_voice->UpdateWaveState(register_data >> 8)) {
			CheckVoiceIrq();
		}
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
		if (target_voice->UpdateVolState(register_data >> 8)) {
			CheckVoiceIrq();
		}
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

	MIXER_LockMixerThread();

	Reset();

	// Prevent discovery of the GUS via the environment
	ClearEnvironment();

	// Stop the game from accessing the IO ports
	for (auto& rh : read_handlers) {
		rh.Uninstall();
	}
	for (auto& wh : write_handlers) {
		wh.Uninstall();
	}

	// Deregister the mixer channel, after which it's cleaned up
	assert(channel);
	MIXER_DeregisterChannel(channel);

	// Deregister the DMA source once the mixer channel is gone, which can
	// pull samples from DMA.
	if (dma_channel) {
		dma_channel->Reset();
	}

	TIMER_DelTickHandler(gus_pic_callback);

	MIXER_UnlockMixerThread();
}

void GUS_MirrorAdLibCommandPortWrite([[maybe_unused]] const io_port_t port,
                                     const io_val_t reg_value, const io_width_t)
{
	// We must only be fed values from the AdLib's command port
	assert(port == Port::AdLib::Command);

	if (gus) {
		gus->MirrorAdLibCommandRegister(check_cast<uint8_t>(reg_value));
	}
}

void GUS_NotifyLockMixer()
{
	if (gus) {
		gus->output_queue.Stop();
	}
}
void GUS_NotifyUnlockMixer()
{
	if (gus) {
		gus->output_queue.Start();
	}
}

static void init_gus_config_settings(SectionProp& secprop)
{
	constexpr auto when_idle = Property::Changeable::WhenIdle;

	auto* bool_prop = secprop.AddBool("gus", when_idle, false);
	assert(bool_prop);
	bool_prop->SetHelp(
	        "Enable Gravis UltraSound emulation ('off' by default). Many games and all demos\n"
	        "upload their own sounds, but some rely on the instrument patch files included\n"
	        "with the GUS for MIDI playback (see 'ultradir' for details). Some games also\n"
	        "require ULTRAMID.EXE to be loaded prior to starting the game.\n"
	        "\n"
	        "Note: The default settings of base address 240, IRQ 5, and DMA 3 have been\n"
	        "      chosen so the GUS can coexist with a Sound Blaster card. This works fine\n"
	        "      for the majority of programs, but some games and demos expect the GUS\n"
	        "      factory defaults of base address 220, IRQ 11, and DMA 1. The default\n"
	        "      IRQ 11 is also problematic with specific versions of the DOS4GW extender\n"
	        "      that cannot handle IRQs above 7.");

	auto* hex_prop = secprop.AddHex("gusbase", when_idle, 0x240);
	assert(hex_prop);
	hex_prop->SetValues({"210", "220", "230", "240", "250", "260"});
	hex_prop->SetHelp(
	        "The IO base address of the Gravis UltraSound (240 by default).\n"
	        "Possible values: 210, 220, 230, 240, 250, 260");

	auto* int_prop = secprop.AddInt("gusirq", when_idle, 5);
	assert(int_prop);
	int_prop->SetValues({"2", "3", "5", "7", "11", "12", "15"});
	int_prop->SetHelp(
	        "The IRQ number of the Gravis UltraSound (5 by default).\n"
	        "Possible values: 2, 3, 5, 7, 11, 12, 15");

	int_prop = secprop.AddInt("gusdma", when_idle, 3);
	assert(int_prop);
	int_prop->SetValues({"1", "3", "5", "6", "7"});
	int_prop->SetHelp(
	        "The DMA channel of the Gravis UltraSound (3 by default).\n"
	        "Possible values: 1, 3, 5, 6, 7");

	auto* str_prop = secprop.AddString("gus_filter", when_idle, "on");
	assert(str_prop);
	str_prop->SetHelp(
	        "Filter for the Gravis UltraSound audio output ('on' by default).\n"
	        "Possible values:\n"
	        "\n"
	        "  on:        Filter the output (default).\n"
	        "  off:       Don't filter the output.\n"
	        "  <custom>:  Custom filter definition; see 'sb_filter' for details.");

	str_prop = secprop.AddString("ultradir", when_idle, "C:\\ULTRASND");
	assert(str_prop);
	str_prop->SetHelp(
	        "Path to the UltraSound directory ('C:\\ULTRASND' by default). This should have a\n"
	        "'MIDI' subdirectory containing the patches (instrument files) required by some\n"
	        "games for MIDI music playback. Not all games need these patches; many GUS-native\n"
	        "games and all demos upload their own custom sounds instead.");
}

void GUS_Init()
{
	const auto section = get_section("gus");
	if (!section->GetBool("gus")) {
		return;
	}

	// Read the GUS config settings
	const auto port = static_cast<uint16_t>(section->GetHex("gusbase"));

	const auto dma = static_cast<uint8_t>(section->GetInt("gusdma"));
	// The section system handles invalid settings, so just assert validity
	assert(contains(DmaAddresses, dma));

	auto irq = static_cast<uint8_t>(section->GetInt("gusirq"));
	// The section system handles invalid settings, so just assert validity
	assert(contains(IrqAddresses, irq));

	const std::string ultradir = section->GetString("ultradir");

	const std::string filter_prefs = section->GetString("gus_filter");

	// Instantiate the GUS with the settings
	gus = std::make_unique<Gus>(port, dma, irq, ultradir.c_str(), filter_prefs);
}

void GUS_Destroy()
{
	// GUS destroy is run when the user wants to deactivate the GUS:
	// C:\> config -set gus=false
	// TODO: therefore, this function should also remove the
	//       ULTRASND and ULTRADIR environment variables.

	if (gus) {
		MIXER_LockMixerThread();

		gus->PrintStats();
		gus.reset();

		MIXER_UnlockMixerThread();
	}
}

static void gus_evict([[maybe_unused]] Section* sec)
{
	GUS_Destroy();
	set_section_property_value("gus", "gus", "off");
}

static void notify_gus_setting_updated(SectionProp& section,
                                       const std::string& prop_name)
{
	if (prop_name == "gus_filter") {
		if (gus) {
			gus->SetFilter(section.GetString("gus_filter"));
		}

	} else {
		GUS_Destroy();
		GUS_Init();
	}
}

void GUS_AddConfigSection(const ConfigPtr& conf)
{
	assert(conf);

	auto section = conf->AddSection("gus");
	section->AddUpdateHandler(notify_gus_setting_updated);

	init_gus_config_settings(*section);
}
