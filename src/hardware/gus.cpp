/*
 *  Copyright (C) 2020-2020  The dosbox-staging team
 *  Copyright (C) 2002-2020  The DOSBox Team
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

#include "gus.h"

#include <iomanip>
#include <sstream>
#include <unistd.h>

#include "setup.h"

#define LOG_GUS 0 // set to 1 for detailed logging

// GUS constants used in functional code
constexpr uint8_t ADLIB_CMD_DEFAULT = 85u;
constexpr float AUDIO_SAMPLE_MAX = static_cast<float>(MAX_AUDIO);
constexpr float AUDIO_SAMPLE_MIN = static_cast<float>(MIN_AUDIO);
constexpr uint32_t BYTES_PER_DMA_XFER = 8 * 1024; // 8 KB per transfer
constexpr auto DELTA_DB = 0.002709201;             // 0.0235 dB increments
constexpr uint32_t ISA_BUS_THROUGHPUT = 32 * 1024 * 1024; // 32 MB/s
constexpr uint16_t DMA_TRANSFERS_PER_S = ISA_BUS_THROUGHPUT / BYTES_PER_DMA_XFER;
constexpr uint8_t MIN_VOICES = 14u;
constexpr float MS_PER_DMA_XFER = 1000.0f / DMA_TRANSFERS_PER_S;
constexpr auto SOFT_LIMIT_RELEASE_INC = AUDIO_SAMPLE_MAX *
                                        static_cast<float>(DELTA_DB);
constexpr int16_t VOLUME_INC_SCALAR = 512; // Volume index increment scalar
constexpr auto VOLUME_LEVEL_DIVISOR = 1.0 + DELTA_DB;
constexpr int16_t WAVE_WIDTH = 1 << 9; // Wave interpolation width (9 bits)
constexpr float WAVE_WIDTH_INV = 1.0f / WAVE_WIDTH;
constexpr uint32_t WAVE_MSW_MASK = (1 << 16) - 1; // Upper wave mask
constexpr uint32_t WAVE_LSW_MASK = 0xffffffff ^ WAVE_MSW_MASK; // Lower wave mask

using namespace std::placeholders;

// External Tie-in for OPL FM-audio
uint8_t adlib_commandreg = ADLIB_CMD_DEFAULT;

static std::unique_ptr<Gus> gus = nullptr;

Voice::Voice(uint8_t num, VoiceIrq &irq)
        : irq_mask(1 << num),
		shared_irq(irq)
          
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
bool Voice::CheckWaveRolloverCondition()
{
	return (vol_ctrl.state & CTRL::BIT16) && !(wave_ctrl.state & CTRL::LOOP);
}

bool Voice::Is8Bit() const
{
	return !(wave_ctrl.state & CTRL::BIT16);
}

float Voice::GetSample(const uint8_t *ram) const
{
	const int32_t addr = sample_address;
	return Is8Bit() ? Read8BitSample(ram, addr) : Read16BitSample(ram, addr);
}

float Voice::GetInterWavePercent() const
{
	const auto wave_fraction = wave_ctrl.pos & (WAVE_WIDTH - 1);
	return static_cast<float>(wave_fraction) * WAVE_WIDTH_INV;
}

float Voice::GetInterWavePortion(const uint8_t *ram, const float sample) const
{
	float portion = 0;
	if (wave_ctrl.inc < WAVE_WIDTH) {
		const int32_t addr = sample_address + 1;
		const float next_sample = Is8Bit() ? Read8BitSample(ram, addr)
		                                   : Read16BitSample(ram, addr);
		portion = (next_sample - sample) * GetInterWavePercent();
	}
	return portion;
}

void Voice::IncrementAddress()
{
	IncrementControl(wave_ctrl, CheckWaveRolloverCondition());
	sample_address = wave_ctrl.pos / WAVE_WIDTH;
}

void Voice::IncrementVolScalar(const float *vol_scalars)
{
	IncrementControl(vol_ctrl, false);
	// Unscale the volume index and check its bounds
	const auto i = static_cast<size_t>(
	        ceil_sdivide(vol_ctrl.pos, VOLUME_INC_SCALAR));
	assert(i < VOLUME_LEVELS);
	sample_vol_scalar = vol_scalars[i];
}

void Voice::GenerateSamples(float *stream,
                            const uint8_t *ram,
                            const float *vol_scalars,
                            const AudioFrame *pan_scalars,
                            const int requested_frames)
{
	if (vol_ctrl.state & wave_ctrl.state & CTRL::DISABLED)
		return;

	// Add the samples to the stream, angled in L-R space
	const int sample_end = requested_frames * 2 - 1;
	for (int i = 0; i < sample_end; i += 2) {
		float sample = GetSample(ram);
		sample += GetInterWavePortion(ram, sample);
		assert(sample <= AUDIO_SAMPLE_MAX && sample >= AUDIO_SAMPLE_MIN);
		sample *= sample_vol_scalar;
		stream[i] += sample * pan_scalars[pan_position].left;
		stream[i + 1] += sample * pan_scalars[pan_position].right;
		IncrementAddress();
		IncrementVolScalar(vol_scalars);
	}
	// Keep track of how many ms this voice has generated
	Is8Bit() ? generated_8bit_ms++ : generated_16bit_ms++;
}

// Read an 8-bit sample scaled into the 16-bit range, returned as a float
float Voice::Read8BitSample(const uint8_t *ram, const int32_t addr) const
{
	constexpr float to_16bit_range = 1u
	                                 << (std::numeric_limits<int16_t>::digits -
	                                     std::numeric_limits<int8_t>::digits);
	const size_t i = static_cast<uint32_t>(addr) & 0xFFFFFu;
	assert(i < RAM_SIZE);
	return static_cast<int8_t>(ram[i]) * to_16bit_range;
}

// Read a 16-bit sample returned as a float
float Voice::Read16BitSample(const uint8_t *ram, const int32_t addr) const
{
	// Calculate offset of the 16-bit sample
	const auto lower = static_cast<unsigned>(addr) & 0xC0000u;
	const auto upper = static_cast<unsigned>(addr) & 0x1FFFFu;
	const size_t i = lower | (upper << 1);
	assert(i < RAM_SIZE);
	return static_cast<int16_t>(host_readw(ram + i));
}

uint8_t Voice::ReadPanPot() const
{
	return pan_position;
}

int32_t Voice::IncrementControl(VoiceControl &ctrl, bool dont_loop_or_restart)
{
	if (ctrl.state & CTRL::DISABLED)
		return ctrl.pos;
	int32_t remaining;
	if (ctrl.state & CTRL::DECREASING) {
		ctrl.pos -= ctrl.inc;
		remaining = ctrl.start - ctrl.pos;
	} else {
		ctrl.pos += ctrl.inc;
		remaining = ctrl.pos - ctrl.end;
	}
	// Not yet reaching a boundary
	if (remaining < 0)
		return ctrl.pos;

	// Generate an IRQ if requested
	if (ctrl.state & CTRL::RAISEIRQ) {
		shared_irq.state |= irq_mask;
	}

	// Allow the current position to move beyond its limit
	if (dont_loop_or_restart)
		return ctrl.pos;

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
	return ctrl.pos;
}

void Voice::WritePanPot(uint8_t pos)
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

void Voice::WriteVolRate(uint16_t val)
{
	vol_ctrl.rate = val;
	constexpr uint8_t bank_lengths = 63;
	const int pos_in_bank = val & bank_lengths;
	const int decimator = 1 << (3 * (val >> 6));
	vol_ctrl.inc = ceil_sdivide(pos_in_bank * VOLUME_INC_SCALAR, decimator);

	// Sanity check the bounds of the incrementer
	assert(vol_ctrl.inc >= 0 && vol_ctrl.inc <= bank_lengths * VOLUME_INC_SCALAR);
}

void Gus::WriteCtrl(VoiceControl &ctrl, uint32_t voice_irq_mask, uint8_t val)
{
	const uint32_t prev_state = voice_irq.state;
	ctrl.state = val & 0x7f;
	// Manually set the irq
	if ((val & 0xa0) == 0xa0)
		voice_irq.state |= voice_irq_mask;
	else
		voice_irq.state &= ~voice_irq_mask;
	if (prev_state != voice_irq.state)
		CheckVoiceIrq();
}

void Voice::WriteWaveRate(uint16_t val)
{
	wave_ctrl.rate = val;
	wave_ctrl.inc = ceil_udivide(val, 2u);
}

void Gus::RegisterIoHandlers()
{
	// Register the IO read addresses
	assert(7 < read_handlers.size());
	const auto read_from = std::bind(&Gus::ReadFromPort, this, _1, _2);
	read_handlers[0].Install(0x302 + port_base, read_from, IO_MB);
	read_handlers[1].Install(0x303 + port_base, read_from, IO_MB);
	read_handlers[2].Install(0x304 + port_base, read_from, IO_MB | IO_MW);
	read_handlers[3].Install(0x305 + port_base, read_from, IO_MB);
	read_handlers[4].Install(0x206 + port_base, read_from, IO_MB);
	read_handlers[5].Install(0x208 + port_base, read_from, IO_MB);
	read_handlers[6].Install(0x307 + port_base, read_from, IO_MB);
	// Board Only
	read_handlers[7].Install(0x20A + port_base, read_from, IO_MB);

	// Register the IO write addresses
	// We'll leave the MIDI interface to the MPU-401
	// Ditto for the Joystick
	// GF1 Synthesizer
	assert(8 < write_handlers.size());
	const auto write_to = std::bind(&Gus::WriteToPort, this, _1, _2, _3);
	write_handlers[0].Install(0x302 + port_base, write_to, IO_MB);
	write_handlers[1].Install(0x303 + port_base, write_to, IO_MB);
	write_handlers[2].Install(0x304 + port_base, write_to, IO_MB | IO_MW);
	write_handlers[3].Install(0x305 + port_base, write_to, IO_MB);
	write_handlers[4].Install(0x208 + port_base, write_to, IO_MB);
	write_handlers[5].Install(0x209 + port_base, write_to, IO_MB);
	write_handlers[6].Install(0x307 + port_base, write_to, IO_MB);
	// Board Only
	write_handlers[7].Install(0x200 + port_base, write_to, IO_MB);
	write_handlers[8].Install(0x20B + port_base, write_to, IO_MB);
}

Gus::Gus(uint16_t port, uint8_t dma, uint8_t irq, const std::string &ultradir)
        : dma1(dma),
          port_base(port - 0x200),
          dma2(dma),
          irq1(irq),
          irq2(irq)
{
	// Create the internal voice channels
	for (uint8_t i = 0; i < MAX_VOICES; ++i) {
		voices.at(i) = std::make_unique<Voice>(i, voice_irq);
	}

	RegisterIoHandlers();

	// Register the Audio and DMA callbacks
	audio_channel = mixer_channel.Install(
		std::bind(&Gus::AudioCallback, this, std::placeholders::_1), 1, "GUS");

	GetDMAChannel(dma1)->Register_Callback(
	        std::bind(&Gus::GUS_DMA_Callback, this, _1, _2));

	// Populate the volume, pan, and auto-exec arrays
	PopulateVolScalars();
	PopulatePanScalars();
	PopulateAutoExec(port, ultradir);
}

void Gus::AudioCallback(const uint16_t requested_frames)
{
	assert(requested_frames <= BUFFER_FRAMES);

	// Zero the accumulator array
	for (int i = 0; i < BUFFER_SAMPLES; ++i) // vectorized
		accumulator[i] = 0;

	for (uint8_t i = 0; i < active_voices; ++i)
		voices[i]->GenerateSamples(accumulator, ram, vol_scalars,
		                           pan_scalars, requested_frames);

	SoftLimit(accumulator, scaled);
	audio_channel->AddSamples_s16(requested_frames, scaled);
	CheckVoiceIrq();
}

void Gus::CheckIrq()
{
	if (irq_status && (mix_ctrl & 0x08))
		PIC_ActivateIRQ(irq1);
}

bool Gus::CheckTimer(const size_t t)
{
	if (!timers[t].is_masked)
		timers[t].has_expired = true;
	if (timers[t].should_raise_irq) {
		irq_status |= 0x4 << t;
		CheckIrq();
	}
	return timers[t].is_counting_down;
}

void Gus::CheckVoiceIrq()
{
	irq_status &= 0x9f;
	const Bitu totalmask = voice_irq.state &  active_voice_mask;
	if (!totalmask)
		return;
	if (voice_irq.state)
		irq_status |= (0x40 | 0x20);
	CheckIrq();
	while (!(totalmask & (1 << voice_irq.count))) {
		voice_irq.count++;
		if (voice_irq.count >= active_voices)
			voice_irq.count = 0;
	}
}

static bool GUS_DMA_Active = false;

size_t Gus::Dma16Addr()
{
	const auto combined = ((dma_addr & 0x1fff) << 1) | (dma_addr & 0xc000);
	return static_cast<size_t>(combined) << 4;
}
size_t Gus::Dma8Addr()
{
	return static_cast<size_t>(dma_addr) << 4;
}

void Gus::GUS_DMA_Event_Transfer(DmaChannel *dma, uint32_t dmawords)
{
	const auto desired = dma->currcnt + 1;
	auto i = IsDma16Bit() ? Dma16Addr() : Dma8Addr();
	assert(i < RAM_SIZE);

	if (dma_ctrl & 0x2) // Write into DMA from GUS memory
		dma->Write(desired, ram + i);

	else if (!(dma_ctrl & 0x80)) // Passover
		dma->Read(desired, ram + i);

	else { // Read from DMA into GUS memory
		const auto start = i + IsDma16Bit();
		const auto samples_read = dma->Read(desired, ram + i);
		const auto sample_size = IsDma16Bit() ? 2 : 1;
		const auto end = i + samples_read * sample_size;
		assert(end <= RAM_SIZE);
		for (i = start; i < end; i += sample_size)
			ram[i] ^= 0x80;
	}
	// Raise the TC irq, and stop DMA
	if (dma_ctrl & 0x20) {
		// LOG_MSG("GUS DMA transfer hit Terminal Count");
		irq_status |= 0x80;
		CheckIrq();
		GUS_StopDMA();
	}
}

static void GUS_DMA_Event(Bitu val)
{
	(void)val; // UNUSED
	DmaChannel *dma = GetDMAChannel(gus->dma1);
	if (dma == NULL) {
		LOG_MSG("GUS DMA event: DMA channel no longer exists, stopping DMA transfer events");
		GUS_DMA_Active = false;
		return;
	}

	if (dma->masked) {
		LOG_MSG("GUS: Stopping DMA transfer interval, DMA masked=%u",
		        dma->masked ? 1 : 0);
		GUS_DMA_Active = false;
		return;
	}

	if (!(gus->dma_ctrl & 0x01 /*DMA enable*/)) {
		LOG_MSG("GUS DMA event: DMA control 'enable DMA' bit was reset, stopping DMA transfer events");
		GUS_DMA_Active = false;
		return;
	}

	LOG_MSG("GUS DMA event: max %u bytes. DMA: tc=%u mask=%u cnt=%u",
	        (unsigned int)BYTES_PER_DMA_XFER, dma->tcount ? 1 : 0,
	        dma->masked ? 1 : 0, dma->currcnt + 1);
	gus->GUS_DMA_Event_Transfer(dma, BYTES_PER_DMA_XFER);

	if (GUS_DMA_Active) {
		/* keep going */
		PIC_AddEvent(GUS_DMA_Event, MS_PER_DMA_XFER);
	}
}

