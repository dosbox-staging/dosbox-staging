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

#include <SDL_endian.h>

#include "control.h"
#include "string_utils.h"

#ifndef DOSBOX_MIDI_H
#include "midi.h"
#endif

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
	auto *str_prop = sec_prop.Add_string("romdir", when_idle, "");
	str_prop->Set_help(
	        "The directory holding the required MT-32 Control and PCM ROMs.\n"
	        "The ROM files should be named as follows:\n"
	        "  MT32_CONTROL.ROM or CM32L_CONTROL.ROM - control ROM file.\n"
	        "  MT32_PCM.ROM or CM32L_PCM.ROM - PCM ROM file.");
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
			char s[1024];
			safe_sprintf(s, fmt, list);
			DEBUG_LOG_MSG("MT32: %s", s);
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

// TODO: use std::strings
static void make_rom_path(char pathName[],
                          const char romDir[],
                          const char fileName[],
                          bool addPathSeparator)
{
	strcpy(pathName, romDir);
	if (addPathSeparator)
		strcat(pathName, "/");
	strcat(pathName, fileName);
}

bool MidiHandler_mt32::Open(MAYBE_UNUSED const char *conf)
{
	service = new MT32Emu::Service();
	uint32_t version = service->getLibraryVersionInt();
	if (version < 0x020100) {
		delete service;
		service = nullptr;
		LOG_MSG("MT32: libmt32emu version is too old: %s",
		        service->getLibraryVersionString());
		return false;
	}
	service->createContext(get_report_handler_interface(), this);
	mt32emu_return_code rc;

	Section_prop *section = static_cast<Section_prop *>(
	        control->GetSection("mt32"));
	const char *romDir = section->Get_string("romdir");
	if (romDir == nullptr)
		romDir = "./"; // Paranoid check, should never happen
	size_t romDirLen = strlen(romDir);
	bool addPathSeparator = false;
	if (romDirLen < 1) {
		romDir = "./";
	} else if (4080 < romDirLen) {
		LOG_MSG("MT32: mt32.romdir is too long, using the current dir.");
		romDir = "./";
	} else {
		char lastChar = romDir[strlen(romDir) - 1];
		addPathSeparator = lastChar != '/' && lastChar != '\\';
	}

	char pathName[4096];

	make_rom_path(pathName, romDir, "CM32L_CONTROL.ROM", addPathSeparator);
	if (MT32EMU_RC_ADDED_CONTROL_ROM != service->addROMFile(pathName)) {
		make_rom_path(pathName, romDir, "MT32_CONTROL.ROM", addPathSeparator);
		if (MT32EMU_RC_ADDED_CONTROL_ROM != service->addROMFile(pathName)) {
			delete service;
			service = nullptr;
			LOG_MSG("MT32: Control ROM file not found");
			return false;
		}
	}
	make_rom_path(pathName, romDir, "CM32L_PCM.ROM", addPathSeparator);
	if (MT32EMU_RC_ADDED_PCM_ROM != service->addROMFile(pathName)) {
		make_rom_path(pathName, romDir, "MT32_PCM.ROM", addPathSeparator);
		if (MT32EMU_RC_ADDED_PCM_ROM != service->addROMFile(pathName)) {
			delete service;
			service = nullptr;
			LOG_MSG("MT32: PCM ROM file not found");
			return false;
		}
	}

	const auto mixer_callback = std::bind(&MidiHandler_mt32::MixerCallBack,
	                                      this, std::placeholders::_1);
	chan = MIXER_AddChannel(mixer_callback, 0, "MT32");
	assert(chan);
	const auto sample_rate = chan->GetSampleRate();

	service->setAnalogOutputMode(ANALOG_MODE);
	service->setStereoOutputSampleRate(sample_rate);
	service->setSamplerateConversionQuality(RATE_CONVERSION_QUALITY);

	if (MT32EMU_RC_OK != (rc = service->openSynth())) {
		delete service;
		service = nullptr;
		MIXER_DelChannel(chan);
		chan = nullptr;
		LOG_MSG("MT32: Error initialising emulation: %i", rc);
		return false;
	}

	service->setDACInputMode(DAC_MODE);
	service->setNiceAmpRampEnabled(USE_NICE_RAMP);

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

		audioBufferSize = framesPerAudioBuffer * CH_PER_FRAME;
		audioBuffer = new int16_t[audioBufferSize];
		service->renderBit16s(audioBuffer, framesPerAudioBuffer - 1);
		renderPos = (framesPerAudioBuffer - 1) * CH_PER_FRAME;
		playedBuffers = 1;
		lock = SDL_CreateMutex();
		framesInBufferChanged = SDL_CreateCond();
		thread = SDL_CreateThread(ProcessingThread, "mt32emu", nullptr);
	}
	chan->Enable(true);

	open = true;
	return true;
}

