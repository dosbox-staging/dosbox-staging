// SPDX-FileCopyrightText:  2002-2024 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "opl.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <queue>
#include <sys/types.h>

#include "audio/channel_names.h"
#include "audio/opl_capture.h"
#include "util/checks.h"
#include "config/config.h"
#include "cpu/cpu.h"
#include "util/math_utils.h"
#include "gui/mapper.h"
#include "hardware/memory.h"
#include "config/setup.h"
#include "util/string_utils.h"
#include "misc/support.h"

CHECK_NARROWING();

constexpr auto OplSampleRateHz = 49716;

static std::unique_ptr<Opl> opl = {};

static const char* to_string(const OplMode opl_mode)
{
	// clang-format off
	switch (opl_mode) {
	case OplMode::None:     return "None";
	case OplMode::Opl2:     return "OPL2";
	case OplMode::DualOpl2: return "DualOPL2";
	case OplMode::Opl3:     return "OPL3";
	case OplMode::Opl3Gold: return "OPL3Gold";
	case OplMode::Esfm:     return "ESFM";
	}
	return "Unknown";
	// clang-format on
}

// Initialize the OPL chip's 4-op and 2-op FM synthesis tone generators per the
// Adlib v1.51 driver's values. Games and audio players typically overwrite the
// card with their own settings however we know the following eight games by
// Silmarils rely on the card being initialized by the Adlib driver:
//
// - Boston Bomb Club (1991),
// - Bunny Bricks (1993),
// - Crystals of Arborea (1990),
// - Ishar 1 (1992),
// - Ishar 2 (1993),
// - Metal Mutant (1991),
// - Storm Master (1992), and
// - Transantartica (1993).

static constexpr void initialize_opl_tone_generators(opl3_chip& chip)
{
	// The first 9 operators are used for 4-op FM synthesis.
	for (auto four_op_generator : {0, 1, 2, 6, 7, 8, 12, 13, 14}) {
		auto& slot    = chip.slot[four_op_generator];
		slot.eg_rout  = 511;
		slot.eg_out   = 571;
		slot.eg_gen   = 3;
		slot.reg_mult = 1;
		slot.reg_ksl  = 1;
		slot.reg_tl   = 15;
		slot.reg_ar   = 15;
		slot.reg_dr   = 1;
		slot.reg_sl   = 5;
		slot.reg_rr   = 3;
		// all other non-pointer slot members are zero
	}

	// The remaining 9 operators are used for 2-op FM synthesis (or as
	// modulators for the 4-op channels).
	for (auto two_op_generator : {3, 4, 5, 9, 10, 11, 15, 16, 17}) {
		auto& slot    = chip.slot[two_op_generator];
		slot.eg_rout  = 511;
		slot.eg_out   = 511;
		slot.eg_gen   = 3;
		slot.reg_ksr  = 1;
		slot.reg_mult = 1;
		slot.reg_ar   = 15;
		slot.reg_dr   = 2;
		slot.reg_sl   = 7;
		slot.reg_rr   = 4;
		// all other non-pointer slot members are zero
	}
}

Timer::Timer(const int micros)
        : clock_interval(micros * 0.001) // interval in milliseconds
{
	SetCounter(0);
}

// Update returns with true if overflow
// Properly syncs up the start/end to current time and changing intervals
bool Timer::Update(const double time)
{
	if (enabled && (time >= trigger)) {
		// How far into the next cycle
		const double delta_time = time - trigger;

		// Sync start to last cycle
		const auto counter_mod = fmod(delta_time, counter_interval);

		start   = time - counter_mod;
		trigger = start + counter_interval;

		// Only set the overflow flag when not masked
		if (!masked) {
			overflow = true;
		}
	}
	return overflow;
}

void Timer::Reset()
{
	// On a reset make sure the start is in sync with the next cycle
	overflow = false;
}

void Timer::SetCounter(const uint8_t val)
{
	counter = val;
	// Interval for next cycle
	counter_interval = (256 - counter) * clock_interval;
}

uint8_t Timer::GetCounter() const
{
	return counter;
}

void Timer::SetMask(const bool set)
{
	masked = set;
	if (masked) {
		overflow = false;
	}
}

