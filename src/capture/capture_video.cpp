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
#include <cmath>

#include "math_utils.h"
#include "mem.h"
#include "render.h"
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

static ZMBV_FORMAT to_zmbv_format(const PixelFormat format)
{
	switch (format) {
	case PixelFormat::Indexed8: return ZMBV_FORMAT::BPP_8;
	case PixelFormat::RGB555_Packed16: return ZMBV_FORMAT::BPP_15;
	case PixelFormat::RGB565_Packed16: return ZMBV_FORMAT::BPP_16;

	// ZMBV internally maps from 24-bit to 32-bit (let ZMBV manage itself).
	case PixelFormat::BGR24_ByteArray: return ZMBV_FORMAT::BPP_24;

	case PixelFormat::BGRX32_ByteArray: return ZMBV_FORMAT::BPP_32;
	default: assertm(false, "Invalid pixel_format value"); break;
	}
	return ZMBV_FORMAT::NONE;
}

// Returns number of bytes needed to represent the given PixelFormat
static uint8_t to_bytes_per_pixel(const PixelFormat format)
{
	// PixelFormat's underly value is colour depth in number of bits
	const float num_bits = static_cast<uint8_t>(format);

	// Use float-ceil to handle non-power-of-two bit quantities
	constexpr uint8_t bits_per_byte = 8;
	const auto num_bytes = iround(ceilf(num_bits / bits_per_byte));

	assert(num_bytes >= 1 && num_bytes <= 4);
	return static_cast<uint8_t>(num_bytes);
}

static uint8_t to_bytes_per_pixel(const ZMBV_FORMAT format)
{
	return ZMBV_ToBytesPerPixel(format);
}

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

// Reads the image's pixels from the emulated DOS video RAM (which is always
// little-endian) and passes them in the same byte-order to the encoder.
//
// Ideally this could just be one memcpy, but many DOS video modes need their
// pixels either doubled horizontally and/or vertically to produce the correct
// on-screen size. So extra copies are needed in both these cases.
//
static void compress_frame(const RenderedImage& image)
{
	const auto& src = image.params;
	auto src_row = image.image_data;

	const auto pixel_multiple = (src.double_width ? 2 : 1);
	const auto row_multiple   = (src.double_height ? 2 : 1);

	auto compress_row = [&](const uint8_t* row_buffer) {
		for (auto i = 0; i < row_multiple; ++i) {
			video.codec->CompressLines(1, &row_buffer);
		}
	};

	const auto src_bpp = to_bytes_per_pixel(src.pixel_format);
	const auto tgt_bpp = to_bytes_per_pixel(to_zmbv_format(src.pixel_format));

	// Maybe compress the source rows straight away without copying. Note
	// that this is a shortcut scenario; hard-code it to false to exercise
	// the rote version below.
	const auto can_use_src_directly = (src_bpp == tgt_bpp) && !src.double_width;
	if (can_use_src_directly) {
		for (auto i = 0; i < src.height; ++i, src_row += image.pitch) {
			compress_row(src_row);
		}
		return;
	}

	// Otherwise we need to arrange the source bytes before compressing
	assert(!can_use_src_directly);

	// Grow the target row buffer if needed
	const auto tgt_row_bytes = check_cast<uint16_t>(src.width * tgt_bpp *
	                                                pixel_multiple);
	static std::vector<uint8_t> tgt_row = {};
	if (tgt_row.size() < tgt_row_bytes) {
		tgt_row.resize(tgt_row_bytes, 0);
	}

	for (auto i = 0; i < src.height; ++i, src_row += image.pitch) {
		auto src_pixel = src_row;
		auto tgt_pixel = tgt_row.data();
		for (auto j = 0; j < src.width; ++j, src_pixel += src_bpp) {
			for (auto k = 0; k < pixel_multiple; ++k, tgt_pixel += tgt_bpp) {
				std::memcpy(tgt_pixel, src_pixel, src_bpp);
			}
		}
		compress_row(tgt_row.data());
	}
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

	const auto format = to_zmbv_format(src.pixel_format);

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

	compress_frame(image);

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
