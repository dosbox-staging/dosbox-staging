/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2012-2021  sergm <sergm@bigmir.net>
 *  Copyright (C) 2020-2021  Nikos Chantziaras <realnc@gmail.com> (settings)
 *  Copyright (C) 2020-2021  The DOSBox Staging Team
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

#include "midi_mt32.h"

#if C_MT32EMU

#include <cassert>
#include <deque>
#include <functional>
#include <string>

#include <SDL_endian.h>

#include "control.h"
#include "cross.h"
#include "fs_utils.h"
#include "midi.h"
#include "mixer.h"
#include "string_utils.h"

// mt32emu Settings
// ----------------

// Buffer sizes
static constexpr int FRAMES_PER_BUFFER = 1024; // synth granularity

// Analogue circuit modes: DIGITAL_ONLY, COARSE, ACCURATE, OVERSAMPLED
constexpr auto ANALOG_MODE = MT32Emu::AnalogOutputMode_ACCURATE;

// DAC Emulation modes: NICE, PURE, GENERATION1, and GENERATION2
constexpr auto DAC_MODE = MT32Emu::DACInputMode_NICE;

// Analog rendering types: BITS16S, FLOAT
constexpr auto RENDERING_TYPE = MT32Emu::RendererType_FLOAT;

// Sample rate conversion quality: FASTEST, FAST, GOOD, BEST
constexpr auto RATE_CONVERSION_QUALITY = MT32Emu::SamplerateConversionQuality_BEST;

// Use improved behavior for volume adjustments, panning, and mixing
constexpr bool USE_NICE_RAMP = true;
constexpr bool USE_NICE_PANNING = true;
constexpr bool USE_NICE_PARTIAL_MIXING = true;

MidiHandler_mt32 mt32_instance;

static void init_mt32_dosbox_settings(Section_prop &sec_prop)
{
	constexpr auto when_idle = Property::Changeable::WhenIdle;

	const char *models[] = {"auto", "cm32l", "mt32", 0};
	auto *str_prop = sec_prop.Add_string("model", when_idle, "auto");
	str_prop->Set_values(models);
	str_prop->Set_help(
	        "Model of synthesizer to use. The default (auto) prefers CM-32L\n"
	        "if both sets of ROMs are provided. For early Sierra games and Dune 2\n"
	        "it's recommended to use 'mt32', while newer games typically made\n"
	        "use of the CM-32L's extra sound effects (use 'auto' or 'cm32l')");

	str_prop = sec_prop.Add_string("romdir", when_idle, "");
	str_prop->Set_help(
	        "The directory containing one or both pairs of MT-32 and/or CM-32L ROMs.\n"
	        "The files must be named in capitals, as follows:\n"
	        "  - MT-32 ROM pair: MT32_CONTROL.ROM and MT32_PCM.ROM\n"
	        "  - CM-32L ROM pair: CM32L_CONTROL.ROM and CM32L_PCM.ROM\n"
	        "The directory can be absolute or relative, or leave it blank to\n"
	        "use the 'mt32-roms' directory in your DOSBox configuration\n"
	        "directory, followed by checking other common system locations.");
}

#if defined(WIN32)

static std::deque<std::string> get_rom_dirs()
{
	return {
	        CROSS_GetPlatformConfigDir() + "mt32-roms\\",
	        "C:\\mt32-rom-data\\",
	};
}

#elif defined(MACOSX)

static std::deque<std::string> get_rom_dirs()
{
	return {
	        CROSS_GetPlatformConfigDir() + "mt32-roms/",
	        CROSS_ResolveHome("~/Library/Audio/Sounds/MT32-Roms/"),
	        "/usr/local/share/mt32-rom-data/",
	        "/usr/share/mt32-rom-data/",
	};
}

#else

static std::deque<std::string> get_rom_dirs()
{
	// First priority is $XDG_DATA_HOME
	const char *xdg_data_home_env = getenv("XDG_DATA_HOME");
	const auto xdg_data_home = CROSS_ResolveHome(
	        xdg_data_home_env ? xdg_data_home_env : "~/.local/share");

	std::deque<std::string> dirs = {
	        xdg_data_home + "/dosbox/mt32-roms/",
	        xdg_data_home + "/mt32-rom-data/",
	};

	// Second priority are the $XDG_DATA_DIRS
	const char *xdg_data_dirs_env = getenv("XDG_DATA_DIRS");
	if (!xdg_data_dirs_env)
		xdg_data_dirs_env = "/usr/local/share:/usr/share";

	for (auto xdg_data_dir : split(xdg_data_dirs_env, ':')) {
		trim(xdg_data_dir);
		if (xdg_data_dir.empty()) {
			continue;
		}
		const auto resolved_dir = CROSS_ResolveHome(xdg_data_dir);
		dirs.emplace_back(resolved_dir + "/mt32-rom-data/");
	}

	// Third priority is $XDG_CONF_HOME, for convenience
	dirs.emplace_back(CROSS_GetPlatformConfigDir() + "mt32-roms/");

	return dirs;
}

