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
#include "utils/string_utils.h"

CHECK_NARROWING();

// The Innovation SSI-2001 and Microprose's "The Entertainer" used a SID 6581
// clocked at 14.318180 MHz from the ISA bus. This clock signal is
// divided by the counter and flip flops on the board by 16 to produce a base
// frequency of .89488625 MHz. The cards default to listening on ports
// 0x280-0x29D. The Entertainer also used I/O address 0x200 for identification,
// which always return the magic value 0xA5.
// https://forum.vcfed.org/index.php?threads/the-entertainer-sound-card-exposed.41255/
// https://nerdlypleasures.blogspot.com/2014/01/sid-and-dos-unlikely-but-true-bedfellows.html

constexpr auto BasePort = io_port_t{0x280};

constexpr auto ChipModel   = reSIDfp::MOS6581;
constexpr auto ChipClockHz = 14318180.0 / 16;

constexpr auto EntertainerIdPort  = io_port_t{0x200};
constexpr auto EntertainerIdValue = uint8_t{0xA5};

Innovation::Innovation(const int sid_filter_strength,
                       const std::string& channel_filter_choice)
        : ms_per_clock{MillisInSecond / ChipClockHz}
{
	using namespace std::placeholders;

	assert(ms_per_clock > 0);

	auto sid_service = std::make_unique<reSIDfp::SID>();

	// Setup the model and filter
	sid_service->setChipModel(ChipModel);

	if (sid_filter_strength > 0) {
		sid_service->enableFilter(true);
		sid_service->setFilter6581Curve(sid_filter_strength / 100.0);
	}

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
	sid_service->setSamplingParameters(ChipClockHz,
	                                   reSIDfp::RESAMPLE,
	                                   sample_rate_hz,
	                                   passband);

	// Setup and assign the port address
	const auto read_from = std::bind(&Innovation::ReadFromPort, this, _1, _2);
	const auto write_to = std::bind(&Innovation::WriteToPort, this, _1, _2, _3);

	read_handler.Install(BasePort, read_from, io_width_t::byte, 0x20);

	read_entertainer_id_handler.Install(
	        EntertainerIdPort,
	        [](io_port_t, io_width_t) { return EntertainerIdValue; },
	        io_width_t::byte,
	        1);

	write_handler.Install(BasePort, write_to, io_width_t::byte, 0x20);

	// Move the locals into members
	service = std::move(sid_service);
	channel = std::move(mixer_channel);

	// Ready state-values for rendering
	last_rendered_ms = 0.0;

	LOG_MSG("INNOVATION: Running on port %xh with filtering at %d%%",
	        BasePort,
	        sid_filter_strength);

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
	read_entertainer_id_handler.Uninstall();
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
	const auto sid_port = static_cast<io_port_t>(port - BasePort);
	return service->read(sid_port);
}

void Innovation::WriteToPort(io_port_t port, io_val_t value, io_width_t)
{
	std::lock_guard lock(mutex);

	RenderUpToNow();

	const auto data     = check_cast<uint8_t>(value);
	const auto sid_port = static_cast<io_port_t>(port - BasePort);
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

	if (section->GetBool("innovation")) {
		innovation = std::make_unique<Innovation>(
		        section->GetInt("innovation_sid_filter"),
		        section->GetString("innovation_filter"));
	}
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

	// Card state
	auto* bool_prop = sec_prop.AddBool("innovation", when_idle, false);
	assert(bool_prop);
	bool_prop->SetHelp(
	        "Enable emulation of the Innovation SSI-2001 and Microprose's \"The Entertainer\"\n"
	        "sound cards on base port of 280. These use the iconic MOS 6581 SID chip of the\n"
	        "Commodore 64 personal computer from the 1980s. Only 15 games are known to\n"
	        "support these cards.");

	// Filter strengths
	auto* int_prop = sec_prop.AddInt("innovation_sid_filter", when_idle, 50);
	assert(int_prop);
	int_prop->SetMinMax(0, 100);
	int_prop->SetHelp(
	        "Adjusts the 6581's filtering strength as a percentage from 0 to 100 (50 by\n"
	        "default). The SID's analog filtering meant that each chip was physically unique.");

	auto* str_prop = sec_prop.AddString("innovation_filter", when_idle, "off");
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
