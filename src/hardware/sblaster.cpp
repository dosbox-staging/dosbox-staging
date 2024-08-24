/*
 *  Copyright (C) 2019-2024  The DOSBox Staging Team
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

#include <array>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <map>
#include <optional>
#include <string>
#include <tuple>

#include "audio_vector.h"
#include "autoexec.h"
#include "bios.h"
#include "bit_view.h"
#include "bitops.h"
#include "channel_names.h"
#include "control.h"
#include "dma.h"
#include "hardware.h"
#include "inout.h"
#include "math_utils.h"
#include "midi.h"
#include "mixer.h"
#include "pic.h"
#include "rwqueue.h"
#include "setup.h"
#include "shell.h"
#include "string_utils.h"
#include "support.h"
#include "timer.h"

constexpr uint8_t MixerIndex = 0x04;
constexpr uint8_t MixerData  = 0x05;

constexpr uint8_t DspReset       = 0x06;
constexpr uint8_t DspReadData    = 0x0A;
constexpr uint8_t DspWriteData   = 0x0C;
constexpr uint8_t DspWriteStatus = 0x0C;
constexpr uint8_t DspReadStatus  = 0x0E;
constexpr uint8_t DspAck16Bit    = 0x0f;

constexpr uint8_t DspNoCommand = 0;

constexpr uint16_t DmaBufSize = 1024;
constexpr uint8_t DspBufSize  = 64;
constexpr uint16_t DspDacSize = 512;

constexpr uint8_t SbShift      = 14;
constexpr uint16_t SbShiftMask = ((1 << SbShift) - 1);

constexpr uint8_t MinAdaptiveStepSize = 0; // max is 32767

// It was common for games perform some initial checks
// and resets on startup, resulting a rapid susccession of resets.
constexpr uint8_t DspInitialResetLimit = 4;

// The official guide states the following:
// "Valid output rates range from 5000 to 45 000 Hz, inclusive."
//
// However, this statement is wrong as in actual reality the maximum
// achievable sample rate is the native SB DAC rate of 45454 Hz, and
// many programs use this highest rate. Limiting the max rate to 45000
// Hz would result in a slightly out-of-tune, detuned pitch in such
// programs.
//
// More details:
// https://www.vogons.org/viewtopic.php?p=621717#p621717
//
// Ref:
//   Sound Blaster Series Hardware Programming Guide,
//   41h Set digitized sound output sampling rate, DSP Commands 6-15
//   https://pdos.csail.mit.edu/6.828/2018/readings/hardware/SoundBlaster.pdf
//
constexpr auto MinPlaybackRateHz         = 5000;
constexpr auto NativeDacRateHz           = 45454;
constexpr uint16_t DefaultPlaybackRateHz = 22050;

enum class DspState {
	Reset, ResetWait, Normal, HighSpeed
};

enum class SbType {
	None        = 0,
	SB1         = 1,
	SBPro1      = 2,
	SB2         = 3,
	SBPro2      = 4,
	SB16        = 6,
	GameBlaster = 7
};

enum class FilterType { None, SB1, SB2, SBPro1, SBPro2, SB16, Modern };

enum class SbIrq { Irq8, Irq16, IrqMpu };

enum class DspMode { None, Dac, Dma, DmaPause, DmaMasked };

enum class DmaMode {
	None,
	Adpcm2Bit,
	Adpcm3Bit,
	Adpcm4Bit,
	Pcm8Bit,
	Pcm16Bit,
	Pcm16BitAliased
};

enum class EssType { None, Es1688 };

struct SbInfo {
	uint32_t freq_hz = 0;

	struct {
		bool stereo   = false;
		bool sign     = false;
		bool autoinit = false;

		DmaMode mode = {};

		uint32_t rate       = 0; // sample rate
		uint32_t mul        = 0; // samples-per-millisecond multipler
		uint32_t singlesize = 0; // size for single cycle transfers
		uint32_t autosize   = 0; // size for auto init transfers
		uint32_t left       = 0; // Left in active cycle
		uint32_t min        = 0;

		union {
			uint8_t b8[DmaBufSize];
			int16_t b16[DmaBufSize];
		} buf = {};

		uint32_t bits        = 0;
		DmaChannel* chan     = nullptr;
		uint32_t remain_size = 0;
	} dma = {};

	bool speaker_enabled = false;
	bool midi_enabled    = false;

	uint8_t time_constant = 0;

	DspMode mode = DspMode::None;
	SbType type  = SbType::None;

	// ESS chipset emulation, to be set only for SbType::SBPro2
	EssType ess_type = EssType::None;

	struct {
		bool pending_8bit;
		bool pending_16bit;
	} irq = {};

	struct {
		DspState state             = {};
		uint8_t cmd                = 0;
		uint8_t cmd_len            = 0;
		uint8_t cmd_in_pos         = 0;
		uint8_t cmd_in[DspBufSize] = {};

		struct {
			// Last values added to the fifo
			uint8_t lastval          = 0;
			uint8_t data[DspBufSize] = {};

			// Index of current entry
			uint8_t pos = 0;

			// Number of entries in the fifo
			uint8_t used = 0;
		} in = {}, out = {};

		uint8_t test_register        = 0;
		uint8_t write_status_counter = 0;

		uint32_t reset_tally = 0;

		int cold_warmup_ms      = 0;
		int hot_warmup_ms       = 0;
		int warmup_remaining_ms = 0;
	} dsp = {};

	struct {
		int16_t data[DspDacSize + 1] = {};

		// Number of entries in the DAC
		uint16_t used = 0;

		// Index of current entry
		int16_t last = 0;
	} dac = {};

	struct {
		uint8_t index = 0;

		// If true, DOS software can programmatically change the Sound
		// Blaster mixer's volume levels on SB Pro 1 and later card for
		// the SB (DAC), OPL (FM) and CDAUDIO channels. These are called
		// "app levels". The final output level is the "user level" set
		// in the DOSBox mixer multiplied with the "app level" for these
		// channels.
		//
		// If the Sound Blaster mixer is disabled, the "app levels"
		// default to unity gain.
		//
		bool enabled = false;

		uint8_t dac[2] = {}; // can be controlled programmatically
		uint8_t fm[2]  = {}; // can be controlled programmatically
		uint8_t cda[2] = {}; // can be controlled programmatically

		uint8_t master[2] = {};
		uint8_t lin[2]    = {};
		uint8_t mic       = 0;

		bool stereo_enabled = false;

		bool filter_enabled    = true;
		bool filter_configured = false;
		bool filter_always_on  = false;

		uint8_t unhandled[0x48] = {};

		uint8_t ess_id_str[4]  = {};
		uint8_t ess_id_str_pos = {};
	} mixer = {};

	struct {
		uint8_t reference = 0;
		uint16_t stepsize = 0;
		bool haveref      = false;
	} adpcm = {};

	struct {
		uint16_t base = 0;
		uint8_t irq   = 0;
		uint8_t dma8  = 0;
		uint8_t dma16 = 0;
	} hw = {};

	struct {
		int value      = 0;
		uint32_t count = 0;
	} e2 = {};

	MixerChannelPtr chan = nullptr;
};

static SbInfo sb = {};

// clang-format off

// Number of bytes in input for commands (sb/sbpro)
static uint8_t dsp_cmd_len_sb[256] = {
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x00
//  1,0,0,0, 2,0,2,2, 0,0,0,0, 0,0,0,0,  // 0x10
  1,0,0,0, 2,2,2,2, 0,0,0,0, 0,0,0,0,  // 0x10 Wari hack
  0,0,0,0, 2,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x20
  0,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0,  // 0x30

  1,2,2,0, 0,0,0,0, 2,0,0,0, 0,0,0,0,  // 0x40
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x50
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x60
  0,0,0,0, 2,2,2,2, 0,0,0,0, 0,0,0,0,  // 0x70

  2,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x80
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x90
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0xa0
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0xb0

  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0xc0
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0xd0
  1,0,1,0, 1,0,0,0, 0,0,0,0, 0,0,0,0,  // 0xe0
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0   // 0xf0
};

// Number of bytes in input for commands (sb16)
static uint8_t dsp_cmd_len_sb16[256] = {
  0,0,0,0, 1,2,0,0, 1,0,0,0, 0,0,2,1,  // 0x00
//  1,0,0,0, 2,0,2,2, 0,0,0,0, 0,0,0,0,  // 0x10
  1,0,0,0, 2,2,2,2, 0,0,0,0, 0,0,0,0,  // 0x10 Wari hack
  0,0,0,0, 2,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x20
  0,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0,  // 0x30

  1,2,2,0, 0,0,0,0, 2,0,0,0, 0,0,0,0,  // 0x40
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x50
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x60
  0,0,0,0, 2,2,2,2, 0,0,0,0, 0,0,0,0,  // 0x70

  2,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x80
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x90
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0xa0
  3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3,  // 0xb0

  3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3,  // 0xc0
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0xd0
  1,0,1,0, 1,0,0,0, 0,0,0,0, 0,0,0,0,  // 0xe0
  0,0,0,0, 0,0,0,0, 0,1,0,0, 0,0,0,0   // 0xf0
};
// clang-format on

static uint8_t asp_regs[256];
static bool asp_init_in_progress = false;

static int e2_incr_table[4][9] = {
        { 0x01, -0x02, -0x04,  0x08, -0x10,  0x20,  0x40, -0x80, -106},
        {-0x01,  0x02, -0x04,  0x08,  0x10, -0x20,  0x40, -0x80,  165},
        {-0x01,  0x02,  0x04, -0x08,  0x10, -0x20, -0x40,  0x80, -151},
        { 0x01, -0x02,  0x04, -0x08, -0x10,  0x20, -0x40,  0x80,   90}
};

static int frames_added_this_tick = 0;
RWQueue<std::unique_ptr<AudioVector>> soundblaster_mixer_queue{128};

static const char* sb_log_prefix()
{
	switch (sb.type) {
	case SbType::SB1: return "SB1";
	case SbType::SB2: return "SB2";
	case SbType::SBPro1: return "SBPRO1";
	case SbType::SBPro2: return "SBPRO2";
	case SbType::SB16: return "SB16";
	case SbType::GameBlaster: return "GB";
	case SbType::None:
		assertm(false, "Should not use SbType::None as a log prefix");
		return "SB";
	default:
		assertm(false,
		        format_str("Invalid SbType value: %d",
		                   static_cast<int>(sb.type)));
		return "";
	}
}

static void dsp_change_mode(const DspMode mode);

static void flush_remainig_dma_transfer();
static void suppress_dma_transfer(const uint32_t size);
static void play_dma_transfer(const uint32_t size);

typedef void (*process_dma_f)(uint32_t);
static process_dma_f ProcessDMATransfer;

static void dsp_enable_speaker(const bool enabled)
{
	// Speaker output is always enabled on the SB16 and ESS cards; speaker
	// enable/disable commands are simply ignored. Only the SB Pro and
	// earlier models can toggle the speaker-output via speaker
	// enable/disable commands.
	if (sb.type == SbType::SB16 || sb.ess_type != EssType::None) {
		return;
	}

	// Speaker-output is already in the requested state
	if (sb.speaker_enabled == enabled) {
		return;
	}

	// If the speaker's being turned on, then flush old
	// content before releasing the channel for playback.
	if (enabled) {
		PIC_RemoveEvents(suppress_dma_transfer);
		flush_remainig_dma_transfer();

		// Speaker powered-on after cold-state, give it warmup time
		sb.dsp.warmup_remaining_ms = sb.dsp.cold_warmup_ms;
	}

	sb.speaker_enabled = enabled;

#if 0
	// This can be very noisy as some games toggle the speaker for every effect
	LOG_MSG("%s: Speaker-output has been toggled %s",
	        sb_log_prefix(),
	        (enabled ? "on" : "off"));
#endif
}

static void init_speaker_state()
{
	if (sb.type == SbType::SB16 || sb.ess_type != EssType::None) {

		// Speaker output (DAC output) is always enabled on the SB16 and ESS
		// cards. Because the channel is active, we treat this as a startup
		// event.
		const bool is_cold_start = sb.dsp.reset_tally <=
		                           DspInitialResetLimit;

		sb.dsp.warmup_remaining_ms = is_cold_start ? sb.dsp.cold_warmup_ms
		                                           : sb.dsp.hot_warmup_ms;
		sb.speaker_enabled = true;

	} else {
		// SB Pro and earlier models have the speaker-output disabled by
		// default.
		sb.speaker_enabled = false;
	}
}

static void log_filter_config(const char* channel_name, const char* output_type,
                              const FilterType filter)
{
	// clang-format off
	static const std::map<FilterType, std::string> filter_name_map = {
	        {FilterType::SB1,    "Sound Blaster 1.0"},
	        {FilterType::SB2,    "Sound Blaster 2.0"},
	        {FilterType::SBPro1, "Sound Blaster Pro 1"},
	        {FilterType::SBPro2, "Sound Blaster Pro 2"},
	        {FilterType::SB16,   "Sound Blaster 16"},
	        {FilterType::Modern, "Modern"},
	};
	// clang-format on

	if (filter == FilterType::None) {
		LOG_MSG("%s: %s filter disabled", channel_name, output_type);
	} else {
		auto it = filter_name_map.find(filter);
		if (it != filter_name_map.end()) {
			auto filter_type = it->second;

			LOG_MSG("%s: %s %s output filter enabled",
			        channel_name,
			        filter_type.c_str(),
			        output_type);
		}
	}
}

struct FilterConfig {
	FilterState hpf_state  = FilterState::Off;
	int hpf_order          = {};
	int hpf_cutoff_freq_hz = {};

	FilterState lpf_state  = FilterState::Off;
	int lpf_order          = {};
	int lpf_cutoff_freq_hz = {};

	ResampleMethod resample_method = {};
	int zoh_rate_hz                = {};
};

static void set_filter(MixerChannelPtr channel, const FilterConfig& config)
{
	if (config.hpf_state == FilterState::On) {
		channel->ConfigureHighPassFilter(config.hpf_order,
		                                 config.hpf_cutoff_freq_hz);
	}
	channel->SetHighPassFilter(config.hpf_state);

	if (config.lpf_state == FilterState::On) {
		channel->ConfigureLowPassFilter(config.lpf_order,
		                                config.lpf_cutoff_freq_hz);
	}
	channel->SetLowPassFilter(config.lpf_state);

	if (config.resample_method == ResampleMethod::ZeroOrderHoldAndResample) {
		channel->SetZeroOrderHoldUpsamplerTargetRate(config.zoh_rate_hz);
	}

	channel->SetResampleMethod(config.resample_method);
}

static std::optional<FilterType> determine_filter_type(const std::string& filter_choice,
                                                       const SbType sb_type)
{
	if (filter_choice == "auto") {
		// clang-format off
		switch (sb_type) {
		case SbType::None:        return FilterType::None;   break;
		case SbType::SB1:         return FilterType::SB1;    break;
		case SbType::SB2:         return FilterType::SB2;    break;
		case SbType::SBPro1:      return FilterType::SBPro1; break;
		case SbType::SBPro2:      return FilterType::SBPro2; break;
		case SbType::SB16:        return FilterType::SB16;   break;
		case SbType::GameBlaster: return FilterType::None;   break;
		}
		// clang-format on
	} else if (filter_choice == "off") {
		return FilterType::None;

	} else if (filter_choice == "sb1") {
		return FilterType::SB1;

	} else if (filter_choice == "sb2") {
		return FilterType::SB2;

	} else if (filter_choice == "sbpro1") {
		return FilterType::SBPro1;

	} else if (filter_choice == "sbpro2") {
		return FilterType::SBPro2;

	} else if (filter_choice == "sb16") {
		return FilterType::SB16;

	} else if (filter_choice == "modern") {
		return FilterType::Modern;
	}

	return {};
}

static void configure_sb_filter_for_model(MixerChannelPtr channel,
                                          const std::string& filter_prefs,
                                          const SbType sb_type)
{
	const auto filter_prefs_parts = split(filter_prefs);

	const auto filter_choice = filter_prefs_parts.empty()
	                                 ? "auto"
	                                 : filter_prefs_parts[0];

	FilterConfig config = {};

	auto enable_lpf = [&](const int order, const int cutoff_freq_hz) {
		config.lpf_state          = FilterState::On;
		config.lpf_order          = order;
		config.lpf_cutoff_freq_hz = cutoff_freq_hz;
	};

	auto enable_zoh_upsampler = [&] {
		config.resample_method = ResampleMethod::ZeroOrderHoldAndResample;
		config.zoh_rate_hz = NativeDacRateHz;
	};

	const auto filter_type = [&]() {
		if (const auto maybe_filter_type = determine_filter_type(filter_choice,
		                                                         sb_type)) {
			return *maybe_filter_type;
		} else {
			LOG_WARNING("%s: Invalid 'sb_filter' setting: '%s', using 'modern'",
			            sb_log_prefix(),
			            filter_choice.c_str());

			set_section_property_value("sblaster", "sb_filter", "modern");
			return FilterType::Modern;
		}
	}();

	switch (filter_type) {
	case FilterType::None: enable_zoh_upsampler(); break;

	case FilterType::SB1:
		enable_lpf(2, 3800);
		enable_zoh_upsampler();
		break;

	case FilterType::SB2:
		enable_lpf(2, 4800);
		enable_zoh_upsampler();
		break;

	case FilterType::SBPro1:
	case FilterType::SBPro2:
		enable_lpf(2, 3200);
		enable_zoh_upsampler();
		break;

	case FilterType::SB16:
		// With the zero-order-hold upsampler disabled, we're
		// just relying on Speex to resample to the host rate.
		// This applies brickwall filtering at half the SB
		// channel rate, which perfectly emulates the dynamic
		// brickwall filter of the SB16.
		config.resample_method = ResampleMethod::Resample;
		break;

	case FilterType::Modern:
		// Linear interpolation upsampling is the legacy DOSBox behaviour
		config.resample_method = ResampleMethod::LerpUpsampleOrResample;
		break;
	}

	constexpr auto OutputType = "DAC";
	log_filter_config(sb_log_prefix(), OutputType, filter_type);
	set_filter(channel, config);
}

static void configure_sb_filter(MixerChannelPtr channel,
                                const std::string& filter_prefs,
                                const bool filter_always_on, const SbType sb_type)
{
	sb.mixer.filter_always_on = filter_always_on;

	// When a custom filter is set, we're doing zero-order-hold upsampling
	// to the native Sound Blaster DAC rate, apply the custom filter, then
	// resample to the host rate.
	//
	// We need to enable the ZOH upsampler and the correct upsample rate
	// first for the filter cutoff frequency validation to work correctly.
	//
	channel->SetZeroOrderHoldUpsamplerTargetRate(NativeDacRateHz);
	channel->SetResampleMethod(ResampleMethod::ZeroOrderHoldAndResample);

	if (!channel->TryParseAndSetCustomFilter(filter_prefs)) {
		// Not a custom filter setting; try to parse it as a
		// model-specific setting.
		configure_sb_filter_for_model(channel, filter_prefs, sb_type);
	}

	sb.mixer.filter_configured = (channel->GetLowPassFilterState() ==
	                              FilterState::On);
}

static void configure_opl_filter_for_model(MixerChannelPtr opl_channel,
                                           const std::string& filter_prefs,
                                           const SbType sb_type)
{
	const auto filter_prefs_parts = split(filter_prefs);

	const auto filter_choice = filter_prefs_parts.empty()
	                                 ? "auto"
	                                 : filter_prefs_parts[0];

	FilterConfig config    = {};
	config.resample_method = ResampleMethod::Resample;

	auto enable_lpf = [&](const int order, const int cutoff_freq_hz) {
		config.lpf_state          = FilterState::On;
		config.lpf_order          = order;
		config.lpf_cutoff_freq_hz = cutoff_freq_hz;
	};

	const auto filter_type = [&]() {
		if (const auto maybe_filter_type = determine_filter_type(filter_choice,
		                                                         sb_type)) {
			return *maybe_filter_type;
		} else {
			LOG_WARNING("%s: Invalid 'opl_filter' setting: '%s', using 'auto'",
			            sb_log_prefix(),
			            filter_choice.c_str());

			set_section_property_value("sblaster", "opl_filter", "auto");

			if (const auto filter_type = determine_filter_type("auto", sb_type);
			    filter_type) {
				return *filter_type;
			} else {
				assert(false);
				return FilterType::None;
			}
		}
	}();

	// The filter parameters have been tweaked by analysing real hardware
	// recordings. The results are virtually indistinguishable from the real
	// thing by ear only.
	switch (filter_type)
	{
	case FilterType::None:
	case FilterType::SB16:
	case FilterType::Modern: break;

	case FilterType::SB1:
	case FilterType::SB2: enable_lpf(1, 12000); break;

	case FilterType::SBPro1:
	case FilterType::SBPro2: enable_lpf(1, 8000); break;
	}

	constexpr auto OutputType = "OPL";
	log_filter_config(ChannelName::Opl, OutputType, filter_type);
	set_filter(opl_channel, config);
}

static void configure_opl_filter(MixerChannelPtr opl_channel,
                                 const std::string& filter_prefs, const SbType sb_type)
{
	assert(opl_channel);

	if (!opl_channel->TryParseAndSetCustomFilter(filter_prefs)) {
		// Not a custom filter setting; try to parse it as a
		// model-specific setting.
		configure_opl_filter_for_model(opl_channel, filter_prefs, sb_type);
	}
}

static void sb_raise_irq(const SbIrq irq_type)
{
	LOG(LOG_SB, LOG_NORMAL)("Raising IRQ");

	switch (irq_type) {
		case SbIrq::Irq8:
		if (sb.irq.pending_8bit) {
			// LOG_MSG("SB: 8bit irq pending");
			return;
		}
		sb.irq.pending_8bit = true;
		PIC_ActivateIRQ(sb.hw.irq);
		break;

		case SbIrq::Irq16:
		if (sb.irq.pending_16bit) {
			// LOG_MSG("SB: 16bit irq pending");
			return;
		}
		sb.irq.pending_16bit = true;
		PIC_ActivateIRQ(sb.hw.irq);
		break;

	default: break;
	}
}

static void dsp_flush_data()
{
	sb.dsp.out.used = 0;
	sb.dsp.out.pos  = 0;
}

static double last_dma_callback = 0.0;

static void dsp_dma_callback(const DmaChannel* chan, const DMAEvent event)
{
	if (chan != sb.dma.chan || event == DMA_REACHED_TC) {
		return;

	} else if (event == DMA_MASKED) {
		if (sb.mode == DspMode::Dma) {
			// Catch up to current time, but don't generate an IRQ!
			// Fixes problems with later sci games.
			const auto t = PIC_FullIndex() - last_dma_callback;
			auto s = static_cast<uint32_t>(sb.dma.rate * t / 1000.0);

			if (s > sb.dma.min) {
				LOG(LOG_SB, LOG_NORMAL)
				("limiting amount masked to sb.dma.min");
				s = sb.dma.min;
			}

			auto min_size = sb.dma.mul >> SbShift;
			if (!min_size) {
				min_size = 1;
			}
			min_size *= 2;

			if (sb.dma.left > min_size) {
				if (s > (sb.dma.left - min_size)) {
					s = sb.dma.left - min_size;
				}
				// This will trigger an irq, see
				// play_dma_transfer, so lets not do that
				if (!sb.dma.autoinit && sb.dma.left <= sb.dma.min) {
					s = 0;
				}
				if (s) {
					ProcessDMATransfer(s);
				}
			}

			sb.mode = DspMode::DmaMasked;

			// dsp_change_mode(DspMode::DmaMasked);
			LOG(LOG_SB, LOG_NORMAL)
			("DMA masked,stopping output, left %d", chan->curr_count);
		}

	} else if (event == DMA_UNMASKED) {
		if (sb.mode == DspMode::DmaMasked && sb.dma.mode != DmaMode::None) {
			dsp_change_mode(DspMode::Dma);
			// sb.mode=DspMode::Dma;
			flush_remainig_dma_transfer();

			LOG(LOG_SB, LOG_NORMAL)
			("DMA unmasked,starting output, auto %d block %d",
			 static_cast<int>(chan->is_autoiniting),
			 chan->base_count);
		}
	} else {
		E_Exit("Unknown sblaster dma event");
	}
}

static uint8_t decode_adpcm_portion(const int bit_portion,
                                    const uint8_t adjust_map[],
                                    const int8_t scale_map[], const int last_index)
{
	auto& scale  = sb.adpcm.stepsize;
	auto& sample = sb.adpcm.reference;

	const auto i = std::clamp(bit_portion + scale, 0, last_index);

	scale = (scale + adjust_map[i]) & 0xff;

	sample = static_cast<uint8_t>(clamp(sample + scale_map[i], 0, 255));
	return sample;
}

static std::array<uint8_t, 4> decode_adpcm_2bit(const uint8_t data)
{
	// clang-format off
	constexpr int8_t ScaleMap[] = {
		0,  1,  0,  -1, 1,  3,  -1,  -3,
		2,  6, -2,  -6, 4, 12,  -4, -12,
		8, 24, -8, -24, 6, 48, -16, -48
	};

	constexpr uint8_t AdjustMap[] = {
		  0, 4,   0, 4,
		252, 4, 252, 4, 252, 4, 252, 4,
		252, 4, 252, 4, 252, 4, 252, 4,
		252, 0, 252, 0
	};
	// clang-format on

	static_assert(ARRAY_LEN(ScaleMap) == ARRAY_LEN(AdjustMap));
	constexpr auto LastIndex = static_cast<uint8_t>(sizeof(ScaleMap) - 1);

	return {decode_adpcm_portion((data >> 6) & 0x3, AdjustMap, ScaleMap, LastIndex),
	        decode_adpcm_portion((data >> 4) & 0x3, AdjustMap, ScaleMap, LastIndex),
	        decode_adpcm_portion((data >> 2) & 0x3, AdjustMap, ScaleMap, LastIndex),
	        decode_adpcm_portion((data >> 0) & 0x3, AdjustMap, ScaleMap, LastIndex)};
}

static std::array<uint8_t, 3> decode_adpcm_3bit(const uint8_t data)
{
	// clang-format off
	constexpr int8_t ScaleMap[40] = {
		0,  1,  2,  3,  0,  -1,  -2,  -3,
		1,  3,  5,  7, -1,  -3,  -5,  -7,
		2,  6, 10, 14, -2,  -6, -10, -14,
		4, 12, 20, 28, -4, -12, -20, -28,
		5, 15, 25, 35, -5, -15, -25, -35
	};

	constexpr uint8_t AdjustMap[40] = {
		  0, 0, 0, 8,   0, 0, 0, 8,
		248, 0, 0, 8, 248, 0, 0, 8,
		248, 0, 0, 8, 248, 0, 0, 8,
		248, 0, 0, 8, 248, 0, 0, 8,
		248, 0, 0, 0, 248, 0, 0, 0
	};
	// clang-format on

	static_assert(ARRAY_LEN(ScaleMap) == ARRAY_LEN(AdjustMap));
	constexpr auto LastIndex = static_cast<uint8_t>(sizeof(ScaleMap) - 1);

	return {decode_adpcm_portion((data >> 5) & 0x7, AdjustMap, ScaleMap, LastIndex),
	        decode_adpcm_portion((data >> 2) & 0x7, AdjustMap, ScaleMap, LastIndex),
	        decode_adpcm_portion((data & 0x3) << 1, AdjustMap, ScaleMap, LastIndex)};
}

static std::array<uint8_t, 2> decode_adpcm_4bit(const uint8_t data)
{
	// clang-format off
	constexpr int8_t ScaleMap[64] = {
		0,  1,  2,  3,  4,  5,  6,  7,  0,  -1,  -2,  -3,  -4,  -5,  -6,  -7,
		1,  3,  5,  7,  9, 11, 13, 15, -1,  -3,  -5,  -7,  -9, -11, -13, -15,
		2,  6, 10, 14, 18, 22, 26, 30, -2,  -6, -10, -14, -18, -22, -26, -30,
		4, 12, 20, 28, 36, 44, 52, 60, -4, -12, -20, -28, -36, -44, -52, -60
	};

	constexpr uint8_t AdjustMap[64] = {
		  0, 0, 0, 0, 0, 16, 16, 16,
		  0, 0, 0, 0, 0, 16, 16, 16,
		240, 0, 0, 0, 0, 16, 16, 16,
		240, 0, 0, 0, 0, 16, 16, 16,
		240, 0, 0, 0, 0, 16, 16, 16,
		240, 0, 0, 0, 0, 16, 16, 16,
		240, 0, 0, 0, 0,  0,  0,  0,
		240, 0, 0, 0, 0,  0,  0,  0
	};
	// clang-format on

	static_assert(ARRAY_LEN(ScaleMap) == ARRAY_LEN(AdjustMap));
	constexpr auto LastIndex = static_cast<uint8_t>(sizeof(ScaleMap) - 1);

	return {decode_adpcm_portion(data >> 4, AdjustMap, ScaleMap, LastIndex),
	        decode_adpcm_portion(data & 0xf, AdjustMap, ScaleMap, LastIndex)};
}

template <typename T>
static const T* maybe_silence(const uint32_t num_samples, const T* buffer)
{
	if (sb.dsp.warmup_remaining_ms <= 0) {
		return buffer;
	}

	static std::vector<T> quiet_buffer = {};
	constexpr auto Silent = Mixer_GetSilentDOSSample<T>();

	if (quiet_buffer.size() < num_samples) {
		quiet_buffer.resize(num_samples, Silent);
	}

	--sb.dsp.warmup_remaining_ms;
	return quiet_buffer.data();
}

static uint32_t read_dma_8bit(const uint32_t bytes_to_read, const uint32_t i = 0)
{
	const auto bytes_read = sb.dma.chan->Read(bytes_to_read, sb.dma.buf.b8 + i);
	assert(bytes_read <= DmaBufSize * sizeof(sb.dma.buf.b8[0]));

	return check_cast<uint32_t>(bytes_read);
}

static uint32_t read_dma_16bit(const uint32_t bytes_to_read, const uint32_t i = 0)
{
	const auto unsigned_buf = reinterpret_cast<uint8_t*>(sb.dma.buf.b16 + i);
	const auto bytes_read = sb.dma.chan->Read(bytes_to_read, unsigned_buf);
	assert(bytes_read <= DmaBufSize * sizeof(sb.dma.buf.b16[0]));

	return check_cast<uint32_t>(bytes_read);
}

static void enqueue_frames(std::unique_ptr<AudioVector> frames)
{
	frames_added_this_tick += frames->num_frames;
	soundblaster_mixer_queue.NonblockingEnqueue(std::move(frames));
}

static void play_dma_transfer(const uint32_t bytes_requested)
{
	// How many bytes should we read from DMA?
	const auto lower_bound = sb.dma.autoinit ? bytes_requested : sb.dma.min;
	const auto bytes_to_read = sb.dma.left <= lower_bound ? sb.dma.left
	                                                      : bytes_requested;

	// All three of these must be populated during the DMA sequence to
	// ensure the proper quantities and unit are being accounted for.
	// For example: use the channel count to convert from samples to frames.
	uint32_t bytes_read = 0;
	uint32_t samples    = 0;
	uint16_t frames     = 0;

	// In DmaMode::Pcm16BitAliased mode temporarily divide by 2 to get
	// number of 16-bit samples, because 8-bit DMA Read returns byte size,
	// while in DmaMode::Pcm16Bit mode 16-bit DMA Read returns word size.
	const uint8_t dma16_to_sample_divisor = sb.dma.mode == DmaMode::Pcm16BitAliased
	                                              ? 2
	                                              : 1;

	// Used to convert from samples to frames (which is what AddSamples
	// unintuitively uses.. )
	const uint8_t channels = sb.dma.stereo ? 2 : 1;

	last_dma_callback = PIC_FullIndex();

	// Temporary counter for ADPCM modes

	auto decode_adpcm_dma =
	        [&](auto decode_adpcm_fn) -> std::tuple<uint32_t, uint32_t, uint16_t> {
		const uint32_t num_bytes = read_dma_8bit(bytes_to_read);
		uint32_t num_samples     = 0;
		uint16_t num_frames      = 0;

		// Parse the reference ADPCM byte, if provided
		uint32_t i = 0;
		if (num_bytes > 0 && sb.adpcm.haveref) {
			sb.adpcm.haveref   = false;
			sb.adpcm.reference = sb.dma.buf.b8[0];
			sb.adpcm.stepsize  = MinAdaptiveStepSize;
			++i;
		}
		// Decode the remaining DMA buffer into samples using the
		// provided function
		while (i < num_bytes) {
			const auto decoded = decode_adpcm_fn(sb.dma.buf.b8[i]);
			constexpr auto NumDecoded = check_cast<uint8_t>(
			        decoded.size());

			enqueue_frames(std::make_unique<AudioVectorM8>(
				NumDecoded, maybe_silence(NumDecoded, decoded.data())));
			num_samples += NumDecoded;
			i++;
		}
		// ADPCM is mono
		num_frames = check_cast<uint16_t>(num_samples);
		return {num_bytes, num_samples, num_frames};
	};

	// Read the actual data, process it and send it off to the mixer
	switch (sb.dma.mode) {
	case DmaMode::Adpcm2Bit:
		std::tie(bytes_read, samples, frames) = decode_adpcm_dma(
		        decode_adpcm_2bit);
		break;

	case DmaMode::Adpcm3Bit:
		std::tie(bytes_read, samples, frames) = decode_adpcm_dma(
		        decode_adpcm_3bit);
		break;

	case DmaMode::Adpcm4Bit:
		std::tie(bytes_read, samples, frames) = decode_adpcm_dma(
		        decode_adpcm_4bit);
		break;

	case DmaMode::Pcm8Bit:
		if (sb.dma.stereo) {
			bytes_read = read_dma_8bit(bytes_to_read, sb.dma.remain_size);
			samples = bytes_read + sb.dma.remain_size;
			frames  = check_cast<uint16_t>(samples / channels);

			// Only add whole frames when in stereo DMA mode. The
			// number of frames comes from the DMA request, and
			// therefore user-space data.
			if (frames) {
				if (sb.dma.sign) {
					const auto signed_buf = reinterpret_cast<int8_t*>(sb.dma.buf.b8);
					enqueue_frames(std::make_unique<AudioVectorS8S>(
						frames, maybe_silence(samples, signed_buf)));
				} else {
					enqueue_frames(std::make_unique<AudioVectorS8>(
						frames, maybe_silence(samples, sb.dma.buf.b8)));
				}
			}
			// Otherwise there's an unhandled dangling sample from
			// the last round
			if (samples & 1) {
				sb.dma.remain_size = 1;
				sb.dma.buf.b8[0]   = sb.dma.buf.b8[samples - 1];
			} else {
				sb.dma.remain_size = 0;
			}

		} else { // Mono
			bytes_read = read_dma_8bit(bytes_to_read);
			samples    = bytes_read;
			frames     = check_cast<uint16_t>(samples / channels);
			assert(channels == 1 && frames == samples); // sanity-check
			                                            // mono
			if (sb.dma.sign) {
				const auto signed_buf = reinterpret_cast<int8_t*>(sb.dma.buf.b8);
				enqueue_frames(std::make_unique<AudioVectorM8S>(
					frames, maybe_silence(samples, signed_buf)));
			} else {
				enqueue_frames(std::make_unique<AudioVectorM8>(
					frames, maybe_silence(samples, sb.dma.buf.b8)));
			}
		}
		break;

	case DmaMode::Pcm16BitAliased:
		assert(dma16_to_sample_divisor == 2);
		[[fallthrough]];

	case DmaMode::Pcm16Bit:
		if (sb.dma.stereo) {
			bytes_read = read_dma_16bit(bytes_to_read, sb.dma.remain_size);
			samples = (bytes_read + sb.dma.remain_size) /
			          dma16_to_sample_divisor;
			frames = check_cast<uint16_t>(samples / channels);

			// Only add whole frames when in stereo DMA mode
			if (frames) {
				if (sb.dma.sign) {
					enqueue_frames(std::make_unique<AudioVectorS16>(
						frames, maybe_silence(samples, sb.dma.buf.b16)));
				} else {
					const auto unsigned_buf = reinterpret_cast<uint16_t*>(sb.dma.buf.b16);
					enqueue_frames(std::make_unique<AudioVectorS16U>(
						frames, maybe_silence(samples, unsigned_buf)));
				}
			}
			if (samples & 1) {
				// Carry over the dangling sample into the next
				// round, or...
				sb.dma.remain_size = 1;
				sb.dma.buf.b16[0] = sb.dma.buf.b16[samples - 1];
			} else {
				// ...the DMA transfer is done
				sb.dma.remain_size = 0;
			}
		} else { // 16-bit mono
			bytes_read = read_dma_16bit(bytes_to_read);
			samples    = bytes_read / dma16_to_sample_divisor;
			frames     = check_cast<uint16_t>(samples / channels);
			assert(channels == 1 && frames == samples); // sanity-check
			                                            // mono
			if (sb.dma.sign) {
				enqueue_frames(std::make_unique<AudioVectorM16>(
					frames, maybe_silence(samples, sb.dma.buf.b16)));
			} else {
				const auto unsigned_buf = reinterpret_cast<uint16_t*>(sb.dma.buf.b16);
				enqueue_frames(std::make_unique<AudioVectorM16U>(
					frames, maybe_silence(samples, unsigned_buf)));
			}
		}
		break;

	default:
		LOG_MSG("%s: Unhandled DMA mode %d",
		        sb_log_prefix(),
		        static_cast<int>(sb.dma.mode));
		sb.mode = DspMode::None;
		return;
	}

	// Sanity check
	assertm(frames <= samples, "Frames should never exceed samples");

	// Deduct the DMA bytes read from the remaining to still read
	sb.dma.left -= bytes_read;

	if (!sb.dma.left) {
		PIC_RemoveEvents(ProcessDMATransfer);

		if (sb.dma.mode >= DmaMode::Pcm16Bit) {
			sb_raise_irq(SbIrq::Irq16);
		} else {
			sb_raise_irq(SbIrq::Irq8);
		}

		if (!sb.dma.autoinit) {
			// Not new single cycle transfer waiting?
			if (!sb.dma.singlesize) {
				LOG(LOG_SB, LOG_NORMAL)
				("Single cycle transfer ended");
				sb.mode     = DspMode::None;
				sb.dma.mode = DmaMode::None;
			} else {
				// A single size transfer is still waiting,
				// handle that now
				sb.dma.left       = sb.dma.singlesize;
				sb.dma.singlesize = 0;
				LOG(LOG_SB, LOG_NORMAL)
				("Switch to Single cycle transfer begun");
			}
		} else {
			if (!sb.dma.autosize) {
				LOG(LOG_SB, LOG_NORMAL)
				("Auto-init transfer with 0 size");
				sb.mode = DspMode::None;
			}
			// Continue with a new auto init transfer
			sb.dma.left = sb.dma.autosize;
		}
	}
#if 0
	LOG_MSG("%s: sb.dma.mode=%d, stereo=%d, signed=%d, bytes_requested=%u,"
	        "bytes_to_read=%u, bytes_read = %u, samples = %u, frames = %u, dma.left = %u",
	        sb_log_prefix(),
	        sb.dma.mode,
	        sb.dma.stereo,
	        sb.dma.sign,
	        bytes_requested,
	        bytes_to_read,
	        bytes_read,
	        samples,
	        frames,
	        sb.dma.left);
#endif
}

static void suppress_dma_transfer(const uint32_t bytes_to_read)
{
	auto num_bytes = bytes_to_read;
	if (sb.dma.left < num_bytes) {
		num_bytes = sb.dma.left;
	}

	const auto read = read_dma_8bit(num_bytes);

	sb.dma.left -= read;
	if (!sb.dma.left) {
		if (sb.dma.mode >= DmaMode::Pcm16Bit) {
			sb_raise_irq(SbIrq::Irq16);
		} else {
			sb_raise_irq(SbIrq::Irq8);
		}

		// FIX, use the auto to single switch mechanics here as well or
		// find a better way to silence
		if (sb.dma.autoinit) {
			sb.dma.left = sb.dma.autosize;
		} else {
			sb.mode     = DspMode::None;
			sb.dma.mode = DmaMode::None;
		}
	}

	if (sb.dma.left) {
		const uint32_t bigger = (sb.dma.left > sb.dma.min) ? sb.dma.min
		                                                   : sb.dma.left;
		double delay          = (bigger * 1000.0) / sb.dma.rate;
		PIC_AddEvent(suppress_dma_transfer, delay, bigger);
	}
}

static void flush_remainig_dma_transfer()
{
	if (!sb.dma.left) {
		return;
	}

	if (!sb.speaker_enabled && sb.type != SbType::SB16) {
		const auto num_bytes = std::min(sb.dma.min, sb.dma.left);
		const double delay   = (num_bytes * 1000.0) / sb.dma.rate;

		PIC_AddEvent(suppress_dma_transfer, delay, num_bytes);

		LOG(LOG_SB, LOG_NORMAL)
		("%s: Silent DMA Transfer scheduling IRQ in %.3f milliseconds",
		 sb_log_prefix(),
		 delay);

	} else if (sb.dma.left < sb.dma.min) {
		const double delay = (sb.dma.left * 1000.0) / sb.dma.rate;

		LOG(LOG_SB, LOG_NORMAL)
		("%s: Short transfer scheduling IRQ in %.3f milliseconds",
		 sb_log_prefix(),
		 delay);

		PIC_AddEvent(ProcessDMATransfer, delay, sb.dma.left);
	}
}

static void set_channel_rate_hz(const int requested_rate_hz)
{
	const auto rate_hz = std::clamp(MinPlaybackRateHz,
	                                requested_rate_hz,
	                                NativeDacRateHz);

	assert(sb.chan);
	if (sb.chan->GetSampleRate() != rate_hz) {
		sb.chan->SetSampleRate(rate_hz);
	}
}

static void dsp_change_mode(const DspMode mode)
{
	if (sb.mode != mode) {
		sb.mode = mode;
	}
}

static void dsp_raise_irq_event(const uint32_t /*val*/)
{
	sb_raise_irq(SbIrq::Irq8);
}

