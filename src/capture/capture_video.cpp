/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2023-2023  The DOSBox Staging Team
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

#include "capture.h"

#include <cassert>

#include "mem.h"
#include "render.h"
#include "rgb888.h"
#include "support.h"

#include "zmbv/zmbv.h"

static constexpr auto NumSampleFramesInBuffer = 16 * 1024;

static constexpr auto SampleFrameSize  = 4;
static constexpr auto NumAudioChannels = 2;

static constexpr auto AviHeaderSize = 500;

static struct {
	FILE* handle = nullptr;

	uint32_t frames          = 0;
	VideoCodec* codec        = nullptr;
	int width                = 0;
	int height               = 0;
	PixelFormat pixel_format = {};
	float frames_per_second  = 0.0f;

	uint32_t written           = 0;
	uint32_t buf_size          = 0;
	std::vector<uint8_t> buf   = {};
	std::vector<uint8_t> index = {};
	uint32_t index_used        = 0;

	struct {
		int16_t buf[NumSampleFramesInBuffer][NumAudioChannels] = {};

		uint32_t sample_rate     = 0;
		uint32_t buf_frames_used = 0;
		uint32_t bytes_written   = 0;
	} audio = {};
} video = {};

static void add_avi_chunk(const char* tag, const uint32_t size,
                          const void* data, const uint32_t flags)
{
	uint8_t chunk[8] = {};

	chunk[0] = tag[0];
	chunk[1] = tag[1];
	chunk[2] = tag[2];
	chunk[3] = tag[3];

	host_writed(&chunk[4], size);

	// Write the actual data
	fwrite(chunk, 1, 8, video.handle);

	auto writesize = (size + 1) & ~1;
	fwrite(data, 1, writesize, video.handle);

	auto pos = video.written + 4;
	video.written += writesize + 8;

	if (video.index_used + 16 >= video.index.size()) {
		video.index.resize(video.index.size() + 16 * 4096);
	}

	auto index = video.index.data() + video.index_used;
	video.index_used += 16;

	index[0] = tag[0];
	index[1] = tag[1];
	index[2] = tag[2];
	index[3] = tag[3];

	host_writed(index + 4, flags);
	host_writed(index + 8, pos);
	host_writed(index + 12, size);
}