#endif

static bool load_rom_set(const std::string &ctr_path,
                         const std::string &pcm_path,
                         MidiHandler_mt32::service_t &service)
{
	const bool paths_exist = path_exists(ctr_path) && path_exists(pcm_path);
	if (!paths_exist)
		return false;

	const bool roms_loaded = (service->addROMFile(ctr_path.c_str()) ==
	                          MT32EMU_RC_ADDED_CONTROL_ROM) &&
	                         (service->addROMFile(pcm_path.c_str()) ==
	                          MT32EMU_RC_ADDED_PCM_ROM);
	return roms_loaded;
}

static bool find_and_load(const std::string &model,
                          const std::deque<std::string> &rom_dirs,
                          MidiHandler_mt32::service_t &service)
{
	const std::string ctr_rom = model + "_CONTROL.ROM";
	const std::string pcm_rom = model + "_PCM.ROM";
	for (const auto &dir : rom_dirs) {
		if (load_rom_set(dir + ctr_rom, dir + pcm_rom, service)) {
			LOG_MSG("MT32: Found ROM pair in %s", dir.c_str());

			mt32emu_rom_info rom_info;
			service->getROMInfo(&rom_info);
			LOG_MSG("MT32: Initialized %s with %s",
			        rom_info.control_rom_description,
			        rom_info.pcm_rom_description);

			return true;
		}
	}
	return false;
}

static mt32emu_report_handler_i get_report_handler_interface()
{
	class ReportHandler {
	public:
		static mt32emu_report_handler_version getReportHandlerVersionID(mt32emu_report_handler_i)
		{
			return MT32EMU_REPORT_HANDLER_VERSION_0;
		}

		static void printDebug(MAYBE_UNUSED void *instance_data,
		                       const char *fmt,
		                       va_list list)
		{
			char msg[1024];
			safe_sprintf(msg, fmt, list);
			DEBUG_LOG_MSG("MT32: %s", msg);
		}

		static void onErrorControlROM(void *)
		{
			LOG_MSG("MT32: Couldn't open Control ROM file");
		}

		static void onErrorPCMROM(void *)
		{
			LOG_MSG("MT32: Couldn't open PCM ROM file");
		}

		static void showLCDMessage(void *, const char *message)
		{
			LOG_MSG("MT32: LCD-Message: %s", message);
		}
	};

	static const mt32emu_report_handler_i_v0 REPORT_HANDLER_V0_IMPL = {
	        ReportHandler::getReportHandlerVersionID,
	        ReportHandler::printDebug,
	        ReportHandler::onErrorControlROM,
	        ReportHandler::onErrorPCMROM,
	        ReportHandler::showLCDMessage,
	        nullptr, // explicit empty function pointers
	        nullptr,
	        nullptr,
	        nullptr,
	        nullptr,
	        nullptr,
	        nullptr,
	        nullptr,
	        nullptr,
	        nullptr};

	static const mt32emu_report_handler_i REPORT_HANDLER_I = {
	        &REPORT_HANDLER_V0_IMPL};

	return REPORT_HANDLER_I;
}

MidiHandler_mt32::MidiHandler_mt32()
        : soft_limiter("MT32", FRAMES_PER_BUFFER),
          keep_rendering(false)
{}

