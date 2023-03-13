/*
 *  SPDX-License-Identifier: GPL-3.0-or-later
 *
 *  Copyright (C) 2023-2023  The DOSBox Staging Team
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

#include <cmath>
#include <cstdio>
#include <vector>

#include <SDL.h>

static void play_tone(const int sample_rate, const int device_id)
{
	constexpr int16_t tone_hz         = 2000;
	constexpr int16_t amplitude       = 5000;
	constexpr int16_t duration_millis = 1500;
	constexpr int16_t num_channels    = 2;
	constexpr int16_t millis_per_s    = 1000;

	int num_samples = sample_rate * num_channels * duration_millis / millis_per_s;

	std::vector<int16_t> buffer(num_samples);

	float angle = 0.0f;

	const auto increment = tone_hz * 2.0f *
	                       static_cast<float>(M_PI / sample_rate);

	for (int i = 0; i < num_samples; i += 2) {
		const auto sample = static_cast<int16_t>(sinf(angle) * amplitude);
		buffer[i]     = sample;
		buffer[i + 1] = sample;
		angle += increment;
	}
	SDL_QueueAudio(device_id, buffer.data(), num_samples);

	constexpr auto unpause = 0;
	SDL_PauseAudioDevice(device_id, unpause);
	SDL_Delay(duration_millis);
}

class SdlAudioDevice {
public:
	SdlAudioDevice(const char* name)
	{
		SDL_AudioSpec desired_spec = {};
		desired_spec.freq          = 48000;
		desired_spec.format        = AUDIO_S16LSB;
		desired_spec.channels      = 2;
		desired_spec.samples       = 2048;
		desired_spec.callback      = nullptr;
		desired_spec.userdata      = nullptr;

		int sdl_allow_flags          = SDL_AUDIO_ALLOW_ANY_CHANGE;
		constexpr int is_for_capture = 0;

		printf("    Opening %s .. ", name);

		device_id = SDL_OpenAudioDevice(name,
		                                is_for_capture,
		                                &desired_spec,
		                                &obtained_spec,
		                                sdl_allow_flags);

		if (device_id <= 0) {
			printf("failed (skipping): %s\n", SDL_GetError());
		}
	}

	void Test()
	{
		if (device_id <= 0) {
			return;
		}
		printf("testing .. ");
		fflush(stdout);

		play_tone(obtained_spec.freq, device_id);
	}

	~SdlAudioDevice()
	{
		if (device_id > 0) {
			printf("closing .. ");
			SDL_CloseAudioDevice(device_id);
			printf("done.\n");
		}
	}

private:
	SDL_AudioDeviceID device_id = 0;
	SDL_AudioSpec obtained_spec = {};
};

class SdlAudioDriver {
public:
	SdlAudioDriver(const char* name)
	{
		printf("\nInitializing %s .. ", name);

		was_initialized = SDL_AudioInit(name) >= 0;

		if (was_initialized) {
			printf("done.\n");
		} else {
			printf("failed (skipping)\n");
			printf("    Reason: %s\n", SDL_GetError());
		}
	}

	~SdlAudioDriver()
	{
		if (was_initialized) {
			SDL_AudioQuit();
		}
	}

	void TestDevices()
	{
		if (!was_initialized) {
			return;
		}
		constexpr int is_for_capture = 0;
		for (int i = 0; i < SDL_GetNumAudioDevices(is_for_capture); ++i) {
			const auto dev_name = SDL_GetAudioDeviceName(i, is_for_capture);
			SdlAudioDevice device(dev_name);
			device.Test();
		}
	}

private:
	bool was_initialized = false;
};

static void test_driver(const char* name)
{
	SdlAudioDriver driver(name);
	driver.TestDevices();
}

static void test_all_drivers()
{
	for (int i = 0; i < SDL_GetNumAudioDrivers(); ++i) {
		test_driver(SDL_GetAudioDriver(i));
	}
}

int main(const int argc, const char** argv)
{
	if (argc == 2) {
		const auto driver_name = argv[1];
		test_driver(driver_name);
	} else {
		test_all_drivers();
	}

	return 0;
}