void capture_video_finalise()
{
	if (!video.handle) {
		return;
	}
	if (video.codec) {
		video.codec->FinishVideo();
	}

	uint8_t avi_header[AviHeaderSize];
	uint32_t header_pos = 0;

#define AVIOUT4(_S_) \
	memcpy(&avi_header[header_pos], _S_, 4); \
	header_pos += 4;
#define AVIOUTw(_S_) \
	host_writew(&avi_header[header_pos], _S_); \
	header_pos += 2;
#define AVIOUTd(_S_) \
	host_writed(&avi_header[header_pos], _S_); \
	header_pos += 4;

	// Try and write an avi header
	AVIOUT4("RIFF"); // Riff header
	AVIOUTd(AviHeaderSize + video.written - 8 + video.index_used);
	AVIOUT4("AVI ");
	AVIOUT4("LIST");

	const auto main_list = header_pos;
	// TODO size of list
	AVIOUTd(0);
	AVIOUT4("hdrl");

	AVIOUT4("avih");
	// # of bytes to follow
	AVIOUTd(56);
	/* Microseconds per frame */
	AVIOUTd((uint32_t)(1000000 / video.frames_per_second));
	AVIOUTd(0);
	AVIOUTd(0);            /* PaddingGranularity (whatever that might be) */
	AVIOUTd(0x110);        /* Flags,0x10 has index, 0x100 interleaved */
	AVIOUTd(video.frames); /* TotalFrames */
	AVIOUTd(0);            /* InitialFrames */
	AVIOUTd(2);            /* Stream count */
	AVIOUTd(0);            /* SuggestedBufferSize */
	AVIOUTd(static_cast<uint32_t>(video.width));  /* Width */
	AVIOUTd(static_cast<uint32_t>(video.height)); /* Height */
	AVIOUTd(0); /* TimeScale:  Unit used to measure time */
	AVIOUTd(0); /* DataRate:   Data rate of playback     */
	AVIOUTd(0); /* StartTime:  Starting time of AVI data */
	AVIOUTd(0); /* DataLength: Size of AVI data chunk    */

	// Video stream list
	AVIOUT4("LIST");
	AVIOUTd(4 + 8 + 56 + 8 + 40); /* Size of the list */
	AVIOUT4("strl");

	// Video stream header
	AVIOUT4("strh");
	AVIOUTd(56);        /* # of bytes to follow */
	AVIOUT4("vids");    /* Type */
	AVIOUT4(CODEC_4CC); /* Handler */
	AVIOUTd(0);         /* Flags */
	AVIOUTd(0);         /* Reserved, MS says: wPriority, wLanguage */
	AVIOUTd(0);         /* InitialFrames */
	AVIOUTd(1000000);   /* Scale */

	/* Rate: Rate/Scale == samples/second */
	AVIOUTd((uint32_t)(1000000 * video.frames_per_second));
	AVIOUTd(0);            /* Start */
	AVIOUTd(video.frames); /* Length */
	AVIOUTd(0);            /* SuggestedBufferSize */
	AVIOUTd(~0);           /* Quality */
	AVIOUTd(0);            /* SampleSize */
	AVIOUTd(0);            /* Frame */
	AVIOUTd(0);            /* Frame */

	// The video stream format
	AVIOUT4("strf");
	AVIOUTd(40);                                  /* # of bytes to follow */
	AVIOUTd(40);                                  /* Size */
	AVIOUTd(static_cast<uint32_t>(video.width));  /* Width */
	AVIOUTd(static_cast<uint32_t>(video.height)); /* Height */
	//		OUTSHRT(1); OUTSHRT(24);     /* Planes, Count */
	AVIOUTd(0);
	AVIOUT4(CODEC_4CC); /* Compression */
	AVIOUTd(video.width * video.height *
	        4); /* SizeImage (in
	                                                           bytes?) */
	AVIOUTd(0); /* XPelsPerMeter */
	AVIOUTd(0); /* YPelsPerMeter */
	AVIOUTd(0); /* ClrUsed: Number of colors used */
	AVIOUTd(0); /* ClrImportant: Number of colors important */

	// Audio stream list
	AVIOUT4("LIST");
	AVIOUTd(4 + 8 + 56 + 8 + 16); /* Length of list in bytes */
	AVIOUT4("strl");

	// The audio stream header
	AVIOUT4("strh");
	AVIOUTd(56); /* # of bytes to follow */
	AVIOUT4("auds");
	AVIOUTd(0); /* Format (Optionally) */
	AVIOUTd(0); /* Flags */
	AVIOUTd(0); /* Reserved, MS says: wPriority, wLanguage */
	AVIOUTd(0); /* InitialFrames */
	AVIOUTd(4); /* Scale */

	/* Rate, actual rate is scale/rate */
	AVIOUTd(video.audio.sample_rate * SampleFrameSize);
	AVIOUTd(0); /* Start */

	if (!video.audio.sample_rate) {
		video.audio.sample_rate = 1;
	}

	AVIOUTd(video.audio.bytes_written / SampleFrameSize); /* Length */
	AVIOUTd(0);               /* SuggestedBufferSize */
	AVIOUTd(~0);              /* Quality */
	AVIOUTd(SampleFrameSize); /* SampleSize */
	AVIOUTd(0);               /* Frame */
	AVIOUTd(0);               /* Frame */

	// The audio stream format
	AVIOUT4("strf");
	AVIOUTd(16);                      /* # of bytes to follow */
	AVIOUTw(1);                       /* Format, WAVE_ZMBV_FORMAT_PCM */
	AVIOUTw(2);                       /* Number of channels */
	AVIOUTd(video.audio.sample_rate); /* SamplesPerSec */
	AVIOUTd(video.audio.sample_rate * SampleFrameSize); /* AvgBytesPerSec*/
	AVIOUTw(4);                                         /* BlockAlign */
	AVIOUTw(16);                                        /* BitsPerSample */

	int nmain = header_pos - main_list - 4;

	// Finish stream list, i.e. put number of bytes in the list to
	// proper pos
	int njunk = AviHeaderSize - 8 - 12 - header_pos;
	AVIOUT4("JUNK");
	AVIOUTd(njunk);

	// Fix the size of the main list
	header_pos = main_list;
	AVIOUTd(nmain);
	header_pos = AviHeaderSize - 12;
	AVIOUT4("LIST");

	// Length of list in bytes
	AVIOUTd(video.written + 4);
	AVIOUT4("movi");

	// First add the index table to the end
	memcpy(video.index.data(), "idx1", 4);
	host_writed(video.index.data() + 4, video.index_used - 8);
	fwrite(video.index.data(), 1, video.index_used, video.handle);

	fseek(video.handle, 0, SEEK_SET);
	fwrite(&avi_header, 1, AviHeaderSize, video.handle);

	fclose(video.handle);
	delete video.codec;
	video.handle = nullptr;
}

