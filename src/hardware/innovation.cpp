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

#include "innovation.h"

#include "control.h"
#include "pic.h"

// Innovation Settings
// -------------------
constexpr uint16_t SAMPLES_PER_BUFFER = 2048;

void Innovation::Open(const std::string &model_choice,
                      const std::string &clock_choice,
                      const int filter_strength_6581,
                      const int filter_strength_8580,
                      const int port_choice)
{
	Close();

	// Sentinel
	if (model_choice == "none")
		return;

	std::string model_name;
	int filter_strength = 0;
	auto sid_service = std::make_unique<reSIDfp::SID>();

	// Setup the model and filter
	if (model_choice == "8580") {
		model_name = "8580";
		sid_service->setChipModel(reSIDfp::MOS8580);
		filter_strength = filter_strength_8580;
		if (filter_strength > 0) {
			sid_service->enableFilter(true);
			sid_service->setFilter8580Curve(filter_strength / 100.0);
		}
	} else {
		model_name = "6581";
		sid_service->setChipModel(reSIDfp::MOS6581);
		filter_strength = filter_strength_6581;
		if (filter_strength > 0) {
			sid_service->enableFilter(true);
			sid_service->setFilter6581Curve(filter_strength / 100.0);
		}
	}

	// Determine chip clock frequency
	if (clock_choice == "default")
		chip_clock = 894886.25;
	else if (clock_choice == "c64ntsc")
		chip_clock = 1022727.14;
	else if (clock_choice == "c64pal")
		chip_clock = 985250;
	else if (clock_choice == "hardsid")
		chip_clock = 1000000;
	assert(chip_clock);

	// Setup the mixer and get it's sampling rate
	using namespace std::placeholders;
	const auto mixer_callback = std::bind(&Innovation::MixerCallBack, this, _1);
	channel_t mixer_channel(MIXER_AddChannel(mixer_callback, 0, "INNOVATION"),
	                        MIXER_DelChannel);
	sid_sample_rate = mixer_channel->GetSampleRate();

	// Determine the passband frequency, which is capped at 90% of Nyquist.
	const double passband = 0.9 * sid_sample_rate / 2;

	// Assign the sampling parameters
	sid_service->setSamplingParameters(chip_clock, reSIDfp::RESAMPLE,
	                                   sid_sample_rate, passband);

	// Setup and assign the port address
	const auto read_from = std::bind(&Innovation::ReadFromPort, this, _1, _2);
	const auto write_to = std::bind(&Innovation::WriteToPort, this, _1, _2, _3);
	base_port = static_cast<uint16_t>(port_choice);
	read_handler.Install(base_port, read_from, io_width_t::byte, 0x20);
	write_handler.Install(base_port, write_to, io_width_t::byte, 0x20);

	// Move the locals into members
	service = std::move(sid_service);
	channel = std::move(mixer_channel);

	// Ready state-values for rendering
	last_used = 0;
	play_buffer_pos = 0;
	keep_rendering = true;

	// Start rendering
	renderer = std::thread(std::bind(&Innovation::Render, this));
	set_thread_name(renderer, "dosbox:innovatn"); // < 16-character cap
	play_buffer = playable.Dequeue(); // populate the first play buffer

	if (filter_strength == 0)
		LOG_MSG("INNOVATION: Running on port %xh with a SID %s at %0.3f MHz",
		        base_port, model_name.c_str(), chip_clock / 1000000.0);
	else
		LOG_MSG("INNOVATION: Running on port %xh with a SID %s at %0.3f MHz filtering at %d%%",
		        base_port, model_name.c_str(), chip_clock / 1000000.0,
		        filter_strength);

	is_open = true;
}

void Innovation::Close()
{
	if (!is_open)
		return;

	DEBUG_LOG_MSG("INNOVATION: Shutting down the SSI-2001 on port %xh", base_port);

	// Stop playback
	if (channel)
		channel->Enable(false);

	// Stop rendering and drain the queues
	keep_rendering = false;
	if (!backstock.Size())
		backstock.Enqueue(std::move(play_buffer));
	while (playable.Size())
		play_buffer = playable.Dequeue();

	// Wait for the rendering thread to finish
	if (renderer.joinable())
		renderer.join();

	// Remove the IO handlers before removing the SID device
	read_handler.Uninstall();
	write_handler.Uninstall();

	// Reset the members
	channel.reset();
	service.reset();
	is_open = false;
}

uint8_t Innovation::ReadFromPort(io_port_t port, io_width_t)
{
	const auto sid_port = static_cast<uint16_t>(port - base_port);
	const std::lock_guard<std::mutex> lock(service_mutex);
	return service->read(sid_port);
}

void Innovation::WriteToPort(io_port_t port, uint8_t data, io_width_t)
{
	const auto sid_port = static_cast<uint16_t>(port - base_port);
	{ // service-lock
		const std::lock_guard<std::mutex> lock(service_mutex);
		service->write(sid_port, data);
	}
	// Turn on the channel after the data's written
	if (!last_used) {
		channel->Enable(true);
	}
	last_used = PIC_Ticks;
}