#if (C_DEBUG)
static const char* get_dma_mode_name()
{
	switch (sb.dma.mode) {
	case DmaMode::Adpcm2Bit: return "2-bit ADPCM"; break;
	case DmaMode::Adpcm3Bit: return "3-bit ADPCM"; break;
	case DmaMode::Adpcm4Bit: return "4-bit ADPCM"; break;
	case DmaMode::Pcm8Bit: return "8-bit PCM"; break;
	case DmaMode::Pcm16Bit: return "16-bit PCM"; break;
	case DmaMode::Pcm16BitAliased: return "16-bit (aliased) PCM"; break;
	case DmaMode::None: return "non-DMA"; break;
	};
	return "Unknown DMA mode";
}
#endif

static void dsp_do_dma_transfer(const DmaMode mode, const uint32_t freq_hz,
                                const bool autoinit, const bool stereo)
{
	// Starting a new transfer will clear any active irqs?
	sb.irq.pending_8bit  = false;
	sb.irq.pending_16bit = false;
	PIC_DeActivateIRQ(sb.hw.irq);

	switch (mode) {
	case DmaMode::Adpcm2Bit: sb.dma.mul = (1 << SbShift) / 4; break;
	case DmaMode::Adpcm3Bit: sb.dma.mul = (1 << SbShift) / 3; break;
	case DmaMode::Adpcm4Bit: sb.dma.mul = (1 << SbShift) / 2; break;
	case DmaMode::Pcm8Bit: sb.dma.mul = (1 << SbShift); break;
	case DmaMode::Pcm16Bit: sb.dma.mul = (1 << SbShift); break;
	case DmaMode::Pcm16BitAliased: sb.dma.mul = (1 << SbShift) * 2; break;
	default:
		LOG(LOG_SB, LOG_ERROR)("DSP:Illegal transfer mode %d", static_cast<int>(mode));
		return;
	}

	// Going from an active autoinit into a single cycle
	if (sb.mode >= DspMode::Dma && sb.dma.autoinit && !autoinit) {
		// Don't do anything, the total will flip over on the next transfer
	}
	// Just a normal single cycle transfer
	else if (!autoinit) {
		sb.dma.left       = sb.dma.singlesize;
		sb.dma.singlesize = 0;
	}
	// Going into an autoinit transfer
	else {
		// Transfer full cycle again
		sb.dma.left = sb.dma.autosize;
	}
	sb.dma.autoinit = autoinit;
	sb.dma.mode     = mode;
	sb.dma.stereo   = stereo;

	// Double the reading speed for stereo mode
	if (sb.dma.stereo) {
		sb.dma.mul *= 2;
	}
	sb.dma.rate = (sb.freq_hz * sb.dma.mul) >> SbShift;
	sb.dma.min  = (sb.dma.rate * 3) / 1000;
	set_channel_rate_hz(freq_hz);

	PIC_RemoveEvents(ProcessDMATransfer);
	// Set to be masked, the dma call can change this again.
	sb.mode = DspMode::DmaMasked;
	sb.dma.chan->RegisterCallback(dsp_dma_callback);

#if (C_DEBUG)
	LOG(LOG_SB, LOG_NORMAL)
	("DMA Transfer:%s %s %s freq_hz %d rate %d size %d",
	 get_dma_mode_name(),
	 stereo ? "Stereo" : "Mono",
	 autoinit ? "Auto-Init" : "Single-Cycle",
	 freq_hz,
	 sb.dma.rate,
	 sb.dma.left);
#endif
}