bool MidiHandler_mt32::Open(MAYBE_UNUSED const char *conf)
{
	Close();

	service_t mt32_service = std::make_unique<MT32Emu::Service>();

	// Check version
	uint32_t version = mt32_service->getLibraryVersionInt();
	if (version < 0x020100) {
		LOG_MSG("MT32: libmt32emu version is too old: %s",
		        mt32_service->getLibraryVersionString());
		return false;
	}

	mt32_service->createContext(get_report_handler_interface(), this);

	Section_prop *section = static_cast<Section_prop *>(
	        control->GetSection("mt32"));
	assert(section);

	// Which Roland model does the user want?
	const std::string model = section->Get_string("model");

	// Get potential ROM directories from the environment and/or system
	auto rom_dirs = get_rom_dirs();

	// Get the user's configured ROM directory; otherwise use 'mt32-roms'
	std::string preferred_dir = section->Get_string("romdir");
	if (preferred_dir.empty()) // already trimmed
		preferred_dir = "mt32-roms";
	if (preferred_dir.back() != '/' && preferred_dir.back() != '\\')
		preferred_dir += CROSS_FILESPLIT;

	// Make sure we search the user's configured directory first
	rom_dirs.emplace_front(CROSS_ResolveHome((preferred_dir)));

	// Try the CM-32L ROMs if the user's model is "auto" or "cm32l"
	bool roms_loaded = false;
	if (model != "mt32")
		roms_loaded = find_and_load("CM32L", rom_dirs, mt32_service);
	// If we need to fallback or if mt32 was selected
	if (!roms_loaded && model != "cm32l")
		roms_loaded = find_and_load("MT32", rom_dirs, mt32_service);

	if (!roms_loaded) {
		for (const auto &dir : rom_dirs) {
			LOG_MSG("MT32: Failed to load Control and PCM ROMs from '%s'",
			        dir.c_str());
		}
		return false;
	}

	const auto mixer_callback = std::bind(&MidiHandler_mt32::MixerCallBack,
	                                      this, std::placeholders::_1);
	channel_t mixer_channel(MIXER_AddChannel(mixer_callback, 0, "MT32"),
	                        MIXER_DelChannel);

	// Let the mixer command adjust the MT32's services gain-level
	const auto set_mixer_level = std::bind(&MidiHandler_mt32::SetMixerLevel,
	                                       this, std::placeholders::_1);
	mixer_channel->RegisterLevelCallBack(set_mixer_level);

	const auto sample_rate = mixer_channel->GetSampleRate();

	mt32_service->setAnalogOutputMode(ANALOG_MODE);
	mt32_service->selectRendererType(RENDERING_TYPE);
	mt32_service->setStereoOutputSampleRate(sample_rate);
	mt32_service->setSamplerateConversionQuality(RATE_CONVERSION_QUALITY);

	const auto rc = mt32_service->openSynth();
	if (rc != MT32EMU_RC_OK) {
		LOG_MSG("MT32: Error initialising emulation: %i", rc);
		return false;
	}

	mt32_service->setDACInputMode(DAC_MODE);
	mt32_service->setNiceAmpRampEnabled(USE_NICE_RAMP);
	mt32_service->setNicePanningEnabled(USE_NICE_PANNING);
	mt32_service->setNicePartialMixingEnabled(USE_NICE_PARTIAL_MIXING); 

	service = std::move(mt32_service);
	channel = std::move(mixer_channel);

	// Start rendering audio
	keep_rendering = true;
	const auto render = std::bind(&MidiHandler_mt32::Render, this);
	renderer = std::thread(render);
	play_buffer = playable.Dequeue(); // populate the first play buffer

	// Start playback
	channel->Enable(true);
	is_open = true;
	return true;
}

MidiHandler_mt32::~MidiHandler_mt32()
{
	Close();
}

// When the user runs "mixer MT32 <percent-left>:<percent-right>", this function
// get those percents as floating point ratios (100% being 1.0f). Instead of
// post-scaling the rendered integer stream in the mixer, we instead provide the
// desired floating point scalar to the MT32 service via its gain() interface
// where it can more elegantly adjust the level of the synthesis.

// Another nuance is that MT32's gain interface takes in a single float, but
// DOSBox's mixer accept left-and-right, so we apply gain using the larger of
// the two and then use the limiter's left-right ratios to scale down by lesser
// ratio.
void MidiHandler_mt32::SetMixerLevel(const AudioFrame &levels) noexcept
{
	const float gain = std::max(levels.left, levels.right);
	{
		const std::lock_guard<std::mutex> lock(service_mutex);
		if (service)
			service->setOutputGain(gain);
	}

	const AudioFrame desired = {levels.left / gain, levels.right / gain};
	// mt32emu generates floats between -1 and 1, so we ask the
	// soft limiter to scale these up to the INT16 range
	soft_limiter.UpdateLevels(desired, INT16_MAX);
}