void Gus::GUS_StopDMA()
{
	if (GUS_DMA_Active)
		LOG_MSG("GUS: Stopping DMA transfer interval");

	PIC_RemoveEvents(GUS_DMA_Event);
	GUS_DMA_Active = false;
}

void Gus::GUS_StartDMA()
{
	if (!GUS_DMA_Active) {
		GUS_DMA_Active = true;
		LOG_MSG("GUS: Starting DMA transfer interval");

		PIC_AddEvent(GUS_DMA_Event, MS_PER_DMA_XFER);

		if (GetDMAChannel(dma1)->masked)
			LOG(LOG_MISC, LOG_WARN)
			("GUS: DMA transfer interval started when channel is masked");
	}
}

void Gus::GUS_DMA_Callback(DmaChannel *, DMAEvent event)
{
	if (event == DMA_UNMASKED) {
		LOG_MSG("GUS: DMA unmasked");
		if (dma_ctrl & 0x01 /*DMA enable*/)
			GUS_StartDMA();
	} else if (event == DMA_MASKED) {
		LOG_MSG("GUS: DMA masked. Perhaps it will stop the DMA transfer event.");
	}
}

void Gus::PopulateAutoExec(uint16_t port, const std::string &ultradir)
{
	// ULTRASND=Port,(rec)DMA1,(pcm)DMA2,(play)IRQ1,(midi)IRQ2
	std::ostringstream sndline;
	sndline << "SET ULTRASND=" << std::hex << std::setw(3) << port << ","
	        << std::dec << static_cast<int>(dma1) << ","
	        << static_cast<int>(dma2) << "," << static_cast<int>(irq1)
	        << "," << static_cast<int>(irq2) << std::ends;
	LOG_MSG("GUS: %s", sndline.str().c_str());
	autoexec_lines.at(0).Install(sndline.str());

	// ULTRADIR=full path to directory containing "midi"
	std::string dirline = "SET ULTRADIR=" + ultradir;
	autoexec_lines.at(1).Install(dirline);
}