static void dsp_prepare_dma_old(const DmaMode mode, const bool autoinit,
                                const bool sign)
{
	sb.dma.sign = sign;
	if (!autoinit) {
		sb.dma.singlesize = 1 + sb.dsp.in.data[0] + (sb.dsp.in.data[1] << 8);
	}
	sb.dma.chan = DMA_GetChannel(sb.hw.dma8);

	dsp_do_dma_transfer(mode,
	                    sb.freq_hz / (sb.mixer.stereo_enabled ? 2 : 1),
	                    autoinit,
	                    sb.mixer.stereo_enabled);
}

static void dsp_prepare_dma_new(const DmaMode mode, const uint32_t length,
                                const bool autoinit, const bool stereo)
{
	const auto freq_hz = sb.freq_hz;

	DmaMode new_mode    = mode;
	uint32_t new_length = length;

	// Equal length if data format and dma channel are both 16-bit or 8-bit
	if (mode == DmaMode::Pcm16Bit) {
		if (sb.hw.dma16 != 0xff) {
			sb.dma.chan = DMA_GetChannel(sb.hw.dma16);
			if (sb.dma.chan == nullptr) {
				sb.dma.chan = DMA_GetChannel(sb.hw.dma8);
				new_mode    = DmaMode::Pcm16BitAliased;
				new_length *= 2;
			}
		} else {
			sb.dma.chan = DMA_GetChannel(sb.hw.dma8);
			new_mode    = DmaMode::Pcm16BitAliased;
			// UNDOCUMENTED:
			// In aliased mode sample length is written to DSP as
			// number of 16-bit samples so we need double 8-bit DMA
			// buffer length
			new_length *= 2;
		}
	} else {
		sb.dma.chan = DMA_GetChannel(sb.hw.dma8);
	}

	// Set the length to the correct register depending on mode
	if (autoinit) {
		sb.dma.autosize = new_length;
	} else {
		sb.dma.singlesize = new_length;
	}

	dsp_do_dma_transfer(new_mode, freq_hz, autoinit, stereo);
}