bool Timer::IsMasked() const
{
	return masked;
}

void Timer::Stop()
{
	enabled = false;
}

void Timer::Start(const double time)
{
	// Only properly start when not running before
	if (!enabled) {
		enabled  = true;
		overflow = false;

		// Sync start to the last clock interval
		const auto clock_mod = fmod(time, clock_interval);

		start = time - clock_mod;

		// Overflow trigger
		trigger = start + counter_interval;
	}
}

bool Timer::IsEnabled() const
{
	return enabled;
}

OplChip::OplChip() : timer0(80), timer1(320) {}

bool OplChip::Write(const io_port_t reg, const uint8_t val)
{
	// if(reg == 0x02 || reg == 0x03 || reg == 0x04)
	// LOG(LOG_MISC,LOG_ERROR)("write adlib timer %X %X",reg,val);
	switch (reg) {
	case 0x02:
		timer0.Update(PIC_FullIndex());
		timer0.SetCounter(val);
		return true;

	case 0x03:
		timer1.Update(PIC_FullIndex());
		timer1.SetCounter(val);
		return true;

	case 0x04:
		// Reset overflow in both timers
		if (val & 0x80) {
			timer0.Reset();
			timer1.Reset();
		} else {
			const auto time = PIC_FullIndex();
			if (val & 0x1) {
				timer0.Start(time);
			} else {
				timer0.Stop();
			}

			if (val & 0x2) {
				timer1.Start(time);
			} else {
				timer1.Stop();
			}

			timer0.SetMask((val & 0x40) > 0);
			timer1.SetMask((val & 0x20) > 0);
		}
		return true;
	}
	return false;
}

uint8_t OplChip::Read()
{
	const auto time = PIC_FullIndex();
	uint8_t ret = 0;

	// Overflow won't be set if a channel is masked
	if (timer0.Update(time)) {
		ret |= 0x40 | 0x80;
	}
	if (timer1.Update(time)) {
		ret |= 0x20 | 0x80;
	}
	return ret;
}

uint8_t OplChip::EsfmReadbackReg(const uint16_t reg)
{
	uint8_t ret = 0;

	switch (reg) {
	case 0x02: ret = timer0.GetCounter(); break;
	case 0x03: ret = timer1.GetCounter(); break;
	case 0x04:
		ret = timer0.IsEnabled() & 1;
		ret |= check_cast<uint8_t>((timer1.IsEnabled() & 1) << 1);
		ret |= check_cast<uint8_t>((timer1.IsMasked() & 1) << 5);
		ret |= check_cast<uint8_t>((timer1.IsMasked() & 1) << 6);
	}
	return ret;
}

void Opl::Init()
{
	opl.newm = 0;

	if (opl.mode == OplMode::Esfm) {
		ESFM_init(&esfm.chip);
	} else {
		OPL3_Reset(&opl.chip, OplSampleRateHz);

		initialize_opl_tone_generators(opl.chip);
	}

	ms_per_frame = MillisInSecond / OplSampleRateHz;

	memset(cache, 0, ARRAY_LEN(cache));

	switch (opl.mode) {
	case OplMode::Opl2: break;

	case OplMode::DualOpl2:
		// Set up OPL3 mode in the hander
		WriteReg(0x105, 1);

		// Also set it up in the cache so the capturing will start OPL3
		CacheWrite(0x105, 1);
		break;

	case OplMode::Opl3: break;

	case OplMode::Opl3Gold:
		adlib_gold = std::make_unique<AdlibGold>(OplSampleRateHz);
		break;

	case OplMode::Esfm: break;

	default:
		assertm(false,
		        format_str("Invalid OPL mode: %s", to_string(opl.mode)));
	}
}

void Opl::WriteReg(const io_port_t selected_reg, const uint8_t val)
{
	if (opl.mode == OplMode::Esfm) {
		ESFM_write_reg_buffered_fast(&esfm.chip, selected_reg, val);


	} else { // OPL
		OPL3_WriteRegBuffered(&opl.chip, selected_reg, val);
		if (selected_reg == 0x105) {
			opl.newm = selected_reg & 0x01;
		}
	}
}

