/*
 *  Copyright (C) 2021-2022  The DOSBox Staging Team
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

#include <cassert>
#include <queue>

#include "bit_view.h"
#include "checks.h"
#include "inout.h"
#include "mixer.h"
#include "parallel_port.h"
#include "pic.h"
#include "setup.h"

CHECK_NARROWING();

class Disney {
public:
	Disney(const io_port_t base_port, const std::string_view filter_pref);
	~Disney();

private:
	Disney()                          = delete;
	Disney(const Disney &)            = delete;
	Disney &operator=(const Disney &) = delete;

	AudioFrame Render();
	void RenderUpToNow();
	void AudioCallback(const uint16_t requested_frames);

	bool IsFifoFull() const;
	uint8_t ReadStatus(const io_port_t, const io_width_t);
	void WriteData(const io_port_t, const io_val_t value, const io_width_t);
	void WriteControl(const io_port_t, const io_val_t value, const io_width_t);

	// The DSS is an LPT DAC with a 16-level FIFO running at 7kHz
	static constexpr auto dss_7khz_rate    = 7000;
	static constexpr auto ms_per_frame     = 1000.0 / dss_7khz_rate;
	static constexpr uint8_t max_fifo_size = 16;

	// Managed objects
	mixer_channel_t channel                    = {};
	std::queue<uint8_t> fifo                   = {};
	std::queue<AudioFrame> render_queue        = {};
	IO_WriteHandleObject data_write_handler    = {};
	IO_ReadHandleObject status_read_handler    = {};
	IO_WriteHandleObject control_write_handler = {};

	// Runtime state
	double last_rendered_ms        = 0.0;
	uint8_t data_reg               = Mixer_GetSilentDOSSample<uint8_t>();
	LptStatusRegister status_reg   = {};
	LptControlRegister control_reg = {};
};

bool Disney::IsFifoFull() const
{
	return fifo.size() >= max_fifo_size;
}

// Eight bit data sent to the D/A convener is loaded into a 16 level FIFO. Data
// is clocked from this FIFO at the fixed rate of 7 kHz +/- 5%.
AudioFrame Disney::Render()
{
	assert(fifo.size());
	const float sample = lut_u8to16[fifo.front()];
	if (fifo.size() > 1)
		fifo.pop();
	return {sample, sample};
}

void Disney::RenderUpToNow()
{
	const auto now = PIC_FullIndex();

	// Wake up the channel and update the last rendered time datum.
	assert(channel);
	if (channel->WakeUp()) {
		last_rendered_ms = now;
		return;
	}
	// Keep rendering until we're current
	while (last_rendered_ms < now) {
		last_rendered_ms += ms_per_frame;
		render_queue.emplace(Render());
	}
}

void Disney::WriteData(const io_port_t, const io_val_t data, const io_width_t)
{
	data_reg = check_cast<uint8_t>(data);
}

void Disney::WriteControl(const io_port_t, const io_val_t value, const io_width_t)
{
	RenderUpToNow();

	const auto new_control = LptControlRegister{check_cast<uint8_t>(value)};

	// The rising edge of the pulse on Pin 17 from the printer interface
	// is used to clock data into the FIFO. Note from diagram 1 that the
	// SELECT and INIT inputs to the D/A chip are isolated from pin 17 by
	// an RC lime constant. Ref:
	// https://archive.org/stream/dss-programmers-guide/dss-programmers-guide_djvu.txt

	if (!control_reg.select && new_control.select)
		if (!IsFifoFull())
			fifo.emplace(data_reg);

	control_reg.data = new_control.data;
}

uint8_t Disney::ReadStatus(const io_port_t, const io_width_t)
{
	// The Disney ACK's (active-low) when the FIFO has room
	status_reg.ack = IsFifoFull();
	return status_reg.data;
}

void Disney::AudioCallback(const uint16_t requested_frames)
{
	assert(channel);

	auto frames_remaining = requested_frames;

	// First, add any frames we've queued since the last callback
	while (frames_remaining && render_queue.size()) {
		channel->AddSamples_sfloat(1, &render_queue.front()[0]);
		render_queue.pop();
		--frames_remaining;
	}
	// If the queue's run dry, render the remainder and sync-up our time datum
	while (frames_remaining) {
		const auto frame = Render();
		channel->AddSamples_sfloat(1, &frame[0]);
		--frames_remaining;
	}
	last_rendered_ms = PIC_FullIndex();
}

std::unique_ptr<Disney> disney = {};

static void setup_filters(mixer_channel_t &channel)
{
	// The filters are meant to emulate the Disney's bandwidth limitations
	// both by ear and spectrum analysis when compared against LGR Oddware's
	// recordings of an authentic Disney Sound Source in ref:
	// https://youtu.be/A1YThKmV2dk?t=1126

	constexpr auto hp_order       = 2;
	constexpr auto hp_cutoff_freq = 100;
	channel->ConfigureHighPassFilter(hp_order, hp_cutoff_freq);
	channel->SetHighPassFilter(FilterState::On);

	constexpr auto lp_order       = 2;
	constexpr auto lp_cutoff_freq = 2000;
	channel->ConfigureLowPassFilter(lp_order, lp_cutoff_freq);
	channel->SetLowPassFilter(FilterState::On);
}

Disney::Disney(const io_port_t base_port, const std::string_view filter_choice)
{
	// Prime the FIFO with a single silent sample
	fifo.emplace(data_reg);

	using namespace std::placeholders;
	const auto audio_callback = std::bind(&Disney::AudioCallback, this, _1);

	// Setup the mixer callback
	channel = MIXER_AddChannel(audio_callback,
	                           0,
	                           "DISNEY",
	                           {ChannelFeature::Sleep,
	                            ChannelFeature::ReverbSend,
	                            ChannelFeature::ChorusSend,
	                            ChannelFeature::DigitalAudio});
	assert(channel);

	// Run the ZoH up-sampler at the higher mixer rate
	const auto mixer_rate_hz = check_cast<uint16_t>(channel->GetSampleRate());
	channel->ConfigureZeroOrderHoldUpsampler(mixer_rate_hz);
	channel->EnableZeroOrderHoldUpsampler();

	// Pull audio frames from the Disney at the DAC's 7 kHz rate
	channel->SetSampleRate(dss_7khz_rate);

	// Setup filters
	if (filter_choice == "on") {
		setup_filters(channel);
	} else {
		if (filter_choice != "off")
			LOG_WARNING("DISNEY: Invalid filter setting '%s', using off",
			            filter_choice.data());

		channel->SetHighPassFilter(FilterState::Off);
		channel->SetLowPassFilter(FilterState::Off);
	}

	// Register port handlers for 8-bit IO
	const auto write_data = std::bind(&Disney::WriteData, this, _1, _2, _3);
	data_write_handler.Install(base_port, write_data, io_width_t::byte);

	const auto status_port = static_cast<io_port_t>(base_port + 1u);
	const auto read_status = std::bind(&Disney::ReadStatus, this, _1, _2);
	status_read_handler.Install(status_port, read_status, io_width_t::byte, 2);

	const auto control_port = static_cast<io_port_t>(base_port + 2u);
	const auto write_control = std::bind(&Disney::WriteControl, this, _1, _2, _3);
	control_write_handler.Install(control_port, write_control, io_width_t::byte);

	// Update our status to indicate we're ready
	status_reg.error = false;
	status_reg.busy  = false;

	LOG_MSG("DISNEY: Disney Sound Source available on LPT1 port %03xh", base_port);
}

Disney::~Disney()
{
	LOG_MSG("DISNEY: Shutting down");

	// Update our status to indicate we're no longer ready
	status_reg.error = true;
	status_reg.busy  = true;

	// Stop the game from accessing the IO ports
	status_read_handler.Uninstall();
	data_write_handler.Uninstall();
	control_write_handler.Uninstall();

	channel->Enable(false);

	fifo         = {};
	render_queue = {};
}

void DISNEY_ShutDown([[maybe_unused]] Section *sec)
{
	disney.reset();
}

void DISNEY_Init(Section *sec)
{
	Section_prop *section = static_cast<Section_prop *>(sec);
	assert(section);

	if (!section->Get_bool("disney")) {
		DISNEY_ShutDown(nullptr);
		return;
	}
	// Some games expect the DSS on port 378h and don't check 278h first.
	disney = std::make_unique<Disney>(Lpt1Port,
	                                  section->Get_string("disney_filter"));

	sec->AddDestroyFunction(&DISNEY_ShutDown, true);
}
