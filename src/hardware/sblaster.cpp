/*
 *  Copyright (C) 2019-2023  The DOSBox Staging Team
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

#include "autoexec.h"
#include "control.h"
#include "dma.h"
#include "hardware.h"
#include "inout.h"
#include "math_utils.h"
#include "midi.h"
#include "mixer.h"
#include "pic.h"
#include "setup.h"
#include "shell.h"
#include "string_utils.h"
#include "support.h"

constexpr uint8_t MIXER_INDEX = 0x04;
constexpr uint8_t MIXER_DATA = 0x05;

constexpr uint8_t DSP_RESET = 0x06;
constexpr uint8_t DSP_READ_DATA = 0x0A;
constexpr uint8_t DSP_WRITE_DATA = 0x0C;
constexpr uint8_t DSP_WRITE_STATUS = 0x0C;
constexpr uint8_t DSP_READ_STATUS = 0x0E;
constexpr uint8_t DSP_ACK_16BIT = 0x0f;

constexpr uint8_t DSP_NO_COMMAND = 0;

constexpr uint16_t DMA_BUFSIZE = 1024;
constexpr uint8_t DSP_BUFSIZE = 64;
constexpr uint16_t DSP_DACSIZE = 512;

constexpr uint8_t SB_SH = 14;
constexpr uint16_t SB_SH_MASK = ((1 << SB_SH) - 1);

constexpr uint8_t MIN_ADAPTIVE_STEP_SIZE = 0; // max is 32767

// It was common for games perform some initial checks
// and resets on startup, resulting a rapid susccession of resets.
constexpr uint8_t DSP_INITIAL_RESET_LIMIT = 4;

constexpr auto native_dac_rate_hz = 45454;
constexpr uint16_t default_playback_rate_hz = 22050;

enum {DSP_S_RESET,DSP_S_RESET_WAIT,DSP_S_NORMAL,DSP_S_HIGHSPEED};

enum SB_TYPES {
	SBT_NONE = 0,
	SBT_1    = 1,
	SBT_PRO1 = 2,
	SBT_2    = 3,
	SBT_PRO2 = 4,
	SBT_16   = 6,
	SBT_GB   = 7
};

enum class FilterType {
	None,
	SB1,
	SB2,
	SBPro1,
	SBPro2,
	SB16,
	Modern
};

enum SB_IRQS {SB_IRQ_8,SB_IRQ_16,SB_IRQ_MPU};

enum DSP_MODES {
	MODE_NONE,
	MODE_DAC,
	MODE_DMA,
	MODE_DMA_PAUSE,
	MODE_DMA_MASKED

};
 
enum DMA_MODES {
	DSP_DMA_NONE,
	DSP_DMA_2,DSP_DMA_3,DSP_DMA_4,DSP_DMA_8,
	DSP_DMA_16,DSP_DMA_16_ALIASED
};

enum {
	PLAY_MONO,PLAY_STEREO
};

struct SB_INFO {
	uint32_t freq = 0;
	struct {
		bool stereo = false;
		bool sign = false;
		bool autoinit = false;
		DMA_MODES mode = DSP_DMA_NONE;
		uint32_t rate = 0;     // sample rate
		uint32_t mul = 0;      // samples-per-millisecond multipler
		uint32_t singlesize = 0; // size for single cycle transfers
		uint32_t autosize = 0;   // size for auto init transfers
		uint32_t left = 0;     // Left in active cycle
		uint32_t min = 0;
		union {
			uint8_t  b8[DMA_BUFSIZE];
			int16_t b16[DMA_BUFSIZE];
		} buf = {};
		uint32_t bits = 0;
		DmaChannel *chan = nullptr;
		uint32_t remain_size = 0;
	} dma = {};
	bool speaker = false;
	bool midi = false;
	uint8_t time_constant = 0;
	DSP_MODES mode = MODE_NONE;
	SB_TYPES type = SBT_NONE;
	FilterType sb_filter_type = FilterType::None;
	FilterType opl_filter_type = FilterType::None;
	FilterState sb_filter_state = FilterState::Off;
	struct {
		bool pending_8bit;
		bool pending_16bit;
	} irq = {};
	struct {
		uint8_t state = 0;
		uint8_t cmd = 0;
		uint8_t cmd_len = 0;
		uint8_t cmd_in_pos = 0;
		uint8_t cmd_in[DSP_BUFSIZE] = {};
		struct {
			uint8_t lastval = 0; // last values added to the fifo
			uint8_t data[DSP_BUFSIZE] = {};
			uint8_t pos = 0;  // index of current entry
			uint8_t used = 0; // number of entries in the fifo
		} in = {}, out = {};
		uint8_t test_register = 0;
		uint32_t write_busy = 0;
		uint32_t reset_tally = 0;
		uint8_t cold_warmup_ms = 0;
		uint8_t hot_warmup_ms = 0;
		uint16_t warmup_remaining_ms = 0;
	} dsp = {};
	struct {
		int16_t data[DSP_DACSIZE + 1] = {};
		uint16_t used = 0; // number of entries in the DAC
		int16_t last = 0;   // index of current entry
	} dac = {};
	struct {
		uint8_t index = 0;
		uint8_t dac[2] = {};
		uint8_t fm[2] = {};
		uint8_t cda[2] = {};
		uint8_t master[2] = {};
		uint8_t lin[2] = {};
		uint8_t mic = 0;
		bool stereo = false;
		bool enabled = false;
		bool filtered = false;
		uint8_t unhandled[0x48] = {};
	} mixer = {};
	struct {
		uint8_t reference = 0;
		uint16_t stepsize = 0;
		bool haveref = false;
	} adpcm = {};
	struct {
		uint16_t base = 0;
		uint8_t irq = 0;
		uint8_t dma8 = 0;
		uint8_t dma16 = 0;
	} hw = {};
	struct {
		int value = 0;
		uint32_t count = 0;
	} e2 = {};
	mixer_channel_t chan = nullptr;
};

static SB_INFO sb = {};

// number of bytes in input for commands (sb/sbpro)
static uint8_t DSP_cmd_len_sb[256] = {
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

// number of bytes in input for commands (sb16)
static uint8_t DSP_cmd_len_sb16[256] = {
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

static uint8_t ASP_regs[256];
static bool ASP_init_in_progress = false;

static int E2_incr_table[4][9] = {
  {  0x01, -0x02, -0x04,  0x08, -0x10,  0x20,  0x40, -0x80, -106 },
  { -0x01,  0x02, -0x04,  0x08,  0x10, -0x20,  0x40, -0x80,  165 },
  { -0x01,  0x02,  0x04, -0x08,  0x10, -0x20, -0x40,  0x80, -151 },
  {  0x01, -0x02,  0x04, -0x08, -0x10,  0x20, -0x40,  0x80,   90 }
};

static const char * CardType()
{
	constexpr std::array<const char *, 8> types = {"NONE",   "SB1",
	                                               "SBPRO1", "SB2",
	                                               "SBPRO2", "UNASSIGNED",
	                                               "SB16",   "GB"};
	const size_t type_id = static_cast<size_t>(sb.type);
	assertm(type_id != 5 && type_id < types.size(),
	        "sb.type does not match a known Sound Blaster type ID");
	return types[type_id];
}

static void DSP_ChangeMode(DSP_MODES mode);

static void FlushRemainingDMATransfer();
static void SuppressDMATransfer(uint32_t size);
static void PlayDMATransfer(uint32_t size);

typedef void (*process_dma_f)(uint32_t);
static process_dma_f ProcessDMATransfer;

static void DSP_SetSpeaker(bool requested_state) {
	// Speaker-output is already in the requested state
	if (sb.speaker == requested_state)
		return;

	// If the speaker's being turned on, then flush old
	// content before releasing the channel for playback.
	if (requested_state) {
		PIC_RemoveEvents(SuppressDMATransfer);
		FlushRemainingDMATransfer();
		// Speaker powered-on after cold-state, give it warmup time
		sb.dsp.warmup_remaining_ms = sb.dsp.cold_warmup_ms;
	}
	sb.chan->Enable(requested_state);
	sb.speaker = requested_state;
	LOG_MSG("%s: Speaker-output has been toggled %s",
	        CardType(), requested_state ? "on" : "off");
}

static void InitializeSpeakerState()
{
	// Real SBPro2 hardware starts with the card's speaker-output disabled
	sb.speaker = false;

	// The SB16's output channel starts active however subsequent
	// requests to disable the speaker will be honored (see: SetSpeaker).
	// Also, because the channel is active, we treat this as startup event.
	if (sb.type == SBT_16) {
		const bool is_cold_start = sb.dsp.reset_tally <= DSP_INITIAL_RESET_LIMIT;
		sb.dsp.warmup_remaining_ms = is_cold_start ? sb.dsp.cold_warmup_ms
		                                           : sb.dsp.hot_warmup_ms;
		sb.chan->Enable(true);
	} else {
		sb.chan->Enable(false);
	}
}

static void log_filter_config(const char *output_type, const FilterType filter)
{
	static const std::map<FilterType, std::string> filter_name_map = {
	        {FilterType::SB1, "Sound Blaster 1.0"},
	        {FilterType::SB2, "Sound Blaster 2.0"},
	        {FilterType::SBPro1, "Sound Blaster Pro 1"},
	        {FilterType::SBPro2, "Sound Blaster Pro 2"},
	        {FilterType::SB16, "Sound Blaster 16"},
	        {FilterType::Modern, "Modern"},
	};

	if (filter == FilterType::None) {
		LOG_MSG("%s: %s filter disabled", CardType(), output_type);
	} else {
		auto it = filter_name_map.find(filter);
		if (it != filter_name_map.end()) {
			auto filter_type = it->second;
			LOG_MSG("%s: %s %s output filter enabled",
			        CardType(),
			        filter_type.c_str(),
			        output_type);
		}
	}
}

struct FilterConfig {
	FilterState hpf_state       = FilterState::Off;
	uint8_t hpf_order           = {};
	uint16_t hpf_cutoff_freq_hz = {};

	FilterState lpf_state       = FilterState::Off;
	uint8_t lpf_order           = {};
	uint16_t lpf_cutoff_freq_hz = {};

	ResampleMethod resample_method = {};
	uint16_t zoh_rate_hz           = {};
};

static void set_filter(mixer_channel_t channel, const FilterConfig &config)
{
	if (config.hpf_state == FilterState::On ||
	    config.hpf_state == FilterState::ForcedOn) {
		channel->ConfigureHighPassFilter(config.hpf_order,
		                                 config.hpf_cutoff_freq_hz);
	}
	channel->SetHighPassFilter(config.hpf_state);

	if (config.lpf_state == FilterState::On ||
	    config.lpf_state == FilterState::ForcedOn) {
		channel->ConfigureLowPassFilter(config.lpf_order,
		                                config.lpf_cutoff_freq_hz);
	}
	channel->SetLowPassFilter(config.lpf_state);

	if (config.resample_method == ResampleMethod::ZeroOrderHoldAndResample)
		channel->SetZeroOrderHoldUpsamplerTargetFreq(config.zoh_rate_hz);

	channel->SetResampleMethod(config.resample_method);
}

static std::optional<FilterType> determine_filter_type(const std::string &filter_choice,
                                                       const SB_TYPES sb_type)
{
	if (filter_choice == "auto") {
		switch (sb_type) {
		case SBT_NONE: return FilterType::None;   break;
		case SBT_1:    return FilterType::SB1;    break;
		case SBT_2:    return FilterType::SB2;    break;
		case SBT_PRO1: return FilterType::SBPro1; break;
		case SBT_PRO2: return FilterType::SBPro2; break;
		case SBT_16:   return FilterType::SB16;   break;
		case SBT_GB:   return FilterType::None;   break;
		}
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

static void configure_sb_filter(mixer_channel_t channel,
                                const std::string &filter_prefs,
                                const bool filter_always_on, const SB_TYPES sb_type)
{
	// A bit unfortunate, but we need to enable the ZOH upsampler and the
	// correct upsample rate first for the filter cutoff frequency
	// validation to work correctly.
	channel->SetZeroOrderHoldUpsamplerTargetFreq(native_dac_rate_hz);
	channel->SetResampleMethod(ResampleMethod::ZeroOrderHoldAndResample);

	if (channel->TryParseAndSetCustomFilter(filter_prefs))
		return;

	const auto filter_prefs_parts = split(filter_prefs);

	const auto filter_choice = filter_prefs_parts.empty()
	                                 ? "auto"
	                                 : filter_prefs_parts[0];

	FilterConfig config = {};

	auto enable_lpf = [&](const uint8_t order, const uint16_t cutoff_freq_hz) {
		config.lpf_state = filter_always_on ? FilterState::ForcedOn
		                                    : FilterState::On;

		config.lpf_order          = order;
		config.lpf_cutoff_freq_hz = cutoff_freq_hz;
	};

	auto enable_zoh_upsampler = [&] {
		config.resample_method = ResampleMethod::ZeroOrderHoldAndResample;
		config.zoh_rate_hz     = native_dac_rate_hz;
	};

	const auto filter_type = determine_filter_type(filter_choice, sb_type);

	if (!filter_type) {
		LOG_WARNING("%s: Invalid 'sb_filter' value: '%s', using 'off'",
		            CardType(),
		            filter_choice.c_str());

		channel->SetHighPassFilter(FilterState::Off);
		channel->SetLowPassFilter(FilterState::Off);
		return;
	}

	switch (*filter_type) {
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
		config.resample_method = ResampleMethod::LinearInterpolation;
		break;
	}

	log_filter_config("DAC", *filter_type);
	set_filter(channel, config);
}

static void configure_opl_filter(mixer_channel_t channel,
                                 const std::string &filter_prefs,
                                 const SB_TYPES sb_type)
{
	assert(channel);
	if (channel->TryParseAndSetCustomFilter(filter_prefs))
		return;

	const auto filter_prefs_parts = split(filter_prefs);

	const auto filter_choice = filter_prefs_parts.empty()
	                                 ? "auto"
	                                 : filter_prefs_parts[0];

	FilterConfig config    = {};
	config.resample_method = ResampleMethod::Resample;

	auto enable_lpf = [&](const uint8_t order, const uint16_t cutoff_freq_hz) {
		config.lpf_state          = FilterState::On;
		config.lpf_order          = order;
		config.lpf_cutoff_freq_hz = cutoff_freq_hz;
	};

	const auto filter_type = determine_filter_type(filter_choice, sb_type);

	if (!filter_type) {
		if (filter_choice != "off")
			LOG_WARNING("%s: Invalid 'opl_filter' value: '%s', using 'off'",
			            CardType(),
			            filter_choice.c_str());

		channel->SetHighPassFilter(FilterState::Off);
		channel->SetLowPassFilter(FilterState::Off);
		return;
	}

	// The filter parameters have been tweaked by analysing real hardware
	// recordings. The results are virtually indistinguishable from the real
	// thing by ear only.
	switch (*filter_type) {
	case FilterType::None:
	case FilterType::SB16:
	case FilterType::Modern: break;

	case FilterType::SB1:
	case FilterType::SB2: enable_lpf(1, 12000); break;

	case FilterType::SBPro1:
	case FilterType::SBPro2: enable_lpf(1, 8000); break;
	}

	log_filter_config("OPL", *filter_type);
	set_filter(channel, config);
}

static void SB_RaiseIRQ(SB_IRQS type)
{
	LOG(LOG_SB, LOG_NORMAL)("Raising IRQ");
	switch (type) {
	case SB_IRQ_8:
		if (sb.irq.pending_8bit) {
//			LOG_MSG("SB: 8bit irq pending");
			return;
		}
		sb.irq.pending_8bit=true;
		PIC_ActivateIRQ(sb.hw.irq);
		break;
	case SB_IRQ_16:
		if (sb.irq.pending_16bit) {
//			LOG_MSG("SB: 16bit irq pending");
			return;
		}
		sb.irq.pending_16bit=true;
		PIC_ActivateIRQ(sb.hw.irq);
		break;
	default:
		break;
	}
}

static void DSP_FlushData()
{
	sb.dsp.out.used = 0;
	sb.dsp.out.pos = 0;
}

static double last_dma_callback = 0.0;

static void DSP_DMA_CallBack(const DmaChannel* chan, DMAEvent event)
{
	if (chan!=sb.dma.chan || event==DMA_REACHED_TC) return;
	else if (event==DMA_MASKED) {
		if (sb.mode==MODE_DMA) {
			//Catch up to current time, but don't generate an IRQ!
			//Fixes problems with later sci games.
			const auto t = PIC_FullIndex() - last_dma_callback;
			auto s = static_cast<uint32_t>(sb.dma.rate * t / 1000.0);
			if (s > sb.dma.min) {
				LOG(LOG_SB,LOG_NORMAL)("limiting amount masked to sb.dma.min");
				s = sb.dma.min;
			}
			auto min_size = sb.dma.mul >> SB_SH;
			if (!min_size) min_size = 1;
			min_size *= 2;
			if (sb.dma.left > min_size) {
				if (s > (sb.dma.left - min_size))
					s = sb.dma.left - min_size;
				// This will trigger an irq, see
				// PlayDMATransfer, so lets not do that
				if (!sb.dma.autoinit && sb.dma.left <= sb.dma.min)
					s = 0;
				if (s)
					ProcessDMATransfer(s);
			}
			sb.mode = MODE_DMA_MASKED;
//			DSP_ChangeMode(MODE_DMA_MASKED);
			LOG(LOG_SB,LOG_NORMAL)("DMA masked,stopping output, left %d",chan->curr_count);
		}
	} else if (event==DMA_UNMASKED) {
		if (sb.mode==MODE_DMA_MASKED && sb.dma.mode!=DSP_DMA_NONE) {
			DSP_ChangeMode(MODE_DMA);
//			sb.mode=MODE_DMA;
			FlushRemainingDMATransfer();
			LOG(LOG_SB, LOG_NORMAL)
			("DMA unmasked,starting output, auto %d block %d",
			 static_cast<int>(chan->is_autoiniting),
			 chan->base_count);
		}
	}
	else {
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

static std::array<uint8_t, 4> decode_ADPCM_2(const uint8_t data)
{
	// clang-format off

	constexpr int8_t scale_map[] = {
		0,  1,  0,  -1, 1,  3,  -1,  -3,
		2,  6, -2,  -6, 4, 12,  -4, -12,
		8, 24, -8, -24, 6, 48, -16, -48
	};
	constexpr uint8_t adjust_map[] = {
		  0, 4,   0, 4,
		252, 4, 252, 4, 252, 4, 252, 4,
		252, 4, 252, 4, 252, 4, 252, 4,
		252, 0, 252, 0
	};
	static_assert(ARRAY_LEN(scale_map) == ARRAY_LEN(adjust_map));
	constexpr auto last_i = static_cast<uint8_t>(sizeof(scale_map) - 1);;

	return {decode_adpcm_portion((data >> 6) & 0x3, adjust_map, scale_map, last_i),
	        decode_adpcm_portion((data >> 4) & 0x3, adjust_map, scale_map, last_i),
	        decode_adpcm_portion((data >> 2) & 0x3, adjust_map, scale_map, last_i),
	        decode_adpcm_portion((data >> 0) & 0x3, adjust_map, scale_map, last_i)};

	// clang-format on
}

static std::array<uint8_t, 3> decode_ADPCM_3(const uint8_t data)
{
	// clang-format off

	constexpr int8_t scale_map[40] = {
		0,  1,  2,  3,  0,  -1,  -2,  -3,
		1,  3,  5,  7, -1,  -3,  -5,  -7,
		2,  6, 10, 14, -2,  -6, -10, -14,
		4, 12, 20, 28, -4, -12, -20, -28,
		5, 15, 25, 35, -5, -15, -25, -35
	};
	constexpr uint8_t adjust_map[40] = {
		  0, 0, 0, 8,   0, 0, 0, 8,
		248, 0, 0, 8, 248, 0, 0, 8,
		248, 0, 0, 8, 248, 0, 0, 8,
		248, 0, 0, 8, 248, 0, 0, 8,
		248, 0, 0, 0, 248, 0, 0, 0
	};
	static_assert(ARRAY_LEN(scale_map) == ARRAY_LEN(adjust_map));
	constexpr auto last_i = static_cast<uint8_t>(sizeof(scale_map) - 1);;

	return {decode_adpcm_portion((data >> 5) & 0x7, adjust_map, scale_map, last_i),
	        decode_adpcm_portion((data >> 2) & 0x7, adjust_map, scale_map, last_i),
	        decode_adpcm_portion((data & 0x3) << 1, adjust_map, scale_map, last_i)};

	// clang-format on
}

static std::array<uint8_t, 2> decode_ADPCM_4(const uint8_t data)
{
	// clang-format off

	constexpr int8_t scale_map[64] = {
		0,  1,  2,  3,  4,  5,  6,  7,  0,  -1,  -2,  -3,  -4,  -5,  -6,  -7,
		1,  3,  5,  7,  9, 11, 13, 15, -1,  -3,  -5,  -7,  -9, -11, -13, -15,
		2,  6, 10, 14, 18, 22, 26, 30, -2,  -6, -10, -14, -18, -22, -26, -30,
		4, 12, 20, 28, 36, 44, 52, 60, -4, -12, -20, -28, -36, -44, -52, -60
	};
	constexpr uint8_t adjust_map[64] = {
		  0, 0, 0, 0, 0, 16, 16, 16,
		  0, 0, 0, 0, 0, 16, 16, 16,
		240, 0, 0, 0, 0, 16, 16, 16,
		240, 0, 0, 0, 0, 16, 16, 16,
		240, 0, 0, 0, 0, 16, 16, 16,
		240, 0, 0, 0, 0, 16, 16, 16,
		240, 0, 0, 0, 0,  0,  0,  0,
		240, 0, 0, 0, 0,  0,  0,  0
	};
	static_assert(ARRAY_LEN(scale_map) == ARRAY_LEN(adjust_map));
	constexpr auto last_i = static_cast<uint8_t>(sizeof(scale_map) - 1);;

	return {decode_adpcm_portion(data >> 4,  adjust_map, scale_map, last_i),
	        decode_adpcm_portion(data & 0xf, adjust_map, scale_map, last_i)};

	// clang-format on
}

template <typename T>
static const T *maybe_silence(const uint32_t num_samples, const T *buffer)
{
	if (!sb.dsp.warmup_remaining_ms)
		return buffer;

	static std::vector<T> quiet_buffer;
	constexpr auto silent = Mixer_GetSilentDOSSample<T>();
	if (quiet_buffer.size() < num_samples)
		quiet_buffer.resize(num_samples, silent);

	sb.dsp.warmup_remaining_ms--;
	return quiet_buffer.data();
}

static uint32_t ReadDMA8(uint32_t bytes_to_read, uint32_t i = 0) {
	const auto bytes_read = sb.dma.chan->Read(bytes_to_read, sb.dma.buf.b8 + i);
	assert(bytes_read <= DMA_BUFSIZE * sizeof(sb.dma.buf.b8[0]));
	return check_cast<uint32_t>(bytes_read);
}

static uint32_t ReadDMA16(uint32_t bytes_to_read, uint32_t i = 0) {
	const auto unsigned_buf = reinterpret_cast<uint8_t *>(sb.dma.buf.b16 + i);
	const auto bytes_read = sb.dma.chan->Read(bytes_to_read, unsigned_buf);
	assert(bytes_read <= DMA_BUFSIZE * sizeof(sb.dma.buf.b16[0]));
	return check_cast<uint32_t>(bytes_read);
}

static void PlayDMATransfer(uint32_t bytes_requested)
{
	// How many bytes should we read from DMA?
	const auto lower_bound = sb.dma.autoinit ? bytes_requested : sb.dma.min;
	const auto bytes_to_read =  sb.dma.left <= lower_bound ? sb.dma.left : bytes_requested;

	// All three of these must be populated during the DMA sequence to
	// ensure the proper quantities and unit are being accounted for.
	// For example: use the channel count to convert from samples to frames.
	uint32_t bytes_read = 0;
	uint32_t samples = 0;
	uint16_t frames = 0;

	/* In DSP_DMA_16_ALIASED mode temporarily divide by 2 to get number of 16-bit
	samples, because 8-bit DMA Read returns byte size, while in DSP_DMA_16 mode
	16-bit DMA Read returns word size */
	const uint8_t dma16_to_sample_divisor = sb.dma.mode == DSP_DMA_16_ALIASED ? 2 : 1;

	// Used to convert from samples to frames (which is what AddSamples unintuitively uses.. )
	const uint8_t channels = sb.dma.stereo ? 2 : 1;

	last_dma_callback = PIC_FullIndex();

	// Temporary counter for ADPCM modes

	auto decode_adpcm_dma =
	        [&](auto decode_adpcm_fn) -> std::tuple<uint32_t, uint32_t, uint16_t> {

		const uint32_t num_bytes = ReadDMA8(bytes_to_read);
		uint32_t num_samples = 0;
		uint16_t num_frames  = 0;

		// Parse the reference ADPCM byte, if provided
		uint32_t i = 0;
		if (num_bytes > 0 && sb.adpcm.haveref) {
			sb.adpcm.haveref   = false;
			sb.adpcm.reference = sb.dma.buf.b8[0];
			sb.adpcm.stepsize=MIN_ADAPTIVE_STEP_SIZE;
			++i;
		}
		// Decode the remaining DMA buffer into samples using the provided function
		while (i < num_bytes) {
			const auto decoded = decode_adpcm_fn(sb.dma.buf.b8[i]);
			constexpr auto num_decoded = check_cast<uint8_t>(decoded.size());
			sb.chan->AddSamples_m8(num_decoded, maybe_silence(num_decoded, decoded.data()));
			num_samples += num_decoded;
			++i;
		}
		 // ADPCM is mono
		num_frames = check_cast<uint16_t>(num_samples);
		return {num_bytes, num_samples, num_frames};
	};

	//Read the actual data, process it and send it off to the mixer
	switch (sb.dma.mode) {
	case DSP_DMA_2:
		std::tie(bytes_read, samples, frames) = decode_adpcm_dma(decode_ADPCM_2);
		break;
	case DSP_DMA_3:
		std::tie(bytes_read, samples, frames) = decode_adpcm_dma(decode_ADPCM_3);
		break;
	case DSP_DMA_4:
		std::tie(bytes_read, samples, frames) = decode_adpcm_dma(decode_ADPCM_4);
		break;
	case DSP_DMA_8:
 		if (sb.dma.stereo) {
			bytes_read = ReadDMA8(bytes_to_read, sb.dma.remain_size);
			samples = bytes_read + sb.dma.remain_size;
			frames = check_cast<uint16_t>(samples / channels);

			// Only add whole frames when in stereo DMA mode. The
			// number of frames comes from the DMA request, and
			// therefore user-space data.
			if (frames) {
				if (sb.dma.sign) {
					const auto signed_buf = reinterpret_cast<int8_t *>(sb.dma.buf.b8);
					sb.chan->AddSamples_s8s(
					        frames,
					        maybe_silence(samples, signed_buf));
				} else {
					sb.chan->AddSamples_s8(
					        frames,
					        maybe_silence(samples, sb.dma.buf.b8));
				}
			}
			// Otherwise there's an unhandled dangling sample from
			// the last round
			if (samples & 1) {
				sb.dma.remain_size = 1;
				sb.dma.buf.b8[0] = sb.dma.buf.b8[samples - 1];
			} else {
				sb.dma.remain_size = 0;
			}
		} else { // Mono
			bytes_read = ReadDMA8(bytes_to_read);
			samples = bytes_read;
			frames = check_cast<uint16_t>(samples / channels);
			assert(channels == 1 && frames == samples); // sanity-check mono
			if (sb.dma.sign) {
				sb.chan->AddSamples_m8s(frames,
				         maybe_silence(samples,
				                       reinterpret_cast<int8_t *>(sb.dma.buf.b8)));
			} else {
				sb.chan->AddSamples_m8(frames,
				         maybe_silence(samples, sb.dma.buf.b8));
			}
		}
		break;
	case DSP_DMA_16_ALIASED:
		assert(dma16_to_sample_divisor == 2);
		[[fallthrough]];
	case DSP_DMA_16:
		if (sb.dma.stereo) {
			bytes_read = ReadDMA16(bytes_to_read, sb.dma.remain_size);
			samples = (bytes_read + sb.dma.remain_size) / dma16_to_sample_divisor;
			frames = check_cast<uint16_t>(samples / channels);

			// Only add whole frames when in stereo DMA mode
			if (frames) {
#if defined(WORDS_BIGENDIAN)
				if (sb.dma.sign) {
					sb.chan->AddSamples_s16_nonnative(frames,
					            maybe_silence(samples, sb.dma.buf.b16));
				} else {
					sb.chan->AddSamples_s16u_nonnative(frames,
					            maybe_silence(samples, reinterpret_cast<uint16_t *>(sb.dma.buf.b16)));
				}
#else
				if (sb.dma.sign) {
					sb.chan->AddSamples_s16(frames,
					            maybe_silence(samples, sb.dma.buf.b16));
				} else {
					sb.chan->AddSamples_s16u(frames,
					            maybe_silence(samples, reinterpret_cast<uint16_t *>(sb.dma.buf.b16)));
				}
#endif
			}
			if (samples & 1) {
				// Carry over the dangling sample into the next round, or
				sb.dma.remain_size = 1;
				sb.dma.buf.b16[0] = sb.dma.buf.b16[samples - 1];
			}
			 else {
				// The DMA transfer is done
				sb.dma.remain_size = 0;
			}
		} else { // 16-bit mono
			bytes_read = ReadDMA16(bytes_to_read);
			samples = bytes_read / dma16_to_sample_divisor;
			frames = check_cast<uint16_t>(samples / channels);
			assert(channels == 1 && frames == samples); // sanity-check mono
#if defined(WORDS_BIGENDIAN)
			if (sb.dma.sign) {
				sb.chan->AddSamples_m16_nonnative(frames,
				             maybe_silence(samples, sb.dma.buf.b16));
			} else {
				sb.chan->AddSamples_m16u_nonnative(frames,
				             maybe_silence(samples,
				                           reinterpret_cast<uint16_t *>(sb.dma.buf.b16)));
			}
#else
			if (sb.dma.sign) {
				sb.chan->AddSamples_m16(frames,
				             maybe_silence(samples, sb.dma.buf.b16));
			} else {
				sb.chan->AddSamples_m16u(frames,
				             maybe_silence(samples,
				                           reinterpret_cast<uint16_t *>(sb.dma.buf.b16)));
			}
#endif
		}
		break;
	default:
		LOG_MSG("%s: Unhandled dma mode %d", CardType(), sb.dma.mode);
		sb.mode=MODE_NONE;
		return;
	}
	// Sanity check
	assertm(frames <= samples, "Frames should never exceed samples");

	// Deduct the DMA bytes read from the remaining to still read
	sb.dma.left -= bytes_read;
	if (!sb.dma.left) {
		PIC_RemoveEvents(ProcessDMATransfer);
		if (sb.dma.mode >= DSP_DMA_16) 
			SB_RaiseIRQ(SB_IRQ_16);
		else
			SB_RaiseIRQ(SB_IRQ_8);

		if (!sb.dma.autoinit) {
			//Not new single cycle transfer waiting?
			if (!sb.dma.singlesize) {
				LOG(LOG_SB, LOG_NORMAL)("Single cycle transfer ended");
				sb.mode = MODE_NONE;
				sb.dma.mode = DSP_DMA_NONE;
			}
			else {
				//A single size transfer is still waiting, handle that now
				sb.dma.left = sb.dma.singlesize;
				sb.dma.singlesize = 0;
				LOG(LOG_SB, LOG_NORMAL)("Switch to Single cycle transfer begun");
			}
		} else {
			if (!sb.dma.autosize) {
				LOG(LOG_SB,LOG_NORMAL)("Auto-init transfer with 0 size");
				sb.mode=MODE_NONE;
			}
			//Continue with a new auto init transfer
			sb.dma.left = sb.dma.autosize;

		}
	}
	/*
	LOG_MSG("%s: sb.dma.mode=%d, stereo=%d, signed=%d, bytes_requested=%u,"
	        "bytes_to_read=%u, bytes_read = %u, samples = %u, frames = %u, dma.left = %u",
	        CardType(), sb.dma.mode, sb.dma.stereo, sb.dma.sign, bytes_requested,
	        bytes_to_read, bytes_read, samples, frames, sb.dma.left);
	*/
}

