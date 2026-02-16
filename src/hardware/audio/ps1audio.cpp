// SPDX-FileCopyrightText:  2021-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "private/ps1audio.h"

#include "private/mame/emu.h"
#include "private/mame/sn76496.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <memory>
#include <queue>

#include "audio/channel_names.h"
#include "config/config.h"
#include "config/setup.h"
#include "dosbox.h"
#include "hardware/dma.h"
#include "hardware/memory.h"
#include "hardware/pic.h"
#include "hardware/port.h"
#include "hardware/timer.h"
#include "utils/checks.h"
#include "utils/math_utils.h"

CHECK_NARROWING();

std::unique_ptr<Ps1Dac> ps1_dac = {};

static void setup_filter(MixerChannelPtr& channel, const bool filter_enabled)
{
	if (filter_enabled) {
		constexpr auto HpfOrder        = 3;
		constexpr auto HpfCutoffFreqHz = 160;

		channel->ConfigureHighPassFilter(HpfOrder, HpfCutoffFreqHz);
		channel->SetHighPassFilter(FilterState::On);

		constexpr auto LpfOrder        = 1;
		constexpr auto LpfCutoffFreqHz = 2100;

		channel->ConfigureLowPassFilter(LpfOrder, LpfCutoffFreqHz);
		channel->SetLowPassFilter(FilterState::On);

	} else {
		channel->SetHighPassFilter(FilterState::Off);
		channel->SetLowPassFilter(FilterState::Off);
	}
}

static void PS1AUDIO_PicCallback()
{
	if (!ps1_dac || !ps1_dac->channel->is_enabled) {
		return;
	}

	ps1_dac->frame_counter += ps1_dac->channel->GetFramesPerTick();
	const auto requested_frames = ifloor(ps1_dac->frame_counter);
	ps1_dac->frame_counter -= static_cast<float>(requested_frames);
	ps1_dac->PicCallback(requested_frames);
}

Ps1Dac::Ps1Dac(const std::string& filter_choice)
{
	using namespace std::placeholders;

	MIXER_LockMixerThread();

	constexpr bool Stereo = false;
	constexpr bool SignedData = false;
	constexpr bool NativeOrder = true;
	const auto callback = std::bind(MIXER_PullFromQueueCallback<Ps1Dac, uint8_t, Stereo, SignedData, NativeOrder>, _1, this);

	channel = MIXER_AddChannel(callback,
	                           UseMixerRate,
	                           ChannelName::Ps1AudioCardDac,
	                           {ChannelFeature::Sleep,
	                            ChannelFeature::ReverbSend,
	                            ChannelFeature::ChorusSend,
	                            ChannelFeature::DigitalAudio});

	// Setup DAC filters
	if (const auto maybe_bool = parse_bool_setting(filter_choice)) {
		// Using the same filter settings for the DAC as for the PSG
		// synth. It's unclear whether this is accurate, but in any
		// case, the filters do a good approximation of how a small
		// integrated speaker would sound.
		const auto filter_enabled = *maybe_bool;
		setup_filter(channel, filter_enabled);

	} else if (!channel->TryParseAndSetCustomFilter(filter_choice)) {
		LOG_WARNING(
		        "%s: Invalid 'ps1audio_dac_filter' setting: '%s', "
		        "using 'on'",
		        ChannelName::Ps1AudioCardDac,
		        filter_choice.c_str());

		const auto filter_enabled = true;
		setup_filter(channel, filter_enabled);
		set_section_property_value("speaker", "ps1audio_dac_filter", "on");
	}

	// Register DAC per-port read handlers
	read_handlers[0].Install(0x02F,
	                         std::bind(&Ps1Dac::ReadPresencePort02F, this, _1, _2),
	                         io_width_t::byte);

	read_handlers[1].Install(0x200,
	                         std::bind(&Ps1Dac::ReadCmdResultPort200, this, _1, _2),
	                         io_width_t::byte);

	read_handlers[2].Install(0x202,
	                         std::bind(&Ps1Dac::ReadStatusPort202, this, _1, _2),
	                         io_width_t::byte);

	read_handlers[3].Install(0x203,
	                         std::bind(&Ps1Dac::ReadTimingPort203, this, _1, _2),
	                         io_width_t::byte);

	read_handlers[4].Install(
	        0x204,
	        std::bind(&Ps1Dac::ReadJoystickPorts204To207, this, _1, _2),
	        io_width_t::byte,
	        3);

	// Register DAC per-port write handlers
	write_handlers[0].Install(0x200,
	                          std::bind(&Ps1Dac::WriteDataPort200, this, _1, _2, _3),
	                          io_width_t::byte);

	write_handlers[1].Install(0x202,
	                          std::bind(&Ps1Dac::WriteControlPort202, this, _1, _2, _3),
	                          io_width_t::byte);

	write_handlers[2].Install(0x203,
	                          std::bind(&Ps1Dac::WriteTimingPort203, this, _1, _2, _3),
	                          io_width_t::byte);

	write_handlers[3].Install(
	        0x204,
	        std::bind(&Ps1Dac::WriteFifoLevelPort204, this, _1, _2, _3),
	        io_width_t::byte);

	// Operate at native sampling rates
	sample_rate_hz = channel->GetSampleRate();
	last_write     = 0;
	Reset(true);

	// Size to 2x blocksize. The mixer callback will request 1x blocksize.
	// This provides a good size to avoid over-runs and stalls.
	output_queue.Resize(iceil(channel->GetFramesPerBlock() * 2.0f));
	TIMER_AddTickHandler(PS1AUDIO_PicCallback);

	MIXER_UnlockMixerThread();
}