static void dsp_add_data(const uint8_t val)
{
	if (sb.dsp.out.used < DspBufSize) {
		auto start = sb.dsp.out.used + sb.dsp.out.pos;
		if (start >= DspBufSize) {
			start -= DspBufSize;
		}
		sb.dsp.out.data[start] = val;
		sb.dsp.out.used++;
	} else {
		LOG(LOG_SB, LOG_ERROR)("DSP:Data Output buffer full");
	}
}

static void dsp_finish_reset(const uint32_t /*val*/)
{
	dsp_flush_data();
	dsp_add_data(0xaa);
	sb.dsp.state = DspState::Normal;
}

static void dsp_reset()
{
	LOG(LOG_SB, LOG_ERROR)("DSP:Reset");

	PIC_DeActivateIRQ(sb.hw.irq);

	dsp_change_mode(DspMode::None);
	dsp_flush_data();

	sb.dsp.cmd     = DspNoCommand;
	sb.dsp.cmd_len = 0;
	sb.dsp.in.pos  = 0;

	sb.dsp.write_status_counter = 0;
	sb.dsp.reset_tally++;

	PIC_RemoveEvents(dsp_finish_reset);

	sb.dma.left        = 0;
	sb.dma.singlesize  = 0;
	sb.dma.autosize    = 0;
	sb.dma.stereo      = false;
	sb.dma.sign        = false;
	sb.dma.autoinit    = false;
	sb.dma.mode        = DmaMode::None;
	sb.dma.remain_size = 0;

	if (sb.dma.chan) {
		sb.dma.chan->ClearRequest();
	}

	sb.adpcm         = {};
	sb.freq_hz       = DefaultPlaybackRateHz;
	sb.time_constant = 45;
	sb.dac.used      = 0;
	sb.dac.last      = 0;
	sb.e2.value      = 0xaa;
	sb.e2.count      = 0;

	sb.irq.pending_8bit  = false;
	sb.irq.pending_16bit = false;

	set_channel_rate_hz(DefaultPlaybackRateHz);

	init_speaker_state();

	PIC_RemoveEvents(ProcessDMATransfer);
}

static void dsp_do_reset(const uint8_t val)
{
	if (((val & 1) != 0) && (sb.dsp.state != DspState::Reset)) {
		// TODO Get out of highspeed mode
		dsp_reset();

		sb.dsp.state = DspState::Reset;
	} else if (((val & 1) == 0) && (sb.dsp.state == DspState::Reset)) {
		// reset off
		sb.dsp.state = DspState::ResetWait;

		PIC_RemoveEvents(dsp_finish_reset);
		// 20 microseconds
		PIC_AddEvent(dsp_finish_reset, 20.0 / 1000.0, 0);

		LOG_MSG("%s: Resetting DSP", sb_log_prefix());
	}
}

static void dsp_e2_dma_callback(const DmaChannel* /*chan*/, const DMAEvent event)
{
	if (event == DMA_UNMASKED) {
		uint8_t val = (uint8_t)(sb.e2.value & 0xff);

		DmaChannel* chan = DMA_GetChannel(sb.hw.dma8);

		chan->RegisterCallback(nullptr);
		chan->Write(1, &val);
	}
}

static void dsp_adc_callback(const DmaChannel* /*chan*/, const DMAEvent event)
{
	if (event != DMA_UNMASKED) {
		return;
	}

	uint8_t val    = 128;
	DmaChannel* ch = DMA_GetChannel(sb.hw.dma8);

	while (sb.dma.left--) {
		ch->Write(1, &val);
	}

	sb_raise_irq(SbIrq::Irq8);
	ch->RegisterCallback(nullptr);
}

static void dsp_change_rate(const uint32_t freq_hz)
{
	if (sb.freq_hz != freq_hz && sb.dma.mode != DmaMode::None) {
		set_channel_rate_hz(freq_hz / (sb.mixer.stereo_enabled ? 2 : 1));
		sb.dma.rate = (freq_hz * sb.dma.mul) >> SbShift;
		sb.dma.min  = (sb.dma.rate * 3) / 1000;
	}
	sb.freq_hz = freq_hz;
}

static bool check_sb16_only()
{
	if (sb.type != SbType::SB16) {
		LOG(LOG_SB, LOG_ERROR)
		("DSP:Command %2X requires SB16", sb.dsp.cmd);
		return false;
	}
	return true;
}

static bool check_sb2_or_above()
{
	if (sb.type <= SbType::SB1) {
		LOG(LOG_SB, LOG_ERROR)
		("DSP:Command %2X requires SB2 or above", sb.dsp.cmd);
		return false;
	}
	return true;
}