static void SuppressDMATransfer(uint32_t bytes_to_read)
{
	if (sb.dma.left < bytes_to_read)
		bytes_to_read = sb.dma.left;
	const auto read = ReadDMA8(bytes_to_read);
	sb.dma.left-=read;
	if (!sb.dma.left) {
		if (sb.dma.mode >= DSP_DMA_16) SB_RaiseIRQ(SB_IRQ_16);
		else SB_RaiseIRQ(SB_IRQ_8);
		//FIX, use the auto to single switch mechanics here as well or find a better way to silence
		if (sb.dma.autoinit) 
			sb.dma.left=sb.dma.autosize;
		else {
			sb.mode=MODE_NONE;
			sb.dma.mode=DSP_DMA_NONE;
		}
	}
	if (sb.dma.left) {
		const uint32_t bigger = (sb.dma.left > sb.dma.min) ? sb.dma.min
		                                                   : sb.dma.left;
		double delay = (bigger * 1000.0) / sb.dma.rate;
		PIC_AddEvent(SuppressDMATransfer, delay, bigger);
	}
}

static void FlushRemainingDMATransfer()
{
	if (!sb.dma.left)
		return;
	if (!sb.speaker && sb.type!=SBT_16) {
		const auto num_bytes = std::min(sb.dma.min, sb.dma.left);
		const double delay = (num_bytes * 1000.0) / sb.dma.rate;
		PIC_AddEvent(SuppressDMATransfer, delay, num_bytes);
		LOG(LOG_SB, LOG_NORMAL)("%s: Silent DMA Transfer scheduling IRQ in %.3f milliseconds", CardType(), delay);
	} else if (sb.dma.left < sb.dma.min) {
		const double delay = (sb.dma.left * 1000.0) / sb.dma.rate;
		LOG(LOG_SB, LOG_NORMAL)("%s: Short transfer scheduling IRQ in %.3f milliseconds", CardType(), delay);
		PIC_AddEvent(ProcessDMATransfer, delay, sb.dma.left);
	}
}