void MidiHandler_mt32::Close()
{
	if (!is_open)
		return;

	// Stop playback
	if (channel)
		channel->Enable(false);

	// Stop rendering and drain the rings
	keep_rendering = false;
	if (!backstock.Size())
		backstock.Enqueue(std::move(play_buffer));
	while (playable.Size())
		play_buffer = playable.Dequeue();

	// Wait for the rendering thread to finish
	if (renderer.joinable())
		renderer.join();

	// Stop the synthesizer
	if (service)
		service->closeSynth();

	soft_limiter.PrintStats();

	// Reset the members
	channel.reset();
	service.reset();
	soft_limiter.Reset();
	total_buffers_played = 0;
	last_played_frame = 0;

	is_open = false;
}

uint32_t MidiHandler_mt32::GetMidiEventTimestamp() const
{
	const uint32_t played_frames = total_buffers_played * FRAMES_PER_BUFFER;
	return service->convertOutputToSynthTimestamp(played_frames +
	                                              last_played_frame);
}

void MidiHandler_mt32::PlayMsg(const uint8_t *msg)
{
	const auto msg_words = reinterpret_cast<const uint32_t *>(msg);
	const std::lock_guard<std::mutex> lock(service_mutex);
	service->playMsgAt(SDL_SwapLE32(*msg_words), GetMidiEventTimestamp());
}

void MidiHandler_mt32::PlaySysex(uint8_t *sysex, size_t len)
{
	assert(len <= UINT32_MAX);
	const auto msg_len = static_cast<uint32_t>(len);
	const std::lock_guard<std::mutex> lock(service_mutex);
	service->playSysexAt(sysex, msg_len, GetMidiEventTimestamp());
}

// The callback operates at the frame-level, steadily adding samples to the
// mixer until the requested numbers of frames is met.
void MidiHandler_mt32::MixerCallBack(uint16_t requested_frames)
{
	while (requested_frames) {
		const auto frames_to_be_played = std::min(GetRemainingFrames(),
		                                          requested_frames);
		const auto sample_offset_in_buffer = play_buffer.data() +
		                                     last_played_frame * 2;
		channel->AddSamples_s16(frames_to_be_played, sample_offset_in_buffer);
		requested_frames -= frames_to_be_played;
		last_played_frame += frames_to_be_played;
	}
}

// Returns the number of frames left to play in the buffer.
uint16_t MidiHandler_mt32::GetRemainingFrames()
{
	// If the current buffer has some frames left, then return those ...
	if (last_played_frame < FRAMES_PER_BUFFER)
		return FRAMES_PER_BUFFER - last_played_frame;

	// Otherwise put the spent buffer in backstock and get the next buffer
	backstock.Enqueue(std::move(play_buffer));
	play_buffer = playable.Dequeue();
	total_buffers_played++;
	last_played_frame = 0; // reset the frame counter to the beginning

	return FRAMES_PER_BUFFER;
}

// Keep the playable queue populated with freshly rendered buffers
void MidiHandler_mt32::Render()
{
	// Allocate our buffers once and reuse for the duration.
	constexpr auto SAMPLES_PER_BUFFER = FRAMES_PER_BUFFER * 2; // L & R
	std::vector<float> render_buffer(SAMPLES_PER_BUFFER);
	std::vector<int16_t> playable_buffer(SAMPLES_PER_BUFFER);

	// Populate the backstock using copies of the current buffer.
	while (backstock.Size() < backstock.MaxCapacity() - 1)
		backstock.Enqueue(playable_buffer);
	backstock.Enqueue(std::move(playable_buffer));
	assert(backstock.Size() == backstock.MaxCapacity());

	while (keep_rendering.load()) {
		{
			const std::lock_guard<std::mutex> lock(service_mutex);
			service->renderFloat(render_buffer.data(), FRAMES_PER_BUFFER);
		}
		// Grab the next buffer from backstock and populate it ...
		playable_buffer = backstock.Dequeue();
		soft_limiter.Process(render_buffer, FRAMES_PER_BUFFER, playable_buffer);

		// and then move it into the playable queue
		playable.Enqueue(std::move(playable_buffer));
	}
}

static void mt32_init(MAYBE_UNUSED Section *sec)
{}

void MT32_AddConfigSection(Config *conf)
{
	assert(conf);
	Section_prop *sec_prop = conf->AddSection_prop("mt32", &mt32_init);
	assert(sec_prop);
	init_mt32_dosbox_settings(*sec_prop);
}

#endif // C_MT32EMU
