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
#include <memory>
#include <string.h>

#include "inout.h"
#include "mixer.h"
#include "pic.h"
#include "setup.h"
#include "support.h"

static constexpr uint8_t buffer_samples = 128;

struct DisneyDac {
	uint8_t buffer[buffer_samples] = {};
	uint8_t used = 0; // current data buffer level
	double speedcheck_sum = 0.0;
	double speedcheck_last = 0.0;
	bool speedcheck_failed = false;
	bool speedcheck_init = false;
};

class Disney {
public:
	Disney(const std::string &filter_pref);
	~Disney();

private:
	Disney()                          = delete;
	Disney(const Disney &)            = delete;
	Disney &operator=(const Disney &) = delete;

	// Types and enums
	enum State { Idle, Running, Finished, Analyzing };

	void Analyze(DisneyDac &dac);
	void Enable(const int rate_hz);

	int CalcSampleRate(const DisneyDac &dac);
	void PlayStereo(uint16_t len, const uint8_t *l, const uint8_t *r);

	void AudioCallback(uint16_t len);
	void UpdateFilterFreq(const uint16_t rate_hz);

	uint8_t ReadFromPort(const io_port_t port, io_width_t);
	void WriteToPort(const io_port_t port, const io_val_t value, io_width_t);

	// Constants
	static constexpr uint16_t base_port = 0x0378;

	// The Disney Sound Source is an LPT DAC with a fixed sample rate of 7kHz.
	static constexpr auto base_rate_hz = 7000;

	// Managed and compound objects
	IO_ReadHandleObject read_handler{};
	IO_WriteHandleObject write_handler{};

	mixer_channel_t channel = nullptr;

	// the D/A channels
	DisneyDac da[2] = {};

	// For mono-output, the analysis step points the leader to the channel
	// with the most rendered-samples. We use the left channel as a valid
	// place-holder prior to the first analysis. This ensures the
	// leader is always valid, such as in cases where the callback in
	// enabled before the analysis.
	DisneyDac *leader = &da[0];

	State state = State::Idle;

	uint32_t interface_det = 0;
	uint32_t interface_det_ext = 0;

	bool stereo         = false;
	bool filter_enabled = false;

	// parallel port registers
	uint8_t data    = 0;
	uint8_t control = 0;
};

void Disney::UpdateFilterFreq(const uint16_t rate_hz)
{
	if (!filter_enabled)
		return;

	// Disney only supports a single fixed 7kHz sample rate; the 6dB/oct LPF @
	// 1kHz is an accurate emulation of how the actual Disney lowpass filter
	// sounds.
	//
	// With Covox one can theorically achieve any sample rate as long as the
	// CPU can keep up. Most Covox adapters have no lowpass filter whatsoever,
	// but as the aim here is to make the sound more pleasant to listen to,
	// we'll apply a gentle 6dB/oct LPF at a bit below half the sample rate to
	// tame the harshest aliased frequencies while still retaining a good dose
	// of the "raw crunchy DAC sound".
	constexpr uint16_t default_cutoff_freq = 1000;
	const auto cutoff_freq                 = (rate_hz == base_rate_hz)
	                                               ? default_cutoff_freq
	                                               : static_cast<uint16_t>(rate_hz * 0.45f);

	constexpr auto order = 1;
	channel->ConfigureLowPassFilter(order, cutoff_freq);
}

void Disney::Enable(const int rate_hz)
{
	if (rate_hz < 500 || rate_hz > 100000) {
		// try again..
		state = State::Idle;
		return;
	}
#if 0
	LOG(LOG_MISC,LOG_NORMAL)("DISNEY: enabled at %d Hz in %s", rate_hz,
	                         stereo ? "stereo" : "mono");
#endif
	channel->SetSampleRate(rate_hz);
	channel->Enable(true);

	constexpr auto max_filter_freq = 44100;
	const auto filter_freq         = std::min(rate_hz, max_filter_freq);
	UpdateFilterFreq(check_cast<uint16_t>(filter_freq));

	state = State::Running;
}

// Calculate the sample rate from DAC samples and speed parameters.
// The maximum possible sample rate is 127,000 Hz which occurs when all 128
// samples are used and the speedcheck_sum has accumulated only one single
// tick.
int Disney::CalcSampleRate(const DisneyDac &dac)
{
	if (dac.used <= 1)
		return 0;

	const auto k_samples = 1000 * (dac.used - 1);

	const auto rate_hz = k_samples / dac.speedcheck_sum;

	return iround(rate_hz);
}

