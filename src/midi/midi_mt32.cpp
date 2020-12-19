/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2012-2020  sergm <sergm@bigmir.net>
 *  Copyright (C) 2020       Nikos Chantziaras <realnc@gmail.com> (settings)
 *  Copyright (C) 2020       The dosbox-staging team
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
#include <string>

#include <SDL_endian.h>

#include "control.h"
#include "cross.h"
#include "fs_utils.h"
#include "midi.h"
#include "string_utils.h"

// mt32emu Settings
// ----------------
// Analogue circuit modes: DIGITAL_ONLY, COARSE, ACCURATE, OVERSAMPLED
constexpr auto ANALOG_MODE = MT32Emu::AnalogOutputMode_ACCURATE;
// DAC Emulation modes: NICE, PURE, GENERATION1, and GENERATION2
constexpr auto DAC_MODE = MT32Emu::DACInputMode_NICE;
// Render at least one video-frames worth of audio (1000 ms / 70 Hz = 14.2 ms)
constexpr uint8_t RENDER_MIN_MS = 15;
// Render up to three video-frames at most, capping latency to 45ms
constexpr uint8_t RENDER_MAX_MS = RENDER_MIN_MS * 3;
// Sample rate conversion quality: FASTEST, FAST, GOOD, BEST
constexpr auto RATE_CONVERSION_QUALITY = MT32Emu::SamplerateConversionQuality_BEST;
// Use improved amplitude ramp characteristics for sustaining instruments
constexpr bool USE_NICE_RAMP = true;
// Perform rendering in separate thread concurrent to DOSBox's 1-ms timer loop
constexpr bool USE_THREADED_RENDERING = true;