io_port_t Opl::WriteAddr(const io_port_t port, const uint8_t val)
{
	if (opl.mode == OplMode::Esfm) {
		uint16_t addr;
		if (esfm.chip.native_mode) {
			ESFM_write_port(&esfm.chip,
			                check_cast<uint8_t>((port & 3) | 2),
			                val);
			return check_cast<io_port_t>(esfm.chip.addr_latch & 0x7ff);
		} else {
			addr = val;
			if ((port & 2) && (addr == 0x05 || esfm.chip.emu_newmode)) {
				addr |= 0x100;
			}
			return addr;
		}

	} else { // OPL
		io_port_t addr = val;
		if ((port & 2) && (addr == 0x05 || opl.newm)) {
			addr |= 0x100;
		}
		return addr;
	}
}

void Opl::EsfmSetLegacyMode()
{
	ESFM_write_port(&esfm.chip, 0, 0);
}

template <LineIndex line_index>
int16_t remove_dc_bias(const int16_t back_sample)
{
	// Calculate the number of samples we need average across to maintain
	// the lowest frequency given an assumed playback rate.
	constexpr auto PcmPlaybackRateHz      = 16000;
	constexpr auto LowestFreqToMaintainHz = 200;
	constexpr auto NumToAverage = PcmPlaybackRateHz / LowestFreqToMaintainHz;

	static int sum = 0;

	static std::queue<int16_t> samples = {};

	// Clear the queue if the stream isn't biased
	constexpr int16_t BiasThreshold = 5;
	if (back_sample < BiasThreshold) {
		sum     = 0;
		samples = {};
		return back_sample;
	}

	// Keep a running sum and push the sample to the back of the queue
	sum += back_sample;
	samples.push(back_sample);

	int16_t average      = 0;
	int16_t front_sample = 0;
	if (samples.size() == NumToAverage) {
		// Compute the average and deduct it from the front sample
		average      = static_cast<int16_t>(sum / NumToAverage);
		front_sample = samples.front();
		sum -= front_sample;
		samples.pop();
	}
	return static_cast<int16_t>(front_sample - average);
}

AudioFrame Opl::RenderFrame()
{
	static int16_t buf[2] = {};

	if (opl.mode == OplMode::Esfm) {
		ESFM_generate_stream(&esfm.chip, buf, 1);

		if (ctrl.wants_dc_bias_removed) {
			buf[0] = remove_dc_bias<Left>(buf[0]);
			buf[1] = remove_dc_bias<Right>(buf[1]);
		}

		AudioFrame frame = {buf[0], buf[1]};
		return frame;

	} else { // OPL
		OPL3_GenerateStream(&opl.chip, buf, 1);

		if (ctrl.wants_dc_bias_removed) {
			buf[0] = remove_dc_bias<Left>(buf[0]);
			buf[1] = remove_dc_bias<Right>(buf[1]);
		}

		AudioFrame frame = {};
		if (adlib_gold) {
			adlib_gold->Process(buf, 1, &frame[0]);
		} else {
			frame.left  = buf[0];
			frame.right = buf[1];
		}
		return frame;
	}
}

void Opl::RenderUpToNow()
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
		fifo.emplace(RenderFrame());
	}
}

void Opl::AudioCallback(const int requested_frames)
{
	std::lock_guard lock(mutex);
	assert(channel);
#if 0
	if (fifo.size()) {
		LOG_MSG("%s: Queued %2lu cycle-accurate frames",
		        channel->GetName().c_str(),
		        fifo.size());
	}
#endif
	auto frames_remaining = requested_frames;

	// First, send any frames we've queued since the last callback
	while (frames_remaining && fifo.size()) {
		channel->AddSamples_sfloat(1, &fifo.front()[0]);
		fifo.pop();
		--frames_remaining;
	}
	// If the queue's run dry, render the remainder and sync-up our time datum
	while (frames_remaining) {
		const auto frame = RenderFrame();
		channel->AddSamples_sfloat(1, &frame[0]);
		--frames_remaining;
	}
	last_rendered_ms = PIC_AtomicIndex();
}

void Opl::CacheWrite(const io_port_t port, const uint8_t val)
{
	// capturing?
	if (capture) {
		capture->DoWrite(port, val);
	}

	// Store it into the cache
	cache[port] = val;
}