void capture_video_add_audio_data(const uint32_t sample_rate,
                                  const uint32_t num_sample_frames,
                                  const int16_t* sample_frames)
{
	if (!video.handle) {
		return;
	}
	auto frames_left = NumSampleFramesInBuffer - video.audio.buf_frames_used;
	if (frames_left > num_sample_frames) {
		frames_left = num_sample_frames;
	}

	memcpy(&video.audio.buf[video.audio.buf_frames_used],
	       sample_frames,
	       frames_left * SampleFrameSize);

	video.audio.buf_frames_used += frames_left;
	video.audio.sample_rate = sample_rate;
}

static void create_avi_file(const uint16_t width, const uint16_t height,
                            const PixelFormat pixel_format,
                            const float frames_per_second, ZMBV_FORMAT format)
{
	video.handle = CAPTURE_CreateFile(CaptureType::Video);
	if (!video.handle) {
		return;
	}
	video.codec = new VideoCodec();
	if (!video.codec->SetupCompress(width, height)) {
		return;
	}

	video.buf_size = video.codec->NeededSize(width, height, format);
	video.buf.resize(video.buf_size);

	video.index.resize(16 * 4096);
	video.index_used = 8;

	video.width             = width;
	video.height            = height;
	video.pixel_format      = pixel_format;
	video.frames_per_second = frames_per_second;

	for (auto i = 0; i < AviHeaderSize; ++i) {
		fputc(0, video.handle);
	}

	video.frames                = 0;
	video.written               = 0;
	video.audio.buf_frames_used = 0;
	video.audio.bytes_written   = 0;
}