static void set_channel_rate_hz(const uint32_t requested_rate_hz)
{
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
	constexpr int min_rate_hz = 5000;

	const auto rate_hz = std::clamp(static_cast<int>(requested_rate_hz),
	                                min_rate_hz,
	                                native_dac_rate_hz);

	assert(sb.chan);
	if (sb.chan->GetSampleRate() != rate_hz) {
		sb.chan->SetSampleRate(rate_hz);
	}
}

static void DSP_ChangeMode(const DSP_MODES mode)
{
	if (sb.mode != mode) {
		sb.chan->FillUp();
		sb.mode = mode;
	}
}

static void DSP_RaiseIRQEvent(uint32_t /*val*/)
{
	SB_RaiseIRQ(SB_IRQ_8);
}

#if (C_DEBUG)
static const char *DmaModeName()
{
	switch (sb.dma.mode) {
	case DSP_DMA_2: return "2-bit ADPCM"; break;
	case DSP_DMA_3: return "3-bit ADPCM"; break;
	case DSP_DMA_4: return "4-bit ADPCM"; break;
	case DSP_DMA_8: return "8-bit PCM"; break;
	case DSP_DMA_16: return "16-bit PCM"; break;
	case DSP_DMA_16_ALIASED: return "16-bit (aliased) PCM"; break;
	case DSP_DMA_NONE: return "non-DMA"; break;
	};
	return "Unknown DMA-mode";
}
#endif