uint8_t Ps1Dac::CalcStatus() const
{
	uint8_t status = regs.status & FifoIrqFlag;
	if (!bytes_pending) {
		status |= FifoEmptyFlag;
	}

	if (bytes_pending < (FifoNearlyEmptyVal << FracShift) &&
	    (regs.command & 3) == 3) {
		status |= FifoNearlyEmptyFlag;
	}

	if (bytes_pending > ((FifoSize - 1) << FracShift)) {
		status |= FifoFullFlag;
	}

	return status;
}

void Ps1Dac::Reset(bool should_clear_adder)
{
	PIC_DeActivateIRQ(IrqNumber);
	memset(fifo, FifoMidline, FifoSize);
	read_index      = 0;
	write_index     = 0;
	read_index_high = 0;

	// Be careful with this, 5 second timeout and Space Quest 4
	if (should_clear_adder) {
		adder = 0;
	}

	bytes_pending   = 0;
	regs.status     = CalcStatus();
	can_trigger_irq = false;
	is_playing      = true;
	is_new_transfer = true;
}

void Ps1Dac::WriteDataPort200(io_port_t, io_val_t value, io_width_t)
{
	channel->WakeUp();

	const auto data = check_cast<uint8_t>(value);
	if (is_new_transfer) {
		is_new_transfer = false;
		if (data) {
			signal_bias = static_cast<int8_t>(data - FifoMidline);
		}
	}
	regs.status = CalcStatus();
	if (!(regs.status & FifoFullFlag)) {
		const auto corrected_data = data - signal_bias;
		fifo[write_index++] = static_cast<uint8_t>(corrected_data);
		write_index &= FifoMaskSize;
		bytes_pending += (1 << FracShift);

		if (bytes_pending > BytesPendingLimit) {
			bytes_pending = BytesPendingLimit;
		}
	}
}

void Ps1Dac::WriteControlPort202(io_port_t, io_val_t value, io_width_t)
{
	channel->WakeUp();

	const auto data = check_cast<uint8_t>(value);
	regs.command    = data;
	if (data & 3) {
		can_trigger_irq = true;
	}
}

