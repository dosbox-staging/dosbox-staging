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

#include <SDL_thread.h>
#include <SDL_endian.h>

#include "control.h"
#include "string_utils.h"

#ifndef DOSBOX_MIDI_H
#include "midi.h"
#endif

MidiHandler_mt32 mt32_instance;

static const Bitu MILLIS_PER_SECOND = 1000;

static void init_mt32_dosbox_settings(Section_prop &sec_prop)
{
	constexpr auto when_idle = Property::Changeable::WhenIdle;

	auto *str_prop = sec_prop.Add_string("romdir", when_idle, "");
	str_prop->Set_help(
	        "The directory holding the required MT-32 Control and PCM ROMs.\n"
	        "The ROM files should be named as follows:\n"
	        "  MT32_CONTROL.ROM or CM32L_CONTROL.ROM - control ROM file.\n"
	        "  MT32_PCM.ROM or CM32L_PCM.ROM - PCM ROM file.");

	auto *bool_prop = sec_prop.Add_bool("reverse.stereo", when_idle, false);
	bool_prop->Set_help("Reverse stereo channels for MT-32 output");

	bool_prop = sec_prop.Add_bool("verbose", when_idle, false);
	bool_prop->Set_help("MT-32 debug logging");

	bool_prop = sec_prop.Add_bool("thread", when_idle, false);
	bool_prop->Set_help("MT-32 rendering in separate thread");

	auto *int_prop = sec_prop.Add_int("chunk", when_idle, 16);
	int_prop->SetMinMax(2, 100);
	int_prop->Set_help(
	        "Minimum milliseconds of data to render at once. (min 2, max 100)\n"
	        "Increasing this value reduces rendering overhead which may improve"
			" performance but also increases audio lag.\n"
	        "Valid for rendering in separate thread only.");

	int_prop = sec_prop.Add_int("prebuffer", when_idle, 32);
	int_prop->SetMinMax(3, 200);
	int_prop->Set_help(
	        "How many milliseconds of data to render ahead. (min 3, max 200)\n"
	        "Increasing this value may help to avoid underruns but also increases audio lag.\n"
	        "Cannot be set less than or equal to mt32.chunk value.\n"
	        "Valid for rendering in separate thread only.");

	int_prop = sec_prop.Add_int("partials", when_idle, 32);
	int_prop->SetMinMax(8, 256);
	int_prop->Set_help(
	        "The maximum number of partials playing simultaneously. (min 8, max 256");

	const char *mt32DACModes[] = {"0", "1", "2", "3", 0};
	int_prop = sec_prop.Add_int("dac", when_idle, 0);
	int_prop->Set_values(mt32DACModes);
	int_prop->Set_help(
	        "MT-32 DAC input emulation mode\n"
	        "Nice = 0 - default\n"
	        "Produces samples at double the volume, without tricks.\n"
	        "Higher quality than the real devices\n\n"

	        "Pure = 1\n"
	        "Produces samples that exactly match the bits output from the emulated LA32.\n"
	        "Nicer overdrive characteristics than the DAC hacks (it simply clips samples within range)\n"
	        "Much less likely to overdrive than any other mode.\n"
	        "Half the volume of any of the other modes.\n"
	        "Perfect for developers while debugging :)\n\n"

	        "GENERATION1 = 2\n"
	        "Re-orders the LA32 output bits as in early generation MT-32s (according to Wikipedia).\n"
	        "Bit order at DAC (where each number represents the original LA32 output bit number, and XX means the bit is always low):\n"
	        "15 13 12 11 10 09 08 07 06 05 04 03 02 01 00 XX\n\n"

	        "GENERATION2 = 3\n"
	        "Re-orders the LA32 output bits as in later generations (personally confirmed on my CM-32L - KG).\n"
	        "Bit order at DAC (where each number represents the original LA32 output bit number):\n"
	        "15 13 12 11 10 09 08 07 06 05 04 03 02 01 00 14");

	const char *mt32analogModes[] = {"0", "1", "2", "3", 0};
	int_prop = sec_prop.Add_int("analog", when_idle, 2);
	int_prop->Set_values(mt32analogModes);
	int_prop->Set_help(
	        "MT-32 analogue output emulation mode\n"
	        "Digital = 0\n"
	        "Only digital path is emulated. The output samples correspond to the digital output signal appeared at the DAC entrance.\n"
	        "Fastest mode.\n\n"

	        "Coarse = 1\n"
	        "Coarse emulation of LPF circuit. High frequencies are boosted, sample rate remains unchanged.\n"
	        "A bit better sounding but also a bit slower.\n\n"

	        "Accurate = 2 - default\n"
	        "Finer emulation of LPF circuit. Output signal is upsampled to 48 kHz to allow emulation of audible mirror spectra above 16 kHz,\n"
	        "which is passed through the LPF circuit without significant attenuation.\n"
	        "Sounding is closer to the analog output from real hardware but also slower than the modes 0 and 1.\n\n"

	        "Oversampled = 3\n"
	        "Same as the default mode 2 but the output signal is 2x oversampled, i.e. the output sample rate is 96 kHz.\n"
	        "Even slower than all the other modes but better retains highest frequencies while further resampled in DOSBox mixer.");

	const char *mt32reverbModes[] = {"0", "1", "2", "3", "auto", 0};
	str_prop = sec_prop.Add_string("reverb.mode", when_idle, "auto");
	str_prop->Set_values(mt32reverbModes);
	str_prop->Set_help("MT-32 reverb mode");

	const char *mt32reverbTimes[] = {"0", "1", "2", "3", "4",
	                                 "5", "6", "7", 0};
	int_prop = sec_prop.Add_int("reverb.time", when_idle, 5);
	int_prop->Set_values(mt32reverbTimes);
	int_prop->Set_help("MT-32 reverb decaying time");

	const char *mt32reverbLevels[] = {"0", "1", "2", "3", "4",
	                                  "5", "6", "7", 0};
	int_prop = sec_prop.Add_int("reverb.level", when_idle, 3);
	int_prop->Set_values(mt32reverbLevels);
	int_prop->Set_help("MT-32 reverb level");

	// Some frequently used option sets
	const char *rates[] = {"44100", "48000", "32000", "22050", "16000",
	                       "11025", "8000",  "49716", 0};
	int_prop = sec_prop.Add_int("rate", when_idle, 44100);
	int_prop->Set_values(rates);
	int_prop->Set_help("Sample rate of MT-32 emulation.");

	const char *mt32srcQuality[] = {"0", "1", "2", "3", 0};
	int_prop = sec_prop.Add_int("src.quality", when_idle, 2);
	int_prop->Set_values(mt32srcQuality);
	int_prop->Set_help(
	        "MT-32 sample rate conversion quality\n"
	        "Value '0' is for the fastest conversion, value '3' provides for the best conversion quality. Default is 2.");

	bool_prop = sec_prop.Add_bool("niceampramp", when_idle, true);
	bool_prop->Set_help(
	        "Toggles \"Nice Amp Ramp\" mode that improves amplitude ramp for sustaining instruments.\n"
	        "Quick changes of volume or expression on a MIDI channel may result in amp jumps on real hardware.\n"
	        "When \"Nice Amp Ramp\" mode is enabled, amp changes gradually instead.\n"
	        "Otherwise, the emulation accuracy is preserved.\n"
	        "Default is true.");
}