// Generate logarithmic to linear volume conversion tables
void Gus::PopulateVolScalars()
{
	double out = 1.0;
	for (uint16_t i = VOLUME_LEVELS - 1; i > 0; --i) {
		vol_scalars[i] = static_cast<float>(out);
		out /= VOLUME_LEVEL_DIVISOR;
	}
	vol_scalars[0] = 0.0f;
}

/*
Constant-Power Panning
-------------------------
The GUS SDK describes having 16 panning positions (0 through 15)
with 0 representing all full left rotation through to center or
mid-point at 7, to full-right rotation at 15.  The SDK also
describes that output power is held constant through this range.

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
void Gus::PopulatePanScalars()
{
	for (int i = 0; i < PAN_POSITIONS; ++i) { // Vectorized
		// Normalize absolute range [0, 15] to [-1.0, 1.0]
		const auto norm = (i - 7.0) / (i < 7 ? 7 : 8);
		// Convert to an angle between 0 and 90-degree, in radians
		const auto angle = (norm + 1) * M_PI / 4;
		pan_scalars[i].left = static_cast<float>(cos(angle));
		pan_scalars[i].right = static_cast<float>(sin(angle));
		// DEBUG_LOG_MSG("GUS: pan_scalar[%u] = %f | %f", i,
		//               pan_scalars.at(i).left,
		//               pan_scalars.at(i).right);
	}
}

void Gus::PrintStats()
{
	// Aggregate stats from all voices
	uint32_t combined_8bit_ms = 0u;
	uint32_t combined_16bit_ms = 0u;
	uint32_t used_8bit_voices = 0u;
	uint32_t used_16bit_voices = 0u;
	for (const auto &v : voices) {
		if (v->generated_8bit_ms) {
			combined_8bit_ms += v->generated_8bit_ms;
			used_8bit_voices++;
		}
		if (v->generated_16bit_ms) {
			combined_16bit_ms += v->generated_16bit_ms;
			used_16bit_voices++;
		}
	}
	const uint32_t combined_ms = combined_8bit_ms + combined_16bit_ms;

	// Is there enough information to be meaningful?
	if (combined_ms < 10000u || (peak.left + peak.right) < 10 ||
	    !(used_8bit_voices + used_16bit_voices))
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

	// Calculate and print info about the volume
	const auto mixer_scalar = std::max(audio_channel->volmain[0],
	                                   audio_channel->volmain[1]);
	const auto peak_sample = std::max(peak.left, peak.right);
	auto peak_ratio = mixer_scalar * peak_sample / AUDIO_SAMPLE_MAX;

	// It's expected and normal for multi-voice audio to periodically
	// accumulate beyond the max, which is gracefully scaled without
	// distortion, so there is no need to recommend that users scale-down
	// their GUS voice.
	peak_ratio = std::min(peak_ratio, 1.0f);
	LOG_MSG("GUS: Peak amplitude reached %.0f%% of max",
	        static_cast<double>(100 * peak_ratio));

	// Make a suggestion if the peak volume was well below 3 dB
	if (peak_ratio < 0.6f) {
		const auto multiplier = static_cast<uint16_t>(
		        100 * mixer_scalar / peak_ratio);
		LOG_MSG("GUS: If it should be louder, %s %u",
		        static_cast<float>(fabs(mixer_scalar - 1.0f)) > 0.01f
		                ? "adjust mixer gus to"
		                : "use: mixer gus",
		        multiplier);
	}
}

uint8_t Gus::ReadCtrl(const VoiceControl &ctrl) const
{
	uint8_t ret = ctrl.state;
	if (voice_irq.state & voice->irq_mask)
		ret |= 0x80;
	return ret;
}

Bitu Gus::ReadFromPort(const Bitu port, const Bitu iolen)
{
	//	LOG_MSG("read from gus port %x",port);
	switch (port - port_base) {
	case 0x206: return irq_status;
	case 0x208:
		uint8_t time;
		time = 0u;
		if (timers[0].has_expired)
			time |= (1 << 6);
		if (timers[1].has_expired)
			time |= (1 << 5);
		if (time & 0x60)
			time |= (1 << 7);
		if (irq_status & 0x04)
			time |= (1 << 2);
		if (irq_status & 0x08)
			time |= (1 << 1);
		return time;
	case 0x20a: return adlib_command_reg;
	case 0x302: return static_cast<uint8_t>(voice_index);
	case 0x303: return selected_register;
	case 0x304:
		if (iolen == 2)
			return ReadFromRegister() & 0xffff;
		else
			return ReadFromRegister() & 0xff;
	case 0x305: return ReadFromRegister() >> 8;
	case 0x307:
		if (dram_addr < RAM_SIZE) {
			return ram[dram_addr];
		} else {
			return 0;
		}
	default:
#if LOG_GUS
		LOG_MSG("Read GUS at port 0x%x", port);
#endif
		break;
	}

	return 0xff;
}

bool Gus::IsDma16Bit()
{
	// What bit-size should DMA memory be transferred as?
	// Mode PCM/DMA  Address Use-16  Note
	// 0x00   8/ 8   Any     No      Most DOS programs
	// 0x04   8/16   >= 4    Yes     16-bit if using High DMA
	// 0x04   8/16   < 4     No      8-bit if using Low DMA
	// 0x40  16/ 8   Any     No      Windows 3.1, Quake
	// 0x44  16/16   >= 4    Yes     Windows 3.1, Quake
	LOG_MSG("GUS: %u-bit DMA using address %u", (dma_ctrl & 0x4) ? 16u : 8u, dma1);
	return (dma_ctrl & 0x4) && (dma1 >= 4);
}

uint16_t Gus::ReadFromRegister()
{
	// LOG_MSG("GUS: Read register %x", selected_register);
	uint8_t reg;

	// Registers that read from the general DSP
	switch (selected_register) {
	case 0x41: // Dma control register - read acknowledges DMA IRQ
		if (!GetDMAChannel(dma1)->masked && !(dma_ctrl & 0x01) &&
		    !(irq_status & 0x80)) {
			LOG_MSG("GUS As instructed, switching on DMA ENABLE upon polling DMA control register (HACK) as workaround");
			dma_ctrl |= 0x01;
			GUS_StartDMA();
		}
		reg = dma_ctrl & 0xbf;
		reg |= (irq_status & 0x80) >> 1;
		irq_status &= 0x7f;
		CheckIrq();
		return static_cast<uint16_t>(reg << 8);
	case 0x42: // Dma address register
		return dma_addr;
	case 0x45: // Timer control register matches Adlib's behavior
		return static_cast<uint16_t>(timer_ctrl << 8);
	case 0x49: // Dma sample register
		reg = dma_ctrl & 0xbf;
		reg |= (irq_status & 0x80) >> 1;
		return static_cast<uint16_t>(reg << 8);
	case 0x8f: // General voice IRQ status register
		reg = voice_irq.count | 0x20;
		uint32_t mask;
		mask = 1 << voice_irq.count;
		if (!(voice_irq.state & mask))
			reg |= (0x40 | 0x80);
		voice_irq.state &= ~mask;
		CheckVoiceIrq();
		return static_cast<uint16_t>(reg << 8);
	}

	if (!voice)
		return (selected_register == 0x80 || selected_register == 0x8d)
		               ? 0x0300
		               : 0u;

	// Registers that read from from the current voice
	switch (selected_register) {
	case 0x80: // Voice wave control read register
		return static_cast<uint16_t>(ReadCtrl(voice->wave_ctrl) << 8);
	case 0x82: // Voice MSB start address register
		return static_cast<uint16_t>(voice->wave_ctrl.start >> 16);
	case 0x83: // Voice LSW start address register
		return static_cast<uint16_t>(voice->wave_ctrl.start);
	case 0x89: // Voice volume register
	{
		const int i = ceil_sdivide(voice->vol_ctrl.pos, VOLUME_INC_SCALAR);
		assert(i < VOLUME_LEVELS);
		return static_cast<uint16_t>(i << 4);
	}
	case 0x8a: // Voice MSB current address register
		return static_cast<uint16_t>(voice->wave_ctrl.pos >> 16);
	case 0x8b: // Voice LSW current address register
		return static_cast<uint16_t>(voice->wave_ctrl.pos);
	case 0x8d: // Voice volume control register
		return static_cast<uint16_t>(ReadCtrl(voice->vol_ctrl) << 8);
	}
#if LOG_GUS
	LOG_MSG("GUS: Unimplemented read Register 0x%x", selected_register);
#endif
	return register_data;
}

void Gus::StopPlayback()
{
	// Halt playback before altering the DSP state
	audio_channel->Enable(false);

	irq_enabled = false;
	irq_status = 0;

	dma_ctrl = 0u;
	mix_ctrl = 0xb; // latches enabled, LINEs disabled
	timer_ctrl = 0u;
	sample_ctrl = 0u;

	voice = nullptr;
	voice_index = 0u;
	active_voices = 0u;

	dma_addr = 0u;
	dram_addr = 0u;
	register_data = 0u;
	selected_register = 0u;
	should_change_irq_dma = false;
	PIC_RemoveEvents(GUS_TimerEvent);
}

void Gus::PrepareForPlayback()
{
	// Initialize the voice states
	for (auto &v : voices) {
		v->vol_ctrl.pos = 0u;
		WriteCtrl(v->wave_ctrl, v->irq_mask, 0x1);
		WriteCtrl(v->vol_ctrl, v->irq_mask, 0x1);
		v->WritePanPot(PAN_DEFAULT_POSITION);
	}

	// Initialize the OPL emulator state
	adlib_command_reg = ADLIB_CMD_DEFAULT;

	voice_irq = VoiceIrq{};
	timers[0] = Timer{TIMER_1_DEFAULT_DELAY};
	timers[1] = Timer{TIMER_2_DEFAULT_DELAY};
}

void Gus::BeginPlayback()
{
	audio_channel->Enable(true);
	if (prev_logged_voices != active_voices) {
		LOG_MSG("GUS: Activated %u voices at %u Hz", active_voices,
		        playback_rate);
		prev_logged_voices = active_voices;
	}
}

void Gus::UpdatePeakAmplitudes(const float *stream)
{
	for (int i = 0; i < BUFFER_SAMPLES - 1; i += 2) {
		peak.left = std::max(peak.left, fabsf(stream[i]));
		peak.right = std::max(peak.right, fabsf(stream[i + 1]));
	}
}

void Gus::SoftLimit(const float *in, int16_t *out)
{
	UpdatePeakAmplitudes(in);

	// If our peaks are under the max, then there's no need to limit
	if (peak.left < AUDIO_SAMPLE_MAX && peak.right < AUDIO_SAMPLE_MAX) {
		for (int i = 0; i < BUFFER_SAMPLES - 1; i += 2) { // vectorized
			out[i] = static_cast<int16_t>(in[i]);
			out[i + 1] = static_cast<int16_t>(in[i + 1]);
		}
		return;
	}
	// Calculate the percent we need to scale down the volume index
	// position.  In cases where one side is less than the max, it's ratio
	// is limited to 1.0.
	const float left_scalar = std::min(ONE_AMP, AUDIO_SAMPLE_MAX / peak.left);
	const float right_scalar = std::min(ONE_AMP, AUDIO_SAMPLE_MAX / peak.right);

	for (int i = 0; i < BUFFER_SAMPLES - 1; i += 2) { // Vectorized
		out[i] = static_cast<int16_t>(in[i] * left_scalar);
		out[i + 1] = static_cast<int16_t>(in[i + 1] * right_scalar);
	}
	if (peak.left > AUDIO_SAMPLE_MAX)
		peak.left -= SOFT_LIMIT_RELEASE_INC;
	if (peak.right > AUDIO_SAMPLE_MAX)
		peak.right -= SOFT_LIMIT_RELEASE_INC;
	// LOG_MSG("GUS: releasing peak_amplitude = %.2f | %.2f",
	//         static_cast<double>(peak.left),
	//         static_cast<double>(peak.right));
}

static void GUS_TimerEvent(Bitu t)
{
	if (gus->CheckTimer(t))
		PIC_AddEvent(GUS_TimerEvent, gus->timers[t].delay, t);
}

void Gus::WriteToPort(Bitu port, Bitu val, Bitu iolen)
{
	//	LOG_MSG("Write gus port %x val %x",port,val);
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
			timers[0].has_expired = false;
			timers[1].has_expired = false;
			return;
		}
		timers[0].is_masked = (val & 0x40) > 0;
		timers[1].is_masked = (val & 0x20) > 0;
		if (val & 0x1) {
			if (!timers[0].is_counting_down) {
				PIC_AddEvent(GUS_TimerEvent, timers[0].delay, 0);
				timers[0].is_counting_down = true;
			}
		} else
			timers[0].is_counting_down = false;
		if (val & 0x2) {
			if (!timers[1].is_counting_down) {
				PIC_AddEvent(GUS_TimerEvent, timers[1].delay, 1);
				timers[1].is_counting_down = true;
			}
		} else
			timers[1].is_counting_down = false;
		break;
		// TODO Check if 0x20a register is also available on the gus
		// like on the interwave
	case 0x20b:
		if (!should_change_irq_dma)
			break;
		should_change_irq_dma = false;
		if (mix_ctrl & 0x40) {
			// IRQ configuration, only use low bits for irq 1
			const auto i = val & 0x7;
			assert(i < irq_addresses.size());
			if (irq_addresses[i])
				irq1 = irq_addresses[i];
#if LOG_GUS
			LOG_MSG("Assigned GUS to IRQ %d", irq1);
#endif
		} else {
			// DMA configuration, only use low bits for dma 1
			const auto i = val & 0x7;
			assert(i < dma_addresses.size());
			// If the DMA address is valid differs from existing
			if (dma_addresses[i] && dma1 != dma_addresses[i]) {
				GetDMAChannel(dma1)->Register_Callback(nullptr);
				dma1 = dma_addresses[i];
				auto dma_callback = std::bind(&Gus::GUS_DMA_Callback,
				                              this, _1, _2);
				GetDMAChannel(dma1)->Register_Callback(dma_callback);
			}
#if LOG_GUS
			LOG_MSG("Assigned GUS to DMA %d", dma1);
#endif
		}
		break;
	case 0x302:
		voice_index = val & 31;
		assert(voice_index < voices.size());
		voice = voices[voice_index].get();
		break;
	case 0x303:
		selected_register = static_cast<uint8_t>(val);
		register_data = 0;
		break;
	case 0x304:
		if (iolen == 2) {
			register_data = static_cast<uint16_t>(val);
			WriteToRegister();
		} else
			register_data = static_cast<uint16_t>(val);
		break;
	case 0x305:
		register_data = static_cast<uint16_t>((0x00ff & register_data) |
		                                      val << 8);
		WriteToRegister();
		break;
	case 0x307:
		if (dram_addr < RAM_SIZE)
			ram[dram_addr] = static_cast<uint8_t>(val);
		break;
	default:
#if LOG_GUS
		LOG_MSG("Write GUS at port 0x%x with %x", port, val);
#endif
		break;
	}
}

void Gus::UpdateWaveLsw(int32_t &addr) const
{
	const auto lower = static_cast<unsigned>(addr) & WAVE_LSW_MASK;
	addr = static_cast<int32_t>(lower | register_data);
}

void Gus::UpdateWaveMsw(int32_t &addr) const
{
	const uint32_t upper = register_data & 0x1fff;
	const auto lower = static_cast<unsigned>(addr) & WAVE_MSW_MASK;
	addr = static_cast<int32_t>(lower | (upper << 16));
}

void Gus::ActivateVoices(uint8_t requested_voices)
{
	requested_voices = clamp(requested_voices, MIN_VOICES, MAX_VOICES);
	if (requested_voices != active_voices) {
		active_voices = requested_voices;
		assert(active_voices <= voices.size());
		active_voice_mask = 0xffffffffU >> (MAX_VOICES - active_voices);
		playback_rate = static_cast<uint32_t>(
		        0.5 + 1000000.0 / (1.619695497 * active_voices));
		audio_channel->SetFreq(playback_rate);
	}
}

void Gus::WriteToRegister()
{
	// Registers that write to the general DSP
	switch (selected_register) {
	case 0xE: // Set active voice register
		selected_register = register_data >> 8; // Jazz Jackrabbit needs this
		{
			uint8_t num_voices = 1 + ((register_data >> 8) & 31);
			ActivateVoices(num_voices);
		}
		return;
	case 0x10: // Undocumented register used in Fast Tracker 2
		return;
	case 0x41: // Dma control register
		dma_ctrl = register_data >> 8;
		if (dma_ctrl & 1)
			GUS_StartDMA();
		else
			GUS_StopDMA();
		break;

	/* 	case 0x41: // Dma control register
	                dma_ctrl = static_cast<uint8_t>(register_data >> 8);
	                {
	                        LOG_MSG("GUS: 0x41");
	                        auto dma_callback = std::bind(&Gus::DmaCallback,
	   this, _1, _2); auto empty_callback = std::function<void(DmaChannel *,
	   DMAEvent)>(nullptr); GetDMAChannel(dma1)->Register_Callback( (dma_ctrl
	   & 0x1) ? dma_callback : empty_callback);
	                }
	                return;
	 */
	case 0x42: // Gravis DRAM DMA address register
		dma_addr = register_data;
		return;
	case 0x43: // MSB Peek/poke DRAM position
		dram_addr = (0xff0000 & dram_addr) |
		            (static_cast<uint32_t>(register_data));
		return;
	case 0x44: // LSW Peek/poke DRAM position
		dram_addr = (0xffff & dram_addr) |
		            (static_cast<uint32_t>(register_data >> 8)) << 16;
		return;
	case 0x45: // Timer control register.  Identical in operation to Adlib's
		timer_ctrl = static_cast<uint8_t>(register_data >> 8);
		timers[0].should_raise_irq = (timer_ctrl & 0x04) > 0;
		if (!timers[0].should_raise_irq)
			irq_status &= ~0x04;
		timers[1].should_raise_irq = (timer_ctrl & 0x08) > 0;
		if (!timers[1].should_raise_irq)
			irq_status &= ~0x08;
		return;
	case 0x46: // Timer 1 control
		timers[0].value = static_cast<uint8_t>(register_data >> 8);
		timers[0].delay = (0x100 - timers[0].value) * TIMER_1_DEFAULT_DELAY;
		return;
	case 0x47: // Timer 2 control
		timers[1].value = static_cast<uint8_t>(register_data >> 8);
		timers[1].delay = (0x100 - timers[1].value) * TIMER_2_DEFAULT_DELAY;
		return;
	case 0x49: // DMA sampling control register
		sample_ctrl = static_cast<uint8_t>(register_data >> 8);
		if (dma_ctrl & 1)
			GUS_StartDMA();
		else
			GUS_StopDMA();
		/* 		{
		                        auto dma_callback =
		   std::bind(&Gus::DmaCallback, this, _1, _2);
		                        std::function<void(DmaChannel *,
		   DMAEvent)> empty_callback = nullptr;
		                        GetDMAChannel(dma1)->Register_Callback(
		                                (sample_ctrl & 0x1) ?
		   dma_callback : empty_callback);
		                }
		 */
		return;
	case 0x4c: // Runtime control
		irq_enabled = register_data & 0x4;
		{
			const auto state = (register_data >> 8) & 7;
			if (state == 0)
				StopPlayback();
			else if (state == 1)
				PrepareForPlayback();
			else if (active_voices)
				BeginPlayback();
		}
		CheckIrq();
		return;
	}

	if (!voice)
		return;

	uint8_t data;
	// Registers that write to the current voice
	switch (selected_register) {
	case 0x0: // Voice wave control register
		WriteCtrl(voice->wave_ctrl, voice->irq_mask, register_data >> 8);
		break;
	case 0x1: // Voice rate control register
		voice->WriteWaveRate(register_data);
		break;
	case 0x2: // Voice MSW start address register
		UpdateWaveMsw(voice->wave_ctrl.start);
		break;
	case 0x3: // Voice LSW start address register
		UpdateWaveLsw(voice->wave_ctrl.start);
		break;
	case 0x4: // Voice MSW end address register
		UpdateWaveMsw(voice->wave_ctrl.end);
		break;
	case 0x5: // Voice LSW end address register
		UpdateWaveLsw(voice->wave_ctrl.end);
		break;
	case 0x6: // Voice volume rate register
		voice->WriteVolRate(register_data >> 8);
		break;
	case 0x7: // Voice volume start register  EEEEMMMM
		data = register_data >> 8;
		// Don't need to bounds-check the value because it's implied:
		// 'data' is a uint8, so is 255 at most. 255 << 4 = 4080, which
		// falls within-bounds of the 4096-long vol_scalars array.
		voice->vol_ctrl.start = (data << 4) * VOLUME_INC_SCALAR;
		break;
	case 0x8: // Voice volume end register  EEEEMMMM
		data = register_data >> 8;
		// Same as above regarding bound-checking.
		voice->vol_ctrl.end = (data << 4) * VOLUME_INC_SCALAR;
		break;
	case 0x9: // Voice current volume register
		// Don't need to bounds-check the value because it's implied:
		// reg data is a uint16, and 65535 >> 4 takes it down to 4095,
		// which is the last element in the 4096-long vol_scalars array.
		voice->vol_ctrl.pos = (register_data >> 4) * VOLUME_INC_SCALAR;
		break;
	case 0xA: // Voice MSW current address register
		UpdateWaveMsw(voice->wave_ctrl.pos);
		break;
	case 0xB: // Voice LSW current address register
		UpdateWaveLsw(voice->wave_ctrl.pos);
		break;
	case 0xC: // Voice pan pot register
		voice->WritePanPot(register_data >> 8);
		break;
	case 0xD: // Voice volume control register
		WriteCtrl(voice->vol_ctrl, voice->irq_mask, register_data >> 8);
		break;
	}

#if LOG_GUS
	LOG_MSG("Unimplemented write register %x -- %x", register_select,
	        register_data);
#endif
	return;
}

void GUS_ShutDown(Section * /*sec*/)
{
	if (gus) {
		gus->PrintStats();
		gus.reset(nullptr);
	}
}

void GUS_Init(Section *sec)
{
	assert(sec);
	Section_prop *conf = dynamic_cast<Section_prop *>(sec);
	assert(conf);

	// Is the GUS disabled?
	if (!conf->Get_bool("gus"))
		return;

	// Read the GUS config settings
	const auto port = static_cast<uint16_t>(conf->Get_hex("gusbase"));
	const auto dma = clamp(static_cast<uint8_t>(conf->Get_int("gusdma")), MIN_DMA_ADDRESS, MAX_DMA_ADDRESS);
	const auto irq = clamp(static_cast<uint8_t>(conf->Get_int("gusirq")), MIN_IRQ_ADDRESS, MAX_IRQ_ADDRESS);
	const std::string ultradir = conf->Get_string("ultradir");

	// Instantiate the GUS with the settings
	gus = std::make_unique<Gus>(port, dma, irq, ultradir);
	sec->AddDestroyFunction(&GUS_ShutDown, true);
}