void Ps1Dac::WriteTimingPort203(io_port_t, io_val_t value, io_width_t)
{
	channel->WakeUp();

	auto data = check_cast<uint8_t>(value);
	// Clock divisor (maybe trigger first IRQ here).
	regs.divisor = data;

	if (data < 45) {    // common in Infocom games
		data = 125; // fallback to a default 8 KHz data rate
	}
	const auto data_rate_hz = static_cast<uint32_t>(ClockRateHz / data);

	adder = (data_rate_hz << FracShift) / sample_rate_hz;

	regs.status = CalcStatus();
	if ((regs.status & FifoNearlyEmptyFlag) && can_trigger_irq) {
		// Generate request for stuff.
		regs.status |= FifoIrqFlag;
		can_trigger_irq = false;
		PIC_ActivateIRQ(IrqNumber);
	}
}

void Ps1Dac::WriteFifoLevelPort204(io_port_t, io_val_t value, io_width_t)
{
	channel->WakeUp();

	const auto data = check_cast<uint8_t>(value);
	regs.fifo_level = data;
	if (!data) {
		Reset(true);
	}
	// When the Microphone is used (PS1MIC01), it writes 0x08 to this during
	// playback presumably beacuse the card is constantly filling the
	// analog-to-digital buffer.
}

uint8_t Ps1Dac::ReadPresencePort02F(io_port_t, io_width_t)
{
	return 0xff;
}

uint8_t Ps1Dac::ReadCmdResultPort200(io_port_t, io_width_t)
{
	regs.status &= check_cast<uint8_t>(~FifoStatusReadyFlag);
	return regs.command;
}

uint8_t Ps1Dac::ReadStatusPort202(io_port_t, io_width_t)
{
	regs.status = CalcStatus();
	return regs.status;
}

// Used by Stunt Island and Roger Rabbit 2 during setup.
uint8_t Ps1Dac::ReadTimingPort203(io_port_t, io_width_t)
{
	return regs.divisor;
}

// Used by Bush Buck as an alternate detection method.
uint8_t Ps1Dac::ReadJoystickPorts204To207(io_port_t, io_width_t)
{
	return 0;
}

void Ps1Dac::PicCallback(const int frames_requested)
{
	assert(frames_requested > 0);

	int32_t pending = 0;
	uint32_t add    = 0;
	uint32_t pos    = read_index_high;
	auto count      = frames_requested;

	if (is_playing) {
		regs.status = CalcStatus();
		pending     = static_cast<int32_t>(bytes_pending);
		add         = adder;
		if ((regs.status & FifoNearlyEmptyFlag) && can_trigger_irq) {
			// More bytes needed.
			regs.status |= FifoIrqFlag;
			can_trigger_irq = false;
			PIC_ActivateIRQ(IrqNumber);
		}
	}

	while (count > 0) {
		uint8_t out = 0;
		if (pending <= 0) {
			pending = 0;
			while (count--) {
				auto midline = FifoMidline;
				output_queue.NonblockingEnqueue(std::move(midline));
			}
			break;
		} else {
			out = fifo[pos >> FracShift];
			pos += add;
			pos &= (FifoSize << FracShift) - 1;
			pending -= static_cast<int32_t>(add);
		}
		output_queue.NonblockingEnqueue(std::move(out));
		count--;
	}
	// Update positions and see if we can clear the FifoFullFlag
	read_index_high = pos;
	read_index      = static_cast<uint16_t>(pos >> FracShift);
	if (pending < 0) {
		pending = 0;
	}
	bytes_pending = static_cast<uint32_t>(pending);
}

Ps1Dac::~Ps1Dac()
{
	MIXER_LockMixerThread();

	// Stop playback
	if (channel) {
		channel->Enable(false);
	}

	// Stop the game from accessing the IO ports
	for (auto& handler : read_handlers) {
		handler.Uninstall();
	}
	for (auto& handler : write_handlers) {
		handler.Uninstall();
	}

	// Deregister the mixer channel, after which it's cleaned up
	assert(channel);
	MIXER_DeregisterChannel(channel);

	TIMER_DelTickHandler(PS1AUDIO_PicCallback);

	MIXER_UnlockMixerThread();
}

class Ps1Synth {
public:
	Ps1Synth(const std::string& filter_choice);
	~Ps1Synth();

private:
	// Block alternate construction routes
	Ps1Synth()                           = delete;
	Ps1Synth(const Ps1Synth&)            = delete;
	Ps1Synth& operator=(const Ps1Synth&) = delete;