static void dsp_do_command()
{
	if (sb.ess_type != EssType::None && sb.dsp.cmd >= 0xa0 && sb.dsp.cmd <= 0xcf) {
		LOG_TRACE("ESS: Command: %02xh", sb.dsp.cmd);
		// ESS DSP commands overlap with SB16 commands. We handle them here,
		// not mucking up the switch statement.

		if (sb.dsp.cmd == 0xc6 || sb.dsp.cmd == 0xc7) {
			// set(0xc6) clear(0xc7) extended mode
			LOG_TRACE("ESS: Extended DAC mode turned on %s",
			          (sb.dsp.cmd == 0xc6) ? "on" : "off");
		} else {
			LOG_TRACE("ESS: Unknown command %02xh", sb.dsp.cmd);
		}

		sb.dsp.cmd     = DspNoCommand;
		sb.dsp.cmd_len = 0;
		sb.dsp.in.pos  = 0;
		return;
	}

	//	LOG_MSG("DSP Command %X",sb.dsp.cmd);
	switch (sb.dsp.cmd) {
	case 0x04:
		if (sb.type == SbType::SB16) {
			// SB16 ASP set mode register
			if ((sb.dsp.in.data[0] & 0xf1) == 0xf1) {
				asp_init_in_progress = true;
			} else {
				asp_init_in_progress = false;
			}

			LOG(LOG_SB, LOG_NORMAL)
			("DSP Unhandled SB16ASP command %X (set mode register to %X)",
			 sb.dsp.cmd,
			 sb.dsp.in.data[0]);

		} else {
			// DSP Status SB 2.0/pro version. NOT SB16.
			dsp_flush_data();
			if (sb.type == SbType::SB2) {
				dsp_add_data(0x88);

			} else if ((sb.type == SbType::SBPro1) || (sb.type == SbType::SBPro2)) {
				dsp_add_data(0x7b);

			} else {
				// Everything enabled
				dsp_add_data(0xff);
			}
		}
		break;

	case 0x05: // SB16 ASP set codec parameter
		LOG(LOG_SB, LOG_NORMAL)
		("DSP Unhandled SB16ASP command %X (set codec parameter)",
		 sb.dsp.cmd);
		break;

	case 0x08: // SB16 ASP get version
		LOG(LOG_SB, LOG_NORMAL)
		("DSP Unhandled SB16ASP command %X sub %X",
		 sb.dsp.cmd,
		 sb.dsp.in.data[0]);

		if (sb.type == SbType::SB16) {
			switch (sb.dsp.in.data[0]) {
			case 0x03:
				dsp_add_data(0x18); // version ID (??)
				break;

			default:
				LOG(LOG_SB, LOG_NORMAL)
				("DSP Unhandled SB16ASP command %X sub %X",
				 sb.dsp.cmd,
				 sb.dsp.in.data[0]);
				break;
			}
		} else {
			LOG(LOG_SB, LOG_NORMAL)
			("DSP Unhandled SB16ASP command %X sub %X",
			 sb.dsp.cmd,
			 sb.dsp.in.data[0]);
		}
		break;

	case 0x0e: // SB16 ASP set register
		if (sb.type == SbType::SB16) {
#if 0
			LOG(LOG_SB, LOG_NORMAL)
			("SB16 ASP set register %X := %X",
			 sb.dsp.in.data[0],
			 sb.dsp.in.data[1]);
#endif
			asp_regs[sb.dsp.in.data[0]] = sb.dsp.in.data[1];
		} else {
			LOG(LOG_SB, LOG_NORMAL)
			("DSP Unhandled SB16ASP command %X (set register)",
			 sb.dsp.cmd);
		}
		break;

	case 0x0f: // SB16 ASP get register
		if (sb.type == SbType::SB16) {
			if ((asp_init_in_progress) && (sb.dsp.in.data[0] == 0x83)) {
				asp_regs[0x83] = ~asp_regs[0x83];
			}
#if 0
			LOG(LOG_SB, LOG_NORMAL)
			("SB16 ASP get register %X == X",
			 sb.dsp.in.data[0],
			 asp_regs[sb.dsp.in.data[0]]);
#endif
			dsp_add_data(asp_regs[sb.dsp.in.data[0]]);
		} else {
			LOG(LOG_SB, LOG_NORMAL)
			("DSP Unhandled SB16ASP command %X (get register)",
			 sb.dsp.cmd);
		}
		break;

	case 0x10: // Direct DAC
		dsp_change_mode(DspMode::Dac);
		if (sb.dac.used < DspDacSize) {
			const auto mono_sample = lut_u8to16[sb.dsp.in.data[0]];
			sb.dac.data[sb.dac.used++] = mono_sample;
			sb.dac.data[sb.dac.used++] = mono_sample;
		}
		break;

	case 0x24: // Singe Cycle 8-Bit DMA ADC
		// Directly write to left?
		sb.dma.left = 1 + sb.dsp.in.data[0] + (sb.dsp.in.data[1] << 8);
		sb.dma.sign = false;
		LOG(LOG_SB, LOG_ERROR)
		("DSP:Faked ADC for %u bytes", sb.dma.left);
		DMA_GetChannel(sb.hw.dma8)->RegisterCallback(dsp_adc_callback);
		break;

	case 0x14: // Singe Cycle 8-Bit DMA DAC
		[[fallthrough]];

	case 0x15:
		// Wari hack. Waru uses this one instead of 0x14, but some
		// weird stuff going on there anyway
		[[fallthrough]];

	case 0x91:
		// Singe Cycle 8-Bit DMA High speed DAC
		// Note: 0x91 is documented only for DSP ver.2.x and 3.x,
		// not 4.x
		dsp_prepare_dma_old(DmaMode::Pcm8Bit, false, false);
		break;

	case 0x90: // Auto Init 8-bit DMA High Speed
		[[fallthrough]];

	case 0x1c: // Auto Init 8-bit DMA
	           // Note: 0x90 is documented only for DSP ver.2.x and 3.x,
	           // not 4.x
		if (check_sb2_or_above()) {
			dsp_prepare_dma_old(DmaMode::Pcm8Bit, true, false);
		}
		break;

	case 0x38: // Write to SB MIDI Output
		if (sb.midi_enabled == true) {
			MIDI_RawOutByte(sb.dsp.in.data[0]);
		}
		break;

	case 0x40: // Set Timeconstant
		dsp_change_rate(1000000 / (256 - sb.dsp.in.data[0]));
		break;

	case 0x41: // Set Output Samplerate
		[[fallthrough]];

	case 0x42: // Set Input Samplerate
		// Note: 0x42 is handled like 0x41, needed by Fasttracker II
		if (check_sb16_only()) {
			dsp_change_rate((sb.dsp.in.data[0] << 8) | sb.dsp.in.data[1]);
		}
		break;

	case 0x48: // Set DMA Block Size
		if (check_sb2_or_above()) {
			sb.dma.autosize = 1 + sb.dsp.in.data[0] + (sb.dsp.in.data[1] << 8);
		}
		break;

	case 0x75: // 075h : Single Cycle 4-bit ADPCM Reference
		sb.adpcm.haveref = true;
		[[fallthrough]];

	case 0x74: // 074h : Single Cycle 4-bit ADPCM
		dsp_prepare_dma_old(DmaMode::Adpcm4Bit, false, false);
		break;

	case 0x77: // 077h : Single Cycle 3-bit(2.6bit) ADPCM Reference
		sb.adpcm.haveref = true;
		[[fallthrough]];

	case 0x76: // 076h : Single Cycle 3-bit(2.6bit) ADPCM
		dsp_prepare_dma_old(DmaMode::Adpcm3Bit, false, false);
		break;

	case 0x7d: // Auto Init 4-bit ADPCM Reference
		if (check_sb2_or_above()) {
			sb.adpcm.haveref = true;
			dsp_prepare_dma_old(DmaMode::Adpcm4Bit, true, false);
		}
		break;

	case 0x17: // 017h : Single Cycle 2-bit ADPCM Reference
		sb.adpcm.haveref = true;
		[[fallthrough]];

	case 0x16: // 016h : Single Cycle 2-bit ADPCM
		dsp_prepare_dma_old(DmaMode::Adpcm2Bit, false, false);
		break;

	case 0x80: // Silence DAC
		PIC_AddEvent(&dsp_raise_irq_event,
		             (1000.0 *
		              (1 + sb.dsp.in.data[0] + (sb.dsp.in.data[1] << 8)) /
		              sb.freq_hz));
		break;

	// clang-format off
	// 0xb0 to 0xbf
	case 0xb0: case 0xb1: case 0xb2: case 0xb3:
	case 0xb4: case 0xb5: case 0xb6: case 0xb7:
	case 0xb8: case 0xb9: case 0xba: case 0xbb:
	case 0xbc: case 0xbd: case 0xbe: case 0xbf:

	// 0xc0 to 0xcf
	case 0xc0: case 0xc1: case 0xc2: case 0xc3:
	case 0xc4: case 0xc5: case 0xc6: case 0xc7:
	case 0xc8: case 0xc9: case 0xca: case 0xcb:
	case 0xcc: case 0xcd: case 0xce: case 0xcf:
		// clang-format on

		// Generic 8/16-bit DMA
		if (!check_sb16_only()) {
			break;
		}
		sb.dma.sign = (sb.dsp.in.data[0] & 0x10) > 0;

		dsp_prepare_dma_new((sb.dsp.cmd & 0x10) ? DmaMode::Pcm16Bit
		                                        : DmaMode::Pcm8Bit,
		                    1 + sb.dsp.in.data[1] + (sb.dsp.in.data[2] << 8),
		                    (sb.dsp.cmd & 0x4) > 0,
		                    (sb.dsp.in.data[0] & 0x20) > 0);
		break;

	case 0xd5: // Halt 16-bit DMA
		if (!check_sb16_only()) {
			break;
		}
		[[fallthrough]];

	case 0xd0: // Halt 8-bit DMA
		// dsp_change_mode(DspMode::None);
		LOG(LOG_SB, LOG_NORMAL)("Halt DMA Command");
		// Games sometimes already program a new dma before stopping,
		// gives noise
		if (sb.mode == DspMode::None) {
			// possibly different code here that does not switch to
			// DspMode::DmaPause
		}
		sb.mode = DspMode::DmaPause;
		PIC_RemoveEvents(ProcessDMATransfer);
		break;

	case 0xd1: // Enable Speaker
		dsp_enable_speaker(true);
		break;

	case 0xd3: // Disable Speaker
		dsp_enable_speaker(false);
		break;

	case 0xd8: // Speaker status
		if (!check_sb2_or_above()) {
			break;
		}

		dsp_flush_data();

		if (sb.speaker_enabled) {
			dsp_add_data(0xff);
			// If the game is courteous enough to ask if the speaker
			// is ready, then we can be confident it won't play
			// garbage content, so we zero the warmup count down.
			sb.dsp.warmup_remaining_ms = 0;
		} else {
			dsp_add_data(0x00);
		}
		break;

	case 0xd6: // Continue DMA 16-bit
		if (!check_sb16_only()) {
			break;
		}
		[[fallthrough]];

	case 0xd4: // Continue DMA 8-bit
		LOG(LOG_SB, LOG_NORMAL)("Continue DMA command");

		if (sb.mode == DspMode::DmaPause) {
			sb.mode = DspMode::DmaMasked;
			if (sb.dma.chan != nullptr) {
				sb.dma.chan->RegisterCallback(dsp_dma_callback);
			}
		}
		break;

	case 0xd9: // Exit Autoinitialize 16-bit
		if (!check_sb16_only()) {
			break;
		}
		[[fallthrough]];

	case 0xda: // Exit Autoinitialize 8-bit
		if (check_sb2_or_above()) {
			LOG(LOG_SB, LOG_NORMAL)("Exit Autoinit command");

			// Should stop itself
			sb.dma.autoinit = false;
		}
		break;

	case 0xe0: // DSP Identification - SB2.0+
		dsp_flush_data();
		dsp_add_data(~sb.dsp.in.data[0]);
		break;

	case 0xe1: // Get DSP Version
		dsp_flush_data();

		switch (sb.type) {
			case SbType::SB1:
			dsp_add_data(0x01);
			dsp_add_data(0x05);
			break;

		case SbType::SB2:
			dsp_add_data(0x02);
			dsp_add_data(0x01);
			break;

		case SbType::SBPro1:
			dsp_add_data(0x03);
			dsp_add_data(0x00);
			break;

		case SbType::SBPro2:
			if (sb.ess_type != EssType::None) {
				dsp_add_data(0x03);
				dsp_add_data(0x01);
			} else {
				dsp_add_data(0x03);
				dsp_add_data(0x02);
			}
			break;

		case SbType::SB16:
			dsp_add_data(0x04);
			dsp_add_data(0x05);
			break;

		default: break;
		}
		break;

	case 0xe2: // Weird DMA identification write routine
	{
		LOG(LOG_SB, LOG_NORMAL)("DSP Function 0xe2");
		for (uint8_t i = 0; i < 8; i++) {
			if ((sb.dsp.in.data[0] >> i) & 0x01) {
				sb.e2.value += e2_incr_table[sb.e2.count % 4][i];
			}
		}
		sb.e2.value += e2_incr_table[sb.e2.count % 4][8];
		sb.e2.count++;
		DMA_GetChannel(sb.hw.dma8)->RegisterCallback(dsp_e2_dma_callback);
	} break;

	case 0xe3: // DSP Copyright
		dsp_flush_data();

		if (sb.ess_type != EssType::None) {
			// ESS chips do not return a copyright string
			dsp_add_data(0);
		} else {
			for (const auto c :
			     "COPYRIGHT (C) CREATIVE TECHNOLOGY LTD, 1992.") {
				dsp_add_data(static_cast<uint8_t>(c));
			}
		}
		break;

	case 0xe4: // Write Test Register
		sb.dsp.test_register = sb.dsp.in.data[0];
		break;

	case 0xe7: // ESS detect/read config
		switch (sb.ess_type) {
		case EssType::None: break;

		case EssType::Es1688:
			dsp_flush_data();
			// Determined via Windows driver debugging.
			dsp_add_data(0x68);
			dsp_add_data(0x80 | 0x09);
			break;
		}
		break;

	case 0xe8: // Read Test Register
		dsp_flush_data();
		dsp_add_data(sb.dsp.test_register);
		break;

	case 0xf2: // Trigger 8bit IRQ
		// Small delay in order to emulate the slowness of the DSP,
		// fixes Llamatron 2012 and Lemmings 3D
		PIC_AddEvent(&dsp_raise_irq_event, 0.01);

		LOG(LOG_SB, LOG_NORMAL)("Trigger 8bit IRQ command");
		break;

	case 0xf3: // Trigger 16bit IRQ
		if (check_sb16_only()) {
			sb_raise_irq(SbIrq::Irq16);

			LOG(LOG_SB, LOG_NORMAL)("Trigger 16bit IRQ command");
		}
		break;

	case 0xf8: // Undocumented, pre-SB16 only
		dsp_flush_data();
		dsp_add_data(0);
		break;

	case 0x30:
	case 0x31:
		LOG(LOG_SB, LOG_ERROR)
		("DSP:Unimplemented MIDI I/O command %2X", sb.dsp.cmd);
		break;

	case 0x34:
	case 0x35:
	case 0x36:
	case 0x37:
		if (check_sb2_or_above()) {
			LOG(LOG_SB, LOG_ERROR)
			("DSP:Unimplemented MIDI UART command %2X", sb.dsp.cmd);
		}
		break;

	case 0x7f:
	case 0x1f:
		if (check_sb2_or_above()) {
			LOG(LOG_SB, LOG_ERROR)
			("DSP:Unimplemented auto-init DMA ADPCM command %2X", sb.dsp.cmd);
		}
		break;

	case 0x20:
		dsp_add_data(0x7f); // Fake silent input for Creative parrot
		break;

	case 0x2c:
	case 0x98:
	case 0x99: // Documented only for DSP 2.x and 3.x
	case 0xa0:
	case 0xa8: // Documented only for DSP 3.x
		LOG(LOG_SB, LOG_ERROR)
		("DSP:Unimplemented input command %2X", sb.dsp.cmd);
		break;

	case 0xf9: // SB16 ASP ???
		if (sb.type == SbType::SB16) {
			LOG(LOG_SB, LOG_NORMAL)
			("SB16 ASP unknown function %x", sb.dsp.in.data[0]);

			// Just feed it what it expects
			switch (sb.dsp.in.data[0]) {
			case 0x0b: dsp_add_data(0x00); break;
			case 0x0e: dsp_add_data(0xff); break;
			case 0x0f: dsp_add_data(0x07); break;
			case 0x23: dsp_add_data(0x00); break;
			case 0x24: dsp_add_data(0x00); break;
			case 0x2b: dsp_add_data(0x00); break;
			case 0x2c: dsp_add_data(0x00); break;
			case 0x2d: dsp_add_data(0x00); break;
			case 0x37: dsp_add_data(0x38); break;
			default: dsp_add_data(0x00); break;
			}
		} else {
			LOG(LOG_SB, LOG_NORMAL)
			("SB16 ASP unknown function %X", sb.dsp.cmd);
		}
		break;

	default:
		LOG(LOG_SB, LOG_ERROR)
		("DSP:Unhandled (undocumented) command %2X", sb.dsp.cmd);
		break;
	}

	sb.dsp.cmd     = DspNoCommand;
	sb.dsp.cmd_len = 0;
	sb.dsp.in.pos  = 0;
}

static void dsp_do_write(const uint8_t val)
{
	switch (sb.dsp.cmd) {
	case DspNoCommand:
		sb.dsp.cmd = val;
		if (sb.type == SbType::SB16) {
			sb.dsp.cmd_len = dsp_cmd_len_sb16[val];
		} else {
			sb.dsp.cmd_len = dsp_cmd_len_sb[val];
		}
		sb.dsp.in.pos = 0;
		if (!sb.dsp.cmd_len) {
			dsp_do_command();
		}
		break;
	default:
		sb.dsp.in.data[sb.dsp.in.pos] = val;
		sb.dsp.in.pos++;
		if (sb.dsp.in.pos >= sb.dsp.cmd_len) {
			dsp_do_command();
		}
	}
}

static uint8_t dsp_read_data()
{
	// Static so it repeats the last value on succesive reads (JANGLE DEMO)
	if (sb.dsp.out.used) {
		sb.dsp.out.lastval = sb.dsp.out.data[sb.dsp.out.pos];
		sb.dsp.out.pos++;
		if (sb.dsp.out.pos >= DspBufSize) {
			sb.dsp.out.pos -= DspBufSize;
		}
		sb.dsp.out.used--;
	}
	return sb.dsp.out.lastval;
}

static float calc_vol(const uint8_t amount)
{
	uint8_t count = 31 - amount;

	float db = static_cast<float>(count);

	if (sb.type == SbType::SBPro1 || sb.type == SbType::SBPro2) {
		if (count) {
			if (count < 16) {
				db -= 1.0f;
			} else if (count > 16) {
				db += 1.0f;
			}
			if (count == 24) {
				db += 2.0f;
			}
			if (count > 27) {
				// Turn it off.
				return 0.0f;
			}
		}
	} else {
		// Give the rest, the SB16 scale, as we don't have data.
		db *= 2.0f;
		if (count > 20) {
			db -= 1.0f;
		}
	}

	return powf(10.0f, -0.05f * db);
}

static void ctmixer_update_volumes()
{
	if (!sb.mixer.enabled) {
		return;
	}

	float m0 = calc_vol(sb.mixer.master[0]);
	float m1 = calc_vol(sb.mixer.master[1]);

	auto chan = MIXER_FindChannel(ChannelName::SoundBlasterDac);
	if (chan) {
		chan->SetAppVolume({m0 * calc_vol(sb.mixer.dac[0]),
		                    m1 * calc_vol(sb.mixer.dac[1])});
	}

	chan = MIXER_FindChannel(ChannelName::Opl);
	if (chan) {
		chan->SetAppVolume({m0 * calc_vol(sb.mixer.fm[0]),
		                    m1 * calc_vol(sb.mixer.fm[1])});
	}

	chan = MIXER_FindChannel(ChannelName::CdAudio);
	if (chan) {
		chan->SetAppVolume({m0 * calc_vol(sb.mixer.cda[0]),
		                    m1 * calc_vol(sb.mixer.cda[1])});
	}
}

static void ctmixer_reset()
{
	constexpr auto DefaultVolume = 31;

	sb.mixer.fm[0] = DefaultVolume;
	sb.mixer.fm[1] = DefaultVolume;

	sb.mixer.cda[0] = DefaultVolume;
	sb.mixer.cda[1] = DefaultVolume;

	sb.mixer.dac[0] = DefaultVolume;
	sb.mixer.dac[1] = DefaultVolume;

	sb.mixer.master[0] = DefaultVolume;
	sb.mixer.master[1] = DefaultVolume;

	ctmixer_update_volumes();
}

static void write_sb_pro_volume(uint8_t* dest, const uint8_t value)
{
	dest[0] = ((((value)&0xf0) >> 3) | (sb.type == SbType::SB16 ? 1 : 3));
	dest[1] = ((((value)&0x0f) << 1) | (sb.type == SbType::SB16 ? 1 : 3));
}

static uint8_t read_sb_pro_volume(const uint8_t* src)
{
	return ((((src[0] & 0x1e) << 3) | ((src[1] & 0x1e) >> 1)) |
	        ((sb.type == SbType::SBPro1 || sb.type == SbType::SBPro2) ? 0x11 : 0));
}

static void dsp_change_stereo(const bool stereo)
{
	if (!sb.dma.stereo && stereo) {
		set_channel_rate_hz(sb.freq_hz / 2);
		sb.dma.mul *= 2;
		sb.dma.rate = (sb.freq_hz * sb.dma.mul) >> SbShift;
		sb.dma.min  = (sb.dma.rate * 3) / 1000;

	} else if (sb.dma.stereo && !stereo) {
		set_channel_rate_hz(sb.freq_hz);
		sb.dma.mul /= 2;
		sb.dma.rate = (sb.freq_hz * sb.dma.mul) >> SbShift;
		sb.dma.min  = (sb.dma.rate * 3) / 1000;
	}

	sb.dma.stereo = stereo;
}