// mt32emu Constants
// -----------------
constexpr uint16_t MS_PER_S = 1000;
constexpr uint8_t CH_PER_FRAME = 2; // left and right channels


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
	        "The directory holding the required MT-32 and/or CM-32L ROMs\n"
	        "named as follows:\n"
	        "  MT32_CONTROL.ROM or CM32L_CONTROL.ROM - control ROM files(s).\n"
	        "  MT32_PCM.ROM or CM32L_PCM.ROM - PCM ROM file(s).\n"
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
                         mt32_service_ptr_t &service)
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
                          mt32_service_ptr_t &service)
{
	const std::string ctr_rom = model + "_CONTROL.ROM";
	const std::string pcm_rom = model + "_PCM.ROM";
	for (const auto &dir : rom_dirs) {
		if (load_rom_set(dir + ctr_rom, dir + pcm_rom, service)) {
			LOG_MSG("MT32: Loaded %s-model ROMs from %s",
			        model.c_str(), dir.c_str());
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

bool MidiHandler_mt32::Open(MAYBE_UNUSED const char *conf)
{
	mt32_service_ptr_t mt32_service = std::make_unique<MT32Emu::Service>();

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
	mixer_channel_ptr_t mixer_channel(MIXER_AddChannel(mixer_callback, 0, "MT32"),
	                                  MIXER_DelChannel);
	const auto sample_rate = mixer_channel->GetSampleRate();

	mt32_service->setAnalogOutputMode(ANALOG_MODE);
	mt32_service->setStereoOutputSampleRate(sample_rate);
	mt32_service->setSamplerateConversionQuality(RATE_CONVERSION_QUALITY);

	const auto rc = mt32_service->openSynth();
	if (rc != MT32EMU_RC_OK) {
		LOG_MSG("MT32: Error initialising emulation: %i", rc);
		return false;
	}

	mt32_service->setDACInputMode(DAC_MODE);
	mt32_service->setNiceAmpRampEnabled(USE_NICE_RAMP);

	if (USE_THREADED_RENDERING) {
		static_assert(RENDER_MIN_MS <= RENDER_MAX_MS, "Incorrect rendering sizes");
		static_assert(RENDER_MAX_MS <= 333, "Excessive latency, use a smaller duration");
		stopProcessing = false;
		playPos = 0;

		// If the mixer's playback thread stalls waiting for the
		// rendering thread to produce samples, then at a minimum we
		// will RENDER_MIN_MS of audio.
		minimumRenderFrames = static_cast<uint16_t>(
		        RENDER_MIN_MS * sample_rate / MS_PER_S);

		// Allow the rendering thread to synthesize up to RENDER_MAX_MS
		// of audio (to keep the buffer topped-up).
		framesPerAudioBuffer = static_cast<uint16_t>(
		        RENDER_MAX_MS * sample_rate / MS_PER_S);
		audioBuffer.resize(framesPerAudioBuffer * CH_PER_FRAME);

		// Ensure the buffer is bounded to the same type and size as the
		// mixer's primary mixing buffer
		assert(audioBuffer.size() <= UINT16_MAX &&
		       audioBuffer.size() <= MIXER_BUFSIZE);

		mt32_service->renderBit16s(audioBuffer.data(),
		                           framesPerAudioBuffer - 1);
		renderPos = (framesPerAudioBuffer - 1) * CH_PER_FRAME;
		playedBuffers = 1;
		lock.reset(SDL_CreateMutex());
		framesInBufferChanged.reset(SDL_CreateCond());
		thread.reset(SDL_CreateThread(ProcessingThread, "mt32emu", nullptr));
	}

	service = std::move(mt32_service);
	channel = std::move(mixer_channel);

	channel->Enable(true);
	open = true;
	return true;
}

void MidiHandler_mt32::Close()
{
	if (!open)
		return;
	channel->Enable(false);
	if (USE_THREADED_RENDERING) {
		stopProcessing = true;
		SDL_LockMutex(lock.get());
		SDL_CondSignal(framesInBufferChanged.get());
		SDL_UnlockMutex(lock.get());
		thread.reset();
		lock.reset();
		framesInBufferChanged.reset();
	}
	service->closeSynth();
	open = false;
}

uint32_t MidiHandler_mt32::GetMidiEventTimestamp() const
{
	const uint32_t played_frames = playedBuffers * framesPerAudioBuffer;
	const uint16_t current_frame = playPos / CH_PER_FRAME;
	return service->convertOutputToSynthTimestamp(played_frames + current_frame);
}

void MidiHandler_mt32::PlayMsg(const uint8_t *msg)
{
	const auto msg_words = reinterpret_cast<const uint32_t *>(msg);
	if (USE_THREADED_RENDERING)
		service->playMsgAt(SDL_SwapLE32(*msg_words), GetMidiEventTimestamp());
	else
		service->playMsg(SDL_SwapLE32(*msg_words));
}

void MidiHandler_mt32::PlaySysex(uint8_t *sysex, size_t len)
{
	assert(len <= UINT32_MAX);
	const auto msg_len = static_cast<uint32_t>(len);
	if (USE_THREADED_RENDERING)
		service->playSysexAt(sysex, msg_len, GetMidiEventTimestamp());
	else
		service->playSysex(sysex, msg_len);
}

int MidiHandler_mt32::ProcessingThread(MAYBE_UNUSED void *data)
{
	mt32_instance.RenderingLoop();
	return 0;
}

MidiHandler_mt32::~MidiHandler_mt32()
{
	Close();
}

void MidiHandler_mt32::MixerCallBack(uint16_t frames)
{
	if (USE_THREADED_RENDERING) {
		while (renderPos == playPos) {
			SDL_LockMutex(lock.get());
			SDL_CondWait(framesInBufferChanged.get(), lock.get());
			SDL_UnlockMutex(lock.get());
			if (stopProcessing)
				return;
		}
		uint16_t cur_render_pos = renderPos;
		uint16_t cur_play_pos = playPos;
		const uint16_t samplesReady = (cur_render_pos < cur_play_pos)
		                                      ? audioBuffer.size() - cur_play_pos
		                                      : cur_render_pos - cur_play_pos;
		if (frames > (samplesReady / CH_PER_FRAME)) {
			assert(samplesReady <= UINT16_MAX);
			frames = samplesReady / CH_PER_FRAME;
		}
		channel->AddSamples_s16(frames, audioBuffer.data() + cur_play_pos);
		cur_play_pos += (frames * CH_PER_FRAME);
		while (audioBuffer.size() <= cur_play_pos) {
			cur_play_pos -= audioBuffer.size();
			playedBuffers++;
		}
		playPos = cur_play_pos;
		cur_render_pos = renderPos;
		const uint16_t samplesFree = (cur_render_pos < cur_play_pos)
		                                     ? cur_play_pos - cur_render_pos
		                                     : audioBuffer.size() +
		                                               cur_play_pos -
		                                               cur_render_pos;
		if (minimumRenderFrames <= (samplesFree / CH_PER_FRAME)) {
			SDL_LockMutex(lock.get());
			SDL_CondSignal(framesInBufferChanged.get());
			SDL_UnlockMutex(lock.get());
		}
	} else {
		auto buffer = reinterpret_cast<int16_t *>(MixTemp);
		service->renderBit16s(buffer, frames);
		channel->AddSamples_s16(frames, buffer);
	}
}

void MidiHandler_mt32::RenderingLoop()
{
	while (!stopProcessing) {
		const uint16_t cur_render_pos = renderPos;
		const uint16_t cur_play_pos = playPos;
		uint16_t samples_to_render = 0;
		if (cur_render_pos < cur_play_pos) {
			samples_to_render = cur_play_pos - cur_render_pos -
			                    CH_PER_FRAME;
		} else {
			samples_to_render = audioBuffer.size() - cur_render_pos;
			if (cur_play_pos == 0) {
				samples_to_render -= CH_PER_FRAME;
			}
		}
		uint16_t frames_to_render = samples_to_render / CH_PER_FRAME;
		if (frames_to_render == 0 || (frames_to_render < minimumRenderFrames &&
		                              cur_render_pos < cur_play_pos)) {
			SDL_LockMutex(lock.get());
			SDL_CondWait(framesInBufferChanged.get(), lock.get());
			SDL_UnlockMutex(lock.get());
		} else {
			service->renderBit16s(audioBuffer.data() + cur_render_pos,
			                      frames_to_render);
			renderPos = (cur_render_pos + samples_to_render) %
			            audioBuffer.size();
			if (cur_render_pos == playPos) {
				SDL_LockMutex(lock.get());
				SDL_CondSignal(framesInBufferChanged.get());
				SDL_UnlockMutex(lock.get());
			}
		}
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