static void DSP_DoDMATransfer(const DMA_MODES mode, uint32_t freq, bool autoinit, bool stereo)
{
	// Fill up before changing state?
	sb.chan->FillUp();

	//Starting a new transfer will clear any active irqs?
	sb.irq.pending_8bit = false;
	sb.irq.pending_16bit = false;
	PIC_DeActivateIRQ(sb.hw.irq);

	switch (mode) {
	case DSP_DMA_2:          sb.dma.mul = (1 << SB_SH) / 4; break;
	case DSP_DMA_3:          sb.dma.mul = (1 << SB_SH) / 3; break;
	case DSP_DMA_4:          sb.dma.mul = (1 << SB_SH) / 2; break;
	case DSP_DMA_8:          sb.dma.mul = (1 << SB_SH); break;
	case DSP_DMA_16:         sb.dma.mul = (1 << SB_SH); break;
	case DSP_DMA_16_ALIASED: sb.dma.mul = (1 << SB_SH) * 2; break;
	default:
		LOG(LOG_SB,LOG_ERROR)("DSP:Illegal transfer mode %d", mode);
		return;
	}

	// Going from an active autoinit into a single cycle
	if (sb.mode >= MODE_DMA && sb.dma.autoinit && !autoinit) {
		//Don't do anything, the total will flip over on the next transfer
	}
	//Just a normal single cycle transfer
	else if (!autoinit) {
		sb.dma.left = sb.dma.singlesize;
		sb.dma.singlesize = 0;
	}
	// Going into an autoinit transfer
	else {
		//Transfer full cycle again
		sb.dma.left = sb.dma.autosize;
	}
	sb.dma.autoinit = autoinit;
	sb.dma.mode = mode;
	sb.dma.stereo = stereo;
	//Double the reading speed for stereo mode
	if (sb.dma.stereo)
		sb.dma.mul*=2;
	sb.dma.rate=(sb.freq*sb.dma.mul) >> SB_SH;
	sb.dma.min=(sb.dma.rate*3)/1000;
	set_channel_rate_hz(freq);

	PIC_RemoveEvents(ProcessDMATransfer);
	//Set to be masked, the dma call can change this again.
	sb.mode = MODE_DMA_MASKED;
	sb.dma.chan->RegisterCallback(DSP_DMA_CallBack);

#if (C_DEBUG)
	LOG(LOG_SB, LOG_NORMAL)
	("DMA Transfer:%s %s %s freq %d rate %d size %d", DmaModeName(),
	 stereo ? "Stereo" : "Mono", autoinit ? "Auto-Init" : "Single-Cycle",
	 freq, sb.dma.rate, sb.dma.left);
#endif
}

static void DSP_PrepareDMA_Old(DMA_MODES mode,bool autoinit,bool sign) {
	sb.dma.sign=sign;
	if (!autoinit)
		sb.dma.singlesize=1+sb.dsp.in.data[0]+(sb.dsp.in.data[1] << 8);
	sb.dma.chan=DMA_GetChannel(sb.hw.dma8);
	DSP_DoDMATransfer(mode,sb.freq / (sb.mixer.stereo ? 2 : 1), autoinit, sb.mixer.stereo);
}

static void DSP_PrepareDMA_New(DMA_MODES mode, uint32_t length, bool autoinit, bool stereo)
{
	const auto freq = sb.freq;

	//equal length if data format and dma channel are both 16-bit or 8-bit
	if (mode==DSP_DMA_16) {
		if (sb.hw.dma16!=0xff) {
			sb.dma.chan=DMA_GetChannel(sb.hw.dma16);
			if (sb.dma.chan==nullptr) {
				sb.dma.chan=DMA_GetChannel(sb.hw.dma8);
				mode=DSP_DMA_16_ALIASED;
				length *= 2;
			}
		} else {
			sb.dma.chan=DMA_GetChannel(sb.hw.dma8);
			mode=DSP_DMA_16_ALIASED;
			//UNDOCUMENTED:
			//In aliased mode sample length is written to DSP as number of
			//16-bit samples so we need double 8-bit DMA buffer length
			length *= 2;
		}
	} else sb.dma.chan=DMA_GetChannel(sb.hw.dma8);
	//Set the length to the correct register depending on mode
	if (autoinit) {
		sb.dma.autosize = length;
	}
	else {
		sb.dma.singlesize = length;
	}
	DSP_DoDMATransfer(mode,freq,autoinit,stereo);
}

static void DSP_AddData(uint8_t val) {
	if (sb.dsp.out.used<DSP_BUFSIZE) {
		auto start = sb.dsp.out.used + sb.dsp.out.pos;
		if (start>=DSP_BUFSIZE) start-=DSP_BUFSIZE;
		sb.dsp.out.data[start]=val;
		sb.dsp.out.used++;
	} else {
		LOG(LOG_SB,LOG_ERROR)("DSP:Data Output buffer full");
	}
}

static void DSP_FinishReset(uint32_t /*val*/)
{
	DSP_FlushData();
	DSP_AddData(0xaa);
	sb.dsp.state=DSP_S_NORMAL;
}

static void DSP_Reset() {
	LOG(LOG_SB,LOG_ERROR)("DSP:Reset");
	PIC_DeActivateIRQ(sb.hw.irq);

	DSP_ChangeMode(MODE_NONE);
	DSP_FlushData();
	sb.dsp.cmd=DSP_NO_COMMAND;
	sb.dsp.cmd_len=0;
	sb.dsp.in.pos=0;
	sb.dsp.write_busy=0;
	sb.dsp.reset_tally++;
	PIC_RemoveEvents(DSP_FinishReset);

	sb.dma.left=0;
	sb.dma.singlesize=0;
	sb.dma.autosize = 0;
	sb.dma.stereo=false;
	sb.dma.sign=false;
	sb.dma.autoinit=false;
	sb.dma.mode=DSP_DMA_NONE;
	sb.dma.remain_size=0;
	if (sb.dma.chan) sb.dma.chan->ClearRequest();

	sb.adpcm = {};
	sb.freq = default_playback_rate_hz;
	sb.time_constant=45;
	sb.dac.used=0;
	sb.dac.last=0;
	sb.e2.value=0xaa;
	sb.e2.count=0;
	sb.irq.pending_8bit=false;
	sb.irq.pending_16bit=false;
	set_channel_rate_hz(default_playback_rate_hz);
	InitializeSpeakerState();
	PIC_RemoveEvents(ProcessDMATransfer);
}

static void DSP_DoReset(uint8_t val) {
	if (((val&1)!=0) && (sb.dsp.state!=DSP_S_RESET)) {
		// TODO Get out of highspeed mode
		// Halt the channel so we're silent across reset events.
		// Channel is re-enabled (if SB16) or via control by the game
		// (non-SB16).
		sb.chan->Enable(false);
		DSP_Reset();
		sb.dsp.state=DSP_S_RESET;
	} else if (((val&1)==0) && (sb.dsp.state==DSP_S_RESET)) {	// reset off
		sb.dsp.state=DSP_S_RESET_WAIT;
		PIC_RemoveEvents(DSP_FinishReset);
		PIC_AddEvent(DSP_FinishReset, 20.0 / 1000.0, 0); // 20 microseconds
		LOG_MSG("%s: DSP was reset", CardType());
	}
}

static void DSP_E2_DMA_CallBack(const DmaChannel* /*chan*/, DMAEvent event)
{
	if (event==DMA_UNMASKED) {
		uint8_t val=(uint8_t)(sb.e2.value&0xff);
		DmaChannel * chan=DMA_GetChannel(sb.hw.dma8);
		chan->RegisterCallback(nullptr);
		chan->Write(1,&val);
	}
}

static void DSP_ADC_CallBack(const DmaChannel* /*chan*/, DMAEvent event)
{
	if (event!=DMA_UNMASKED) return;
	uint8_t val=128;
	DmaChannel * ch=DMA_GetChannel(sb.hw.dma8);
	while (sb.dma.left--) {
		ch->Write(1,&val);
	}
	SB_RaiseIRQ(SB_IRQ_8);
	ch->RegisterCallback(nullptr);
}

static void DSP_ChangeRate(uint32_t freq)
{
	if (sb.freq != freq && sb.dma.mode != DSP_DMA_NONE) {
		sb.chan->FillUp();
		set_channel_rate_hz(freq / (sb.mixer.stereo ? 2 : 1));
		sb.dma.rate=(freq*sb.dma.mul) >> SB_SH;
		sb.dma.min=(sb.dma.rate*3)/1000;
	}
	sb.freq=freq;
}

#define DSP_SB16_ONLY if (sb.type != SBT_16) { LOG(LOG_SB,LOG_ERROR)("DSP:Command %2X requires SB16",sb.dsp.cmd); break; }
#define DSP_SB2_ABOVE if (sb.type <= SBT_1) { LOG(LOG_SB,LOG_ERROR)("DSP:Command %2X requires SB2 or above",sb.dsp.cmd); break; }