static void write_ess_volume(const uint8_t val, uint8_t* out)
{
	auto map_nibble_to_5bit = [](const uint8_t v) {
		assert(v <= 0x0f);

		// Nibble (4-bit, 0-15) to 5-bit (0-31) mapping:
		//
		// 0 -> 0
		// 1 -> 2
		// 2 -> 4
		// 3 -> 6
		// ....
		// 7 -> 14
		// 8 -> 17
		// 9 -> 19
		// 10 -> 21
		// 11 -> 23
		// ....
		// 15 -> 31
		//
		return (v << 1) | (v >> 3);
	};

	out[0] = map_nibble_to_5bit(high_nibble(val));
	out[1] = map_nibble_to_5bit(low_nibble(val));
}

static uint8_t read_ess_volume(const uint8_t v[2])
{
	assert(v[0] <= 0x1f);
	assert(v[1] <= 0x1f);

	// 5-bit (0-31) to nibble (4-bit, 0-15) mapping
	const auto high_nibble = v[0] >> 1;
	const auto low_nibble  = v[1] >> 1;
	return (high_nibble << 4) + low_nibble;
}

static void ctmixer_write(const uint8_t val)
{
	using namespace bit::literals;

	switch (sb.mixer.index) {
	case 0x00: // Reset
		ctmixer_reset();
		LOG(LOG_SB, LOG_WARN)("Mixer reset value %x", val);
		break;

	case 0x02: // Master Volume (SB2 Only)
		write_sb_pro_volume(sb.mixer.master, (val & 0xf) | (val << 4));
		ctmixer_update_volumes();
		break;

	case 0x04: // DAC Volume (SBPRO)
		write_sb_pro_volume(sb.mixer.dac, val);
		ctmixer_update_volumes();
		break;

	case 0x06: { // FM output selection
		// Somewhat obsolete with dual OPL SBpro + FM volume (SB2 Only)
		// volume controls both channels
		write_sb_pro_volume(sb.mixer.fm, (val & 0xf) | (val << 4));

		ctmixer_update_volumes();

		if (val & 0x60) {
			LOG(LOG_SB, LOG_WARN)
			("Turned FM one channel off. not implemented %X", val);
		}
		// TODO Change FM Mode if only 1 fm channel is selected
	} break;

	case 0x08: // CDA Volume (SB2 Only)
		write_sb_pro_volume(sb.mixer.cda, (val & 0xf) | (val << 4));
		ctmixer_update_volumes();
		break;

	case 0x0a: // Mic Level (SBPRO) or DAC Volume (SB2)
		// 2-bit, 3-bit on SB16
		if (sb.type == SbType::SB2) {
			sb.mixer.dac[0] = sb.mixer.dac[1] = ((val & 0x6) << 2) | 3;
			ctmixer_update_volumes();
		} else {
			sb.mixer.mic = ((val & 0x7) << 2) |
			               (sb.type == SbType::SB16 ? 1 : 3);
		}
		break;

	case 0x0e: {
		// Output/Stereo Select
		sb.mixer.stereo_enabled = bit::is(val, b1);

		if (sb.type == SbType::SBPro2) {
			// Toggling the filter programmatically is only possible
			// on the Sound Blaster Pro 2.

			const auto last_filter_enabled = sb.mixer.filter_enabled;

			// This is not a mistake; clearing bit 5 enables the
			// filter as per the official Creative documentation.
			sb.mixer.filter_enabled = bit::cleared(val, b5);

			if (sb.mixer.filter_configured &&
			    sb.mixer.filter_enabled != last_filter_enabled) {

				if (sb.mixer.filter_always_on) {
					LOG_DEBUG("%s: Filter always on; ignoring %s low-pass filter command",
					          sb_log_prefix(),
					          sb.mixer.filter_enabled
					                  ? "enable"
					                  : "disable");
				} else {
					LOG_DEBUG("%s: %s low-pass filter",
					          sb_log_prefix(),
					          sb.mixer.filter_enabled
					                  ? "Enabling"
					                  : "Disabling");

					sb.chan->SetLowPassFilter(
					        sb.mixer.filter_enabled
					                ? FilterState::On
					                : FilterState::Off);
				}
			}
		}

		dsp_change_stereo(sb.mixer.stereo_enabled);

		LOG(LOG_SB, LOG_WARN)
		("Mixer set to %s", sb.dma.stereo ? "STEREO" : "MONO");
	} break;

	case 0x14: // Audio 1 Play Volume (ESS)
		if (sb.ess_type != EssType::None) {
			write_ess_volume(val, sb.mixer.dac);
			ctmixer_update_volumes();
		}
		break;

	case 0x22: // Master Volume (SBPRO)
		write_sb_pro_volume(sb.mixer.master, val);
		ctmixer_update_volumes();
		break;

	case 0x26: // FM Volume (SBPRO)
		write_sb_pro_volume(sb.mixer.fm, val);
		ctmixer_update_volumes();
		break;

	case 0x28: // CD Audio Volume (SBPRO)
		write_sb_pro_volume(sb.mixer.cda, val);
		ctmixer_update_volumes();
		break;

	case 0x2e: // Line-in Volume (SBPRO)
		write_sb_pro_volume(sb.mixer.lin, val);
		break;

	// case 0x20: // Master Volume Left (SBPRO) ?
	case 0x30: // Master Volume Left (SB16)
		if (sb.type == SbType::SB16) {
			sb.mixer.master[0] = val >> 3;
			ctmixer_update_volumes();
		}
		break;

	// case 0x21:		// Master Volume Right (SBPRO) ?
	case 0x31: // Master Volume Right (SB16)
		if (sb.type == SbType::SB16) {
			sb.mixer.master[1] = val >> 3;
			ctmixer_update_volumes();
		}
		break;

	case 0x32:
		// DAC Volume Left (SB16)
		// Master Volume (ESS)
		if (sb.type == SbType::SB16) {
			sb.mixer.dac[0] = val >> 3;
			ctmixer_update_volumes();

		} else if (sb.ess_type != EssType::None) {
			write_ess_volume(val, sb.mixer.master);
			ctmixer_update_volumes();
		}
		break;

	case 0x33: // DAC Volume Right (SB16)
		if (sb.type == SbType::SB16) {
			sb.mixer.dac[1] = val >> 3;
			ctmixer_update_volumes();
		}
		break;

	case 0x34: // FM Volume Left (SB16)
		if (sb.type == SbType::SB16) {
			sb.mixer.fm[0] = val >> 3;
			ctmixer_update_volumes();
		}
		break;

	case 0x35: // FM Volume Right (SB16)
		if (sb.type == SbType::SB16) {
			sb.mixer.fm[1] = val >> 3;
			ctmixer_update_volumes();
		}
		break;

	case 0x36:
		// CD Volume Left (SB16)
		// FM Volume (ESS)
		if (sb.type == SbType::SB16) {
			sb.mixer.cda[0] = val >> 3;
			ctmixer_update_volumes();

		} else if (sb.ess_type != EssType::None) {
			write_ess_volume(val, sb.mixer.fm);
			ctmixer_update_volumes();
		}
		break;

	case 0x37: // CD Volume Right (SB16)
		if (sb.type == SbType::SB16) {
			sb.mixer.cda[1] = val >> 3;
			ctmixer_update_volumes();
		}
		break;

	case 0x38:
		// Line-in Volume Left (SB16)
		// AuxA (CD) Volume Register (ESS)
		if (sb.type == SbType::SB16) {
			sb.mixer.lin[0] = val >> 3;

		} else if (sb.ess_type != EssType::None) {
			write_ess_volume(val, sb.mixer.cda);
			ctmixer_update_volumes();
		}
		break;

	case 0x39: // Line-in Volume Right (SB16)
		if (sb.type == SbType::SB16) {
			sb.mixer.lin[1] = val >> 3;
		}
		break;

	case 0x3a: // Mic Volume (SB16)
		if (sb.type == SbType::SB16) {
			sb.mixer.mic = val >> 3;
		}
		break;

	case 0x3e: // Line Volume (ESS)
		if (sb.ess_type != EssType::None) {
			write_ess_volume(val, sb.mixer.lin);
		}
		break;

	case 0x80: // IRQ Select
		sb.hw.irq = 0xff;

		if (val & 0x1) {
			sb.hw.irq = 2;
		} else if (val & 0x2) {
			sb.hw.irq = 5;
		} else if (val & 0x4) {
			sb.hw.irq = 7;
		} else if (val & 0x8) {
			sb.hw.irq = 10;
		}
		break;

	case 0x81: // DMA Select
		sb.hw.dma8  = 0xff;
		sb.hw.dma16 = 0xff;

		if (val & 0x1) {
			sb.hw.dma8 = 0;
		} else if (val & 0x2) {
			sb.hw.dma8 = 1;
		} else if (val & 0x8) {
			sb.hw.dma8 = 3;
		}

		if (val & 0x20) {
			sb.hw.dma16 = 5;
		} else if (val & 0x40) {
			sb.hw.dma16 = 6;
		} else if (val & 0x80) {
			sb.hw.dma16 = 7;
		}

		LOG(LOG_SB, LOG_NORMAL)
		("Mixer select dma8:%x dma16:%x", sb.hw.dma8, sb.hw.dma16);
		break;

	default:
		if (((sb.type == SbType::SBPro1 || sb.type == SbType::SBPro2) &&
		     sb.mixer.index == 0x0c) || // Input control on SBPro
		    (sb.type == SbType::SB16 && sb.mixer.index >= 0x3b &&
		     sb.mixer.index <= 0x47)) { // New SB16 registers
			sb.mixer.unhandled[sb.mixer.index] = val;
		}

		LOG(LOG_SB, LOG_WARN)
		("MIXER:Write %X to unhandled index %X", val, sb.mixer.index);
	}
}

static uint8_t ctmixer_read()
{
	uint8_t ret = 0;
	// if ( sb.mixer.index< 0x80) LOG_MSG("Read mixer %x",sb.mixer.index);

	switch (sb.mixer.index) {
	case 0x00: // Reset
		return 0x00;

	case 0x02: // Master Volume (SB2 only)
		return ((sb.mixer.master[1] >> 1) & 0xe);

	case 0x14: // Audio 1 Play Volume (ESS)
		if (sb.ess_type != EssType::None) {
			return read_ess_volume(sb.mixer.dac);
		}
		break;

	case 0x22: // Master Volume (SB Pro)
		return read_sb_pro_volume(sb.mixer.master);

	case 0x04: // DAC Volume (SB Pro)
		return read_sb_pro_volume(sb.mixer.dac);

	case 0x06: // FM Volume (SB2 only) + FM output selection
		return ((sb.mixer.fm[1] >> 1) & 0xe);

	case 0x08: // CD Volume (SB2 only)
		return ((sb.mixer.cda[1] >> 1) & 0xe);

	case 0x0a: // Mic Level (SB Pro) or Voice (SB2 only)
		if (sb.type == SbType::SB2) {
			return (sb.mixer.dac[0] >> 2);
		} else {
			return ((sb.mixer.mic >> 2) & (sb.type == SbType::SB16 ? 7 : 6));
		}

	case 0x0e: // Output/Stereo Select
		return 0x11 | (sb.mixer.stereo_enabled ? 0x02 : 0x00) |
		       (sb.mixer.filter_enabled ? 0x00 : 0x20);

	case 0x26: // FM Volume (SB Pro)
		return read_sb_pro_volume(sb.mixer.fm);

	case 0x28: // CD Audio Volume (SB Pro)
		return read_sb_pro_volume(sb.mixer.cda);

	case 0x2e: // Line-in Volume (SB Pro)
		return read_sb_pro_volume(sb.mixer.lin);

	case 0x30: // Master Volume Left (SB16)
		if (sb.type == SbType::SB16) {
			return sb.mixer.master[0] << 3;
		}
		ret = 0xa;
		break;

	case 0x31: // Master Volume Right (S16)
		if (sb.type == SbType::SB16) {
			return sb.mixer.master[1] << 3;
		}
		ret = 0xa;
		break;

	case 0x32:
		// DAC Volume Left (SB16)
		// Master Volume (ESS)
		if (sb.type == SbType::SB16) {
			return sb.mixer.dac[0] << 3;
		}
		if (sb.ess_type != EssType::None) {
			return read_ess_volume(sb.mixer.master);
		}
		ret = 0xa;
		break;

	case 0x33: // DAC Volume Right (SB16)
		if (sb.type == SbType::SB16) {
			return sb.mixer.dac[1] << 3;
		}
		ret = 0xa;
		break;

	case 0x34:
		// FM Volume Left (SB16)
		if (sb.type == SbType::SB16) {
			return sb.mixer.fm[0] << 3;
		}
		ret = 0xa;
		break;

	case 0x35: // FM Volume Right (SB16)
		if (sb.type == SbType::SB16) {
			return sb.mixer.fm[1] << 3;
		}
		ret = 0xa;
		break;

	case 0x36:
		// CD Volume Left (SB16)
		// FM Volume (ESS)
		if (sb.type == SbType::SB16) {
			return sb.mixer.cda[0] << 3;
		}
		if (sb.ess_type != EssType::None) {
			return read_ess_volume(sb.mixer.fm);
		}
		ret = 0xa;
		break;

	case 0x37: // CD Volume Right (SB16)
		if (sb.type == SbType::SB16) {
			return sb.mixer.cda[1] << 3;
		}
		ret = 0xa;
		break;

	case 0x38:
		// Line-in Volume Left (SB16)
		// AuxA (CD) Volume Register (ESS)
		if (sb.type == SbType::SB16) {
			return sb.mixer.lin[0] << 3;
		}
		if (sb.ess_type != EssType::None) {
			return read_ess_volume(sb.mixer.cda);
		}
		ret = 0xa;
		break;

	case 0x39: // Line-in Volume Right (SB16)
		if (sb.type == SbType::SB16) {
			return sb.mixer.lin[1] << 3;
		}
		ret = 0xa;
		break;

	case 0x3a: // Mic Volume (SB16)
		if (sb.type == SbType::SB16) {
			return sb.mixer.mic << 3;
		}
		ret = 0xa;
		break;

	case 0x3e: // Line Volume (ESS)
		if (sb.ess_type != EssType::None) {
			return read_ess_volume(sb.mixer.lin);
		}
		break;

	case 0x40: // ESS Identification Value (ES1488 and later)
		switch (sb.ess_type) {
		case EssType::None:
		case EssType::Es1688:
			ret = sb.mixer.ess_id_str[sb.mixer.ess_id_str_pos];
			++sb.mixer.ess_id_str_pos;
			if (sb.mixer.ess_id_str_pos >= 4) {
				sb.mixer.ess_id_str_pos = 0;
			}
			break;

		default:
			ret = 0xa;
			LOG_WARNING("ESS: Identification function 0x%x is not implemented",
						sb.mixer.index);
		}
		break;

	case 0x80: // IRQ Select
		ret = 0;
		switch (sb.hw.irq) {
		case 2: return 0x1;
		case 5: return 0x2;
		case 7: return 0x4;
		case 10: return 0x8;
		}
		break;

	case 0x81: // DMA Select
		ret = 0;
		switch (sb.hw.dma8) {
		case 0: ret |= 0x1; break;
		case 1: ret |= 0x2; break;
		case 3: ret |= 0x8; break;
		}
		switch (sb.hw.dma16) {
		case 5: ret |= 0x20; break;
		case 6: ret |= 0x40; break;
		case 7: ret |= 0x80; break;
		}
		return ret;

	case 0x82: // IRQ Status
		return (sb.irq.pending_8bit ? 0x1 : 0) |
		       (sb.irq.pending_16bit ? 0x2 : 0) |
		       ((sb.type == SbType::SB16) ? 0x20 : 0);

	default:
		if (((sb.type == SbType::SBPro1 || sb.type == SbType::SBPro2) &&
		     sb.mixer.index == 0x0c) || // Input control on SBPro
		    (sb.type == SbType::SB16 && sb.mixer.index >= 0x3b &&
		     sb.mixer.index <= 0x47)) { // New SB16 registers
			ret = sb.mixer.unhandled[sb.mixer.index];
		} else {
			ret = 0xa;
		}
		LOG(LOG_SB, LOG_WARN)
		("MIXER:Read from unhandled index %X", sb.mixer.index);
	}
	return ret;
}

