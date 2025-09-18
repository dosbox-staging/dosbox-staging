// SPDX-FileCopyrightText:  2023-2025 The DOSBox Staging Team
// SPDX-FileCopyrightText:  2002-2021 The DOSBox Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "capture.h"

#include <cstdio>
#include <cstdlib>

#include "config/setup.h"
#include "gui/titlebar.h"
#include "hardware/memory.h"
#include "hardware/video/video.h"

static constexpr auto SampleFrameSize   = 4;
static constexpr auto NumFramesInBuffer = 16 * 1024;
static constexpr auto NumChannels       = 2;

static struct {
	FILE* handle = nullptr;

	uint16_t buf[NumFramesInBuffer][NumChannels] = {};

	uint32_t sample_rate_hz  = 0;
	uint32_t buf_frames_used = 0;
	
	// TODO A 16-bit / 44.1kHz WAV file is limited to a bit less than 4GB
	// worth of sample data because the chunk sizes are stored as 32-bit
	// unsigned integers in the RIFF container the WAV format uses.
	//
	// So technically we should chunk the recording into separate WAV files at
	// ~3.4 hour intervals, which is the duration of a recording of a 2GB
	// WAV file recorded at 16-bit/44.1kHz (some programs use 32-bit signed
	// integers when handling WAV files, therefore 2GB is the safe limit).
	//
	// This will be more of a problem when adding support for 24 and 32-bit
	// formats, as in case of a 32-bit float WAV file, the safe duration is
	// reduced to ~1.7 hour.
	uint32_t data_bytes_written = 0;
} wave = {};

// clang-format off
static uint8_t wav_header[] = {
	'R',  'I',  'F',  'F',   // uint32 - RIFF chunk ID
	0x00, 0x00, 0x00, 0x00,	 // uint32 - RIFF chunk size
	'W',  'A',  'V',  'E',   // uint32 - RIFF format
	'f',  'm',  't',  ' ',	 // uint32 - fmt chunk ID
	0x10, 0x00, 0x00, 0x00,  // uint32 - fmt chunksize
	0x01, 0x00,              // uint16 - Audio format, 1 = PCM
	0x02, 0x00,              // uint16 - Num channels, 2 = stereo
	0x00, 0x00, 0x00, 0x00,  // uint32 - Sample rate
	0x00, 0x00, 0x00, 0x00,	 // uint32 - Byte rate
	0x04, 0x00,              // uint16 - Block align
	0x10, 0x00,              // uint16 - Bits per sample, 16-bit
	'd',  'a',  't',  'a',	 // uint32 - Data chunk ID
	0x00, 0x00, 0x00, 0x00,	 // uint32 - Data chunk size
};
// clang-format on

static void create_wave_file(const uint32_t sample_rate_hz)
{
	wave.handle = CAPTURE_CreateFile(CaptureType::Audio);
	if (!wave.handle) {
		return;
	}

	wave.sample_rate_hz     = sample_rate_hz;
	wave.buf_frames_used    = 0;
	wave.data_bytes_written = 0;

	fwrite(wav_header, 1, sizeof(wav_header), wave.handle);
}

void capture_audio_add_data(const uint32_t sample_rate_hz,
                            const uint32_t num_sample_frames,
                            const int16_t* sample_frames)
{
	if (!wave.handle) {
		TITLEBAR_NotifyAudioCaptureStatus(true);
		create_wave_file(sample_rate_hz);
	}
	if (!wave.handle) {
		TITLEBAR_NotifyAudioCaptureStatus(false);
		return;
	}

	const int16_t* data   = sample_frames;
	auto remaining_frames = num_sample_frames;

	while (remaining_frames > 0) {
		uint32_t frames_left = NumFramesInBuffer - wave.buf_frames_used;
		if (!frames_left) {
			const auto bytes_to_write = NumFramesInBuffer * SampleFrameSize;
			fwrite(wave.buf, 1, bytes_to_write, wave.handle);

			wave.data_bytes_written += bytes_to_write;
			wave.buf_frames_used = 0;

			frames_left = NumFramesInBuffer;
		}

		if (frames_left > remaining_frames) {
			frames_left = remaining_frames;
		}

		memcpy(&wave.buf[wave.buf_frames_used],
		       data,
		       frames_left * SampleFrameSize);

		wave.buf_frames_used += frames_left;
		data += frames_left * NumChannels;
		remaining_frames -= frames_left;
	}
}

void capture_audio_finalise()
{
	if (!wave.handle) {
		return;
	}

	// Flush audio buffer
	const auto bytes_to_write = wave.buf_frames_used * SampleFrameSize;
	fwrite(wave.buf, 1, bytes_to_write, wave.handle);
	wave.data_bytes_written += bytes_to_write;

	// Update headers
	constexpr auto chunk_header_size = 8;

	const auto riff_chunk_size = static_cast<uint32_t>(wave.data_bytes_written +
	                             sizeof(wav_header) - chunk_header_size);

	constexpr auto riff_chunk_size_offset = 0x04;
	host_writed(&wav_header[riff_chunk_size_offset], riff_chunk_size);

	constexpr auto sample_rate_offset = 0x18;
	host_writed(&wav_header[sample_rate_offset], wave.sample_rate_hz);

	constexpr auto byte_rate_offset = 0x1c;
	host_writed(&wav_header[byte_rate_offset],
	            wave.sample_rate_hz * SampleFrameSize);

	constexpr auto data_chunk_size_offset = 0x28;
	host_writed(&wav_header[data_chunk_size_offset], wave.data_bytes_written);

	fseek(wave.handle, 0, 0);
	fwrite(wav_header, 1, sizeof(wav_header), wave.handle);
	fclose(wave.handle);

	wave = {};

	TITLEBAR_NotifyAudioCaptureStatus(false);
}
