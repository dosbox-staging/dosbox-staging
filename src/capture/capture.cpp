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

#include <cerrno>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "cross.h"
#include "dosbox.h"
#include "fs_utils.h"
#include "mapper.h"
#include "mem.h"
#include "pic.h"
#include "render.h"
#include "rgb24.h"
#include "setup.h"
#include "string_utils.h"
#include "support.h"

#if (C_SSHOT)
#include <png.h>
#include "../libs/zmbv/zmbv.h"
#endif

static std::string capturedir;
extern const char* RunningProgram;
Bitu CaptureState;

#define WAVE_BUF 16*1024
#define MIDI_BUF 4*1024
#define AVI_HEADER_SIZE	500

static struct {
	struct {
		FILE *handle = nullptr;
		uint16_t buf[WAVE_BUF][2] = {};
		uint32_t used = 0;
		uint32_t length = 0;
		uint32_t freq = 0;
	} wave = {};

	struct {
		FILE *handle = nullptr;
		uint8_t buffer[MIDI_BUF] = {0};
		uint32_t used = 0;
		uint32_t done = 0;
		uint32_t last = 0;
	} midi = {};

	struct {
		uint32_t rowlen = 0;
	} image = {};

#if (C_SSHOT)
	struct {
		FILE *handle = nullptr;
		uint32_t frames = 0;
		int16_t audiobuf[WAVE_BUF][2] = {};
		uint32_t audioused = 0;
		uint32_t audiorate = 0;
		uint32_t audiowritten = 0;
		VideoCodec *codec = nullptr;
		int width = 0;
		int height = 0;
		int bpp = 0;
		uint32_t written = 0;
		float fps = 0.0f;
		uint32_t bufSize = 0;
		std::vector<uint8_t> buf = {};
		std::vector<uint8_t> index = {};
		uint32_t indexused = 0;
	} video = {};
#endif

} capture = {};

std::string CAPTURE_GetScreenshotFilename(const char *type, const char *ext)
{
	if (capturedir.empty()) {
		LOG_MSG("Please specify a capture directory");
		return 0;
	}

	char file_start[16];
	dir_information *dir;
	/* Find a filename to open */
	dir = open_directory(capturedir.c_str());
	if (!dir) {
		// Try creating it first
		if (create_dir(capturedir, 0700, OK_IF_EXISTS) != 0) {
			LOG_WARNING("Can't create dir '%s' for capturing: %s",
			            capturedir.c_str(),
			            safe_strerror(errno).c_str());
			return 0;
		}
		dir = open_directory(capturedir.c_str());
		if (!dir) {
			LOG_MSG("Can't open dir %s for capturing %s",
			        capturedir.c_str(),
			        type);
			return 0;
		}
	}
	safe_strcpy(file_start, RunningProgram);
	lowcase(file_start);
	strcat(file_start,"_");
	bool is_directory;
	char tempname[CROSS_LEN];
	bool testRead = read_directory_first(dir, tempname, is_directory );
	int last = 0;
	for ( ; testRead; testRead = read_directory_next(dir, tempname, is_directory) ) {
		char * test=strstr(tempname,ext);
		if (!test || strlen(test)!=strlen(ext)) 
			continue;
		*test=0;
		if (strncasecmp(tempname,file_start,strlen(file_start))!=0) continue;
		const int num = atoi(&tempname[strlen(file_start)]);
		if (num >= last)
			last = num + 1;
	}
	close_directory( dir );
	char file_name[CROSS_LEN];
	sprintf(file_name, "%s%c%s%03d%s",
	        capturedir.c_str(), CROSS_FILESPLIT, file_start, last, ext);
	return file_name;
}

FILE *CAPTURE_OpenFile(const char *type, const char *ext)
{
	const auto file_name = CAPTURE_GetScreenshotFilename(type, ext);
	FILE *handle         = fopen(file_name.c_str(), "wb");
	if (handle) {
		LOG_MSG("Capturing %s to %s", type, file_name.c_str());
	} else {
		LOG_MSG("Failed to open %s for capturing %s", file_name.c_str(), type);
	}
	return handle;
}