	void AudioCallback(const int requested_frames);
	float RenderSample();
	void RenderUpToNow();

	void WriteSoundGeneratorPort205(io_port_t port, io_val_t, io_width_t);

	// Managed objects
	MixerChannelPtr channel            = nullptr;
	IO_WriteHandleObject write_handler = {};
	std::queue<float> fifo             = {};
	std::mutex mutex                   = {};
	sn76496_device device;

	// Static rate-related configuration
	static constexpr auto Ps1PsgClockHz = 4'000'000;
	static constexpr auto RenderDivisor = 16;
	static constexpr auto RenderRateHz  = ceil_sdivide(Ps1PsgClockHz,
                                                          RenderDivisor);
	static constexpr auto MsPerRender   = MillisInSecond / RenderRateHz;

	// Runtime states
	device_sound_interface* dsi = static_cast<sn76496_base_device*>(&device);
	double last_rendered_ms = 0.0;
};

static std::unique_ptr<Ps1Synth> ps1_synth = {};

Ps1Synth::Ps1Synth(const std::string& filter_choice)
        : device(nullptr, nullptr, Ps1PsgClockHz)
{
	using namespace std::placeholders;

	MIXER_LockMixerThread();

	const auto callback = std::bind(&Ps1Synth::AudioCallback, this, _1);

	channel = MIXER_AddChannel(callback,
	                           RenderRateHz,
	                           ChannelName::Ps1AudioCardPsg,
	                           {ChannelFeature::Sleep,
	                            ChannelFeature::ReverbSend,
	                            ChannelFeature::ChorusSend,
	                            ChannelFeature::Synthesizer});

	// Setup PSG filters
	if (const auto maybe_bool = parse_bool_setting(filter_choice)) {
		// The filter parameters have been tweaked by analysing real
		// hardware recordings. The results are virtually
		// indistinguishable from the real thing by ear only.
		const auto filter_enabled = *maybe_bool;
		setup_filter(channel, filter_enabled);

	} else if (!channel->TryParseAndSetCustomFilter(filter_choice)) {
		LOG_WARNING(
		        "%s: Invalid 'ps1audio_filter' setting: '%s', "
		        "using 'on'",
		        ChannelName::Ps1AudioCardPsg,
		        filter_choice.c_str());

		const auto filter_enabled = true;
		setup_filter(channel, filter_enabled);
		set_section_property_value("speaker", "ps1audio_filter", "on");
	}

	const auto generate_sound =
	        std::bind(&Ps1Synth::WriteSoundGeneratorPort205, this, _1, _2, _3);

	write_handler.Install(0x205, generate_sound, io_width_t::byte);
	static_cast<device_t&>(device).device_start();
	device.convert_samplerate(RenderRateHz);

	MIXER_UnlockMixerThread();
}

float Ps1Synth::RenderSample()
{
	assert(dsi);

	static device_sound_interface::sound_stream ss;

	// Request a mono sample from the audio device
	int16_t sample;
	int16_t* buf[] = {&sample, nullptr};

	dsi->sound_stream_update(ss, nullptr, buf, 1);

	return static_cast<float>(sample);
}

void Ps1Synth::RenderUpToNow()
{
	const auto now = PIC_FullIndex();

	// Wake up the channel and update the last rendered time datum.
	if (channel->WakeUp()) {
		last_rendered_ms = now;
		return;
	}
	// Keep rendering until we're current
	while (last_rendered_ms < now) {
		last_rendered_ms += MsPerRender;
		fifo.emplace(RenderSample());
	}
}

void Ps1Synth::WriteSoundGeneratorPort205(io_port_t, io_val_t value, io_width_t)
{
	std::lock_guard lock(mutex);

	RenderUpToNow();

	const auto data = check_cast<uint8_t>(value);
	device.write(data);
}