static void DSP_DoCommand() {
//	LOG_MSG("DSP Command %X",sb.dsp.cmd);
	switch (sb.dsp.cmd) {
	case 0x04:
		if (sb.type == SBT_16) {
			/* SB16 ASP set mode register */
			if ((sb.dsp.in.data[0]&0xf1)==0xf1) ASP_init_in_progress=true;
			else ASP_init_in_progress=false;
			LOG(LOG_SB,LOG_NORMAL)("DSP Unhandled SB16ASP command %X (set mode register to %X)",sb.dsp.cmd,sb.dsp.in.data[0]);
		} else {
			/* DSP Status SB 2.0/pro version. NOT SB16. */
			DSP_FlushData();
			if (sb.type == SBT_2) DSP_AddData(0x88);
			else if ((sb.type == SBT_PRO1) || (sb.type == SBT_PRO2)) DSP_AddData(0x7b);
			else DSP_AddData(0xff);			//Everything enabled
		}
		break;
	case 0x05:	/* SB16 ASP set codec parameter */
		LOG(LOG_SB,LOG_NORMAL)("DSP Unhandled SB16ASP command %X (set codec parameter)",sb.dsp.cmd);
		break;
	case 0x08:	/* SB16 ASP get version */
		LOG(LOG_SB,LOG_NORMAL)("DSP Unhandled SB16ASP command %X sub %X",sb.dsp.cmd,sb.dsp.in.data[0]);
		if (sb.type == SBT_16) {
			switch (sb.dsp.in.data[0]) {
				case 0x03:
					DSP_AddData(0x18);	// version ID (??)
					break;
				default:
					LOG(LOG_SB,LOG_NORMAL)("DSP Unhandled SB16ASP command %X sub %X",sb.dsp.cmd,sb.dsp.in.data[0]);
					break;
			}
		} else {
			LOG(LOG_SB,LOG_NORMAL)("DSP Unhandled SB16ASP command %X sub %X",sb.dsp.cmd,sb.dsp.in.data[0]);
		}
		break;
	case 0x0e:	/* SB16 ASP set register */
		if (sb.type == SBT_16) {
//			LOG(LOG_SB,LOG_NORMAL)("SB16 ASP set register %X := %X",sb.dsp.in.data[0],sb.dsp.in.data[1]);
			ASP_regs[sb.dsp.in.data[0]] = sb.dsp.in.data[1];
		} else {
			LOG(LOG_SB,LOG_NORMAL)("DSP Unhandled SB16ASP command %X (set register)",sb.dsp.cmd);
		}
		break;
	case 0x0f:	/* SB16 ASP get register */
		if (sb.type == SBT_16) {
			if ((ASP_init_in_progress) && (sb.dsp.in.data[0]==0x83)) {
				ASP_regs[0x83] = ~ASP_regs[0x83];
			}
//			LOG(LOG_SB,LOG_NORMAL)("SB16 ASP get register %X == %X",sb.dsp.in.data[0],ASP_regs[sb.dsp.in.data[0]]);
			DSP_AddData(ASP_regs[sb.dsp.in.data[0]]);
		} else {
			LOG(LOG_SB,LOG_NORMAL)("DSP Unhandled SB16ASP command %X (get register)",sb.dsp.cmd);
		}
		break;
	case 0x10:	/* Direct DAC */
		DSP_ChangeMode(MODE_DAC);
		if (sb.dac.used<DSP_DACSIZE) {
			const auto mono_sample = lut_u8to16[sb.dsp.in.data[0]];
			sb.dac.data[sb.dac.used++] = mono_sample;
			sb.dac.data[sb.dac.used++] = mono_sample;
		}
		break;
	case 0x24:	/* Singe Cycle 8-Bit DMA ADC */
		//Directly write to left?
		sb.dma.left = 1 + sb.dsp.in.data[0] + (sb.dsp.in.data[1] << 8);
		sb.dma.sign=false;
		LOG(LOG_SB,LOG_ERROR)("DSP:Faked ADC for %u bytes",sb.dma.left);
		DMA_GetChannel(sb.hw.dma8)->RegisterCallback(DSP_ADC_CallBack);
		break;
	case 0x14:	/* Singe Cycle 8-Bit DMA DAC */
	case 0x15:	/* Wari hack. Waru uses this one instead of 0x14, but some weird stuff going on there anyway */
	case 0x91:	/* Singe Cycle 8-Bit DMA High speed DAC */
		/* Note: 0x91 is documented only for DSP ver.2.x and 3.x, not 4.x */
		DSP_PrepareDMA_Old(DSP_DMA_8,false,false);
		break;
	case 0x90:	/* Auto Init 8-bit DMA High Speed */
	case 0x1c:	/* Auto Init 8-bit DMA */
		DSP_SB2_ABOVE; /* Note: 0x90 is documented only for DSP ver.2.x and 3.x, not 4.x */
		DSP_PrepareDMA_Old(DSP_DMA_8,true,false);
		break;
	case 0x38:  /* Write to SB MIDI Output */
		if (sb.midi == true) MIDI_RawOutByte(sb.dsp.in.data[0]);
		break;
	case 0x40:	/* Set Timeconstant */
		DSP_ChangeRate(1000000 / (256 - sb.dsp.in.data[0]));
		break;
	case 0x41:	/* Set Output Samplerate */
	case 0x42:	/* Set Input Samplerate */
		/* Note: 0x42 is handled like 0x41, needed by Fasttracker II */
		DSP_SB16_ONLY;
		DSP_ChangeRate((sb.dsp.in.data[0] << 8) | sb.dsp.in.data[1]);
		break;
	case 0x48:	/* Set DMA Block Size */
		DSP_SB2_ABOVE;
		sb.dma.autosize=1+sb.dsp.in.data[0]+(sb.dsp.in.data[1] << 8);
		break;
	case 0x75:	/* 075h : Single Cycle 4-bit ADPCM Reference */
		sb.adpcm.haveref = true;
		[[fallthrough]];
	case 0x74:	/* 074h : Single Cycle 4-bit ADPCM */
		DSP_PrepareDMA_Old(DSP_DMA_4,false,false);
		break;
	case 0x77:	/* 077h : Single Cycle 3-bit(2.6bit) ADPCM Reference*/
		sb.adpcm.haveref = true;
		[[fallthrough]];
	case 0x76:	/* 076h : Single Cycle 3-bit(2.6bit) ADPCM */
		DSP_PrepareDMA_Old(DSP_DMA_3,false,false);
		break;
	case 0x7d:	/* Auto Init 4-bit ADPCM Reference */
		DSP_SB2_ABOVE;
		sb.adpcm.haveref = true;
		DSP_PrepareDMA_Old(DSP_DMA_4,true,false);
		break;
	case 0x17:	/* 017h : Single Cycle 2-bit ADPCM Reference*/
		sb.adpcm.haveref = true;
		[[fallthrough]];
	case 0x16:	/* 016h : Single Cycle 2-bit ADPCM */
		DSP_PrepareDMA_Old(DSP_DMA_2,false,false);
		break;
	case 0x80:	/* Silence DAC */
		PIC_AddEvent(&DSP_RaiseIRQEvent,
		             (1000.0 * (1 + sb.dsp.in.data[0] + (sb.dsp.in.data[1] << 8)) /
		              sb.freq));
		break;
	case 0xb0:	case 0xb1:	case 0xb2:	case 0xb3:  case 0xb4:	case 0xb5:	case 0xb6:	case 0xb7:
	case 0xb8:	case 0xb9:	case 0xba:	case 0xbb:  case 0xbc:	case 0xbd:	case 0xbe:	case 0xbf:
	case 0xc0:	case 0xc1:	case 0xc2:	case 0xc3:  case 0xc4:	case 0xc5:	case 0xc6:	case 0xc7:
	case 0xc8:	case 0xc9:	case 0xca:	case 0xcb:  case 0xcc:	case 0xcd:	case 0xce:	case 0xcf:
		DSP_SB16_ONLY;
		/* Generic 8/16 bit DMA */
		sb.dma.sign=(sb.dsp.in.data[0] & 0x10) > 0;
		DSP_PrepareDMA_New((sb.dsp.cmd & 0x10) ? DSP_DMA_16 : DSP_DMA_8,
			1+sb.dsp.in.data[1]+(sb.dsp.in.data[2] << 8),
			(sb.dsp.cmd & 0x4)>0,
			(sb.dsp.in.data[0] & 0x20) > 0
		);
		break;
	case 0xd5:	/* Halt 16-bit DMA */
		DSP_SB16_ONLY;
		[[fallthrough]];
	case 0xd0:	/* Halt 8-bit DMA */
//		DSP_ChangeMode(MODE_NONE);
		LOG(LOG_SB, LOG_NORMAL)("Halt DMA Command");
//		Games sometimes already program a new dma before stopping, gives noise
		if (sb.mode==MODE_NONE) {
			// possibly different code here that does not switch to MODE_DMA_PAUSE
		}
		sb.mode=MODE_DMA_PAUSE;
		PIC_RemoveEvents(ProcessDMATransfer);
		break;
	case 0xd1:	/* Enable Speaker */
		DSP_SetSpeaker(true);
		break;
	case 0xd3:	/* Disable Speaker */
		DSP_SetSpeaker(false);
		break;
	case 0xd8:  /* Speaker status */
		DSP_SB2_ABOVE;
		DSP_FlushData();
		if (sb.speaker) {
			DSP_AddData(0xff);
			// If the game is courteous enough to ask if the speaker
			// is ready, then we can be confident it won't play
			// garbage content, so we zero the warmup count down.
			// remaining warmup time.
			sb.dsp.warmup_remaining_ms = 0;
		} else {
			DSP_AddData(0x00);
		}
		break;
	case 0xd6:	/* Continue DMA 16-bit */
		DSP_SB16_ONLY;
		[[fallthrough]];
	case 0xd4:	/* Continue DMA 8-bit*/
		LOG(LOG_SB, LOG_NORMAL)("Continue DMA command");
		if (sb.mode==MODE_DMA_PAUSE) {
			sb.mode=MODE_DMA_MASKED;
			if (sb.dma.chan!=nullptr) sb.dma.chan->RegisterCallback(DSP_DMA_CallBack);
		}
		break;
	case 0xd9:  /* Exit Autoinitialize 16-bit */
		DSP_SB16_ONLY;
		[[fallthrough]];
	case 0xda:	/* Exit Autoinitialize 8-bit */
		DSP_SB2_ABOVE;
		LOG(LOG_SB, LOG_NORMAL)("Exit Autoinit command");
		sb.dma.autoinit=false;		//Should stop itself
		break;
	case 0xe0:	/* DSP Identification - SB2.0+ */
		DSP_FlushData();
		DSP_AddData(~sb.dsp.in.data[0]);
		break;
	case 0xe1:	/* Get DSP Version */
		DSP_FlushData();
		switch (sb.type) {
		case SBT_1:
			DSP_AddData(0x1);DSP_AddData(0x05);break;
		case SBT_2:
			DSP_AddData(0x2);DSP_AddData(0x1);break;
		case SBT_PRO1:
			DSP_AddData(0x3);DSP_AddData(0x0);break;
		case SBT_PRO2:
			DSP_AddData(0x3);DSP_AddData(0x2);break;
		case SBT_16:
			DSP_AddData(0x4);DSP_AddData(0x5);break;
		default:
			break;
		}
		break;
	case 0xe2:	/* Weird DMA identification write routine */
		{
			LOG(LOG_SB,LOG_NORMAL)("DSP Function 0xe2");
		        for (uint8_t i = 0; i < 8; i++)
			        if ((sb.dsp.in.data[0] >> i) & 0x01)
				        sb.e2.value += E2_incr_table[sb.e2.count % 4][i];
		        sb.e2.value += E2_incr_table[sb.e2.count % 4][8];
		        sb.e2.count++;
		        DMA_GetChannel(sb.hw.dma8)->RegisterCallback(DSP_E2_DMA_CallBack);
		}
		break;
	case 0xe3: /* DSP Copyright */
		DSP_FlushData();
		for (const auto c : "COPYRIGHT (C) CREATIVE TECHNOLOGY LTD, 1992.")
			DSP_AddData(static_cast<uint8_t>(c));
		break;
	case 0xe4:	/* Write Test Register */
		sb.dsp.test_register=sb.dsp.in.data[0];
		break;
	case 0xe8:	/* Read Test Register */
		DSP_FlushData();
		DSP_AddData(sb.dsp.test_register);;
		break;
	case 0xf2:	/* Trigger 8bit IRQ */
		//Small delay in order to emulate the slowness of the DSP, fixes Llamatron 2012 and Lemmings 3D
		PIC_AddEvent(&DSP_RaiseIRQEvent, 0.01);
		LOG(LOG_SB, LOG_NORMAL)("Trigger 8bit IRQ command");
		break;
	case 0xf3:   /* Trigger 16bit IRQ */
		DSP_SB16_ONLY;
		SB_RaiseIRQ(SB_IRQ_16);
		LOG(LOG_SB, LOG_NORMAL)("Trigger 16bit IRQ command");
		break;
	case 0xf8:  /* Undocumented, pre-SB16 only */
		DSP_FlushData();
		DSP_AddData(0);
		break;
	case 0x30: case 0x31:
		LOG(LOG_SB,LOG_ERROR)("DSP:Unimplemented MIDI I/O command %2X",sb.dsp.cmd);
		break;
	case 0x34: case 0x35: case 0x36: case 0x37:
		DSP_SB2_ABOVE;
		LOG(LOG_SB,LOG_ERROR)("DSP:Unimplemented MIDI UART command %2X",sb.dsp.cmd);
		break;
	case 0x7f: case 0x1f:
		DSP_SB2_ABOVE;
		LOG(LOG_SB,LOG_ERROR)("DSP:Unimplemented auto-init DMA ADPCM command %2X",sb.dsp.cmd);
		break;
	case 0x20:
		DSP_AddData(0x7f);   // fake silent input for Creative parrot
		break;
	case 0x2c:
	case 0x98: case 0x99: /* Documented only for DSP 2.x and 3.x */
	case 0xa0: case 0xa8: /* Documented only for DSP 3.x */
		LOG(LOG_SB,LOG_ERROR)("DSP:Unimplemented input command %2X",sb.dsp.cmd);
		break;
	case 0xf9:	/* SB16 ASP ??? */
		if (sb.type == SBT_16) {
			LOG(LOG_SB,LOG_NORMAL)("SB16 ASP unknown function %x",sb.dsp.in.data[0]);
			// just feed it what it expects
			switch (sb.dsp.in.data[0]) {
			case 0x0b:
				DSP_AddData(0x00);
				break;
			case 0x0e:
				DSP_AddData(0xff);
				break;
			case 0x0f:
				DSP_AddData(0x07);
				break;
			case 0x23:
				DSP_AddData(0x00);
				break;
			case 0x24:
				DSP_AddData(0x00);
				break;
			case 0x2b:
				DSP_AddData(0x00);
				break;
			case 0x2c:
				DSP_AddData(0x00);
				break;
			case 0x2d:
				DSP_AddData(0x00);
				break;
			case 0x37:
				DSP_AddData(0x38);
				break;
			default:
				DSP_AddData(0x00);
				break;
			}
		} else {
			LOG(LOG_SB,LOG_NORMAL)("SB16 ASP unknown function %X",sb.dsp.cmd);
		}
		break;
	default:
		LOG(LOG_SB,LOG_ERROR)("DSP:Unhandled (undocumented) command %2X",sb.dsp.cmd);
		break;
	}
	sb.dsp.cmd=DSP_NO_COMMAND;
	sb.dsp.cmd_len=0;
	sb.dsp.in.pos=0;
}