#if (C_SSHOT)
static void CAPTURE_AddAviChunk(const char * tag, uint32_t size, void * data, uint32_t flags) {
	uint8_t chunk[8];uint8_t *index;uint32_t pos, writesize;

	chunk[0] = tag[0];chunk[1] = tag[1];chunk[2] = tag[2];chunk[3] = tag[3];
	host_writed(&chunk[4], size);   
	/* Write the actual data */
	fwrite(chunk,1,8,capture.video.handle);
	writesize = (size+1)&~1;
	fwrite(data,1,writesize,capture.video.handle);
	pos = capture.video.written + 4;
	capture.video.written += writesize + 8;
	if (capture.video.indexused + 16 >= capture.video.index.size())
		capture.video.index.resize(capture.video.index.size() + 16 * 4096);

	index = capture.video.index.data() + capture.video.indexused;
	capture.video.indexused += 16;
	index[0] = tag[0];
	index[1] = tag[1];
	index[2] = tag[2];
	index[3] = tag[3];
	host_writed(index+4, flags);
	host_writed(index+8, pos);
	host_writed(index+12, size);
}
#endif

#if (C_SSHOT)
static void CAPTURE_VideoEvent(bool pressed) {
	if (!pressed)
		return;
	if (CaptureState & CAPTURE_VIDEO) {
		/* Close the video */
		if (capture.video.codec)
			capture.video.codec->FinishVideo();
		CaptureState &= ~CAPTURE_VIDEO;
		LOG_MSG("Stopped capturing video.");	

		uint8_t avi_header[AVI_HEADER_SIZE];
		Bitu main_list;
		Bitu header_pos=0;
#define AVIOUT4(_S_) memcpy(&avi_header[header_pos],_S_,4);header_pos+=4;
#define AVIOUTw(_S_) host_writew(&avi_header[header_pos], _S_);header_pos+=2;
#define AVIOUTd(_S_) host_writed(&avi_header[header_pos], _S_);header_pos+=4;
		/* Try and write an avi header */
		AVIOUT4("RIFF");                    // Riff header 
		AVIOUTd(AVI_HEADER_SIZE + capture.video.written - 8 + capture.video.indexused);
		AVIOUT4("AVI ");
		AVIOUT4("LIST");                    // List header
		main_list = header_pos;
		AVIOUTd(0);				            // TODO size of list
		AVIOUT4("hdrl");

		AVIOUT4("avih");
		AVIOUTd(56);                         /* # of bytes to follow */
		AVIOUTd((uint32_t)(1000000 / capture.video.fps));       /* Microseconds per frame */
		AVIOUTd(0);
		AVIOUTd(0);                         /* PaddingGranularity (whatever that might be) */
		AVIOUTd(0x110);                     /* Flags,0x10 has index, 0x100 interleaved */
		AVIOUTd(capture.video.frames);      /* TotalFrames */
		AVIOUTd(0);                         /* InitialFrames */
		AVIOUTd(2);                         /* Stream count */
		AVIOUTd(0);                         /* SuggestedBufferSize */
		AVIOUTd(static_cast<uint32_t>(capture.video.width));  /* Width */
		AVIOUTd(static_cast<uint32_t>(capture.video.height)); /* Height */
		AVIOUTd(0);                                           /* TimeScale:  Unit used to measure time */
		AVIOUTd(0);                                           /* DataRate:   Data rate of playback     */
		AVIOUTd(0);                                           /* StartTime:  Starting time of AVI data */
		AVIOUTd(0);                                           /* DataLength: Size of AVI data chunk    */

		/* Video stream list */
		AVIOUT4("LIST");
		AVIOUTd(4 + 8 + 56 + 8 + 40);       /* Size of the list */
		AVIOUT4("strl");
		/* video stream header */
        AVIOUT4("strh");
		AVIOUTd(56);                        /* # of bytes to follow */
		AVIOUT4("vids");                    /* Type */
		AVIOUT4(CODEC_4CC);		            /* Handler */
		AVIOUTd(0);                         /* Flags */
		AVIOUTd(0);                         /* Reserved, MS says: wPriority, wLanguage */
		AVIOUTd(0);                         /* InitialFrames */
		AVIOUTd(1000000);                   /* Scale */
		AVIOUTd((uint32_t)(1000000 * capture.video.fps));              /* Rate: Rate/Scale == samples/second */
		AVIOUTd(0);                         /* Start */
		AVIOUTd(capture.video.frames);      /* Length */
		AVIOUTd(0);                  /* SuggestedBufferSize */
		AVIOUTd(~0);                 /* Quality */
		AVIOUTd(0);                  /* SampleSize */
		AVIOUTd(0);                  /* Frame */
		AVIOUTd(0);                  /* Frame */
		/* The video stream format */
		AVIOUT4("strf");
		AVIOUTd(40);                 /* # of bytes to follow */
		AVIOUTd(40);                 /* Size */
		AVIOUTd(static_cast<uint32_t>(capture.video.width));  /* Width */
		AVIOUTd(static_cast<uint32_t>(capture.video.height)); /* Height */
		//		OUTSHRT(1); OUTSHRT(24);     /* Planes, Count */
		AVIOUTd(0);
		AVIOUT4(CODEC_4CC);          /* Compression */
		AVIOUTd(capture.video.width * capture.video.height*4);  /* SizeImage (in bytes?) */
		AVIOUTd(0);                  /* XPelsPerMeter */
		AVIOUTd(0);                  /* YPelsPerMeter */
		AVIOUTd(0);                  /* ClrUsed: Number of colors used */
		AVIOUTd(0);                  /* ClrImportant: Number of colors important */

		/* Audio stream list */
		AVIOUT4("LIST");
		AVIOUTd(4 + 8 + 56 + 8 + 16);  /* Length of list in bytes */
		AVIOUT4("strl");
		/* The audio stream header */
		AVIOUT4("strh");
		AVIOUTd(56);            /* # of bytes to follow */
		AVIOUT4("auds");
		AVIOUTd(0);             /* Format (Optionally) */
		AVIOUTd(0);             /* Flags */
		AVIOUTd(0);             /* Reserved, MS says: wPriority, wLanguage */
		AVIOUTd(0);             /* InitialFrames */
		AVIOUTd(4);    /* Scale */
		AVIOUTd(capture.video.audiorate*4);             /* Rate, actual rate is scale/rate */
		AVIOUTd(0);             /* Start */
		if (!capture.video.audiorate)
			capture.video.audiorate = 1;
		AVIOUTd(capture.video.audiowritten/4);   /* Length */
		AVIOUTd(0);             /* SuggestedBufferSize */
		AVIOUTd(~0);            /* Quality */
		AVIOUTd(4);				/* SampleSize */
		AVIOUTd(0);             /* Frame */
		AVIOUTd(0);             /* Frame */
		/* The audio stream format */
		AVIOUT4("strf");
		AVIOUTd(16);            /* # of bytes to follow */
		AVIOUTw(1);             /* Format, WAVE_ZMBV_FORMAT_PCM */
		AVIOUTw(2);             /* Number of channels */
		AVIOUTd(capture.video.audiorate);          /* SamplesPerSec */
		AVIOUTd(capture.video.audiorate*4);        /* AvgBytesPerSec*/
		AVIOUTw(4);             /* BlockAlign */
		AVIOUTw(16);            /* BitsPerSample */
		int nmain = header_pos - main_list - 4;
		/* Finish stream list, i.e. put number of bytes in the list to proper pos */

		int njunk = AVI_HEADER_SIZE - 8 - 12 - header_pos;
		AVIOUT4("JUNK");
		AVIOUTd(njunk);
		/* Fix the size of the main list */
		header_pos = main_list;
		AVIOUTd(nmain);
		header_pos = AVI_HEADER_SIZE - 12;
		AVIOUT4("LIST");
		AVIOUTd(capture.video.written+4); /* Length of list in bytes */
		AVIOUT4("movi");
		/* First add the index table to the end */
		memcpy(capture.video.index.data(), "idx1", 4);
		host_writed(capture.video.index.data() + 4, capture.video.indexused - 8);
		fwrite(capture.video.index.data(), 1, capture.video.indexused, capture.video.handle);
		fseek(capture.video.handle, 0, SEEK_SET);
		fwrite(&avi_header, 1, AVI_HEADER_SIZE, capture.video.handle);
		fclose(capture.video.handle);
		delete capture.video.codec;
		capture.video.handle = nullptr;
	} else {
		CaptureState |= CAPTURE_VIDEO;
	}
}
#endif

