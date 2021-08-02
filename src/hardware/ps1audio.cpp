/*
 *  Copyright (C) 2021-2021  The DOSBox Staging Team
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

#include "dosbox.h"

#include <algorithm>
#include <cassert>
#include <memory>
#include <string.h>

#include "control.h"
#include "dma.h"
#include "inout.h"
#include "mem.h"
#include "mixer.h"
#include "pic.h"
#include "setup.h"

#include "mame/emu.h"
#include "mame/sn76496.h"

using namespace std::placeholders;
using mixer_channel_t = std::unique_ptr<MixerChannel, decltype(&MIXER_DelChannel)>;

struct Ps1Registers {
	uint8_t status = 0;     // Read via port 0x202 control status
	uint8_t command = 0;    // Written via port 0x202 for control, read via 0x200 for DAC
	uint8_t divisor = 0;    // Read via port 0x203 for FIFO timing
	uint8_t fifo_level = 0; // Written via port 0x204 when FIFO is almost empty
};

class Ps1Dac {
public:
	Ps1Dac();
	~Ps1Dac();

private:
	uint8_t CalcStatus() const;
	void Reset(bool should_clear_adder);
	void Update(uint16_t samples);
	uint8_t ReadPresencePort02F(io_port_t port, io_width_t);
	uint8_t ReadCmdResultPort200(io_port_t port, io_width_t);
	uint8_t ReadStatusPort202(io_port_t port, io_width_t);
	uint8_t ReadTimingPort203(io_port_t port, io_width_t);
	uint8_t ReadJoystickPorts204To207(io_port_t port, io_width_t);

	void WriteDataPort200(io_port_t port, uint8_t data, io_width_t);
	void WriteControlPort202(io_port_t port, uint8_t data, io_width_t);
	void WriteTimingPort203(io_port_t port, uint8_t data, io_width_t);
	void WriteFifoLevelPort204(io_port_t port, uint8_t data, io_width_t);

	// Constants
	static constexpr auto clock_rate_hz = 1000000;
	static constexpr auto fifo_size = 2048;
	static constexpr auto fifo_size_mask = fifo_size - 1;
	static constexpr auto fifo_nearly_empty_val = 128;
	static constexpr auto frac_shift = 12;            // Fixed precision
	static constexpr auto fifo_status_ready_flag = 0x10;
	static constexpr auto fifo_full_flag = 0x08;
	static constexpr auto fifo_empty_flag = 0x04;
	static constexpr auto fifo_nearly_empty_flag = 0x02; // >= 1792 bytes free
	static constexpr auto fifo_irq_flag = 0x01; // IRQ triggered by DAC
	static constexpr auto fifo_midline = ceil_udivide(static_cast<uint8_t>(UINT8_MAX), 2u);
	static constexpr auto irq_number = 7;
	static constexpr auto bytes_pending_limit =  fifo_size << frac_shift;

	// Managed objects
	mixer_channel_t channel{nullptr, MIXER_DelChannel};
	IO_ReadHandleObject read_handlers[5] = {};
	IO_WriteHandleObject write_handlers[4] = {};
	Ps1Registers regs = {};
	uint8_t fifo[fifo_size] = {};

	// Counters
	size_t last_write = 0;
	uint32_t adder = 0;
	uint32_t bytes_pending = 0;
	uint32_t read_index_high = 0;
	uint32_t sample_rate = 0;
	uint16_t read_index = 0;
	uint16_t write_index = 0;
	int8_t signal_bias = 0;

	// States
	bool is_new_transfer = true;
	bool is_playing = false;
	bool can_trigger_irq = false;
};

static void keep_alive_channel(size_t &last_used_on, mixer_channel_t &channel)
{
	last_used_on = PIC_Ticks;
	if (!channel->is_enabled)
		channel->Enable(true);
}

static void maybe_suspend_channel(const size_t last_used_on, mixer_channel_t &channel)
{
	const bool last_used_five_seconds_ago = PIC_Ticks > last_used_on + 5000;
	if (last_used_five_seconds_ago)
		channel->Enable(false);
}

Ps1Dac::Ps1Dac()
{
	const auto callback = std::bind(&Ps1Dac::Update, this, _1);
	channel = mixer_channel_t(MIXER_AddChannel(callback, 0, "PS1DAC"),
	                          MIXER_DelChannel);
	assert(channel);

	// Register DAC per-port read handlers
	read_handlers[0].Install(0x02F, std::bind(&Ps1Dac::ReadPresencePort02F, this, _1, _2), io_width_t::byte);
	read_handlers[1].Install(0x200, std::bind(&Ps1Dac::ReadCmdResultPort200, this, _1, _2), io_width_t::byte);
	read_handlers[2].Install(0x202, std::bind(&Ps1Dac::ReadStatusPort202, this, _1, _2), io_width_t::byte);
	read_handlers[3].Install(0x203, std::bind(&Ps1Dac::ReadTimingPort203, this, _1, _2), io_width_t::byte);
	read_handlers[4].Install(0x204, // to 0x207
	                         std::bind(&Ps1Dac::ReadJoystickPorts204To207, this, _1, _2), io_width_t::byte, 3);

	// Register DAC per-port write handlers
	write_handlers[0].Install(0x200, std::bind(&Ps1Dac::WriteDataPort200, this, _1, _2, _3), io_width_t::byte);
	write_handlers[1].Install(0x202, std::bind(&Ps1Dac::WriteControlPort202, this, _1, _2, _3), io_width_t::byte);
	write_handlers[2].Install(0x203, std::bind(&Ps1Dac::WriteTimingPort203, this, _1, _2, _3), io_width_t::byte);
	write_handlers[3].Install(0x204, std::bind(&Ps1Dac::WriteFifoLevelPort204, this, _1, _2, _3),
	                          io_width_t::byte);

	// Operate at native sampling rates
	sample_rate = channel->GetSampleRate();
	last_write = 0;
	Reset(true);
}

uint8_t Ps1Dac::CalcStatus() const
{
	uint8_t status = regs.status & fifo_irq_flag;
	if (!bytes_pending)
		status |= fifo_empty_flag;

	if (bytes_pending < (fifo_nearly_empty_val << frac_shift) &&
	    (regs.command & 3) == 3)
		status |= fifo_nearly_empty_flag;

	if (bytes_pending > ((fifo_size - 1) << frac_shift))
		status |= fifo_full_flag;

	return status;
}

void Ps1Dac::Reset(bool should_clear_adder)
{
	PIC_DeActivateIRQ(irq_number);
	memset(fifo, fifo_midline, fifo_size);
	read_index = 0;
	write_index = 0;
	read_index_high = 0;

	 // Be careful with this, 5 second timeout and Space Quest 4
	if (should_clear_adder)
		adder = 0;

	bytes_pending = 0;
	regs.status = CalcStatus();
	can_trigger_irq = false;
	is_playing = true;
	is_new_transfer = true;
}

void Ps1Dac::WriteDataPort200(io_port_t, uint8_t data, io_width_t)
{
	keep_alive_channel(last_write, channel);
	if (is_new_transfer) {
		is_new_transfer = false;
		if (data) {
			signal_bias = static_cast<int8_t>(data - fifo_midline);
		}
	}
	regs.status = CalcStatus();
	if (!(regs.status & fifo_full_flag)) {
		const auto corrected_data = data - signal_bias;
		fifo[write_index++] = static_cast<uint8_t>(corrected_data);
		write_index &= fifo_size_mask;
		bytes_pending += (1 << frac_shift);

		if (bytes_pending > bytes_pending_limit) {
			bytes_pending = bytes_pending_limit;
		}
	}
}

void Ps1Dac::WriteControlPort202(io_port_t, uint8_t data, io_width_t)
{
	keep_alive_channel(last_write, channel);
	regs.command = data;
	if (data & 3)
		can_trigger_irq = true;
}

void Ps1Dac::WriteTimingPort203(io_port_t, uint8_t data, io_width_t)
{
	keep_alive_channel(last_write, channel);
	// Clock divisor (maybe trigger first IRQ here).
	regs.divisor = data;

	if (data < 45) // common in Infocom games
		data = 125; // fallback to a default 8 KHz data rate
	const auto data_rate_hz = static_cast<uint32_t>(clock_rate_hz / data);
	adder = (data_rate_hz << frac_shift) / sample_rate;

	regs.status = CalcStatus();
	if ((regs.status & fifo_nearly_empty_flag) && (can_trigger_irq)) {
		// Generate request for stuff.
		regs.status |= fifo_irq_flag;
		can_trigger_irq = false;
		PIC_ActivateIRQ(irq_number);
	}
}

void Ps1Dac::WriteFifoLevelPort204(io_port_t, uint8_t data, io_width_t)
{
	keep_alive_channel(last_write, channel);
	regs.fifo_level = data;
	if (!data)
		Reset(true);
	// When the Microphone is used (PS1MIC01), it writes 0x08 to this during
	// playback presumably beacuse the card is constantly filling the
	// analog-to-digital buffer.
}

uint8_t Ps1Dac::ReadPresencePort02F(io_port_t, io_width_t)
{
	keep_alive_channel(last_write, channel);
	return 0xff;
}

uint8_t Ps1Dac::ReadCmdResultPort200(io_port_t, io_width_t)
{
	keep_alive_channel(last_write, channel);
	regs.status &= ~fifo_status_ready_flag;
	return regs.command;
}

uint8_t Ps1Dac::ReadStatusPort202(io_port_t, io_width_t)
{
	keep_alive_channel(last_write, channel);
	regs.status = CalcStatus();
	return regs.status;
}

// Used by Stunt Island and Roger Rabbit 2 during setup.
uint8_t Ps1Dac::ReadTimingPort203(io_port_t, io_width_t)
{
	keep_alive_channel(last_write, channel);
	return regs.divisor;
}

// Used by Bush Buck as an alternate detection method.
uint8_t Ps1Dac::ReadJoystickPorts204To207(io_port_t, io_width_t)
{
	keep_alive_channel(last_write, channel);
	return 0;
}

void Ps1Dac::Update(uint16_t samples)
{
	uint8_t *buffer = MixTemp;

	int32_t pending = 0;
	uint32_t add = 0;
	uint32_t pos = read_index_high;
	uint16_t count = samples;

	if (is_playing) {
		regs.status = CalcStatus();
		pending = static_cast<int32_t>(bytes_pending);
		add = adder;
		if ((regs.status & fifo_nearly_empty_flag) && (can_trigger_irq)) {
			// More bytes needed.
			regs.status |= fifo_irq_flag;
			can_trigger_irq = false;
			PIC_ActivateIRQ(irq_number);
		}
	}

	while (count) {
		uint8_t out = 0;
		if (pending <= 0) {
			pending = 0;
			while (count--) {
				*(buffer++) = fifo_midline;
			}
			break;
		} else {
			out = fifo[pos >> frac_shift];
			pos += add;
			pos &= (fifo_size << frac_shift) - 1;
			pending -= static_cast<int32_t>(add);
		}
		*(buffer++) = out;
		count--;
	}
	// Update positions and see if we can clear the fifo_full_flag
	read_index_high = pos;
	read_index = static_cast<uint16_t>(pos >> frac_shift);
	if (pending < 0)
		pending = 0;
	bytes_pending = static_cast<uint32_t>(pending);

	channel->AddSamples_m8(samples, MixTemp);
	maybe_suspend_channel(last_write, channel);
}

Ps1Dac::~Ps1Dac()
{
	// Stop the game from accessing the IO ports
	for (auto &handler : read_handlers)
		handler.Uninstall();
	for (auto &handler : write_handlers)
		handler.Uninstall();

	// Stop and remove the mixer callback
	if (channel) {
		channel->Enable(false);
		channel.reset();
	}
}

class Ps1Synth {
public:
	Ps1Synth();
	~Ps1Synth();

private:
	void Update(uint16_t samples);
	void WriteSoundGeneratorPort205(io_port_t port, uint8_t data, io_width_t);

	mixer_channel_t channel{nullptr, MIXER_DelChannel};
	IO_WriteHandleObject write_handler = {};
	static constexpr auto clock_rate_hz = 4000000;
	sn76496_device device;
	static constexpr auto max_samples_expected = 64;
	int16_t buffer[1][max_samples_expected];
	size_t last_write = 0;
};

Ps1Synth::Ps1Synth() : device(machine_config(), 0, 0, clock_rate_hz)
{
	const auto callback = std::bind(&Ps1Synth::Update, this, _1);
	channel = mixer_channel_t(MIXER_AddChannel(callback, 0, "PS1"),
	                          MIXER_DelChannel);
	assert(channel);

	const auto generate_sound = std::bind(&Ps1Synth::WriteSoundGeneratorPort205, this, _1, _2, _3);
	write_handler.Install(0x205, generate_sound, io_width_t::byte);
	static_cast<device_t &>(device).device_start();

	auto sample_rate = static_cast<int32_t>(channel->GetSampleRate());
	device.convert_samplerate(sample_rate);
	last_write = 0;
}

void Ps1Synth::WriteSoundGeneratorPort205(io_port_t, uint8_t data, io_width_t)
{
	keep_alive_channel(last_write, channel);
	device.write(data);
}

void Ps1Synth::Update(uint16_t samples)
{
	assert(samples <= max_samples_expected);

	// sound_stream_update's API requires an array of two pointers that
	// point to either the mono array head or left and right heads. In this
	// case, we're using a mono array but we still want to comply with the
	// API, so we give it a valid two-element pointer array.
	int16_t *buffer_head[] = {buffer[0], buffer[0]}; 

	device_sound_interface::sound_stream ss;
	static_cast<device_sound_interface &>(device).sound_stream_update(
	        ss, nullptr, buffer_head, samples);
	channel->AddSamples_m16(samples, buffer[0]);
	maybe_suspend_channel(last_write, channel);
}

Ps1Synth::~Ps1Synth()
{
	// Stop the game from accessing the IO ports
	write_handler.Uninstall();

	if (channel) {
		channel->Enable(false);
		channel.reset();
	}
}

static std::unique_ptr<Ps1Dac> ps1_dac = {};
static std::unique_ptr<Ps1Synth> ps1_synth = {};

static void PS1AUDIO_ShutDown(MAYBE_UNUSED Section *sec)
{
	LOG_MSG("PS/1: Shutting down IBM PS/1 Audio card");
	ps1_dac.reset();
	ps1_synth.reset();
}

bool PS1AUDIO_IsEnabled()
{
	const auto section = control->GetSection("speaker");
	assert(section);
	const auto properties = static_cast<Section_prop *>(section);
	return properties->Get_bool("ps1audio");
}

void PS1AUDIO_Init(MAYBE_UNUSED Section *sec)
{
	if (!PS1AUDIO_IsEnabled())
		return;

	ps1_dac = std::make_unique<Ps1Dac>();
	ps1_synth = std::make_unique<Ps1Synth>();

	LOG_MSG("PS/1: Initialized IBM PS/1 Audio card");
	sec->AddDestroyFunction(&PS1AUDIO_ShutDown, true);
}
