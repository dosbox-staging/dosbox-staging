// SPDX-FileCopyrightText:  2020-2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_FLUIDSYNTH_H
#define DOSBOX_FLUIDSYNTH_H

#include "midi_synth.h"

#include <memory>
#include <string>
#include <vector>

#include <fluidsynth.h>

#include "audio/mixer.h"
#include "dos/programs/more_output.h"
#include "misc/std_filesystem.h"

struct ChorusParameters {
	int voice_count = {};
	double level    = {};
	double speed    = {};
	double depth    = {};
	int mod_wave    = {};
};

struct ReverbParameters {
	double room_size = {};
	double damping   = {};
	double width     = {};
	double level     = {};
};

enum class SoundFont {
	// Unidentified SoundFont
	Unknown,

	// GeneralUser GS -- a general-purpose Roland GS compatible SoundFont by
	// S. Christian Collins
	//
	// Ref: https://schristiancollins.com/generaluser.php
	//
	GeneralUserGs,

	// Conversion of the original 'synthgs.sbk' AWE32 SoundFont by S.
	// Christian Collins
	//
	// Ref: https://github.com/mrbumpy409/AWE32-midi-conversions
	//
	Awe32_SynthGs,

	// Conversion of the original '4gmgsmt.sf2' Sound Blaster Live! SoundFont
	// by S. Christian Collins
	//
	// Ref: https://github.com/mrbumpy409/AWE32-midi-conversions
	//
	SbLive_4GmGsMt,

	// Fluid R3 -- a general-purpose Roland GS compatible SoundFont by Frank
	// Wen
	//
	// Ref:
	// - https://www.polyphone.io/en/soundfonts/instrument-sets/250-fluidr3-gm
	// - https://archive.org/download/fluidr3-gm-gs
	//
	FluidR3,

	// Trevor0402's Roland SC-55 emulation
	//
	// Ref:
	// -
	// https://www.doomworld.com/forum/topic/118828-trevor0402s-sc-55-soundfont/
	// - https://archive.org/download/500-soundfonts-full-gm-sets/ (SC-55
	// SoundFont.v1.2b [Trevor0402].sf2)
	//
	Trevor0402_Sc55
};

class MidiDeviceFluidSynth final : public MidiSynth {
public:
	// Throws `std::runtime_error` if the MIDI device cannot be initialiased
	// (e.g., the requested SoundFont cannot be loaded).
	MidiDeviceFluidSynth();

	~MidiDeviceFluidSynth()
	{
		Shutdown();
	}

	// prevent copying
	MidiDeviceFluidSynth(const MidiDeviceFluidSynth&) = delete;
	// prevent assignment
	MidiDeviceFluidSynth& operator=(const MidiDeviceFluidSynth&) = delete;

	std::string GetName() const override
	{
		return MidiDeviceName::FluidSynth;
	}

	Type GetType() const override
	{
		return MidiDevice::Type::Internal;
	}

	void PrintStats();

	std_fs::path GetSoundFontPath();

	void SetChorus();
	void SetReverb();
	void SetFilter();

	void SetVolume(const int volume_percent);

private:
	void IdentifySoundFont();

	void SetChorusParams(const ChorusParameters& params);
	void SetReverbParams(const ReverbParameters& params);

	void ApplyChannelMessage(const std::vector<uint8_t>& msg);
	void ApplySysExMessage(const std::vector<uint8_t>& msg);

	void ProcessWorkItem(const MidiWork& work) override;
	void RenderAudioFramesToFifo(const int num_audio_frames) override;

	void CloseSynth() override {}

	using FluidSynthSettingsPtr =
	        std::unique_ptr<fluid_settings_t, decltype(&delete_fluid_settings)>;

	using FluidSynthPtr = std::unique_ptr<fluid_synth_t, decltype(&delete_fluid_synth)>;

	FluidSynthSettingsPtr settings = {nullptr, &delete_fluid_settings};
	FluidSynthPtr synth            = {nullptr, &delete_fluid_synth};

	std_fs::path soundfont_path = {};
	SoundFont soundfont         = {};
};

void FSYNTH_ListDevices(MidiDeviceFluidSynth* device, MoreOutputStrings& output);

#endif // DOSBOX_FLUIDSYNTH_H