void CAPTURE_VideoStart() {
#if (C_SSHOT)
	if (CaptureState & CAPTURE_VIDEO) {
		LOG_MSG("Already capturing video.");
	} else {
		CAPTURE_VideoEvent(true);
	}
#else
	LOG_MSG("Avi capturing has not been compiled in");
#endif
}

void CAPTURE_VideoStop() {
#if (C_SSHOT)
	if (CaptureState & CAPTURE_VIDEO) {
		CAPTURE_VideoEvent(true);
	} else {
		LOG_MSG("Not capturing video.");
	}
#else 
	LOG_MSG("Avi capturing has not been compiled in");
#endif
}

void capture_image(const uint16_t width, const uint16_t height,
                   const uint8_t bpp, const uint16_t pitch, const uint8_t flags,
                   const uint8_t* data, const uint8_t* pal)
{
	png_structp png_ptr;
	png_infop info_ptr;
	png_color palette[256];

	FILE* fp = CAPTURE_OpenFile("Screenshot", ".png");
	if (!fp) {
		return;
	}

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr) {
		fclose(fp);
		return;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
		fclose(fp);
		return;
	}

	png_init_io(png_ptr, fp);
	png_set_compression_level(png_ptr, Z_BEST_COMPRESSION);

	png_set_compression_mem_level(png_ptr, 8);
	png_set_compression_strategy(png_ptr, Z_DEFAULT_STRATEGY);
	png_set_compression_window_bits(png_ptr, 15);
	png_set_compression_method(png_ptr, 8);
	png_set_compression_buffer_size(png_ptr, 8192);

	if (bpp == 8) {
		png_set_IHDR(png_ptr,
		             info_ptr,
		             width,
		             height,
		             8,
		             PNG_COLOR_TYPE_PALETTE,
		             PNG_INTERLACE_NONE,
		             PNG_COMPRESSION_TYPE_DEFAULT,
		             PNG_FILTER_TYPE_DEFAULT);

		for (auto i = 0; i < 256; ++i) {
			palette[i].red   = pal[i * 4 + 0];
			palette[i].green = pal[i * 4 + 1];
			palette[i].blue  = pal[i * 4 + 2];
		}
		png_set_PLTE(png_ptr, info_ptr, palette, 256);

	} else {
		png_set_bgr(png_ptr);
		png_set_IHDR(png_ptr,
		             info_ptr,
		             width,
		             height,
		             8,
		             PNG_COLOR_TYPE_RGB,
		             PNG_INTERLACE_NONE,
		             PNG_COMPRESSION_TYPE_DEFAULT,
		             PNG_FILTER_TYPE_DEFAULT);
	}

