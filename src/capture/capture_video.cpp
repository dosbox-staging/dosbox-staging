/*
 *  Copyright (C) 2002-2023  The DOSBox Team
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

#include "mem.h"
#include "render.h"
#include "rgb24.h"

#if (C_SSHOT)
#include "../libs/zmbv/zmbv.h"

#define WAVE_BUF        16 * 1024
#define AVI_HEADER_SIZE 500

static struct {
	FILE* handle                  = nullptr;
	uint32_t frames               = 0;
	int16_t audiobuf[WAVE_BUF][2] = {};
	uint32_t audioused            = 0;
	uint32_t audiorate            = 0;
	uint32_t audiowritten         = 0;
	VideoCodec* codec             = nullptr;
	int width                     = 0;
	int height                    = 0;
	int bits_per_pixel            = 0;
	uint32_t written              = 0;
	float frames_per_second       = 0.0f;
	uint32_t bufSize              = 0;
	std::vector<uint8_t> buf      = {};
	std::vector<uint8_t> index    = {};
	uint32_t indexused            = 0;
} video = {};

static void add_avi_chunk(const char* tag, const uint32_t size,
                          const void* data, const uint32_t flags)
{
	uint8_t chunk[8];
	uint8_t* index;
	uint32_t pos, writesize;

	chunk[0] = tag[0];
	chunk[1] = tag[1];
	chunk[2] = tag[2];
	chunk[3] = tag[3];
	host_writed(&chunk[4], size);
	/* Write the actual data */
	fwrite(chunk, 1, 8, video.handle);
	writesize = (size + 1) & ~1;
	fwrite(data, 1, writesize, video.handle);
	pos = video.written + 4;
	video.written += writesize + 8;
	if (video.indexused + 16 >= video.index.size())
		video.index.resize(video.index.size() + 16 * 4096);

	index = video.index.data() + video.indexused;
	video.indexused += 16;
	index[0] = tag[0];
	index[1] = tag[1];
	index[2] = tag[2];
	index[3] = tag[3];
	host_writed(index + 4, flags);
	host_writed(index + 8, pos);
	host_writed(index + 12, size);
}