void Opl::DualWrite(const uint8_t index, const uint8_t port, const uint8_t value)
{
	// Make sure we don't use OPL3 features
	// Don't allow write to disable OPL3
	if (port == 5) {
		return;
	}

	// Only allow 4 waveforms
	auto val = value;
	if (port >= 0xe0) {
		val &= 3;
	}

	// Write to the timer?
	if (chip[index].Write(port, val)) {
		return;
	}

	// Enabling panning
	if (port >= 0xc0 && port <= 0xc8) {
		val &= 0x0f;
		val |= check_cast<uint8_t>(index ? 0xA0 : 0x50);
	}
	const auto full_port = check_cast<io_port_t>(port + (index ? 0x100 : 0u));
	WriteReg(full_port, val);
	CacheWrite(full_port, val);
}

void Opl::AdlibGoldControlWrite(const uint8_t val)
{
	switch (ctrl.index) {
	case 0x04:
		adlib_gold->StereoControlWrite(StereoProcessorControlReg::VolumeLeft,
		                               val);
		break;
	case 0x05:
		adlib_gold->StereoControlWrite(StereoProcessorControlReg::VolumeRight,
		                               val);
		break;
	case 0x06:
		adlib_gold->StereoControlWrite(StereoProcessorControlReg::Bass, val);
		break;

	case 0x07:
		adlib_gold->StereoControlWrite(StereoProcessorControlReg::Treble, val);
		break;

	case 0x08:
		adlib_gold->StereoControlWrite(StereoProcessorControlReg::SwitchFunctions,
		                               val);
		break;

	case 0x09: // Left FM Volume
		ctrl.lvol = val;
		goto setvol;

	case 0x0a: // Right FM Volume
		ctrl.rvol = val;

	setvol:
		if (ctrl.mixer_enabled) {
			// Dune CD version uses 32 volume steps in an apparent
			// mistake, should be 128
			channel->SetAppVolume(
			        {static_cast<float>(ctrl.lvol & 0x1f) / 31.0f,
			         static_cast<float>(ctrl.rvol & 0x1f) / 31.0f});
		}
		break;

	case 0x18: // Surround
		adlib_gold->SurroundControlWrite(val);
	}
}

uint8_t Opl::AdlibGoldControlRead()
{
	switch (ctrl.index) {
	case 0x00: // Board Options
	           // 16-bit ISA, surround module, no telephone/CDROM
		return 0x50;

		// 16-bit ISA, no telephone/surround/CD-ROM
		// return 0x70;

	case 0x09: // Left FM Volume
		return ctrl.lvol;

	case 0x0a: // Right FM Volume
		return ctrl.rvol;

	case 0x15: // Audio Relocation
		return 0x388 >> 3; // Cryo installer detection
	}
	return 0xff;
}