// Called by DspWriteStatus to check if the write buffer is at capacity.
static bool write_buffer_at_capacity()
{
	// Is the DSP in an abnormal state and therefore we should consider the
	// buffer at capacity and unable to receive data?
	if (sb.dsp.state != DspState::Normal) {
		return true;
	}

	// Report the buffer as having some room every 8th call, as the buffer
	// will have run down by some amount after sequential calls.
	if ((++sb.dsp.write_status_counter % 8) == 0) {
		return false;
	}

	// If DMA isn't running then the buffer's definitely not at capacity.
	if (sb.dma.mode == DmaMode::None) {
		return false;
	}

	// Finally, the DMA buffer is considered full until it's able to accept
	// a full (64 KB) write; which is once we've hit the minimum threshold.
	//
	// Notes:
	// 86Box and DOSBox-X both calculate the playback rate of the current
	// DMA transfer and then steadily draining down that time until it nears
	// completion. One of the nuances is that games can change the playback
	// rate as well as switch from mono to stereo; so the drain down rate
	// can vary: and indeed, 86Box does this extra bookkeeping.
	//
	// In our case, these rate changes are already taken care of by the
	// existing code, and it just happens that this threshold (dma.left vs
	// dma.min) will happen sooner if the rates are faster.
	//
	return (sb.dma.left > sb.dma.min);
}

// Sound Blaster DSP status byte
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Ref: http://archive.gamedev.net/archive/reference/articles/article443.html
//
// Read from 02x0Ch (DSP Write Buffer Status): Bit 7, when 0, indicates that the
// DSP is ready to receive data through the DSP Write Data or Command port
// (02x0Ch write).
//
// Read from 02x0Eh (DSP Data Available Status): Bit 7, when 1, indicates that
// the DSP has pending data to be read through the DSP Read Data port (02x0Ah
// read).
//
union BufferStatus {
	// Default to all bits high
	uint8_t data = 0b1111'1111;

	// Unused bits. They appear to be set to 1 when reading the status
	// register.
	bit_view<0, 7> reserved;

	// when 1, the buffer has data and is ready to send
	// when 0, the buffer doesn't have data and is ready to receive
	bit_view<7, 1> has_data;
};

static uint8_t read_sb(const io_port_t port, const io_width_t)
{
	switch (port - sb.hw.base) {
	case MixerIndex: return sb.mixer.index;

	case MixerData: return ctmixer_read();

	case DspReadData: return dsp_read_data();

	case DspReadStatus: {
		// TODO See for high speed dma :)
		if (sb.irq.pending_8bit) {
			sb.irq.pending_8bit = false;
			PIC_DeActivateIRQ(sb.hw.irq);
		}
		BufferStatus read_status = {};
		read_status.has_data     = (sb.dsp.out.used != 0);
		return read_status.data;
	}

	case DspAck16Bit: sb.irq.pending_16bit = false; break;

	case DspWriteStatus: {
		BufferStatus write_status = {};
		write_status.has_data     = write_buffer_at_capacity();
		return write_status.data;
	}

	case DspReset: return 0xff;

	default:
		LOG(LOG_SB, LOG_NORMAL)
		("Unhandled read from SB Port %4X", port);
		break;
	}
	return 0xff;
}

static void write_sb(const io_port_t port, const io_val_t value, const io_width_t)
{
	const auto val = check_cast<uint8_t>(value);

	switch (port - sb.hw.base) {
	case DspReset: dsp_do_reset(val); break;

	case DspWriteData: dsp_do_write(val); break;

	case MixerIndex: sb.mixer.index = val; break;

	case MixerData: ctmixer_write(val); break;

	default:
		LOG(LOG_SB, LOG_NORMAL)("Unhandled write to SB Port %4X", port);
		break;
	}
}

bool SB_GetAddress(uint16_t &sbaddr, uint8_t &sbirq, uint8_t &sbdma)
{
	sbaddr = 0;
	sbirq  = 0;
	sbdma  = 0;

	if (sb.type != SbType::None) {
		sbaddr = sb.hw.base;
		sbirq  = sb.hw.irq;
		sbdma  = sb.hw.dma8;
	}

	return (sbaddr != 0 && sbirq != 0 && sbdma != 0);
}

static void sblaster_mixer_callback([[maybe_unused]]const int requested_frames)
{
	// We can ignore requested frames as this function gets called in a loop until it gets what it needs
	// Overflow is not a concern as extra frames will remain in the channel's buffer and get mixed on the next callback

	const auto frames = soundblaster_mixer_queue.Dequeue();
	if (!frames) {
		// Queue must be stopped, otherwise Dequeue() will block until some frames are available
		sb.chan->AddSilence();
		return;
	}
	// Weird double de-referencing syntax as the object type is std::optional<std::unique_ptr<AudioVector>>
	(*frames)->AddSamples(sb.chan.get());
}

static void generate_frames(const int frames_requested)
{
	static int ticks_of_silence = 0;

	switch (sb.mode) {
	case DspMode::None:
	case DspMode::DmaPause:
	case DspMode::DmaMasked:
	if (!soundblaster_mixer_queue.IsRunning()) {
		// We've been playing silence and are continuing to play silence
		// Nothing to do except increment this variable so we don't loop forever
		frames_added_this_tick += frames_requested;
		break;
	}
	// Enqueue a tick's worth of silenced frames
	// Some games (Tyrian for example) will switch to DmaMasked briefly (less than 5 ticks usually)
	// We can't use AddSilence on the mixer thread as that asks for a blocksize of audio (usually over 10ms)
	enqueue_frames(std::make_unique<AudioVectorM8S>(
		frames_requested,
		std::vector<int8_t>(frames_requested).data()
	));

	++ticks_of_silence;
	if (ticks_of_silence > 5000) {
		// We've been playing silence for 5 seconds
		// Some games only play DMA sound in certain segments or for small durations
		// Either we're not in a DMA game at all or its been quiet for a while
		// Stop the mixer channel for performance reasons as this channel blocks waiting for the main thread to provide more audio
		// 5 seconds is safe to avoid stuttering. Be careful if modifying this value as we're about to clear pending audio.

		// This is different from the "sb.speaker_enable = false" state
		// If the speaker is off, this function never gets called
		// This is speaker on but playing silence
		soundblaster_mixer_queue.Stop();
		soundblaster_mixer_queue.Clear();
		sb.chan->Enable(false);
	}
	break;

	case DspMode::Dac:
	// No-op if channel is already running
	soundblaster_mixer_queue.Start();
	sb.chan->Enable(true);
	ticks_of_silence = 0;

	if (sb.dac.used > 0) {
		enqueue_frames(std::make_unique<AudioVectorStretched>(sb.dac.used, sb.dac.data));
		// Reverse the addition done by enqueue_frames()
		// This works everywhere but here because of stretched frames
		frames_added_this_tick -= sb.dac.used;
		frames_added_this_tick += frames_requested;
		sb.dac.used = 0;
	} else {
		sb.mode = DspMode::None;
	}
	break;

	case DspMode::Dma:
	{
		// No-op if channel is already running
		soundblaster_mixer_queue.Start();
		sb.chan->Enable(true);
		ticks_of_silence = 0;

		auto len = check_cast<uint32_t>(frames_requested);
		len *= sb.dma.mul;
		if (len & SbShiftMask) {
			len += 1 << SbShift;
		}

		len >>= SbShift;
		if (len > sb.dma.left) {
			len = sb.dma.left;
		}

		ProcessDMATransfer(len);
		break;
	}
	}
}

static void sblaster_pic_callback()
{
	if (!sb.speaker_enabled) {
		// These are all no-ops if we're already stopped
		soundblaster_mixer_queue.Stop();
		soundblaster_mixer_queue.Clear();
		sb.chan->Enable(false);
		return;
	}

	static float frame_counter = 0.0f;

	frame_counter += sb.chan->GetFramesPerTick();
	const int total_frames = ifloor(frame_counter);
	frame_counter -= static_cast<float>(total_frames);

	while (frames_added_this_tick < total_frames) {
		generate_frames(total_frames - frames_added_this_tick);
	}

	frames_added_this_tick -= total_frames;
}

static SbType determine_sb_type(const std::string& pref)
{
	if (pref == "gb") {
		return SbType::GameBlaster;

	} else if (pref == "sb1") {
		return SbType::SB1;

	} else if (pref == "sb2") {
		return SbType::SB2;

	} else if (pref == "sbpro1") {
		return SbType::SBPro1;

	} else if (pref == "sbpro2" || pref == "ess") {
		return SbType::SBPro2;

	} else if (pref == "sb16") {
		// Invalid settings result in defaulting to 'sb16'
		return SbType::SB16;
	}

	// "falsey" setting ("off", "none", "false", etc.)
	return SbType::None;
}

static EssType determine_ess_type(const std::string& pref)
{
	return (pref == "ess") ? EssType::Es1688 : EssType::None;
}

static OplMode determine_oplmode(const std::string& pref, const SbType sb_type,
                                 const EssType ess_type)
{
	if (ess_type == EssType::None) {
		if (pref == "cms") {
			// Skip for backward compatibility with existing configurations
			return OplMode::None;

		} else if (pref == "opl2") {
			return OplMode::Opl2;

		} else if (pref == "dualopl2") {
			return OplMode::DualOpl2;

		} else if (pref == "opl3") {
			return OplMode::Opl3;

		} else if (pref == "opl3gold") {
			return OplMode::Opl3Gold;

		} else if (pref == "esfm") {
			return OplMode::Esfm;

		} else if (pref == "auto") {
			// Invalid settings result in defaulting to 'auto'
			switch (sb_type) {
			case SbType::GameBlaster: return OplMode::None;
			case SbType::SB1: return OplMode::Opl2;
			case SbType::SB2: return OplMode::Opl2;
			case SbType::SBPro1: return OplMode::DualOpl2;
			case SbType::SBPro2:
				return ess_type == EssType::None ? OplMode::Opl3
												 : OplMode::Esfm;
			case SbType::SB16: return OplMode::Opl3;
			case SbType::None: return OplMode::None;
			}
		}
		// "falsey" setting ("off", "none", "false", etc.)
		return OplMode::None;

	} else { // ESS
		if (pref == "esfm" || pref == "auto") {
			return OplMode::Esfm;

		} else {
			LOG_WARNING(
			        "OPL: Invalid 'oplmode' setting for the ESS card: '%s', "
			        "using 'auto'", pref.c_str());

			auto* sect_updater = static_cast<Section_prop*>(
			        control->GetSection("sblaster"));
			sect_updater->Get_prop("oplmode")->SetValue("auto");

			return OplMode::Esfm;
		}
	}
}

static bool is_cms_enabled(const SbType sbtype)
{
	const auto* sect = static_cast<Section_prop*>(control->GetSection("sblaster"));
	assert(sect);

	const bool cms_enabled = [sect, sbtype]() {
		// Backward compatibility with existing configurations
		if (sect->Get_string("oplmode") == "cms") {
			LOG_WARNING(
			        "%s: The 'cms' setting for 'oplmode' is deprecated; "
			        "use 'cms = on' instead.",
			        sb_log_prefix());
			return true;
		} else {
			const auto cms_str = sect->Get_string("cms");
			const auto cms_enabled_opt = parse_bool_setting(cms_str);
			if (cms_enabled_opt) {
				return *cms_enabled_opt;
			} else if (cms_str == "auto") {
				return sbtype == SbType::SB1 || sbtype == SbType::GameBlaster;
			}
			return false;
		}
	}();

	switch (sbtype) {
	case SbType::SB2: // CMS is optional for Sound Blaster 1 and 2
	case SbType::SB1: return cms_enabled;
	case SbType::GameBlaster:
		if (!cms_enabled) {
			LOG_WARNING(
			        "%s: 'cms' setting is 'off', but is forced to 'auto' "
			        "on the Game Blaster.",
			        sb_log_prefix());
			auto* sect_updater = static_cast<Section_prop*>(
			        control->GetSection("sblaster"));
			sect_updater->Get_prop("cms")->SetValue("auto");
		}
		return true; // Game Blaster is CMS
	default:
		if (cms_enabled) {
			LOG_WARNING(
			        "%s: 'cms' setting 'on' not supported on this card, "
			        "using 'auto'.",
			        sb_log_prefix());

			auto* sect_updater = static_cast<Section_prop*>(
			        control->GetSection("sblaster"));

			sect_updater->Get_prop("cms")->SetValue("auto");
		}
		return false;
	}

	return false;
}

void shutdown_sblaster(Section*);

class SBLASTER final {
private:
	// Data
	IO_ReadHandleObject read_handlers[0x10]   = {};
	IO_WriteHandleObject write_handlers[0x10] = {};

	static constexpr auto BlasterEnvVar = "BLASTER";

	OplMode oplmode = OplMode::None;
	bool cms        = false;