void handle_video_event(const bool pressed)
{
	if (!pressed)
		return;

	if (CaptureState & CAPTURE_VIDEO) {
		/* Close the video */
		if (video.codec)
			video.codec->FinishVideo();
		CaptureState &= ~CAPTURE_VIDEO;
		LOG_MSG("CAPTURE: Stopped capturing video output");

		uint8_t avi_header[AVI_HEADER_SIZE];
		uint32_t main_list;
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
		/* Try and write an avi header */
		AVIOUT4("RIFF"); // Riff header
		AVIOUTd(AVI_HEADER_SIZE + video.written - 8 + video.indexused);
		AVIOUT4("AVI ");
		AVIOUT4("LIST"); // List header
		main_list = header_pos;
		AVIOUTd(0); // TODO size of list
		AVIOUT4("hdrl");

		AVIOUT4("avih");
		AVIOUTd(56); /* # of bytes to follow */
		AVIOUTd((uint32_t)(1000000 / video.frames_per_second)); /* Microseconds
		                                                           per
		                                                           frame */
		AVIOUTd(0);
		AVIOUTd(0); /* PaddingGranularity (whatever that might be) */
		AVIOUTd(0x110); /* Flags,0x10 has index, 0x100 interleaved */
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

		/* Video stream list */
		AVIOUT4("LIST");
		AVIOUTd(4 + 8 + 56 + 8 + 40); /* Size of the list */
		AVIOUT4("strl");
		/* video stream header */
		AVIOUT4("strh");
		AVIOUTd(56);        /* # of bytes to follow */
		AVIOUT4("vids");    /* Type */
		AVIOUT4(CODEC_4CC); /* Handler */
		AVIOUTd(0);         /* Flags */
		AVIOUTd(0);       /* Reserved, MS says: wPriority, wLanguage */
		AVIOUTd(0);       /* InitialFrames */
		AVIOUTd(1000000); /* Scale */
		AVIOUTd((uint32_t)(1000000 * video.frames_per_second)); /* Rate:
		                                                           Rate/Scale
		                                                           ==
		                                                           samples/second
		                                                         */
		AVIOUTd(0);            /* Start */
		AVIOUTd(video.frames); /* Length */
		AVIOUTd(0);            /* SuggestedBufferSize */
		AVIOUTd(~0);           /* Quality */
		AVIOUTd(0);            /* SampleSize */
		AVIOUTd(0);            /* Frame */
		AVIOUTd(0);            /* Frame */
		/* The video stream format */
		AVIOUT4("strf");
		AVIOUTd(40); /* # of bytes to follow */
		AVIOUTd(40); /* Size */
		AVIOUTd(static_cast<uint32_t>(video.width));  /* Width */
		AVIOUTd(static_cast<uint32_t>(video.height)); /* Height */
		//		OUTSHRT(1); OUTSHRT(24);     /* Planes, Count */
		AVIOUTd(0);
		AVIOUT4(CODEC_4CC);                      /* Compression */
		AVIOUTd(video.width * video.height * 4); /* SizeImage (in
		                                            bytes?) */
		AVIOUTd(0);                              /* XPelsPerMeter */
		AVIOUTd(0);                              /* YPelsPerMeter */
		AVIOUTd(0); /* ClrUsed: Number of colors used */
		AVIOUTd(0); /* ClrImportant: Number of colors important */

		/* Audio stream list */
		AVIOUT4("LIST");
		AVIOUTd(4 + 8 + 56 + 8 + 16); /* Length of list in bytes */
		AVIOUT4("strl");
		/* The audio stream header */
		AVIOUT4("strh");
		AVIOUTd(56); /* # of bytes to follow */
		AVIOUT4("auds");
		AVIOUTd(0); /* Format (Optionally) */
		AVIOUTd(0); /* Flags */
		AVIOUTd(0); /* Reserved, MS says: wPriority, wLanguage */
		AVIOUTd(0); /* InitialFrames */
		AVIOUTd(4); /* Scale */
		AVIOUTd(video.audiorate * 4); /* Rate, actual rate is scale/rate */
		AVIOUTd(0);                   /* Start */
		if (!video.audiorate)
			video.audiorate = 1;
		AVIOUTd(video.audiowritten / 4); /* Length */
		AVIOUTd(0);                      /* SuggestedBufferSize */
		AVIOUTd(~0);                     /* Quality */
		AVIOUTd(4);                      /* SampleSize */
		AVIOUTd(0);                      /* Frame */
		AVIOUTd(0);                      /* Frame */
		/* The audio stream format */
		AVIOUT4("strf");
		AVIOUTd(16);                  /* # of bytes to follow */
		AVIOUTw(1);                   /* Format, WAVE_ZMBV_FORMAT_PCM */
		AVIOUTw(2);                   /* Number of channels */
		AVIOUTd(video.audiorate);     /* SamplesPerSec */
		AVIOUTd(video.audiorate * 4); /* AvgBytesPerSec*/
		AVIOUTw(4);                   /* BlockAlign */
		AVIOUTw(16);                  /* BitsPerSample */
		int nmain = header_pos - main_list - 4;
		/* Finish stream list, i.e. put number of bytes in the list to
		 * proper pos */

		int njunk = AVI_HEADER_SIZE - 8 - 12 - header_pos;
		AVIOUT4("JUNK");
		AVIOUTd(njunk);
		/* Fix the size of the main list */
		header_pos = main_list;
		AVIOUTd(nmain);
		header_pos = AVI_HEADER_SIZE - 12;
		AVIOUT4("LIST");
		AVIOUTd(video.written + 4); /* Length of list in bytes */
		AVIOUT4("movi");
		/* First add the index table to the end */
		memcpy(video.index.data(), "idx1", 4);
		host_writed(video.index.data() + 4, video.indexused - 8);
		fwrite(video.index.data(), 1, video.indexused, video.handle);
		fseek(video.handle, 0, SEEK_SET);
		fwrite(&avi_header, 1, AVI_HEADER_SIZE, video.handle);
		fclose(video.handle);
		delete video.codec;
		video.handle = nullptr;
	} else {
		CaptureState |= CAPTURE_VIDEO;
	}
}

void capture_video_add_wave(const uint32_t freq, const uint32_t len,
                            const int16_t* data)
{
	uint32_t left = WAVE_BUF - video.audioused;
	if (left > len) {
		left = len;
	}
	memcpy(&video.audiobuf[video.audioused], data, left * 4);
	video.audioused += left;
	video.audiorate = freq;
}

