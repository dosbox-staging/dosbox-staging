// SPDX-FileCopyrightText:  2021-2026 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "private/innovation.h"

#include <memory>

#include "audio/channel_names.h"
#include "config/config.h"
#include "hardware/pic.h"
#include "misc/notifications.h"
#include "misc/support.h"
#include "utils/checks.h"

CHECK_NARROWING();

Innovation::Innovation(const std::string_view model_choice,
                       const std::string_view clock_choice,
                       const int filter_strength_6581,
                       const int filter_strength_8580, const int port_choice,
                       const std::string& channel_filter_choice)
{
	using namespace std::placeholders;

	int filter_strength = 0;
	auto sid_service    = std::make_unique<reSIDfp::SID>();

	// Setup the model and filter
	if (model_choice == "8580") {
		sid_service->setChipModel(reSIDfp::MOS8580);
		filter_strength = filter_strength_8580;
		if (filter_strength > 0) {
			sid_service->enableFilter(true);
			sid_service->setFilter8580Curve(filter_strength / 100.0);
		}
	} else {
		sid_service->setChipModel(reSIDfp::MOS6581);
		filter_strength = filter_strength_6581;
		if (filter_strength > 0) {
			sid_service->enableFilter(true);
			sid_service->setFilter6581Curve(filter_strength / 100.0);
		}
	}

	// Determine chip clock frequency
	if (clock_choice == "default") {
		chip_clock = 894886.25;
	} else if (clock_choice == "c64ntsc") {
		chip_clock = 1022727.14;
	} else if (clock_choice == "c64pal") {
		chip_clock = 985250.0;
	} else if (clock_choice == "hardsid") {
		chip_clock = 1000000.0;
	}
	assert(chip_clock);

	ms_per_clock = MillisInSecond / chip_clock;

	MIXER_LockMixerThread();

	// Setup the mixer and get it's sampling rate
	const auto mixer_callback = std::bind(&Innovation::AudioCallback, this, _1);

	auto mixer_channel = MIXER_AddChannel(mixer_callback,
	                                      UseMixerRate,
	                                      ChannelName::InnovationSsi2001,
	                                      {ChannelFeature::Sleep,
	                                       ChannelFeature::ReverbSend,
	                                       ChannelFeature::ChorusSend,
	                                       ChannelFeature::Synthesizer});

	if (!mixer_channel->TryParseAndSetCustomFilter(channel_filter_choice)) {
		const auto filter_choice_has_bool = parse_bool_setting(
		        channel_filter_choice);

		if (!filter_choice_has_bool) {
			NOTIFY_DisplayWarning(Notification::Source::Console,
			                      "INNOVATION",
			                      "PROGRAM_CONFIG_INVALID_SETTING",
			                      "innovation_filter",
			                      channel_filter_choice.c_str(),
			                      "off");
		}

		mixer_channel->SetHighPassFilter(FilterState::Off);
		mixer_channel->SetLowPassFilter(FilterState::Off);

		set_section_property_value("innovation", "innovation_filter", "off");
	}

	const auto sample_rate_hz = mixer_channel->GetSampleRate();

	// Determine the passband frequency, which is capped at 90% of Nyquist.
	const double passband = 0.9 * sample_rate_hz / 2;

	// Assign the sampling parameters
	sid_service->setSamplingParameters(chip_clock,
	                                   reSIDfp::RESAMPLE,
	                                   sample_rate_hz,
	                                   passband);

	// Setup and assign the port address
	const auto read_from = std::bind(&Innovation::ReadFromPort, this, _1, _2);
	const auto write_to = std::bind(&Innovation::WriteToPort, this, _1, _2, _3);
	base_port = check_cast<io_port_t>(port_choice);
	read_handler.Install(base_port, read_from, io_width_t::byte, 0x20);
	write_handler.Install(base_port, write_to, io_width_t::byte, 0x20);

	// Move the locals into members
	service = std::move(sid_service);
	channel = std::move(mixer_channel);

	// Ready state-values for rendering
	last_rendered_ms = 0.0;

	// Variable model_name is only used for logging, so use a const char* here
	const char* model_name  = model_choice == "8580" ? "8580" : "6581";
	constexpr auto us_per_s = 1'000'000.0;
	if (filter_strength == 0) {
		LOG_MSG("INNOVATION: Running on port %xh with a SID %s at %0.3f MHz",
		        base_port,
		        model_name,
		        chip_clock / us_per_s);
	} else {
		LOG_MSG("INNOVATION: Running on port %xh with a SID %s at %0.3f MHz filtering at %d%%",
		        base_port,
		        model_name,
		        chip_clock / us_per_s,
		        filter_strength);
	}

	MIXER_UnlockMixerThread();
}

Innovation::~Innovation()
{
	LOG_MSG("INNOVATION: Shutting down");

	MIXER_LockMixerThread();

	// Stop playback
	if (channel) {
		channel->Enable(false);
	}

	// Remove the IO handlers before removing the SID device
	read_handler.Uninstall();
	write_handler.Uninstall();

	// Deregister the mixer channel and remove it
	assert(channel);
	MIXER_DeregisterChannel(channel);
	channel.reset();

	// Reset the members
	channel.reset();
	service.reset();

	MIXER_UnlockMixerThread();
}

uint8_t Innovation::ReadFromPort(io_port_t port, io_width_t)
{
	std::lock_guard lock(mutex);
	const auto sid_port = static_cast<io_port_t>(port - base_port);
	return service->read(sid_port);
}

void Innovation::WriteToPort(io_port_t port, io_val_t value, io_width_t)
{
	std::lock_guard lock(mutex);

	RenderUpToNow();

	const auto data     = check_cast<uint8_t>(value);
	const auto sid_port = static_cast<io_port_t>(port - base_port);
	service->write(sid_port, data);
}