void Opl::PortWrite(const io_port_t port, const io_val_t value, const io_width_t)
{
	std::lock_guard lock(mutex);
	RenderUpToNow();

	const auto val = check_cast<uint8_t>(value);

	if (opl.mode == OplMode::Esfm && esfm.mode == EsfmMode::Native) {
		switch (port & 3) {
		case 0:
			// Disable native mode
			EsfmSetLegacyMode();
			esfm.mode = EsfmMode::Legacy;
			break;

		case 1:
			if ((reg.normal & 0x500) == 0x400) {
				// Emulation mode register pokehole region at
				// 0x400 (mirrored at 0x600)
				if (!chip[0].Write(reg.normal & 0xff, val)) {
					WriteReg(reg.normal, val);
				}
			} else {
				WriteReg(reg.normal, val);
			}
			// TODO Capture for ESFM native mode? It's
			// complicated...
			// CacheWrite(reg.normal, val);
			break;

		case 2:
		case 3: reg.normal = WriteAddr(port, val) & 0x7ff; break;
		}
		return;
	}

	if (port & 1) {
		switch (opl.mode) {
		case OplMode::Opl3Gold:
			if (port == 0x38b) {
				if (ctrl.active) {
					AdlibGoldControlWrite(val);
					break;
				}
			}
			[[fallthrough]];

		case OplMode::Opl2:
		case OplMode::Opl3:
			if (!chip[0].Write(reg.normal, val)) {
				WriteReg(reg.normal, val);
				CacheWrite(reg.normal, val);
			}
			break;

		case OplMode::DualOpl2:
			// Not a 0x??8 port, then write to a specific port
			if (!(port & 0x8)) {
				uint8_t index = (port & 2) >> 1;
				DualWrite(index, reg.dual[index], val);
			} else {
				// Write to both ports
				DualWrite(0, reg.dual[0], val);
				DualWrite(1, reg.dual[1], val);
			}
			break;

		case OplMode::Esfm:
			if (!chip[0].Write(reg.normal, val)) {
				if (reg.normal == 0x105 && (val & 0x80)) {
					esfm.mode = EsfmMode::Native;

					if (capture) {
						LOG_WARNING(
						        "OPL: ESFM native mode has been enabled "
						        "which is not supported by the raw OPL "
						        "capture feature.");
					}
				}
				WriteReg(reg.normal & 0x1ff, val);
				CacheWrite(reg.normal & 0x1ff, val);
			}
			break;

		default:
			assertm(false,
			        format_str("Invalid OPL mode: %s",
			                   to_string(opl.mode)));
		}
	} else {
		// Ask the handler to write the address
		// Make sure to clip them in the right range
		switch (opl.mode) {
		case OplMode::Opl2:
			reg.normal = WriteAddr(port, val) & 0xff;
			break;

		case OplMode::DualOpl2:
			// Not a 0x?88 port, when write to a specific side
			if (!(port & 0x8)) {
				uint8_t index   = (port & 2) >> 1;
				reg.dual[index] = val & 0xff;
			} else {
				reg.dual[0] = val & 0xff;
				reg.dual[1] = val & 0xff;
			}
			break;

		case OplMode::Opl3Gold:
			if (port == 0x38a) {
				if (val == 0xff) {
					ctrl.active = true;
					break;
				} else if (val == 0xfe) {
					ctrl.active = false;
					break;
				} else if (ctrl.active) {
					ctrl.index = val & 0xff;
					break;
				}
			}
			[[fallthrough]];

		case OplMode::Opl3:
		case OplMode::Esfm:
			reg.normal = WriteAddr(port, val) & 0x1ff;
			break;

		default:
			assertm(false,
			        format_str("Invalid OPL mode: %s",
			                   to_string(opl.mode)));
		}

		// Pass the command value onto the GUS (regardless of OPL card type)
		if (port == Port::AdLib::Command) {
			GUS_MirrorAdLibCommandPortWrite(port, val);
		}
	}
}

uint8_t Opl::PortRead(const io_port_t port, const io_width_t)
{
	// Roughly half a microsecond (as we already do 1 us on each port read
	// and some tests revealed it taking 1.5 us to read an AdLib port).
	auto delaycyc = (CPU_CycleMax / 2048);
	if (delaycyc > CPU_Cycles) {
		delaycyc = CPU_Cycles;
	}

	CPU_Cycles -= delaycyc;
	CPU_IODelayRemoved += delaycyc;

	switch (opl.mode) {
	case OplMode::Opl2:
		// We allocated 4 ports, so just return -1 for the higher ones.
		if (!(port & 3)) {
			// Make sure the low bits are 6 on OPL2
			return chip[0].Read() | 0x6;
		} else {
			return 0xff;
		}

	case OplMode::DualOpl2:
		// Only return for the lower ports
		if (port & 1) {
			return 0xff;
		}
		// Make sure the low bits are 6 on opl2
		return chip[(port >> 1) & 1].Read() | 0x6;

	case OplMode::Opl3Gold:
		if (ctrl.active) {
			if (port == 0x38a) {
				// Control status, not busy
				return 0;
			} else if (port == 0x38b) {
				return AdlibGoldControlRead();
			}
		}
		[[fallthrough]];

	case OplMode::Opl3:
		// We allocated 4 ports, so just return -1 for the higher ones
		if (!(port & 3)) {
			return chip[0].Read();
		} else {
			return 0xff;
		}

	case OplMode::Esfm:
		switch (port & 3) {
		case 0: return chip[0].Read();
		case 1:
			if (esfm.mode == EsfmMode::Native) {
				if ((reg.normal & 0x500) == 0x400) {
					// Emulation mode register pokehole
					// region at 0x400 (mirrored at 0x600)
					return chip[0].EsfmReadbackReg(
					        reg.normal & 0xff);
				}
				return ESFM_readback_reg(&esfm.chip, reg.normal);
			} else {
				return 0x00;
			}
		case 2:
		case 3: return 0xff;
		}
		break;

	default:
		assertm(false,
		        format_str("Invalid OPL mode: %s", to_string(opl.mode)));
	}
	return 0;
}