void capture_video(const uint16_t width, const uint16_t height,
                   const uint8_t bits_per_pixel, const uint16_t pitch,
                   const uint8_t capture_flags, const float frames_per_second,
                   const uint8_t* image_data, const uint8_t* palette_data)
{
	ZMBV_FORMAT format;
	/* Disable capturing if any of the test fails */
	if (video.handle && (video.width != width || video.height != height ||
	                     video.bits_per_pixel != bits_per_pixel ||
	                     video.frames_per_second != frames_per_second)) {
		const auto pressed = true;
		handle_video_event(pressed);
	}
	switch (bits_per_pixel) {
	case 8: format = ZMBV_FORMAT::BPP_8; break;
	case 15: format = ZMBV_FORMAT::BPP_15; break;
	case 16: format = ZMBV_FORMAT::BPP_16; break;

	// ZMBV is "the DOSBox capture format" supported by external
	// tools such as VLC, MPV, and ffmpeg. Because DOSBox originally
	// didn't have 24-bit color, the format itself doesn't support
	// it. I this case we tell ZMBV the data is 32-bit and let the
	// rgb24's int() cast operator up-convert.
	case 24: format = ZMBV_FORMAT::BPP_32; break;
	case 32: format = ZMBV_FORMAT::BPP_32; break;
	default: return;
	}
	if (!video.handle) {
		video.handle = CAPTURE_CreateFile("video output", ".avi");
		if (!video.handle) {
			return;
		}
		video.codec = new VideoCodec();
		if (!video.codec) {
			return;
		}
		if (!video.codec->SetupCompress(width, height)) {
			return;
		}
		video.bufSize = video.codec->NeededSize(width, height, format);
		video.buf.resize(video.bufSize);
		video.index.resize(16 * 4096);
		video.indexused = 8;

		video.width             = width;
		video.height            = height;
		video.bits_per_pixel    = bits_per_pixel;
		video.frames_per_second = frames_per_second;
		for (auto i = 0; i < AVI_HEADER_SIZE; ++i)
			fputc(0, video.handle);
		video.frames       = 0;
		video.written      = 0;
		video.audioused    = 0;
		video.audiowritten = 0;
	}
	int codecFlags;
	if (video.frames % 300 == 0)
		codecFlags = 1;
	else
		codecFlags = 0;
	if (!video.codec->PrepareCompressFrame(
	            codecFlags, format, palette_data, video.buf.data(), video.bufSize)) {
		return;
	}

	const bool is_double_width = capture_flags & CAPTURE_FLAG_DBLW;
	const auto height_divisor = (capture_flags & CAPTURE_FLAG_DBLH) ? 1 : 0;

	uint8_t doubleRow[SCALER_MAXWIDTH * 4];

	for (auto i = 0; i < height; ++i) {
		const uint8_t* rowPointer = doubleRow;
		const auto srcLine = image_data + (i >> height_divisor) * pitch;

		if (is_double_width) {
			const auto countWidth = width >> 1;
			switch (bits_per_pixel) {
			case 8:
				for (auto x = 0; x < countWidth; ++x)
					doubleRow[x * 2 + 0] =
					        doubleRow[x * 2 + 1] = srcLine[x];
				break;
			case 15:
			case 16:
				for (auto x = 0; x < countWidth; ++x)
					((uint16_t*)doubleRow)[x * 2 + 0] = ((
					        uint16_t*)doubleRow)[x * 2 + 1] =
					        ((uint16_t*)srcLine)[x];
				break;
			case 24:
				for (auto x = 0; x < countWidth; ++x) {
					const auto pixel = reinterpret_cast<const rgb24*>(
					        srcLine)[x];
					reinterpret_cast<uint32_t*>(
					        doubleRow)[x * 2 + 0] = pixel;
					reinterpret_cast<uint32_t*>(
					        doubleRow)[x * 2 + 1] = pixel;
				}
				break;
			case 32:
				for (auto x = 0; x < countWidth; ++x)
					((uint32_t*)doubleRow)[x * 2 + 0] = ((
					        uint32_t*)doubleRow)[x * 2 + 1] =
					        ((uint32_t*)srcLine)[x];
				break;
			}
			rowPointer = doubleRow;
		} else {
			if (bits_per_pixel == 24) {
				for (auto x = 0; x < width; ++x) {
					const auto pixel = reinterpret_cast<const rgb24*>(
					        srcLine)[x];
					reinterpret_cast<uint32_t*>(doubleRow)[x] = pixel;
				}
				// Using doubleRow for this conversion when it
				// is not actually double row!
				rowPointer = doubleRow;
			} else {
				rowPointer = srcLine;
			}
		}
		video.codec->CompressLines(1, &rowPointer);
	}
	int written = video.codec->FinishCompressFrame();
	if (written < 0) {
		return;
	}
	add_avi_chunk("00dc", written, video.buf.data(), codecFlags & 1 ? 0x10 : 0x0);
	video.frames++;
	//		LOG_MSG("CAPTURE: Frame %d video %d audio
	//%d",video.frames, written, video.audioused *4 );
	if (video.audioused) {
		add_avi_chunk("01wb", video.audioused * 4, video.audiobuf, 0);
		video.audiowritten = video.audioused * 4;
		video.audioused    = 0;
	}

	/* Everything went okay, set flag again for next frame */
	CaptureState |= CAPTURE_VIDEO;
}

#endif