#ifdef PNG_TEXT_SUPPORTED
	constexpr char keyword[] = "Software";
	static_assert(sizeof(keyword) < 80, "libpng limit");

	constexpr char value[] = CANONICAL_PROJECT_NAME " " VERSION;
	constexpr int num_text = 1;

	png_text texts[num_text] = {};

	texts[0].compression = PNG_TEXT_COMPRESSION_NONE;
	texts[0].key         = const_cast<png_charp>(keyword);
	texts[0].text        = const_cast<png_charp>(value);
	texts[0].text_length = sizeof(value);

	png_set_text(png_ptr, info_ptr, texts, num_text);
#endif
	png_write_info(png_ptr, info_ptr);

	const bool is_double_width = (flags & CAPTURE_FLAG_DBLW);
	const auto row_divisor     = (flags & CAPTURE_FLAG_DBLH) ? 1 : 0;

	uint8_t row_buffer[SCALER_MAXWIDTH * 4];

	for (auto i = 0; i < height; ++i) {
		auto src_row     = data + (i >> row_divisor) * pitch;
		auto row_pointer = src_row;

		switch (bpp) {
		case 8:
			if (is_double_width) {
				for (auto x = 0; x < width; ++x) {
					row_buffer[x * 2 + 0] =
					        row_buffer[x * 2 + 1] = src_row[x];
				}
				row_pointer = row_buffer;
			}
			break;

		case 15:
			if (is_double_width) {
				for (auto x = 0; x < width; ++x) {
					const auto pixel = host_to_le(
					        reinterpret_cast<const uint16_t*>(
					                src_row)[x]);

					row_buffer[x * 6 + 0] = row_buffer[x * 6 + 3] =
					        ((pixel & 0x001f) * 0x21) >> 2;
					row_buffer[x * 6 + 1] = row_buffer[x * 6 + 4] =
					        ((pixel & 0x03e0) * 0x21) >> 7;
					row_buffer[x * 6 + 2] = row_buffer[x * 6 + 5] =
					        ((pixel & 0x7c00) * 0x21) >> 12;
				}
			} else {
				for (auto x = 0; x < width; ++x) {
					const auto pixel = host_to_le(
					        reinterpret_cast<const uint16_t*>(
					                src_row)[x]);

					row_buffer[x * 3 + 0] = ((pixel & 0x001f) *
					                         0x21) >>
					                        2;
					row_buffer[x * 3 + 1] = ((pixel & 0x03e0) *
					                         0x21) >>
					                        7;
					row_buffer[x * 3 + 2] = ((pixel & 0x7c00) *
					                         0x21) >>
					                        12;
				}
			}
			row_pointer = row_buffer;
			break;

		case 16:
			if (is_double_width) {
				for (auto x = 0; x < width; ++x) {
					const auto pixel = host_to_le(
					        reinterpret_cast<const uint16_t*>(
					                src_row)[x]);
					row_buffer[x * 6 + 0] = row_buffer[x * 6 + 3] =
					        ((pixel & 0x001f) * 0x21) >> 2;
					row_buffer[x * 6 + 1] = row_buffer[x * 6 + 4] =
					        ((pixel & 0x07e0) * 0x41) >> 9;
					row_buffer[x * 6 + 2] = row_buffer[x * 6 + 5] =
					        ((pixel & 0xf800) * 0x21) >> 13;
				}
			} else {
				for (auto x = 0; x < width; ++x) {
					const auto pixel = host_to_le(
					        reinterpret_cast<const uint16_t*>(
					                src_row)[x]);
					row_buffer[x * 3 + 0] = ((pixel & 0x001f) *
					                         0x21) >>
					                        2;
					row_buffer[x * 3 + 1] = ((pixel & 0x07e0) *
					                         0x41) >>
					                        9;
					row_buffer[x * 3 + 2] = ((pixel & 0xf800) *
					                         0x21) >>
					                        13;
				}
			}
			row_pointer = row_buffer;
			break;

		case 24:
			if (is_double_width) {
				for (auto x = 0; x < width; ++x) {
					const auto pixel = host_to_le(
					        reinterpret_cast<const rgb24*>(
					                src_row)[x]);

					reinterpret_cast<rgb24*>(
					        row_buffer)[x * 2 + 0] = pixel;
					reinterpret_cast<rgb24*>(
					        row_buffer)[x * 2 + 1] = pixel;

					row_pointer = row_buffer;
				}
			}
			// There is no else statement here because
			// row_pointer is already defined as src_row
			// above which is already 24-bit single row
			break;

		case 32:
			if (is_double_width) {
				for (auto x = 0; x < width; ++x) {
					row_buffer[x * 6 + 0] = row_buffer[x * 6 + 3] =
					        src_row[x * 4 + 0];
					row_buffer[x * 6 + 1] = row_buffer[x * 6 + 4] =
					        src_row[x * 4 + 1];
					row_buffer[x * 6 + 2] = row_buffer[x * 6 + 5] =
					        src_row[x * 4 + 2];
				}
			} else {
				for (auto x = 0; x < width; ++x) {
					row_buffer[x * 3 + 0] = src_row[x * 4 + 0];
					row_buffer[x * 3 + 1] = src_row[x * 4 + 1];
					row_buffer[x * 3 + 2] = src_row[x * 4 + 2];
				}
			}
			row_pointer = row_buffer;
			break;
		}

		png_write_row(png_ptr, row_pointer);
	}

	png_write_end(png_ptr, 0);
	png_destroy_write_struct(&png_ptr, &info_ptr);
	fclose(fp);
}