static void DSP_DoWrite(uint8_t val) {
	switch (sb.dsp.cmd) {
	case DSP_NO_COMMAND:
		sb.dsp.cmd=val;
		if (sb.type == SBT_16) sb.dsp.cmd_len=DSP_cmd_len_sb16[val];
		else sb.dsp.cmd_len=DSP_cmd_len_sb[val];
		sb.dsp.in.pos=0;
		if (!sb.dsp.cmd_len) DSP_DoCommand();
		break;
	default:
		sb.dsp.in.data[sb.dsp.in.pos]=val;
		sb.dsp.in.pos++;
		if (sb.dsp.in.pos>=sb.dsp.cmd_len) DSP_DoCommand();
	}
}

static uint8_t DSP_ReadData() {
/* Static so it repeats the last value on succesive reads (JANGLE DEMO) */
	if (sb.dsp.out.used) {
		sb.dsp.out.lastval=sb.dsp.out.data[sb.dsp.out.pos];
		sb.dsp.out.pos++;
		if (sb.dsp.out.pos>=DSP_BUFSIZE) sb.dsp.out.pos-=DSP_BUFSIZE;
		sb.dsp.out.used--;
	}
	return sb.dsp.out.lastval;
}

static float calc_vol(uint8_t amount) {
	uint8_t count = 31 - amount;
	float db = static_cast<float>(count);
	if (sb.type == SBT_PRO1 || sb.type == SBT_PRO2) {
		if (count) {
			if (count < 16) db -= 1.0f;
			else if (count > 16) db += 1.0f;
			if (count == 24) db += 2.0f;
			if (count > 27) return 0.0f; //turn it off.
		}
	} else { //Give the rest, the SB16 scale, as we don't have data.
		db *= 2.0f;
		if (count > 20) db -= 1.0f;
	}
	return powf(10.0f, -0.05f * db);
}
static void CTMIXER_UpdateVolumes() {
	if (!sb.mixer.enabled) return;

	float m0 = calc_vol(sb.mixer.master[0]);
	float m1 = calc_vol(sb.mixer.master[1]);
	auto chan = MIXER_FindChannel("SB");
	if (chan) {
		chan->SetAppVolume(m0 * calc_vol(sb.mixer.dac[0]),
		                   m1 * calc_vol(sb.mixer.dac[1]));
	}
	chan = MIXER_FindChannel("OPL");
	if (chan) {
		chan->SetAppVolume(m0 * calc_vol(sb.mixer.fm[0]),
		                   m1 * calc_vol(sb.mixer.fm[1]));
	}
	chan = MIXER_FindChannel("CDAUDIO");
	if (chan) {
		chan->SetAppVolume(m0 * calc_vol(sb.mixer.cda[0]),
		                   m1 * calc_vol(sb.mixer.cda[1]));
	}
}

static void CTMIXER_Reset() {
	sb.mixer.fm[0]=
	sb.mixer.fm[1]=
	sb.mixer.cda[0]=
	sb.mixer.cda[1]=
	sb.mixer.dac[0]=
	sb.mixer.dac[1]=31;
	sb.mixer.master[0]=
	sb.mixer.master[1]=31;
	CTMIXER_UpdateVolumes();
}

#define SETPROVOL(_WHICH_,_VAL_)										\
	_WHICH_[0]=   ((((_VAL_) & 0xf0) >> 3)|(sb.type==SBT_16 ? 1:3));	\
	_WHICH_[1]=   ((((_VAL_) & 0x0f) << 1)|(sb.type==SBT_16 ? 1:3));	\

#define MAKEPROVOL(_WHICH_)											\
	((((_WHICH_[0] & 0x1e) << 3) | ((_WHICH_[1] & 0x1e) >> 1)) |	\
		((sb.type==SBT_PRO1 || sb.type==SBT_PRO2) ? 0x11:0))

static void DSP_ChangeStereo(bool stereo) {
	if (!sb.dma.stereo && stereo) {
		set_channel_rate_hz(sb.freq / 2);
		sb.dma.mul*=2;
		sb.dma.rate=(sb.freq*sb.dma.mul) >> SB_SH;
		sb.dma.min=(sb.dma.rate*3)/1000;
	} else if (sb.dma.stereo && !stereo) {
		set_channel_rate_hz(sb.freq);
		sb.dma.mul/=2;
		sb.dma.rate=(sb.freq*sb.dma.mul) >> SB_SH;
		sb.dma.min=(sb.dma.rate*3)/1000;
	}
	sb.dma.stereo=stereo;
}

