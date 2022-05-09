/*
 *  Copyright (C) 2019-2022  The DOSBox Staging Team
 *  Copyright (C) 2002-2017  The DOSBox Team
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

#include "gameblaster.h"

#include "setup.h"
#include "support.h"
#include "pic.h"

void GameBlaster::Open(const int port_choice, const std::string_view card_choice)
{
	Close();
	assert(!is_open);

	is_standalone_gameblaster = (card_choice == "gb");

	// Ports are filtered and corrected by the conf system, so we simply
	// assert here
	const std::vector<io_port_t> valid_gb_ports = {0x210, 0x220, 0x230, 0x240, 0x250, 0x260};
	const std::vector<io_port_t> valid_cms_ports = {0x220, 0x240, 0x260, 0x280, 0x2a0, 0x2c0, 0x2e0, 0x300};
	const auto valid_ports = is_standalone_gameblaster ? valid_gb_ports
	                                                   : valid_cms_ports;
	base_port = check_cast<io_port_t>(port_choice);
	assert(contains(valid_ports, base_port));

	// Create the SAA1099 devices
	for (auto &d : devices) {
		d = std::make_unique<saa1099_device>(machine_config(), "", nullptr, chip_clock);
		d->device_start();
	}

	// Creative included CMS chips on several Sound Blaster cards, which
	// games could use (in addition to the SB features), so we always setup
	// those handlers - even if the card type isn't a GameBlaster.
	using namespace std::placeholders;
	const auto data_to_left = std::bind(&GameBlaster::WriteDataToLeftDevice, this, _1, _2, _3);
	const auto control_to_left = std::bind(&GameBlaster::WriteControlToLeftDevice, this, _1, _2, _3);
	const auto data_to_right = std::bind(&GameBlaster::WriteDataToRightDevice, this, _1, _2, _3);
	const auto control_to_right = std::bind(&GameBlaster::WriteControlToRightDevice, this, _1, _2, _3);

	write_handlers[0].Install(base_port, data_to_left, io_width_t::byte);
	write_handlers[1].Install(base_port + 1, control_to_left, io_width_t::byte);
	write_handlers[2].Install(base_port + 2, data_to_right, io_width_t::byte);
	write_handlers[3].Install(base_port + 3, control_to_right, io_width_t::byte);

	// However, standalone GameBlaster cards came with a dedicated chip on
	// it that could be used for detection. So we setup those handlers for
	// this chip only if the card-type is a GameBlaster:
	if (is_standalone_gameblaster) {
		const auto read_from_detection_port = std::bind(&GameBlaster::ReadFromDetectionPort, this, _1, _2);
		const auto write_to_detection_port = std::bind(&GameBlaster::WriteToDetectionPort, this, _1, _2, _3);

		read_handler_for_detection.Install(base_port, read_from_detection_port, io_width_t::byte, 16);
		write_handler_for_detection.Install(base_port + 4,
		                                    write_to_detection_port,
		                                    io_width_t::byte,
		                                    12);
	}

	const auto audio_callback = std::bind(&GameBlaster::AudioCallback, this, _1);
	channel = MIXER_AddChannel(audio_callback, frame_rate_hz, CardName());

	LOG_MSG("%s: Running on port %xh with two %0.3f MHz Phillips SAA-1099 chips",
	        CardName(),
	        base_port,
	        chip_clock / 1e6);

	assert(channel);
	assert(devices[0]);
	assert(devices[1]);

	is_open = true;
}

GameBlaster::frame_t GameBlaster::RenderOnce()
{
	static frame_t input = {};
	static int16_t *buffer[] = {&input[0], &input[1]};
	static device_sound_interface::sound_stream stream;

	frame_t output = {};
	for (const auto &d : devices) {
		d->sound_stream_update(stream, 0, buffer, 1);
		output[0] += input[0];
		output[1] += input[1];
	}
	return output;
}

void GameBlaster::RenderForMs(const double duration_ms)
{
	auto render_count = iround(duration_ms * frame_rate_per_ms);
	while (render_count-- > 0)
		fifo.emplace(RenderOnce());
}

void GameBlaster::RenderUpToNow()
{
	const auto now = PIC_FullIndex();
	if (channel->is_enabled)
		RenderForMs(now - last_render_time);
	else
		channel->Enable(true);
	last_render_time = now;
	unwritten_for_ms = 0;
}

void GameBlaster::WriteDataToLeftDevice(io_port_t, io_val_t value, io_width_t)
{
	RenderUpToNow();
	devices[0]->data_w(0, 0, check_cast<uint8_t>(value));
}

void GameBlaster::WriteControlToLeftDevice(io_port_t, io_val_t value, io_width_t)
{
	RenderUpToNow();
	devices[0]->control_w(0, 0, check_cast<uint8_t>(value));
}

void GameBlaster::WriteDataToRightDevice(io_port_t, io_val_t value, io_width_t)
{
	RenderUpToNow();
	devices[1]->data_w(0, 0, check_cast<uint8_t>(value));
}

void GameBlaster::WriteControlToRightDevice(io_port_t, io_val_t value, io_width_t)
{
	RenderUpToNow();
	devices[1]->control_w(0, 0, check_cast<uint8_t>(value));
}

double GameBlaster::ConvertFramesToMs(const int frames) const
{
	return frames / frame_rate_per_ms;
}

void GameBlaster::AudioCallback(uint16_t requested_frames)
{
	assert(channel);
	while (requested_frames && fifo.size()) {
		channel->AddSamples_s16(1, fifo.front().data());
		fifo.pop();
		--requested_frames;
	}

	if (requested_frames) {
		last_render_time += ConvertFramesToMs(requested_frames);
		while (requested_frames--) {
			const auto frame = RenderOnce();
			channel->AddSamples_s16(1, frame.data());
		}
	}
	// Pause the card if it hasn't been written to for 10 seconds
	if (unwritten_for_ms++ > 10000)
		channel->Enable(false);
}

void GameBlaster::WriteToDetectionPort(io_port_t port, io_val_t value, io_width_t)
{
	switch (port - base_port) {
	case 0x6:
	case 0x7: cms_detect_register = check_cast<uint8_t>(value); break;
	}
}

uint8_t GameBlaster::ReadFromDetectionPort(io_port_t port, io_width_t) const
{
	uint8_t retval = 0xff;
	switch (port - base_port) {
	case 0x4:
		retval = 0x7f;
		break;
	case 0xa:
	case 0xb:
		retval = cms_detect_register;
		break;
	}
	return retval;
}

const char *GameBlaster::CardName() const
{
	return is_standalone_gameblaster ? "GAMEBLASTER" : "CMS";
}

void GameBlaster::Close()
{
	if (!is_open)
		return;

	LOG_INFO("%s: Shutting down the card on port %xh", CardName(), base_port);

	// Drop access to the IO ports
	for (auto &w : write_handlers)
		w.Uninstall();
	write_handler_for_detection.Uninstall();
	read_handler_for_detection.Uninstall();

	// Stop playback
	if (channel)
		channel->Enable(false);

	// Remove the mixer channel and the SAA1099 devices
	channel.reset();
	devices[0].reset();
	devices[1].reset();

	is_open = false;
}

GameBlaster gameblaster;
void CMS_Init(Section *configuration)
{
	Section_prop *section = static_cast<Section_prop *>(configuration);
	gameblaster.Open(section->Get_hex("sbbase"), section->Get_string("sbtype"));
}
void CMS_ShutDown([[maybe_unused]] Section* sec) {
	gameblaster.Close();
}