void Innovation::RenderUpToNow()
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
		last_rendered_ms += ms_per_clock;
		if (float frame = 0.0f; MaybeRenderFrame(frame)) {
			fifo.emplace(frame);
		}
	}
}

bool Innovation::MaybeRenderFrame(float& frame)
{
	assert(service);

	int16_t sample = {0};

	const auto frame_is_ready = service->clock(1, &sample);

	// Get the frame
	if (frame_is_ready) {
		frame = static_cast<float>(sample * 2);
	}

	return frame_is_ready;
}

void Innovation::AudioCallback(const int requested_frames)
{
	assert(channel);

	std::lock_guard lock(mutex);

	// if (fifo.size())
	//	LOG_MSG("INNOVATION: Queued %2lu cycle-accurate frames",
	// fifo.size());

	auto frames_remaining = requested_frames;

	// First, send any frames we've queued since the last callback
	while (frames_remaining && fifo.size()) {
		channel->AddSamples_mfloat(1, &fifo.front());
		fifo.pop();
		--frames_remaining;
	}
	// If the queue's run dry, render the remainder and sync-up our time datum
	while (frames_remaining) {
		if (float frame = 0.0f; MaybeRenderFrame(frame)) {
			channel->AddSamples_mfloat(1, &frame);
		}
		--frames_remaining;
	}
	last_rendered_ms = PIC_AtomicIndex();
}

static std::unique_ptr<Innovation> innovation = {};

void INNOVATION_Init()
{
	const auto section = get_section("innovation");

	const auto model_choice         = section->GetString("sidmodel");
	const auto clock_choice         = section->GetString("sidclock");
	const auto port_choice          = section->GetHex("sidport");
	const auto filter_strength_6581 = section->GetInt("6581filter");
	const auto filter_strength_8580 = section->GetInt("8580filter");
	const auto channel_filter_choice = section->GetString("innovation_filter");

	if (has_false(model_choice)) {
		return;
	}

	innovation = std::make_unique<Innovation>(model_choice,
	                                          clock_choice,
	                                          filter_strength_6581,
	                                          filter_strength_8580,
	                                          port_choice,
	                                          channel_filter_choice);
}

void INNOVATION_Destroy()
{
	innovation = {};
}

static void notify_innovation_setting_updated([[maybe_unused]] SectionProp& section,
                                              [[maybe_unused]] const std::string& prop_name)
{
	INNOVATION_Destroy();
	INNOVATION_Init();
}

static void init_innovation_config_settings(SectionProp& sec_prop)
{
	constexpr auto when_idle = Property::Changeable::WhenIdle;

	// Chip type
	auto* str_prop = sec_prop.AddString("sidmodel", when_idle, "none");
	str_prop->SetValues({"auto", "6581", "8580", "none"});
	str_prop->SetHelp(
	        "Model of chip to emulate in the Innovation SSI-2001 card ('none' by default).\n"
	        "Possible values:\n"
	        "\n"
	        "  auto:  Use the 6581 chip.\n"
	        "\n"
	        "  6581:  The original chip, known for its bassy and rich character.\n"
	        "\n"
	        "  8580:  A later revision that more closely matched the SID specification.\n"
	        "         It fixed the 6581's DC bias and is less prone to distortion.\n"
	        "         The 8580 is an option on reproduction cards, like the DuoSID.\n"
	        "\n"
	        "  none:  Disable the card (default).");

	// Chip clock frequency
	str_prop = sec_prop.AddString("sidclock", when_idle, "default");
	str_prop->SetValues({"default", "c64ntsc", "c64pal", "hardsid"});
	str_prop->SetHelp(
	        "The SID chip's clock frequency, which is jumperable on reproduction cards\n"
	        "('default' by default). Possible values:\n"
	        "\n"
	        "  default:  0.895 MHz, per the original SSI-2001 card (default).\n"
	        "  c64ntsc:  1.023 MHz, per NTSC Commodore PCs and the DuoSID.\n"
	        "  c64pal:   0.985 MHz, per PAL Commodore PCs and the DuoSID.\n"
	        "  hardsid:  1.000 MHz, available on the DuoSID.");

	// IO Address
	auto* hex_prop = sec_prop.AddHex("sidport", when_idle, 0x280);
	hex_prop->SetValues({"240", "260", "280", "2a0", "2c0"});
	hex_prop->SetHelp(
	        "The IO port address of the Innovation SSI-2001 (280 by default).\n"
	        "Possible values: 240, 260, 280, 2a0, 2c0");

	// Filter strengths
	auto* int_prop = sec_prop.AddInt("6581filter", when_idle, 50);
	int_prop->SetMinMax(0, 100);
	int_prop->SetHelp(
	        "Adjusts the 6581's filtering strength as a percentage from 0 to 100 (50 by\n"
	        "default). The SID's analog filtering meant that each chip was physically unique.");

	int_prop = sec_prop.AddInt("8580filter", when_idle, 50);
	int_prop->SetMinMax(0, 100);
	int_prop->SetHelp(
	        "Adjusts the 8580's filtering strength as a percentage from 0 to 100 (50 by\n"
	        "default).");

	str_prop = sec_prop.AddString("innovation_filter", when_idle, "off");
	assert(str_prop);
	str_prop->SetHelp(
	        "Filter for the Innovation audio output ('off' by default). Possible values:\n"
	        "\n"
	        "  off:       Don't filter the output (default).\n"
	        "  <custom>:  Custom filter definition; see 'sb_filter' for details.");
}

void INNOVATION_AddConfigSection(const ConfigPtr& conf)
{
	assert(conf);

	auto section = conf->AddSection("innovation");
	section->AddUpdateHandler(notify_innovation_setting_updated);

	init_innovation_config_settings(*section);
}