void Ps1Synth::AudioCallback(const int requested_frames)
{
	assert(channel);

	std::lock_guard lock(mutex);

#if 0
	if (fifo.size()) {
		LOG_MSG("%s: Queued %2lu cycle-accurate frames",
		        ChannelName::Ps1AudioCardPsg,
		        fifo.size());
	}
#endif

	auto frames_remaining = requested_frames;

	// First, send any frames we've queued since the last callback
	while (frames_remaining && fifo.size()) {
		channel->AddSamples_mfloat(1, &fifo.front());
		fifo.pop();
		--frames_remaining;
	}
	// If the queue's run dry, render the remainder and sync-up our time datum
	while (frames_remaining) {
		float sample = RenderSample();
		channel->AddSamples_mfloat(1, &sample);
		--frames_remaining;
	}
	last_rendered_ms = PIC_AtomicIndex();
}

Ps1Synth::~Ps1Synth()
{
	MIXER_LockMixerThread();

	// Stop playback
	if (channel) {
		channel->Enable(false);
	}

	// Stop the game from accessing the IO ports
	write_handler.Uninstall();

	// Deregister the mixer channel, after which it's cleaned up
	assert(channel);
	MIXER_DeregisterChannel(channel);

	MIXER_UnlockMixerThread();
}

void PS1DAC_NotifyLockMixer()
{
	if (ps1_dac) {
		ps1_dac->output_queue.Stop();
	}
}

void PS1DAC_NotifyUnlockMixer()
{
	if (ps1_dac) {
		ps1_dac->output_queue.Start();
	}
}

bool PS1AUDIO_IsEnabled()
{
	const auto section = control->GetSection("speaker");
	assert(section);

	const auto properties = static_cast<SectionProp*>(section);
	return properties->GetBool("ps1audio");
}

static void init_ps1audio_settings(SectionProp& section)
{
	using enum Property::Changeable::Value;

	auto pbool = section.AddBool("ps1audio", WhenIdle, false);
	pbool->SetHelp("Enable IBM PS/1 Audio emulation ('off' by default).");

	auto pstring = section.AddString("ps1audio_filter", WhenIdle, "on");
	pstring->SetHelp(
	        "Filter for the PS/1 Audio synth output ('on' by default). Possible values:\n"
	        "\n"
	        "  on:        Filter the output (default).\n"
	        "  off:       Don't filter the output.\n"
	        "  <custom>:  Custom filter definition; see 'sb_filter' for details.");

	pstring = section.AddString("ps1audio_dac_filter", WhenIdle, "on");
	pstring->SetHelp(
	        "Filter for the PS/1 Audio DAC output ('on' by default). Possible values:\n"
	        "\n"
	        "  on:        Filter the output (default).\n"
	        "  off:       Don't filter the output.\n"
	        "  <custom>:  Custom filter definition; see 'sb_filter' for details.");
}

void PS1AUDIO_Init(SectionProp& section)
{
	if (!PS1AUDIO_IsEnabled()) {
		return;
	}

	ps1_dac = std::make_unique<Ps1Dac>(section.GetString("ps1audio_dac_filter"));
	ps1_synth = std::make_unique<Ps1Synth>(section.GetString("ps1audio_filter"));

	LOG_MSG("%s: Initialised IBM PS/1 Audio card", ChannelName::Ps1AudioCardPsg);
}

void PS1AUDIO_Destroy()
{
	if (ps1_dac || ps1_synth) {
		LOG_MSG("%s: Shutting down IBM PS/1 Audio card",
		        ChannelName::Ps1AudioCardPsg);

		ps1_dac.reset();
		ps1_synth.reset();
	}
}

void PS1AUDIO_NotifySettingUpdated(SectionProp& section,
                                   [[maybe_unused]] const std::string& prop_name)
{
	// The [speaker] section controls multiple audio devices, so we want to
	// make sure to only restart the device affected by the setting.
	//
	if (prop_name == "ps1audio" || prop_name == "ps1audio_filter" ||
	    prop_name == "ps1audio_dac_filter") {

		PS1AUDIO_Destroy();
		PS1AUDIO_Init(section);
	}

	// TODO support changing filter params without restarting the device
}

void PS1AUDIO_AddConfigSection(Section* sec)
{
	assert(sec);

	const auto section = static_cast<SectionProp*>(sec);

	init_ps1audio_settings(*section);
}