static void CTMIXER_Write(uint8_t val) {
	switch (sb.mixer.index) {
	case 0x00:		/* Reset */
		CTMIXER_Reset();
		LOG(LOG_SB,LOG_WARN)("Mixer reset value %x",val);
		break;
	case 0x02:		/* Master Volume (SB2 Only) */
		SETPROVOL(sb.mixer.master,(val&0xf)|(val<<4));
		CTMIXER_UpdateVolumes();
		break;
	case 0x04:		/* DAC Volume (SBPRO) */
		SETPROVOL(sb.mixer.dac,val);
		CTMIXER_UpdateVolumes();
		break;
	case 0x06:		/* FM output selection, Somewhat obsolete with dual OPL SBpro + FM volume (SB2 Only) */
		//volume controls both channels
		SETPROVOL(sb.mixer.fm,(val&0xf)|(val<<4));
		CTMIXER_UpdateVolumes();
		if(val&0x60) LOG(LOG_SB,LOG_WARN)("Turned FM one channel off. not implemented %X",val);
		//TODO Change FM Mode if only 1 fm channel is selected
		break;
	case 0x08:		/* CDA Volume (SB2 Only) */
		SETPROVOL(sb.mixer.cda,(val&0xf)|(val<<4));
		CTMIXER_UpdateVolumes();
		break;
	case 0x0a:		/* Mic Level (SBPRO) or DAC Volume (SB2): 2-bit, 3-bit on SB16 */
		if (sb.type==SBT_2) {
			sb.mixer.dac[0]=sb.mixer.dac[1]=((val & 0x6) << 2)|3;
			CTMIXER_UpdateVolumes();
		} else {
			sb.mixer.mic=((val & 0x7) << 2)|(sb.type==SBT_16?1:3);
		}
		break;
	case 0x0e:		/* Output/Stereo Select */
		sb.mixer.stereo=(val & 0x2) > 0;
		sb.mixer.filtered=(val & 0x20) > 0;
		if (sb.sb_filter_state != FilterState::ForcedOn) {
			sb.sb_filter_state = sb.mixer.filtered ? FilterState::On
			                                       : FilterState::Off;
			sb.chan->SetLowPassFilter(sb.sb_filter_state);
		}
		DSP_ChangeStereo(sb.mixer.stereo);
		LOG(LOG_SB,LOG_WARN)("Mixer set to %s",sb.dma.stereo ? "STEREO" : "MONO");
		break;
	case 0x22:		/* Master Volume (SBPRO) */
		SETPROVOL(sb.mixer.master,val);
		CTMIXER_UpdateVolumes();
		break;
	case 0x26:		/* FM Volume (SBPRO) */
		SETPROVOL(sb.mixer.fm,val);
		CTMIXER_UpdateVolumes();
		break;
	case 0x28:		/* CD Audio Volume (SBPRO) */
		SETPROVOL(sb.mixer.cda,val);
		CTMIXER_UpdateVolumes();
		break;
	case 0x2e:		/* Line-in Volume (SBPRO) */
		SETPROVOL(sb.mixer.lin,val);
		break;
	//case 0x20:		/* Master Volume Left (SBPRO) ? */
	case 0x30:		/* Master Volume Left (SB16) */
		if (sb.type==SBT_16) {
			sb.mixer.master[0]=val>>3;
			CTMIXER_UpdateVolumes();
		}
		break;
	//case 0x21:		/* Master Volume Right (SBPRO) ? */
	case 0x31:		/* Master Volume Right (SB16) */
		if (sb.type==SBT_16) {
			sb.mixer.master[1]=val>>3;
			CTMIXER_UpdateVolumes();
		}
		break;
	case 0x32:		/* DAC Volume Left (SB16) */
		if (sb.type==SBT_16) {
			sb.mixer.dac[0]=val>>3;
			CTMIXER_UpdateVolumes();
		}
		break;
	case 0x33:		/* DAC Volume Right (SB16) */
		if (sb.type==SBT_16) {
			sb.mixer.dac[1]=val>>3;
			CTMIXER_UpdateVolumes();
		}
		break;
	case 0x34:		/* FM Volume Left (SB16) */
		if (sb.type==SBT_16) {
			sb.mixer.fm[0]=val>>3;
			CTMIXER_UpdateVolumes();
		}
                break;
	case 0x35:		/* FM Volume Right (SB16) */
		if (sb.type==SBT_16) {
			sb.mixer.fm[1]=val>>3;
			CTMIXER_UpdateVolumes();
		}
		break;
	case 0x36:		/* CD Volume Left (SB16) */
		if (sb.type==SBT_16) {
			sb.mixer.cda[0]=val>>3;
			CTMIXER_UpdateVolumes();
		}
		break;
	case 0x37:		/* CD Volume Right (SB16) */
		if (sb.type==SBT_16) {
			sb.mixer.cda[1]=val>>3;
			CTMIXER_UpdateVolumes();
		}
		break;
	case 0x38:		/* Line-in Volume Left (SB16) */
		if (sb.type==SBT_16) sb.mixer.lin[0]=val>>3;
		break;
	case 0x39:		/* Line-in Volume Right (SB16) */
		if (sb.type==SBT_16) sb.mixer.lin[1]=val>>3;
		break;
	case 0x3a:
		if (sb.type==SBT_16) sb.mixer.mic=val>>3;
		break;
	case 0x80:		/* IRQ Select */
		sb.hw.irq=0xff;
		if (val & 0x1) sb.hw.irq=2;
		else if (val & 0x2) sb.hw.irq=5;
		else if (val & 0x4) sb.hw.irq=7;
		else if (val & 0x8) sb.hw.irq=10;
		break;
	case 0x81:		/* DMA Select */
		sb.hw.dma8=0xff;
		sb.hw.dma16=0xff;
		if (val & 0x1) sb.hw.dma8=0;
		else if (val & 0x2) sb.hw.dma8=1;
		else if (val & 0x8) sb.hw.dma8=3;
		if (val & 0x20) sb.hw.dma16=5;
		else if (val & 0x40) sb.hw.dma16=6;
		else if (val & 0x80) sb.hw.dma16=7;
		LOG(LOG_SB,LOG_NORMAL)("Mixer select dma8:%x dma16:%x",sb.hw.dma8,sb.hw.dma16);
		break;
	default:

		if(	((sb.type == SBT_PRO1 || sb.type == SBT_PRO2) && sb.mixer.index==0x0c) || /* Input control on SBPro */
			 (sb.type == SBT_16 && sb.mixer.index >= 0x3b && sb.mixer.index <= 0x47)) /* New SB16 registers */
			sb.mixer.unhandled[sb.mixer.index] = val;
		LOG(LOG_SB,LOG_WARN)("MIXER:Write %X to unhandled index %X",val,sb.mixer.index);
	}
}

static uint8_t CTMIXER_Read() {
	uint8_t ret;
//	if ( sb.mixer.index< 0x80) LOG_MSG("Read mixer %x",sb.mixer.index);
	switch (sb.mixer.index) {
	case 0x00:		/* RESET */
		return 0x00;
	case 0x02:		/* Master Volume (SB2 Only) */
		return ((sb.mixer.master[1]>>1) & 0xe);
	case 0x22:		/* Master Volume (SBPRO) */
		return	MAKEPROVOL(sb.mixer.master);
	case 0x04:		/* DAC Volume (SBPRO) */
		return MAKEPROVOL(sb.mixer.dac);
	case 0x06:		/* FM Volume (SB2 Only) + FM output selection */
		return ((sb.mixer.fm[1]>>1) & 0xe);
	case 0x08:		/* CD Volume (SB2 Only) */
		return ((sb.mixer.cda[1]>>1) & 0xe);
	case 0x0a:		/* Mic Level (SBPRO) or Voice (SB2 Only) */
		if (sb.type==SBT_2) return (sb.mixer.dac[0]>>2);
		else return ((sb.mixer.mic >> 2) & (sb.type==SBT_16 ? 7:6));
	case 0x0e:		/* Output/Stereo Select */
		return 0x11|(sb.mixer.stereo ? 0x02 : 0x00)|(sb.mixer.filtered ? 0x20 : 0x00);
	case 0x26:		/* FM Volume (SBPRO) */
		return MAKEPROVOL(sb.mixer.fm);
	case 0x28:		/* CD Audio Volume (SBPRO) */
		return MAKEPROVOL(sb.mixer.cda);
	case 0x2e:		/* Line-IN Volume (SBPRO) */
		return MAKEPROVOL(sb.mixer.lin);
	case 0x30:		/* Master Volume Left (SB16) */
		if (sb.type==SBT_16) return sb.mixer.master[0]<<3;
		ret=0xa;
		break;
	case 0x31:		/* Master Volume Right (S16) */
		if (sb.type==SBT_16) return sb.mixer.master[1]<<3;
		ret=0xa;
		break;
	case 0x32:		/* DAC Volume Left (SB16) */
		if (sb.type==SBT_16) return sb.mixer.dac[0]<<3;
		ret=0xa;
		break;
	case 0x33:		/* DAC Volume Right (SB16) */
		if (sb.type==SBT_16) return sb.mixer.dac[1]<<3;
		ret=0xa;
		break;
	case 0x34:		/* FM Volume Left (SB16) */
		if (sb.type==SBT_16) return sb.mixer.fm[0]<<3;
		ret=0xa;
		break;
	case 0x35:		/* FM Volume Right (SB16) */
		if (sb.type==SBT_16) return sb.mixer.fm[1]<<3;
		ret=0xa;
		break;
	case 0x36:		/* CD Volume Left (SB16) */
		if (sb.type==SBT_16) return sb.mixer.cda[0]<<3;
		ret=0xa;
		break;
	case 0x37:		/* CD Volume Right (SB16) */
		if (sb.type==SBT_16) return sb.mixer.cda[1]<<3;
		ret=0xa;
		break;
	case 0x38:		/* Line-in Volume Left (SB16) */
		if (sb.type==SBT_16) return sb.mixer.lin[0]<<3;
		ret=0xa;
		break;
	case 0x39:		/* Line-in Volume Right (SB16) */
		if (sb.type==SBT_16) return sb.mixer.lin[1]<<3;
		ret=0xa;
		break;
	case 0x3a:		/* Mic Volume (SB16) */
		if (sb.type==SBT_16) return sb.mixer.mic<<3;
		ret=0xa;
		break;
	case 0x80:		/* IRQ Select */
		ret = 0;
		switch (sb.hw.irq) {
		case 2:  return 0x1;
		case 5:  return 0x2;
		case 7:  return 0x4;
		case 10: return 0x8;
		}
		break;
	case 0x81:		/* DMA Select */
		ret=0;
		switch (sb.hw.dma8) {
		case 0:ret|=0x1;break;
		case 1:ret|=0x2;break;
		case 3:ret|=0x8;break;
		}
		switch (sb.hw.dma16) {
		case 5:ret|=0x20;break;
		case 6:ret|=0x40;break;
		case 7:ret|=0x80;break;
		}
		return ret;
	case 0x82:		/* IRQ Status */
		return	(sb.irq.pending_8bit ? 0x1 : 0) |
				(sb.irq.pending_16bit ? 0x2 : 0) | 
				((sb.type == SBT_16) ? 0x20 : 0);
	default:
		if (	((sb.type == SBT_PRO1 || sb.type == SBT_PRO2) && sb.mixer.index==0x0c) || /* Input control on SBPro */
			(sb.type == SBT_16 && sb.mixer.index >= 0x3b && sb.mixer.index <= 0x47)) /* New SB16 registers */
			ret = sb.mixer.unhandled[sb.mixer.index];
		else
			ret=0xa;
		LOG(LOG_SB,LOG_WARN)("MIXER:Read from unhandled index %X",sb.mixer.index);
	}
	return ret;
}

static uint8_t read_sb(io_port_t port, io_width_t)
{
	switch (port - sb.hw.base) {
	case MIXER_INDEX: return sb.mixer.index;
	case MIXER_DATA: return CTMIXER_Read();
	case DSP_READ_DATA: return DSP_ReadData();
	case DSP_READ_STATUS:
		//TODO See for high speed dma :)
		if (sb.irq.pending_8bit)  {
			sb.irq.pending_8bit=false;
			PIC_DeActivateIRQ(sb.hw.irq);
		}
		if (sb.dsp.out.used) return 0xff;
		else return 0x7f;
	case DSP_ACK_16BIT:
		sb.irq.pending_16bit=false;
		break;
	case DSP_WRITE_STATUS:
		switch (sb.dsp.state) {
		case DSP_S_NORMAL:
			sb.dsp.write_busy++;
			if (sb.dsp.write_busy & 8) return 0xff;
			return 0x7f;
		case DSP_S_RESET:
		case DSP_S_RESET_WAIT:
			return 0xff;
		}
		return 0xff;
	case DSP_RESET:
		return 0xff;
	default:
		LOG(LOG_SB,LOG_NORMAL)("Unhandled read from SB Port %4X",port);
		break;
	}
	return 0xff;
}

static void write_sb(io_port_t port, io_val_t value, io_width_t)
{
	const auto val = check_cast<uint8_t>(value);
	switch (port - sb.hw.base) {
	case DSP_RESET: DSP_DoReset(val); break;
	case DSP_WRITE_DATA: DSP_DoWrite(val); break;
	case MIXER_INDEX: sb.mixer.index = val; break;
	case MIXER_DATA: CTMIXER_Write(val); break;
	default: LOG(LOG_SB, LOG_NORMAL)("Unhandled write to SB Port %4X", port); break;
	}
}

static void adlib_gusforward(io_port_t, io_val_t value, io_width_t)
{
	const auto val = check_cast<uint8_t>(value);
	adlib_commandreg = (uint8_t)(val & 0xff);
}