void Innovation::Render()
{
	const auto cycles_per_sample = static_cast<uint16_t>(chip_clock /
	                                                     sid_sample_rate);

	// Allocate one buffer and reuse it for the duration.
	std::vector<int16_t> buffer(SAMPLES_PER_BUFFER);

	// Populate the backstock queue using copies of the current buffer.
	while (backstock.Size() < backstock.MaxCapacity() - 1)
		backstock.Enqueue(buffer);    // copied
	backstock.Enqueue(std::move(buffer)); // moved; buffer is hollow
	assert(backstock.Size() == backstock.MaxCapacity());

	while (keep_rendering.load()) {
		// Variables populated during rendering.
		uint16_t n = 0;
		buffer = backstock.Dequeue();
		std::unique_lock<std::mutex> lock(service_mutex);

		while (n < SAMPLES_PER_BUFFER) {
			const auto buffer_pos = buffer.data() + n;
			const auto n_remaining = SAMPLES_PER_BUFFER - n;
			const auto cycles = static_cast<unsigned int>(cycles_per_sample * n_remaining);
			n += service->clock(cycles, buffer_pos);
		}
		assert(n == SAMPLES_PER_BUFFER);
		lock.unlock();

		// The buffer is now populated so move it into the playable queue.
		playable.Enqueue(std::move(buffer));
	}
}

void Innovation::MixerCallBack(uint16_t requested_samples)
{
	while (requested_samples) {
		const auto n = std::min(GetRemainingSamples(), requested_samples);
		const auto buffer_pos = play_buffer.data() + play_buffer_pos;
		channel->AddSamples_m16(n, buffer_pos);
		requested_samples -= n;
		play_buffer_pos += n;
	}
	// Stop the channel after 5 seconds of idle-time.
	if (last_used + 5000 < PIC_Ticks) {
		last_used = 0;
		channel->Enable(false);
	}
}

// Return the number of samples left to play in the current buffer.
uint16_t Innovation::GetRemainingSamples()
{
	// If the current buffer has some samples left, then return those ...
	if (play_buffer_pos < SAMPLES_PER_BUFFER)
		return SAMPLES_PER_BUFFER - play_buffer_pos;

	// Otherwise put the spent buffer in backstock and get the next buffer.
	backstock.Enqueue(std::move(play_buffer));
	play_buffer = playable.Dequeue();
	play_buffer_pos = 0; // reset the sample counter to the beginning.

	return SAMPLES_PER_BUFFER;
}

Innovation innovation;
static void innovation_destroy(MAYBE_UNUSED Section *sec)
{
	innovation.Close();
}

static void innovation_init(Section *sec)
{
	assert(sec);
	Section_prop *conf = static_cast<Section_prop *>(sec);

	const auto model_choice = conf->Get_string("sidmodel");
	const auto clock_choice = conf->Get_string("sidclock");
	const auto port_choice = conf->Get_hex("sidport");
	const auto filter_strength_6581 = conf->Get_int("6581filter");
	const auto filter_strength_8580 = conf->Get_int("8580filter");

	innovation.Open(model_choice, clock_choice, filter_strength_6581,
	                filter_strength_8580, port_choice);

	sec->AddDestroyFunction(&innovation_destroy, true);
}

static void init_innovation_dosbox_settings(Section_prop &sec_prop)
{
	constexpr auto when_idle = Property::Changeable::WhenIdle;

	// Chip type
	auto *str_prop = sec_prop.Add_string("sidmodel", when_idle, "none");
	const char *sid_models[] = {"auto", "6581", "8580", "none", 0};
	str_prop->Set_values(sid_models);
	str_prop->Set_help(
	        "Model of chip to emulate in the Innovation SSI-2001 card:\n"
	        " - auto:  Selects the 6581 chip.\n"
	        " - 6581:  The original chip, known for its bassy and rich character.\n"
	        " - 8580:  A later revision that more closely matched the SID specification.\n"
	        "          It fixed the 6581's DC bias and is less prone to distortion.\n"
	        "          The 8580 is an option on reproduction cards, like the DuoSID.\n"
	        " - none:  Disables the card.");

	// Chip clock frequency
	str_prop = sec_prop.Add_string("sidclock", when_idle, "default");
	const char *sid_clocks[] = {"default", "c64ntsc", "c64pal", "hardsid", 0};
	str_prop->Set_values(sid_clocks);
	str_prop->Set_help(
	        "The SID chip's clock frequency, which is jumperable on reproduction cards.\n"
	        " - default: uses 0.895 MHz, per the original SSI-2001 card.\n"
	        " - c64ntsc: uses 1.023 MHz, per NTSC Commodore PCs and the DuoSID.\n"
	        " - c64pal:  uses 0.985 MHz, per PAL Commodore PCs and the DuoSID.\n"
	        " - hardsid: uses 1.000 MHz, available on the DuoSID.");

	// IO Address
	auto *hex_prop = sec_prop.Add_hex("sidport", when_idle, 0x280);
	const char *sid_ports[] = {"240", "260", "280", "2a0", "2c0", 0};
	hex_prop->Set_values(sid_ports);
	hex_prop->Set_help("The IO port address of the Innovation SSI-2001.");

	// Filter strengths
	auto *int_prop = sec_prop.Add_int("6581filter", when_idle, 50);
	int_prop->SetMinMax(0, 100);
	int_prop->Set_help(
	        "The SID's analog filtering meant that each chip was physically unique.\n"
	        "Adjusts the 6581's filtering strength as a percent from 0 to 100.");

	int_prop = sec_prop.Add_int("8580filter", when_idle, 50);
	int_prop->SetMinMax(0, 100);
	int_prop->Set_help(
	        "Adjusts the 8580's filtering strength as a percent from 0 to 100.");
}

void INNOVATION_AddConfigSection(Config *conf)
{
	assert(conf);
	Section_prop *sec = conf->AddSection_prop("innovation",
	                                          &innovation_init, true);
	assert(sec);
	init_innovation_dosbox_settings(*sec);
}