void Disney::Analyze(DisneyDac &dac)
{
	switch (state) {
	case State::Running: // should not get here
		break;
	case State::Idle:
		// initialize channel data
		for (int i = 0; i < 2; i++) {
			da[i].used              = 0;
			da[i].speedcheck_sum    = 0;
			da[i].speedcheck_failed = false;
			da[i].speedcheck_init   = false;
		}
		dac.speedcheck_last = PIC_FullIndex();
		dac.speedcheck_init = true;

		state = State::Analyzing;
		break;

	case State::Finished: {
		// The leading channel has the most populated samples
		leader = da[0].used > da[1].used ? &da[0] : &da[1];

		// Stereo-mode if both DACs are similarly filled
		const auto st_diff = abs(da[0].used - da[1].used);
		stereo             = (st_diff < 5);

		// Run with the greater DAC sample rate
		const auto max_rate_hz = std::max(CalcSampleRate(da[0]),
		                                  CalcSampleRate(da[1]));
		Enable(max_rate_hz);
	} break;

	case State::Analyzing: {
		const auto current = PIC_FullIndex();

		if (!dac.speedcheck_init) {
			dac.speedcheck_init = true;
			dac.speedcheck_last = current;
			break;
		}
		const auto speed_delta = current - dac.speedcheck_last;
		dac.speedcheck_sum += speed_delta;
		// LOG_MSG("t=%f",current - dac.speedcheck_last);

		// sanity checks (printer...)
		if (speed_delta < 0.01 || speed_delta > 2)
			dac.speedcheck_failed = true;

		// if both are failed we are back at start
		if (da[0].speedcheck_failed && da[1].speedcheck_failed) {
			state = State::Idle;
			break;
		}

		dac.speedcheck_last = current;

		// analyze finish condition
		if (da[0].used > 30 || da[1].used > 30)
			state = State::Finished;
	} break;
	}
}

void Disney::WriteToPort(const io_port_t port, const io_val_t value, io_width_t)
{
	assert(channel);
	channel->WakeUp();

	const auto val = check_cast<uint8_t>(value);

	switch (port - base_port) {
	case 0: /* Data Port */
	{
		data = val;
		// if data is written here too often without using the stereo
		// mechanism we use the simple DAC machanism.
		if (state != State::Running) {
			interface_det++;
			if (interface_det > 5)
				Analyze(da[0]);
		}
		if (interface_det > 5) {
			if (da[0].used < buffer_samples) {
				da[0].buffer[da[0].used] = data;
				da[0].used++;
			} // else LOG_MSG("disney overflow 0");
		}
		break;
	}
	case 1:		/* Status Port */
		LOG(LOG_MISC, LOG_NORMAL)("DISNEY:Status write %u", val);
		break;
	case 2:		/* Control Port */
		if ((control & 0x2) && !(val & 0x2)) {
			if (state != State::Running) {
				interface_det     = 0;
				interface_det_ext = 0;
				Analyze(da[1]);
			}

			// stereo channel latch
			if (da[1].used < buffer_samples) {
				da[1].buffer[da[1].used] = data;
				da[1].used++;
			} // else LOG_MSG("disney overflow 1");
		}

		if ((control & 0x1) && !(val & 0x1)) {
			if (state != State::Running) {
				interface_det     = 0;
				interface_det_ext = 0;
				Analyze(da[0]);
			}
			// stereo channel latch
			if (da[0].used < buffer_samples) {
				da[0].buffer[da[0].used] = data;
				da[0].used++;
			} // else LOG_MSG("disney overflow 0");
		}

		if ((control & 0x8) && !(val & 0x8)) {
			// emulate a device with 16-byte sound FIFO
			if (state != State::Running) {
				interface_det_ext++;
				interface_det = 0;
				if (interface_det_ext > 5) {
					leader = &da[0];
					Enable(base_rate_hz);
				}
			}
			if (interface_det_ext > 5) {
				if (da[0].used < buffer_samples) {
					da[0].buffer[da[0].used] = data;
					da[0].used++;
				}
			}
		}

//		LOG_WARN("DISNEY:Control write %x",val);
		if (val&0x10) LOG(LOG_MISC,LOG_ERROR)("DISNEY:Parallel IRQ Enabled");
		control = val;
		break;
	}
}

uint8_t Disney::ReadFromPort(const io_port_t port, io_width_t)
{
	uint8_t retval;
	switch (port - base_port) {
	case 0: /* Data Port */
		// LOG(LOG_MISC,LOG_NORMAL)("DISNEY:Read from data port");
		return data;
		break;
	case 1:               /* Status Port */
		retval = 0x07;//0x40; // Stereo-on-1 and (or) New-Stereo DACs present
		if (interface_det_ext > 5) {
			if (leader && leader->used >= 16) {
				retval |= 0x40; // ack
				retval &= ~0x4; // interrupt
			}
		}
		if (!(data & 0x80))
			retval |= 0x80; // pin 9 is wired to pin 11
		return retval;
		break;
	case 2:		/* Control Port */
		LOG(LOG_MISC,LOG_NORMAL)("DISNEY:Read from control port");
		return control;
		break;
	}
	return 0xff;
}

void Disney::PlayStereo(uint16_t len, const uint8_t *l, const uint8_t *r)
{
	static uint8_t stereodata[buffer_samples * 2];
	for (uint16_t i = 0; i < len; ++i) {
		stereodata[i*2] = l[i];
		stereodata[i*2+1] = r[i];
	}
	channel->AddSamples_s8(len, stereodata);
}