static mt32emu_report_handler_i get_report_handler_interface()
{
	class ReportHandler {
	public:
		static mt32emu_report_handler_version getReportHandlerVersionID(mt32emu_report_handler_i)
		{
			return MT32EMU_REPORT_HANDLER_VERSION_0;
		}

		static void printDebug(void *instance_data, const char *fmt, va_list list)
		{
			MidiHandler_mt32 &midiHandler_mt32 = *(
			        MidiHandler_mt32 *)instance_data;
			if (midiHandler_mt32.noise) {
				char s[1024];
				safe_sprintf(s, fmt, list);
				LOG_MSG("MT32: %s", s);
			}
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

bool MidiHandler_mt32::Open(const char *conf)
{
	service = new MT32Emu::Service();
	Bit32u version = service->getLibraryVersionInt();
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

	service->setPartialCount(Bit32u(section->Get_int("partials")));
	service->setAnalogOutputMode(
	        (MT32Emu::AnalogOutputMode)section->Get_int("analog"));
	int sampleRate = section->Get_int("rate");
	service->setStereoOutputSampleRate(sampleRate);
	service->setSamplerateConversionQuality(
	        (MT32Emu::SamplerateConversionQuality)section->Get_int("src.quality"));

	if (MT32EMU_RC_OK != (rc = service->openSynth())) {
		delete service;
		service = nullptr;
		LOG_MSG("MT32: Error initialising emulation: %i", rc);
		return false;
	}

	if (strcmp(section->Get_string("reverb.mode"), "auto") != 0) {
		Bit8u reverbsysex[] = {0x10, 0x00, 0x01, 0x00, 0x05, 0x03};
		reverbsysex[3] = (Bit8u)atoi(section->Get_string("reverb.mode"));
		reverbsysex[4] = (Bit8u)section->Get_int("reverb.time");
		reverbsysex[5] = (Bit8u)section->Get_int("reverb.level");
		service->writeSysex(16, reverbsysex, 6);
		service->setReverbOverridden(true);
	}

	service->setDACInputMode((MT32Emu::DACInputMode)section->Get_int("dac"));

	service->setReversedStereoEnabled(section->Get_bool("reverse.stereo"));
	service->setNiceAmpRampEnabled(section->Get_bool("niceampramp"));
	noise = section->Get_bool("verbose");
	renderInThread = section->Get_bool("thread");

	if (noise)
		LOG_MSG("MT32: Set maximum number of partials %d",
		        service->getPartialCount());

	if (noise)
		LOG_MSG("MT32: Adding mixer channel at sample rate %d", sampleRate);

	const auto mixer_callback = std::bind(&MidiHandler_mt32::MixerCallBack,
	                                      this, std::placeholders::_1);
	chan = MIXER_AddChannel(mixer_callback, sampleRate, "MT32");

	if (renderInThread) {
		stopProcessing = false;
		playPos = 0;
		int chunkSize = section->Get_int("chunk");
		minimumRenderFrames = (chunkSize * sampleRate) / MILLIS_PER_SECOND;
		int latency = section->Get_int("prebuffer");
		if (latency <= chunkSize) {
			latency = 2 * chunkSize;
			LOG_MSG("MT32: chunk length must be less than prebuffer length, prebuffer length reset to %i ms.",
			        latency);
		}
		framesPerAudioBuffer = (latency * sampleRate) / MILLIS_PER_SECOND;
		audioBufferSize = framesPerAudioBuffer << 1;
		audioBuffer = new int16_t[audioBufferSize];
		service->renderBit16s(audioBuffer, framesPerAudioBuffer - 1);
		renderPos = (framesPerAudioBuffer - 1) << 1;
		playedBuffers = 1;
		lock = SDL_CreateMutex();
		framesInBufferChanged = SDL_CreateCond();
		thread = SDL_CreateThread(processingThread, "mt32emu", nullptr);
	}
	chan->Enable(true);

	open = true;
	return true;
}

void MidiHandler_mt32::Close(void)
{
	if (!open)
		return;
	chan->Enable(false);
	if (renderInThread) {
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

void MidiHandler_mt32::PlayMsg(const uint8_t *msg)
{
	if (renderInThread) {
		service->playMsgAt(SDL_SwapLE32(*(Bit32u *)msg),
		                   getMidiEventTimestamp());
	} else {
		service->playMsg(SDL_SwapLE32(*(Bit32u *)msg));
	}
}

void MidiHandler_mt32::PlaySysex(Bit8u *sysex, Bitu len)
{
	if (renderInThread) {
		service->playSysexAt(sysex, len, getMidiEventTimestamp());
	} else {
		service->playSysex(sysex, len);
	}
}

int MidiHandler_mt32::processingThread(void *)
{
	mt32_instance.renderingLoop();
	return 0;
}

MidiHandler_mt32::~MidiHandler_mt32()
{
	Close();
}

void MidiHandler_mt32::MixerCallBack(uint16_t len)
{
	if (renderInThread) {
		while (renderPos == playPos) {
			SDL_LockMutex(lock);
			SDL_CondWait(framesInBufferChanged, lock);
			SDL_UnlockMutex(lock);
			if (stopProcessing)
				return;
		}
		Bitu renderPosSnap = renderPos;
		Bitu playPosSnap = playPos;
		Bitu samplesReady = (renderPosSnap < playPosSnap)
		                            ? audioBufferSize - playPosSnap
		                            : renderPosSnap - playPosSnap;
		if (len > (samplesReady >> 1)) {
			len = samplesReady >> 1;
		}
		chan->AddSamples_s16(len, audioBuffer + playPosSnap);
		playPosSnap += (len << 1);
		while (audioBufferSize <= playPosSnap) {
			playPosSnap -= audioBufferSize;
			playedBuffers++;
		}
		playPos = playPosSnap;
		renderPosSnap = renderPos;
		const Bitu samplesFree = (renderPosSnap < playPosSnap)
		                                 ? playPosSnap - renderPosSnap
		                                 : audioBufferSize + playPosSnap -
		                                           renderPosSnap;
		if (minimumRenderFrames <= (samplesFree >> 1)) {
			SDL_LockMutex(lock);
			SDL_CondSignal(framesInBufferChanged);
			SDL_UnlockMutex(lock);
		}
	} else {
		service->renderBit16s((Bit16s *)MixTemp, len);
		chan->AddSamples_s16(len, (Bit16s *)MixTemp);
	}
}

void MidiHandler_mt32::renderingLoop()
{
	while (!stopProcessing) {
		const Bitu renderPosSnap = renderPos;
		const Bitu playPosSnap = playPos;
		Bitu samplesToRender;
		if (renderPosSnap < playPosSnap) {
			samplesToRender = playPosSnap - renderPosSnap - 2;
		} else {
			samplesToRender = audioBufferSize - renderPosSnap;
			if (playPosSnap == 0)
				samplesToRender -= 2;
		}
		Bitu framesToRender = samplesToRender >> 1;
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

static void mt32_init(Section *sec)
{}

void MT32_AddConfigSection(Config *conf)
{
	assert(conf);
	Section_prop *sec = conf->AddSection_prop("mt32", &mt32_init);
	assert(sec);
	init_mt32_dosbox_settings(*sec);
}

#endif // C_MT32EMU