bool SB_Get_Address(uint16_t &sbaddr, uint8_t &sbirq, uint8_t &sbdma)
{
	sbaddr = 0;
	sbirq =0;
	sbdma =0;
	if (sb.type == SBT_NONE) return false;
	else {
		sbaddr=sb.hw.base;
		sbirq =sb.hw.irq;
		sbdma = sb.hw.dma8;
		return true;
	}
}

static void SBLASTER_CallBack(uint32_t len)
{
	switch (sb.mode) {
	case MODE_NONE:
	case MODE_DMA_PAUSE:
	case MODE_DMA_MASKED:
		sb.chan->AddSilence();
		break;
	case MODE_DAC:
//		GenerateDACSound(len);
//		break;
		if (!sb.dac.used) {
			sb.mode=MODE_NONE;
			return;
		}
		sb.chan->AddStretched(sb.dac.used,sb.dac.data);
		sb.dac.used=0;
		break;
	case MODE_DMA:
		len*=sb.dma.mul;
		if (len&SB_SH_MASK) len+=1 << SB_SH;
		len>>=SB_SH;
		if (len>sb.dma.left) len=sb.dma.left;
		ProcessDMATransfer(len);
		break;
	}
}

SB_TYPES find_sbtype()
{
	const auto sect = static_cast<Section_prop *>(control->GetSection("sblaster"));
	assert(sect);

	const std::string_view pref = sect->Get_string("sbtype");

	// Default
	auto sbtype = SB_TYPES::SBT_NONE;

	// Newest to oldest
	if (pref == "sb16") {
		sbtype = SBT_16;
	} else if (pref == "sbpro2") {
		sbtype = SBT_PRO2;
	} else if (pref == "sbpro1") {
		sbtype = SBT_PRO1;
	} else if (pref == "sb2") {
		sbtype = SBT_2;
	} else if (pref == "sb1") {
		sbtype = SBT_1;
	} else if (pref == "gb") {
		sbtype = SBT_GB;
	}
	return sbtype;
}

OplMode find_oplmode()
{
	const auto sect = static_cast<Section_prop *>(control->GetSection("sblaster"));
	assert(sect);

	const std::string_view pref = sect->Get_string("oplmode");

	// Default
	auto opl_mode = OplMode::None;

	// Newest to oldest
	if (pref == "opl3gold") {
		opl_mode = OplMode::Opl3Gold;
	} else if (pref == "opl3") {
		opl_mode = OplMode::Opl3;
	} else if (pref == "dualopl2") {
		opl_mode = OplMode::DualOpl2;
	} else if (pref == "opl2") {
		opl_mode = OplMode::Opl2;
	} else if (pref == "cms") {
		opl_mode = OplMode::Cms;
	}

	// Else assume auto
	else {
		switch (find_sbtype()) {
		case SBT_16:
		case SBT_PRO2: opl_mode = OplMode::Opl3; break;
		case SBT_PRO1: opl_mode = OplMode::DualOpl2; break;
		case SBT_2:
		case SBT_1: opl_mode = OplMode::Opl2; break;
		case SBT_GB: opl_mode = OplMode::Cms; break;
		case SBT_NONE: opl_mode = OplMode::None; break;
		}
	}
	return opl_mode;
}

void SBLASTER_Destroy(Section*);

class SBLASTER final {
private:
	/* Data */
	IO_ReadHandleObject read_handlers[0x10]   = {};
	IO_WriteHandleObject write_handlers[0x10] = {};

	static constexpr auto blaster_env_name = "BLASTER";
	OplMode oplmode = OplMode::None;

	void SetupEnvironment()
	{
		// Ensure our port and addresses will fit in our format widths.
		// The config selection controls their actual values, so this is
		// a maximum-limit.
		assert(sb.hw.base < 0xfff);
		assert(sb.hw.irq <= 12);
		assert(sb.hw.dma8 < 10);

		char blaster_env_val[] = "AHHH II DD HH TT";

		if (sb.type == SBT_16) {
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
		LOG_MSG("%s: Setting '%s' environment variable to '%s'", CardType(), blaster_env_name, blaster_env_val);
		AUTOEXEC_SetVariable(blaster_env_name, blaster_env_val);
	}

	void ClearEnvironment()
	{
		AUTOEXEC_SetVariable(blaster_env_name, "");
	}

public:
	SBLASTER(Section* conf)
	{
		assert(conf);

		Section_prop * section=static_cast<Section_prop *>(conf);

		sb.hw.base=section->Get_hex("sbbase");

		sb.hw.irq = static_cast<uint8_t>(section->Get_int("irq"));

		sb.dsp.cold_warmup_ms = check_cast<uint8_t>(section->Get_int("sbwarmup"));
		sb.dsp.hot_warmup_ms = sb.dsp.cold_warmup_ms >> 5;

		sb.mixer.enabled=section->Get_bool("sbmixer");
		sb.mixer.stereo=false;

		sb.type = find_sbtype();
		oplmode = find_oplmode();

		switch (oplmode) {
		case OplMode::None:
			write_handlers[0].Install(0x388,
			                          adlib_gusforward,
			                          io_width_t::byte);
			break;

		case OplMode::Cms:
			write_handlers[0].Install(0x388,
			                          adlib_gusforward,
			                          io_width_t::byte);
			CMS_Init(section);
			break;

		case OplMode::Opl2:
			CMS_Init(section);
			[[fallthrough]];

		case OplMode::DualOpl2:
		case OplMode::Opl3:
		case OplMode::Opl3Gold: {
			OPL_Init(section, oplmode);
			auto opl_channel = MIXER_FindChannel("OPL");
			assert(opl_channel);

			const std::string opl_filter_prefs = section->Get_string(
			        "opl_filter");
			configure_opl_filter(opl_channel, opl_filter_prefs, sb.type);
		} break;
		}

		if (sb.type == SBT_NONE || sb.type == SBT_GB)
			return;

		sb.hw.dma8 = static_cast<uint8_t>(section->Get_int("dma"));
		auto dma_channel = DMA_GetChannel(sb.hw.dma8);
		assert(dma_channel);
		dma_channel->ReserveFor(CardType(), SBLASTER_Destroy);

		// Only Sound Blaster 16 uses a 16-bit DMA channel.
		if (sb.type == SB_TYPES::SBT_16) {
			sb.hw.dma16 = static_cast<uint8_t>(section->Get_int("hdma"));

			// Reserve the second DMA channel only if it's unique.
			if (sb.hw.dma16 != sb.hw.dma8) {
				dma_channel = DMA_GetChannel(sb.hw.dma16);
				assert(dma_channel);
				dma_channel->ReserveFor(CardType(),
										SBLASTER_Destroy);
			}
		}

		std::set channel_features = {ChannelFeature::ReverbSend,
		                             ChannelFeature::ChorusSend,
		                             ChannelFeature::DigitalAudio};

		if (sb.type == SBT_PRO1 || sb.type == SBT_PRO2 || sb.type == SBT_16)
			channel_features.insert(ChannelFeature::Stereo);

		sb.chan = MIXER_AddChannel(&SBLASTER_CallBack,
		                           default_playback_rate_hz,
		                           "SB",
		                           channel_features);

		const std::string sb_filter_prefs = section->Get_string("sb_filter");

		const auto sb_filter_always_on = section->Get_bool("sb_filter_always_on");

		configure_sb_filter(sb.chan,
		                    sb_filter_prefs,
		                    sb_filter_always_on,
		                    sb.type);

		sb.dsp.state=DSP_S_NORMAL;
		sb.dsp.out.lastval=0xaa;
		sb.dma.chan=nullptr;

		for (uint8_t i = 4; i <= 0xf; ++i) {
			if (i == 8 || i == 9)
				continue;
			// Disable mixer ports for lower soundblaster
			if ((sb.type == SBT_1 || sb.type == SBT_2) && (i == 4 || i == 5))
				continue;
			read_handlers[i].Install(sb.hw.base + i,
			                         read_sb,
			                         io_width_t::byte);
			write_handlers[i].Install(sb.hw.base + i,
			                          write_sb,
			                          io_width_t::byte);
		}
		for (uint16_t i = 0; i < 256; ++i)
			ASP_regs[i] = 0;
		ASP_regs[5] = 0x01;
		ASP_regs[9] = 0xf8;

		DSP_Reset();

		CTMIXER_Reset();

		ProcessDMATransfer = &PlayDMATransfer;

		SetupEnvironment();

		// Soundblaster midi interface
		if (!MIDI_Available()) {
			sb.midi = false;
		} else {
			sb.midi = true;
		}

		if (sb.type == SB_TYPES::SBT_16) {
			LOG_MSG("%s: Running on port %xh, IRQ %d, DMA %d, and high DMA %d",
			        CardType(),
			        sb.hw.base,
			        sb.hw.irq,
			        sb.hw.dma8,
			        sb.hw.dma16);
		} else {
			LOG_MSG("%s: Running on port %xh, IRQ %d, and DMA %d",
			        CardType(),
			        sb.hw.base,
			        sb.hw.irq,
			        sb.hw.dma8);
		}
	}

	~SBLASTER()
	{
		// Prevent discovery of the Sound Blaster via the environment
		ClearEnvironment();

		// Shutdown any FM Synth devices
		switch (oplmode) {
		case OplMode::None: break;
		case OplMode::Cms: CMS_ShutDown(); break;
		case OplMode::Opl2: CMS_ShutDown(); [[fallthrough]];
		case OplMode::DualOpl2:
		case OplMode::Opl3:
		case OplMode::Opl3Gold: OPL_Destroy(); break;
		}
		if (sb.type == SBT_NONE || sb.type == SBT_GB) {
			return;
		}

		LOG_MSG("%s: Shutting down", CardType());

		// Stop playback
		if (sb.chan) {
			sb.chan->Enable(false);
		}
		// Stop the game from accessing the IO ports
		for (auto& rh : read_handlers) {
			rh.Uninstall();
		}
		for (auto& wh : write_handlers) {
			wh.Uninstall();
		}
		DSP_Reset(); // Stop everything
		sb.dsp.reset_tally = 0;

		// Deregister the mixer channel and remove it
		assert(sb.chan);
		MIXER_DeregisterChannel(sb.chan);
		sb.chan.reset();

		// Reset the DMA channels as the mixer is no longer reading samples
		DMA_ResetChannel(sb.hw.dma8);
		if (sb.type == SB_TYPES::SBT_16) {
			DMA_ResetChannel(sb.hw.dma16);
		}

		sb = {};
	}

}; // End of SBLASTER class
void SBLASTER_Configure(const ModuleLifecycle lifecycle, Section* section = nullptr)
{
	static std::unique_ptr<SBLASTER> sblaster_instance = {};

	switch (lifecycle) {
	case ModuleLifecycle::Reconfigure:
		sblaster_instance.reset();
		[[fallthrough]];
		// reconfigure through recreation

	case ModuleLifecycle::Create:
		if (!sblaster_instance) {
			sblaster_instance = std::make_unique<SBLASTER>(section);
		}
		break;

	case ModuleLifecycle::Destroy:
		sblaster_instance.reset();
		break;
	}
}

void SBLASTER_Destroy(Section* section) {
	SBLASTER_Configure(ModuleLifecycle::Destroy, section);
}

void SBLASTER_Init(Section * section) {
	SBLASTER_Configure(ModuleLifecycle::Create, section);

	constexpr auto changeable_at_runtime = true;
	section->AddDestroyFunction(&SBLASTER_Destroy, changeable_at_runtime);
}