void Disney::AudioCallback(uint16_t len)
{
	if (!len || !channel)
		return;

	// get the smaller used
	uint16_t real_used;
	if (stereo) {
		real_used = da[0].used;
		if (da[1].used < real_used)
			real_used = da[1].used;
	} else
		real_used = leader->used;

	if (real_used >= len) { // enough data for now
		if (stereo)
			PlayStereo(len, da[0].buffer, da[1].buffer);
		else
			channel->AddSamples_m8(len, leader->buffer);

		// put the rest back to start
		for (uint8_t i = 0; i < 2; ++i) {
			// TODO for mono only one
			memmove(da[i].buffer,
			        &da[i].buffer[len],
			        buffer_samples /*real_used*/ - len);
			da[i].used = check_cast<uint8_t>(da[i].used - len);
		}
	// TODO: len > DISNEY
	} else { // not enough data
		if (stereo) {
			uint8_t gapfiller0 = 128;
			uint8_t gapfiller1 = 128;
			if (real_used) {
				gapfiller0 = da[0].buffer[real_used - 1];
				gapfiller1 = da[1].buffer[real_used - 1];
			};

			memset(da[0].buffer + real_used, gapfiller0, len - real_used);
			memset(da[1].buffer + real_used, gapfiller1, len - real_used);

			PlayStereo(len, da[0].buffer, da[1].buffer);
			len -= real_used;

		} else {                         // mono
			uint8_t gapfiller = 128; // Keep the middle
			if (real_used) {
				// fix for some stupid game; it outputs 0 at the end of the stream
				// causing a click. So if we have at least two bytes availible in the
				// buffer and the last one is a 0 then ignore that.
				if (leader->buffer[real_used - 1] == 0)
					real_used--;
			}
			// do it this way because AddSilence sounds like a gnawing mouse
			if (real_used)
				gapfiller = leader->buffer[real_used - 1];
			//LOG_MSG("gapfiller %x, fill len %d, realused %d",gapfiller,len-real_used,real_used);
			memset(leader->buffer + real_used, gapfiller, len - real_used);
			channel->AddSamples_m8(len, leader->buffer);
		}
		da[0].used = 0;
		da[1].used = 0;

		//LOG_MSG("disney underflow %d",len - real_used);
	}
}

Disney::~Disney()
{
	DEBUG_LOG_MSG("DISNEY: Shutting down");

	// Stop and remove the mixer callback
	if (channel) {
		channel->Enable(false);
		channel.reset();
	}

	// Stop the game from accessing the IO ports
	read_handler.Uninstall();
	write_handler.Uninstall();
}

Disney::Disney(const std::string &filter_pref)
{
	using namespace std::placeholders;
	const auto audio_callback = std::bind(&Disney::AudioCallback, this, _1);

	// Setup the mixer callback
	disney.chan = MIXER_AddChannel(DISNEY_CallBack,
	                               0,
	                               "DISNEY",
	                               {ChannelFeature::Sleep,
	                                ChannelFeature::ReverbSend,
	                                ChannelFeature::ChorusSend,
	                                ChannelFeature::DigitalAudio});

	// Setup zero-order-hold resampler to emulate the "crunchiness" of early
	// DACs
	assert(channel);
	const auto rate_hz = check_cast<uint16_t>(channel->GetSampleRate());

	channel->ConfigureZeroOrderHoldUpsampler(rate_hz);
	channel->EnableZeroOrderHoldUpsampler();

	if (filter_pref == "on") {
		filter_enabled = true;
		UpdateFilterFreq(rate_hz);
		channel->SetLowPassFilter(FilterState::On);
	} else {
		if (filter_pref != "off")
			LOG_WARNING("DISNEY: Invalid filter setting '%s', using off",
			            filter_pref.c_str());

		filter_enabled = false;
		channel->SetLowPassFilter(FilterState::Off);
	}

	// Register port handlers for 8-bit IO
	const auto read_from_port = std::bind(&Disney::ReadFromPort, this, _1, _2);
	const auto write_to_port = std::bind(&Disney::WriteToPort, this, _1, _2, _3);

	write_handler.Install(base_port, write_to_port, io_width_t::byte, 3);
	read_handler.Install(base_port, read_from_port, io_width_t::byte, 3);

	// Initialize the Disney states
	control = 0;
}

// The Tandy DAC and PSG (programmable sound generator) managed pointers
std::unique_ptr<Disney> disney = {};

void DISNEY_ShutDown([[maybe_unused]] Section *sec)
{
	disney.reset();
}

void DISNEY_Init(Section *sec)
{
	Section_prop *section = static_cast<Section_prop *>(sec);
	assert(section);

	if (!section->Get_bool("disney")) {
		DISNEY_ShutDown(sec);
		return;
	}

	std::string filter_pref = section->Get_string("filter");

	disney = std::make_unique<Disney>(filter_pref);

	sec->AddDestroyFunction(&DISNEY_ShutDown, true);
}