void capture_video_add_frame(const RenderedImage& image, const float frames_per_second)
{
	const auto& src = image.params;
	assert(src.width <= SCALER_MAXWIDTH);

	const auto video_width = src.double_width ? src.width * 2 : src.width;
	const auto video_height = src.double_height ? src.height * 2 : src.height;

	// Disable capturing if any of the test fails
	if (video.handle && (video.width != video_width || video.height != video_height ||
	                     video.pixel_format != src.pixel_format ||
	                     video.frames_per_second != frames_per_second)) {
		capture_video_finalise();
	}

	ZMBV_FORMAT format;

	switch (src.pixel_format) {
	case PixelFormat::Indexed8: format = ZMBV_FORMAT::BPP_8; break;
	case PixelFormat::BGR555: format = ZMBV_FORMAT::BPP_15; break;
	case PixelFormat::BGR565: format = ZMBV_FORMAT::BPP_16; break;

	// ZMBV is "the DOSBox capture format" supported by external
	// tools such as VLC, MPV, and ffmpeg. Because DOSBox originally
	// didn't have 24-bit color, the format itself doesn't support
	// it. I this case we tell ZMBV the data is 32-bit and let the
	// rgb24's int() cast operator up-convert.
	case PixelFormat::BGR888: format = ZMBV_FORMAT::BPP_32; break;
	case PixelFormat::BGRX8888: format = ZMBV_FORMAT::BPP_32; break;
	default: assertm(false, "Invalid pixel_format value"); return;
	}

	if (!video.handle) {
		create_avi_file(video_width,
		                video_height,
		                src.pixel_format,
		                frames_per_second,
		                format);
	}
	if (!video.handle) {
		return;
	}

	const auto codec_flags = (video.frames % 300 == 0) ? 1 : 0;

	if (!video.codec->PrepareCompressFrame(codec_flags,
	                                       format,
	                                       image.palette_data,
	                                       video.buf.data(),
	                                       video.buf_size)) {
		return;
	}

	alignas(uint32_t) uint8_t row_buffer[SCALER_MAXWIDTH * 4];

	auto src_row = image.image_data;

	for (auto i = 0; i < src.height; ++i) {
		const uint8_t* row_pointer = row_buffer;

		// TODO This all assumes little-endian byte order; should be
		// made endianness-aware like capture_image.cpp
		if (src.double_width) {
			switch (src.pixel_format) {
			case PixelFormat::Indexed8:
				for (auto x = 0; x < src.width; ++x) {
					const auto pixel      = src_row[x];
					row_buffer[x * 2 + 0] = pixel;
					row_buffer[x * 2 + 1] = pixel;
				}
				break;

			case PixelFormat::BGR555:
			case PixelFormat::BGR565:
				for (auto x = 0; x < src.width; ++x) {
					const auto pixel = ((uint16_t*)src_row)[x];

					((uint16_t*)row_buffer)[x * 2 + 0] = pixel;
					((uint16_t*)row_buffer)[x * 2 + 1] = pixel;
				}
				break;

			case PixelFormat::BGR888:
				for (auto x = 0; x < src.width; ++x) {
					const auto pixel = reinterpret_cast<const Rgb888*>(
					        src_row)[x];

					reinterpret_cast<uint32_t*>(
					        row_buffer)[x * 2 + 0] = pixel;
					reinterpret_cast<uint32_t*>(
					        row_buffer)[x * 2 + 1] = pixel;
				}
				break;

			case PixelFormat::BGRX8888:
				for (auto x = 0; x < src.width; ++x) {
					const auto pixel = ((uint32_t*)src_row)[x];

					((uint32_t*)row_buffer)[x * 2 + 0] = pixel;
					((uint32_t*)row_buffer)[x * 2 + 1] = pixel;
				}
				break;
			}
			row_pointer = row_buffer;

		} else {
			if (src.pixel_format == PixelFormat::BGR888) {
				for (auto x = 0; x < src.width; ++x) {
					const auto pixel = reinterpret_cast<const Rgb888*>(
					        src_row)[x];

					reinterpret_cast<uint32_t*>(
					        row_buffer)[x] = pixel;
				}
				row_pointer = row_buffer;
			} else {
				row_pointer = src_row;
			}
		}

		auto lines_to_write = src.double_height ? 2 : 1;
		while (lines_to_write--) {
			video.codec->CompressLines(1, &row_pointer);
		}

		src_row += image.pitch;
	}

	const auto written = video.codec->FinishCompressFrame();
	if (written < 0) {
		return;
	}

	add_avi_chunk("00dc", written, video.buf.data(), codec_flags & 1 ? 0x10 : 0x0);
	video.frames++;

	//		LOG_MSG("CAPTURE: Frame %d video %d audio
	//%d",video.frames, written, video.audio_buf_frames_used *4 );
	if (video.audio.buf_frames_used) {
		add_avi_chunk("01wb",
		              video.audio.buf_frames_used * SampleFrameSize,
		              video.audio.buf,
		              0);

		video.audio.bytes_written = video.audio.buf_frames_used *
		                            SampleFrameSize;
		video.audio.buf_frames_used = 0;
	}
}