void capture_video(const uint16_t width, const uint16_t height,
                   const uint8_t bpp, const uint16_t pitch, const uint8_t flags,
                   const float fps, const uint8_t* data, const uint8_t* pal)
{
	ZMBV_FORMAT format;
	/* Disable capturing if any of the test fails */
	if (capture.video.handle &&
	    (capture.video.width != width || capture.video.height != height ||
	     capture.video.bpp != bpp || capture.video.fps != fps)) {
		CAPTURE_VideoEvent(true);
	}
	switch (bpp) {
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
	if (!capture.video.handle) {
		capture.video.handle = CAPTURE_OpenFile("Video", ".avi");
		if (!capture.video.handle) {
			return;
		}
		capture.video.codec = new VideoCodec();
		if (!capture.video.codec) {
			return;
		}
		if (!capture.video.codec->SetupCompress(width, height)) {
			return;
		}
		capture.video.bufSize = capture.video.codec->NeededSize(width,
		                                                        height,
		                                                        format);
		capture.video.buf.resize(capture.video.bufSize);
		capture.video.index.resize(16 * 4096);
		capture.video.indexused = 8;

		capture.video.width  = width;
		capture.video.height = height;
		capture.video.bpp    = bpp;
		capture.video.fps    = fps;
		for (auto i = 0; i < AVI_HEADER_SIZE; ++i)
			fputc(0, capture.video.handle);
		capture.video.frames       = 0;
		capture.video.written      = 0;
		capture.video.audioused    = 0;
		capture.video.audiowritten = 0;
	}
	int codecFlags;
	if (capture.video.frames % 300 == 0)
		codecFlags = 1;
	else
		codecFlags = 0;
	if (!capture.video.codec->PrepareCompressFrame(codecFlags,
	                                               format,
	                                               pal,
	                                               capture.video.buf.data(),
	                                               capture.video.bufSize)) {
		return;
	}

	const bool is_double_width = flags & CAPTURE_FLAG_DBLW;
	const auto height_divisor  = (flags & CAPTURE_FLAG_DBLH) ? 1 : 0;

	uint8_t doubleRow[SCALER_MAXWIDTH * 4];

	for (auto i = 0; i < height; ++i) {
		const uint8_t* rowPointer = doubleRow;
		const auto srcLine = data + (i >> height_divisor) * pitch;

		if (is_double_width) {
			const auto countWidth = width >> 1;
			switch (bpp) {
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
			if (bpp == 24) {
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
		capture.video.codec->CompressLines(1, &rowPointer);
	}
	int written = capture.video.codec->FinishCompressFrame();
	if (written < 0) {
		return;
	}
	CAPTURE_AddAviChunk("00dc",
	                    written,
	                    capture.video.buf.data(),
	                    codecFlags & 1 ? 0x10 : 0x0);
	capture.video.frames++;
	//		LOG_MSG("Frame %d video %d audio %d",capture.video.frames,
	//written, capture.video.audioused *4 );
	if (capture.video.audioused) {
		CAPTURE_AddAviChunk("01wb",
		                    capture.video.audioused * 4,
		                    capture.video.audiobuf,
		                    0);
		capture.video.audiowritten = capture.video.audioused * 4;
		capture.video.audioused    = 0;
	}

	/* Everything went okay, set flag again for next frame */
	CaptureState |= CAPTURE_VIDEO;
}

void CAPTURE_AddImage([[maybe_unused]] uint16_t width,
                      [[maybe_unused]] uint16_t height,
                      [[maybe_unused]] uint8_t bpp, [[maybe_unused]] uint16_t pitch,
                      [[maybe_unused]] uint8_t flags, [[maybe_unused]] float fps,
                      [[maybe_unused]] uint8_t* data, [[maybe_unused]] uint8_t* pal)
{
#if (C_SSHOT)
	if (flags & CAPTURE_FLAG_DBLH) {
		height *= 2;
	}
	if (flags & CAPTURE_FLAG_DBLW) {
		width *= 2;
	}
	if (height > SCALER_MAXHEIGHT) {
		return;
	}
	if (width > SCALER_MAXWIDTH) {
		return;
	}

	if (CaptureState & CAPTURE_IMAGE) {
		capture_image(width, height, bpp, pitch, flags, data, pal);
		CaptureState &= ~CAPTURE_IMAGE;
	}
	if (CaptureState & CAPTURE_VIDEO) {
		capture_video(width, height, bpp, pitch, flags, fps, data, pal);
		CaptureState &= ~CAPTURE_VIDEO;
	}
#endif
}

#if (C_SSHOT)
static void CAPTURE_ScreenshotEvent(bool pressed) {
	if (!pressed)
		return;
	CaptureState |= CAPTURE_IMAGE;
}
#endif


/* WAV capturing */
static uint8_t wavheader[]={
	'R','I','F','F',	0x0,0x0,0x0,0x0,		/* uint32_t Riff Chunk ID /  uint32_t riff size */
	'W','A','V','E',	'f','m','t',' ',		/* uint32_t Riff Format  / uint32_t fmt chunk id */
	0x10,0x0,0x0,0x0,	0x1,0x0,0x2,0x0,		/* uint32_t fmt size / uint16_t encoding/ uint16_t channels */
	0x0,0x0,0x0,0x0,	0x0,0x0,0x0,0x0,		/* uint32_t freq / uint32_t byterate */
	0x4,0x0,0x10,0x0,	'd','a','t','a',		/* uint16_t byte-block / uint16_t bits / uint16_t data chunk id */
	0x0,0x0,0x0,0x0,							/* uint32_t data size */
};

void CAPTURE_AddWave(uint32_t freq, uint32_t len, int16_t * data) {
#if (C_SSHOT)
	if (CaptureState & CAPTURE_VIDEO) {
		Bitu left = WAVE_BUF - capture.video.audioused;
		if (left > len)
			left = len;
		memcpy( &capture.video.audiobuf[capture.video.audioused], data, left*4);
		capture.video.audioused += left;
		capture.video.audiorate = freq;
	}
#endif
	if (CaptureState & CAPTURE_WAVE) {
		if (!capture.wave.handle) {
			capture.wave.handle=CAPTURE_OpenFile("Wave Output",".wav");
			if (!capture.wave.handle) {
				CaptureState &= ~CAPTURE_WAVE;
				return;
			}
			capture.wave.length = 0;
			capture.wave.used = 0;
			capture.wave.freq = freq;
			fwrite(wavheader,1,sizeof(wavheader),capture.wave.handle);
		}
		int16_t * read = data;
		while (len > 0 ) {
			Bitu left = WAVE_BUF - capture.wave.used;
			if (!left) {
				fwrite(capture.wave.buf,1,4*WAVE_BUF,capture.wave.handle);
				capture.wave.length += 4*WAVE_BUF;
				capture.wave.used = 0;
				left = WAVE_BUF;
			}
			if (left > len)
				left = len;
			memcpy( &capture.wave.buf[capture.wave.used], read, left*4);
			capture.wave.used += left;
			read += left*2;
			len -= left;
		}
	}
}
static void CAPTURE_WaveEvent(bool pressed) {
	if (!pressed)
		return;
	/* Check for previously opened wave file */
	if (capture.wave.handle) {
		LOG_MSG("Stopped capturing wave output.");
		/* Write last piece of audio in buffer */
		fwrite(capture.wave.buf,1,capture.wave.used*4,capture.wave.handle);
		capture.wave.length+=capture.wave.used*4;
		/* Fill in the header with useful information */
		host_writed(&wavheader[0x04],capture.wave.length+sizeof(wavheader)-8);
		host_writed(&wavheader[0x18],capture.wave.freq);
		host_writed(&wavheader[0x1C],capture.wave.freq*4);
		host_writed(&wavheader[0x28],capture.wave.length);
		
		fseek(capture.wave.handle,0,0);
		fwrite(wavheader,1,sizeof(wavheader),capture.wave.handle);
		fclose(capture.wave.handle);
		capture.wave.handle=0;
		CaptureState |= CAPTURE_WAVE;
	} 
	CaptureState ^= CAPTURE_WAVE;
}

/* MIDI capturing */

static uint8_t midi_header[]={
	'M','T','h','d',			/* uint32_t, Header Chunk */
	0x0,0x0,0x0,0x6,			/* uint32_t, Chunk Length */
	0x0,0x0,					/* uint16_t, Format, 0=single track */
	0x0,0x1,					/* uint16_t, Track Count, 1 track */
	0x01,0xf4,					/* uint16_t, Timing, 2 beats/second with 500 frames */
	'M','T','r','k',			/* uint32_t, Track Chunk */
	0x0,0x0,0x0,0x0,			/* uint32_t, Chunk Length */
	//Track data
};


static void RawMidiAdd(uint8_t data) {
	capture.midi.buffer[capture.midi.used++]=data;
	if (capture.midi.used >= MIDI_BUF ) {
		capture.midi.done += capture.midi.used;
		fwrite(capture.midi.buffer,1,MIDI_BUF,capture.midi.handle);
		capture.midi.used = 0;
	}
}

static void RawMidiAddNumber(uint32_t val) {
	if (val & 0xfe00000) RawMidiAdd((uint8_t)(0x80|((val >> 21) & 0x7f)));
	if (val & 0xfffc000) RawMidiAdd((uint8_t)(0x80|((val >> 14) & 0x7f)));
	if (val & 0xfffff80) RawMidiAdd((uint8_t)(0x80|((val >> 7) & 0x7f)));
	RawMidiAdd((uint8_t)(val & 0x7f));
}

void CAPTURE_AddMidi(bool sysex, Bitu len, uint8_t * data) {
	if (!capture.midi.handle) {
		capture.midi.handle=CAPTURE_OpenFile("Raw Midi",".mid");
		if (!capture.midi.handle) {
			return;
		}
		fwrite(midi_header,1,sizeof(midi_header),capture.midi.handle);
		capture.midi.last=PIC_Ticks;
	}
	uint32_t delta=PIC_Ticks-capture.midi.last;
	capture.midi.last=PIC_Ticks;
	RawMidiAddNumber(delta);
	if (sysex) {
		RawMidiAdd( 0xf0 );
		RawMidiAddNumber(static_cast<uint32_t>(len));
	}
	for (Bitu i=0;i<len;i++) 
		RawMidiAdd(data[i]);
}

static void CAPTURE_MidiEvent(bool pressed) {
	if (!pressed)
		return;
	/* Check for previously opened wave file */
	if (capture.midi.handle) {
		LOG_MSG("Stopping raw midi saving and finalizing file.");
		//Delta time
		RawMidiAdd(0x00);
		//End of track event
		RawMidiAdd(0xff);
		RawMidiAdd(0x2F);
		RawMidiAdd(0x00);
		/* clear out the final data in the buffer if any */
		fwrite(capture.midi.buffer,1,capture.midi.used,capture.midi.handle);
		capture.midi.done+=capture.midi.used;
		if (fseek(capture.midi.handle, 18, SEEK_SET) != 0) {
			LOG_WARNING("CAPTURE: Failed seeking in MIDI capture file: %s",
			            strerror(errno));
			CaptureState ^= CAPTURE_MIDI;
			return;
		}
		uint8_t size[4];
		size[0]=(uint8_t)(capture.midi.done >> 24);
		size[1]=(uint8_t)(capture.midi.done >> 16);
		size[2]=(uint8_t)(capture.midi.done >> 8);
		size[3]=(uint8_t)(capture.midi.done >> 0);
		fwrite(&size,1,4,capture.midi.handle);
		fclose(capture.midi.handle);
		capture.midi.handle=0;
		CaptureState &= ~CAPTURE_MIDI;
		return;
	} 
	CaptureState ^= CAPTURE_MIDI;
	if (CaptureState & CAPTURE_MIDI) {
		LOG_MSG("Preparing for raw midi capture, will start with first data.");
		capture.midi.used=0;
		capture.midi.done=0;
		capture.midi.handle=0;
	} else {
		LOG_MSG("Stopped capturing raw midi before any data arrived.");
	}
}

void CAPTURE_Destroy([[maybe_unused]] Section *sec)
{
#if (C_SSHOT)
	if (capture.video.handle) CAPTURE_VideoEvent(true);
#endif
	if (capture.wave.handle) CAPTURE_WaveEvent(true);
	if (capture.midi.handle) CAPTURE_MidiEvent(true);
}

void CAPTURE_Init(Section* sec)
{
	assert(sec);

	const Section_prop* conf = dynamic_cast<Section_prop*>(sec);

	Prop_path* proppath = conf->Get_path("captures");
	capturedir          = proppath->realpath;
	CaptureState        = 0;

	MAPPER_AddHandler(CAPTURE_WaveEvent,
	                  SDL_SCANCODE_F6,
	                  PRIMARY_MOD,
	                  "recwave",
	                  "Rec. Audio");

	MAPPER_AddHandler(CAPTURE_MidiEvent, SDL_SCANCODE_UNKNOWN, 0, "caprawmidi", "Rec. MIDI");
#if (C_SSHOT)
	MAPPER_AddHandler(CAPTURE_ScreenshotEvent,
	                  SDL_SCANCODE_F5,
	                  PRIMARY_MOD,
	                  "scrshot",
	                  "Screenshot");

	MAPPER_AddHandler(CAPTURE_VideoEvent,
	                  SDL_SCANCODE_F7,
	                  PRIMARY_MOD,
	                  "video",
	                  "Rec. Video");
#endif

	constexpr auto changeable_at_runtime = true;
	sec->AddDestroyFunction(&CAPTURE_Destroy, changeable_at_runtime);
}