static void OPL_SaveRawEvent(const bool pressed)
{
	if (!pressed) {
		return;
	}

//	OPLCAPTURE_SaveRad(&opl->cache);

	if (!opl) {
		LOG_WARNING(
		        "OPL: Can't capture the OPL stream because "
		        "the OPL device is unavailable");
		return;
	}

	// Are we already recording? If so, close the stream
	if (opl->capture) {
		opl->capture.reset();

	} else {
		// Otherwise start a new recording
		opl->capture = std::make_unique<OplCapture>(&opl->cache);
	}
}

Opl::Opl(Section* configuration, const OplMode _opl_mode)
{
	assert(_opl_mode != OplMode::None);

	MIXER_LockMixerThread();

	opl.mode = _opl_mode;

	SectionProp* section = static_cast<SectionProp*>(configuration);
	const auto base = static_cast<uint16_t>(section->GetHex("sbbase"));

	ctrl.mixer_enabled = section->GetBool("sbmixer");

	std::set channel_features = {ChannelFeature::Sleep,
	                             ChannelFeature::FadeOut,
	                             ChannelFeature::NoiseGate,
	                             ChannelFeature::ReverbSend,
	                             ChannelFeature::ChorusSend,
	                             ChannelFeature::Synthesizer};

	const auto dual_opl = opl.mode != OplMode::Opl2;

	if (dual_opl) {
		channel_features.emplace(ChannelFeature::Stereo);
	}

	const auto mixer_callback = std::bind(&Opl::AudioCallback,
	                                      this,
	                                      std::placeholders::_1);

	// Register the audio channel
	channel = MIXER_AddChannel(mixer_callback,
	                           OplSampleRateHz,
	                           ChannelName::Opl,
	                           channel_features);

	channel->SetResampleMethod(ResampleMethod::Resample);

	// We're generating the samples in the full 16-bit value range, so this
	// effectively just adds a 1.5x gain factor.
	//
	// Used to be 2.0, which was measured to be too high. Exact value
	// depends on card/clone.
	//
	// Please don't touch this value *EVER* again as many people fine-tune
	// their mixer volumes per game, so changing this would break their
	// settings. The value cannot be "improved"; there's simply no
	// universally "good" setting that would work well in all games in
	// existence.
	//
	constexpr auto OplVolumeGain = 1.5f;
	channel->Set0dbScalar(OplVolumeGain);

	// This gets rid of the residual noise which is in the [-8, 0] range
	// on the OPL2, and in the [-18, 0] range on the OPL3 (in absolute
	// unscaled 16-bit sample values).
	// 
	// This is accurate hardware behaviour, but pretty annoying to people
	// with sensitive hearing. The OPL chips use bitwise inversion to
	// negate operator output for the negative part of sine, so a small
	// oscillation between 0 and -1 can happen even when envelope generator
	// is muted.
	//
	// Non-exclusive list of affected games:
	//
	// - 1st Degree, The (Win 3.x game; when playing either test song in
	//   the MIDI Setup)
	// - Beneath A Steel Sky (right from the start and during the intro)
	// - Doom E2M2 music (in the quiet parts)
	// - Gateway (audible after exiting to DOS)
	// - Gateway II: Homeworld (audible after exiting to DOS)
	// - Gods (after starting the game)
	// - Passport to Adventure / Monkey Island demo (after the startup music
	//   is finished)
	// - Tetris Classic (audible after the level music finished playing)
	// - Wizardry 6
	//
	// This gate threshold is fine-tuned to get rid of both [-9, 0] OPL2
	// and [-18, 0] OPL3 noise while leaving very low level signals largely
	// intact (they 100 ms release time is a key factor in achieving that).
	// 
	const auto threshold_db = -65.0f + gain_to_decibel(OplVolumeGain);
	constexpr auto AttackTimeMs  = 1.0f;
	constexpr auto ReleaseTimeMs = 100.0f;
	channel->ConfigureNoiseGate(threshold_db, AttackTimeMs, ReleaseTimeMs);

	const auto denoiser_enabled = get_mixer_section()->GetBool("denoiser");
	channel->EnableNoiseGate(denoiser_enabled);

	// Setup fadeout
	if (!channel->ConfigureFadeOut(section->GetString("opl_fadeout"))) {
		set_section_property_value("sblaster", "opl_fadeout", "off");
	}

	ctrl.wants_dc_bias_removed = section->GetBool("opl_remove_dc_bias");
	if (ctrl.wants_dc_bias_removed) {
		LOG_MSG("%s: DC bias removal enabled", channel->GetName().c_str());
	}

	Init();

	using namespace std::placeholders;

	const auto read_from = std::bind(&Opl::PortRead, this, _1, _2);
	const auto write_to  = std::bind(&Opl::PortWrite, this, _1, _2, _3);

	// 0x388-0x38b ports (read/write)
	WriteHandler[0].Install(Port::AdLib::Command, write_to, io_width_t::byte, 4);
	ReadHandler[0].Install(Port::AdLib::Command, read_from, io_width_t::byte, 4);

	// 0x220-0x223 ports (read/write)
	if (dual_opl) {
		WriteHandler[1].Install(base, write_to, io_width_t::byte, 4);
		ReadHandler[1].Install(base, read_from, io_width_t::byte, 4);
	}
	// 0x228-0x229 ports (write)
	WriteHandler[2].Install(base + 8u, write_to, io_width_t::byte, 2);

	// 0x228 port (read)
	ReadHandler[2].Install(base + 8u, read_from, io_width_t::byte, 1);

	MAPPER_AddHandler(OPL_SaveRawEvent, SDL_SCANCODE_UNKNOWN, 0, "caprawopl", "Rec. OPL");

	LOG_MSG("%s: Running %s on ports %xh and %xh",
	        channel->GetName().c_str(),
	        to_string(opl.mode),
	        base,
	        Port::AdLib::Command);

	MIXER_UnlockMixerThread();
}