	void SetupEnvironment()
	{
		// Ensure our port and addresses will fit in our format widths.
		// The config selection controls their actual values, so this is
		// a maximum-limit.
		assert(sb.hw.base < 0xfff);
		assert(sb.hw.irq <= 12);
		assert(sb.hw.dma8 < 10);

		char blaster_env_val[] = "AHHH II DD HH TT";

		if (sb.type == SbType::SB16) {
			assert(sb.hw.dma16 < 10);
			safe_sprintf(blaster_env_val,
			             "A%x I%u D%u H%u T%d",
			             sb.hw.base,
			             sb.hw.irq,
			             sb.hw.dma8,
			             sb.hw.dma16,
			             static_cast<int>(sb.type));
		} else {
			safe_sprintf(blaster_env_val,
			             "A%x I%u D%u T%d",
			             sb.hw.base,
			             sb.hw.irq,
			             sb.hw.dma8,
			             static_cast<int>(sb.type));
		}

		// Update AUTOEXEC.BAT line
		LOG_MSG("%s: Setting '%s' environment variable to '%s'",
		        sb_log_prefix(),
		        BlasterEnvVar,
		        blaster_env_val);

		AUTOEXEC_SetVariable(BlasterEnvVar, blaster_env_val);
	}

	void ClearEnvironment()
	{
		AUTOEXEC_SetVariable(BlasterEnvVar, "");
	}

public:
	SBLASTER(Section* conf)
	{
		assert(conf);

		MIXER_LockMixerThread();

		Section_prop* section = static_cast<Section_prop*>(conf);

		sb.hw.base = section->Get_hex("sbbase");
		sb.hw.irq = static_cast<uint8_t>(section->Get_int("irq"));

		sb.dsp.cold_warmup_ms = section->Get_int("sbwarmup");

		// Magic 32 divisor was probably the result of experimentation
		sb.dsp.hot_warmup_ms = sb.dsp.cold_warmup_ms / 32;

		sb.mixer.enabled = section->Get_bool("sbmixer");
		sb.mixer.stereo_enabled = false;

		const auto sbtype_pref = section->Get_string("sbtype");

		sb.type     = determine_sb_type(sbtype_pref);
		sb.ess_type = determine_ess_type(sbtype_pref);

		switch (sb.ess_type) {
		case EssType::None: break;
		case EssType::Es1688:
			sb.mixer.ess_id_str[0] = 0x16;
			sb.mixer.ess_id_str[1] = 0x88;
			sb.mixer.ess_id_str[2] = (sb.hw.base >> 8) & 0xff;
			sb.mixer.ess_id_str[3] = sb.hw.base & 0xff;
		}

		oplmode = determine_oplmode(section->Get_string("oplmode"),
		                            sb.type,
		                            sb.ess_type);

		// Init OPL
		switch (oplmode) {
		case OplMode::None:
			write_handlers[0].Install(Port::AdLib::Command,
			                          GUS_MirrorAdLibCommandPortWrite,
			                          io_width_t::byte);
			break;

		case OplMode::Opl2:
		case OplMode::DualOpl2:
		case OplMode::Opl3:
		case OplMode::Opl3Gold:
		case OplMode::Esfm: {
			OPL_Init(section, oplmode);
			auto opl_channel = MIXER_FindChannel(ChannelName::Opl);
			assert(opl_channel);

			const std::string opl_filter_str = section->Get_string(
			        "opl_filter");
			configure_opl_filter(opl_channel, opl_filter_str, sb.type);
		} break;
		}

		cms = is_cms_enabled(sb.type);
		if (cms) {
			CMS_Init(section);
		}

		// The CMS/Adlib (sbtype=none) and GameBlaster don't have DACs
		const auto has_dac = (sb.type != SbType::None &&
		                      sb.type != SbType::GameBlaster);

		sb.hw.dma8 = has_dac ? static_cast<uint8_t>(section->Get_int("dma"))
		                     : 0;

		// Configure the BIOS DAC callbacks as soon as the card's access
		// ports ( port, IRQ, and potential 8-bit DMA address) are
		// defined.
		//
		if (BIOS_ConfigureTandyDacCallbacks()) {
			// Disable the hot warmup when the SB is being used as
			// the Tandy's DAC because the BIOS toggles the SB's
			// speaker on and off rapidly per-audio-sequence,
			// resulting in "edge-to-edge" samples.
			//
			sb.dsp.hot_warmup_ms = 0;
		}

		if (!has_dac) {
			MIXER_UnlockMixerThread();
			return;
		}

		// The code below here sets up the DAC and DMA channels on all
		// "sbtype = sb*" Sound Blaster type cards.
		//
		auto dma_channel = DMA_GetChannel(sb.hw.dma8);
		assert(dma_channel);
		dma_channel->ReserveFor(sb_log_prefix(), shutdown_sblaster);

		// Only Sound Blaster 16 uses a 16-bit DMA channel.
		if (sb.type == SbType::SB16) {
			sb.hw.dma16 = static_cast<uint8_t>(section->Get_int("hdma"));

			// Reserve the second DMA channel only if it's unique.
			if (sb.hw.dma16 != sb.hw.dma8) {
				dma_channel = DMA_GetChannel(sb.hw.dma16);
				assert(dma_channel);
				dma_channel->ReserveFor(sb_log_prefix(),
				                        shutdown_sblaster);
			}
		}

		std::set channel_features = {ChannelFeature::ReverbSend,
		                             ChannelFeature::ChorusSend,
		                             ChannelFeature::DigitalAudio};

		if (sb.type == SbType::SBPro1 || sb.type == SbType::SBPro2 ||
		    sb.type == SbType::SB16) {
			channel_features.insert(ChannelFeature::Stereo);
		}

		sb.chan = MIXER_AddChannel(sblaster_mixer_callback,
		                           DefaultPlaybackRateHz,
		                           ChannelName::SoundBlasterDac,
		                           channel_features);

		const std::string sb_filter_prefs = section->Get_string("sb_filter");

		const auto sb_filter_always_on = section->Get_bool(
		        "sb_filter_always_on");

		configure_sb_filter(sb.chan,
		                    sb_filter_prefs,
		                    sb_filter_always_on,
		                    sb.type);

		sb.dsp.state       = DspState::Normal;
		sb.dsp.out.lastval = 0xaa;
		sb.dma.chan        = nullptr;

		for (uint8_t i = 4; i <= 0xf; ++i) {
			if (i == 8 || i == 9) {
				continue;
			}
			// Disable mixer ports for lower soundblaster
			if ((sb.type == SbType::SB1 || sb.type == SbType::SB2) &&
			    (i == 4 || i == 5)) {
				continue;
			}
			read_handlers[i].Install(sb.hw.base + i,
			                         read_sb,
			                         io_width_t::byte);

			write_handlers[i].Install(sb.hw.base + i,
			                          write_sb,
			                          io_width_t::byte);
		}
		for (uint16_t i = 0; i < 256; ++i) {
			asp_regs[i] = 0;
		}
		asp_regs[5] = 0x01;
		asp_regs[9] = 0xf8;

		dsp_reset();

		ctmixer_reset();

		ProcessDMATransfer = &play_dma_transfer;

		SetupEnvironment();

		// Sound Blaster MIDI interface
		if (!MIDI_Available()) {
			sb.midi_enabled = false;
		} else {
			sb.midi_enabled = true;
		}

		if (sb.type == SbType::SB16) {
			LOG_MSG("%s: Running on port %xh, IRQ %d, DMA %d, and high DMA %d",
			        sb_log_prefix(),
			        sb.hw.base,
			        sb.hw.irq,
			        sb.hw.dma8,
			        sb.hw.dma16);
		} else {
			LOG_MSG("%s: Running on port %xh, IRQ %d, and DMA %d",
			        sb_log_prefix(),
			        sb.hw.base,
			        sb.hw.irq,
			        sb.hw.dma8);
		}
		TIMER_AddTickHandler(sblaster_pic_callback);
		MIXER_UnlockMixerThread();
	}

	~SBLASTER()
	{
		MIXER_LockMixerThread();
		TIMER_DelTickHandler(sblaster_pic_callback);

		// Prevent discovery of the Sound Blaster via the environment
		ClearEnvironment();

		// Shutdown any FM Synth devices
		if (oplmode != OplMode::None) {
			OPL_ShutDown();
		}
		if (cms) {
			CMS_ShutDown();
		}
		if (sb.type == SbType::None || sb.type == SbType::GameBlaster) {
			MIXER_UnlockMixerThread();
			return;
		}

		LOG_MSG("%s: Shutting down", sb_log_prefix());

		// Stop playback
		if (sb.chan) {
			soundblaster_mixer_queue.Stop();
			sb.chan->Enable(false);
		}
		// Stop the game from accessing the IO ports
		for (auto& rh : read_handlers) {
			rh.Uninstall();
		}
		for (auto& wh : write_handlers) {
			wh.Uninstall();
		}
		dsp_reset(); // Stop everything
		sb.dsp.reset_tally = 0;

		// Deregister the mixer channel and remove it
		assert(sb.chan);
		MIXER_DeregisterChannel(sb.chan);
		sb.chan.reset();

		// Reset the DMA channels as the mixer is no longer reading samples
		DMA_ResetChannel(sb.hw.dma8);
		if (sb.type == SbType::SB16) {
			DMA_ResetChannel(sb.hw.dma16);
		}

		sb = {};
		MIXER_UnlockMixerThread();
	}

}; // End of SBLASTER class

void init_sblaster_dosbox_settings(Section_prop& secprop)
{
	constexpr auto when_idle = Property::Changeable::WhenIdle;

	auto pstring = secprop.Add_string("sbtype", when_idle, "sb16");
	pstring->Set_values(
	        {"gb", "sb1", "sb2", "sbpro1", "sbpro2", "sb16", "ess", "none"});
	pstring->Set_help(
	        "Sound Blaster model to emulate ('sb16' by default).\n"
	        "The models auto-selected with 'oplmode' and 'cms' on 'auto' are also listed.\n"
	        "  gb:        Game Blaster          - CMS\n"
	        "  sb1:       Sound Blaster 1.0     - OPL2, CMS\n"
	        "  sb2:       Sound Blaster 2.0     - OPL2\n"
	        "  sbpro1:    Sound Blaster Pro     - Dual OPL2\n"
	        "  sbpro2:    Sound Blaster Pro 2   - OPL3\n"
	        "  sb16:      Sound Blaster 16      - OPL3 (default)\n"
	        "  ess:       ESS ES1688 AudioDrive - ESFM\n"
	        "  none/off:  Disable Sound Blaster emulation.\n"
	        "Notes:\n"
	        "  - Creative Music System was later rebranded to Game Blaster; they are the\n"
	        "    same card.\n"
	        "  - The 'ess' option is for getting ESS Enhanced FM music via the card's ESFM\n"
	        "    synthesiser in games that support it. The ESS DAC is not emulated but the\n"
	        "    card is Sound Blaster Pro compatible; just configure the game for Sound\n"
	        "    Blaster digital sound.");

	auto phex = secprop.Add_hex("sbbase", when_idle, 0x220);
	phex->Set_values({"220", "240", "260", "280", "2a0", "2c0", "2e0", "300"});
	phex->Set_help("The IO address of the Sound Blaster (220 by default).");

	auto pint = secprop.Add_int("irq", when_idle, 7);
	pint->Set_values({"3", "5", "7", "9", "10", "11", "12"});
	pint->Set_help("The IRQ number of the Sound Blaster (7 by default).");

	pint = secprop.Add_int("dma", when_idle, 1);
	pint->Set_values({"0", "1", "3", "5", "6", "7"});
	pint->Set_help("The DMA channel of the Sound Blaster (1 by default).");

	pint = secprop.Add_int("hdma", when_idle, 5);
	pint->Set_values({"0", "1", "3", "5", "6", "7"});
	pint->Set_help("The High DMA channel of the Sound Blaster 16 (5 by default).");

	auto pbool = secprop.Add_bool("sbmixer", when_idle, true);
	pbool->Set_help(
	        "Allow the Sound Blaster mixer to modify volume levels (enabled by default).\n"
	        "Sound Blaster Pro 1 and later cards allow programs to set the volume of the\n"
	        "digital audio (DAC), FM synth, and CD Audio output. These correspond to the\n"
	        "SB, OPL, and CDAUDIO DOSBox mixer channels, respectively.\n"
	        "  on:   The final level of the above channels is a combination of the volume\n"
	        "        set by the program, and the volume set in the DOSBox mixer.\n"
	        "  off:  Only the DOSBox mixer determines the volume of these channels.\n"
	        "Note: Some games change the volume levels dynamically (e.g., lower the FM music\n"
	        "      volume when speech is playing); it's best to leave 'sbmixer' enabled for\n"
	        "      such games.");

	pint = secprop.Add_int("sbwarmup", when_idle, 100);
	pint->Set_help(
	        "Silence initial DMA audio after card power-on, in milliseconds\n"
	        "(100 by default). This mitigates pops heard when starting many SB-based games.\n"
	        "Reduce this if you notice intial playback is missing audio.");
	pint->SetMinMax(0, 100);

	pstring = secprop.Add_string("sb_filter", when_idle, "modern");
	pstring->Set_help(
	        "Type of filter to emulate for the Sound Blaster digital sound output:\n"
	        "  auto:      Use the appropriate filter determined by 'sbtype'.\n"
	        "  sb1, sb2, sbpro1, sbpro2, sb16:\n"
	        "             Use the filter of this Sound Blaster model.\n"
	        "  modern:    Use linear interpolation upsampling that acts as a low-pass\n"
	        "             filter; this is the legacy DOSBox behaviour (default).\n"
	        "  off:       Don't filter the output.\n"
	        "  <custom>:  One or two custom filters in the following format:\n"
	        "               TYPE ORDER FREQ\n"
	        "             Where TYPE can be 'hpf' (high-pass) or 'lpf' (low-pass),\n"
	        "             ORDER is the order of the filter from 1 to 16\n"
	        "             (1st order = 6dB/oct slope, 2nd order = 12dB/oct, etc.),\n"
	        "             and FREQ is the cutoff frequency in Hz. Examples:\n"
	        "                lpf 2 12000\n"
	        "                hpf 3 120 lfp 1 6500");

	pbool = secprop.Add_bool("sb_filter_always_on", when_idle, false);
	pbool->Set_help(
	        "Force the Sound Blaster Pro 2 filter to be always on (disabled by default).\n"
	        "Other Sound Blaster models don't allow toggling the filter in software.");
}

static std::unique_ptr<SBLASTER> sblaster = {};

void init_sblaster(Section* sec)
{
	assert(sec);

	sblaster = std::make_unique<SBLASTER>(sec);

	constexpr auto ChangeableAtRuntime = true;
	sec->AddDestroyFunction(&shutdown_sblaster, ChangeableAtRuntime);
}

void shutdown_sblaster(Section* /*sec*/) {
	sblaster = {};
}

void SB_AddConfigSection(const ConfigPtr& conf)
{
	constexpr auto changeable_at_runtime = true;

	assert(conf);
	Section_prop* secprop = conf->AddSection_prop("sblaster",
	                                              &init_sblaster,
	                                              changeable_at_runtime);
	assert(secprop);
	init_sblaster_dosbox_settings(*secprop);
}