void MidiHandler_mt32::Close()
{
	if (!open)
		return;
	chan->Enable(false);
	if (USE_THREADED_RENDERING) {
		stopProcessing = true;
		SDL_LockMutex(lock);
		SDL_CondSignal(framesInBufferChanged);
		SDL_UnlockMutex(lock);
		SDL_WaitThread(thread, nullptr);
		thread = nullptr;
		SDL_DestroyMutex(lock);
		lock = nullptr;
		SDL_DestroyCond(framesInBufferChanged);
		framesInBufferChanged = nullptr;
		delete[] audioBuffer;
		audioBuffer = nullptr;
	}
	MIXER_DelChannel(chan);
	chan = nullptr;
	service->closeSynth();
	delete service;
	service = nullptr;
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

void MidiHandler_mt32::MixerCallBack(uint16_t len)
{
	if (USE_THREADED_RENDERING) {
		while (renderPos == playPos) {
			SDL_LockMutex(lock);
			SDL_CondWait(framesInBufferChanged, lock);
			SDL_UnlockMutex(lock);
			if (stopProcessing)
				return;
		}
		uint16_t renderPosSnap = renderPos;
		uint16_t playPosSnap = playPos;
		const uint16_t samplesReady = (renderPosSnap < playPosSnap)
		                                      ? audioBufferSize - playPosSnap
		                                      : renderPosSnap - playPosSnap;
		if (len > (samplesReady / CH_PER_FRAME)) {
			assert(samplesReady <= UINT16_MAX);
			len = samplesReady / CH_PER_FRAME;
		}
		chan->AddSamples_s16(len, audioBuffer + playPosSnap);
		playPosSnap += (len * CH_PER_FRAME);
		while (audioBufferSize <= playPosSnap) {
			playPosSnap -= audioBufferSize;
			playedBuffers++;
		}
		playPos = playPosSnap;
		renderPosSnap = renderPos;
		const uint16_t samplesFree = (renderPosSnap < playPosSnap)
		                                     ? playPosSnap - renderPosSnap
		                                     : audioBufferSize + playPosSnap - renderPosSnap;
		if (minimumRenderFrames <= (samplesFree / CH_PER_FRAME)) {
			SDL_LockMutex(lock);
			SDL_CondSignal(framesInBufferChanged);
			SDL_UnlockMutex(lock);
		}
	} else {
		auto buffer = reinterpret_cast<int16_t *>(MixTemp);
		service->renderBit16s(buffer, len);
		chan->AddSamples_s16(len, buffer);
	}
}

void MidiHandler_mt32::RenderingLoop()
{
	while (!stopProcessing) {
		const uint16_t renderPosSnap = renderPos;
		const uint16_t playPosSnap = playPos;
		uint16_t samplesToRender = 0;
		if (renderPosSnap < playPosSnap) {
			samplesToRender = playPosSnap - renderPosSnap - CH_PER_FRAME;
		} else {
			samplesToRender = audioBufferSize - renderPosSnap;
			if (playPosSnap == 0)
				samplesToRender -= CH_PER_FRAME;
		}
		uint16_t framesToRender = samplesToRender / CH_PER_FRAME;
		if ((framesToRender == 0) || ((framesToRender < minimumRenderFrames) &&
		                              (renderPosSnap < playPosSnap))) {
			SDL_LockMutex(lock);
			SDL_CondWait(framesInBufferChanged, lock);
			SDL_UnlockMutex(lock);
		} else {
			service->renderBit16s(audioBuffer + renderPosSnap,
			                      framesToRender);
			renderPos = (renderPosSnap + samplesToRender) % audioBufferSize;
			if (renderPosSnap == playPos) {
				SDL_LockMutex(lock);
				SDL_CondSignal(framesInBufferChanged);
				SDL_UnlockMutex(lock);
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