Opl::~Opl()
{
	LOG_MSG("%s: Shutting down %s", channel->GetName().c_str(), to_string(opl.mode));

	MIXER_LockMixerThread();

	// Stop playback
	if (channel) {
		channel->Enable(false);
	}

	// Stop the game from accessing the IO ports
	for (auto& rh : ReadHandler) {
		rh.Uninstall();
	}
	for (auto& wh : WriteHandler) {
		wh.Uninstall();
	}

	// Deregister the mixer channel, after which it's cleaned up
	assert(channel);
	MIXER_DeregisterChannel(channel);

	MIXER_UnlockMixerThread();
}

static void init_opl_dosbox_settings(SectionProp& secprop)
{
	constexpr auto deprecated = Property::Changeable::Deprecated;
	constexpr auto when_idle  = Property::Changeable::WhenIdle;

	auto pint = secprop.AddInt("oplrate", deprecated, false);
	pint->SetHelp("The OPL output is now transparently resampled to the mixer's sample rate.");

	auto pstring = secprop.AddString("oplmode", when_idle, "auto");
	pstring->SetValues(
	        {"auto", "cms", "opl2", "dualopl2", "opl3", "opl3gold", "esfm", "none"});
	pstring->SetHelp(
	        "OPL model to emulate ('auto' by default).\n"
	        "  auto:      Use the appropriate model determined by 'sbtype'.\n"
	        "  opl2:      Yamaha OPL2 (YM3812, mono).\n"
	        "  dualopl2:  Dual OPL2 (two OPL2 chips in stereo configuration).\n"
	        "  opl3:      Yamaha OPL3 (YMF262, stereo).\n"
	        "  opl3gold:  OPL3 and the optional AdLib Gold Surround module.\n"
	        "             Use with 'sbtype = sb16' to emulate the AdLib Gold 1000.\n"
	        "  esfm:      ESS ESFM (enhanced Yamaha OPL3 compatible FM synth).\n"
	        "  none/off:  Disable OPL emulation.\n"
	        "Notes:\n"
	        "  - 'sbtype = none' and 'oplmode = opl2' emulates the original AdLib card.\n"
	        "  - Only 'oplmode = esfm' is not enough to get ESS Enhanced FM music in games;\n"
	        "    you'll also need to set 'sbtype = ess'. 'oplmode = esfm' is useful to\n"
	        "    get ESFM-flavoured OPL with original Sound Blaster models.");

	pstring = secprop.AddString("opl_fadeout", when_idle, "off");
	pstring->SetHelp(
	        "Fade out hanging notes on the OPL synth:\n"
	        "  off:       Don't fade out hanging notes (default).\n"
	        "  fade:      Fade out hanging notes. You should only enable this in games that\n"
	        "             sometimes play hanging notes that never stop (e.g., Bard's Tale).\n"
	        "  <custom>:  A custom fade-out definition in the following format:\n"
	        "               WAIT FADE\n"
	        "             Where WAIT is how long after the last I/O port write fading begins\n"
	        "             (between 100 and 5000 milliseconds); and FADE is the fade-out\n"
	        "             period (between 10 and 3000 milliseconds).\n"
	        "             Examples:\n"
	        "               300 200   (wait 300 ms before fading out over a 200 ms period)\n"
	        "               1000 3000 (wait 1 second before fading out over 3 seconds)");

	auto pbool = secprop.AddBool("opl_remove_dc_bias", when_idle, false);
	pbool->SetHelp(
	        "Remove DC bias from the OPL output. This should only be used as a last resort\n"
	        "to fix popping in games that play PCM audio using the OPL synthesiser on a\n"
	        "Sound Blaster or AdLib card, such as in: Golden Eagle (1991), Wizardry 6\n"
	        "(1990), and Wizardry 7 (1992). Please open an issue ticket if you find other\n"
	        "affected games.");

	pstring = secprop.AddString("oplemu", deprecated, "");
	pstring->SetHelp("Only 'nuked' OPL emulation is supported now.");

	pstring = secprop.AddString("opl_filter", when_idle, "auto");
	pstring->SetHelp(
	        "Type of filter to emulate for the Sound Blaster OPL output:\n"
	        "  auto:      Use the appropriate filter determined by 'sbtype' (default).\n"
	        "  sb1, sb2, sbpro1, sbpro2, sb16:\n"
	        "             Use the filter of this Sound Blaster model.\n"
	        "  off:       Don't filter the output.\n"
	        "  <custom>:  Custom filter definition; see 'sb_filter' for details.");

	pstring = secprop.AddString("cms", when_idle, "auto");
	pstring->SetValues({"on", "off", "auto"});
	pstring->SetHelp(
	        "Enable CMS emulation ('auto' by default).\n"
	        "  off:   Disable CMS emulation (except when the Game Blaster is selected).\n"
	        "  on:    Enable CMS emulation on Sound Blaster 1 and 2.\n"
	        "  auto:  Auto-enable CMS emulation for Sound Blaster 1 and Game Blaster.");

	pstring = secprop.AddString("cms_filter", when_idle, "on");
	pstring->SetHelp(
	        "Filter for the Sound Blaster CMS output:\n"
	        "  on:        Filter the output (default).\n"
	        "  off:       Don't filter the output.\n"
	        "  <custom>:  Custom filter definition; see 'sb_filter' for details.");
}

void OPL_ShutDown([[maybe_unused]] Section* sec)
{
	opl = {};
}

void OPL_Init(Section* sec, const OplMode oplmode)
{
	assert(sec);
	opl = std::make_unique<Opl>(sec, oplmode);

	constexpr auto changeable_at_runtime = true;
	sec->AddDestroyFunction(&OPL_ShutDown, changeable_at_runtime);
}

// Must be called after SB_AddConfigSection
void OPL_AddConfigSettings(const ConfigPtr& conf)
{
	assert(conf);
	auto secprop = static_cast<SectionProp*>(conf->GetSection("sblaster"));

	assert(secprop);
	init_opl_dosbox_settings(*secprop);
}
